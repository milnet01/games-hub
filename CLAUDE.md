# CLAUDE.md

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

# --game takes the REGISTERED name, which is what the tile shows. Klondike is
# registered as "Solitaire" (Klondike is its blurb), and an unknown name warns
# and opens the tile grid rather than failing.

# Photograph a game instead of playing it. Needs no display, no compositor and
# no injection tool, so it works here under Wayland, over plain SSH, and on a
# CI runner. --legible turns large play on for the shot without writing it to
# settings. Use it before reasoning about a layout: this is the only thing in
# the project that can SEE one.
QT_QPA_PLATFORM=offscreen ./build/gameshub --shot /tmp/hearts.png \
      --game hearts --size 1400x620 --legible

# Unlike playing, an unknown --game REFUSES rather than falling back to the
# grid, and a malformed --size refuses rather than picking another size. A
# picture of the wrong thing is the one failure a screenshot cannot survive:
# it still gets written, and it still looks like an answer.

cd build && ctest --output-on-failure   # both test binaries, the two --shot
                                        # runs and the hook test
cmake --install build                   # refresh the installed copy
```

**`--shot` photographs a game the moment it opens, which for a card game is a
deal and nothing else.** Any layout that only appears once a hand has been
played — Canasta's melds, its canasta stack, a frozen pack — is invisible to
it. Getting one on screen took a **throwaway harness**, and the recipe is
written down here because rebuilding it is half an hour and every piece of it
is non-obvious:

1. `kAiPause` to `0.0` in `canastaview.cpp`, or the computers move once a
   second and nine seconds buys nine moves.
2. The `else if (m_engine.currentSeat() != 0)` guard in `CanastaView::tick()`
   to `else if (true)`, so seat 0 plays itself. Without it the game stops dead
   on your turn and waits, which looks like the fast-forward not working.
3. A spin in `takeShot()` before `window.grab()` — `QElapsedTimer` plus
   `processEvents` for ~9 seconds.
4. `XDG_CONFIG_HOME` pointed at a scratch directory holding a hand-written
   `GamesHub/Games.conf`, so **the owner's real settings are never touched**.
   It needs `useHouse=true` as well as the `house\...` keys — the rule set in
   force is a separate setting, and with it missing the toolbar comes up
   Classic and a house-rule layout never appears at all.
5. `house\canastaSize=4` to make canastas form in one hand instead of five.

Then revert all of it. **`git diff` is the check** — the harness must not
survive into a commit.

**Two things this found that no arithmetic in the project had flagged**: a
four-canasta stack whose name badges landed on top of one another, and the
right-hand end of that stack hanging over the edge of the band. GHUB-0093
(a `--seed` flag) would make the shots comparable; a `--turns` flag would
retire steps 1 to 3 and has not been asked for.

Run a test binary directly for its per-check output — `ctest` only reports
pass/fail:

```bash
./build/gameshub_selftest                             # all game rules
QT_QPA_PLATFORM=offscreen ./build/gameshub_uitest     # widgets and hub
QT_QPA_PLATFORM=offscreen ./build/gameshub_uitest --bench   # frame cost alone
tests/pre-push-test.sh                                # which arm the hook takes
```

**`--bench` is the only thing here that can time a frame**, and it is what
makes a painting change provable. It prints ms/frame for Canasta mid-deal and
at rest, a full Klondike tableau, a FreeCell board and the tile grid, and
**reports rather than asserts** — a frame time is a property of the machine.
Take it before and after, never one reading in isolation.

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
`docs/`, `.gitignore` or the licence texts runs the workflow linters and stops; anything
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

Two workflows in `.github/workflows/`, contract in
`docs/specs/GHUB-0025-downloadable-builds.md`. `ci.yml` builds and runs both
test binaries on `ubuntu-24.04` and `windows-2022` for every push **to
`master`** and every pull request. A push to any other branch runs nothing, so
the Windows leg — the only place MSVC is exercised — does not run on branch
work until a pull request opens. `release.yml`
turns a tag into a Linux AppImage and a Windows zip on the releases page.

**`ci.yml` has a THIRD job** — `Saved-game fuzz (ASan/UBSan)`, on the same
triggers, and it is not one of the two test binaries. `scripts/local-ci.sh`
skips it unless given `--with-sanitizers`, and prints that it skipped it. So a
plain local run is green against two of `ci.yml`'s three jobs, and the Windows
leg is uncovered on top of that: `scripts/wintest-ci.sh` is the only local
route to it, and it exits without running anything when the box is off.

Cutting a release is three edits, a check and a tag, **in this order**:

1. Bump `project(gameshub VERSION ...)` in `CMakeLists.txt`.
2. Bump `Current version X.Y.Z` in `README.md`.
3. Close `## [Unreleased]` in `CHANGELOG.md` into `## [X.Y.Z] - <date>`, and
   leave a fresh empty `[Unreleased]` above it.
