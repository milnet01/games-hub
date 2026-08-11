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

cd build && ctest --output-on-failure   # both test binaries
cmake --install build                   # refresh the installed copy
```

Run a test binary directly for its per-check output — `ctest` only reports
pass/fail:

```bash
./build/gameshub_selftest                             # all game rules
QT_QPA_PLATFORM=offscreen ./build/gameshub_uitest     # widgets and hub
```

`CMAKE_INSTALL_PREFIX` must be set at **configure** time, not passed to
`cmake --install --prefix`. The `.desktop` file bakes in an absolute `Exec`
path via `configure_file`, so a late `--prefix` installs the binary correctly
while leaving `Exec=/usr/local/bin/gameshub` pointing at nothing.

The panel launcher runs the **installed** copy, so re-run `cmake --install`
after changing code or the pinned icon keeps launching the old build.

## Architecture

**Every game is a rules core plus a view, and the core never includes a
widget.** That split is the reason the whole collection is testable without a
display, and it is the rule to preserve when adding a game.

- `src/gameview.h` — `GameView`, the contract between hub and game: a QWidget
  that offers `gameActions()` for the toolbar and emits `statusChanged`. It has
  a near-empty `gameview.cpp` on purpose: AUTOMOC only generates a Q_OBJECT
  class's metaobject if its header has a matching source file in the build.
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
  the same card whichever pack they came from.
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

**`CardArt::paintFace` stops drawing the face below 46 pixels wide** and leaves
only the corner index, because pips are unreadable smaller than that. Anything
drawing cards at reduced scale — Canasta's melds are 0.62 — gets a stack of
slivers rather than cards, and has to name them some other way. The melds carry
a "K ×5" badge for exactly this reason. Check the width before assuming a face.

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
