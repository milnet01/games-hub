<!-- ants-roadmap-format: 1 -->
# Games Hub — Roadmap

A collection of desktop games in one window. Pre-1.0 (0.2.0), so the blocks
below are phases rather than releases. Shipped items stay in the file and flip
to ✅; `CHANGELOG.md` is the separate user-facing record.

Execution order is positional: work a phase top to bottom. IDs are identity
only, so they are not in order and are never renumbered.

## P01 — Shipped

### 🎨 Games

- ✅ [GHUB-0001] **Chess, with a full rule set and an opponent at three strengths.**
  Castling, en passant, promotion and every draw rule. The move
  generator is proved by perft against the published node counts for four
  reference positions rather than by playing it.
  Layman: A proper game of chess against the computer.
  Kind: feature.
  Source: user-request-2026-08-10.

- ✅ [GHUB-0002] **Canasta for four, in partnerships, with an editable second rule set.**
  Melds, wild cards, red threes, freezing and taking the discard
  pile, going out and the full scoring table. Every number the game plays by
  lives in one `canasta::Rules` struct, which is what let the owner's six
  family rules land as values rather than as branches.
  Layman: Canasta against three computer players, including your family's own
  rules.
  Kind: feature.
  Source: user-request-2026-08-10.

- ✅ [GHUB-0003] **The other twelve games of the collection.**
  Reversi,
  Draughts, Minesweeper, Klondike, Spider, FreeCell, Pyramid, Sudoku, Hearts,
  Snake, 2048 and Pinball, all behind one hub window and one toolbar.
  Layman: Twelve more games, all in the same window.
  Kind: feature.
  Source: user-request-2026-08-10.

- ✅ [GHUB-0032] **A Canasta hand nobody goes out on can now be scored as dead.**
  New house rule `deadHandIfNobodyGoesOut` in canasta::Rules, off in
  Rules::classic() and ON by default in the House set — the one house
  default that deliberately differs from classic. Engine::scoreHand()
  scores both sides zero when it is on and m_outSeat < 0, which covers
  both ways a hand can die: the stock emptying with nobody able to take
  the pile, and the last stock card being a red three with nothing left
  to replace it.

  Reported after a real game ended 5215-1160 on an exhausted stock with a
  48-card frozen pile nobody could break into. The ending itself was
  correct Canasta and is unchanged; what was missing was the option not
  to score it. The summary panel now says the hand is dead rather than
  only "The stock ran out.", because two zeroes with no explanation reads
  as a lost score rather than a voided hand.

  Save format grows a tail pair (Engine::kTail 2 to 3, view blob version
  3 to 4); a game saved by an older build still loads and comes back
  without the rule.
  **Layman:** When the stock runs out and nobody has gone out, the round can now count for nothing instead of handing a big score to whoever sat on a frozen pile.
  Kind: feature.
  Source: user-report-2026-08-19.

- ✅ [GHUB-0033] **The Canasta AI no longer feeds the meld you are one card from finishing.**
  Ai::chooseDiscard already avoided ranks the opponents had melded, but
  with a flat penalty: feeding a six-card meld cost exactly what feeding
  a three-card meld cost. So with nothing safe in hand it would hand over
  a natural canasta as readily as a harmless card.

  The penalty is now canasta::discardRisk(), a free function beside
  handScoreFor so the ranking can be checked on a hand-built table rather
  than a position played into existence. It scales with the meld's
  distance from a canasta, so the least damaging throw is the meld with
  furthest to go, and it inverts for a rank a canasta has already closed
  under canastaMakesRankSafe — that rank cannot take the pile off anyone,
  which makes it the safest card in hand rather than the most dangerous.

  Unchanged by decision: a frozen pile still costs nothing to feed (it
  needs a matching pair out of hand), and Easy still ignores opponent
  melds entirely so that it stays beatable. canastaLevelsDiffer() still
  passes, so the strength ladder holds.
  **Layman:** The computer used to throw away the card that completed your canasta. Now it works out which of your melds is furthest from finishing and throws into that one instead.
  Kind: fix.
  Source: user-report-2026-08-19.

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

- ✅ [GHUB-0005] **Canasta saves and resumes.**
  The whole engine writes through
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

