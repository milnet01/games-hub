<!-- ants-roadmap-format: 1 -->
# Games Hub — Roadmap

A collection of desktop games in one window. Pre-1.0 (0.2.0), so the blocks
below are phases rather than releases. Shipped items stay in the file and flip
to ✅; `CHANGELOG.md` is the separate user-facing record.

Execution order is positional: work a phase top to bottom. IDs are identity
only, so they are not in order and are never renumbered.

## P01 — Shipped

### 🎨 Games

- ✅ [GHUB-0001] **Chess, with a full rule set and an opponent at three
  strengths.** Castling, en passant, promotion and every draw rule. The move
  generator is proved by perft against the published node counts for four
  reference positions rather than by playing it.
  Layman: A proper game of chess against the computer.
  Kind: feature.
  Source: user-request-2026-08-10.

- ✅ [GHUB-0002] **Canasta for four, in partnerships, with an editable second
  rule set.** Melds, wild cards, red threes, freezing and taking the discard
  pile, going out and the full scoring table. Every number the game plays by
  lives in one `canasta::Rules` struct, which is what let the owner's six
  family rules land as values rather than as branches.
  Layman: Canasta against three computer players, including your family's own
  rules.
  Kind: feature.
  Source: user-request-2026-08-10.

- ✅ [GHUB-0003] **The other twelve games of the collection.** Reversi,
  Draughts, Minesweeper, Klondike, Spider, FreeCell, Pyramid, Sudoku, Hearts,
  Snake, 2048 and Pinball, all behind one hub window and one toolbar.
  Layman: Twelve more games, all in the same window.
  Kind: feature.
  Source: user-request-2026-08-10.

### 🎨 Saving a game in progress

- ✅ [GHUB-0004] **A save mechanism every game can opt into.**
  `GameView::saveState()` / `restoreState()` are the contract; the hub stores
  whatever a game hands back and gives it straight back the next time that
  game is opened. No save dialog and nothing to remember to press; an empty
  state means "nothing worth keeping" and clears any stale save, which is how
  a finished game avoids resuming onto its own result.
  Layman: Games can remember where you left off, with nothing to press.
  Kind: implement.
  Source: user-request-2026-08-11.

- ✅ [GHUB-0005] **Canasta saves and resumes.** The whole engine writes through
  `QDataStream` behind a version number that refuses an older or truncated
  save rather than misreading it. Rules added later append to a counted tail,
  so a save from an earlier build still loads and comes back without the rules
  it predates.
  Layman: Close Canasta mid-game and the whole table comes back.
  Kind: implement.
  Source: user-request-2026-08-11.

- ✅ [GHUB-0006] **Chess saves and resumes, and sets the pattern for the rest.**
  It keeps the *moves*, not the position: a FEN says where the pieces are and
  nothing else, while threefold repetition needs every position the game has
  passed through and Undo needs the boards behind them. `ChessGame` rebuilds
  all three from `play()`, so replaying the move list restores them from one
  list with no second copy free to drift. Each move is matched against
  `legalMoves()` on the way back in, which proves the save is a game this build
  would play and lets the generator — not the file — set the castling and
  en-passant flags.
  Layman: Close Chess mid-game and come back to the same board, with Undo still
  working.
  Kind: implement.
  Source: in-session-2026-08-11.

## P02 — Queued

Worked top to bottom. Nothing here is started.

### 🎨 Saving the remaining games

The shape is GHUB-0006's: save what the game was *told*, replay it, and match
each step against the rules on the way back in.

- 📋 [GHUB-0007] **Hearts saves and resumes.** Hands, tricks taken, passing
  direction and the running scores. The longest game in the hub after Canasta.
  Layman: Close Hearts mid-game and come back to it.
  Kind: implement.
  Lanes: hearts.

