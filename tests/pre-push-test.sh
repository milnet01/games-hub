#!/usr/bin/env bash
# Proves which arm of .githooks/pre-push a given push takes.
#
# This exists because the hook's whole job is a decision — lint-only or full
# pipeline — made from data git hands it on stdin, and a wrong decision is
# SILENT: the push succeeds either way. GHUB-0027 was exactly that. A release
# push ran the linters and nothing else for as long as the hook had existed,
# and the only reason it was ever noticed was someone reading the output.
#
# So the test drives real `git push` calls against a real (bare, local) remote
# rather than hand-feeding the hook a stdin format that might be wrong. The
# throwaway repo carries its own scripts/local-ci.sh, a stub that announces
# which way it was called; the hook resolves that path relative to the repo
# root, so no seam is needed in the hook itself.
#
#     tests/pre-push-test.sh          # run it
#
# Skipped, not failed, when git is absent.

set -uo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
HOOK="$ROOT/.githooks/pre-push"

command -v git >/dev/null 2>&1 || { echo "SKIP: git not installed"; exit 0; }
[ -x "$HOOK" ] || { echo "FAIL: $HOOK is missing or not executable"; exit 1; }

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

mkdir -p .githooks scripts src docs
cp "$HOOK" .githooks/pre-push
chmod +x .githooks/pre-push
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
# <what> is LINT, FULL, or NONE (the hook declined to check anything).
expect() {
    want=$1; shift
    out=$(git push "$@" 2>&1)
    case "$want" in
        LINT) got=$(printf '%s' "$out" | grep -c 'STUB:LINT') ;;
        FULL) got=$(printf '%s' "$out" | grep -c 'STUB:FULL') ;;
        NONE) got=$(printf '%s' "$out" | grep -c 'nothing to check') ;;
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

# 1. The very first push: code, so the full pipeline. Also establishes the
#    remote-tracking refs every later case leans on.
echo 'int main(){}' > src/main.cpp
echo '# hub' > README.md
commit "initial"
expect FULL -u origin master

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

echo
echo "pre-push: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