- 📋 [GHUB-0007] **Hearts saves and resumes.**
  Hands, tricks taken, passing
  direction and the running scores. The longest game in the hub after Canasta.
  Layman: Close Hearts mid-game and come back to it.
  Kind: implement.
  Lanes: hearts.

- ✅ [GHUB-0008] **The four solitaires save and resume.**
  Klondike, Spider,
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

- 📋 [GHUB-0009] **Sudoku saves and resumes.**
  Grid, pencil marks and elapsed
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

- ✅ [GHUB-0027] **The pre-push hook skips the pipeline on exactly the pushes that matter most.**
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
  Resolved (2026-08-12): both halves fixed. The loop now accumulates
  the changed files across every ref instead of overwriting a single
  range, and a ref the remote has never seen is asked for the commits it
  adds (`git log --name-only --not --remotes`) rather than diffed as a
  bare sha against the working tree. The docs-only arm is unchanged and
  still lints.

  The check the bullet asked for is `tests/pre-push-test.sh`, wired into
  ctest as `prepush` on Unix. It drives real `git push` calls at a
  throwaway bare remote — so the stdin format is git's own, not a guess
  — and asserts the arm taken for seven cases: first push, docs-only,
  mixed, `--follow-tags` release, new code branch, new prose branch, and
  a deletion. Run against the OLD hook it fails three of them (first
  push, release, new branch); against the new one all seven pass.
  ctest is 3/3.

- 📋 [GHUB-0031] **The Windows build rides on an action GitHub is deprecating the runtime under.**
  Every CI run now annotates: `ilammy/msvc-dev-cmd@0b201ec74f` (v1.13.0)
  targets Node.js 20 and is being FORCED onto Node.js 24. Today that is a
  warning and the Windows leg is green. When the forcing is withdrawn the
  action fails, and it is used in both workflows — `ci.yml:70` and
  `release.yml:216` — so it takes the Windows build and the Windows half of
  every release with it.

  Not urgent and not silent: it prints on every run. The fix is a version
  bump, and it must keep the pinned form — a commit SHA with the version in
  a trailing comment — because these workflows publish binaries strangers
  download and a moved tag on a third-party action would run arbitrary code
  against them. Check upstream has a Node 24 release first; if it has not,
  the fallback is calling `vcvarsall.bat` directly, which is fewer moving
  parts than an unmaintained action.

  Source: https://github.blog/changelog/2025-09-19-deprecation-of-node-20-on-github-actions-runners/
  **Layman:** A helper GitHub uses to build the Windows version is running on an old engine that GitHub will switch off; when it does, Windows builds and releases stop.
  Kind: chore.
  Source: in-session-2026-08-14 (CI annotation on run 31811637751).

### 🎨 Games agreed and not yet started

Asked for on 2026-08-10, in the order agreed. All are traditional or
public-domain; see the standing rules for why each is safe.

- 📋 [GHUB-0011] **Gin Rummy, two-handed against the computer.**
  Knocking,
  deadwood, gin and undercut. Medium.
  Layman: The classic two-player rummy game.
  Kind: feature.
  Source: user-request-2026-08-10.

- 📋 [GHUB-0012] **Cribbage, two-handed, with the pegging board.**
  The crib and
  the show included. Medium.
  Layman: Cribbage, board and all.
  Kind: feature.
  Source: user-request-2026-08-10.

- 📋 [GHUB-0013] **Blackjack against a dealer.**
  Traditional twenty-one. Small.
  Layman: Twenty-one against the house.
  Kind: feature.
  Source: user-request-2026-08-10.

- 📋 [GHUB-0014] **Spades, four-handed partnership trick-taking with bidding.**
  Reuses the Hearts shape almost wholesale. Medium.
  Layman: Partnership card game where you bid how many tricks you will win.
  Kind: feature.
  Source: user-request-2026-08-10.

- 📋 [GHUB-0015] **TriPeaks, Golf and Yukon.**
  Three more solitaires, and the
  cheapest work on this page: they reuse the card engine and the drag-and-drop
  wholesale. Small each.
  Layman: Three more games of patience.
  Kind: feature.
  Source: user-request-2026-08-10.