4. Run `.claude/bump.json`'s `post_check` and see it print `version X.Y.Z
   consistent`. **Nothing else runs it** — not a hook, not `local-ci.sh`, not
   the workflow — so a step skipped above is caught here or nowhere.
5. Commit, then `git tag vX.Y.Z && git push --follow-tags`.

**`.claude/bump.json` is the one enumeration of version-bearing files** — it
names `CMakeLists.txt` and `README.md`, and its `post_check` is what catches
drift between them and the changelog. Read it rather than this list if the two
ever disagree; a file added there and not here is how this list goes stale.

The tag's `v` prefix is stripped and compared against `CMakeLists.txt`, and
the changelog must already have a `## [X.Y.Z]` block — the `verify` job
checks both before either build starts, because the release notes are read
from that block and a forgotten step would otherwise publish an empty one.
Get it wrong and the fix is to delete the tag; nothing is published.
**That job does NOT look at `README.md`**, which is what step 4 is for: skip
both and the release page still advertises the previous version, with nothing
having said so.

Every action is pinned to a commit SHA with the version in a trailing
comment. That is not decoration: these workflows publish binaries that
strangers download, and a moved tag on a third-party action would run
arbitrary code against them. `actionlint`, `yamllint` and `zizmor` all pass
clean and are the check before pushing a workflow edit.

## Architecture

**A game is a rules core plus a view, and the core never includes a widget.**
That split is what makes a game's rules testable without a display, and it is
the rule to hold when adding one.

**It now holds for all fourteen.** `GAME_CORE_SOURCES` in `CMakeLists.txt` is
the list of record, and `gameshub_selftest` links it — so a rules check for any
game can be written straight into the self-test. Six games held their rules
inside the widget until GHUB-0066 closed on 2026-08-25 (Klondike, Spider,
FreeCell, Pyramid, Snake and 2048); the split found two shipped bugs that
nothing could have caught while it did not hold, GHUB-0125 and GHUB-0126.

**Three of those cores hold a lifted run themselves rather than handing it to
the view**, and that is deliberate. Klondike and Spider turn over whatever a run
was covering when it lands, so the drop has to know where the cards came from;
keeping the run in the table means the view cannot tell it wrong. FreeCell hands
the run back because it uncovers nothing. **In all three the undo snapshot is
banked when the run is LIFTED, never when it is dropped** — the drop happens
after the cards have left their pile, so a snapshot taken there is of a table
they were never on. That is GHUB-0126, and it lost the card in two of the three.

