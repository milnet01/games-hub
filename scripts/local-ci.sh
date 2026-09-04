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
# so every run. That half is verified by CI, and — since GHUB-0091 — optionally
# by scripts/wintest-ci.sh, which builds and tests it on a real Windows box
# over SSH. The two are not interchangeable: that box has fonts installed and
# the runner does not, so read wintest-ci.sh's header before trusting one for
# the other.
#
# Usage:  scripts/local-ci.sh            # lint + build + test the Linux leg
#         scripts/local-ci.sh --lint     # workflow linters only (docs pushes)
#         scripts/local-ci.sh --with-sanitizers   # add the ASan/UBSan fuzz leg
#         scripts/local-ci.sh --with-tidy         # add the clang-tidy leg

# shellcheck disable=SC2016  # the ${{ }} literal below is deliberately unexpanded
set -uo pipefail

cd "$(dirname "$0")/.." || exit 1
WORKFLOW=.github/workflows/ci.yml
FAILED=0
SKIPPED=()
# The sanitizer leg is real work rather than a lint, so it is opt-in here and
# always runs on CI. See the job guard in the step loop below.
WITH_SANITIZERS=0
SANITIZERS_NOTED=0
# Same shape as the sanitizer leg: real work rather than a lint, so opt-in here
# and always on CI. It also runs a DIFFERENT clang-tidy from the pinned one CI
# uses -- whatever this machine has -- which is deliberate and noted in the
# workflow: check families only grow, so the local run is the stricter of the
# two.
WITH_TIDY=0
TIDY_NOTED=0
LAST_JOB=
LINT_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --lint)             LINT_ONLY=1 ;;
        --with-sanitizers)  WITH_SANITIZERS=1 ;;
        --with-tidy)        WITH_TIDY=1 ;;
        *) echo "unknown option '$arg'"; exit 1 ;;
    esac
done

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

if [ "$LINT_ONLY" -eq 1 ]; then
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
Install clang-tidy|provision|clang-tidy --version
Set up MSVC environment|windows-only|
RULES
)

bold "Steps from $WORKFLOW (Linux legs)"

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
# Every job, not just `build`. This read the build job alone until GHUB-0052
# added a second one, and a job this script cannot see is the same silent
# drift a step it cannot see would be -- worse, because it is a whole leg.
# An unknown job stops the run exactly as an unknown step does.
known = ('build', 'sanitizers', 'tidy')
for name in wf['jobs']:
    if name not in known:
        sys.stdout.write('\x1e'.join(['UNKNOWNJOB', name, '']) + '\0')
for job_name in known:
    job = wf['jobs'].get(job_name)
    if job is None:
        continue
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
        # This script runs a `run:` body and applies nothing around it, so a
        # step carrying an `env:` block would execute here WITHOUT it and
        # differ from CI quietly. Refuse rather than mirror it wrongly; put
        # the setting inline in the run body instead.
        if step.get('env'):
            kind = 'HASENV'
        # NUL between records, \x1e between fields: a `run:` block is
        # multi-line, so a newline-separated record would be split mid-body
        # and the step would run only its first line -- silently, and green.
        sys.stdout.write('\x1e'.join([kind, name, body, job_name]) + '\0')
PY
PARSE_RC=$?
[ $PARSE_RC -eq 0 ] || { echo "could not parse $WORKFLOW (is python3-yaml installed?)"; exit 1; }

while IFS= read -r -d '' REC; do
    STEPS_RUN=$((STEPS_RUN + 1))
    KIND=${REC%%$'\x1e'*}
    REST=${REC#*$'\x1e'}
    NAME=${REST%%$'\x1e'*}
    REST=${REST#*$'\x1e'}
    BODY=${REST%$'\x1e'*}
    JOB=${REST##*$'\x1e'}
    [ -z "$KIND" ] && continue

    if [ "$KIND" = "HASENV" ]; then
        bad "step '$NAME' carries an env: block, which this script does not apply — inline it in the run body"
        continue
    fi
    if [ "$KIND" = "UNKNOWNJOB" ]; then
        bad "UNKNOWN JOB '$NAME' in $WORKFLOW — teach $0 about it or it runs nowhere but CI"
        continue
    fi

    # The sanitizer leg CAN run here, unlike the Windows one — it is simply
    # dear: its own build tree, and a binary several times slower. Off by
    # default so the pre-push hook stays quick, named in the summary every
    # run so that default never reads as coverage.
    if [ "$JOB" = "sanitizers" ] && [ "$WITH_SANITIZERS" -eq 0 ]; then
        [ "$SANITIZERS_NOTED" -eq 0 ] && {
            note "sanitizers job — not run (pass --with-sanitizers)"
            SKIPPED+=("the ASan/UBSan saved-game fuzz")
            SANITIZERS_NOTED=1
        }
        continue
    fi
    if [ "$JOB" = "tidy" ] && [ "$WITH_TIDY" -eq 0 ]; then
        [ "$TIDY_NOTED" -eq 0 ] && {
            note "clang-tidy job — not run (pass --with-tidy)"
            SKIPPED+=("the clang-tidy analysis")
            TIDY_NOTED=1
        }
        continue
    fi
    if [ "$JOB" != "$LAST_JOB" ]; then
        echo
        bold "Steps from the '$JOB' job"
        LAST_JOB=$JOB
    fi
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
    #
    # Fail CLOSED on a body that provisions. STEP_RULES is keyed on a step's
    # free-text NAME, so renaming a step in ci.yml drops its rule silently and
    # promotes it from guarded to EXECUTED -- and the step this matters for
    # installs packages, so a rename would run `sudo apt-get` from inside a
    # pre-push hook. The body is what decides here, and a rename cannot change
    # a body.
    if [ "$RULE_KIND" != "provision" ] \
       && printf '%s' "$BODY" | grep -qE '(^|[^[:alnum:]_])(sudo|apt-get|apt|dnf|zypper|pacman)([^[:alnum:]_]|$)'; then
        bad "step '$NAME' installs packages and carries no 'provision' rule — the step was probably renamed in $WORKFLOW; update STEP_RULES in $0"
        continue
    fi
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
    echo "  Nothing on Linux drives MSVC. Either push and let CI run it, or"
    echo "  run scripts/wintest-ci.sh to build and test it on the wintest box."
    echo "  That box is NOT the runner: it has real fonts, CI runs headless"
    echo "  with an empty font database, so font-derived checks can differ."
fi

echo
if [ $FAILED -eq 0 ]; then
    bold "Local CI passed — but only for what ran. See 'Not checked locally' above."
else
    bold "Local CI FAILED — do not push."
fi
exit $FAILED
