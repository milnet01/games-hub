#!/usr/bin/env bash
# Run the GitHub CI pipeline locally, before pushing.
#
# The steps are READ OUT OF .github/workflows/ci.yml rather than copied here,
# because a hand-written mirror of a pipeline drifts and then returns green
# for a build that will fail. This script executes the workflow's own `run:`
# blocks, in the workflow's own order.
#
# It fails loudly on anything it does not recognise. A new step added to
# ci.yml that this script has no rule for stops the run rather than being
# skipped silently — a skipped step is the drift the whole approach exists to
# prevent.
#
# What it CANNOT do: the Windows leg. Nothing on Linux runs MSVC, and a
# checker pointed at the wrong toolchain is worse than none. The summary says
# so every run; that half is verified by CI and nowhere else.
#
# Usage:  scripts/local-ci.sh            # lint + build + test the Linux leg
#         scripts/local-ci.sh --lint     # workflow linters only (docs pushes)

# shellcheck disable=SC2016  # the ${{ }} literal below is deliberately unexpanded
set -uo pipefail

cd "$(dirname "$0")/.." || exit 1
WORKFLOW=.github/workflows/ci.yml
FAILED=0
SKIPPED=()

bold() { printf '\033[1m%s\033[0m\n' "$1"; }
ok()   { printf '  \033[32m✓\033[0m %s\n' "$1"; }
bad()  { printf '  \033[31m✗\033[0m %s\n' "$1"; FAILED=1; }
note() { printf '  \033[33m-\033[0m %s\n' "$1"; }

[ -f "$WORKFLOW" ] || { echo "no $WORKFLOW — nothing to run"; exit 1; }

# --------------------------------------------------------------------------
# 1. The workflow files themselves
# --------------------------------------------------------------------------
bold "Workflow linters"
for tool in actionlint yamllint zizmor; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        # An absent linter prints nothing, which is indistinguishable from a
        # clean run unless it is called out.
        note "$tool is not installed — NOT CHECKED"
        SKIPPED+=("$tool")
        continue
    fi
    case $tool in
        actionlint) out=$(actionlint 2>&1); rc=$? ;;
        yamllint)   out=$(yamllint -d '{extends: default, rules: {line-length: {max: 100}, truthy: {check-keys: false}}}' .github/workflows/ 2>&1); rc=$? ;;
        zizmor)     out=$(zizmor --no-progress .github/workflows/ 2>&1); rc=$? ;;
        *)          out=""; rc=1 ;;
    esac
    if [ "$rc" -eq 0 ]; then
        ok "$tool"
    else
        bad "$tool"
        printf '%s\n' "$out" | grep -Ev '^ INFO' | sed 's/^/      /' | head -25
    fi
done

if [ "${1:-}" = "--lint" ]; then
    echo
    bold "Lint-only run (documentation change) — build and tests not run."
    exit $FAILED
fi

# --------------------------------------------------------------------------
# 2. The Linux leg's steps, taken from the workflow
# --------------------------------------------------------------------------
# Steps this script handles without executing them verbatim. Anything not
# named here stops the run.
#   provision  — sets up a fresh GitHub runner; this machine is already set
#                up, so the local equivalent is to check the tool is present
#   action     — a `uses:` step; the real action cannot run outside Actions
STEP_RULES=$(cat <<'RULES'
Install Qt|action|qmake6 --version
Install Linux build and runtime dependencies|provision|ninja --version
Set up MSVC environment|windows-only|
RULES
)

bold "Steps from $WORKFLOW (Linux leg)"

# Via a temp file, not $(...): command substitution strips NUL bytes, so the
# whole step list came back empty and the run reported success having
# executed nothing. STEPS_RUN below is the guard that makes that impossible
# to mistake for a pass a second time.
STEPS_FILE=$(mktemp)
trap 'rm -f "$STEPS_FILE"' EXIT
STEPS_RUN=0