- `src/gameview.h` — `GameView`, the contract between hub and game: a QWidget
  that offers `gameActions()` for the toolbar and emits `statusChanged`. It also
  declares `applyLegibility(bool)`, called when the hub's legibility switch
  moves; the base constructor is what makes that connection, so **every**
  constructed game hears about it and not just the one on screen. `gameview.cpp`
  holds that constructor, the caption helpers below, and nothing else — a
  Q_OBJECT class needs a matching source file in the build or AUTOMOC generates
  no metaobject for it.

  Four members are what a new game's legibility pass is built from, and using
  them is cheaper than inventing anything. `captionText()` defaults to the last
  string the game emitted through `statusChanged`, which the base remembers, so
  no game keeps a second copy of a sentence it has already composed; override it
  only where the surface needs something the status bar does not carry.
  `paintStatusCaption()` draws that sentence on a plate and does nothing at all
  while the switch is off, so a pass is three lines at the end of `paintEvent`.
  `captionBand()` is the strip to keep clear for it — subtract it from the
  height the game lays out in. It is a **fixed** two-line strip, capped, and
  deliberately not the height of the current sentence; under the cap a caption
  may overlap the board slightly, and that is the accepted outcome rather than
  a bug. The caption-band trap below owns both reasons. `smallestCardWidth()` is the
  narrowest card the game draws, **at the smallest scale it draws one at**, and
  a game with no cards returns 0. **A card game must override it.** The
  inherited 0 makes `cardsKeepTheirFaces` skip that game in silence, and its
  `checked >= 6` floor is already met by the six that do override it — so a
  fifteenth card game that forgets is caught by nothing, and ships drawing
  cards too small to show a face.

  **`applyLegibility` is never called at construction** — `gameview.h` says so
  — and games are built lazily, so a game opened for the first time while the
  switch is already on gets no callback at all. Every game here is safe from
  that because the switch is read **live**, at the point it is used — in the
  game's own `paintEvent`, font accessor or `minimumSizeHint`, or on its behalf
  by the inherited `paintStatusCaption()` and `captionBand()`, which read it
  live inside `gameview.cpp`. **Seven games never name `Legibility` at all**
  and are correct as they stand; inheriting the read is compliance, not a gap
  to paper over with a redundant call.
  **That is the rule, not an accident.** A game that instead caches anything
  derived from the switch must read it in its own constructor as well, and no
  test catches the omission — `everyGameAnswersTheSwitch` builds every game
  with the switch off, so a game that ignores its initial state looks perfect.

  `deactivate()` is not optional for a game with a clock or an animation, and
  `tests/uitest.cpp` asserts it over all fourteen: no game may still be moving
  once the hub has left it. Pinball had no override at all and its ball kept
  rolling — and draining — on a table nobody was looking at (GHUB-0073).
  **What that assertion can catch is a game that is actually in motion when the
  hub leaves**, and that is a weaker net than it sounds: every clock here except
  Pinball's is idle on a freshly opened board, so deleting its `stop()` reddens
  nothing. Measured, by deleting Sudoku's and watching the suite stay green.
  **That is how Snake and Hearts kept running through every green run** — both
  owned a timer and neither overrode `deactivate()`, so leaving Snake mid-game
  drove the snake into a wall and leaving Hearts finished the hand without you.
  Both were fixed. **The rule to apply is structural, not observed: a game that
  owns a `QTimer` overrides `deactivate()`**, whether or not a test can see it
  running.

  **`deactivate()` freezes a game; it does not settle it, and the two are
  different.** A board frozen mid-deal is static and will pass any stillness
  probe while still holding state the next settings change consumes — Canasta's
  cards in flight carry a destination captured when they left, so
  `applyLegibility` lands them, correctly and irreversibly.
  `hasPendingAnimation()` is how a game says so, and Canasta is the only one
  that answers true. **Asking pixels instead does not work**: a staggered deal
  has lulls where every remaining card is counting down its delay, so two
  matching renders mean nothing. That mistake passed here every time and
  reddened both CI legs.