- 📋 [GHUB-0036] **Poker against three computer players, with chips and betting.**
  Asked for 2026-08-19, after the owner noticed the collection has
  no poker. Clears the standing-rules test outright: poker's rules are
  public domain and "Poker" is a generic name, as are "five-card draw" and
  "Texas hold'em" (a place plus a game). Nothing here needs a new asset -
  the shared deck in cards/card.* and cards/cardart.* already draws
  everything a poker table shows.

  VARIANT NOT YET CHOSEN, and it decides most of the work. Five-card draw
  is the smallest honest poker: one draw, two betting rounds, no shared
  cards, and it is the variant most people learn first. Texas hold'em is
  what most people now mean by the word, needs community cards and four
  betting rounds, and has far more written about its strategy for an AI to
  be measured against. Seven-card stud sits between them. Pick one and
  ship it rather than building a variant framework nobody asked for.

  What is genuinely new, and why this is bigger than another solitaire.
  None of the fourteen games has money in it, so a chip stack, a pot, side
  pots when someone is all-in, and a betting round that goes round until
  the bets are level are all new machinery. A hand evaluator that ranks
  any five of seven cards is new too, and is the one part with an exact
  right answer - it should be checked exhaustively in the selftest rather
  than by playing, the way chess move generation is checked by perft
  rather than by eye.

  The AI is the interesting half and the reason to give this room. Every
  other opponent here plays a game of complete or nearly complete
  information; poker is the first where the computer must bet on what it
  CANNOT see, and where bluffing is part of correct play rather than a
  flourish. Expect the four-rung ladder Canasta uses - and expect it to
  need canastaLevelsDiffer()'s treatment, each rung played against the one
  below it, because a poker AI that is merely described as harder is
  indistinguishable from one that is not.

  Legibility is a first-class constraint here, not a pass to be done
  afterwards. The owner is partially sighted and reads cards by their pip
  pattern, so the table must SAY what you hold - "two pair, kings and
  fours" in words under the hand, the way Canasta names the last discard -
  rather than leaving the player to read five cards and rank them. The
  same goes for the bet: what it costs to call, in figures, without
  arithmetic. Design that in from the start; GHUB-0017's per-game passes
  exist because it was not.

  Saving follows Chess's shape where it can: a hand in progress is a
  position plus a betting history, and the pack check that cardcodec's
  matchesPack does for the solitaires applies here too.

  Open question for the owner beyond the variant: play money only, with a
  stack that resets, or a running bankroll across sessions the way best
  scores persist?
  **Layman:** A poker table against three computer opponents, with chips to bet and a hand that says in words what you are holding.
  Kind: feature.
  Source: user-request-2026-08-19.

### 📚 Documentation

- 📋 [GHUB-0016] **Every game explains its own rules, inside the app.**
  Fourteen
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
  Owner's call (2026-08-12): parked, neither route taken. Asked
  directly which of the two the bullet offers he wanted — capture it
  himself, or a --screenshot option — and the answer was to leave it for
  now and let the README keep the six-game picture. So the --screenshot
  option is NOT declined on its merits, it is simply unbuilt; if this is
  picked up later both routes are still open. Stays 📋.

- 📋 [GHUB-0029] **cardart.h says it serves three games; it serves six.**
  The header comment on src/cards/cardart.h reads "Shared card drawing
  for Klondike, Spider and Hearts, so the three games look like one deck
  rather than three." Six game views include it: canasta, freecell,
  hearts, klondike, pyramid, spider — confirmed by
  `grep -rl cardart.h src --include=*.cpp` minus cardart.cpp itself.

  Trivial, and filed rather than fixed in passing because it surfaced
  during a documentation review, which does not edit code. It matters a
  little more than a normal stale comment: GHUB-0017 makes this file the
  home of kFaceMinWidth, the threshold all six games must compute
  against, so a reader arriving there to understand that constant is
  told the wrong set of callers on the first line.

  Fix is one line. Worth folding into the first GHUB-0017 commit that
  touches cardart.h rather than taking a commit of its own.
  **Layman:** The comment at the top of the shared card-drawing file names the wrong games.
  Kind: doc-fix.
  Source: in-session-2026-08-13 (found building the GHUB-0017 review packet).

## P03 — Considered

Nothing here is agreed. 💭 means the scope, the value or the decision is still
open.

### 🖥 Legibility and accessibility

