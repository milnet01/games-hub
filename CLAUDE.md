# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A Qt 6 Widgets game collection — a hub window holding Reversi, Minesweeper,
Klondike, Spider, Hearts and Pinball. Started 2026-08-10 as a single Reversi
game and expanded the same day; there is no git repo, ROADMAP or CHANGELOG yet.

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
  miniature; `openGameNamed()` backs the `--game` flag.
- `CMakeLists.txt` splits `GAME_CORE_SOURCES` (Qt-free or QtCore-only) from
  `GAME_VIEW_SOURCES`. `gameshub_selftest` links only the cores, so anything
  that pulls in QtWidgets belongs in the view half.

### Per game

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
- **Cards** — `cards/card.*` (deck building; `makeDeck(decks, suitsUsed)` is
  what gives Spider its difficulty) and `cards/cardart.*` (shared drawing, so
  the three card games look like one deck).
- **Klondike / Spider** — `klondike/`, `spider/`. Both keep piles as
  `std::vector<Card>` and drag by lifting a run off its pile into `m_drag`,
  restoring it on a failed drop. Card width is solved from the row cost
  (`7w + 6·gap` for Klondike) — assuming a fixed pixel gap pushed the last
  column off screen.
- **Hearts** — `hearts/heartsengine.*` holds the whole rule set: passing
  rotation, the forced two-of-clubs lead, following suit, no points on the
  first trick, hearts breaking, and the moon shot (26 to everyone *else*).
  `HeartsView` is presentation and timers only.
- **Pinball** — `pinball/pinballtable.*` is the simulation in fixed table units
  (400×720), scaled to the widget so resizing never changes the physics.
  Everything collides as a circle against a fat line segment. `PinballView`
  only draws it and feeds input.

## Traps worth knowing

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

**`pkill -f <pattern>` will kill this session's own shell** when the pattern
appears in the command line being run. Use `pkill -x gameshub`.

## Testing notes

Wayland blocks synthetic clicks and no injection tool is installed, so GUI
testing is: construct widgets offscreen, `render()` them into a QPixmap to
force `paintEvent` through, and click the hub's tiles via `QPushButton::click`.
Anything needing real pointer input has to be verified by eye instead.

The self-test is where game logic gets proven — it plays 200 random Reversi
games, 20 full AI Hearts games, and flies pinballs. Prefer adding a check there
over a UI test.