- `src/legibility.*` — `Legibility`, the app-wide legibility preference
  (`display/legibility`, default off). It is no longer the only thing this app
  stores outside a game's own group: `donate/ask` and `donate/launches` are
  app-wide too, and `window/geometry/<page>` and `saved/<game>` are written
  per page and per game by `hubwindow.cpp`'s `geometryKey()` and `saveKey()`.
  **Anything sweeping stored state — a settings reset, a migration — has all
  four families to handle, PLUS every per-game group**: `scores.cpp` writes
  `<game>/best_*` keys and Canasta keeps its House set under `canasta/house/`
  and its target under `canasta/target`, so a reset that clears only the four
  app-wide families leaves a rule set and a score table standing, and getting it wrong is quiet: clearing the donate
  switch but leaving the counter at 149 fires the prompt on the very next
  start, and leaving `saved/` behind resumes games a "reset" was meant to
  forget.
  A singleton like `Sound`, but stored and
  broadcasting: games are built lazily and live for the session, so one built
  before the switch moved would never learn without the signal.
  `docs/specs/GHUB-0017-legibility-switch.md` is the contract. **All fourteen
  per-game passes have shipped** — Canasta (GHUB-0038) and Sudoku (GHUB-0039)
  first, both described under Traps below, and the other twelve as GHUB-0071.
  **A pass does not have to be a caption, and three of the fourteen are not.**
  Four shapes, covering all fourteen. **Ten reserve a band and draw a caption.**
  **Hearts draws a caption but reserves nothing** — it puts the sentence in the
  gap that already exists between the trick and your hand, because its hand is
  anchored to the bottom and a band would take space off the cards instead.
  **Pinball grows what it already says** rather than adding a caption: the
  backglass and both labels on it, together. And **Canasta and Sudoku predate
  the caption entirely** — Canasta's pass is a raised minimum size and Sudoku's
  is bigger pencil marks, both under Traps below. What is required is that the
  game answer the switch, not that it answer in one particular shape.

  **Canasta and Sudoku reserve no band, and Canasta must never be given one**:
  its melds clear `kFaceMinWidth` by 0.4 px, and a band comes off the height
  its table solves card width from.

  What holds that true is not the count: `tests/uitest.cpp` walks the games the
  hub can open, renders each at its own smallest size with the switch off and
  on, and asserts the picture changed and then went back. **A fifteenth game
  added without a pass reddens that block**, which is the point of it — it
  lands in `silent` if it holds still, and fails the `deactivate()` assertion
  if it does not. **Sudoku is excluded by name**, because its pass grows pencil
  marks and a freshly generated board has none, so all three renders match at
  any window size; its own block puts a mark in every cell that will take one
  before asserting.
- `src/donate.*` and `src/donatedialog.*` — the one place the app asks for
  money, reached from Help → Support this project and from the every-150th-
  launch prompt. **A `--game` launch advances the count and shows nothing** —
  a startup prompt is an interruption, but one landing on your turn is a
  different thing — so a launch that skips the prompt is deliberate rather
  than an off-by-one. `donate::launchOwesPrompt` is pure so the off-by-one everyone
  remembers is checkable without touching stored settings; the counter is per
  process, written as it is read, so a killed process still counted. The links
  themselves are generated, never written — see the trap below.
- `src/hubwindow.*` — the tile grid and one page per game in a `QStackedWidget`.
  Games are constructed lazily on first open. Each tile paints its own
  miniature; `openGameNamed()` backs the `--game` flag. It also owns two things
  every game inherits: window size and position, kept **per page** so each game
  reopens the size it was left, and saved games — a game that overrides
  `GameView::saveState()`/`restoreState()` is stored on close and restored the
  next time it is opened, with no save dialog anywhere. An empty state means
  "nothing worth keeping" and clears the stored one, which is how a finished
  game avoids resuming onto its own final scores.
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

**The caption band is a FIXED height — two lines of the caption font plus its
plate, `fm.height() * 3.1` — and it is capped, and both are load bearing.** A
band sized to the current sentence would resize the board every time the
sentence changed length, and a board that jumps between moves is
worse than a slightly smaller one. The cap — 22% of the surface's height — is the
Windows lesson again in a new place: the band comes off the height a card game
solves its card width from, and `fm.height()` is a property of the platform's
font, not of this code. `windows-2022` under the offscreen platform has no font
environment at all, so an uncapped band would be far wider there than here and
could drive a card below `CardArt::kFaceMinWidth` on a runner and nowhere else.
A capped band can be narrower than the sentence needs, and then the caption
just overlaps a little; a faceless card cannot be recovered from.

