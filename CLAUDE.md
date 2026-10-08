# CLAUDE.md

Genre: record

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A Qt 6 Widgets game collection — a hub window holding fourteen games: Chess,
Reversi, Draughts, Minesweeper, Solitaire, Spider, FreeCell, Pyramid, Sudoku,
Hearts, Canasta, Snake, 2048 and Pinball. Started 2026-08-10 as a single
Reversi game and expanded the same day. `ROADMAP.md` holds the queue of games
still to come.

## Commands

```bash
# Configure (once, or after editing CMakeLists.txt)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local"

cmake --build build                     # build everything
./build/gameshub                        # the hub
./build/gameshub --game spider          # straight into one game

# --game takes the game's id, which is what the tile shows until a translation
# loads: a translation changes the tile, never the id (GHUB-0161). Klondike's id
# is "Solitaire" (Klondike is its blurb), and an unknown name warns and opens
# the tile grid rather than failing.

cd build && ctest --output-on-failure   # both test binaries, the --shot runs,
                                        # the hook test, and the Python checks
                                        # -- scorepad (the phone score book's
                                        # numbers against the game's),
                                        # legibility (the kFaceMinWidth
                                        # threshold) and translatable (every
                                        # word a player reads goes through tr())
cmake --install build                   # refresh the installed copy
```

**Build options, hardening and how the build is laid out** are in
`.claude/rules/build.md`, which loads when `CMakeLists.txt`, `cmake/` or a workflow
is opened.

**Photographing a game (`--shot`, `--seed`, `--turns`) and timing a frame
(`--bench`)** are in `.claude/rules/shots.md`, which loads when a file under
`src/` is opened. Use a shot before reasoning about a layout: it is the only
thing in the project that can see one.

Run a test binary directly for its per-check output — `ctest` only reports
pass/fail:

```bash
./build/gameshub_selftest                             # all game rules
QT_QPA_PLATFORM=offscreen ./build/gameshub_uitest     # widgets and hub
QT_QPA_PLATFORM=offscreen ./build/gameshub_uitest --bench   # frame cost alone
QT_QPA_PLATFORM=offscreen ./build/gameshub_uitest --write-saves  # see .claude/rules/tests.md
tests/pre-push-test.sh                                # which arm the hook takes
python3 scripts/scorepad-check.py                     # score book vs the game
python3 scripts/legibility-check.py --thresholds      # kFaceMinWidth is unique
python3 scripts/translatable-check.py                 # every word is tr()'d
```

Which ctest cases register on which platform, and why an absent checker
registers a failing case: `.claude/rules/tests.md`.

`CMAKE_INSTALL_PREFIX` must be set at **configure** time, not passed to
`cmake --install --prefix`. The `.desktop` file bakes in an absolute `Exec`
path via `configure_file`, so a late `--prefix` installs the binary correctly
while leaving `Exec=/usr/local/bin/gameshub` pointing at nothing.

The panel launcher runs the **installed** copy, so re-run `cmake --install`
after changing code or the pinned icon keeps launching the old build.

## Run the pipeline locally before pushing

```bash
git config core.hooksPath .githooks   # once per clone
scripts/local-ci.sh                   # or just push; the hook runs it
```

Four things a plain run does not cover, and it says so every time rather than
implying coverage. **The Windows leg does not run here** — nothing on Linux
drives MSVC, so that half is verified by CI and nowhere else. The `uses:` steps
are stood in for by this machine's own Qt and Ninja rather than executed. And
the **sanitizer** and **clang-tidy** legs are off unless asked for:
`--with-sanitizers` and `--with-tidy`. The `pre-push` hook passes neither, so
the push gate does not touch them either — `.claude/rules/release.md` has the whole
list of what CI runs.

The `pre-push` hook runs it automatically. A push touching only `.md` files,
`docs/`, `.gitignore` or the licence texts runs the workflow linters and stops; anything
touching code, CMake or a workflow runs the full pipeline. `SKIP_LOCAL_CI=1
git push` bypasses it when you mean to.

**Every push is first scanned for secrets, and `SKIP_LOCAL_CI=1` does not skip
that.** The hook hands the pushed commits to the machine-wide hook's
`--secrets-only` mode, which runs gitleaks; a finding refuses the push. Where
that hook is absent the push goes ahead and the hook says no scan ran.

How `local-ci.sh` reads `ci.yml`, and the rule the hook keeps across refs:
`.claude/rules/ci.md`, which loads when the hook, `scripts/local-ci.sh` or a
workflow is opened.

## Committing

