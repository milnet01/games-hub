#!/usr/bin/env bash
# Proves which arm of the pre-push gate a given push takes.
#
# This exists because the hook's whole job is a decision — lint-only or full
# pipeline — made from data git hands it on stdin, and a wrong decision is
# SILENT: the push succeeds either way. GHUB-0027 was exactly that. A release
# push ran the linters and nothing else for as long as the hook had existed,
# and the only reason it was ever noticed was someone reading the output.
#
# Since GHUB-0202 .githooks/pre-push decides nothing itself: it hands the push
# to the machine-wide hook (~/.claude/githooks/pre-push), and this project's
# answers live in .ants/gate.conf. So what this proves is that the two
# together take the right arm here — that the committed settings say what this
# project means by documentation.
#
# So the test drives real `git push` calls against a real (bare, local) remote
# rather than hand-feeding the hook a stdin format that might be wrong. The
# throwaway repo carries the real shim and gate.conf, plus its own
# scripts/local-ci.sh, a stub that announces which way it was called.
#
#     tests/pre-push-test.sh          # run it
#
# Skipped, not failed, when git is absent. Where the machine-wide hook is
# absent — a CI runner, a colleague's clone — only the first case runs: that
# the shim says nothing was checked rather than passing in silence.

set -uo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
HOOK="$ROOT/.githooks/pre-push"
CONF="$ROOT/.ants/gate.conf"
GLOBAL_HOOK="${ANTS_GLOBAL_HOOKS:-$HOME/.claude/githooks}/pre-push"

command -v git >/dev/null 2>&1 || { echo "SKIP: git not installed"; exit 0; }
[ -x "$HOOK" ] || { echo "FAIL: $HOOK is missing or not executable"; exit 1; }
[ -f "$CONF" ] || { echo "FAIL: $CONF is missing"; exit 1; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

PASS=0
FAIL=0

# --- the throwaway repo ------------------------------------------------------
# -c init.defaultBranch: the branch name is pushed by name below, and a runner
# whose git defaults to `main` would otherwise fail on a refspec, not on the
# decision this test is about.
git -c init.defaultBranch=master init --quiet --bare "$TMP/remote.git"
git -c init.defaultBranch=master init --quiet "$TMP/work"
cd "$TMP/work" || exit 1
git config user.email test@example.invalid
git config user.name  "Pre-push Test"
git config commit.gpgsign false
git config tag.gpgsign false
git config advice.detachedHead false
git remote add origin "$TMP/remote.git"

mkdir -p .githooks .ants scripts src docs
cp "$HOOK" .githooks/pre-push
chmod +x .githooks/pre-push
cp "$CONF" .ants/gate.conf
git config core.hooksPath .githooks

# The stub stands in for the pipeline and says which way it was called. It
# always succeeds, so a push is never blocked by this test.
cat > scripts/local-ci.sh <<'STUB'
#!/usr/bin/env bash
if [ "${1:-}" = "--lint" ]; then echo "STUB:LINT"; else echo "STUB:FULL"; fi
exit 0
STUB
chmod +x scripts/local-ci.sh

commit() { git add -A && git commit --quiet -m "$1"; }

# `expect <what> <push args...>` runs the push and checks the arm taken.
# <what> is LINT, FULL, NONE (the hook declined to check anything) or
# UNCHECKED (no machine-wide hook, and the shim said so).
expect() {
    want=$1; shift
    out=$(git push "$@" 2>&1)
    case "$want" in
        LINT)      got=$(printf '%s' "$out" | grep -c 'STUB:LINT') ;;
        FULL)      got=$(printf '%s' "$out" | grep -c 'STUB:FULL') ;;
        NONE)      got=$(printf '%s' "$out" | grep -c 'nothing to check') ;;
        UNCHECKED) got=$(printf '%s' "$out" | grep -c 'NOTHING WAS CHECKED') ;;
    esac
    if [ "$got" -ge 1 ]; then
        PASS=$((PASS + 1))
        echo "  ok    $want   (git push $*)"
    else
        FAIL=$((FAIL + 1))
        echo "  FAIL  expected $want   (git push $*)"
        printf '%s\n' "$out" | sed 's/^/          /'
    fi
}