**Card art is cached, and the cache key must decide the picture completely.**
`CardArt::paintFace`/`paintBack` snap a card to whole device pixels, draw it
once and keep it. Three things that cost real time to learn. **The key is
computed from the SNAPPED size and the card must be drawn at that same snapped
size** — key on the rounded size while drawing at the exact one and two rects a
fraction of a pixel apart share an entry, so whichever drew first decides the
picture and a frame after an eviction differs from the same frame before one.
`cardArtKeyDecidesThePicture` is the guard. **The shadow padding must be a whole
pixel**, or the card sits at a fractional offset inside the pixmap and every line
antialiases differently. And **a rotated FACE is never cached** — resampling
softens it, and this game is read by pip pattern. Rotated backs are cached
because there is nothing on a back to read.

**`--shot` cannot compare two card games.** The deal is random, so two runs of
one build differ in about 101,000 of 740,000 pixels. That is GHUB-0093, and it
produced two confident wrong readings before it was spotted. To compare a
drawing change, build a scratch probe that draws the shape at a fixed rect
against both trees — or use a deterministic surface like the tile grid, which
matches itself exactly.

**A widget that resizes its own window after lowering its minimum must let the
layout catch up first.** `setMinimumSize()` lowers *that widget's* floor, but
the hub's minimum is computed from its central widget **through a
`QStackedWidget`**, and that chain is recalculated lazily — so a `resize()`
issued in the same breath is clamped straight back up by the stale figure.
Canasta's legibility switch was one-way inside the hub for this reason
(GHUB-0072), and **its own reversibility check passed the whole time**: that
check uses a bare `CanastaView`, where `window()` is the view itself and the
stale chain does not exist. A widget test can be green about a bug that only
exists in the real window. Walk up to the window activating each layout before
resizing.

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
reach — at its smallest window, **with the legibility switch on** — is Klondike
67.9, FreeCell 59.4, Spider 54.2, Hearts 52.5, Pyramid 52.1 and Canasta's melds
46.4, all clear of `kFaceMinWidth`. **The band is subtracted only by the four
that reserve one**, so Hearts' and Canasta's figures carry no band cost at all
and would not survive one being added. **Pyramid and FreeCell are the two the
band actually costs** — 68.6 and 67.4 before it — and they clear by 13% and 29%.
Klondike and Spider reserve a band and are still decided by their width at their
smallest windows, so it costs them nothing there. FreeCell joined that list on
2026-08-21 (GHUB-0083): its height budget had been assuming a column of about
two cards of fan against a deal of seven, so before the fix it was never
height-bound and the deal ran under the plate instead. `cardsKeepTheirFaces` in
`tests/uitest.cpp` prints all six every run, so these are readings rather than
history. Their `std::max(30.0, …)` … `std::max(34.0,
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
**A font ratio tuned on one machine is not portable, and this one cost two red
Windows CI legs to learn.** A mark tuned to 0.29 of the cell passed here and
failed on `windows-2022`, and no Linux run could have caught it:
`scripts/local-ci.sh` executes `ci.yml`'s own steps, but nothing on Linux
drives the Windows leg.

**Do not reason about it from typographic ratios — that was the second
mistake.** Measured at em 100 the faces are close (this font 0.742 of an em
bold; Segoe UI 0.728, Arial 0.731, Tahoma 0.760 unbold on the owner's Windows
box via GDI+), while the *same* font here measures about 0.685 at the 7-to-11
point sizes a mark is actually drawn at. That gap is hinting rounding the ink
by a whole pixel, not the typeface — so a figure taken at one size says very
little about the other, and comparing a large-size measurement on one platform
against a small-size one on another says nothing at all.

**`windows-2022` under `QT_QPA_PLATFORM=offscreen` has no font environment at
all, and this is the number to remember: `QFontDatabase::families()` returns
EMPTY and the default face measures digits at 0.997 of an em** — the full em
box, which is a headless stub rather than any real typeface. Anything derived
from font metrics therefore degrades to its floor on that runner and must be
allowed to. It is not evidence about what a Windows *player* sees: a real
desktop has Segoe UI and behaves like this machine.

**The rule the three red runs taught: a test may ASSERT what the code does, and
must only REPORT what the platform happens to provide.** Each rewrite here put
a fresh environment constant into an assertion — a tuned ratio, then a growth
multiple, then a minimum font count — and each passed locally and failed on
`windows-2022`. If a number describes the machine rather than the change, print
it and assert something else.

So `SudokuView::markFont()` **solves** rather than scales: one metric probe
fixes the font's ink-per-point, then it steps down until the ink measured at
the size it will really be drawn at fits. `marksFitAt(pointSize)` exists so a
test can ask about a size the view did *not* pick — with the size solved,
"it fits" is true by construction and only "it is the largest that fits" has
teeth. **The test that guards this loops over the machine's own font families**
(locally 0.49 to 0.99 of an em, bracketing anything a desktop would pick), and
putting the tuned constant back reddens it here rather than three minutes into
CI. Reach for that shape whenever a constant is really a property of the
platform's font, not of this codebase.

**Under `noMeldingFirstRound`, `discardRisk` is zero for every rank, so any AI
rule written in terms of it is dead code.** The rule bars melding for one round
and therefore bars taking the pile — `validateTake` refuses outright — which is
what makes a first-round throw safe. But it also means no team has melded yet,
and `discardRisk` returns 0 the moment `theirs.meldOfRank(rank)` is null. So
"the throw is free, dump your most dangerous card" cannot be built on
`discardRisk`: there is no dangerous card to find. What the AI does instead is
drop the whole `safety` accumulator in `chooseDiscard` — every judgement about
handing the pile over, gathered into one variable precisely so it can be
dropped in one place — leaving the hand-value terms to answer honestly. The
only safety term with real force in that window is Expert's `+50 ×
countRank(pile, rank)`, which is why the test that locks this uses Expert and a
king matching the up-card.

**`Engine::meldingAllowed()` and `Engine::discardCannotBeTaken()` answer about
DIFFERENT seats, and the fourth seat of the first round is where they part.**
The first is about the seat playing now; the second about the seat that plays
next, hence its `+1`. The last seat of the round is still barred from melding
while the turn after it — the first seat playing a second time — is not, so it
is the one seat in the round whose discard is live. An AI or a status line that
reads `meldingAllowed()` for "is my throw safe?" gets it right three times and
wrong on the fourth, which is the hardest quarter of a bug to notice.

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
back, nothing missing and nothing doubled. Spider, Pyramid and Hearts do remove
cards (a harvested run, a matched pair, a collected trick), so they get
`fitsPack` plus a count of their own — Hearts' is that hands plus the cards on
the table equal fifty-two less four per trick collected, without which a blob
restores a full hand into the endgame. Without that check a corrupt blob
restores into a deal that cannot be won, and the player finds out an hour later.
Minesweeper, Reversi, Draughts, Sudoku and 2048 have no pack at all, so what
stands in is the check their `restore()`/`load()` makes, refusing a board the
game could not have reached (in a core for the first four, and in the view for
2048, which has none): the mine count must match the level and
the numbers are recomputed rather than read, a Reversi board must hold at least
the opening four discs, a draughts piece may not stand on a light square, a
Sudoku solution must be a completed grid that every clue agrees with, and
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
at its destination and the eye sees it twice. **The match is CONSUMED one per
flight** — `CanastaView::m_consumed` marks a flight the moment it answers, so a
second identical card in the air finds the next unmarked flight rather than the
same one. Without that, two identical cards arriving together suppress both
destination copies and one card disappears; Canasta shuffles two packs and
`Card::deck` sits outside `operator==`, so identical cards in flight together
are routine here rather than exotic.

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

**A `QStackedWidget` takes the largest minimum size of every page it has
built, so one page can decide how small every other one can be made.** The tile
grid was doing exactly that: fourteen 190-pixel tiles are five rows deep, which
put a floor of about 1170 pixels on Chess as well as on the grid — taller than a
1080p screen, and the opposite of the README's opening promise. The grid now
lives in a `QScrollArea` and asks for nothing. `HubWindow::kFitsBesideYourWork`
(960x1000) is that promise written as a number — half a 1920x1080 desktop
across, its height less a panel and a title bar — and **any** game whose minimum size
exceeds it fails the check in `tests/uitest.cpp` — in either legibility state,
so a per-game legibility pass that raises a minimum is bound by it too.
Canasta already sits at 900 wide against a 960 bar, which is 60 pixels of
headroom rather than a comfortable margin. **That check gives each
game its own hub**, because measured through one window every game reports the
worst one's floor and thirteen innocent games go red together.

**The three donate URLs are generated from `.github/FUNDING.yml` at configure
time and must never be typed into a C++ file.** That YAML is already GitHub's
sponsor button, so it is the copy that stays maintained; a list in a .cpp is a
second copy that drifts the day one link changes, with nothing to catch it.
`CMakeLists.txt` writes `funding.h` from it and **stops the build on a funding
key it has no rule for** — a dropped link otherwise looks exactly like a working
build. A uitest check counts the keys back out of the YAML as a second guard.

**A new platform's rule goes in that mapping loop, not under `custom:`.** The
handle-to-URL stems (`https://github.com/sponsors/`, `https://www.patreon.com/`)
do live in `CMakeLists.txt`, because FUNDING.yml stores account names rather
than addresses for the platforms GitHub knows — so the ban above is on C++, and
the loop is where a rule for `ko_fi:` belongs. Routing it through `custom:`
instead does not even build: the loop reads a one-URL `custom:` list and refuses
a two-entry one, though GitHub's `custom:` key takes a list. Widening that
regex is the fix if a second custom link is ever wanted.

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
source with `CMAKE_AUTORCC ON`. **`tests/uitest.cpp` asserting every effect is
present and non-trivial in size is the check.** `assets/sounds/` is 17 files
and about 380 KB, and the compiled resource object matches it — but the
binary's own total grows with every game added, so it says nothing on its own.
It was ~800 KB when this trap was written and is ~1.6 MB now; a build that had
silently dropped the audio would sit well above the old figure too.

