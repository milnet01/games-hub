---
paths:
  - "tests/**"
  - "src/**"
  - "scripts/*.py"
  - "CMakeLists.txt"
---

## Which test cases register where

Moved from `CLAUDE.md` § Commands.

The Python scripts are pure Python and run as ctest cases, so a check skipped
by hand is caught there. **The registered count differs by platform, and the
Python cases and the hook test separate the extremes** — the Python cases need
an interpreter CMake will use, and the hook test is bash and Unix-only. Linux
registers all of them; a Windows runner registers every one but the hook test;
a Windows box whose only `python.exe` is the Store stub registers no Python case
either. The configure step says when it drops the Python cases.

**An absent checker registers a FAILING case rather than none at all, and that
is the rule a new tool-gated case has to follow.** A configure with no bash or
no python used to drop the test silently, and ctest then reported everything
passing over a shorter suite — the same silent skip `scripts/local-ci.sh`
refuses by name. So the `else()` branch is not optional: `prepush_needs_bash`,
`scorepad_needs_python`, `legibility_needs_python` and
`translatable_needs_python` exist to be red.

**A case is genuinely dropped only where the platform is exempted on purpose,
and there are two of those, not one.** The Python cases where there is no
interpreter CMake will use AND the platform is not Unix — configure prints why.
**That is not a Windows exemption**: the guard is `if(Python3_Interpreter_FOUND)`
with no platform test at all, so a Windows runner that HAS an interpreter runs
them — which is how a Linux-only `grep` pipeline inside one of them reddened
the Windows leg six times. And the hook test on any non-Unix platform, which is why its stub
sits INSIDE `if(UNIX)` rather than beside it: bash is what it runs, so a red
stub on Windows would be an alarm with nothing behind it. Put a new Unix-only
case's `else()` inside the same guard. **Count them with `ctest
-N` rather than against a figure here** — the number moves whenever a case is
added, and a stale one sends you hunting for a case that was never registered
on that platform.

## Testing notes

Moved from `CLAUDE.md` § Testing notes.

**`tests/saves/` is a committed corpus of real saves, one per saving game, and
the UI test restores every one of them.** A save-format change that refuses the
old format is not a crash — the hub keeps the fresh deal it just made, the app
runs, nothing looks broken, and the player's half-finished game is gone without
being told. `docs/standards/versioning-overrides.md` § 1 names that a breaking
change; this corpus is what enforces it. **When it reddens, the choice is write
a migration or take the break deliberately — never regenerate the corpus to
make the check green**, which deletes the only evidence the format moved.

**The deliberate break has an end state, or it is just a red suite forever.**
Replace that game's corpus file in the SAME commit that records the break, so
the diff shows the old save going and the reason for it arriving together.
`--write-saves` is how you do that; what the rule forbids is reaching for it
with nothing recorded.

**It is not a reason to cut `1.0.0`.** § 2 of that same document keeps MAJOR at
0 until the last of its six items ships, so a reddened corpus inside `0.x` is
not a MAJOR bump waiting to happen — what a breaking change costs at this
version is the versioning standard's answer, not this file's. `--write-saves` rewrites it and is
for adding a game, not for making the check pass.

The corpus is written by `startedSave()`, which is also what the mutation fuzz
seeds from: most games answer "nothing worth keeping" for a board nobody has
moved in, so it pokes at the surface until something is worth saving, and
freezes the clock first because Minesweeper and Sudoku save a running elapsed
figure. Snake and Pinball offer no save and are absent by design.

**A test that builds a position with a WILD or a red three as the up-card gets
one fewer card in the stock than it looks.** `Engine::dealFrom` covers such a
turn-up with another card off the stock, so the pack starts at two and every
subsequent draw shifts down by one — a card placed at `kBelowCount - 5` for a
seat's second draw arrives at `kBelowCount - 6` instead. The symptom is a check
whose hand simply does not contain the card it was built around, which reads as
a broken AI. Cost a debug cycle on `canastaFirstCanastaIsInsurance`; both that
check and `canastaAiOpensOnAJokerToKeepThePair` say which case they are in
where their stock is built.

**`canastaLevelsDiffer()` prints its four rungs on every run, and what the
ladder reads today is the baseline for judging tomorrow's change** — the figures
are re-read by running the suite, never assumed from a handoff. As of
2026-09-02, after GHUB-0129: medium v easy 22/24 +2538, hard v easy 22/24
+3547, hard v medium 69/120 +424, expert v hard 121/240 -1.

**Two rungs moved that day — hard v medium and expert v hard — and both causes
are known rather than suspected.**
GHUB-0148 made validateTake subtract the pile's red threes before the
no-legal-move guard, so a take that would strand the seat on one card is now
refused -- and the AI made some of those takes. That alone took expert v hard
from 121/240 +14 to 116/240 -183. GHUB-0129 then widened `worthHolding` to Hard
and graded its pile floor, which put hard v medium up from 66/120 +236 and
expert v hard back to level. **Two different things get called noise here, and
they pull opposite ways.** Run to run there is none: the shuffle is seeded and
hand-written, so two consecutive runs give the same figures to the digit and a
rung that moved really did move. Across DEALS there is plenty — 24 or 120 games
is a small sample — which is what GHUB-0110 settled and why a moved rung is not
by itself evidence that a level got stronger. Do not read the GHUB-0148 dip as a
regression introduced by an AI change; it was the price of a correctness fix.
**A rung that does NOT move is the useful reading when a change is gated to
some levels** — GHUB-0124 touched Expert alone, so the first three not moving is
what said so, and GHUB-0129 touched Hard and Expert, so medium v easy standing
unchanged to the digit is what says that. Two things
that reading does NOT mean. The absolute numbers are not a target — GHUB-0110
settled that the ladder cannot separate a small change from noise, so a check
against a hand-built position is the judge and this is context. And **the ladder
plays default Rules**: `canastaMatch` builds a bare `ca::Engine`, so
`canastaNeededToScore`, `deadHandIfNobodyGoesOut` and every other House flag are
OFF there. Anything gated on a house rule is invisible to it, and a flat reading
is then evidence of nothing at all.

Two patterns from Canasta worth reusing. `Engine::newGameFromStock()` deals
from a stock the caller supplies, so a check can build an exact position
instead of hunting for a seed that produces one. And the scoring table and the
opening bands are free functions (`handScoreFor`, `openRequirementFor`) rather
than private methods, so they can be checked directly on a hand-built position
rather than one played into existence.