- ✅ [GHUB-0008] **The four solitaires save and resume.** Klondike, Spider,
  FreeCell and Pyramid: piles and stock. A shared card codec does all four at
  once and is worth writing first — these are also the games most often left
  half-finished.
  Layman: Close a game of patience and come back to it.
  Kind: implement.
  Lanes: klondike, spider, freecell, pyramid, cards.
  Resolved (2026-08-11): src/cards/cardcodec.* holds the shared codec —
  piles to and from a QDataStream, plus fitsPack/matchesPack, which is
  what a table-based save has instead of Chess's legal-move check.
  Klondike and FreeCell must produce the whole pack back; Spider and
  Pyramid take cards out of play, so they get fitsPack and a count of
  their own. Each saveState() folds a run held in mid-drag back onto the
  pile it came from. 15 new rules checks and 27 new UI checks: an
  untouched deal saves nothing, a deal in progress renders identically
  after a round trip, a corrupt save is refused without disturbing the
  table. GHUB-0010's four small games can now reuse the codec.

- 📋 [GHUB-0009] **Sudoku saves and resumes.** Grid, pencil marks and elapsed
  time. Pause already covers the walk-away case, so this is the smaller half.
  Layman: Close a puzzle part way and come back to it.
  Kind: implement.
  Lanes: sudoku.

- ✅ [GHUB-0010] **Minesweeper, Reversi, Draughts and 2048 save and resume.**
  Small states, quick wins, worth doing in one pass once the codec above
  exists.
  Layman: The four quick games remember where you were too.
  Kind: implement.
  Resolved (2026-08-11): all four save and resume through the hub, with no
  save dialog. None of them keeps a move log, so each writes its board and
  each core's restore() is what re-checks it on the way back in — the pack
  check's equivalent. Minesweeper also carries its clock and its pause, and
  picks the clock up in activate() the way it does when you come back from
  another game; Draughts carries the last-move marks, because losing them
  turns a resumed game into a puzzle. Undo history is not saved, matching the
  solitaires. 12 new rules checks on the refusing paths (a board built
  deliberately wrong is the only way to reach them) and 30 new UI checks on
  the round trip. Full suite green: 353 rules checks, 115 UI checks.

### 📦 Getting it to other people

Fourteen finished games that only run from a build directory on one

machine. This section is about the gap between that and someone else

double-clicking a file.

- 🚧 [GHUB-0025] **Games Hub downloads as one file, on Linux and on Windows.**
  Nothing in this project has ever left the machine it was written on. There
  is no tag, no CI, no published build, and three changelog entries already
  owed a release. C++ helps here — a compiled binary needs no interpreter on
  the far side — but it does not carry Qt with it, so each platform needs
  its runtime bundled in a different way.

  Two halves. A workflow that builds and runs both test binaries on Linux
  and on Windows for every push, so a change that breaks one platform is
  caught by the machine rather than by a user; and a release workflow that
  turns a version tag into published files — a single-file AppImage for
  Linux and a portable zip for Windows, since a truly single Windows .exe
  needs a statically linked Qt and hours of build time per run.

  Windows has never been compiled once, so the first job is finding out what
  in fourteen games does not build there.
  **Layman:** Download one file, run it, play — no compiler, no build steps, on either operating system.
  Kind: release.
  Lanes: packaging, ci.
  Source: user-request-2026-08-12.
  Progress (2026-08-12): contract accepted as
  docs/specs/GHUB-0025-downloadable-builds.md after three cold-eyes loops
  (31 verified findings, all fixed). Implementation started.

### 🎨 Games agreed and not yet started

Asked for on 2026-08-10, in the order agreed. All are traditional or
public-domain; see the standing rules for why each is safe.

- 📋 [GHUB-0011] **Gin Rummy, two-handed against the computer.** Knocking,
  deadwood, gin and undercut. Medium.
  Layman: The classic two-player rummy game.
  Kind: feature.
  Source: user-request-2026-08-10.

