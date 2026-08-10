# Roadmap

## Agreed queue

Games the owner has asked for, in the order agreed on 2026-08-10. All are
traditional or public-domain; see *Choosing games* below for why each is safe.

| Game | Notes | Size |
|------|-------|------|
| ~~**Chess**~~ | **Shipped 2026-08-10.** Full rules, an opponent at three strengths, and a move generator proven against the published node counts for four reference positions. | Large. Bigger than any single game currently in the hub. |
| **Canasta** | Partnerships (2v2), melds, wild cards, red threes, freezing and taking the discard pile, going out, full scoring table. | Large. Most complex card game on the list. |
| **Gin Rummy** | Two-handed against the computer: knocking, deadwood, gin, undercut. | Medium. |
| **Cribbage** | Two-handed with the pegging board, the crib, and the show. | Medium. |
| **Blackjack** | Traditional twenty-one against a dealer. | Small. |
| **Spades** | Four-handed partnership trick-taking with bidding. Reuses the Hearts shape. | Medium. |
| **TriPeaks / Golf / Yukon** | Three more solitaires. Cheapest of all — they reuse the card engine and drag-and-drop wholesale. | Small each. |

Chess is done; nothing below it is started. The hub currently ships thirteen
games.

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
