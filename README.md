# Games

A small collection of desktop games for Linux, in one window sized to sit
beside whatever you are actually working on.

![the games hub](docs/hub.png)

## The games

| Game | What it is |
|------|------------|
| **Reversi** | The classic disc-flipping board game, against the computer, with three difficulty levels. |
| **Minesweeper** | Beginner, Intermediate and Expert fields. |
| **Solitaire** | Klondike, draw one or draw three. |
| **Spider** | Spider solitaire in one, two or four suits. |
| **Hearts** | Four-handed Hearts against three computer players, to 100. |
| **Pinball** | One table, three balls, two flippers. |

Pick a game from the grid; **← All Games** (or `Esc`) goes back. Each game puts
its own buttons — New Game, Undo, difficulty — on the toolbar beside it.

## Playing

**Reversi.** You are Black and move first. Click a square with a faint dot on
it. Your move must trap a line of the computer's discs between the square you
click and one of your own, and every disc trapped flips to your colour. If you
have no legal move your turn is skipped automatically — that is the rule, not a
bug. Undo rewinds your move *and* the computer's reply.

**Minesweeper.** Left click digs, right click plants a flag. A number says how
many mines touch that square. Clicking a number you have fully flagged clears
its remaining neighbours in one go. The first click is always safe.

**Solitaire (Klondike).** Drag cards to build the four foundations up from Ace
to King in one suit. On the table, build downwards in alternating colours; only
a King may start an empty column. Double-click a card to send it to a
foundation. Click the face-down pile to deal.

**Spider.** Build downwards in each column regardless of suit, but only a run
of *one* suit moves as a unit — the yellow outline shows how much will lift.
Complete King-down-to-Ace in one suit and it is removed. Clear all eight runs
to win. Click the corner stack to deal another row; every column must be
non-empty first. Start with 1 Suit.

**Hearts.** Avoid taking hearts (1 point each) and the queen of spades (13).
Lowest score when someone reaches 100 wins. Choose three cards to pass, then
click **Pass 3 Cards**. Follow the led suit if you can — cards you may not play
are dimmed. Take *every* heart and the queen and you shoot the moon: 26 points
to everyone else instead of you.

**Pinball.** Hold `Space` to charge the plunger, release to launch. `Z` and `M`
(or the arrow keys) work the flippers; clicking the left or right half of the
table does the same. Three balls. A launch that falls short rolls back to the
plunger rather than costing you a ball.

## Building

Needs Qt 6, CMake and a C++20 compiler.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
./build/gameshub
```

`--game <name>` opens one game directly, skipping the menu:

```bash
./build/gameshub --game pinball
```

## Installing

Adds Games to the application menu with its own icon:

```bash
cmake --install build
```

Set `CMAKE_INSTALL_PREFIX` when configuring, as above — the `.desktop` file
records an absolute path to the binary, so passing `--prefix` only at install
time writes a launcher pointing at the wrong place.

## Tests

```bash
cd build && ctest --output-on-failure
```

`gameshub_selftest` checks every game's rules with no UI: Reversi's engine
against random play, Minesweeper's first-click safety, deck integrity, twenty
complete AI-versus-AI games of Hearts, and a pinball ball flown round the table
at every plunger strength. `gameshub_uitest` builds each game widget offscreen,
paints it, and opens all six through the hub. Neither needs a display.

## Licence

MIT — see [LICENSE](LICENSE). All six games are traditional ones whose rules
are in the public domain, and the code here is written from scratch.

The app links Qt 6 dynamically, which Qt offers under LGPL-3.0 (or LGPL-2.1
with the Qt Company exception). Qt itself is neither bundled nor modified here.