- 📋 [GHUB-0012] **Cribbage, two-handed, with the pegging board.** The crib and
  the show included. Medium.
  Layman: Cribbage, board and all.
  Kind: feature.
  Source: user-request-2026-08-10.

- 📋 [GHUB-0013] **Blackjack against a dealer.** Traditional twenty-one. Small.
  Layman: Twenty-one against the house.
  Kind: feature.
  Source: user-request-2026-08-10.

- 📋 [GHUB-0014] **Spades, four-handed partnership trick-taking with bidding.**
  Reuses the Hearts shape almost wholesale. Medium.
  Layman: Partnership card game where you bid how many tricks you will win.
  Kind: feature.
  Source: user-request-2026-08-10.

- 📋 [GHUB-0015] **TriPeaks, Golf and Yukon.** Three more solitaires, and the
  cheapest work on this page: they reuse the card engine and the drag-and-drop
  wholesale. Small each.
  Layman: Three more games of patience.
  Kind: feature.
  Source: user-request-2026-08-10.

### 📚 Documentation

- 📋 [GHUB-0016] **Every game explains its own rules, inside the app.** Fourteen
  games ship with no instructions anywhere — a player who has never met Reversi
  or Canasta has to leave the program to learn it. Give `GameView` a virtual
  returning the game's rules as rich text, so a game carries its explanation in
  its own directory and adding a game means writing its rules next to its code;
  the hub shows them in one shared dialog so they all look the same. Worth a
  short line on the *controls* too — which button, what a click on the stock
  does — because that is what a player actually gets stuck on. **Standing rule
  2 binds harder here than anywhere else in this file: rule text is exactly
  what a rulebook author owns, so every word is written fresh.** Medium, and
  most of it is writing rather than code.
  Layman: A Rules button that tells you how to play whichever game is on
  screen.
  Kind: doc.
  Source: user-request-2026-08-10.

## P03 — Considered

Nothing here is agreed. 💭 means the scope, the value or the decision is still
open.

### 🖥 Legibility and accessibility

- 📋 [GHUB-0017] **The other thirteen games have had no legibility pass.** The
  owner is partially sighted and reads cards by their pip pattern rather than
  the corner index, which is why Canasta ended up with named discards, a wild
  count on every meld, cards drawn large enough for `CardArt::paintFace` to
  draw a face at all, and a computer that pauses long enough to be followed.
  None of that is true anywhere else. Expected to be wrong, and worth checking
  by rendering each game rather than by reasoning about it: the four solitaires
  draw cards far smaller than Canasta does, Hearts plays a trick with no record
  of what was led, Chess and Draughts announce nothing, and Sudoku's pencil
  marks are a third of a cell. Worth doing as one **Large cards / high
  contrast** switch the hub owns and every game reads, rather than as thirteen
  separate judgements. Large, and most of it is looking rather than typing.
  Layman: Make the other games as easy to read as Canasta now is.
  Kind: accessibility.
  Source: in-session-2026-08-11.
  Agreed (2026-08-11): owner approved the single hub-owned "Large cards /
  high contrast" switch as the shape. Moved from considered to planned; not
  yet started.

### 🎨 Play

- 💭 [GHUB-0018] **Canasta cannot take a move back.** Chess and Reversi can. A
  mis-clicked discard is gone, and it is the game whose cards are hardest to
  read — the two facts compound. One step is enough: the discard, or the last
  lay-down. Small.
  Layman: An undo button for Canasta.
  Kind: enhancement.
  Source: in-session-2026-08-11.

- 💭 [GHUB-0019] **Nothing on screen says which house rules are switched on.**
  GHUB-0016 covers teaching the games; this is the cheaper other half. Canasta
  has six house rules and the only place any of them is described is
  `README.md` on disk. A **Rules in force** panel listing what is on would
  answer it without the writing a rules screen needs. Small.
  Layman: A panel showing which of your own rules are turned on.
  Kind: ux.
  Source: in-session-2026-08-11.

