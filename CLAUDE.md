# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A Qt 6 Widgets game collection — a hub window holding fourteen games: Chess,
Reversi, Draughts, Minesweeper, Klondike, Spider, FreeCell, Pyramid, Sudoku,
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

cd build && ctest --output-on-failure   # both test binaries + the hook test
cmake --install build                   # refresh the installed copy
```

Run a test binary directly for its per-check output — `ctest` only reports
pass/fail:

```bash
./build/gameshub_selftest                             # all game rules
QT_QPA_PLATFORM=offscreen ./build/gameshub_uitest     # widgets and hub
tests/pre-push-test.sh                                # which arm the hook takes
```

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

**`scripts/local-ci.sh` reads its steps out of `.github/workflows/ci.yml`
rather than restating them.** That is the whole point: a hand-written mirror
of a pipeline drifts, and then passes locally for a build that fails on
GitHub. It executes the workflow's own `run:` blocks in the workflow's own
order, and **stops on any step it has no rule for** — a new action added to
`ci.yml` fails the local run until `STEP_RULES` in the script accounts for
it, because a silently skipped step is exactly the drift being prevented.

Two things it cannot do, and says so on every run rather than implying
coverage. **The Windows leg does not run here** — nothing on Linux drives
MSVC, so that half is verified by CI and nowhere else. And the `uses:` steps
are stood in for by this machine's own Qt and Ninja rather than executed.

The `pre-push` hook runs it automatically. A push touching only `.md` files,
`docs/`, or the licence texts runs the workflow linters and stops; anything
touching code, CMake or a workflow runs the full pipeline. `SKIP_LOCAL_CI=1
git push` bypasses it when you mean to.

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

## Releasing

Two workflows in `.github/workflows/`, contract in
`docs/specs/GHUB-0025-downloadable-builds.md`. `ci.yml` builds and runs both
test binaries on `ubuntu-24.04` and `windows-2022` for every push. `release.yml`
turns a tag into a Linux AppImage and a Windows zip on the releases page.

Cutting a release is three edits and a tag, **in this order**:

1. Bump `project(gameshub VERSION ...)` in `CMakeLists.txt`.
2. Close `## [Unreleased]` in `CHANGELOG.md` into `## [X.Y.Z] - <date>`, and
   leave a fresh empty `[Unreleased]` above it.
3. Commit, then `git tag vX.Y.Z && git push --follow-tags`.

The tag's `v` prefix is stripped and compared against `CMakeLists.txt`, and
the changelog must already have a `## [X.Y.Z]` block — the `verify` job
checks both before either build starts, because the release notes are read
from that block and a forgotten step would otherwise publish an empty one.
Get it wrong and the fix is to delete the tag; nothing is published.

Every action is pinned to a commit SHA with the version in a trailing
comment. That is not decoration: these workflows publish binaries that
strangers download, and a moved tag on a third-party action would run
arbitrary code against them. `actionlint`, `yamllint` and `zizmor` all pass
clean and are the check before pushing a workflow edit.

## Architecture

**Every game is a rules core plus a view, and the core never includes a
widget.** That split is the reason the whole collection is testable without a
display, and it is the rule to preserve when adding a game.

- `src/gameview.h` — `GameView`, the contract between hub and game: a QWidget
  that offers `gameActions()` for the toolbar and emits `statusChanged`. It also
  declares `applyLegibility(bool)`, called when the hub's legibility switch
  moves; the base constructor is what makes that connection, so **every**
  constructed game hears about it and not just the one on screen. `gameview.cpp`
  holds that constructor and nothing else — a Q_OBJECT class needs a matching
  source file in the build or AUTOMOC generates no metaobject for it.
- `src/legibility.*` — `Legibility`, the one app-wide persisted preference
  (`display/legibility`, default off). A singleton like `Sound`, but stored and
  broadcasting: games are built lazily and live for the session, so one built
  before the switch moved would never learn without the signal.
  `docs/specs/GHUB-0017-legibility-switch.md` is the contract, and the fourteen
  per-game passes that read the switch have not started yet.
