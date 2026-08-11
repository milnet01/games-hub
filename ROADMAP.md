# Roadmap

## Agreed queue

Games the owner has asked for, in the order agreed on 2026-08-10. All are
traditional or public-domain; see *Choosing games* below for why each is safe.

| Game | Notes | Size |
|------|-------|------|
| ~~**Chess**~~ | **Shipped 2026-08-10.** Full rules, an opponent at three strengths, and a move generator proven against the published node counts for four reference positions. | Large. Bigger than any single game currently in the hub. |
| ~~**Canasta**~~ | **Shipped 2026-08-10.** Partnerships (2v2), melds, wild cards, red threes, freezing and taking the discard pile, going out and the full scoring table — plus a second, editable rule set for the owner's family's house rules. | Large. Most complex card game on the list. |
| **Gin Rummy** | Two-handed against the computer: knocking, deadwood, gin, undercut. | Medium. |
| **Cribbage** | Two-handed with the pegging board, the crib, and the show. | Medium. |
| **Blackjack** | Traditional twenty-one against a dealer. | Small. |
| **Spades** | Four-handed partnership trick-taking with bidding. Reuses the Hearts shape. | Medium. |
| **TriPeaks / Golf / Yukon** | Three more solitaires. Cheapest of all — they reuse the card engine and drag-and-drop wholesale. | Small each. |

Chess and Canasta are done; nothing below them is started. The hub currently
ships fourteen games.

## Agreed, not yet scheduled

Asked for by the owner on 2026-08-10, to be built after the queue above unless
he says otherwise. None of these are started.

### How to play, inside the app

**Every game explains its own rules.** Thirteen games ship with no instructions
anywhere — a player who has never met Reversi or Canasta has to leave the
program to learn it. Add a **Rules** action to the toolbar that opens the rules
for whichever game is on screen.

The shape that fits what is already here: give `GameView` a virtual returning
the game's rules as rich text, so a game carries its own explanation in its own
directory and adding a game means writing its rules next to its code. The hub
shows them in one shared dialog, so all thirteen look the same. Also worth a
short line of the *controls* — which mouse button, what a click on the stock
does — because that is what a player actually gets stuck on.

**Standing rule 2 below binds hard here, harder than anywhere else in this
file.** Rules text is exactly the thing a rulebook author owns. Every word has
to be written fresh from an understanding of the game. Do not paste from
Wikipedia, from Bicycle's site, or from a printed rulebook, and do not
paraphrase one closely enough that the sentence order survives.

Size: medium, and most of it is writing rather than code.

### More card games

The queue above already names Canasta, Gin Rummy, Cribbage, Blackjack, Spades
and three more solitaires. Beyond those, the traditional card catalogue is
enormous and entirely free. Worth picking from, roughly cheapest first:

- **War**, **Go Fish**, **Old Maid**, **Beggar-my-neighbour** — trivial rules,
  and the first genuinely child-friendly games in the hub.
- **Crazy Eights** — the public-domain game Uno was built from. Free under its
  own name; do not use Uno's name, colours or card faces.
- **Sevens** (also called Fan Tan or Domino) — one of the simplest card games
  that is still worth playing.
- **Whist** and **Euchre** — both already listed as safe below, and both reuse
  the Hearts trick-taking shape almost wholesale.
- **Rummy 500**, **Cassino**, **Pinochle**, **Bezique**, **Scopa**,
  **Briscola** — bigger, and each needs its own scoring.

Every one of these reuses `src/cards/` for the deck and the drawing. Nothing
here needs a new asset.

### Board games

Same test, same answer: the classics are ancient and free.

- **Nine Men's Morris**, **Fox and Geese**, **Alquerque** — small, and all
  three are close in shape to the Draughts board already built.
- **Gomoku** (five in a row) and **Four in a Row** — the second is Connect
  Four's game under a generic name, which is the rename trick described below.
  Both are small.
