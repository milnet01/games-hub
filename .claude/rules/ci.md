---
paths:
  - ".githooks/**"
  - "scripts/local-ci.sh"
  - "scripts/wintest-ci.sh"
  - "tests/pre-push-test.sh"
  - ".github/**"
---

## The local pipeline and the pre-push hook

Moved from `CLAUDE.md` § Run the pipeline locally before pushing.

**`scripts/local-ci.sh` reads its steps out of `.github/workflows/ci.yml`
rather than restating them.** That is the whole point: a hand-written mirror
of a pipeline drifts, and then passes locally for a build that fails on
GitHub. It executes the workflow's own `run:` blocks in the workflow's own
order, and **stops on any step it has no rule for** — a new action added to
`ci.yml` fails the local run until `STEP_RULES` in the script accounts for
it, because a silently skipped step is exactly the drift being prevented.

**The hook reads one line per ref and must accumulate across all of them.**
`git push --follow-tags` sends the tag *last*, and a new ref has no remote sha
— so a hook that let the last ref decide diffed a bare sha against the working
tree, found a clean tree, and called a release push a documentation change. It
ran lint-only on every release and every new branch, silently, because a push
succeeds either way. `tests/pre-push-test.sh` is the guard: it drives real
pushes at a throwaway remote and asserts which arm each one takes. Keep the
docs-only arm in it — that path is a feature, and the obvious "fix" of always
running the full pipeline deletes it.

**Two traps this script hit while being written**, both of which produce a
green run that checked nothing. `$(...)` strips NUL bytes, so the
NUL-separated step list came back empty and the run "passed" having executed
zero steps — hence the `STEPS_RUN` guard. And a newline-separated record
splits a multi-line `run:` block mid-body, so a step executes only its first
line, silently.