- `src/hubwindow.*` — the tile grid and one page per game in a `QStackedWidget`.
  Games are constructed lazily on first open. Each tile paints its own
  miniature; `openGameNamed()` backs the `--game` flag. It also owns two things
  every game inherits: window size and position, kept **per page** so each game
  reopens the size it was left, and saved games — a game that overrides
  `GameView::saveState()`/`restoreState()` is stored on close and restored the
  next time it is opened, with no save dialog anywhere. An empty state means
  "nothing worth keeping" and clears the stored one, which is how a finished
  game avoids resuming onto its own final scores.
- `CMakeLists.txt` splits `GAME_CORE_SOURCES` (Qt-free or QtCore-only) from
  `GAME_VIEW_SOURCES`. `gameshub_selftest` links only the cores, so anything
  that pulls in QtWidgets belongs in the view half.

### Per game

- **Chess** — `chess/chessboard.*` is the rule set, wrapped in `namespace chess`
  because the draughts core already owns `Side`, `Piece` and `Square` at global
  scope and the self-test includes both. `Board` is the position alone —
  fixed-size, heap-free, copied per search node — while `ChessGame` adds the
  position history that threefold repetition needs, so the search never carries
  it. `chess/chessai.*` is negamax with alpha-beta, quiescence and iterative
  deepening. `chess/chessart.*` paints the pieces and is shared with the hub
  tile. `ChessView::advance()` is the single point that moves the game on.
- **Reversi** — `reversi/board.*` is the rule set, funnelled through
  `Board::ray()`, which walks one of eight directions and returns how many
  discs are bracketed. `reversi/ai.*` is negamax with alpha-beta over copied
  `Board`s (`Board` is a value type and the search relies on that). Depth is
  1/4/6 by difficulty. `ReversiView::advance()` is the single point that moves
  the game on — player move, engine reply and forced pass all route back into
  it. Change flow there, not by adding a second path.
- **Minesweeper** — `minesweeper/minefield.*`. Mines are laid on the *first
  reveal*, excluding that square and its neighbours, so the opening click is
  always safe and always opens a blank area. The flood fill is iterative; a
  recursive one overflows the stack on an Expert-sized blank.
- **Cards** — `cards/card.*` (deck building; `makeDeck(decks, suitsUsed,
  jokers)` is what gives Spider its difficulty and Canasta its 108-card pack)
  and `cards/cardart.*` (shared drawing, so the card games look like one deck).
  A joker is rank `kJoker` = 0, which sorts below every real rank and so never
  collides with the arithmetic other games do. `Card::deck` records which pack
  a card came from and decides only the colour of its back — Canasta shuffles
  two packs together, so its stock shows red and blue backs mixed the way a
  real table does. It is deliberately outside `operator==`: two red kings are
  the same card whichever pack they came from. `cards/cardcodec.*` is the third
  shared piece: piles in and out of a `QDataStream`, plus `fitsPack` /
  `matchesPack`, which are what a table-based save has instead of Chess's
  legal-move check — see the save trap below.
- **Klondike / Spider** — `klondike/`, `spider/`. Both keep piles as
  `std::vector<Card>` and drag by lifting a run off its pile into `m_drag`,
  restoring it on a failed drop. Card width is solved from the row cost
  (`7w + 6·gap` for Klondike) — assuming a fixed pixel gap pushed the last
  column off screen.
- **Hearts** — `hearts/heartsengine.*` holds the whole rule set: passing
  rotation, the forced two-of-clubs lead, following suit, no points on the
  first trick, hearts breaking, and the moon shot (26 to everyone *else*).
  `HeartsView` is presentation and timers only.
- **Canasta** — `canasta/canastaengine.*` is the rule set, in `namespace
  canasta` because `Meld`, `Team` and `Phase` are far too common to leave at
  global scope. **Every number the game plays by lives in one `Rules` struct**
  — card values, opening minimums, canasta bonuses, what freezes the pile —
  because house rules are the norm with Canasta rather than the exception. The
  game ships two sets: `Rules::classic()`, which is never edited, and a House
  set the player edits in a dialog and which is saved via QSettings. Adding a
  house variation should be a new field there, not a branch in the engine.
  `canasta/canastaai.*` is judgement rather than search, so unlike Chess it
  needs no work budget. **Its four levels are checked against each other, not
  just described** — `canastaLevelsDiffer()` plays each rung against the one
  below it, which is how Hard was caught being weaker than Medium. Any change
  to one level has to be re-measured against its neighbours.
  `canasta/canastaview.*` is presentation and timing.