- **Backgammon** — needs dice, a doubling cube and a real opponent. Medium to
  large, and the most likely to be actually played.
- **Mancala** and **Dominoes** — medium, and neither looks like anything else
  in the hub, which is worth something on the tile grid.
- **Snakes and Ladders** (the ancient *Moksha Patam*) and **Pachisi** — pure
  race games. Small, and the other family that suits children.
- **Go** — the rules are tiny and the opponent is a research project. Ship it
  only if a weak opponent is acceptable, and say so on the tile.
- **Halma**, **Hnefatafl**, **Shogi**, **Xiangqi** — deeper cuts, all free.

## Deferred, deliberately

**Cutting the pack, in Canasta.** At a table the cut stops the dealer stacking
the deck and breaks up cards left clumped from the last hand. Neither can
happen here — every deal is a fresh random ordering — so a cut would be an
animation that changes nothing, and putting one on screen implies it matters.
What *does* rotate is the deal: each hand a different seat deals and the player
to their left leads, so the advantage moves round the table as it should. If a
cut is ever wanted it belongs in the house-rules dialog as honest decoration,
labelled as such. Decided with the owner on 2026-08-10.

## Agreed, in progress

**Saving a game in progress — the mechanism is built, most games still need
their half.** Asked for on 2026-08-11, which retired the entry below that had
deferred it. `GameView::saveState()` / `restoreState()` are the contract; the
hub stores whatever a game hands back and gives it straight back the next time
that game is opened, so there is no save dialog and nothing to remember to
press. **Canasta is done** — the whole engine writes through `QDataStream`
behind a version number that refuses an older or truncated save rather than
misreading it, and the self-test proves a resumed table is the same table and
plays on.

Still to do, in the order they are worth doing:

- **Chess** — position, history for threefold repetition, level, whose move.
- **Hearts** — hands, tricks taken, passing direction, running scores.
- **Klondike, Spider, FreeCell, Pyramid** — piles and stock. A shared card
  codec would do all four at once and is worth writing first.
- **Sudoku** — grid, pencil marks, elapsed time. Pause covers the short case.
- **Minesweeper, Reversi, Draughts, 2048** — small states, quick wins.
- **Snake, Pinball** — real-time and session-shaped; probably not worth it.

**Choosing your colour, in Chess or Draughts.** Both put the human on the side
that moves first — White and Red — and neither offers a swap or a board flip.
That is scope, not oversight: it keeps `advance()` a single path with one
`m_human`, which is the shape every engine game in the hub shares. Worth adding
one day, but add it to *both* games at once and to that shared shape, rather
than special-casing Chess. Decided while building Chess on 2026-08-10.

## Choosing games

**Game rules cannot be copyrighted; names and specific artwork can.** That is
the whole test, and it is why every game here is either centuries old or has a
generic name.

**Avoid** — actively enforced trademarks: Tetris (the most aggressively
defended, including the look), Scrabble, Monopoly, Connect Four, Uno, Yahtzee,
Battleship, Pac-Man, Space Invaders, Bejeweled.

**A rename often fixes it**, because only the name is owned: Battleship's
pencil-and-paper ancestor is fine as *Sea Battle*; Yahtzee derives from the
public-domain dice game *Yacht*; Breakout and Pong are Atari names but the
genres are free.

**Safe** — public domain: Chess, Draughts, Backgammon, Go, Gomoku, Nine Men's
Morris, Mancala, Dominoes, Cribbage, Canasta, Gin Rummy, Whist, Euchre,
Blackjack, Spades, Sudoku, Hangman, Mahjong solitaire, and every solitaire
variant.

**Two standing rules for anything added:**

1. **No third-party assets.** Art is drawn with QPainter, sounds are
   synthesised by `tools/make_sounds.py`. This is what keeps the whole
   repository original under one MIT licence — do not introduce a downloaded
   image, font or sound sample without changing the licensing story first.
2. **Do not copy rule text verbatim** from a published rulebook. The rules are
   free; a particular author's wording is not.