echo "pre-push hook decisions:"

# 0. With no machine-wide hook the shim gates nothing, and must SAY so: a check
#    that did not run must not look like one that passed. The only case a CI
#    runner can run, so it runs everywhere, pointed at an empty directory.
echo 'int main(){}' > src/main.cpp
echo '# hub' > README.md
commit "initial"
mkdir "$TMP/no-hooks"
ANTS_GLOBAL_HOOKS="$TMP/no-hooks" expect UNCHECKED -u origin master

if [ ! -x "$GLOBAL_HOOK" ]; then
    echo "  skip  the gate's decisions ($GLOBAL_HOOK is absent here)"
    echo
    echo "pre-push: $PASS passed, $FAIL failed"
    [ "$FAIL" -eq 0 ]
    exit
fi

# 1. A code push runs the full pipeline.
echo '// first' >> src/main.cpp
commit "code"
expect FULL origin master

# 2. A documentation-only push still lints and stops. This path is a feature,
#    not an accident — keep it working.
echo 'more prose' >> README.md
echo 'a note' > docs/note.md
commit "docs only"
expect LINT origin master

# 3. A mixed push builds. One code file among the prose is enough.
echo '// tweak' >> src/main.cpp
echo 'even more prose' >> README.md
commit "code and docs"
expect FULL origin master

# 4. THE REGRESSION. A release push sends the branch and the tag together, and
#    git feeds the tag LAST. A hook that lets the last ref decide sees a new
#    ref, diffs a bare sha against a clean working tree, finds nothing, and
#    calls a CMakeLists change documentation.
echo 'project(x VERSION 0.0.2)' > CMakeLists.txt
commit "release 0.0.2"
git tag -a v0.0.2 -m "0.0.2"
expect FULL --follow-tags origin master

# 5. The other half of the same defect: a brand-new branch is a new ref too,
#    so it went down the same broken path even with no tag in sight.
git checkout --quiet -b feature
echo '// feature work' >> src/main.cpp
commit "feature work"
expect FULL -u origin feature

# 6. A new branch carrying only prose still lints.
git checkout --quiet -b prose
echo 'prose branch' >> docs/note.md
commit "prose only"
expect LINT -u origin prose

# 7. Deleting a branch checks nothing — there is no new code in a deletion.
expect NONE origin --delete prose

# 8. GHUB-0202: the gate answers for the PUSHED commits, not the working tree.
#    An uncommitted edit to the gate itself is the sharpest form: gated in
#    place it would run, and say DIRTY.
git checkout --quiet master
echo '// pushed' >> src/main.cpp
commit "pushed code"
sed -i 's/STUB:FULL/STUB:DIRTY/' scripts/local-ci.sh
out=$(git push origin master 2>&1)
if printf '%s' "$out" | grep -q 'STUB:FULL' && ! printf '%s' "$out" | grep -q 'STUB:DIRTY'; then
    PASS=$((PASS + 1))
    echo "  ok    PUSHED (an uncommitted gate edit is not what runs)"
else
    FAIL=$((FAIL + 1))
    echo "  FAIL  expected the committed gate to run, not the working tree's"
    printf '%s\n' "$out" | sed 's/^/          /'
fi
git checkout --quiet -- scripts/local-ci.sh

# 9. A secret is refused, and on a documentation-only push too: the scan runs
#    before the docs-only decision. The token is built at run time so this file
#    never holds one. It needs gitleaks; without it, it skips and says so.
if command -v gitleaks >/dev/null 2>&1; then
    tok="ghp_$(head -c 96 /dev/urandom | base64 | tr -dc 'A-Za-z0-9' | cut -c1-36)"
    echo "token = $tok" > docs/leak.md
    commit "a leaked token"
    if git push origin master >/dev/null 2>&1; then
        FAIL=$((FAIL + 1))
        echo "  FAIL  expected the push refused   (a token in docs/leak.md)"
    else
        PASS=$((PASS + 1))
        echo "  ok    SECRET (a token in docs/leak.md is refused)"
    fi
else
    echo "  skip  SECRET (no gitleaks here)"
fi

echo
echo "pre-push: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