- **Pinball** — `pinball/pinballtable.*` is the simulation in fixed table units
  (400×720), scaled to the widget so resizing never changes the physics.
  Everything collides as a circle against a fat line segment. `PinballView`
  only draws it and feeds input.

## Traps worth knowing

**Alpha-beta only resolves the BEST move's score exactly.** Every other root
move comes back as an upper bound, and a bad move whose search fails low can be
reported level with the best one. Chess's Easy and Medium levels pick at random
among moves within a few centipawns of the best, so they searched a full window
at the root (`rootScores`'s `exact` flag) — without it, Hard was playing
`Nf3-g1` from a normal opening because a fail-low tie sorted to the front. The
observable symptom is an engine that is strong in tactics and absurd in quiet
positions, which reads as a bad evaluation rather than a bad window.

**A chess move generator is proved by perft, not by eyeballing.** Counting
every leaf to a fixed depth and matching the published totals for a handful of
reference positions catches castling-through-check, en-passant and pin bugs
that no amount of playing will surface reliably. `chessMoveGeneration()` in the
self-test checks four positions and runs in about 20 ms.

**The engine searches on the GUI thread, so it is bounded by a node budget
rather than by depth alone.** `planFor()` in `chessai.cpp` sets one per level;
Hard's worst observed middlegame answer is about 1.2 s. Raising the depth
without raising the budget does nothing, and raising both freezes the window.

**Pinball's launch is calibrated, not guessed.** `minimumLaunchSpeed()` derives
the weakest plunger from the dome height above the lane, so even a limp launch
reaches the play field. The original build used a hardcoded 620 that lifted the
ball less than half way; it fell back down a then-open-bottomed lane and
drained, losing a ball before the player touched a flipper. The lane is now
walled to the floor and a short launch re-parks the ball. `pinballLaunch()` in
the self-test locks this in across all plunger strengths.

**The one-way gate at the mouth of the lane is deliberately never drawn.** As a
visible line it reads as a wall sealing the launch lane — that was a reported
bug, not a theory.

**Card corner text needs room for two characters and a descender.** A box half
a card wide clipped "10" to a stray stroke and cut the tail off "Q".

**`slots` is a Qt keyword macro and expands to nothing.** A local named
`slots` compiles as `const int = ...` and the error points at the `=`, which
reads as a parser bug rather than a name collision. `signals` and `emit` are
the same. Canasta's meld layout hit this.

**`CardArt::paintFace` stops drawing the face below `CardArt::kFaceMinWidth`
(46) pixels wide** and leaves
only the corner index, because pips are unreadable smaller than that. That
constant in `cardart.h` is the number's only definition, and
`scripts/legibility-check.py --thresholds` fails if any other source states it
as a literal. A game holding the threshold must hold it at the **smallest scale
it draws a card at**, not at 1.0 — Canasta's melds at 0.74 need `cardWidth()`
≥ 62.2, not 46. Anything
drawing cards at reduced scale — Canasta's melds are `kMeldScale` = 0.74, its
opponent hands 0.8 — gets a stack of
slivers rather than cards, and has to name them some other way. The melds carry
a "K ×5" badge for exactly this reason. Check the width before assuming a face.

**Canasta's legibility pass raises the window's minimum size; it does not grow
the melds where they stand.** Growing them in place was tried first and cannot
work: a meld card wide enough to show a face makes a seven-card canasta about
130 px tall at the smallest window, and `bandFor()` gives it 107 — the overflow
runs into the stock and discard row above. So `minimumSizeHint()` returns
900×656 while the switch is on, the smallest window at which `cardWidth()`
reaches `CardArt::kFaceMinWidth / kMeldScale` unaided, and every card on the
table grows together. **Floor, smallest scale and minimum size move together or
not at all**; `cardsFitTable()` is what asserts the floor never actually has to
clamp, because a clamped card is one the table has no room for. **And the switch
has to put the window back** — Qt clamps the window up to the new minimum and
`HubWindow::rememberPage()` writes that enlarged geometry over the stored one,
so `applyLegibility` keeps the pre-clamp size and restores it on the way out. It
clears `m_flights` first: `Flight::to` is a point captured when the card left,
so a card in the air would otherwise land where its destination used to be.

**The other five card games do not need a size pass, and the floors that say
they do are unreachable.** Measured, not assumed: every card view calls
`setMinimumSize(minimumSizeHint())`, so the smallest card each can actually
reach is Klondike 67.9, FreeCell 67.4, Pyramid 68.6, Spider 54.2 and Hearts
52.5 — all clear of `kFaceMinWidth`. Their `std::max(30.0, …)` … `std::max(34.0,
…)` floors read alarming and no window can drive them there. Canasta was the
only game drawing faceless cards, and only in its melds: the opponents' hands
are drawn at 0.8 but face **down**, so the threshold never applied to them.

**`QPainter::drawText(rect, flags, text)` clips to the rect, so a font is
bounded by its LINE box and not by its ink.** Sudoku's pencil marks are the
case: nine sit in a fixed 3×3 pattern inside one cell, each centred in a cell
third, and at `cell * 0.20` the marks were already close to the largest whose
line box fits that third — so raising the ratio alone clips the top off every
mark and draws a *worse* mark, not a bigger one. `Qt::TextDontClip` hands over
the gap between the ink and the line box, and the legibility pass is what uses
it. The flag is set in both switch states because at 0.20 it changes nothing.
**Derive the ceiling from the ink, never from the point size:** this app font's
digits are about 0.685 of an em, so mark ink lands at roughly `2.74 × ratio` as
a fraction of the cell third. `SudokuView::marksFitCell()` measures it with
`QFontMetricsF::tightBoundingRect` over all ten digits rather than trusting that
constant, because a platform with taller digits should fail the check rather
than draw marks that touch. 0.30 was tried and goes red at the smallest window,
where a 34-pixel cell rounds the ink up a whole pixel.

**Canasta can reach a position with no legal move, and the engine has to refuse
the move that gets there.** Down to one card with no canasta, you may not go
out, and discarding your last card *is* going out — so nothing is legal and the
turn cannot end. The guard is in `keepsADiscard()`, which refuses a lay-down
leaving fewer than two cards unless a canasta comes with it. Without it the
self-test's full games hang rather than fail, which is a much worse symptom.

**The owner is partially sighted, and reads cards by their pip pattern rather
than the corner index.** That is a design constraint, not a preference. It is
why melds put their wild cards first, why melded cards are drawn at 0.74 rather
than at the smallest scale that fits (below 46 pixels wide `CardArt::paintFace`
gives up on the face entirely), why the computer's pause is nearly a second,
and why the last discard is spelled out in words under the centre of the table
rather than left to be read off the pile. Anything added here is checked
against "can this be read slowly?" before "does this look neat?".

**A game's save is the moves that made it, not the position it reached** —
where the game keeps a history at all. Chess saves its move list and replays it
through `ChessGame::play()`, which rebuilds the board, the undo stack and the
position keys threefold repetition counts from one list; storing the position
instead would need a second copy of each, free to drift. Each move is matched
against `legalMoves()` on the way back in, so the file supplies from/to/promotion
and the generator supplies the castling and en-passant flags — a save that is
not a game this build would play is refused rather than half-loaded. Canasta
cannot do this (it has no move log, so its engine serialises directly), which is
why the two look different; prefer Chess's shape when a game offers the choice.

**A game with no move log saves the table, and then the PACK is what re-checks
it.** The four solitaires keep piles rather than moves, so there is nothing to
replay against the rules. What stands in for that is `cardcodec::matchesPack` —
Klondike and FreeCell never take a card out of play, so the whole deck must come
back, nothing missing and nothing doubled. Spider and Pyramid do remove cards
(a harvested run, a matched pair), so they get `fitsPack` plus a count of their
own. Without that check a corrupt blob restores into a deal that cannot be won,
and the player finds out an hour later. Minesweeper, Reversi, Draughts and 2048
have no pack at all, so each core's `restore()` is the equivalent and refuses a
board the game could not have reached: the mine count must match the level and
the numbers are recomputed rather than read, a Reversi board must hold at least
the opening four discs, a draughts piece may not stand on a light square, and
every 2048 tile must be a power of two. **A drag is the other half:** a run
lifted in mid-drag has been erased from its pile and lives in `m_drag` until it
is dropped, so each `saveState()` writes it back onto the pile it came from —
otherwise closing the window with a card in hand loses it, and the pack check
then refuses the save it just wrote.

**Card order that the eye depends on belongs in the model, not the painter.**
A hand fans wild-cards-first and so does every meld, and both orders are made
by `canasta::sortsBefore` inside the engine. That is not a layering slip: the
flights index into `Meld::cards` and into the hand to work out where a card is
flying, so a display order held only in the painter would put the animation in
one place and the card in another. Sort before the flights are built.

**A card game with animation must not let the model and the picture disagree.**
Each flight carries where it is going, and the destination skips drawing that
card until it lands (`suppressed()`); otherwise a card in the air is also drawn
at its destination and the eye sees it twice. The matching one-per-flight, so
several cards arriving at once each get their own slot.

**A UI check that clicks a Canasta card has to wait twice** — once for the turn
to come round to the human, and again for the cards to land, since the board
ignores clicks while anything is animating. Testing only the first produced a
suite that failed about one run in three.

**A seed does not mean the same deal on two compilers, so `shuffleCards` is
a hand-written Fisher-Yates.** The standard pins down what `std::mt19937`
emits but not how `std::shuffle` consumes it, and `std::uniform_int_distribution`
is unspecified the same way — so libstdc++ and MSVC's library produce
different permutations from identical state. Every seeded check here would
then play a different game on Windows. That is not theory: Canasta's AI
strength ladder passed on this machine and failed on the Windows runner with
no difference in the engine at all, which reads as a broken AI rather than a
broken shuffle. `card.cpp` now draws its index from `rng()` directly.
**`minefield.cpp` and `sudokugrid.cpp` still call `std::shuffle`** — nothing
asserts their sequence across platforms today, but a new test that seeds
either one needs the same treatment first.

**`QSettings` has no file on Windows.** It writes to the registry there, so
`QFile::exists(QSettings().fileName())` is false however well saving works —
a persistence check has to construct a fresh `QSettings` and read the value
back instead. That assertion cost a red Windows run for a feature that was
working.

**`M_PI` does not exist on MSVC, so this codebase uses `std::numbers::pi`.**
MSVC's `<cmath>` defines `M_PI` only if `_USE_MATH_DEFINES` was defined
before it was included, so the seven uses that lived here compiled on GCC
and would have failed the Windows build. `<numbers>` is C++20, which the
project already targets, and it needs no build flag to be load-bearing for a
maths constant. A new painter reaching for `M_PI` is caught only by the
Windows CI job, minutes later.

**`--version` is answered from `argv` before `QApplication` exists, and it
must stay that way.** `qt_add_executable` sets `WIN32_EXECUTABLE`, so the
Windows binary is a GUI-subsystem process: `QCommandLineParser::showVersion()`
puts the version in a *message box* there instead of on stdout, and the
release workflow's smoke test would hang waiting for a dialog no runner can
close. Going through `argv` also means `--version` needs no display and no
platform plugin anywhere, which is what makes it usable as a packaging check
at all. Folding it back into the parser looks tidier and breaks the release.

**An AppImage is not a closed box — linuxdeploy deliberately leaves 53
libraries to the host**, `libGL.so.1`, `libxcb.so.1` and `libX11.so.6` among
them, because a bundled graphics stack breaks against the host driver. So
"self-contained" here means *carries its own Qt*, not *runs on an empty
filesystem*, and the release workflow's clean-room container installs that
baseline before testing. Adding anything Qt to that container would make the
test prove nothing.

**Each smoke test runs the artifact twice, and only the second run loads
Qt.** `--version` returns before `QApplication` exists, so on its own it
proves the file unpacks and reports the right version and nothing more — a
bundle with no platform plugin passed every gate that way, which is how 0.3.0
shipped carrying only `xcb`. The second run starts `--game spider` under
`QT_QPA_PLATFORM=offscreen` and **passes only if the app is still alive when
the timeout fires**, so `timeout`'s own 124 is the success status and any exit
of the app's own is the failure. **On Windows liveness alone is not enough,
and assuming it was would pass the failure the check exists to catch:** a
release build with no console does not exit when the platform plugin will not
load — Qt shows a blocking message box first (guarded by `!isDebugBuild() &&
!GetConsoleWindow()`) and reaches `qFatal` only when someone dismisses it,
which on a runner nobody does. So that run adds `-NoNewWindow` and asserts the
process owns no top-level window; under offscreen a healthy app opens none, so
a window is the dialog. Both artifacts bundle the offscreen plugin
for it: Linux via `EXTRA_PLATFORM_PLUGINS=libqoffscreen.so` (not
`EXTRA_QT_PLUGINS`, a deprecated alias for `EXTRA_QT_MODULES` that matches
modules and would silently match nothing), Windows by copying
`qoffscreen.dll` from the Qt install beside `windeployqt`. **The running
check cannot see a missing desktop plugin**, and that is not a detail: it
asks for offscreen, so it loads `libqoffscreen.so` / `qoffscreen.dll` and
never touches `libqxcb.so` / `qwindows.dll` — a bundle missing the plugin
every player needs starts fine under it. The staged-tree assertions name both
platform plugins for exactly that reason, and they are the *only* guard for
the desktop one. They use different paths per platform: `linuxdeploy` keeps
Qt's `plugins/<group>/` layout, while `windeployqt` mirrors each group into
the deployment root. A missing *multimedia* plugin is likewise caught only
there — the app runs in silence rather than failing.

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

**`qt_add_resources(<target> <file>.qrc)` silently compiles an EMPTY resource.**
That signature expects a resource name plus a `FILES` list, so handing it a
`.qrc` gives no error, no warning and no content — every sound lookup then
fails at runtime. Sounds are embedded by listing `sounds.qrc` as a target
source with `CMAKE_AUTORCC ON`. The binary is ~800 KB with the audio and
~425 KB without, which is the quickest check; `tests/uitest.cpp` asserts every
effect is present and non-trivial in size.

**QPainter's `drawRect` fills with the current brush as well as outlining it.**
A brush set for one thing leaks into everything drawn after it: the flag's red
brush turned every dug Minesweeper square red, and only from the first flag
onward, because painting runs row by row. Set the brush — or `Qt::NoBrush` —
immediately before a shape call rather than trusting what came before.

**Sounds are generated, never sampled.** `tools/make_sounds.py` synthesises all
17 effects from a fixed seed, so re-running it is byte-identical. That is a
licensing decision rather than a stylistic one — see `ROADMAP.md` § Standing
rules before adding any asset.

## Testing notes

Wayland blocks synthetic clicks and no injection tool is installed, so GUI
testing is: construct widgets offscreen, `render()` them into a QPixmap to
force `paintEvent` through, and click the hub's tiles via `QPushButton::click`.
Anything needing real pointer input has to be verified by eye instead.

The self-test is where game logic gets proven — it plays 200 random Reversi
games, 20 full AI Hearts games, 18 full AI Canasta games at three strengths,
and flies pinballs. Prefer adding a check there over a UI test.

Two patterns from Canasta worth reusing. `Engine::newGameFromStock()` deals
from a stock the caller supplies, so a check can build an exact position
instead of hunting for a seed that produces one. And the scoring table and the
opening bands are free functions (`handScoreFor`, `openRequirementFor`) rather
than private methods, so they can be checked directly on a hand-built position
rather than one played into existence.