### 🧰 Tests

- 💭 [GHUB-0020] **A legality check that does not rely on the author's
  imagination.** Four separate bugs on 2026-08-11 were positions where a move
  the player could see was legal got refused, all in one corner: where wild
  cards go. Every one passed the self-test, because the self-test checks
  positions somebody thought of. The check that would have caught at least two:
  over thousands of random positions, enumerate by brute force every legal way
  to take the pile and confirm the engine agrees. Medium, and it retires a
  class of bug rather than a bug.
  Layman: Have the tests find the illegal-move bugs instead of the player.
  Kind: test.
  Source: in-session-2026-08-11.

### 🎨 More games, if wanted

- 💭 [GHUB-0021] **More card games, none of which need a new asset.** Beyond the
  queue above the traditional catalogue is enormous and entirely free, roughly
  cheapest first: **War**, **Go Fish**, **Old Maid**, **Beggar-my-neighbour**
  (trivial rules, and the first genuinely child-friendly games here);
  **Crazy Eights** (the public-domain game Uno was built from — free under its
  own name, but do not use Uno's name, colours or card faces); **Sevens**, also
  called Fan Tan; **Whist** and **Euchre**, both reusing the Hearts shape;
  then **Rummy 500**, **Cassino**, **Pinochle**, **Bezique**, **Scopa** and
  **Briscola**, each needing its own scoring. All reuse `src/cards/`.
  Layman: A long list of traditional card games that would be cheap to add.
  Kind: research.
  Source: user-request-2026-08-10.

- 💭 [GHUB-0022] **Board games, on the same test and the same answer.**
  **Nine Men's Morris**, **Fox and Geese** and **Alquerque** are small and
  close in shape to the Draughts board already built. **Gomoku** and **Four in
  a Row** (Connect Four's game under a generic name) are small.
  **Backgammon** needs dice, a doubling cube and a real opponent — medium to
  large, and the most likely to actually be played. **Mancala** and
  **Dominoes** look like nothing else on the tile grid. **Snakes and Ladders**
  (the ancient *Moksha Patam*) and **Pachisi** are pure race games and suit
  children. **Go** has tiny rules and an opponent that is a research project —
  ship it only if a weak opponent is acceptable, and say so on the tile.
  **Halma**, **Hnefatafl**, **Shogi** and **Xiangqi** are deeper cuts, all
  free.
  Layman: A long list of traditional board games that would be safe to add.
  Kind: research.
  Source: user-request-2026-08-10.

### 🧹 Decided against, with reasons

Kept because the reasoning is the useful part: without it these get proposed
again.

- 💭 [GHUB-0023] **Cutting the pack, in Canasta — not built, deliberately.** At
  a table the cut stops the dealer stacking the deck and breaks up cards left
  clumped from the last hand. Neither can happen here, because every deal is a
  fresh random ordering, so a cut would be an animation that changes nothing
  and putting one on screen implies it matters. What *does* rotate is the deal:
  each hand a different seat deals and the player to their left leads, so the
  advantage moves round the table as it should. If a cut is ever wanted it
  belongs in the house-rules dialog as honest decoration, labelled as such.
  Layman: Cutting the cards would look right but change nothing, so it was left
  out.
  Kind: investigate.
  Source: user-request-2026-08-10.

- 💭 [GHUB-0024] **Choosing your colour, in Chess or Draughts — scope, not
  oversight.** Both put the human on the side that moves first, White and Red,
  and neither offers a swap or a board flip. That keeps `advance()` a single
  path with one `m_human`, which is the shape every engine game in the hub
  shares. Worth adding one day, but add it to *both* games at once and to that
  shared shape rather than special-casing Chess.
  Layman: You always play the side that moves first; changing that touches both
  games at once.
  Kind: enhancement.
  Source: in-session-2026-08-10.

## Standing rules — choosing what to add

Narration, not work. No bullet here takes a status.

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