- 🚧 [GHUB-0017] **The other thirteen games have had no legibility pass.**
  The
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
  Spec (2026-08-13): docs/specs/GHUB-0017-legibility-switch.md, accepted.
  Covers the shared mechanism ONLY — a hub-owned Legibility singleton, a
  GameView::applyLegibility hook, CardArt's 46px threshold exported as
  kFaceMinWidth, the toolbar control, and 2048's ink fixed
  unconditionally. The fourteen per-game passes stay under this bullet
  and follow one at a time, each shown to the owner first (owner's call,
  2026-08-12).

  Two things measurement changed. Contrast is mostly already fine —
  card and Sudoku inks all clear WCAG's 4.5 — so SIZE is the lever and
  the item's "high contrast" half buys less than assumed. And 2048 is a
  plain bug: inkFor() returns near-white for every tile from 8 up,
  giving 1.50-1.72 against a 3.0 floor, so it is fixed for everyone
  rather than behind the switch.

  Cold-eyes: 3 loops, 3 lanes each, 29 verified findings all fixed, 1
  dismissed. Reached the 3-loop cap without an empty loop; §12 records
  the assessment that this is document growth (434 to 727 lines) rather
  than an unsettled contract — no lane challenged the mechanism after
  loop 1. Two invariants (INV-3 card size, INV-6 reversibility) were
  withdrawn to the per-game pass contract: both were untestable against
  a change that adapts no game, and INV-3 was unwritable at all since
  cardWidth() is private on all six card views.
  Mechanism shipped (2026-08-14): src/legibility.* (the singleton, key
  display/legibility, default off), GameView::applyLegibility with the
  connection made in the base constructor, the "🔍 Normal / 🔍 Large"
  toolbar action, CardArt::kFaceMinWidth, 2048's ink replaced by a
  luminance test, and scripts/legibility-check.py (17 contrast pairs +
  INV-4's grep, both green). Four uitest blocks lock INV-1, INV-2, INV-5
  and INV-7; each was seen failing against a deliberate break first.
  ctest 3/3.

  The bullet stays open: no game responds to the switch yet. The fourteen
  per-game passes follow one at a time, each shown to the owner before
  the next is started, and whichever lands first carries withdrawn INV-3
  and INV-6 with its own numbers and owns making its layout observable.

  Three spec clauses the build proved wrong are folded back into §12
  rather than silently fixed — INV-5's "Breaks when" (a drifted block
  does NOT stay green: Scores::clear() wipes the whole settings store),
  INV-1's block needing contains() on both halves, and the script's own
  array parser, which skipped a four-argument QColor and measured the
  wrong Minesweeper colour. §2.3's hand-transcribed table was right.
  Canasta's pass shipped (2026-08-19) — the first of the fourteen, so it
  carries withdrawn INV-3 and INV-6 in its own numbers. Under the switch
  CanastaView::minimumSizeHint() returns 900x656 and cardWidth() floors at
  CardArt::kFaceMinWidth / kMeldScale, so a melded card goes from 37.1 px
  at the smallest window to 46.4 and shows a face. applyLegibility lands
  in-flight cards first (Flight::to is captured at launch) and keeps the
  pre-clamp window size, so turning the switch off is not one-way. Four
  uitest blocks lock it; each was seen red against a deliberate break, and
  the first version of INV-3 passed a broken build until cardsFitTable()
  was added. selftest 366/366, uitest 126/126, ctest 3/3.

  Measurement that shrinks what is left: the other five card games do NOT
  need a size pass. Every card view calls setMinimumSize(minimumSizeHint()),
  so the smallest card each can be driven to is Klondike 67.9, FreeCell
  67.4, Pyramid 68.6, Spider 54.2 and Hearts 52.5 - all clear of the 46-px
  threshold. The spec's 2.1 reasoned from the floor CONSTANTS (30/32/34)
  and named Spider the likeliest to go faceless; no window can drive any of
  them there. Canasta was the only game drawing a faceless card, and only
  in its melds - the opponents' hands are drawn at 0.8 but face DOWN, so
  the threshold never applied to them either.

  Still open: thirteen games unvisited. What is left for them is reading
  pace, contrast and what a game says out loud (Hearts naming the led suit,
  Sudoku's pencil marks at a fifth of a cell), not card size.
  Release decision (2026-08-19): 0.4.0 was considered and deliberately
  NOT cut. [Unreleased] holds six entries and is arguably owed one, but it
  advertises "a legibility switch, on or off for the whole collection"
  while thirteen of the fourteen games still ignore it — so cutting now
  ships a switch that visibly does nothing outside Canasta and 2048's
  unconditional ink fix. Canasta's pass was done first to make that line
  true. Cut 0.4.0 once one or two more passes land, or reword the
  changelog entry to say what the switch actually reaches today; do not
  re-derive this from scratch.
  Sudoku's pass shipped (2026-08-19) — the second of the fourteen, and the
  first that changes no geometry at all. Contrast was never Sudoku's problem
  (pencil ink 4.88:1); the mark font was. Under the switch it goes from
  cell*0.20 to cell*0.29 and turns bold, and nothing else on the board moves —
  no minimum-size change, because Sudoku has no cliff of Canasta's kind and
  the marks scale smoothly with the window. A minimum-size pass here would
  have been invisible at any window a player actually uses.

  Two things worth not re-deriving. Qt::TextDontClip is what the pass really
  turns on: drawText clips to its rect, each mark gets a cell third, and the
  font's LINE box rather than the digit's ink is what has to fit — so 0.20 was
  already near the ceiling and raising the ratio alone clips every mark. And
  the ratio is measured, not chosen: the app font's digits are ~0.685 of an em,
  so ink lands at ~2.74x the ratio as a fraction of the cell third; 0.30 goes
  red at the smallest window, where a 34-px cell rounds the ink up a whole
  pixel. marksFitCell() checks the ink with QFontMetricsF rather than trusting
  that constant, so a platform with taller digits fails rather than drawing
  marks that touch.

  INV-6 lands here as originally written - two renders that must match with a
  third between them that must not. It needed a per-view latch to be seen
  failing; a static one latched during the earlier test block and made all
  three renders symmetric, which is a bad break rather than a passing test.
  INV-3 stays Canasta's - Sudoku draws no cards, and marksFitCell() is this
  pass's equivalent. Five uitest assertions, each seen red against its own
  deliberate break. selftest 366/366, uitest 131/131, ctest 3/3.

  Still open: twelve games unvisited. Sudoku is now off that list, so what is
  left is reading pace, contrast and what a game says out loud - Hearts naming
  the led suit, Chess and Draughts announcing nothing.
  Corrected (2026-08-19) after the Windows CI leg went red. The 0.29 ratio
  recorded above was measured on the wrong thing: it is the largest that fits
  THIS machine's font, and the ceiling is not a property of this codebase but
  of how tall the platform draws a digit. Measured on the owner's Windows box
  over SSH: Segoe UI 0.728 of an em, Arial 0.731, Tahoma 0.760, against 0.685
  here. Under Segoe UI a 0.29 mark is 0.845 of its cell third in exact
  arithmetic - inside the 0.85 limit - and tips past it once tightBoundingRect
  rounds to whole pixels at the smallest cell. windows-2022 failed on exactly
  the assertion written to catch it while ubuntu-24.04 passed.

  No local run could have caught it and that is not drift: scripts/local-ci.sh
  executes ci.yml's own run: blocks, but nothing on Linux drives MSVC, and the
  script says so on every run.

  The fix removes the constant instead of re-tuning it. markFont() probes the
  font once for its ink-per-point, takes the analytic size, then steps down
  until the ink MEASURED AT THE SIZE IT WILL BE DRAWN AT fits - so the mark is
  the largest each platform allows rather than a number right in one place and
  wrong or timid elsewhere. marksFitAt(pointSize) was added because once the
  size is solved "it fits" is true by construction; the assertion with teeth
  is "a step larger would not".

  The regression test became a portability test: it loops over the machine's
  own font families, locally spanning 0.49 to 0.99 of an em, which brackets
  every Windows candidate. Restoring the tuned constant reddens it LOCALLY,
  which is the whole point - verified by doing exactly that.

  Standing note for future passes: a per-game legibility number derived from
  font metrics is a platform property. Solve it, do not tune it.
  Correction to the note above (2026-08-19, same day). Two figures in it are
  not comparable and the conclusion drawn from them was wrong. "This machine
  0.685 of an em" was measured at 7-10 POINT sizes, where hinting rounds the
  ink by a whole pixel; the Windows figures (Segoe UI 0.728, Arial 0.731,
  Tahoma 0.760) were measured via GDI+ at em 100, unbold. Measured properly at
  em 100 this machine's bold face is 0.742 - TALLER than Segoe UI, not shorter.
  So "Windows draws taller digits" is not supported, and the operative cause is
  rasterisation at the small sizes a mark is drawn at rather than the typeface.

  The fix is unaffected and is if anything better justified: solving against the
  ink measured AT THE SIZE IT WILL BE DRAWN is right precisely because the
  rounded number is the one that decides, and no ratio taken at another size
  predicts it.

  The second CI run then failed differently and usefully. "as large as it fits"
  PASSED on Windows - so the solve works there - while an absolute growth
  assertion (1.15x) failed, and the font-family count assertion (>= 5) failed
  because a bare runner installs few faces. Both were assertions about the
  ENVIRONMENT wearing the clothes of assertions about the code. They now ask the
  font what growth is arithmetically available (0.2125 / inkPerEm / 0.20) and
  assert only where there is room, printing the measured figures either way; the
  family count asserts >= 1 and prints what it tried. This machine reports 0.742
  of an em, a mark solved to 9.73pt against 6.80pt normal, and 13 families.

  Lesson worth keeping past this item: a test may assert what the code does, and
  must only REPORT what the platform happens to provide. Three attempts were
  needed here because each rewrite put a new environment constant in the
  assertion.

- 📋 [GHUB-0030] **The toolbar label goes stale if anything but the button moves the switch.**
  Not a defect today, and deliberately not fixed while filing: the
  toolbar action is the only writer of Legibility, so its own toggled
  signal keeps the label right, and spec GHUB-0017 §4.3 specifies
  exactly that code after three cold-review loops.

  It is a hazard for the per-game passes. The action subscribes to
  itself, not to Legibility::changed, so a second writer — a keyboard
  shortcut, a settings dialog, a game offering its own toggle — leaves
  the button reading "🔍 Normal" while large play is on. Seen directly:
  driving Legibility::instance().setEnabled(true) from a test renders a
  toolbar still labelled Normal and unchecked.

  The fix is one connect from Legibility::changed to the label, guarded
  against the loop back through setEnabled. Worth doing the moment a
  second writer appears, not before. Spec §10 already records that
  nothing checks this label — it was found by rendering the toolbar,
  which is the only thing that can see it.
  **Layman:** The 'Large / Normal' button would show the wrong word if the setting were ever changed from somewhere other than that button.
  Kind: accessibility.
  Source: in-session-2026-08-14 (observed while rendering the hub for GHUB-0017).

- ✅ [GHUB-0037] **The legibility switch itself, hub-owned and read by every game that has had its pass.**
  Split out of GHUB-0017 (2026-08-19) so the release ledger can say what
  actually shipped. GHUB-0017 stays open: it tracks the fourteen per-game
  passes, twelve of which are still to come, and an umbrella that cannot
  be ticked made every changelog entry crediting it read as a claim that
  the whole thing was done.

  What shipped here is the MECHANISM and nothing that adapts a game:
  src/legibility.* (the singleton, key display/legibility, default off),
  GameView::applyLegibility with the connection made in the base
  constructor so a game built earlier still hears the switch move, the
  "Normal / Large" toolbar action, CardArt::kFaceMinWidth exported so a
  game can compute against the threshold instead of a literal, and
  scripts/legibility-check.py with its 17 contrast pairs.

  2048's ink rides along and is the one thing here a player sees without
  touching the switch. inkFor() picked the colour from the tile's NUMBER,
  so every tile from 8 up got near-white text on mid-orange or mid-yellow
  - as low as 1.50:1 against a 3:1 floor. It is now picked from the tile's
  measured brightness and every tile clears 4.9:1. Fixed for everyone
  rather than hidden behind the switch, because a tile nobody can read is
  a defect and not a preference.

  Contract: docs/specs/GHUB-0017-legibility-switch.md, accepted. Four
  uitest blocks lock INV-1, INV-2, INV-5 and INV-7, each seen red against
  a deliberate break.
  **Layman:** The Large/Normal switch in the toolbar, and the 2048 tiles whose numbers nobody could read.
  Kind: accessibility.
  Source: in-session-2026-08-19 (split out of GHUB-0017 for the 0.4.0 release).

- ✅ [GHUB-0038] **Canasta answers the legibility switch: a melded card shows a face instead of a sliver.**
  The first of the fourteen per-game passes (GHUB-0017 section 9), split
  out so it can be ticked. It carries the two invariants withdrawn from
  the mechanism spec, INV-3 and INV-6, in its own numbers.

  Under the switch CanastaView::minimumSizeHint() returns 900x656 and
  cardWidth() floors at CardArt::kFaceMinWidth / kMeldScale, so a melded
  card goes from 37.1 px at the smallest window to 46.4 and draws a face.
  Growing the melds in place was tried first and cannot work: a face-
  clearing seven-card canasta is about 130 px tall against the 107 px
  bandFor() allows, and the overflow runs into the stock and discard row.

  applyLegibility lands in-flight cards first - Flight::to is a point
  captured at launch, so any geometry change mid-animation misplaces them
  - and keeps the pre-clamp window size so turning the switch off is not
  one-way. Four uitest blocks lock it, each seen red against a deliberate
  break; the first version of INV-3 passed a broken build until
  cardsFitTable() was added.

  Measurement that shrank what was left: the other five card games need no
  size pass. Every card view calls setMinimumSize(minimumSizeHint()), so
  the smallest card each can reach is Klondike 67.9, FreeCell 67.4,
  Pyramid 68.6, Spider 54.2 and Hearts 52.5 - all clear of the 46-px
  threshold. Canasta was the only game drawing a faceless card.
  **Layman:** Turn Large on and Canasta's melded cards get big enough to show their pips rather than a letter in the corner.
  Kind: accessibility.
  Source: in-session-2026-08-19 (split out of GHUB-0017 for the 0.4.0 release).

- ✅ [GHUB-0039] **Sudoku answers the legibility switch: pencil marks grow to the largest that fit, and turn bold.**
  The second per-game pass (GHUB-0017 section 9), split out so it can be
  ticked. Contrast was never Sudoku's problem - the pencil ink measures
  4.88:1 - so this is entirely size. No geometry and no minimum-size
  change: the marks scale with the window, so a minimum-size pass of
  Canasta's shape would have been invisible at any window a player uses.

  The find was that cell*0.20 already sat against a ceiling nobody had
  written down. drawText clips to its rect, each mark gets a cell third,
  and it is the font's LINE box rather than the digit's ink that has to
  fit - so raising the ratio alone clips every mark. Qt::TextDontClip
  hands over the room between ink and line box.

  The size is SOLVED, not tuned, and that cost three red Windows CI runs
  to learn. A ratio measured here is a property of the platform's font and
  its rasterisation at 7-11 point, not of this codebase. markFont() now
  probes the font for its ink-per-point and steps down until the ink
  measured at the size it will be drawn at fits, so the mark is the
  largest each platform allows. The regression test became a portability
  test over the machine's own font families; restoring the tuned constant
  reddens it locally.

  Standing lesson recorded on GHUB-0017: a test may ASSERT what the code
  does and must only REPORT what the platform provides.
  **Layman:** Turn Large on and Sudoku's little candidate numbers get as big as they can while nine still fit in a square.
  Kind: accessibility.
  Source: in-session-2026-08-19 (split out of GHUB-0017 for the 0.4.0 release).

### 🎨 Play

- 💭 [GHUB-0018] **Canasta cannot take a move back.**
  Chess and Reversi can. A
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

- ✅ [GHUB-0034] **Canasta's rule set and Minesweeper's difficulty are remembered between sessions.**
  One defect shape in two games. Both settings were already
  persisted - but only inside a SAVED game, which is exactly as long as
  the game stays unfinished. CanastaView writes m_useHouse into its blob
  and MinesweeperView writes m_level into its own; saveState() returns an
  empty blob for a game that is over, and HubWindow treats an empty state
  as "clear whatever was stored". So finishing a hand under House rules,
  or winning on Expert, put the player back on Classic and Intermediate
  with nothing to show they had ever chosen otherwise.

  Fixed the way Canasta already stored canasta/sortHand and
  canasta/target: a QSettings key written at the one point every explicit
  choice funnels through, and read in the constructor before the toolbar
  actions are ticked. canasta/useHouse is written in applyRules(), which
  all three rule-set routes call; minesweeper/level is written in
  newGame(), which every level action calls. Minesweeper's read is
  clamped, because the store is a file the game does not own and an
  out-of-range index reads kLevels past its end.

  restoreState() deliberately does NOT write either key: resuming a saved
  game should not re-point the preference at the game being resumed.

  Six uitest assertions, each written as the player meets the setting -
  choose, close, reopen - and each seen red against its own deliberate
  break. Asserting the key alone would pass on a build that writes it and
  never reads it back, which is half the defect. selftest 366/366, uitest
  137/137.

  Not done here, and not asked for: Sudoku's Easy/Medium/Hard is the same
  shape and is still forgotten, as is Spider's suit count.
  **Layman:** Pick House rules or Expert once and the game opens that way next time, instead of forgetting as soon as you finish a game.
  Kind: fix.
  Source: user-request-2026-08-19.

- ✅ [GHUB-0035] **The computer plays the first round knowing its throw cannot be taken, and both teams' opening minimums are on the table.**
  Three things, all from the owner in one sitting.

  The RULE was already right and already implemented. noMeldingFirstRound
  bars melding until every seat has played, and validateTake refuses while
  that holds, on the stated grounds that taking the pile always melds the
  top card. What was missing was that the AI had no idea: not one
  reference to the first round anywhere in canastaai.cpp.

  Two seats' worth of arithmetic decide it. m_turnsTaken counts finished
  turns, so the seat playing now is turn m_turnsTaken and the seat after
  it is m_turnsTaken + 1 - which is why Engine::discardCannotBeTaken() is
  +1 and meldingAllowed() is not. Seats 1 to 3 throw into a pile nobody
  can touch; the FOURTH seat's throw is live, because the next turn is the
  first seat's second and the rule has lifted by then. Getting that wrong
  is right three times in four.

  What the AI does with it. A black three's only worth is stopping the
  next seat taking the pile, so in the safe window the +12 bonus becomes a
  -15 penalty - held, it still blocks later; thrown, it buys nothing. Kept
  small enough that a hand with nothing else legal still throws one.

  The other half is subtler and the first attempt at it was DEAD CODE.
  "The throw is free, so dump your most dangerous card" cannot be built on
  discardRisk, because that returns 0 for any rank the opponents have not
  melded and under this very rule nobody has melded yet - so it is zero
  for every card in the hand. What ships instead gathers every
  pile-safety judgement into one `safety` accumulator and drops the lot
  when the throw cannot be taken, leaving the hand-value terms to answer.
  Play outside the first round is unchanged to the last decimal, which the
  AI strength ladder confirms.

  Display, and the owner's reason for it: he does not look at the status
  bar during a game - his focus is the play area - so both additions are
  ON THE TABLE. The centre strip gains "FIRST ROUND - your throw is safe",
  switching to "FIRST ROUND ENDS - the next seat can take" on the fourth
  seat, which is exactly the moment the protection stops. Each score plate
  gains its team's opening requirement beside the team name, gold while
  owed and dim once paid; the plate minimum widened from 150 to 178 px
  because at 150 the two strings met in the middle at the smallest window.
  Verified by rendering the table at 1000x720 and at the 720x560 minimum
  and looking at both.

  noMeldFirstRound now defaults ON in the House set - the owner's family
  rule rather than a variation offered. Only reaches a profile that has
  never saved house rules; a stored 0 is a choice and stays one.

  Seven selftest assertions, each seen red against its own deliberate
  break. selftest 397/397, uitest 137/137. The AI ladder still holds at
  all four rungs, and one of the breaks toppled "expert beats hard",
  which is the ladder proving it is sensitive to this change rather than
  decorative.
  **Layman:** Under house rules nobody can take the pile in the first round, so the computer stops wasting its black threes there - and each team's "points needed to open" now shows on its own score plate.
  Kind: enhancement.
  Source: user-request-2026-08-19.

### 🧰 Tests

- 💭 [GHUB-0020] **A legality check that does not rely on the author's imagination.**
  Four separate bugs on 2026-08-11 were positions where a move
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

- 💭 [GHUB-0021] **More card games, none of which need a new asset.**
  Beyond the
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

- 💭 [GHUB-0023] **Cutting the pack, in Canasta — not built, deliberately.**
  At
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

- 💭 [GHUB-0024] **Choosing your colour, in Chess or Draughts — scope, not oversight.**
  Both put the human on the side that moves first, White and Red,
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