**QPainter's `drawRect` fills with the current brush as well as outlining it.**
A brush set for one thing leaks into everything drawn after it: the flag's red
brush turned every dug Minesweeper square red, and only from the first flag
onward, because painting runs row by row. Set the brush — or `Qt::NoBrush` —
immediately before a shape call rather than trusting what came before.

**Sounds are generated, never sampled.** `tools/make_sounds.py` synthesises all
17 effects from a fixed seed, so re-running it is byte-identical. That is a
licensing decision rather than a stylistic one — see `ROADMAP.md` § Standing
rules before adding any asset.

## Review history

This file has been through `review-contract` as a standard. The loop log is
kept in `docs/claude-md-review-2026-08-20.md` rather than here, because a table
appended to forever is a cost every session pays and almost none reads.

## Testing notes

Wayland blocks synthetic clicks and no injection tool is installed, so GUI
testing is: construct widgets offscreen, `render()` them into a QPixmap to
force `paintEvent` through, and click the hub's tiles via `QPushButton::click`.
Anything needing real pointer input has to be verified by eye instead.

The self-test is where game logic gets proven — it plays 200 random Reversi
games, 20 full AI Hearts games, 18 full AI Canasta games at three strengths,
and flies pinballs. Prefer adding a check there over a UI test.

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
2026-08-25, after the Canasta AI pass and GHUB-0124: medium v easy 22/24 +2538,
hard v easy 23/24 +3844, hard v medium 66/120 +233, expert v hard 121/240 +14.
**The first three rungs are the useful reading when a change is gated to one
level** — GHUB-0124 touched Expert alone, and those three not moving at all is
what says so. Two things
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