python3 - "$WORKFLOW" > "$STEPS_FILE" <<'PY'
import sys, yaml
wf = yaml.safe_load(open(sys.argv[1]))
job = wf['jobs']['build']
for step in job['steps']:
    cond = step.get('if', '')
    if "runner.os == 'Windows'" in cond:
        kind = 'WINDOWS'
    elif 'uses' in step:
        kind = 'USES'
    else:
        kind = 'RUN'
    name = step.get('name') or step.get('uses', '').split('@')[0]
    body = step.get('run', '') if kind == 'RUN' else step.get('uses', '')
    # NUL between records, \x1e between fields: a `run:` block is multi-line,
    # so a newline-separated record would be split mid-body and the step
    # would run only its first line -- silently, and green.
    sys.stdout.write('\x1e'.join([kind, name, body]) + '\0')
PY
PARSE_RC=$?
[ $PARSE_RC -eq 0 ] || { echo "could not parse $WORKFLOW (is python3-yaml installed?)"; exit 1; }

while IFS= read -r -d '' REC; do
    STEPS_RUN=$((STEPS_RUN + 1))
    KIND=${REC%%$'\x1e'*}
    REST=${REC#*$'\x1e'}
    NAME=${REST%%$'\x1e'*}
    BODY=${REST#*$'\x1e'}
    [ -z "$KIND" ] && continue
    RULE=$(printf '%s\n' "$STEP_RULES" | awk -F'|' -v n="$NAME" '$1==n {print $2"|"$3}')
    RULE_KIND=${RULE%%|*}
    RULE_CHECK=${RULE#*|}

    case "$KIND" in
      WINDOWS)
        note "$NAME — Windows leg, cannot run here"
        SKIPPED+=("$NAME (Windows)")
        continue ;;
      USES)
        if [ "$RULE_KIND" != "action" ]; then
            if [ "$NAME" = "actions/checkout" ]; then
                note "$NAME — already in the working tree"
                continue
            fi
            bad "UNKNOWN ACTION '$NAME' in $WORKFLOW — add a rule to STEP_RULES in $0"
            continue
        fi
        if eval "$RULE_CHECK" >/dev/null 2>&1; then
            ok "$NAME — using this machine's own install instead ($RULE_CHECK)"
        else
            bad "$NAME — the local stand-in '$RULE_CHECK' does not work here"
        fi
        continue ;;
    esac

    # A RUN step.
    if [ "$RULE_KIND" = "provision" ]; then
        if eval "$RULE_CHECK" >/dev/null 2>&1; then
            ok "$NAME — already provisioned here ($RULE_CHECK)"
        else
            bad "$NAME — '$RULE_CHECK' is missing on this machine"
        fi
        continue
    fi
    if [ -n "$RULE_KIND" ] && [ "$RULE_KIND" != "$NAME" ]; then
        bad "step '$NAME' has rule '$RULE_KIND', which this script does not implement"
        continue
    fi
    if printf '%s' "$BODY" | grep -q '\${{'; then
        bad "step '$NAME' uses a \${{ }} expression this script cannot evaluate"
        continue
    fi

    echo "  → $NAME"
    if bash -eo pipefail -c "$BODY" 2>&1 | sed 's/^/      /'; then
        ok "$NAME"
    else
        bad "$NAME"
    fi
done < "$STEPS_FILE"

# --------------------------------------------------------------------------
# 3. Say what was not covered — silence would read as coverage
# --------------------------------------------------------------------------
# A step list that came back empty must never read as a pass.
if [ "$STEPS_RUN" -eq 0 ]; then
    bad "no steps were read from $WORKFLOW — the parse produced nothing"
fi

echo
if [ ${#SKIPPED[@]} -gt 0 ]; then
    bold "Not checked locally"
    for s in "${SKIPPED[@]}"; do echo "  - $s"; done
    echo "  The Windows build and tests run only on GitHub."
fi

echo
if [ $FAILED -eq 0 ]; then
    bold "Local CI passed. The Windows leg is still unverified until CI runs."
else
    bold "Local CI FAILED — do not push."
fi
exit $FAILED
