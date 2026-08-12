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

- ✅ [GHUB-0025] **Games Hub downloads as one file, on Linux and on Windows.**
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
  Resolved (2026-08-12): v0.3.0 is published with both files —
  GamesHub-0.3.0-x86_64.AppImage (49 MB) and
  GamesHub-0.3.0-windows-x64.zip (62 MB). CI builds and runs both test
  binaries on ubuntu-24.04 and windows-2022 for every push; all fourteen
  games compile under MSVC, which had never been tried. The published
  AppImage was downloaded to this machine and runs. Three source changes
  the port needed: std::numbers::pi for M_PI, --version answered from
  argv before Qt starts, and a hand-written Fisher-Yates shuffle so a
  seed deals the same game on both standard libraries. Contract and its
  three cold-eyes loops in docs/specs/GHUB-0025-downloadable-builds.md.

- ✅ [GHUB-0026] **The release smoke tests never start Qt, so they cannot see a broken bundle.**
  Both smoke tests run the artifact with --version, and GHUB-0025 made
  --version return before QApplication is constructed so it works headless
  on Windows. The consequence is that neither test loads a Qt plugin, opens
  a window or plays a sound: a bundle missing its platform plugin passes
  every gate and reaches a user as "could not load the Qt platform plugin".

  This is not theoretical. The published 0.3.0 AppImage carries only the
  xcb platform plugin, so it runs on a desktop — verified by downloading it
  and launching it here — but aborts under QT_QPA_PLATFORM=offscreen. The
  staged-tree assertion (INV-7) is the only thing catching a missing plugin
  today, and it checks that the directory is non-empty, not that the right
  plugin is in it.

  The fix is to make the container test actually start the app: bundle the
  offscreen plugin (EXTRA_QT_PLUGINS=offscreen for linuxdeploy-plugin-qt),
  then run the AppImage with QT_QPA_PLATFORM=offscreen under a timeout and
  require that it is still alive when the timeout fires. That exercises Qt
  init, the hub and one game inside a container with no Qt installed, which
  is the property the download actually has to have. The Windows job can do
  the same with Start-Process and a timeout.
  **Layman:** The checks that guard a download prove it can say its version, not that it can open a game.
  Kind: test.
  Lanes: packaging, ci.
  Source: in-session-2026-08-12.
  Progress (2026-08-12): written and lint-clean, not yet verified.
  Both smoke tests now run the artifact twice — once for --version, once
  starting --game spider under QT_QPA_PLATFORM=offscreen, passing only if
  the process is still alive when a 20 s timeout fires. The offscreen
  plugin is bundled for it: EXTRA_PLATFORM_PLUGINS=libqoffscreen.so on
  Linux, an explicit copy of qoffscreen.dll beside windeployqt on Windows.
  The bullet's own prescription (EXTRA_QT_PLUGINS=offscreen) was wrong:
  that name is a deprecated alias for EXTRA_QT_MODULES and matches Qt
  modules, so it would have matched nothing and failed silently.
  Also fixed in passing: the step's shell is bash -e without pipefail, so
  the container's exit status was being discarded by tee.
  Stays in-progress deliberately — release.yml runs on a tag only, so
  nothing has executed this. The v0.3.1 run is its first observation.
  Resolved (2026-08-12): shipped in v0.3.1 and observed green.
  The v0.3.1 release run executed the new checks for the first time.
  Evidence rather than a green tick: the deploy log carries "Deploying
  extra platform plugin: libqoffscreen.so"; the two smoke steps took 35 s
  and 21 s, consistent with a 20 s run survived rather than skipped; and
  the container log carries the offscreen plugin's own
  propagateSizeHints() line, which is Qt initialising inside a machine
  with no Qt. The published AppImage was downloaded here and carries
  libqoffscreen.so and libqxcb.so, where 0.3.0 carried only the latter.

  Two things the cold-eyes gate on GHUB-0025 changed on the way. The
  bullet prescribed EXTRA_QT_PLUGINS=offscreen, which is a deprecated
  alias for EXTRA_QT_MODULES and matches Qt modules — it would have
  matched nothing, silently. The variable is EXTRA_PLATFORM_PLUGINS and
  it takes full sonames. And liveness alone is not a sufficient signal on
  Windows: a release build with no console does not exit when the
  platform plugin will not load, it shows a blocking message box and
  reaches qFatal only when someone dismisses it, so HasExited would have
  passed the exact failure this item exists to catch. That run now also
  asserts the process owns no top-level window.

  Still uncovered, deliberately: the running check runs offscreen, so it
  cannot see a missing DESKTOP platform plugin — the staged-tree
  assertion is the only guard for that, and for the multimedia plugin.
  INV-6's no-window clause fires only on failure, so a green release
  cannot confirm it.

- 📋 [GHUB-0027] **The pre-push hook skips the pipeline on exactly the pushes that matter most.**
  git feeds the hook one line per ref being pushed, and the loop in
  .githooks/pre-push assigns RANGE inside that loop rather than
  accumulating, so the LAST ref wins. `git push --follow-tags` sends the
  tag last, and a new ref takes the `remotesha = ZERO` branch, which sets
  RANGE to a bare sha. `git diff --name-only <sha>` then compares that
  commit against the working tree — clean, so nothing comes back. CHANGED
  is empty, CODE is empty, and the hook takes its documentation-only path
  and runs `local-ci.sh --lint`.

  Observed on the v0.3.1 push (2026-08-12), which carried CMakeLists.txt,
  release.yml, CLAUDE.md and the spec, and printed "Lint-only run
  (documentation change) — build and tests not run". No harm done: the
  full pipeline had been run by hand minutes earlier. The defect is that
  a release push is the one push where the guard is least likely to be
  questioned and most expensive to get wrong.

  Two things to fix, not one. Accumulate the file list across every ref
  instead of overwriting a single RANGE, and stop using `git diff` for a
  new ref — against a bare sha it diffs the working tree, which is never
  what was meant; `git show --name-only` or a diff against the remote
  branch is. A tag whose commit is already covered by the branch push
  then contributes nothing rather than erasing everything.

  Worth a check while in there: the docs-only path is a real feature and
  should keep working, so the fix needs a case that proves a docs-only
  push still lints and a mixed push still builds.
  **Layman:** The guard that runs the tests before a push quietly does nothing when you push a release.
  Kind: fix.
  Source: in-session-2026-08-12.

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

- 📋 [GHUB-0028] **The README's screenshot is from a six-game build.**
  docs/hub.png shows Reversi, Minesweeper, Solitaire, Spider, Hearts and
  Pinball, over a status bar reading "Six games. Pick one." It is the
  first thing anyone sees on the repository page, and it undersells the
  collection by eight games. Everything else in README.md was brought up
  to date on 2026-08-12; this was not, because it cannot be.

  No session on this machine can replace it unaided. The existing image
  is a real desktop capture, with KDE window decorations and a shadow,
  and the offscreen platform an agent can drive produces no decorations
  and cannot be screenshotted the same way. Two routes, and the choice is
  the owner's: either he captures the hub himself and drops the file at
  docs/hub.png, or the app gains a small --screenshot <file> option that
  grabs its own window, which is one QWidget::grab plus a save and would
  also make future refreshes a single command.

  Worth doing either way when a game is added, since the same staleness
  returns silently: nothing checks that the picture matches the tile
  grid, and no test can, so it is a standing manual step.
  **Layman:** The picture at the top of the README shows six games when there are fourteen.
  Kind: doc-fix.
  Source: in-session-2026-08-12.

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