**One commit per roadmap item where the work splits cleanly; group them when
splitting would produce a commit that does not build.** Owner's call,
2026-08-20, settling a question that had been decided in practice several
times and written down nowhere. The grouped case is real rather than
hypothetical: GHUB-0041 and GHUB-0042 both rebuilt `HubWindow::buildChrome()`,
so either half alone was a broken tree.

**Either way the BODY names every ID the commit covers, one at a time.** The
subject may abbreviate a run as `GHUB-0066..0070`, and this history does — but
that form contains no literal `GHUB-0067`, so a tool grepping for one finds
nothing. The body is what makes a grouped commit auditable, and every grouped
commit here already enumerates its IDs there. Check the body, not the subject,
before believing an ID never shipped.

## Releasing

The release recipe, what CI runs, and the packaging traps are in
`.claude/rules/release.md`. It loads when `CHANGELOG.md`, `CMakeLists.txt`,
`SECURITY.md`, `.claude/bump.json` or a workflow is opened. Read it before
cutting a release.

## Core rules

**How the code is shaped, and why, is `docs/design.md`.** It holds the parts,
the `GameView` contract every game follows, saves, legibility, and a section
per game. Read the section for what you are changing before you change it.

**A game is a rules core plus a view, and the core never includes a widget.**
That split is what makes a game's rules testable without a display, and it is
the rule to hold when adding one.

- `CMakeLists.txt` splits `GAME_CORE_SOURCES` from `GAME_VIEW_SOURCES`, and
  `gameshub_selftest` links only the cores. **Two things put a file in the view
  half, not one.** Pulling in QtWidgets is the obvious one. The other is being
  something a rules core has no business reading — a display preference, a
  stored score, a session counter — **even when the file is QtCore-only**, and
  that is not a style rule: it is what keeps the self-test from acquiring a
  dependency on stored state. `legibility.cpp`, `scores.cpp` and `donate.cpp`
  are all QtCore-only and all live in the view half; `legibility.cpp` carries
  the reason inline. Judging by the QtWidgets test alone puts a new preference
  store in the core half, where it links, passes, and tells you nothing.

  **Since GHUB-0187 the compiler catches the FIRST of those two and still not
  the second, and the difference is the whole point of this bullet.**
  `gameshub_core` links `Qt6::Core` alone, so a core file that includes a
  widget does not build. A core file that reads a stored score compiles
  perfectly, links, and passes — exactly as before. Do not read "the split is
  enforced now" as covering the half that has never had a mechanical check.

**The owner is partially sighted, and reads cards by their pip pattern rather
than the corner index.** That is a design constraint, not a preference. It is
why melds put their wild cards first, why melded cards are drawn at 0.74 rather
than at the smallest scale that fits (below 46 pixels wide `CardArt::paintFace`
gives up on the face entirely), why the computer's pause is nearly a second,
and why the last discard is spelled out in words under the centre of the table
rather than left to be read off the pile. Anything added here is checked
against "can this be read slowly?" before "does this look neat?".

## Traps worth knowing

These bite any change, whatever game it touches. A trap about one game or one
shared part is in `docs/design.md`, with that game or part.

Code traps (Qt, MSVC, the Windows runner) are in `.claude/rules/code-traps.md`, which
loads when a file under `src/`, `tests/`, `tools/` or `assets/` is opened.
Packaging traps are in `.claude/rules/release.md`, and comparing shots in
`.claude/rules/shots.md`.

**`pkill -f <pattern>` will kill this session's own shell** when the pattern
appears in the command line being run. Use `pkill -x gameshub`.

**Chaining `cmake --build` and a test binary on one shell line races the
linker, and the failure it produces reads as a real one.** `cmake --build build
&& ./build/gameshub_selftest` can run the *previous* binary, or one being
rewritten, and what comes back is a plausible FAIL against a check you did not
touch — twice in one session it was reported as a broken Minesweeper test that
was in fact green. Build, then run as a separate command, or `sleep 1` between
them. The same shape in reverse is the well-known one: a green test over a
stale binary. Red is the more expensive direction, because it sends you
debugging code that is not broken.

## Review history

When this file's review gate is owed, and its run history:
`.claude/rules/claude-md-review.md`, which loads when `CLAUDE.md` or a rules file
is opened.

## Testing notes

Wayland blocks synthetic clicks and no injection tool is installed, so GUI
testing is: construct widgets offscreen, `render()` them into a QPixmap to
force `paintEvent` through, and click the hub's tiles via `QPushButton::click`.
Anything needing real pointer input has to be verified by eye instead.

The self-test is where game logic gets proven — it plays 200 random Reversi
games, 20 full AI Hearts games, 18 full AI Canasta games at three strengths,
and flies pinballs. Prefer adding a check there over a UI test.
