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

- ✅ [GHUB-0007] **Hearts saves and resumes.**
  Hands, tricks taken, passing
  direction and the running scores. The longest game in the hub after Canasta.
  Layman: Close Hearts mid-game and come back to it.
  Kind: implement.
  Lanes: hearts.
  Resolved (2026-08-25): `HeartsEngine::save`/`load` write the position -- both
  hands and passes, both score columns, the trick on the table, phase, hand number,
  whose turn it is, the leader, the last winner, the trick count and whether hearts
  are broken -- and `HeartsView` adds what the view holds that the rules do not:
  the cards lifted for a pass nobody has confirmed, and whether a finished trick is
  still sitting there.

  No move log, so the save is the position and the PACK re-checks it, the way
  Spider and Pyramid do rather than the way Chess does. Hearts takes four cards out
  of play with every collected trick, so it gets `fitsPack` plus a count of its
  own: hands plus the cards on the table must equal fifty-two less four per trick.
  Without that a blob restores a full hand of thirteen into trick nine, which is a
  position the game cannot reach and a hand that cannot be played out.

  The clock is deliberately NOT restarted in restoreState. The hub calls activate()
  immediately afterwards and that already works out whether the computers owe a
  move; starting it in both places runs the timer twice.

  Checked in uitest the way the other eleven are -- save, restore into a fresh
  view, and the two renders must be the same picture, against a fresh deal that is
  different. The count check is exercised by a forged blob, and the forgery was
  verified to hit the check it names: with the count test removed the blob loads
  and the check goes red, so it is not passing for some other reason.

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

- ✅ [GHUB-0009] **Sudoku saves and resumes.**
  Grid, pencil marks and elapsed
  time. Pause already covers the walk-away case, so this is the smaller half.
  Layman: Close a puzzle part way and come back to it.
  Kind: implement.
  Lanes: sudoku.
  Resolved (2026-08-25): `SudokuGrid::save`/`load` write the solution, the clues,
  what has been entered and the pencil marks; `SudokuView` adds the level, the
  cursor, pencil mode, the error highlight, the pause state and the elapsed time.

  The time is taken from `elapsedMs()` rather than the banked `m_elapsedMs`, which
  is missing whatever the clock has run since it was last banked -- saving the
  banked figure would quietly give the player back time they had already spent, and
  this game records a best time.

  No pack to check against, so what refuses a board the game could not have reached
  is the puzzle itself: the stored solution has to be a COMPLETED grid -- every row,
  column and box holding each digit once -- every clue has to agree with it, and a
  clue cannot have been written over or carry a mark. Everything else in the save
  is measured against the solution, so if the solution is real the rest cannot be
  nonsense.

  The view restores as suspended with the clock banked, for the same reason Hearts
  does not start its timer: the hub calls activate() straight after, and that is
  what picks the clock up. Starting it here as well charges the player for the
  moment in between.

  Checked in uitest like the others, and the forged-blob check was verified to hit
  the solution test rather than passing by accident -- with that test removed the
  all-ones solution loads and the check goes red.

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

- ✅ [GHUB-0031] **The Windows build rides on an action GitHub is deprecating the runtime under.**
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
  Resolved (2026-08-25): upstream is unmaintained -- v1.13.0 (2024-01) is
  still the latest release and master still declares `node20`, last touched
  2024-03 -- so the bump route does not exist and the bullet's own fallback
  was taken. `scripts/setup-msvc.ps1` does what the action did: ask vswhere
  for the VS install, run its vcvarsall.bat, and export only the variables it
  changed. Shared by ci.yml and release.yml rather than inlined twice, so the
  release cannot drift from what CI proved.

  Verified on the wintest box rather than assumed: 38 variables exported,
  cl.exe reachable on the exported Path, INCLUDE / LIB / LIBPATH /
  VCToolsInstallDir / VSCMD_ARG_TGT_ARCH all present. That probe also
  corrected a claim this session had written down wrongly -- vcvarsall DOES
  put Visual Studio's own ninja.exe on PATH, via
  CommonExtensions\Microsoft\CMake\Ninja, so the spec's original statement
  was right and the "it comes from the runner image" replacement was not.

  One less third-party action holding a foothold in workflows that publish
  binaries strangers download, which is the same reasoning that dropped
  softprops/action-gh-release.

- ✅ [GHUB-0041] **A donate entry in the hub, pointing at the funding links this repo already declares.**
  Asked for 2026-08-19. `.github/FUNDING.yml` was missing from this repo and
  was copied in from the other projects on the same day, so the three links
  now exist in one place and are already the sidebar button on GitHub:
  GitHub Sponsors (milnet01), Patreon (AntsProjectsHub) and the PayBru tip
  page.

  The links live in FUNDING.yml and the code must not restate them by hand.
  A hardcoded URL in a .cpp is a fourth copy that drifts the day one of the
  three changes, and nothing would catch it. Two honest options and the
  choice is not made: read the YAML at build time and generate the entries,
  or list them once in a small table with a check that fails if it and
  FUNDING.yml disagree. Prefer whichever is smaller.

  Prior art to copy rather than invent: Ants_Terminal's mainwindow.cpp has
  a Donate submenu that opens each link with QDesktopServices::openUrl and
  says so in the status bar. The mechanism transfers; the status-bar part
  does not, because GHUB-0040 is on file precisely for that reason.

  Where it goes is open. The hub has a tile grid and a toolbar, and neither
  obviously owns it — a Help menu entry is the conventional answer, a small
  tile is the discoverable one, and a tile costs a slot in a grid that is
  already fourteen games wide. Worth asking rather than deciding.

  Legibility is the same constraint as everywhere else here: whatever it
  is, it says in words what it does and where it will take you before it
  opens a browser, rather than showing a bare icon.
  Decided 2026-08-19 by the owner, closing the two open questions above.
  It lives in the Help menu — so the tile grid keeps all fourteen of its
  slots for games, and the entry is where a stranger already looks for
  "about this program".

  And it asks. Every 150th launch of the app shows a popup inviting a
  donation. That needs a launch counter in QSettings beside the other
  persisted preferences, incremented once per process start rather than
  per game opened, so a session that plays six games still counts as one.
  150 is roughly a prompt every few months at a game a day, which is the
  point: rare enough not to be a nag.

  Three things the popup owes, none of them decided yet and none of them
  expensive. It says what it is before it says what it wants, in words
  large enough to read (the legibility switch applies to it like anything
  else). It offers a way to stop being asked, because a popup that cannot
  be turned off is one the player learns to dismiss without reading, which
  loses the request as well as the goodwill. And it never appears over a
  game in progress — a startup prompt is an interruption, but an
  interruption at the hub is a different thing from one on your turn.

  Worth testing rather than assuming: the counter must survive the app
  being killed rather than closed, and 149 → 150 → 151 must show the popup
  exactly once. An off-by-one here shows the prompt every launch, which is
  the failure everyone remembers.
  Resolved 2026-08-20. The Help menu is new — the hub had no menu bar at
  all — and carries one entry, Support this project…, which opens a
  dialog rather than a browser. The dialog says what it is before what it
  wants, then offers one named button per link with the address spelled
  out underneath, so the destination is readable before the browser opens
  rather than after. The legibility switch reaches it: it reads the
  setting at construction and enlarges its own font, which is enough for
  something that lives a few seconds.

  The link question was settled by generating rather than checking.
  CMakeLists.txt reads .github/FUNDING.yml at configure time and writes
  src/funding.h.in out to funding.h, so there is no second copy to drift
  and nothing to keep in step. A funding key the loop has no rule for
  STOPS THE BUILD, which is the property that matters: a silently dropped
  link looks exactly like a working build. Proved by adding
  `ko_fi: antsprojects` to the YAML and watching cmake refuse. A uitest
  check counts the non-comment keys back out of the file and demands the
  same number of entries, as a second guard on the same thing.

  The prompt is every 150th launch, counted per process in QSettings
  (`donate/launches`) and written as it is read, so a killed process still
  counted. `donate::launchOwesPrompt` is pure, which is what makes the
  off-by-one everyone remembers checkable without touching stored
  settings — 149 does not ask, 150 asks, 151 does not ask again, and 600
  launches owe exactly four prompts. The prompt carries a "Keep asking me
  now and then" switch, stored as it is toggled rather than on accept, so
  closing with the window button honours the choice. It never appears over
  a game: `--game` goes straight into play, and that launch is skipped. It
  is also deferred to the event loop so the window is up and painted
  first.

  17 new UI checks, each proved able to fail by breaking the code it
  guards. uitest 168/168, ctest 3/3, `scripts/local-ci.sh` green.
  **Layman:** A way to support the project from inside the game, using the same donation links the repository already lists.
  Kind: feature.
  Source: user-request-2026-08-19.

- 📋 [GHUB-0043] **The app tells you a new version exists, shows what changed, and installs it for you.**
  A published AppImage today is a file someone downloaded once. There is no
  route from that copy to the next one except noticing the releases page, so
  every player is frozen at whichever version they happened to fetch.

  The shape is finbreak's (FIBR-0054 / FIBR-0131), read across on 2026-08-20
  and adapted: a check against the GitHub releases API, an offer carrying the
  accumulated release notes for every version between the installed one and
  the latest, and on Update now a download, an Ed25519 signature check and an
  in-place swap of $APPIMAGE followed by a detached relaunch. Four decisions
  were taken with the owner before any of it was written:

    - Linux AppImage installs itself; Windows offers the download page. The
      Windows artifact is a portable zip rather than one .exe, so installing
      means replacing a whole directory. That is a separate item, not a wider
      version of this one.
    - Releases are signed. A public-domain Ed25519 verifier ships in-tree so
      this needs no new library, the private key never enters the repo, and a
      download that does not verify is deleted rather than installed.
    - The first launch asks whether to check automatically. Nothing reaches
      the network before that answer, and Help -> Check for updates works
      whichever way it was answered.
    - One check a day, not one a launch. A hub opened five times an evening
      should cost one network call.

  Inert wherever it cannot work -- a cmake --install copy, a distro package, a
  build directory -- because an updater that overwrites a file it does not own
  is worse than no updater. That is the same detect_installer() seam finbreak
  uses, and it is what keeps the OBS and Flathub builds below free of any
  outbound surface at all.

  The traps finbreak paid for and this must not re-learn: a fresh AppImage
  started before the old one has finished tearing down collides with the still
  mounted image and dies, which reads to a player as "it closed and never came
  back"; and the relaunch has to wait for the old process rather than assume
  it.
  Note (2026-08-20, found while filing the Security section): shipping this
  falsifies the first line of SECURITY.md, which currently reads "**No
  network.** Nothing in the app opens a socket, fetches a URL or phones home.
  There is no telemetry and no update check." That paragraph is what tells a
  reporter which findings matter, so it has to be rewritten in the same change
  rather than swept afterwards — the new text owes the reader what is fetched,
  from where, on whose consent, and how the download is verified. GHUB-0054
  extends this item's signing key to the artifacts a person downloads by hand,
  which is the other half of the same story.
  **Layman:** Games Hub checks GitHub for a newer release, shows the changelog for every version you have missed, and updates itself when you say yes.
  Kind: feature.
  Source: user-request-2026-08-20.

- 📋 [GHUB-0044] **Native packages on the openSUSE Build Service, for as many distributions as it will build for.**
  An AppImage is a file you have to find, download and mark executable. A
  package is one line in a terminal or one click in a software centre, and it
  is how most Linux users expect to get software.

  finbreak already publishes this way from home:milnet on build.opensuse.org
  (FIBR-0155), so the account, the layout and the submit scripts exist; this
  is a sibling subproject beside home:milnet:finbreak and
  home:milnet:ants-terminal. Its recipes are far simpler than finbreak's,
  and that is the point worth writing down: finbreak vendors a wheel closure
  and ships a frozen Python runtime, while this is a C++ CMake project that
  builds from source against the distribution's own Qt 6. No vendoring, no
  bundling decision, no offline-build service -- an RPM spec, a debian/
  recipe, an OBS _service that fetches the tagged tarball and sets the
  version, and the reverse-DNS .desktop and AppStream metainfo files that a
  software centre reads.

  Targets follow finbreak's repository list as far as their Qt allows:
  openSUSE Tumbleweed, Fedora, Debian and Ubuntu. Each one is a build that
  either goes green or does not, so "as many as possible" is answered by
  trying them rather than by predicting -- but a distribution shipping a Qt
  older than the 6.5 CMakeLists.txt requires is a deferral with a reason
  rather than a failure, the way Leap 15.6 was deferred for finbreak.

  The app-ID is fixed at this step and every later packaging step inherits it,
  Flathub included.
  **Layman:** Install Games Hub with your distribution's own package manager instead of downloading a file.
  Kind: package.
  Source: user-request-2026-08-20.

- 📋 [GHUB-0045] **On Flathub, so the software centre finds it.**
  The distribution-agnostic half of the item above. OBS reaches users who
  install by package manager; Flathub reaches everyone else, and it is the
  one listing that puts the collection in front of somebody who was not
  looking for it.

  finbreak's manifest (FIBR-0159) is the model, and again this is the easier
  case. finbreak builds on org.freedesktop.Platform carrying its own pinned
  PySide6 wheel closure, because a finance app will not take a substituted
  Qt; a C++ Qt Widgets game hub builds on org.kde.Platform, which ships Qt 6
  already, so the manifest is a cmake module against a tagged release and
  little else.

  The sandbox is where the thinking goes, and it points the other way from
  finbreak's. That app's permission list is deliberately empty of network and
  filesystem because it holds bank statements; this one needs a display, GPU
  acceleration and -- unlike finbreak -- sound, and it stores nothing but
  QSettings. Whether it gets --share=network at all is the real question:
  without it the auto-update above is unreachable inside Flatpak, which is
  the correct answer, since Flatpak updates itself and an app that
  overwrites its own runtime inside a sandbox is fighting the packaging.
  So the updater must detect a Flatpak the same way it detects a distro
  package, and stay inert.

  Carries the same app-ID, .desktop and metainfo as the OBS work, which is
  why that item fixes them and this one reuses them. A screenshot set and a
  summary that reads well in a software centre are part of the deliverable
  rather than an afterthought -- this is a shop window.
  **Layman:** Games Hub appears in GNOME Software, KDE Discover and flathub.org like any other app.
  Kind: package.
  Source: user-request-2026-08-20.

- 💭 [GHUB-0076] **A release candidate cannot be tagged at all — the verify job rejects the suffix.**
  release.yml's verify job compares the WHOLE tag against
  CMakeLists.txt:

    tag="${GITHUB_REF_NAME#v}"
    cmake_version=$(sed -n 's/^project(gameshub VERSION \([0-9.]*\).*/\1/p' ...)
    if [ "$tag" != "$cmake_version" ]; then exit 1

  Measured 2026-08-20: a v0.5.0-rc.1 tag gives tag=0.5.0-rc.1 against
  cmake_version=0.5.0, so the job exits 1 before any build starts.

  The global standard (~/.claude/standards/versioning.md § 5) puts the
  -rc.N suffix on the tag and never in a source file, which is right --
  and on this project leaves no route at all, because the mismatch is
  on the tag string the workflow reads back rather than on anything a
  recipe writes. cut-release cannot help either: it validates its
  target against ^[0-9]+\.[0-9]+\.[0-9]+$.

  The fix is in the verify job: split the tag into its triple and its
  optional suffix, compare the triple against CMakeLists.txt and the
  CHANGELOG heading, and use the presence of a suffix to decide
  prerelease vs latest when publishing. The changelog heading carries
  the plain triple per global § 5, so the grep must use the triple
  rather than the whole tag.

  Not urgent -- nothing here has ever needed a candidate build. Worth
  doing before the first release that wants testers, and worth knowing
  about before someone discovers it from a red workflow.
  Not wanted (2026-08-20). Owner's call: this project has no
  testers, so a candidate build is a release cycle bought for
  nothing. docs/standards/versioning-overrides.md § 3 records the
  decision.

  Kept as considered rather than deleted because the measurement
  above is the expensive half and should not have to be redone. If
  candidates are ever wanted, the work is three places in
  release.yml, not one: both verify checks compare the whole tag
  (against CMakeLists.txt, and again in the CHANGELOG grep), and
  gh release create carries no --prerelease at all, so a candidate
  would be rejected twice and then published as latest.

  Note the second and third of those were found by the cold gate on
  the standard, not by the original measurement -- the first draft
  said "one check" and would have sent an implementer to fix a
  third of the problem.
  **Layman:** There is no way to publish a test build for people to try before the real release.
  Kind: fix.
  Source: in-session-2026-08-20 (docs/standards/versioning-overrides.md § 3).

### ⚡ Performance

Nothing in this collection is slow in the way a spreadsheet is slow. The cost

shows up in the two places a game can afford it least: a window that stops

answering while the computer thinks, and a laptop fan that comes on and stays

on.

Every item here was found by reading the source on 2026-08-20, and the first one

carries a measurement rather than an opinion. The last one exists because three

of the four cannot be proved fixed with anything this project currently owns —

which is the reason it is filed rather than an excuse for not filing the rest.

- ✅ [GHUB-0046] **A game you have left keeps playing itself, and Pinball costs a fifth of a CPU core to do it.**
  Measured rather than suspected. A hub sitting on the Pinball page used 1120 ms
  of CPU in a five-second window -- 22% of one core -- with the ball parked and
  nobody touching it. The tile grid over the same window used 0 ms. The table
  steps every 16 ms whether or not anyone is watching.

  The hub already has the seam that fixes this. `openGame()` calls `deactivate()`
  on the page being left and `activate()` on the one arriving, and Minesweeper,
  Sudoku and Canasta all override `deactivate()` -- Minesweeper's even banks its
  clock so the timer resumes where it was left rather than where it would have
  got to unattended. **Pinball, Snake and Hearts own a `QTimer` and override
  nothing.** Pinball's timer stops only at `gameOver()`, so once the page has
  been opened the cost follows you into every other game until the app closes.

  It is not only CPU, and the other two symptoms are worse. `PinballView::tick()`
  plays the bumper, slingshot and drain effects, so an invisible table makes
  noises over the game you are actually playing. And the simulation keeps running,
  so the ball drains and the balls are spent while you are elsewhere -- the same
  for Snake, which keeps moving and dies unattended. A player who ducks out to
  the tile grid mid-ball comes back to a game that played itself badly.

  The work is three `deactivate()` overrides following the pattern already in the
  tree, plus the test that stops this recurring: every registered game gets
  activated, deactivated, and asserted quiet. Written as a property of the whole
  registry rather than three named games, a fifteenth game with a timer is caught
  the day it lands rather than the day someone measures again.
  Shipped (2026-08-20) as GHUB-0073 (Pinball) and GHUB-0074 (Snake
  and Hearts) — all three deactivate() overrides this bullet names,
  plus gamesStopTheirClocks, which is the registry-wide test it asks
  for: every game the hub can open is activated, deactivated and
  asserted to have no QTimer still running, so a fifteenth game with a
  clock is caught the day it lands.

  Recorded plainly because it is a process failure worth not
  repeating: this bullet already existed, with the diagnosis, the three
  named games and the shape of the test, and the work was done without
  finding it. It was reached the long way round — a cold-review lane
  asked whether the new stop-on-leave assertion really reached only
  Pinball — and two duplicate items were filed before anyone looked
  here. The roadmap is the first place to check when a defect turns
  up, not the last.

  What is NOT covered by those two, and stays with this bullet's
  neighbours: the CPU measurement is not re-taken. This bullet's 1120
  ms per five seconds was measured with the ball parked on an open
  Pinball page; stopping the clock on leave removes the cost of
  carrying Pinball into other games, which is what the measurement was
  about, but nothing here re-measures it. GHUB-0049 owns the fact that
  nothing in the project can measure a frame at all.
  **Layman:** Open Pinball once and the fan stays on for the rest of the session, even while you are playing Chess.
  Kind: fix.
  Source: in-session-2026-08-20.

- ✅ [GHUB-0047] **The computer thinks on the drawing thread, so the window stops answering while it does.**
  There is no `QThread` and no `QtConcurrent` anywhere in `src/`. Every engine
  runs inside the signal handler that started it, on the thread that also paints
  and handles input, so the window is genuinely frozen for the duration: no
  repaint, no resize, no menu, and on some desktops a "not responding" prompt.

  CLAUDE.md already records the consequence for Chess -- Hard's worst observed
  middlegame answer is about 1.2 seconds -- and records the workaround, which is
  that `planFor()` hands each level a node budget rather than a depth, because
  depth alone would freeze the window. **That budget is currently doing two jobs:
  choosing how strong the engine is, and keeping the UI alive.** Only the first
  is a game-design decision. Reversi at depth 6 and Draughts sit behind the same
  constraint.

  The architecture is already most of the way there, and this is the argument for
  doing it here rather than living with the freeze. The rule that a rules core
  never includes a widget means every engine is Qt-free or QtCore-only, takes a
  value-type position, and returns a move -- which is exactly the shape that moves
  to a worker thread without redesign. `Board` is copied per search node by
  design; nothing is shared to race over.

  Two things the item must not lose. `ChessView::advance()` is the single point
  that moves the game on, and a threaded reply must route back through it rather
  than becoming a second path -- CLAUDE.md names that trap for Reversi in the same
  words. And a search that is still running when the player starts a new game,
  switches level, or leaves the page has to be abandoned rather than awaited; that
  cancellation, not the threading, is where this kind of change usually goes
  wrong.

  The visible win is a window that stays alive and a "thinking" indicator that can
  actually animate. The strength win is separate and optional: with the UI no
  longer hostage to it, the node budget can be reconsidered on merit.
  Resolved (2026-08-28): Chess, Reversi and Draughts now search on a
  worker thread. Measured on a middlegame position at Hard: the window
  went from 420ms with no response, in one stretch, to 8ms — which is
  what an idle window gives.

  A prerequisite the bullet did not mention, and it would have been a
  real bug. All three engines pick between equally-good moves with a
  function-local `static std::mt19937`. C++ guarantees thread-safe
  INITIALISATION of one and says nothing about using it, and an
  abandoned search keeps running while its replacement starts, so two
  threads can be inside that function at once. They are `thread_local`
  now. Per-thread seeding costs nothing: it only breaks ties.

  The cancellation is by generation, as the bullet asked. Every search
  carries the number the game held when it set off; an answer whose
  number no longer matches is dropped. Bumped by a new game, an undo, a
  level change, a restore and the hub leaving the page. The worker is
  left to finish rather than made to poll a flag — it holds only copies,
  it cannot reach the game, and stopping it early buys the player
  nothing.

  Abandoning what is in flight turned out NOT to be enough on its own.
  advance() schedules the search on a short timer, so one that had been
  SCHEDULED before the hub left would still set off afterwards and play
  a move on a board nobody was looking at — GHUB-0073's fault in a new
  form. m_paused closes it, and activate() picks the thinking back up so
  the game is never left stuck on the computer's turn.

  Undo changed behaviour deliberately. All three refused while the
  engine was thinking, which was free when the window was frozen and the
  player could not press it. Now they can, and waiting for a search
  whose board is about to be discarded is the one thing the bullet said
  not to do — so it abandons instead.

  ChessView::advance() is still the single point that moves the game on;
  the answer routes back through it rather than becoming a second path.

  Verified. The freeze test measures the LONGEST GAP between heartbeat
  ticks, not how many there were — counting was the first attempt and it
  PASSED on the unfixed build (50 ticks against an idle 60), because one
  100ms freeze inside a 300ms window still leaves most ticks standing.
  Red/green with the gap metric: 420ms unfixed, 8ms fixed. A second
  weakness was caught by the sanitizer build, where the same search takes
  17.6s: the check that the move actually gets played waited a fixed
  4000ms and failed. It waits for the outcome now, which is the project's
  own rule about asserting the code rather than the machine.

  Lifetime is the other risk and it is covered: six rounds that abandon a
  running search by every route and then destroy the view outright, run
  under ASan+UBSan with no error.

  ThreadSanitizer was tried and is NOT usable here, so it is not being
  claimed as evidence either way. Against a system Qt built without
  instrumentation it reports thousands of races entirely inside Qt's own
  allocator between Qt's own pooled threads — including on paths this
  change never touched, and Qt was already spawning pool threads before
  it. What is worth recording: not one report names an engine core, which
  is the code that actually runs on the worker.

  Not done: the node budget still does two jobs, choosing strength and
  bounding the wait. The bullet calls reconsidering it separate and
  optional, and it is a game-design decision rather than this one.

  Windows unverified locally — the wintest box was powered down. CI is
  the check, and this adds a Qt component (Concurrent) as well as
  threading, so both legs are worth watching. ctest 6/6, local CI green.
  **Layman:** When the computer is working out its move the whole window freezes -- it cannot even be resized until it finishes.
  Kind: perf.
  Source: in-session-2026-08-20.

- ✅ [GHUB-0048] **Every card is drawn from scratch, every frame, and nothing is ever cached.**
  There is no `QPixmap` and no `QPixmapCache` in the entire source tree. Every
  pixel is painted by `QPainter` on every paint event.

  What one card costs, from `CardArt::paintFace` and `Theme::paintDropShadow`:
  three stacked translucent rounded rectangles for the shadow, a rounded-rect
  path, a linear-gradient fill, a stroked outline, then the corner index twice
  -- once rotated 180 degrees -- with four `QFont` mutations between them, and
  finally up to ten pips. All antialiased. Canasta drives a 16 ms timer during
  its card flights and repaints the whole table, so roughly fifty cards pay that
  price sixty times a second while a single card is in the air. The fourteen hub
  tile miniatures are the same story on a smaller scale: each repaints its whole
  illustration on hover in and hover out.

  A face is a pure function of rank, suit, width and the legibility state. It is
  the textbook cache, and the drawing code is already isolated behind
  `cards/cardart.*` so the change has one home.

  Three traps, and the first is a hard requirement rather than a refinement. **A
  cached pixmap must carry its device pixel ratio** or every card goes soft on a
  HiDPI screen -- for a partially sighted player that trades a cost he cannot see
  for a blur he can, which is a straight loss however fast it runs. **The
  legibility switch belongs in the cache key**, since it is exactly what changes
  the size a card is drawn at, and Canasta already draws melds at 0.74 and
  opponents' hands at 0.8 of the base width -- three live sizes per card in one
  view. And **`CardArt::kFaceMinWidth` still decides what gets drawn**: below 46
  pixels there is no face to cache, only a corner index.

  The second half of the same problem, and deliberately second: every `update()`
  in the tree is the whole-widget form. A card crossing the table invalidates the
  forty that did not move. `update(QRect)` over the union of where a flight was
  and where it now is would cut that, but it is fiddly and easy to get subtly
  wrong -- a missed rectangle leaves smears on screen. Do the cache first, measure
  again, and only reach for this if the number still justifies it.
  Resolved (2026-08-25): the owner reported Canasta slowing down during play, and
  this was it. Measured with the new `--bench` (GHUB-0049), a RESTING Canasta table
  cost 24.10 ms a frame against its own 16 ms timer -- so the game could never meet
  its clock, and it got worse as the hand filled up. Now 8.87 ms. Klondike 20.42 ->
  4.09, Canasta mid-deal 7.92 -> 3.27, the hub grid unchanged at ~2.3.

  What shipped is a pixmap cache for card BACKS only, in cardart.cpp, keyed on size,
  deck and the scale the card actually lands on screen at. perf put `CardArt::paintBack`
  an order above `paintFace` and the reason is in the code: a back is ~50 antialiased
  lines clipped to a rounded path, and Canasta keeps three fanned hands of backs plus
  the stock on the table at all times.

  FACES ARE DELIBERATELY NOT CACHED, and that is the finding rather than a shortcut.
  A cached pixmap under the rotation of a fanned hand is resampled and goes slightly
  soft; on a back there is nothing to read, on a face there is, and this game is read
  by pip pattern. The bullet's own first trap says the same thing about HiDPI. Do not
  "finish the job" without measuring what it does to a face at the smallest scale the
  game draws one.

  The second half -- update(QRect) instead of whole-widget update() -- is answered
  rather than deferred. The bullet said to do the cache first, measure again, and
  only reach for it if the number still justified it. At 8.87 ms against a 16 ms
  budget it does not.

  Two things worth keeping. The shadow padding inside the cache must be a WHOLE pixel:
  with a fractional one the card sits at a fractional offset inside the pixmap and
  every lattice line antialiases differently -- 542 pixels of one back moved by more
  than 8 levels. And byte-identity was never available: the ORIGINAL code draws the
  same back with a max channel difference of 35 when moved thirteen WHOLE pixels
  sideways, so the lattice rasterisation is position-dependent on its own. The cache's
  residual is that same effect, not a new defect -- measured with a scratch probe, both
  at max 35.

  Also a method note for the next visual change: `--shot` CANNOT compare two card
  games. Two runs of the same build differ in 101,741 of 740,000 pixels because the
  deal is random. That is GHUB-0093, and it cost two wrong readings here before it was
  spotted.
  Correction (2026-08-25, same day): the note above says faces are deliberately not
  cached. That was true when it was written and is no longer. Read the note below
  instead; the reasoning behind it stands, and only its conclusion changed.

  The measurement that changed it: a sweep of all fourteen games -- which the note
  above had not taken -- put FreeCell at 18.81 ms, the SLOWEST board in the
  collection and worse than Canasta ever was, with Pyramid at 10.50. Both deal
  their whole layout face UP, so no back cache reaches them.

  What the sweep also showed is that the quality argument and the speed argument
  were never actually in conflict. Resampling only happens under a ROTATION. Every
  solitaire lays its cards out square, so a face cache reaches FreeCell, Pyramid,
  Klondike and Spider through the pixel-exact path and is not resampled at all. So
  faces are now cached when the card is unrotated, and still drawn live when it is
  fanned -- Canasta's and Hearts' hands.

  Measured with the scratch probe, against the build without the face cache: six
  unrotated faces came back with a max channel difference of 2 out of 255 and not
  one pixel over 8, and the fanned face was byte-identical, which is what "drawn
  live" has to mean.

  Fourteen games, before -> after in ms/frame: FreeCell 18.81 -> 3.15, Pyramid
  10.50 -> 2.66, Canasta 9.64 -> 7.00 (at rest 24.10 -> 7.70), Hearts 5.80 -> 1.87,
  Solitaire 5.70 -> 2.78, Spider 5.24 -> 2.01, Draughts 4.94 -> 3.95, Chess 4.46 ->
  3.39, Pinball 3.94 -> 3.47, Reversi 3.03 -> 2.48, Minesweeper 2.90 -> 2.42,
  Sudoku 2.16 -> 1.83, Snake 1.86 -> 1.57, 2048 1.30 -> 1.02. The non-card games
  draw no cards and their movement is run-to-run noise -- do not read it as a win.

  One defect found and fixed on the way, and it is the reason for the new
  cardArtKeyDecidesThePicture check: the cache key rounded a card's size to whole
  device pixels while the DRAWING used the exact size, so two rects a fraction of a
  pixel apart shared one entry and whichever drew first decided the picture. A
  frame drawn after an eviction then differed from the same frame before one. It
  surfaced as FreeCell failing the existing legibility reversibility check, and
  only because enough games had run first to fill the cache -- luck rather than a
  guard. Both key and content now derive from the snapped size.

  That new check was itself wrong twice before it was kept, both times passing
  against the defect it was written for, and both times caught by reintroducing the
  defect and watching it stay green. First it drew the subject at an integer width,
  where the rounding it was testing does nothing. Then its cache-emptying loop tied
  `deck` to `i % 2` while the width was `30 + (i % 60)` -- 60 is even, so every
  width got exactly one deck, 60 distinct back keys against a 64-entry bound, and
  the back cache never emptied at all. A test that has never been seen to fail is
  not evidence.
  **Layman:** The card games redraw all fifty-odd cards sixty times a second, even the ones sitting still.
  Kind: perf.
  Source: in-session-2026-08-20.

- ✅ [GHUB-0049] **Nothing in the project can measure a frame, so no painting fix can be proved.**
  The three items above were found by reading source, and only the first carries a
  number -- taken by hand from `/proc` against a running process on 2026-08-20,
  under the offscreen platform, and not repeatable by anything in the repository.
  That is the gap. `gameshub_selftest` proves rules and `gameshub_uitest` proves
  widgets, and neither has ever asked how long anything takes.

  This project already knows what to do when a number matters, and does it well.
  Chess move generation is proved by perft against published totals rather than by
  eyeballing. Canasta's four AI levels are played against each other rather than
  described, which is how Hard was caught being weaker than Medium. Sudoku's mark
  font is solved against measured ink and the test loops over the machine's own
  font families. The same instinct has simply never been pointed at frame cost.

  The smallest useful version: a check that renders a view into a `QPixmap` a
  fixed number of times and reports the milliseconds -- the `render()` trick the
  UI tests already use to force `paintEvent` through, with a clock around it.
  Canasta mid-flight, a full Klondike tableau and the fourteen-tile grid are the
  three that would say the most.

  **Report the number; do not assert a threshold.** CLAUDE.md's own rule from the
  three red Windows runs is that a test may assert what the code does and must
  only report what the platform happens to provide, and a frame time is a property
  of the machine, the compositor and the graphics stack -- exactly the kind of
  constant that passes here and reddens on `windows-2022`. A printed figure that a
  human compares before and after is worth having; a hardcoded budget is a red CI
  leg waiting for a slow runner.

  One thing it can assert honestly, because it is a property of the code rather
  than the machine: a view that has been deactivated does no work at all. That is
  the permanent guard for the first item in this section.
  Resolved (2026-08-25): `frameCost()` in tests/uitest.cpp, runnable alone as
  `gameshub_uitest --bench`. Four subjects rather than the three suggested --
  Canasta mid-deal, Canasta at rest, a full Klondike tableau and the fourteen-tile
  grid. Canasta at rest was added after the fact and is the one that mattered: it
  costs THREE TIMES what mid-deal does, which is the opposite of the guess, because
  a resting table has every card drawn where a dealing one still has most of them
  in the stock.

  Reports and does not assert, as the bullet asked. The one assertion is the one it
  named -- a deactivated view does no work -- and it ships with a positive control,
  because a paint counter that never fires under the offscreen platform would pass
  the deactivated half for free and prove nothing.

  It paid for itself inside the hour: GHUB-0048 was measured, fixed and re-measured
  against it, and the first fix attempt was caught making the picture worse.
  **Layman:** There is no way to check whether a speed-up actually sped anything up.
  Kind: test.
  Source: in-session-2026-08-20.

### 🔒 Security

Start with what is already right, because it decides which of these matter.

SECURITY.md is accurate: no network, no accounts, no personal data, and the only

input the app parses that it did not write is a saved game. That parsing is

genuinely careful — every count is capped before it reaches a `reserve`, every

field is range-checked before it becomes a `Card`, and Chess replays its move

list through its own generator rather than trusting the file. Nothing below is a

known hole.

What is missing is not defence. It is PROOF and PROVENANCE. Ten hand-audited

parsers stay correct only until someone edits one; a release built by a workflow

that pulls an unpinned binary is only as trustworthy as that binary was this

morning; and a stranger who downloads an AppImage today has no way to check they

got what the workflow built. Those are the gaps, and they are worth more here

than any amount of hardening applied to an app with no sockets.

- 📋 [GHUB-0050] **Every action is pinned to a commit, and then the release downloads two unpinned binaries and runs them.**
  `release.yml`'s Fetch linuxdeploy step pulls
  `linuxdeploy-x86_64.AppImage` and `linuxdeploy-plugin-qt-x86_64.AppImage` from
  the `continuous` release tag, `chmod +x`es them, and hands them the AppDir.
  Those two binaries produce the artifact that strangers download. There is no
  checksum, no signature and no pin — `continuous` is a tag that moves by design,
  so the workflow does not build the same thing twice and cannot tell if it did.

  This is not a general worry, it is this project's own stated rule going
  unapplied twenty lines away. CLAUDE.md § Releasing says every action is pinned
  to a commit SHA with the version in a trailing comment, and gives the reason
  verbatim: *these workflows publish binaries that strangers download, and a moved
  tag on a third-party action would run arbitrary code against them.* Both
  sentences are true of a curled AppImage; only the actions got the treatment.

  linuxdeploy publishes no stable release tags, which is presumably why it was
  fetched this way, so the fix is a recorded SHA-256 per binary and a
  `sha256sum -c` before either is made executable. That turns an upstream change
  into a failed build with an obvious diff, which is exactly what a SHA pin buys
  for an action. Refreshing the digest becomes a deliberate commit — the moment to
  look at what changed — rather than something that happens silently on the next
  release.

  The check belongs in `zizmor`'s territory conceptually but no linter will catch
  it, because it is a `run:` block rather than a `uses:` line. That is the whole
  reason it survived: the automated check looks at the field this hole is not in.
  **Layman:** The tool that packages the Linux download is fetched fresh each time from a link that can change, and whatever arrives builds the file people download.
  Kind: security.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0051] **The only check on the workflows that publish binaries runs on one machine, by choice.**
  `actionlint`, `yamllint` and `zizmor` are real and they pass clean. They live in
  `scripts/local-ci.sh`, which runs from the `pre-push` hook — a hook that does
  nothing until someone runs `git config core.hooksPath .githooks` in their clone,
  and that `SKIP_LOCAL_CI=1` is documented to bypass. `ci.yml` runs configure,
  build and `ctest`, and nothing else; neither workflow lints the other.

  So the workflow-security scanner guarding the pipeline that signs nothing and
  publishes everything is opt-in, local, and bypassable — three properties none of
  the project's other gates have. A contributor's pull request is not scanned at
  all, and neither is a push from any machine but this one.

  The fix is small: a lint job in `ci.yml` running the same three tools, so the
  check is a property of the repository rather than of a workstation. It does not
  replace the local run — catching it before the push is still better, and
  `local-ci.sh` deliberately executes the workflow's own steps rather than
  mirroring them, so a lint job added to `ci.yml` is picked up by the local runner
  for free. That is the design working as intended, and it is the argument for
  putting the tools in the workflow rather than beside it.

  While there: `zizmor` has an audit level worth turning up on a repository that
  publishes releases, and the two workflows already do the things it most wants to
  see — `permissions:` scoped per job, `persist-credentials: false` on every
  checkout, and `contents: write` confined to `publish`.
  **Layman:** The safety checks on the release process only run on your PC, and only if you set them up and do not skip them.
  Kind: security.
  Source: in-session-2026-08-20.

- ✅ [GHUB-0052] **Ten hand-audited parsers, and nothing but hands checking them.**
  `restoreState()` has ten implementations, and they are the app's entire untrusted
  input surface — SECURITY.md names it as the one thing parsed that the app did
  not write. The code is good. `cardcodec::readPile` caps its count against
  `kMaxPileSize` before it reserves; `readCard` range-checks suit, rank and deck
  before any of them becomes a `Card`; Chess refuses a version mismatch, caps the
  move list at 1024 plies, and matches every move against `legalMoves()` so the
  file supplies from/to/promotion and the generator supplies the flags; Draughts
  bounds a jump chain at twelve because that is how many men there are.

  Every one of those bounds was put there by a person thinking about it, and every
  one of them is invisible to the test suite. Nothing feeds a malformed blob to
  any parser. The eleventh game, or an edit to the tenth, has nothing to fail
  against.

  This project already knows the answer to this shape of problem. A chess move
  generator is proved by perft rather than by eyeballing, for exactly the reason
  that bugs of this class do not surface reliably by playing. The equivalent here
  is a fuzz harness: take each registered game's `saveState()` output as a seed,
  mutate it — flipped bits, truncations, inflated counts, appended garbage — feed
  it back through `restoreState()`, and require that it never crashes and never
  leaves the game half-loaded. A parser that returns `false` has passed.

  **Build it under AddressSanitizer and UndefinedBehaviorSanitizer or it proves
  almost nothing.** An out-of-bounds read that happens to land in owned memory
  returns a wrong answer quietly and the harness scores it a pass; under ASan the
  same run aborts on the spot. Fuzzing without sanitizers is the version of this
  that looks done. That is the same trap CLAUDE.md already records for the audio
  resource, where a silently empty `.qrc` compiled, linked, and passed everything.

  The cores make this cheap: they are Qt-free or QtCore-only by the architecture
  rule, `gameshub_selftest` links only those cores, and the seeds come from code
  that already exists.
  Resolved (2026-08-28): built, and it found two defects on its first
  two runs — one of them only visible under the sanitizers the bullet
  insisted on.

  Two corrections to the bullet. There are TWELVE parsers, not ten.
  And "the cores make this cheap" is wrong: restoreState() is on the
  VIEWS, so the harness lives in gameshub_uitest and not in the
  self-test the bullet points at. The cores hold restore()/load(),
  which take structured data; the QDataStream parsing is above them.

  `gameshub_uitest --fuzz [rounds]` seeds from each game's own
  saveState(), mutates it the four ways the bullet named plus an empty
  blob and pure noise, and holds two invariants taken from GameView's
  own contract. Refused -> the position must be byte-identical to the
  one held before the attempt, which is that contract's "the game keeps
  the fresh one it already dealt". Accepted -> what it now holds must
  re-save, re-load and survive being painted. Accepting a mutant is not
  a failure and is reported rather than asserted: a flipped bit inside
  a card's rank is usually still a legal position.

  DEFECT 1, plain build, CanastaView::restoreState. It half-loaded on
  refusal, twice over. m_useHouse and m_sortHand were streamed straight
  into the members, so a truncated save could flip WHICH RULE SET IS IN
  FORCE and then report itself corrupt. And m_engine.load() commits on
  success while the tail after it could still fail, leaving the table
  holding the mutant's game. Engine::load itself is clean and says why
  -- "read into a copy, so a stream that runs out part way leaves the
  game that is already on the table alone" -- so the view was breaking
  the rule its own engine documents. Fixed with a pre-flight over the
  whole blob; nothing is written until the last field has parsed.

  DEFECT 2, ONLY under UBSan, canastaengine.cpp. readRules() takes
  `decks` and `jokers` off the untrusted stream with no bound at all,
  and load()'s pack-wholeness check -- the one check that catches a
  file that parsed cleanly but says something impossible -- then
  computes decks * 52. A mutant claiming 905,969,666 decks overflows
  signed int, which is undefined behaviour, and an optimising build is
  entitled to assume it cannot happen. The ordinary build scored that
  mutant a clean pass. Bounded at the read instead.

  Evidence: 96,000 mutants across all twelve parsers under ASan+UBSan,
  clean. Red/green proved by stashing the Canasta fix and re-running --
  HALF-LOADED ON REFUSAL: Canasta with it out, PASS with it in.

  Snake and Pinball are the two games not covered, and correctly so:
  neither overrides saveState, so there is no parser. Getting the other
  twelve covered took a second pass -- the first run fuzzed three games
  and skipped eleven, because a freshly opened game answers "nothing
  worth keeping". nudgeIntoPlay() pokes at the board with clicks,
  drags, select-then-move pairs, double-clicks and keys until something
  is worth saving. It knows no game's rules on purpose, and names any
  game it cannot start rather than passing quietly.

  Wiring, so it stays run: -DGAMESHUB_SANITIZE=ON, a `sanitizers` job
  in ci.yml, and 150 rounds inside the ordinary uitest. ASan runs with
  detect_leaks=0 -- GHUB-0058 predicted the sounds would drown out leak
  checking and it is right, so that stays off until GHUB-0057/0058 are
  closed.

  scripts/local-ci.sh needed two fixes to stay honest about this. It
  read the `build` job ALONE, so a second job would have been invisible
  to the mirror -- exactly the drift it exists to prevent. It now walks
  every job and REFUSES an unknown one. And it applies nothing around a
  `run:` body, so an `env:` block would have run differently here than
  on CI, silently; it now refuses a step carrying one, and the fuzz
  step keeps its settings inline. The sanitizer leg is opt-in locally
  (--with-sanitizers) so the pre-push hook stays quick, and is named in
  "Not checked locally" every run so that default cannot read as
  coverage. Both guards were tested by feeding the script a doctored
  workflow.
  **Layman:** The code that loads a saved game is careful, but nothing tests what happens if a save file is damaged or tampered with.
  Kind: security.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0053] **The build asks the compiler for no help at all — not a warning, not a guard.**
  `CMakeLists.txt` sets `CMAKE_CXX_STANDARD 20` and stops. There is no `-Wall`, no
  `-Wextra`, no `/W4`, no `-Werror`, and no hardening flags anywhere in the tree.
  The CI job configures `-DCMAKE_BUILD_TYPE=Release` and builds; that is the whole
  of what the toolchain is asked to do.

  Two separate things follow, and the first is worth more.

  **Warnings are the cheapest bug detector there is, and this build has none.**
  Uninitialised reads, sign-compare mistakes, a switch that quietly stopped
  covering an enum after a game was added, a shadowed variable — the compiler
  finds all of them for free, and none of them is being asked about. In a
  codebase that has already been bitten by a name colliding with the `slots` macro
  and by `M_PI` not existing on MSVC, the toolchain is demonstrably paying
  attention; it just has not been asked to speak. `-Wall -Wextra` on GCC and
  `/W4` on MSVC, with `-Werror` in CI only, so a local build stays workable while
  the gate stays honest.

  **Hardening flags are the second half and are pure defence in depth.**
  `-D_FORTIFY_SOURCE=3`, `-fstack-protector-strong`, `-fPIE`,
  `-Wl,-z,relro,-z,now` on the Linux build; `/GS`, `/DYNAMICBASE`, `/guard:cf` on
  MSVC. None of them fixes a bug — they decide whether a bug that does exist turns
  into a crash or into something worse, in a binary being handed to people who
  cannot inspect it. A distribution package gets most of this automatically from
  the distro's own build flags, which is one more reason GHUB-0044 is worth doing;
  the AppImage and the portable zip get whatever this file asks for, and today it
  asks for nothing.

  Expect the first `-Wall -Wextra` run to be noisy across twenty-odd view files.
  That is the finding, not an obstacle to it.
  **Layman:** The compiler can spot whole classes of mistake and add cheap protections to the finished program, and this build turns none of it on.
  Kind: security.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0054] **A downloaded release cannot be checked against what the workflow actually built.**
  `release.yml` publishes an AppImage and a zip and nothing else. No checksum
  file, no signature, no build provenance. Someone who downloads either one has
  exactly GitHub's word for it, and no way to notice if a re-uploaded asset,
  a mirror, or a link posted somewhere else hands them something different.

  GHUB-0043 puts an Ed25519 signature on the AppImage, which is the same
  machinery, but it solves a different person's problem: it lets the *app* refuse
  a bad update. It does nothing for the person downloading by hand from the
  releases page, and it does not cover the Windows zip at all. Doing both from one
  key is the sensible shape, and the two items should land in that order — the key
  and the signing step arrive with the updater, and this one extends them to every
  published artifact plus a `SHA256SUMS` file the README can point at.

  The cheaper half is worth taking on its own even if signing slips.
  `actions/attest-build-provenance` gives a signed, publicly verifiable statement
  of which workflow run, which commit and which repository produced each file,
  verifiable with `gh attestation verify`, with no key to generate, guard or lose.
  It is a few lines in the `publish` job and it needs `id-token: write` and
  `attestations: write` scoped to that job only — which the workflow's existing
  per-job permissions block makes natural rather than awkward.

  Both halves want a line in the README saying how to check a download, since a
  checksum nobody is told about protects nobody.
  **Layman:** There is no way for someone to confirm the file they downloaded is the one your build produced and not something altered on the way.
  Kind: security.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0055] **SECURITY.md promises to bump a vulnerable Qt, and nothing anywhere is watching for one.**
  SECURITY.md is straight about this and makes a commitment: the downloads bundle
  Qt 6, a Qt vulnerability is inherited here, and *bumping it is this project's
  job*. The mechanism for noticing is a person reading Qt security announcements.

  Qt is pinned to `6.8.3` in three places — once in `ci.yml`, twice in
  `release.yml`. Every third-party action is pinned to a commit SHA, correctly and
  deliberately. Both kinds of pin have the same property: they are exactly as
  old as the last time somebody looked, and nothing in the repository reports how
  long ago that was. There is no `.github/dependabot.yml`; `.github/` holds
  `FUNDING.yml` and `workflows/` and nothing else.

  Dependabot's `github-actions` ecosystem understands SHA pins and raises a pull
  request that updates the digest and the trailing version comment together, which
  is the maintenance half of a pinning policy this project has already committed
  to — it keeps the pins without letting them rot silently. Qt is not one of its
  ecosystems, so the Qt half needs something else: the honest minimum is a note in
  the release checklist to check the Qt security page against the pinned version,
  and to record the date it was checked. A recorded date is what turns *nobody has
  looked* into a visible fact.

  The three pinned copies of `6.8.3` should also become one value, so a bump
  cannot land in the CI leg and be missed in the two release legs — which would
  publish downloads built against an older Qt than the one the tests ran on, and
  look green throughout.
  **Layman:** The downloads carry their own copy of Qt, and nothing tells you when that copy needs updating for a security fix.
  Kind: security.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0127] **Bound the remaining values a saved game supplies.**
  The CRITICAL and HIGH save defects are fixed. This is the MEDIUM
  remainder, each verified against source by its lane. Hearts adopts
  totals and handPoints with no range check, and its restore count is
  aggregate-only, so a seat can come back with an empty hand and the
  timer then loops with no way out but New Game. 2048 bounds a
  restored score only below, so the next merge overflows, and kMaxTile
  admits tiles well past anything reachable. Minesweeper bounds
  elapsed below but not above and narrows it to int, writing a best
  time nothing can beat; Sudoku's sibling bound is also too loose.
  FreeCell's restore never validates the foundations. Klondike and
  Spider check the pack but not the column invariants their own
  canLift and movableRunLength assume.
  **Layman:** A hand-edited save file can still put silly numbers into a few games.
  Kind: security.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0128] **Close the release pipeline's remaining supply-chain gaps.**
  The unpinned linuxdeploy fetch and the writable smoke-test mount are
  fixed. Still open: neither release build job runs ctest and ci.yml
  cannot trigger on a tag, so a release can publish from a commit whose
  suite never ran on either OS. The changelog check proves the heading
  exists, not that the notes are non-empty. wintest-ci.sh drives a
  recursive remote delete from an unvalidated WINTEST_DIR. And
  local-ci.sh keys STEP_RULES on a step's free-text name, so renaming a
  step in ci.yml silently promotes it from guarded to executed -- which
  would run apt-get from inside a pre-push hook.
  **Layman:** A few smaller risks in the machinery that publishes the downloads.
  Kind: security.
  Source: review-code sweep 2026-08-31.

### 🧠 Memory

Measured before written, on 2026-08-20: the hub sitting on the tile grid holds

55.6 MB resident, opening Canasta adds about 2 MB, and opening Chess adds

nothing measurable. Baseline is Qt itself. There is no leak that grows while you

play and nothing here is urgent — which is the useful finding, and the reason

this section is three items rather than ten.

Three things a memory audit would normally flag are NOT findings here, recorded

so nobody files them later. Raw `new` with a Qt parent is not a leak, it is how

Qt owns objects, and the single `std::unique_ptr` in the tree

(`MinesweeperView::m_field`) is right where a non-QObject needed one. Every undo

history is already capped at 200 snapshots. Every deserialiser already bounds

its counts before it allocates. What is left is churn, one real leak that is

small in bytes and awkward in timing, and a growth curve that is fine at

fourteen games and worth watching at thirty.

- ✅ [GHUB-0056] **Undo copies the whole table on every move, and copies it again to undo one.**
  Four solitaires keep undo as a stack of snapshots, and a snapshot is the entire
  table. FreeCell's is the widest: eight columns, four free cells and four
  foundations, so sixteen separate `std::vector<Card>` copies -- sixteen heap
  allocations -- every time `pushUndo()` runs, which is every move. Spider's is
  eleven vectors over a two-deck game, Klondike's thirteen, Pyramid's three.

  **The cheap fix first, because it is real and it is one line.** `undo()` reads
  `const Snapshot s = m_history.back();` and then pops -- a full deep copy of all
  sixteen vectors, taken from a snapshot that is about to be destroyed. A
  `std::move` out of the back before the pop does the same job with no allocation
  at all. Four files, same shape in each.

  **The structure is second, and smaller than it looks.** `m_history` is a
  `std::vector` used as a queue: past 200 entries every push runs
  `m_history.erase(m_history.begin())`, shifting 199 snapshots down. Worth being
  precise about the cost rather than alarming about it -- `erase` move-assigns
  rather than copies, and a moved `std::vector` is a pointer steal, so this is
  thousands of cheap pointer operations and not thousands of allocations. A
  `std::deque` with `pop_front()` is the right container and makes it free, but it
  is tidiness, not a fire.

  **The architectural option, recorded and deliberately not chosen here.** Chess
  stores the moves that made the game rather than the position it reached, and
  CLAUDE.md states that as the preference for any game offering the choice. Undo
  by replaying moves would drop the per-move allocation entirely. It is also a
  rewrite of four games' undo for a cost nobody has felt, so it belongs in this
  bullet as context for whoever touches that code next -- not as the work.
  Resolved (2026-08-28): done, and the bullet had its two halves the
  wrong way round. Measured with a scratch probe over 40,000 Klondike
  moves, before and after, three runs each.

  The "cheap fix" it named is not in the code. All four solitaires
  already read `const Snapshot& s = m_history.back();` — a reference,
  not the value copy the bullet quotes. The waste is one line further
  in: each member is COPY-assigned from a snapshot destroyed on the
  very next line. Moving out of the back before popping fixes the same
  thing the bullet wanted fixed, at a different line. Worth ~15% of
  undo churn (7.3 -> 6.1 ms per 40,000), which is marginal.

  The structural half the bullet called "tidiness, not a fire" is the
  whole win: 110 ms -> 5.3 ms per 40,000 pushes, a 20x reduction.
  The bullet reasoned that `erase(begin())` move-assigns and a moved
  vector is a pointer steal, so the shift would be cheap. It is cheap
  per element and there are 2,600 of them per push once the history is
  full (thirteen vectors, 200 snapshots), and that dominates the push
  itself. `std::deque` with `pop_front()` makes it free. Klondike,
  FreeCell and Spider only — Pyramid has NO cap, so it never runs the
  eviction path and its vector is left alone.

  Nobody would have felt either: 2.75us -> 0.13us per move. The bullet
  said as much and it is still true.

  Not done, deliberately: undo by replaying moves. The bullet records
  it as context rather than the work, and this change makes the case
  for it weaker rather than stronger.

  Pyramid's history has no cap at all and grows for the life of a deal.
  Bounded in practice by 28 cards plus redeals, so it is noted here
  rather than filed.

  Coverage was already there and is what proves the move is safe:
  klondikeSurvivesRandomPlay and the Spider equivalent play 60 games
  each, undoing on about a fifth of moves and asserting the pack is
  whole after every one; FreeCell and Pyramid assert the table comes
  back exactly. ctest 6/6.
  **Layman:** Each move in the card games saves a full snapshot of every pile; taking a move back copies it all a second time for no reason.
  Kind: perf.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0057] **A game you have opened is never freed, which is fine at fourteen and worth watching at thirty.**
  Games are built on first open and live for the session by design -- the lazy
  construction is deliberate and good, and `hubwindow.cpp` contains no `delete`,
  no `deleteLater` and no `removeWidget`. Nothing is ever taken back down.

  Measured rather than feared. The tile grid holds 55.6 MB resident; opening
  Canasta -- the heaviest game here by a distance, 2265 lines of view over a
  108-card two-pack table -- takes it to 57.6 MB. Chess measured no increase at
  all. So the worst case today is a couple of megabytes per game against a Qt
  baseline of fifty-five, and a player who opens all fourteen is still nowhere
  near a number anyone would notice. **This item is filed as a thing to know, not
  a thing to fix.**

  Two reasons it is worth a bullet anyway. It is the mechanism behind GHUB-0046:
  the reason a Pinball table can keep simulating while you play Chess is that the
  Pinball view is still there, fully alive, holding a running timer. Fixing the
  timers is the fix; this is why the timers can run at all.

  And the curve is the part to watch rather than the current number. Nine more
  games are already queued in the sections above, and Bridge has just joined them.
  Thirty games at Canasta's weight is a different conversation from fourteen. The
  honest trigger to revisit is a measurement, not a feeling: if the roadmap's game
  count doubles, take this reading again before deciding whether an idle game
  should be torn down and rebuilt from its saved state -- which the save/restore
  machinery already makes possible, since every game that keeps anything worth
  keeping can already serialise itself and come back.
  **Layman:** Every game you try stays in memory for the rest of the session, even if you never go back to it.
  Kind: investigate.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0058] **The sound effects are never deleted, and that will drown out the leak checking GHUB-0052 needs.**
  `Sound::play()` builds `kVoices` (4) `QSoundEffect` objects the first time an
  effect is asked for, so a full session can reach 68 of them across the 17 files
  in `assets/sounds/`. Each is `new QSoundEffect` with **no parent**, held in a
  static singleton whose destructor deletes none of them.

  In bytes this barely matters, and the lazy construction around it is right --
  the comment says a game never pays for sounds it does not play, and that is
  worth keeping. Qt's own sample cache means four voices of one effect share a
  single decoded buffer rather than four. Nobody's machine has ever noticed this.

  The reason to fix it is the timing rather than the size. GHUB-0052 asks for a
  fuzz harness under AddressSanitizer, and ASan reports leaks at exit. A run that
  always ends with 68 known-leaked `QSoundEffect` objects trains whoever reads
  that output to skim past the leak section -- and the first real leak arrives in
  exactly that section, indistinguishable from the noise. **A known leak is worse
  than its size, because it teaches people to ignore the detector.** Clear it
  before the detector arrives, not after it starts crying wolf.

  A parent `QObject` owned by the singleton is the whole fix, with one Qt-specific
  care: these are `QObject`s in a function-static, so teardown order against
  `QApplication` matters, and the safe shape is destruction driven from
  `QCoreApplication::aboutToQuit` rather than from a static destructor running
  after Qt has gone. The offscreen guard in the constructor already means the test
  binaries build none of these at all, which is why the leak has never shown up in
  a test run.
  **Layman:** Each sound is created and never cleaned up; harmless in itself, but it will make the new memory-error tests report noise.
  Kind: fix.
  Source: in-session-2026-08-20.

### 🪟 The window and getting around it

Reported by the owner on 2026-08-20: *"a new window opens for every game and

every window is of a different size. I was hoping for more of a unified

window."*

Worth stating what is actually happening, because the diagnosis changes the fix.

There is only ever ONE window. `HubWindow` holds a `QStackedWidget` with a page

per game, and no game has ever opened a window of its own. What the report

describes is that single window resizing and MOVING on every switch, which is

indistinguishable from a new window appearing — and it is a deliberate feature

doing it. The first item below reverses that feature; the other two are about

the same complaint from a different angle, which is that the app gives you no

way to go from one game to another and no sign of what you already have in

progress.

- 📋 [GHUB-0060] **One window that stays where you put it, instead of fifteen remembered shapes fighting each other.**
  **This reverses a deliberate feature, and the owner has asked for it.**
  `hubwindow.cpp`'s `geometryKey()` stores a separate geometry per page --
  `window/geometry/<game>` for each of the fourteen, plus `window/geometry/menu`
  for the tile grid -- and `applyPageGeometry()` restores it on every switch. The
  intention was that each game reopens at the size it was last played at, and
  CLAUDE.md records it as a feature of the hub.

  In use it produces exactly the reported symptom, and two details make it worse
  than it sounds. `restoreGeometry()` restores POSITION as well as size, so the
  window does not merely resize -- it jumps across the screen. And the tile grid
  owns a geometry too, so a round trip of grid to Chess to grid to Spider resizes
  and relocates the window four times. Fifteen geometries drifting apart
  independently is not a set of remembered preferences; it is a window that will
  not sit still.

  The replacement is one key. One window geometry for the whole app, written when
  it closes, restored when it opens, and untouched by switching pages.

  **The trap that will break a naive version of this, and the pattern that already
  solves it.** Games have different minimum sizes -- Canasta's `minimumSizeHint()`
  returns 900x656 while the legibility switch is on -- and Qt CLAMPS a window up
  to the current page's minimum. With one shared geometry, opening Canasta once
  would grow the window, `rememberPage()` would write that grown size back as the
  user's preference, and every other game would inherit it permanently. That is
  the identical trap CLAUDE.md documents for the legibility switch, and
  `applyLegibility` already solves it: keep the pre-clamp size and put the window
  back on the way out. Copy that, do not reinvent it.

  Two smaller things to carry. The fifteen old keys should be removed rather than
  left behind, and CLAUDE.md warns that anything sweeping stored state has four
  families to handle -- `display/legibility`, `donate/*`, `window/geometry/*` and
  `saved/*` -- so touch only the one. And the first-run default is 880x680 while
  `kFitsBesideYourWork` is 960x1000; a single remembered size makes that opening
  number matter more than it did, since it is now the shape everything starts in.
  **Layman:** The window should stay the size and place you left it, rather than resizing and hopping about every time you open a game.
  Kind: ux.
  Source: user-request-2026-08-20.

- 📋 [GHUB-0061] **There is no way to get from one game to another without going back to the front door.**
  The menu bar holds one menu, `&Help`. Every move between games goes through the
  tile grid: Escape or the toolbar's back button to the grid, scroll to the tile,
  click it. For a collection whose whole premise is fourteen games in one place,
  the only route between any two of them is via the lobby.

  A `&Games` menu listing all of them, each opening its page directly, is a small
  addition that does most of the work of making this feel like one application
  rather than a launcher. The tile grid stays exactly as it is -- it is a good
  front door, and the miniatures are part of the character of the thing. This is
  the route for someone who already knows where they are going.

  Three things it gets for free. It makes the REGISTERED game names visible, which
  is what `--game` takes and is a documented trip hazard -- Klondike is registered
  as "Solitaire" and the flag wants the registered name. It gives every game a
  natural home for a `Ctrl+<n>` accelerator if that turns out to be wanted. And it
  scales where the grid does not: fourteen tiles at 190 pixels are already five
  rows deep and the first-run window is 880x680, so the bottom rows need scrolling
  on a fresh install today. Nine more games are queued and Bridge has just joined
  them -- at twenty-four tiles the grid is a scrolling list with pictures, while a
  menu is still a menu.

  The grid's own density is worth a look at that point, but it is a separate
  question and should not be bundled here.
  **Layman:** To switch games you must always return to the tile screen first; there is no menu listing the games.
  Kind: ux.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0062] **The hub knows which games you have left half-played and shows it in the one place he does not look.**
  The save machinery works and is invisible. A game that overrides `saveState()`
  is stored on close and restored the next time it is opened, with no dialogue
  anywhere -- which is the right design. But the hub then announces it with
  `m_status->setText("Carried on from where you left off.")`, and the status bar is
  specifically the place the owner does not read during play. So the one signal
  that a half-finished game came back sits where it will not be seen, and the
  first indication that anything resumed is recognising the position on the board.

  The tiles have the same gap in the other direction. Each paints its name, its
  blurb and its miniature, and a game with a hand in progress is indistinguishable
  from one that has never been opened. The information exists -- `openGame()`
  already reads `QSettings().value(saveKey(e.name))` -- it is simply never shown
  until you are inside.

  So: mark a game that has something stored, on its tile and on the `&Games` menu
  entry above. "Continue" rather than "Play", or a small corner mark; the wording
  matters less than the fact that the front door stops lying about what is behind
  it. And say it on the play surface when a game resumes rather than only in the
  status bar.

  Two cautions. **Do not read settings from `paintEvent`** -- fourteen QSettings
  lookups per repaint of the grid, which hovers repaint constantly, is exactly the
  kind of cost the Performance section is about; read once when the grid is shown
  and hold it. And the existing rule that an empty state clears the stored save is
  what keeps this honest: a finished game correctly carries no mark, so the
  indicator means "in progress" rather than "has ever been played".
  **Layman:** Games you have in progress look exactly like games you have never opened, and the only notice that a game resumed appears in the status bar.
  Kind: ux.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0067] **A saved game survives a clean exit and nothing else, and two copies of the app quietly overwrite each other.**
  Two halves of one question: when is progress actually written to disk?

  **Only on the way out.** `rememberPage()` stores the current page's game when
  you leave it, and `closeEvent()` stores every game that was opened. Both are
  clean-exit paths. A crash, an out-of-memory kill, a power cut or a `pkill` while
  you are sitting in a game loses that game entirely — every other game was
  banked when you left it, so the exposure is exactly the one you were playing,
  which is also the only one you cared about. `pkill` is not hypothetical here:
  CLAUDE.md documents killing the app that way as routine, with a warning about
  the pattern rather than about the lost state.

  Saving after each completed move is the obvious answer and is probably cheap for
  every game but Canasta, which serialises its whole engine. If a timer is used
  instead, it belongs to the active page only and must stop on `deactivate()` —
  GHUB-0046 is about exactly the timer that does not.

  **And two copies fight.** Nothing stops the app running twice — a second click
  on the panel launcher, or `gameshub --game spider` while the hub is already
  open, which is a documented and encouraged way to start. Both processes write
  the same `QSettings`, so the second to exit overwrites the first's saves,
  geometry and best scores with its own older view of them. Silently, and with no
  way to tell afterwards.

  The donate counter has the same shape and is worth checking while in there:
  `donate/launches` is read and written per process, so two instances count
  against each other.

  finbreak solved this with a single-instance guard (FIBR-0189) and hit a real
  trap doing it, which is worth reading before writing one here: its guard held a
  socket that the update relaunch had to release explicitly, or the replacement
  process saw a live owner and quietly exited. GHUB-0043 introduces exactly that
  relaunch. The two items should be built with each other in mind rather than in
  either order by accident.
  **Layman:** If the app crashes, the game you were in is lost — and opening it twice means whichever copy you close last wipes the other's saves.
  Kind: fix.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0068] **There are five places to change a setting and no place to change settings.**
  Sound is a toolbar toggle. Legibility is the toolbar toggle beside it.
  Minesweeper's difficulty is a toolbar action inside Minesweeper. Canasta's house
  rules are a dialog reached from inside Canasta. The donate prompt's on/off lives
  in the donate dialog under Help. Each one is sensibly placed for the moment you
  want it, and there is nowhere to go to see what the app can be told.

  That has been survivable at fourteen games and two app-wide switches. It gets
  worse on a schedule: GHUB-0043 adds "check for updates automatically", which is
  the first setting with no natural home at all — it belongs to no game and to no
  toolbar — and the first-run question it asks has to be changeable afterwards or
  it is a decision the player is stuck with.

  A single Preferences dialog, reached from a menu, holding the app-wide switches
  — sound, volume, legibility, update checking, the donate prompt — with per-game
  rules staying where they are. The toolbar toggles stay too; a settings dialog
  that removes the one-click sound switch is a worse app, and this is about having
  a place where everything is visible, not about taking the shortcuts away.

  One thing to get right, because the app already has a rule about it. CLAUDE.md
  notes that stored state comes in four families — `display/legibility`,
  `donate/*`, `window/geometry/*` and `saved/*` — and that anything sweeping them
  has all four to handle. A Preferences dialog is where somebody eventually adds a
  "reset everything" button, and the warning already written down is that clearing
  the donate switch while leaving the counter fires the prompt on the very next
  start. If that button is built, it handles all four or it is not built.
  **Layman:** Preferences are scattered across the toolbar, a menu and a dialog, with no single settings window.
  Kind: ux.
  Source: in-session-2026-08-20.

- ✅ [GHUB-0074] **Snake and Hearts kept playing after you left them.**
  The same defect as GHUB-0073, in two more games. Both own a QTimer
  and neither overrode GameView::deactivate(), whose whole purpose is
  to stop a clock when the hub moves on.

  Found by a lane question rather than by a finding: it asked whether
  the new stop-on-leave assertion really reaches only Pinball. It does
  — and the reason is that a pixel probe can only catch a game that
  HAPPENS to be moving when the hub leaves, and every clock here except
  Pinball's is idle on a freshly opened board. So both games passed
  every green run.

  Hearts needed the resume as much as the stop. Its activate() only
  refreshed, so stopping the clock without adding one would have left
  the computers frozen mid-trick — a worse bug than the one being
  fixed. It now restarts when a trick is pending or a computer is due.

  gamesStopTheirClocks replaces the observation with a structural rule:
  no QTimer may be active after deactivate(), asserted for all fourteen
  games whether or not anything was visibly moving. Snake's and
  Pinball's stop-and-resume are checked directly.

  Honest gap: removing Hearts' resume reddens nothing, because reaching
  a state where its clock is due means driving the pass first. Snake's
  break was seen red; Hearts' was not, and that is recorded rather than
  implied away.

  The block cost a lesson too. Leaving the two views running inside it
  kept their timers alive through the rest of the suite, and Pinball
  repaints on every pump — a 34-second run had not finished in two
  minutes. Each view is now scoped and left stopped.
  **Layman:** Leaving Snake mid-game ran the snake into a wall while you were elsewhere, and leaving Hearts finished the hand without you.
  Kind: fix.
  Source: in-session-2026-08-20 (CLAUDE.md cold gate, loop 3).

- ✅ [GHUB-0135] **The per-page window size ratchets up and cannot come back down.**
  Every view calls setMinimumSize(minimumSizeHint()), and
  QStackedLayout takes the largest minimum over every page it has
  BUILT. So once Canasta exists the window cannot go below its width
  on any page: applyPageGeometry restores the stored size, Qt clamps
  it up, and the next rememberPage writes the clamped value over the
  stored one. Permanent, and dependent on which games were opened
  that session. Making the hidden page's size policy Ignored is the
  likely fix.
  Resolved (2026-09-02): HubWindow::onlyTheOpenPageSetsTheFloor(),
  called on every page change before applyPageGeometry.

  The suggested fix -- an Ignored size policy on the hidden pages --
  is half of it, and half moves nothing. Qt works a page's
  contribution out from BOTH its policy and its explicit minimum:
  Ignored drops the minimumSizeHint from the sum, and the explicit
  minimum every view sets with setMinimumSize(minimumSizeHint()) is
  then written straight back over the top. Measured with the policy
  half alone and the figures did not budge; both halves together fix
  it. The page's own policy is remembered rather than assumed,
  because the tile grid is a QScrollArea and does not carry a plain
  widget's default.

  The floor is handed back from the page's own minimumSizeHint(), the
  same expression the views use on themselves, so a game whose hint
  moves gets its current answer rather than a stale copy. Lowering the
  floors is followed by the layout walk CanastaView::applyLegibility
  already makes, for the reason CLAUDE.md gives: the chain through the
  QStackedWidget is recalculated lazily, so the resize that follows
  would otherwise be clamped back up by the stale figure.

  Measured: Chess asks 360x444 alone, 720x644 once Canasta has been
  opened, and 360x444 again afterwards. Two checks in uitest print the
  figures and assert the relationship, not the numbers -- those are
  properties of this machine's fonts. Proven red. Windows leg run,
  4/4, with no FAIL line in LastTest.log.
  **Layman:** Once you have opened Canasta, no other game's window can be made as small again -- and it is remembered that way.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- ✅ [GHUB-0136] **--shot and --legible write to the player's stored settings.**
  takeShot's own comment says answering a question about the app
  should not change what the player chose. But openGameNamed reaches
  rememberPage with m_geometryReady already true, so even a REFUSED
  shot has written window/geometry/menu; and --legible goes through
  Legibility::setEnabled, which persists, so a crash between the two
  writes leaves large play on for good. An ephemeral flag on
  HubWindow and a non-persisting setter would remove the reason
  CLAUDE.md's shot recipe needs XDG_CONFIG_HOME.
  Resolved (2026-09-02): HubWindow::setRemembering(false), set before
  the game is opened, and Legibility::setEnabledForSession(), which
  broadcasts without storing. Measured with a scratch XDG_CONFIG_HOME,
  fix reverted: a successful shot wrote BOTH display/legibility and
  window/geometry\menu; with the fix, a successful shot, a shot
  refused for a bad --size and a shot refused for an unknown game all
  leave the store empty.

  One correction to the finding. The refused shot that writes is the
  one refused AFTER the game opened -- a bad --size. An unknown game
  makes openGameNamed return false without ever reaching openGame, so
  rememberPage is not called and nothing is written. Both were run
  both ways.

  Six checks in uitest rather than a settings-file check, because
  QSettings has no file on Windows. Each absence is paired with the
  positive case -- the ordinary setter still stores, an ordinary hub
  still remembers -- so neither can pass by the mechanism never
  running.
  **Layman:** Taking a screenshot changes what the app remembers, which is why the recipe needs a scratch settings directory.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0137] **Decide what hasPendingAnimation() is for, or drop the claim.**
  Declared on GameView, overridden once by Canasta, asserted by
  tests, and described in CLAUDE.md as how a game says it is holding
  state a settings change would consume -- with no product caller.
  Three lanes found it independently. The hazard it names is now
  guarded for background pages by the applyLegibility isVisible fix,
  and is deliberate for the visible one. So either the hub consults
  it before storeSave/rememberPage, or it and the CLAUDE.md claim go.
  Not a call to make from a review.
  **Layman:** A safety mechanism the notes describe has nothing in the app that asks it.
  Kind: investigate.
  Source: review-code sweep 2026-08-31.

- ✅ [GHUB-0155] **Scores::clear() wipes the whole settings store, not the scores.**
  scores.cpp: Scores::clear() is QSettings::clear() on the
  default-constructed, whole-application scope. It destroys
  display/legibility, audio/muted, donate/ask, donate/launches, every
  window/geometry/<page>, every saved/<game> and canasta/house/* --
  not the scores its class name and header promise. It has zero product
  callers today, which is the only reason it has not lost anyone's
  saved games; GHUB-0068 explicitly anticipates a reset-everything
  button in a Preferences dialog, and this is the method whose name
  someone will reach for. Remove the score keys it owns, and leave
  whole-store wiping to a method named for that.
  Resolved (2026-09-02): clear() removes score keys only, matched by
  shape rather than by a list -- a leaf beginning best_, or the leaf
  wins, which is how chess and draughts spell theirs. A list here
  would be a second copy, since the keys are declared partly in
  scores.h and partly as constants inside the views, and the game it
  missed would keep its record through a reset with nothing to say so.
  The header now states what the method does and does not touch.
  Locked in uitest: a saved game, a remembered window size and a house
  rule all stand through a clear, and both spellings of a score go.
  Proven red against the old whole-store wipe.
  **Layman:** The method named for clearing scores actually erases everything the app remembers, including saved games.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- ✅ [GHUB-0156] **The hub's stored settings are written unchecked, and a stored read can silently return zero.**
  hubwindow.cpp consults QSettings::status() nowhere and never syncs on
  the close path, so a full disk or a read-only ~/.config loses every
  saved game and remembered size in silence -- against the README's
  promise that closing mid-game brings it back. scores.cpp reads with
  QVariant::toInt(), which returns 0 rather than the fallback on a
  non-numeric stored value: for the recordLow games (Minesweeper and
  Sudoku times, Spider and FreeCell moves, the Hearts total) that
  leaves best() at 0 and recordLow then refuses every future result
  forever. Use the ok-flag overload and treat a bad read as absent.
  Also: restoreGeometry's return is dropped, so a corrupt geometry key
  skips the 880x680 fallback and the window gets no sizing at all.
  Resolved (2026-09-02): all three parts. has() and best() use the
  ok-flag overload, so a stored value that is not a number reads as no
  record rather than as zero -- which for the recordLow games was a
  score nobody could beat, locking the record for good. Writes go
  through checkSettingsWritable(), which syncs and reads status(), and
  says once in the status bar that games and window sizes will not be
  remembered; it is said there rather than on the close path, where
  the window is going away and there is nowhere left to say it.
  applyPageGeometry honours restoreGeometry's return, so an unreadable
  blob still reaches the 880x680 fallback instead of leaving the
  window unsized. Four checks in uitest cover the corrupt best, proven
  red.
  **Layman:** If the app cannot write its settings it says nothing, and a damaged best score can lock you out of ever beating it.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- ✅ [GHUB-0157] **The legibility toolbar button does not follow the switch it sets.**
  hubwindow.cpp connects the action's toggled signal to
  Legibility::setEnabled, and nothing connects Legibility::changed back
  to the action. Under --shot --legible the photograph shows the
  toolbar reading "Normal", unchecked, with large play plainly on --
  visible in this sweep's own screenshots. One connect fixes it, and
  setEnabled is already a no-op when unchanged so there is no loop.
  Resolved (2026-09-02): one connect from Legibility::changed to
  QAction::setChecked. setEnabled() is a no-op when unchanged so it
  cannot loop, and setChecked() emits toggled only on a real change,
  which carries the label across as well. Two checks in uitest, proven
  red.
  **Layman:** Turn large play on any other way and the toolbar still says Normal.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- ✅ [GHUB-0158] **--help and a mistyped flag open a message box on Windows instead of exiting.**
  main.cpp routes --version around QCommandLineParser via an argv scan
  for exactly this reason -- qt_add_executable sets WIN32_EXECUTABLE, so
  showVersion() puts its output in a message box. --help and every
  parser ERROR still go through process(), which uses the same
  mechanism: gameshub.exe --gaem spider opens a modal dialog and hangs
  where it should exit 1. Use parser.parse() plus explicit errorText()
  handling. Same family: parseSize has no upper bound, so --size
  999999x999999 attempts an enormous pixmap; and geometryKey/saveKey
  are built from the registered display name, so renaming a tile
  orphans its save, its geometry and its scores with no migration.
  Resolved (2026-09-02): all three parts.

  parse() replaces process(), with errorText() to stderr and exit 1,
  and helpText() printed to stdout. Verified by running the binary:
  --help prints and exits 0, --gaem spider prints "Unknown option
  'gaem'" and exits 1, neither hangs. No --version branch was added --
  the argv scan at the top of main() answers it before Qt exists, so
  one there could never be reached; the option stays registered so it
  appears in the help.

  parseSize now bounds each side at 8192 as well as requiring it
  positive; --size 999999x999999 is refused with the bound named
  instead of asking for a four-terabyte pixmap.

  The display-name coupling is recorded as a comment at geometryKey
  and saveKey rather than rebuilt. Nothing renames a game today, a
  stable id per game is the fix if anything ever needs to, and the
  value now is that the next person renaming one reads why they
  should not. Called out here rather than left implied.
  **Layman:** On Windows a typo in a command-line option pops a dialog and hangs rather than printing an error.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0167] **Filter the tile grid, including a favourites filter.**
  The grid lists every game and nothing narrows it; it already
  scrolls, and the roadmap queue adds more.

  Filter by a category the game itself declares at registration, not
  by a list held in the filter bar -- a list is a second copy that
  goes stale the day a game is added, and the grid is built from the
  registry. Proposed categories, one per game: Board (Chess, Reversi,
  Draughts), Cards (Solitaire, Spider, FreeCell, Pyramid, Hearts,
  Canasta), Puzzle (Minesweeper, Sudoku, 2048) and Arcade (Snake,
  Pinball). A game names exactly one, so the filters partition the
  grid rather than overlapping.

  Favourites is the second filter and is per player, so it is stored
  state rather than a registry property -- a starred-name list beside
  the other app-wide settings, toggled from the tile. Note the reset
  sweep in CLAUDE.md: a new app-wide settings family has to be added
  to whatever clears stored state, or a reset leaves it standing.

  Open for the owner: whether the filters are toggle buttons above
  the grid or a combo box, and whether "All" is a filter or the
  absence of one.
  **Layman:** The game list gets buttons to show only one kind of game, and a star to keep your favourites together.
  Kind: feature.
  Source: user-request-2026-09-02.

### ✨ Look and feel

Not decoration. Every item here is a piece of information the game currently

states in words — usually in the status bar, which § Legibility has already

established the owner does not read during play (GHUB-0040) — and could instead

show, where the eye finds it without being sent to look.

That is the through-line, and it is why the owner's own suggestion opens the

section: a soft light on whoever is playing answers *is it my turn yet* the way

a lit room answers *is anyone home*. The other two are the same trade in

different places — a suit that is a drawn shape rather than a borrowed font

glyph, and a card that visibly travels rather than teleporting.

The existing legibility work is GHUB-0017's, not this section's. That item owns

the twelve remaining per-game passes; these three are about what every game

draws, whether or not it has had one.

- 📋 [GHUB-0063] **A soft light on whoever is playing, so whose turn it is needs no reading.**
  The owner's suggestion, and it is the right answer to a problem this project
  keeps hitting from other directions.

  What the games do today is all text, and all small. Hearts recolours a seat
  label to gold and leaves everything else identical -- a few words changing
  colour at the edge of the table. Canasta writes a sentence, "North is playing."
  The two-player board games mostly leave it to the status bar, which GHUB-0040
  already established is the one place the owner does not look while playing. In
  every case the information is present and phrased as something to be read.

  A glow is not read. Brightness and area register before attention is directed at
  them, which is exactly the property wanted for a question the player is not
  asking on purpose -- *is it me yet* -- and it is worth more to a partially
  sighted player than any wording could be. It applies in three places: the four
  seats in Hearts, the four around the Canasta table, and the two sides of Chess,
  Draughts and Reversi, where "the computer is thinking" and "it is your move" are
  the same two states seen from opposite ends.

  **It pairs with GHUB-0047.** Once the engine stops running on the drawing
  thread, the window stays alive while the computer thinks -- and a light sitting
  on the computer's side is then a genuine progress indicator rather than a frozen
  picture.

  Four design constraints, and the last two are the ones that bite.

  Do not let the glow be the only cue. Luminance and area are already better than
  hue, but pairing the light with something structural -- a lifted seat panel, a
  thickened edge -- means it survives any display, any contrast setting, and a
  player who is also colour-blind.

  It should strengthen when the legibility switch is on, in the same way the rest
  of that work does. `Legibility::instance()` broadcasts to every constructed
  game, so the hook already exists.

  **Fake the blur; do not compute one.** `Theme::paintDropShadow` already sets the
  house precedent -- three stacked translucent rounded rectangles, with a comment
  saying a real blur is not worth it at card size. One radial gradient is the
  equivalent here. A genuine blur per frame is the sort of thing the Performance
  section exists to prevent.

  **Fade in once and hold. Do not pulse.** A pulsing light means a repaint every
  frame, forever, in games that currently repaint only when something changes --
  turning a still screen into a 60 Hz one and undoing GHUB-0046 by a different
  route. A short fade when the turn passes, then a steady light, gives the eye the
  movement that draws it and costs nothing while it sits. In Canasta the fade must
  also keep clear of `m_flights` and `animating()`, which already own that view's
  timing.
  **Layman:** The seat of the player whose turn it is gets a gentle glow, so you can see at a glance who is up.
  Kind: ux.
  Source: user-request-2026-08-20.

- 📋 [GHUB-0064] **The pip pattern is how he reads a card, and its shape is chosen by whatever font the operating system supplies.**
  `CardArt::drawPip` sets a font size and calls `p.drawText()` with the Unicode
  character for the suit. So do both corner indices. The pips are typography, not
  artwork.

  That matters more here than it would in most card games. The owner reads a card
  by its pip pattern rather than its corner index -- CLAUDE.md states it as a
  design constraint, and it is the reason melds put wild cards first and are drawn
  at 0.74 rather than as slivers. The single most load-bearing graphic in the
  collection is currently whatever glyph the host system decides to hand over.

  The consequences are the ones this project has already paid for once. A glyph's
  weight and proportion differ between platforms, so the same hand does not look
  the same on Windows and Linux. Hinting rounds the ink by a whole pixel at small
  sizes -- measured during the Sudoku work, where one font came out at 0.742 of an
  em at size 100 and about 0.685 at the 7-to-11 point sizes actually drawn. And a
  host with no suitable font gives replacement boxes; `windows-2022` under the
  offscreen platform returns an EMPTY font family list, which is recorded in
  CLAUDE.md as the number to remember.

  Four `QPainterPath` shapes remove all of it. They are identical on every
  platform, they scale cleanly instead of being hinted, and -- the part a font
  cannot do -- they can be given extra weight or a heavier outline when the
  legibility switch is on, so the pattern thickens for the person who needs it
  rather than merely getting bigger.

  **The court cards are NOT part of this, and the current design is right.**
  `drawCourt` gives J, Q and K a ruled panel with a letter rather than figure art,
  and its comment says why: it reads as a court card at any size and never turns
  to mush when cards are small. Keep that. The only opening there is at the large
  end -- with the legibility switch on and room to spare, a richer court could be
  shown -- and that is a separate question from getting the pips under our own
  control.
  **Layman:** The club, diamond, heart and spade symbols are typed as text, so they look different on different computers instead of being drawn by us.
  Kind: ux.
  Source: in-session-2026-08-20.

- 🚧 [GHUB-0065] **One game animates and thirteen teleport.**
  Canasta has card flights. Nothing else does. In every other game a card is in
  one place, and then it is in another, with nothing in between: a deal arrives
  fully formed, a run lands on a foundation, a trick gathers itself up. The move
  happened and the screen reports the result.

  Motion is information, and it is the kind this player can use. It answers *what
  just changed and where did it go* -- which a redraw of the finished position
  does not answer at all, because by the time you look, the change is over. An
  auto-move to a foundation in Klondike, FreeCell or Spider is the clearest case:
  cards leave on their own, several at a time, with no indication of which ones
  went or where from.

  **Reuse Canasta's design, and read its traps before writing a second
  implementation.** They are documented and they were expensive. A card in the air
  must be suppressed at its destination or the eye sees it twice. The match
  between a flight and a card is consumed one per flight -- without that, two
  identical cards arriving together suppress both destination copies and one card
  vanishes, which is routine rather than exotic in a two-pack game. And a flight
  carries a captured destination point, so anything that moves the layout has to
  clear the flights first or a card lands where its target used to be.

  Two boundaries. **An animation timer is a timer**, so whatever gets built here
  owes GHUB-0046 a `deactivate()` that stops it -- adding motion to thirteen games
  without that would multiply the exact fault that section is about. And this is a
  large surface if taken all at once; the auto-moves above are where the ambiguity
  actually is, and dealing animations are the pretty part rather than the useful
  part. Start where a player currently cannot tell what happened.
  Progress (2026-08-28): the auto-move half is done — the part the
  bullet says to start with. Left open deliberately; the rest of the
  surface it describes is untouched.

  A correction first. The bullet's clearest case — "an auto-move to a
  foundation, cards leave on their own, SEVERAL AT A TIME" — is not
  behaviour this app has. KlondikeTable::autoFinishStep() exists and
  is called by the self-test and by nothing else; no view offers it.
  So the teleports a player actually meets are a double-click sending
  ONE card home, and Spider harvesting a completed run. Both are now
  animated.

  src/cards/cardflight.* is the shared piece: a card, where it left,
  where it is going, an eased position and a stagger. Presentation, so
  it sits in GAME_VIEW_SOURCES even though it needs nothing from
  QtWidgets — the reasoning legibility.cpp already carries. Two of
  Canasta's three traps are handled inside it: suppressAt() consumes
  ONE flight per answer, so two identical cards arriving together
  suppress two destination copies rather than one twice, and a locked
  test covers exactly that. The third cannot be — a flight carries a
  destination captured when the card left, so the caller must clear
  them when the layout moves. All three views therefore clear on both
  deactivate() and applyLegibility(); the caption band comes off the
  height these games solve their card size from, so the switch moves
  every rect on the surface.

  Each view owns a QTimer and so overrides deactivate(), which is
  GHUB-0046's boundary and the rule CLAUDE.md states structurally
  rather than by observation.

  Spider is the starkest and got the most care: thirteen cards leave a
  column at once, staggered so it reads as a sequence. They fly to the
  stock corner, which is the only anchor on that surface meaning "put
  away" — Spider draws no completed-runs pile, and the count lives in
  the status bar, the one place this project knows the owner does not
  read. Giving those runs a home on the play surface is a layout change
  and a bigger item; the motion at least answers where they went.

  Tested rather than eyeballed, because --shot photographs a game the
  moment it opens and can never see a flight. flightsInTheAir() exists
  so a check can ask what no rendered picture can answer, on the
  precedent of SudokuView::marksFitAt. Klondike and FreeCell open fresh
  deals until one has a home-able ace (three, both times) and assert
  the card flies, lands and stops the timer. Spider's position is
  BUILT — a one-suit game, King down to Two in the first column, the
  Ace alone in the second — because no amount of poking at a random
  deal completes a run; it then asserts all thirteen fly, that they are
  still going after a tick that would have finished one unstaggered
  card, and that they all arrive.

  Still to do, and why this stays open: dealing animations (the bullet
  calls them the pretty part rather than the useful part), Klondike's
  stock and waste, Pyramid's matched pairs, the board games, and
  Spider's harvest via dealRow rather than a drop — that path can
  complete several runs across several columns at once and needs the
  table to say which, where the drop path already reports its column.

  Frame cost unchanged: canasta at rest 7.69 against the 7.70 recorded
  on 2026-08-25. It costs nothing when nothing is flying. ctest 6/6.
  Shipped in 0.5.0 (2026-08-31): the auto-move half only, so this
  stays open. The release notes describe the work but deliberately do
  NOT cite this id on the entry's bullet line — cut-release reads a
  bullet-line id as a claim that the item closed, and stops the release
  when the roadmap disagrees. The id sits in the entry's prose instead,
  which that gate treats as a cross-reference rather than a claim. Put
  the citation back on the bullet line when the rest lands: the deals,
  the board games, Klondike's stock and waste, Pyramid's matched pairs,
  and Spider's harvest via dealRow.
  **Layman:** Only Canasta shows cards moving; everywhere else a card is simply somewhere else the next time you look.
  Kind: ux.
  Source: in-session-2026-08-20.

- ✅ [GHUB-0094] **A frozen pile draws its freezing card twice, and out of order.**
  Reported with a screenshot: East threw 2 of clubs, the pile
  is frozen, and the same 2 is drawn once sideways and once
  upright.

  Two faults in one block of paintTable. The sideways card is
  found with a reverse search for the last wild in the pile,
  which IS the top card when the freezing throw was the most
  recent one -- so it is painted sideways and then painted
  again upright on top of itself. And it is always drawn one
  place below the top card rather than at the depth it
  actually sits at, so every later discard slides UNDER it
  instead of over it.

  The fix is one idea: the freezing card has an index in the
  pile like any other card, and the stack is drawn in that
  order.
  Resolved (2026-08-24): Engine::freezeCardIndex() names the card
  that froze the pile -- the last wild or red three still in it,
  which is exactly what both freeze sites turn onto the pile. The
  table draws the stack deepest-first in one pass with one branch
  per card, so the freezing card takes the sideways branch OR the
  top-card branch and never both. Confirmed on screen with --shot.
  canastaFrozenPileDepth() in the self-test locks the index in both
  positions, and was proved red against the old always-just-under-
  the-top behaviour before the fix went in.
  **Layman:** When the pack is frozen the wild card that froze it is shown both sideways and upright, and it jumps back on top whenever anyone throws.
  Kind: fix.
  Source: user-report-2026-08-24.

- ✅ [GHUB-0095] **House rule: the card that freezes the pack lies as a T, not fully across.**
  A table convention rather than a rule of play: the engine
  never reads it, and nothing about which moves are legal
  changes. It belongs in the House rules dialog beside the
  rest, because it is one of the things this family does
  differently.

  Off in Classic, on in House.
  Resolved (2026-08-24): Rules::freezeCardMakesATee, off in Classic
  and on by default in House. CanastaView::freezeCardCentre() slides
  the card along its own length until its near edge lines up with the
  pile's, outward from the stock so the T never reaches back over it.
  Confirmed on screen: a 2 of diamonds froze the pack and lies behind
  an ace of hearts with one end out.
  **Layman:** The family lays the freezing card so one end sticks out, making a T with the pile, rather than squarely across the middle.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0096] **House rule: finished canastas stack on the red threes, turned about.**
  The owner's family plays it this way with or without red
  threes under the pile. Each new canasta turns ninety degrees
  from the one below, which is how the stack is counted at a
  glance.

  Visibility was raised with the request and settled by the
  owner on 2026-08-24: the STAIRCASE. Each canasta is offset
  along its own long axis so a readable strip of every one
  below stays out, and each keeps its own badge. A squared
  stack true to the table hides all but the top canasta, which
  is the wrong trade for a partially sighted player.

  Off in Classic, on in House. Off, canastas stay in the meld
  row exactly as now.
  Resolved (2026-08-24): Rules::canastasStackOnRedThrees, off in Classic
  and on by default in House. meldOrder() drops a finished canasta from
  the row and canastaStackRect() lays it on the red threes, index 0
  across over them and each one after that turned ninety degrees and
  slid left into the room the row just freed.

  The slide is kStackBadgeRoom = 1.75 card widths, which is what one
  canasta reserves for its badge -- measured on screen, not guessed: at
  1.34 the badges of a four-canasta stack ran into one another and the
  stack could not be read at all, which is the one thing the staircase
  was chosen over a squared pile to avoid. The reservation and the slide
  are the same figure, so a stack that fits the band is a stack whose
  badges are readable.

  The order is the view's, not the engine's: a meld becomes a canasta
  long after it was laid, and sorting by meld order would make an
  earlier canasta stand up when a later one completed.

  canastaStackFitsItsBand in the UI test walks stacks one to eight deep
  in both legibility states, asserting the whole footprint -- card, ring
  and badge -- stays in the band and that the alternation holds. It
  asserts a floor of four canastas fully spaced and REPORTS the real
  capacity, which moves with the window. Both defects were proved to
  redden it: the narrow slide, and the right-hand end hanging over the
  edge of the band.

  Seen rather than reasoned about, which is what GHUB-0090 was for.
  **Layman:** A completed canasta is squared up and laid on the team's red threes -- across, then upright, then across again -- so you can count them by their edges.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0097] **A finished canasta names itself by the colour of its top card.**
  The universal table convention, and the one the code's own
  comment already reaches for -- paintMelds rings a canasta
  gold or silver and calls that "the traditional red/black
  pile marker, in the table's own metals". This is the real
  thing.

  Settled by the owner 2026-08-24: MIXED means any wild card,
  a two exactly as much as a joker. The alternative -- jokers
  only, as first described -- would have a canasta built with
  two deuces show red while scoring the 300-point mixed bonus,
  so the top card and the score would contradict each other.

  Only applies to a canasta squared up in the stack. An
  unfinished meld keeps its wilds-first fan, which is what
  the owner reads a meld's contents from.
  Resolved (2026-08-24): canastaTopCard() picks the card a squared
  canasta is turned up under -- red when the meld holds no wild, black
  when it does. It prefers a natural of the wanted colour so the corner
  index names the canasta's own rank, falls back to a wild of that
  colour, and falls back again to the last card. That last step is real
  rather than defensive: four naturals CAN all be red across two packs.

  Mixed means any wild, a two exactly as much as a joker. The owner
  confirmed the vocabulary himself the same day -- his family calls a
  two a small joker and the 50-point one a big joker -- which is also
  what isNatural() means and what the 300-point bonus is paid on, so the
  top card, the ring and the score all say one thing.

  The gold and silver rings stay. The top card's colour is a small
  signal and the owner reads slowly.
  **Layman:** A canasta with no wild cards in it is squared up with a red card showing; one built with a joker or a two shows a black card.
  Kind: feature.
  Source: user-request-2026-08-24.

- 📋 [GHUB-0151] **The card's corner index can clip, and it is all that is left at small sizes.**
  cardart.cpp: paintFace draws the corner index into a box 0.24 of the
  card's width, and drawText(rect, flags, text) CLIPS. Two digits at
  the index font need roughly 0.25 to 0.27 of the width, so "10" is
  tight on a normal face -- and on a font-less offscreen environment,
  where digits measure the full em box, it is about 0.45 and clips to
  roughly half. Below kFaceMinWidth paintFace returns early and the
  corner index is the ENTIRE card, which is where it matters most.
  Qt::TextDontClip on both drawText calls is the fix the project
  already uses for this class in Sudoku's pencil marks; with
  AlignLeft|AlignTop the origin does not move.
  **Layman:** The number in a card's corner can be cut off, exactly where it is the only thing identifying the card.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0152] **CardArt leaves the caller's painter in a state it did not set.**
  cardart.cpp: paintSlot and paintHighlight set pen, brush and font on
  the caller's QPainter and never restore them. Worse, the two paths
  through paintFace leave it in DIFFERENT states -- a cached blit
  touches nothing, while the fallback (any non-translate transform, or
  a card over 4096 device pixels) draws live and leaks the last font and
  ink pen. So a caller that works today breaks when a card is rotated
  or the window grows. Only CanastaView::paintCard wraps in
  save()/restore(); spiderview calls paintHighlight bare inside a paint
  loop. Same shape in chessart.cpp's paintPiece, which is shared with
  the hub tile, so it leaks onto two surfaces. Wrap each public entry
  point and document the postcondition.
  **Layman:** Drawing a card can change the colours used by whatever is drawn next.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0153] **Canasta's table has several layout figures that do not survive a change of shape.**
  canastaview.cpp, all MEDIUM, none fixed. The hover lift disagrees
  between the hit test and the painter, so the top sixth of a card you
  can see is not clickable and a strip of felt below it is. The four
  human* entry points have no animating() guard, where mousePressEvent
  does -- Space is bound to Meld, so pressing it mid-flight can draw one
  card in two places. cardsFitTable is false at reachable window sizes,
  because the horizontal inset is driven by the SMALLER dimension, so a
  taller window shrinks the usable width. A discard that freezes the
  pack flies flat to the pile centre and then jumps and rotates. Two
  fonts inherit bold from the previous painter against evident intent.
  The score plate's width tracks table width while its font tracks
  height (+53% against +5.4% between two reachable shapes). The CANASTA!
  flourish is the only string sized in POINTS, scaled from width, drawn
  into a pixel rect that clips. And suppressed() skips the ring and
  badge along with the card, so a whole canasta vanishes while one card
  flies to it.
  **Layman:** A few numbers on the Canasta table are sized against the wrong dimension, or inherit a style from whatever was drawn before.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0154] **Canasta's rules dialog runs the clock, does not scroll, and syncs its toolbar by text.**
  canastaview.cpp. The dialog is modal for input but m_timer is never
  stopped, so tick() keeps driving aiHalfTurn on the three computer
  seats at kAiPause -- a slow read of a 27-row form can let the hand
  finish unwatched, which is the situation deactivate()'s own comment
  says the computers must stop for. That form has no QScrollArea: 27
  rows plus a 3-line label and a button box is past the 768px floor
  GHUB-0017 names, and a QDialog does not scroll. applyRules() neither
  calls trackCanastas() nor refresh(), so lowering canastaSize mid-hand
  turns melds into canastas the paint order does not know about; it
  also announces a rule-set change when only the TARGET moved.
  restoreState syncs the toolbar by comparing QAction::text() against
  untranslated literals, so adding tr() -- which the Qt standard
  requires -- breaks the sync silently.
  **Layman:** The House rules window can let a whole hand play out behind it, runs off the bottom of a small screen, and is wired up in a way that translating the app would break.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

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

- 📋 [GHUB-0059] **Bridge, with a computer partner and two computer opponents.**
  **Safe, and on the same footing as Canasta.** Rules cannot be copyrighted, which
  is this project's whole test, and Contract Bridge is a century old with no
  owner: no trademark is enforced on the name the way it is on Monopoly or
  Scrabble, and the game is played and published freely everywhere. The one thing
  that *is* owned is the wording — the WBF's Laws of Duplicate Bridge and the
  ACBL's publications are copyrighted as text — which is standing rule 2 and
  nothing new. Write the rules in our own words, as every other game here already
  does. Bridge is missing from the safe list in § Standing rules only because
  nobody had asked for it; Whist and Euchre, its immediate relatives, are both
  already on it.

  Two-thirds of it is already built. Trick-taking, following suit, and a trick
  resolved to a winner are the Hearts engine's shape; partnership scoring against
  a target is Canasta's. Neither is reusable as code, but both are proven designs
  in this codebase, and the AI approach is Canasta's rather than Chess's —
  judgement, not search, with the four levels played against each other rather
  than described, the way `canastaLevelsDiffer()` caught Hard being weaker than
  Medium.

  **The bidding is the game, and it is the risk.** An auction is a language, and a
  computer partner that bids badly makes the whole thing unplayable in a way a
  weak Hearts opponent never does — your partner's bid is information you are
  required to act on. Pick one simple published system, implement its opening
  bids, responses and basic conventions, and say plainly in the blurb which system
  it plays. A vague bidder is worse than a limited one.

  **The dummy is a UI shape nothing here has.** After the opening lead, declarer's
  partner lays their hand face up and declarer plays both. So one player controls
  two hands, one of them exposed, and when the human is dummy they watch a hand
  they can see being played by someone else. Every other game in the collection is
  one hand, one player.

  **Size it before building it, because this is the densest screen in the
  collection and it is the thing most likely to sink it.** Four hands of thirteen,
  a bidding history, a contract, a trick in progress and an exposed dummy —
  against `HubWindow::kFitsBesideYourWork` at 960 wide, and against
  `CardArt::kFaceMinWidth` (46), below which a card has no face at all. Canasta
  already sits at 900 wide with 60 pixels of headroom and needed a whole
  legibility pass of its own (GHUB-0038) to get there. The owner reads cards by
  their pip pattern rather than the corner index, so thirteen fanned cards at a
  readable width is a hard constraint, not a layout preference. Work out whether
  the hand fits at all before any engine is written; if it does not, the answer is
  a different presentation rather than smaller cards.

  Rubber bridge rather than duplicate: duplicate scoring compares your result
  against other tables playing the same deal, and there are no other tables in a
  single-player game.
  **Layman:** The classic four-player partnership card game — you and a computer partner against two others.
  Kind: feature.
  Source: user-request-2026-08-20.

### 🌐 Playing against other people

Every game here plays against the computer or against nobody. This section is

for playing against a person — at the same machine, on the same network, or

over the internet — and for the pieces that need to exist first.

Four games have an opponent at all today (Chess, Reversi, Draughts, Hearts,

and Canasta makes five with three computer seats); the other nine are

solitaire, puzzle or arcade games where a second player has no meaning without

inventing one.

- 📋 [GHUB-0080] **Play against another person — same machine, same network, or over the internet — with Windows and Linux in the same game.**
  Large, and deliberately filed rather than started. What follows is
  what is already known so a later session does not re-derive it.

  **Scope is the five games that already have an opponent, and only
  those.** Owner's call, 2026-08-20. Chess, Reversi, Draughts and
  Hearts have one computer player; Canasta has three. In each of them
  a human simply takes a seat the game already deals with, which is
  what makes the work tractable.

  **The other nine are out of scope and are not a later phase.** The
  four solitaires, Sudoku, Minesweeper, Snake, 2048 and Pinball are
  single-player by design: a second player there would mean inventing
  a mode that does not exist (shared board, race, best-of-three on
  score), which is a game-design question rather than a networking
  one. Do not carry them along "for completeness".

  **Cross-platform dealing is already solved for these five, and
  narrowing the scope is what solved it.** Measured 2026-08-20. A seed
  does not mean the same deal on two compilers -- the standard pins
  what std::mt19937 emits but not how std::shuffle consumes it, so
  libstdc++ and MSVC deal different hands from identical state, which
  this project found the hard way when Canasta's AI ladder passed on
  Linux and failed on the Windows runner with no engine change.

  The fix already exists where it is needed. Hearts and Canasta are
  the only two in-scope games that deal at all, and both go through
  cards/card.cpp's hand-written Fisher-Yates
  (heartsengine.cpp:52, canastaengine.cpp:457). The two remaining
  std::shuffle sites are sudokugrid.cpp and minefield.cpp -- Sudoku
  and Minesweeper, both out of scope. Chess, Reversi and Draughts hold
  one mt19937 each and all three are inside the AI, picking among
  near-equal moves; no AI runs in a human-versus-human game, and
  nothing about it is shared state.

  So the RNG hazard CLAUDE.md documents does not reach this work. It
  would return the moment an out-of-scope game was added, which is one
  more reason the scope line is worth holding.

  **Send moves, not positions.** Chess already saves its game as the
  move list and replays it through ChessGame::play(), which rebuilds
  the board, the undo stack and the threefold-repetition keys from
  one list -- and every move is re-checked against legalMoves() on
  the way in, so a move the build would not play is refused rather
  than half-applied. That is exactly the shape a wire protocol wants,
  and it is the shape to prefer where a game offers the choice.
  Canasta cannot do it (no move log, so its engine serialises
  directly), which is why the two look different.

  **The security cost is real and lands on code that has never seen a
  stranger.** Today restoreState() only ever reads bytes this app
  wrote. Networked play means parsing input from an untrusted peer,
  and GHUB-0052 already records ten hand-audited parsers with nothing
  but hands checking them. Fuzzing those is close to a prerequisite
  rather than a nice-to-have, and SECURITY.md would need to say what
  the app does and does not accept over a socket.

  **A wire protocol is a new breaking surface.** See
  docs/standards/versioning-overrides.md section 1 -- it would join
  the saved game, the settings store and the command line, and
  changing it after two people have installed different versions is
  the thing a version number exists to warn about.

  **Nothing in the tree is networked.** No QTcpSocket, no QNetwork
  anything, and Qt6::Network is not linked. That is a clean start
  rather than a problem, but it means CMakeLists.txt, the AppImage
  bundle and the Windows zip all grow, and the release smoke tests
  would want something to say about it.

  **Pacing is a design constraint here, not a preference.** The
  computer pauses nearly a second on purpose because the owner reads
  a card by its pip pattern and needs time. A human opponent will not
  wait, so no move clock by default, and the on-surface captions
  added under GHUB-0071 -- what was just played, whose turn it is --
  matter more with a person on the other end, not less.

  **Three decisions needed before any of this is scoped, and they
  change the work completely:**

  1. What "local" means -- two people at one keyboard (hot-seat, no
     networking at all, much the cheapest and worth doing first), or
     two machines on a home network.
  2. How two machines find each other over the internet -- a typed
     IP and port forwarding, which is free and most people cannot do;
     or a relay/matchmaking service, which works and is an ongoing
     cost and an operational burden for a project shipped as an
     AppImage and a zip.
  3. Whether a disconnected game is abandoned, resumable, or handed
     to the AI.

  Suggested order if it goes ahead: hot-seat first (proves the games
  can take a second human at all, no network, no security surface),
  then direct connection on a LAN, then the internet question. Each
  of those is its own bullet under this section when it is picked up.
  Filed 2026-08-20 on the owner's request, for later. Not started,
  and the three decisions listed above are what a session picking
  this up should ask before scoping anything.
  **Layman:** Play Chess or Hearts against a real person instead of the computer, whether they are sitting beside you or on the other side of the world, and it should not matter which system either of you uses.
  Kind: feature.
  Source: user-request-2026-08-20.

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

- ✅ [GHUB-0042] **The README promises a window size that nothing says how to judge.**
  Found 2026-08-19 by an adopt-project run — a cold reader given the README,
  CLAUDE.md and SECURITY.md, then the two specs, and forbidden the source.

  The README opens "A small collection of desktop games for Linux and
  Windows, in one window / sized to sit beside whatever you are actually
  working on." Three of the four things the project is for have a real bar
  somewhere: rules correctness is chess perft against published totals,
  legibility is the owner's own eye per game before the next pass starts
  (GHUB-0017 § 10, which says outright that nothing mechanical substitutes),
  and the download is "still alive when the timeout fires" in a container
  with no Qt in it (GHUB-0025 INV-5/6).

  This clause has nothing. No size, no bar, no judge, no way to tell. It is
  in the most prominent sentence the project has, and six documents later
  nobody has said what it would have to be true for.

  Two honest fixes and they are opposite. Write the bar down — a default
  window that fits beside a browser at 1080p, say, checkable the way
  kLegibleMinimum is checked. Or delete the clause, on the grounds that it
  was scene-setting rather than a promise. Deleting is not a cop-out: an
  unjudgeable promise in the first sentence is worse than no promise, and
  the reader who found this said the gap is real rather than hidden in a
  file it was denied.

  Worth knowing about the run that found it: the first pass, given only the
  three root documents, wrongly reported that LEGIBILITY had no stated sign
  of success. It named the spec it had been refused, was given it, and
  overturned its own answer. A candidate list that stops at the repository
  root gets this project wrong, because its bars live in docs/specs/.
  Resolved 2026-08-20. The owner chose to write the bar rather than delete
  the clause. It is `HubWindow::kFitsBesideYourWork` (960x1000) — half a
  1920x1080 desktop across, its height less a panel and a title bar — and
  the README now says so in words: every game can be made as small as half
  a 1920x1080 screen.

  Writing the bar found the promise was false, which is the argument for
  having written it. A QStackedWidget takes the largest minimum size of
  every page it has built, so the tile grid was deciding how small Chess
  could be made: fourteen 190-pixel tiles are five rows deep, and every
  page measured 638x1170 — taller than a 1080p screen, so the window could
  not fit beside anything, or even fit at all. The grid now lives in a
  QScrollArea and asks for nothing. Measured after: the least shrinkable
  game is Canasta at 720x644, or 900x740 with large play on, and the tile
  grid alone is 68x144.

  The check gives each game its own hub, because measured through one
  window every game reports the worst one's floor and thirteen innocent
  games go red together. It also asserts the tile grid never sets that
  floor again, and that a first run opens inside the bar unaided
  (880x680). All four proved able to fail — tightening the bar to 800x600
  reddens three of them, and putting a minimum height back on the scroller
  reddens all four.
  **Layman:** The README's first sentence makes a promise about the window fitting beside your work, and nothing anywhere says what would count as keeping it.
  Kind: doc.
  Source: adopt-project-2026-08-19.

- ✅ [GHUB-0077] **This project now says which version number to pick, and what would make it 1.0.**
  docs/standards/versioning-overrides.md, gated with review-contract
  --genre standard: 3 loops, 3 cold lanes each, 18 verified findings
  all fixed.

  It is a case-2 OVERRIDE, not a project versioning standard. The
  machine-wide ~/.claude/standards/versioning.md was written the same
  day and owns which level to bump, the 0.x shift, the security
  carve-out, the changelog tests and the -rc.N spelling. This file
  answers only the two questions that standard deliberately refuses
  to answer for a project -- what a breaking change can break here,
  and what would make this 1.0 -- plus the local facts a conformer
  needs. A first draft restated the global rules and was cut.

  The 1.0 bar (owner, 2026-08-20): the release that ships the last of
  six items IS 1.0.0 -- nothing a player does can lose a saved game,
  and a published build can be checked against what built it.
  GHUB-0067, 0075, 0054, 0050, 0031, 0053. New games do not gate it.

  The breaking surfaces are the saved game, the settings store, the
  command line including what --version prints, keyboard shortcuts,
  and -- because packaging is coming -- install targets and the
  configure-time prefix contract.

  Three things the gate found that nothing else would have. The save
  rule was keyed on the STAMP moving when the dangerous case is
  changing what saveState() writes and leaving the stamp at 1. The
  --version output line turned out to be a contract already, asserted
  by both CI legs, with nobody having written it down -- while the -v
  alias is exercised by nothing. And gh release create carries no
  --prerelease at all.

  The cap was violent: all five of loop 3's findings landed on text
  the run itself wrote. Recorded in the loop log with the reason --
  the document was still being authored during its own gate, so each
  loop read largely new text. Not a size problem at 139 lines.
  **Layman:** The leading zero in 0.4.0 now means something: there is a written list of what has to be true before version 1.0.
  Kind: doc.
  Source: user-request-2026-08-20.

- ✅ [GHUB-0078] **The bump recipe now says why the save-format versions are not in it.**
  ~/.claude/standards/versioning.md section 7 requires an independent
  version line to be absent from the release recipe AND for the
  recipe to say so, in a $-prefixed comment key the recipe format
  defines as ignored -- because an unexplained absence reads as an
  omission and the next person auditing lockstep adds it.

  .claude/bump.json now carries $note_save_versions: ten games stamp
  their own quint32 save-format version, those move on a different
  clock from a release, and a release that walked them would silently
  discard every player's saved game.

  Verified after the edit: post_check still reports version 0.4.0
  consistent across CMakeLists.txt, README.md and CHANGELOG.md.
  **Layman:** A note stops someone helpfully adding ten numbers to the release checklist that must never be there.
  Kind: chore.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0079] **Nothing points a releaser at the versioning standard, so the one document that decides the number is unfindable.**
  CLAUDE.md's Releasing section is where a releaser looks. It gives
  four numbered steps starting "Bump project(gameshub VERSION ...)"
  and says nothing about WHICH number to bump. So the standards
  written today -- ~/.claude/standards/versioning.md and
  docs/standards/versioning-overrides.md -- are reachable only by
  someone who already knows they exist.

  The consequence is concrete and counter-intuitive, which is why a
  pointer is not enough on its own and the line has to say it:
  inside 0.x the global standard shifts the levels down one, so a
  release adding a whole new game bumps the PATCH. This project's
  history does not follow that -- 0.4.0 added the legibility switch
  and would have been 0.3.2 -- and nothing in the tree would tell
  the next releaser.

  NOT done in the session that wrote the standards, deliberately.
  CLAUDE.md rule 14's test was applied and it comes back YES: a
  conformer reading the amended Releasing section picks a different
  number than one reading it today, and the line that changes can
  be named. So the edit owes a review-contract gate, and that
  session had already run six loops across two documents. Filing it
  rather than either skipping the gate or spending a fourth on a
  two-line pointer.

  Small when picked up: amend the Releasing section to name both
  standards and state the 0.x consequence in one sentence, then
  gate as a standard.
  Confirmed live while cutting 0.5.0 (2026-08-31). The Releasing
  section's five steps say to bump CMakeLists and README and never say
  to WHICH number; the answer came from docs/standards/versioning-overrides.md
  plus the global standard, neither of which this file names. The
  release was breaking (canastaview.cpp's save stamp moved 4 to 7,
  which section 1 of the overrides makes a breaking surface), so under
  the 0.x shift it was 0.5.0 and not the 0.4.1 the step list invites.

  A second edit belongs in the SAME gate, since it lands in the same
  section. cut-release reads an ID on a changelog entry's BULLET LINE
  as a claim that the item shipped, and stops the release when the
  roadmap still has it open — GHUB-0065 hit exactly this. An ID in the
  entry's continuation prose is a cross-reference and passes. Nothing
  in the Releasing section says so, and the gate is the only thing that
  catches it. Write both rules at once rather than gating this section
  twice.
  **Layman:** The rules for choosing a version number exist now, but the page someone actually reads before a release does not mention them.
  Kind: doc.
  Source: in-session-2026-08-20.

- ✅ [GHUB-0144] **README denies two things the app already does.**
  It lists Sudoku among the games that do not resume, and Sudoku has
  saved and restored since GHUB-0009 -- so the count above that list
  is wrong by one too. It also says the other twelve games ignore the
  legibility switch, which GHUB-0071 closed for all fourteen. Both are
  the document's side, not the code's.
  Resolved (2026-09-02): and the count was wrong by TWO, not one.
  Twelve games implement saveState/restoreState, not ten -- the list
  omitted Hearts as well as Sudoku, and Hearts has had a save with its
  own pack-and-trick-count check for some time. Verified by reading
  the overrides rather than the prose.

  Both passages are now written by EXCEPTION rather than by count --
  "almost every game", "the two that do not are Snake and Pinball" --
  so adding a fifteenth game cannot silently make them wrong again,
  which is how this one went stale.

  The legibility paragraph now says every game answers the switch and
  names the three that answer in a shape other than a caption: Canasta
  by its window size, Sudoku by its pencil marks, Pinball by growing
  the backglass.
  **Layman:** The README says some features are missing that shipped a while ago.
  Kind: doc-fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0145] **Correct the stale claims a lane read past in CLAUDE.md and the specs.**
  The measured card floors and the pinned linuxdeploy fetch were
  corrected as part of the audit. Left: CLAUDE.md still says the chess
  engine searches on the GUI thread, which GHUB-0047 moved to a
  worker, and it is the stated justification for the node budgets, so
  anyone tuning planFor reads a constraint that no longer exists.
  GHUB-0025 says one matrix job where ci.yml has two, and its section
  4.6 Windows snippet omits the -NoNewWindow and MainWindowHandle
  checks its own prose requires -- an implementer copying that block
  would re-ship the failure loop 6 was run to catch.
  **Layman:** A few notes describe how things used to work.
  Kind: doc-fix.
  Source: review-code sweep 2026-08-31.

- ✅ [GHUB-0146] **Canasta's discardCannotBeTaken comment is inverted.**
  The header describes it as though it were named discardCanBeTaken:
  it says the fourth seat is the one this returns true for, and the
  body returns FALSE for the fourth seat and true for the three before
  it. Both live callers follow the code and are correct, so this is
  the document's side -- but it is a loaded gun for the next author of
  an AI rule, in exactly the place CLAUDE.md already flags as the
  hardest quarter of a bug to notice.
  Resolved (2026-09-02): the whole comment block was inverted, not
  just the tail the finding quoted. Its opening two lines -- "whether
  what the current seat throws CAN be taken" and "FALSE only while the
  rule bars the seat that plays next" -- are the same inversion, so
  fixing only the sentence about the fourth seat would have left the
  block contradicting itself.

  Verified against the implementation: kSeats is 4 and the body is
  noMeldingFirstRound && m_turnsTaken + 1 < kSeats, so the first three
  seats get true and the fourth false.

  No test was needed. canastaFirstRoundSafeThrow already drives four
  real turns and asserts exactly { true, true, true, false }, so the
  polarity was locked the whole time -- it was only the prose that
  said otherwise. The comment now names the inverted reading as the
  trap and says what it costs: right three times a round, wrong on the
  fourth.
  **Layman:** A comment says the opposite of what the code does, in the trickiest corner of the rules.
  Kind: doc-fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0161] **Decide whether this project localises, because nothing here says.**
  Eight of the nineteen review lanes reported it independently: a
  project-wide search for tr( under src/ returns nothing, and every
  user-visible string is a bare QStringLiteral.
  ~/.claude/standards/languages/qt.md requires tr() around every
  user-visible string from the first commit, and no override is
  recorded here -- so either the strings need it or the override does.
  What makes it more than a style question: canastaview.cpp syncs its
  toolbar after a restore by comparing QAction::text() against those
  same untranslated literals, so retrofitting tr() as the standard asks
  would break that sync with no compile error and no crash -- just ticks
  that stop matching. Whichever way this goes, that comparison should
  stop matching on text first.
  **Layman:** Every visible word in the app is written in a way that cannot be translated, and no note says whether that is on purpose.
  Kind: investigate.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0162] **Sundry small defects in the shared code and the tooling, kept so they are not lost.**
  CARDS: the face cache is bounded at 160 ENTRIES rather than bytes
  (~40MB full, ~160MB at devicePixelRatio 2); its key is computed from
  the padded size, so two distinct card sizes can collide; the pixmap
  caches are function-local statics destroyed after QGuiApplication;
  cardflight's faceUp field is honoured by no consumer -- all three
  users draw face-up unconditionally; makeDeck guards a negative joker
  count and not a negative deck count.
  SOUND: QSoundEffect::status() is never read, so a missing WAV and the
  empty-.qrc catastrophe are both silent at runtime; the offscreen probe
  is an exact string match and misses offscreen:enable_fonts.
  DONATE: a --game launch with an unknown name suppresses the prompt
  though no game opened; QDesktopServices::openUrl has no scheme check
  (the URL is generated from FUNDING.yml, so low risk).
  PYTHON: legibility-check.py's grep -v kFaceMinWidth drops WHOLE lines,
  the shape the file rejects by name three lines above; its QColor regex
  matches only the call form while the rest of the tree brace-
  initialises, which is how it shipped a plausible wrong number once
  already; it hardcodes the ternary polarity it claims to read from
  source, so an inverted inkFor would go undetected; its tile list is
  never checked against the case labels; scorepad-check.py never checks
  its matched band count against the array length; make_sounds.py writes
  a structurally valid WAV for a silent recipe.
  **Layman:** The last of the small findings, written down rather than forgotten.
  Kind: doc-fix.
  Source: review-code sweep 2026-08-31.

## P03 — Considered

Nothing here is agreed. 💭 means the scope, the value or the decision is still
open.

### 🖥 Legibility and accessibility

- ✅ [GHUB-0017] **The other thirteen games have had no legibility pass.**
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
  0.4.0 cut (2026-08-19), which settles the release decision recorded above.
  The condition it named - "cut once one or two more passes land, or reword
  the changelog entry" - was met both ways: Canasta and Sudoku both landed,
  and the switch's changelog entry now says it is read by those two rather
  than by the whole collection.

  The shipped parts of this bullet were split out first so the release could
  credit finished work: GHUB-0037 (the switch, the hook, kFaceMinWidth and
  2048's ink), GHUB-0038 (Canasta's pass) and GHUB-0039 (Sudoku's pass).
  This bullet stays open for the twelve games still to come. Crediting an
  in-progress umbrella in a changelog reads as a claim the whole thing
  landed, and cut-release stops on it - correctly.

  Published: both artifacts on the releases page, AppImage 49.1 MB and
  Windows zip 61.8 MB, each smoke-tested by the workflow. The published
  AppImage was downloaded here afterwards and confirmed to report 0.4.0 and
  to still be running after six seconds under the offscreen platform.
  Count correction 2026-08-20. The headline says thirteen and it is now
  twelve — Sudoku's pass (GHUB-0039) shipped after this bullet was
  written, alongside Canasta's (GHUB-0038). The headline is a store
  column and cannot be amended in place, so the number lives here.
  Found by a cold lane reviewing CLAUDE.md, which carried the same stale
  figure.
  Closed (2026-08-20) by GHUB-0071, which carries the last twelve
  passes. All fourteen games now change what they paint when the
  switch moves, and a uitest block walks every game the hub can open
  and asserts it — so the umbrella is not closed on a count of
  bullets but on a check that fails if a fifteenth game arrives
  without a pass.

  Two adjacent defects fell out of writing that check and are filed
  separately rather than folded in: GHUB-0072 (Canasta's switch was
  one-way inside the hub, invisible to a bare-view test) and
  GHUB-0073 (Pinball never stopped its ball on deactivate).

  The headline is a store column and still reads "thirteen"; the
  count correction note above already records that it became twelve
  when Sudoku shipped, and it is now zero.

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

- ✅ [GHUB-0040] **Canasta says why a move was refused in the status bar, which the owner never looks at.**
  Raised with the owner 2026-08-19 while adding the first-round notice
  and the opening minimums; not answered, so it is filed rather than built.

  The owner said plainly: "In many apps the status bar is very, very useful
  but for games, I don't think so (at least not for me). With a game my
  focus is on the play area and thus I never look at the status bar." He is
  also partially sighted, so scanning to the window edge and back is a real
  cost rather than a glance.

  What that makes a defect rather than a preference: CanastaView::announce()
  puts its text in m_message, which refresh() folds into the statusChanged
  signal and nowhere else. That text is where every REFUSAL lands - "The
  pile is frozen: you need two matching cards from your hand", "Your side
  needs 90 to open, and that is only 50", "You have no way to use the 7 on
  top". Those sentences are the most valuable text the game produces,
  because they are the only thing that explains why a click did nothing,
  and they are delivered exclusively to the one place he does not read.

  Both of this session's display additions went on the table for this
  reason - the first-round notice on the centre strip and each side's
  opening minimum on its score plate - so the pattern and the space are
  already there. paintCentreStrip() is the model: it spells the last
  discard out in words under the middle of the table.

  Design note, not decided: an error needs to persist long enough to be
  read slowly and then get out of the way, which is a different lifetime
  from the centre strip's "until the next card replaces it". A timed panel
  near the hand, or the strip switching to the message and back, are both
  plausible. Worth asking him which reads better rather than choosing.

  Scope is Canasta only as filed. The other thirteen games route their own
  text the same way, so if this lands well it becomes the pattern - but
  that is a second item, not this one.
  Resolved 2026-08-19. The two open design questions were put to the owner
  and both answered: the message gets its own panel above the hand rather
  than taking the centre strip over, and it holds until his next move
  rather than timing out.

  What landed. CanastaView::messageRect() places a near-opaque amber plate
  directly above the hand — above the Lay down button when that is showing,
  since the message is usually the reason the button refused — and
  paintMessagePanel() draws it, word-wrapped rather than elided, because
  the half of a refusal that says what to do instead is at the END of the
  sentence. The amber is kAlert, named once and shared with the centre
  strip's first-round warning, which had the same literal.

  The lifetime change is the smaller diff and the larger fix.
  mousePressEvent() cleared m_message before it had worked out what had
  been clicked, so ANY click wiped the explanation — including the clicks
  you make while acting on it. It now clears only where a move actually
  succeeds: the four human moves, and the deal of a new hand.

  Scope stayed Canasta, as filed. The other thirteen games route their
  text the same way and the panel is now the pattern to copy, but that is
  a second item.

  One trade taken knowingly: the panel sits in the lower part of your meld
  band, which is where the Lay down button already sits, so with a full
  band it covers the bottom of a melded card while it is showing. It is
  transient and it is the most important thing on the table while it is up,
  so it wins that lane.

  Checked: six new UI checks in tests/uitest.cpp, each proved able to fail
  — restoring the old clear-on-any-click reddens the idle-click check and
  nothing else, and removing the clear from a successful discard reddens
  the clearing check and nothing else. Frame captured and read by eye
  before and after; 2,384 pixels changed.
  **Layman:** When the game refuses a move it explains why in a place he never looks; the explanation should be on the table.
  Kind: accessibility.
  Source: in-session-2026-08-19 (owner: "with a game my focus is on the play area and thus I never look at the status bar").

- 📋 [GHUB-0069] **Every card move needs a drag, except the one move that does not.**
  Klondike and FreeCell already answer part of this: a double-click sends a card
  to its foundation, which is the most frequent move in both games and the one
  players most resent dragging. Everything else — a run between tableau columns, a
  card into a free cell, a Spider sequence onto another column, a Pyramid pairing
  — is drag-only. Press, hold, travel, release, and if the release lands wrong the
  run goes back where it came from.

  A press-hold-drag over a long distance is a fine interaction for someone with a
  steady hand and a clear view of both ends of the journey, and a poor one
  otherwise. This project already treats that as a design constraint rather than a
  preference everywhere else — it is why melds are drawn large, why the computer
  pauses long enough to follow, and why the last discard is spelled out in words.
  The input side has not had the same attention.

  Click-to-select then click-to-place, alongside dragging rather than instead of
  it. The selected run lifts exactly as it does mid-drag, the legal destinations
  can be marked while it is held, and the second click completes or a click
  elsewhere cancels. Chess already works this way — `ChessView` highlights
  destinations on selection — so the interaction exists in the codebase and the
  question is bringing the card games in line with the board games.

  It costs nothing to keep drag working, and the two can share a path: a drag is
  already a lift plus a drop, and this makes the lift and the drop independent of
  whether the button stayed down. Whichever game gets a rules core first under
  GHUB-0066 is the natural place to try it, since the lift/drop logic is exactly
  what that extraction has to pull out of the mouse handlers anyway.
  **Layman:** You have to drag cards with the mouse held down; only sending a card to a foundation can be done with a double-click.
  Kind: accessibility.
  Source: in-session-2026-08-20.

- 💭 [GHUB-0070] **Nothing in the app has a name a screen reader could read — recorded, with an honest doubt about whether it is wanted.**
  There is not one `setAccessibleName`, `setAccessibleDescription` or
  `QAccessible` anywhere in `src/`. Qt gives standard widgets — the toolbar, the
  menu, the tile buttons — some default accessibility, but every game is a
  custom-painted `QWidget` that draws itself and exposes nothing. To assistive
  software the play area is an empty rectangle.

  **Filed as considered rather than planned, and the honest reason is that it may
  not be the right work.** The owner is partially sighted and reads the screen;
  every accessibility decision in this project so far — card size, pip patterns,
  named discards, pacing — has been about making the picture readable, which is a
  different axis from making it audible. Building a screen-reader surface nobody
  here uses would be effort spent away from GHUB-0017's twelve outstanding
  legibility passes, which are known to matter.

  What argues the other way is that the app is now published to strangers, with
  OBS and Flathub queued, and some of these games are genuinely playable without
  sight: Chess, Draughts, Reversi, Sudoku and 2048 are all grid games with a small
  vocabulary of announcements. Card games mostly are not. So the honest scope is
  never "make the collection accessible" but "five grid games could announce their
  state," which is a much smaller and more truthful proposition.

  The cheap part is worth doing whatever is decided about the rest: naming the
  toolbar actions and the fourteen tiles costs almost nothing and makes the hub
  navigable even if no game ever is. Do that much; leave the rest until somebody
  asks.
  **Layman:** Screen-reader software would find the games completely blank; whether that matters here is a real question, not an assumption.
  Kind: accessibility.
  Source: in-session-2026-08-20.

- ✅ [GHUB-0071] **The last twelve games answer the legibility switch.**
  Closes the GHUB-0017 umbrella: Chess, Reversi, Draughts,
  Minesweeper, Klondike, Spider, FreeCell, Pyramid, Hearts, Snake,
  2048 and Pinball, in one pass.

  The finding that shaped it: NINE of the twelve drew no text on
  their own play surface at all — not the score, not whose turn it
  is, not "game over". Everything they said went to the hub's
  status bar, which the owner does not read during play. Card SIZE
  was already measured as a non-issue for these games (the floors
  are unreachable), so what was left really was "what a game says
  out loud", exactly as the umbrella predicted.

  So the shared piece is a CAPTION rather than a per-game tweak.
  GameView::captionText() defaults to the status line each game
  already composes, so a pass is three lines in paintEvent instead
  of twelve copies of a block; Theme::paintCaption draws the plate;
  GameView::captionBand reserves the strip so a board SHRINKS by
  exactly what the sentence takes rather than being covered by it.
  Chess, Draughts and Hearts override the text — Hearts names the
  suit that was led, which existed nowhere in that game before and
  decides every legal play you have.

  The band is a fixed two lines, never the height of the current
  sentence: a band that tracked the text would resize the board
  every time the text changed length. It is also capped at 22% of
  the surface, and that cap is not tidiness — fm.height() is a
  platform property, windows-2022 under offscreen has no font
  environment, and an uncapped band could drive a card below
  CardArt::kFaceMinWidth on a runner and nowhere else.

  Size work where it was earned rather than everywhere: Minesweeper
  neighbour counts 0.46 to 0.58 of a cell, Chess frame coordinates
  0.20 to 0.28, both with Qt::TextDontClip because drawText clips
  to its rect; Chess and Draughts last-move washes from alpha 60-70
  to 150; Pinball's backglass and both its labels together, since
  it is the one game that already spoke on its own surface. 2048's
  tile digits are SOLVED against the font in use rather than
  scaled by a ratio, per the standing note Sudoku's three red
  Windows legs bought.

  GameView::smallestCardWidth() is what makes GHUB-0017's withdrawn
  INV-3 writable at last — it was withdrawn because cardWidth() is
  private on all six card views. One line per game, and one check
  now holds all six against kFaceMinWidth at their smallest window
  with the switch on: Klondike 67.9, FreeCell 67.4, Spider 54.2,
  Hearts 52.5, Pyramid 52.1, Canasta 46.4. Only Pyramid pays for
  its band (68.6 to 52.1) and it still clears by 13%.

  The check with the real teeth walks every game the hub can open,
  renders it with the switch off and on, and asserts the picture
  changed and then went back — so a fifteenth game added without a
  pass reddens rather than shipping silently. Sudoku is excluded by
  name with the reason (its pass grows pencil marks and a fresh
  board has none; its own block asserts it). Twelve new uitest
  assertions, each seen red against a deliberate break.

  selftest 398/398, uitest 183/183, ctest 3/3, local-ci green.
  **Layman:** Every game in the collection now changes when you turn Large play on, not just Canasta and Sudoku.
  Kind: accessibility.
  Source: in-session-2026-08-20.

- ✅ [GHUB-0072] **Canasta's legibility switch was one-way inside the hub.**
  Found by the new every-game check, which measures each game
  through a real HubWindow. CanastaView::applyLegibility lowers its
  own minimum and then resizes the window back to the size it had
  before the switch — but the hub's minimum is computed from its
  central widget THROUGH a QStackedWidget, and that chain is
  recalculated lazily, so the resize was clamped straight back up
  by the stale figure. The view stayed at 900x656 instead of
  returning to 720x560.

  Why it survived GHUB-0038's own reversibility check: that check
  uses a bare CanastaView, where window() is the view itself and
  the stale-minimum chain does not exist. The bug was only ever
  present in the hub — the only place a player sees it.

  The fix walks up to the window activating each layout before the
  resize. Proved by removing it again and watching the new check go
  red.
  **Layman:** Turning Large play off in Canasta left the window stuck at the bigger size.
  Kind: fix.
  Source: in-session-2026-08-20.

- ✅ [GHUB-0073] **Pinball kept its ball rolling after you left the table.**
  GameView::deactivate() exists precisely so a game with a clock or
  an animation stops it when the hub moves on — its own comment
  names the cost, a Minesweeper time you never spent. PinballView
  had no override at all, so the physics timer ran on a table
  nobody was looking at and a ball could drain unseen.

  Surfaced by the new every-game legibility check, which deactivates
  each game before rendering it: Pinball was the one game that would
  not hold still, and three renders spread over time were needed to
  catch it — two in a row can land between physics ticks and match.

  The check now ASSERTS that no game moves after deactivate(),
  rather than reporting it, so the next game to forget this is
  caught. Seen red against the override removed again.
  **Layman:** Switching to another game left the pinball still in play, and it could drain while you were not looking.
  Kind: fix.
  Source: in-session-2026-08-20.

- ✅ [GHUB-0081] **Nobody has looked at the twelve new legibility passes, and one of them is the risky one.**
  GHUB-0017's spec says every per-game pass is judged BY EYE (§ 3.2,
  § 9). GHUB-0071 shipped twelve of them in one session and none has
  been seen. Everything asserted about them is measured -- card widths
  against kFaceMinWidth, contrast ratios, font solves, off/on/off
  renders -- and measurement cannot answer "can he read this".

  **Hearts is the one to look at first, and the reason is
  structural.** Ten games reserve a caption band and shrink the board
  by it. Hearts does not: its hand is anchored to the bottom, so a
  band would come off the cards instead, and its caption goes in the
  gap that already exists between the trick and the hand. That gap is
  generous at a comfortable window and tightest at the minimum (620 x
  480), where the trick cards and the hand are closest together. If
  the caption overlaps the trick anywhere, it is there.

  Canasta is second, for the opposite reason: it sits 900 wide against
  the 960 kFitsBesideYourWork bar with 60 px of headroom, and its
  melds clear the face-width floor by 0.4 px.

  The rest are low risk -- a caption in a reserved band, or a font that
  was solved rather than tuned -- but "low risk" is a measurement
  talking again.

  What would close this: open each of the twelve with the switch on
  and off at a small window and at a comfortable one, and say which
  look wrong. It is looking rather than typing, and it is the half of
  GHUB-0017 that a session cannot do for him.
  Resolved (2026-08-21): looked at. All fourteen were opened through a
  real HubWindow -- not a bare view, per the GHUB-0072 trap -- with the
  switch off and on, at each game's own minimum and at 1120x820, plus
  wide-and-short shapes once Hearts pointed that way.

  Six confirmed defects, filed as GHUB-0082 through GHUB-0085 and
  GHUB-0088; one pre-existing overflow the switch did not cause as
  GHUB-0086; the board-shrink trade as GHUB-0087 for the owner to decide.

  Two things this look settled that measurement had not. **The prediction
  in this bullet was wrong about where Hearts breaks** -- the 620x480
  minimum is clear by about 39px, and the caption reaches the trick on
  WIDE, SHORT windows instead (GHUB-0084). And **Canasta, called the
  second risk here, is clean**: it behaves exactly as CLAUDE.md
  documents. The risky one turned out to be Pyramid, which this bullet
  listed among "the rest are low risk".
  **Layman:** The twelve games changed today were checked by measurement, not by eye. Someone has to actually look at them.
  Kind: accessibility.
  Source: in-session-2026-08-20.

- ✅ [GHUB-0082] **Pyramid and Spider draw their stock pile inside the caption band, and the plate hides it.**
  Found by looking (GHUB-0081), not by any test. Both games solve card
  width against `height - ... - captionBand()` correctly, so the CARDS
  shrink -- and then anchor the stock to the bottom of the WIDGET, which
  is inside the band the card size just reserved. The caption plate is
  opaque and painted last, so the pile is drawn and then covered.

  Pyramid at 600x544 with the switch on: the stock AND the waste are not
  on screen at all, while the caption reads "Stock 24". At 1120x820 the
  plate cuts both piles in half. Spider at 620x524: the stock is gone
  bar a white sliver of card edge past the plate's corner.

  The fix is arithmetic -- subtract `captionBand(QRectF(rect()))` from
  the bottom anchor in each:
    src/pyramid/pyramidview.cpp:228 `stockRect()`
    src/pyramid/pyramidview.cpp:234 `wasteRect()`
    src/spider/spiderview.cpp:254 `stockRect()`
  No other game bottom-anchors anything; the six board games all centre
  within `height - band` and are clear. Checked by grep, not assumed.
  Resolved (2026-08-21): both anchors now subtract
  captionBand(QRectF(rect())). Pyramid gained a private pileTop()
  shared by stockRect() and wasteRect(); Spider's stockRect() takes
  the band off its bottom edge before the margin. cardWidth() was
  already correct in both and was not touched.

  The regression check is pilesClearTheCaptionPlate in
  tests/uitest.cpp. It mirrors the geometry each pile is SUPPOSED to
  have -- bottom minus band -- clicks it, and asserts the stock count
  in the game's own status went down: Pyramid 600x544 Stock 24 -> 23,
  Spider 620x524 Stock 5 -> 4. Proved red by reverting both fixes and
  re-running: both clicks land on empty table and both checks fail,
  with the two band assertions still passing, so the failure is the
  anchor rather than the switch. Confirmed by eye through the harness
  at 600x544 and 1120x820 with the switch on.

  Not fixed here, and pre-existing with the switch OFF: Pyramid's
  stock overlaps the bottom row of the pyramid at its minimum size.
  Same in both states, so it is not a legibility defect.
  **Layman:** At their smallest window these two games hide the pile you draw from behind the caption.
  Kind: fix.
  Source: in-session-2026-08-21 GHUB-0081 eyeball check.

- ✅ [GHUB-0083] **FreeCell's caption covers the bottom card of five of its eight columns.**
  At 620x524 -- FreeCell's OWN minimum, on a freshly dealt board with no
  play at all -- the caption plate covers the last card of five of the
  eight columns. In FreeCell the bottom card of a column is the only one
  that can be moved, so the plate is hiding exactly the cards the game is
  played with.

  Not the same bug as Pyramid and Spider. Nothing is bottom-anchored
  here; the card size is solved against a height budget of
  `(height - 2*kMargin - captionBand()) / (1.4 * 2.5)` at
  src/freecell/freecellview.cpp:203, and that budget leaves room for a
  header plus about two cards of fan. FreeCell deals seven, all face up.

  Klondike has the same shape with a 2.6 divisor (klondikeview.cpp:245)
  and is filed separately -- it clears a fresh deal by about six pixels
  and stops clearing as soon as you play.
  Resolved (2026-08-21): one fix with GHUB-0086, as filed.
  FreeCell's height budget was a flat `/(1.4 * 2.5)` and is now solved
  from the layout it has to hold -- the cell/foundation row, the gap
  under it, six fan steps and one whole card, which is 3.84 card
  heights against the 2.5 assumed. Klondike's flat 2.6 became 2.94 the
  same way. The ratios those sums are built from (0.22 and 0.27 in
  FreeCell, 0.14 / 1.6 / 0.13 / 0.28 in Klondike) moved into named
  constants in each header, so the fan and the budget can no longer
  drift apart -- which is how the deal came to be taller than the space
  reserved for it.

  FreeCell's smallest card falls from 67.4 to 59.4 px, still clear of
  `CardArt::kFaceMinWidth`; CLAUDE.md's measured list is updated, and
  FreeCell now joins Pyramid as a game the band actually costs.
  Klondike is unchanged at 67.9 -- width-bound at its own minimum.

  `dealtColumnsFitTheTable` in tests/uitest.cpp is the regression
  check: four window shapes x both switch states x both games, with
  the deal's height stated independently of the view. Proved red by
  reverting both fixes -- 7 of the 16 fail, among them the exact two
  reported symptoms (FreeCell 620x440 switch on, FreeCell 1400x438
  switch off). Confirmed by eye through the harness.

  What is NOT fixed: a column that grows past the deal. Filed as
  GHUB-0089 with the measurement and the price of the two ways out.
  **Layman:** With the switch on, FreeCell hides the only cards you are allowed to move.
  Kind: fix.
  Source: in-session-2026-08-21 GHUB-0081 eyeball check.

- ✅ [GHUB-0084] **Hearts' caption covers your own played card on wide, short windows.**
  GHUB-0081 predicted that if Hearts' caption ever reached the trick it
  would be at the 620x480 minimum, where the trick and the hand are
  closest. **That prediction is wrong and the minimum is fine** -- it
  clears with about 39px to spare. It goes wrong on WIDE, SHORT windows.
  Measured at 900x600 and 1400x620, both with a full trick on the table:
  the plate covers the seat-0 card, which is the one YOU played, while
  the other three stay visible. The caption reads "You led spades" while
  sitting on the spade.

  Why that shape: `cardWidth()` is capped at 92px, so past a certain
  width the trick stops moving down the window while the caption, which
  is bottom-aligned in `QRectF(0, 0, width(), height() - cardHeight() -
  16)` (src/hearts/heartsview.cpp:425), keeps rising to meet it.

  Hearts deliberately reserves no band and must not be given one -- its
  hand is bottom-anchored, so a band comes off the cards. Clamping the
  caption's bottom to the top of the trick is the shape that fits.
  Resolved (2026-08-22): HeartsView::captionArea() takes the seat-0 trick card's bottom as its top edge, reserved whether or not a card is sitting there so the plate does not hop up the window each time a trick is swept. Measured before: plate top 397.6 against a trick bottom of 403.9 at 900x600, and 411.3 against 419.2 at 1400x620. After: 407.1 and 420.9, clear at both. heartsCaptionClearsTheTable in tests/uitest.cpp asks the view's own rects at four window shapes rather than a mirror of its arithmetic, and asserts no overlap only where the gap is tall enough to hold the plate -- where it is not, the caption overlaps a little, which is the same trade the capped band already makes and the only thing it can be on a runner with no font environment.
  **Layman:** In Hearts the sentence explaining the trick can sit on top of the card you just played.
  Kind: fix.
  Source: in-session-2026-08-21 GHUB-0081 eyeball check.

- ✅ [GHUB-0085] **Hearts' lifted pass cards run under the caption plate.**
  A card chosen for the pass lifts by `h * 0.18` to show it is chosen
  (src/hearts/heartsview.cpp:317). The caption area is measured from the
  UNlifted hand top at :425, so all three chosen cards rise into the
  plate and the top of the gold selection highlight is cut off.

  Cosmetic rather than blinding -- the sides of the outline still read --
  but it is the one moment in Hearts where the highlight is the whole
  point. One term to add.
  Resolved (2026-08-22): captionArea() subtracts the pass lift, now the named constant kPassLift, so the plate clears a card chosen for the pass. Found while proving the check red: clicking a card's centre lands on whichever card is drawn on top of it, so the first version of this check was vacuous at three of the four window shapes. It clicks the left sliver of each card instead, and asserts a card actually lifted before asserting the plate clears it.
  **Layman:** The cards you pick to pass rise up behind the caption and lose the top of their gold outline.
  Kind: fix.
  Source: in-session-2026-08-21 GHUB-0081 eyeball check.

- ✅ [GHUB-0086] **FreeCell and Klondike columns run off the bottom of the window at wide, short shapes.**
  Found while looking at GHUB-0081 and **not caused by the legibility
  pass** -- it reproduces with the switch OFF, which is what separates it
  from the caption items above. At 1400x520 (a legal size: FreeCell's
  minimum is 620x524 and the height clamps to 524) only five of each
  column's seven cards are on screen; the rest are below the widget edge.

  Same root as the FreeCell caption item -- the `/(1.4 * 2.5)` height
  budget assumes a shorter column than the game deals -- so the two are
  probably one fix. Filed separately because the caption is the symptom
  and this is the disease, and because a fix aimed only at the caption
  would leave this standing.
  Resolved (2026-08-21): fixed with GHUB-0083 -- one fix, as filed.
  The `/(1.4 * 2.5)` and `/(1.4 * 2.6)` height budgets are now solved
  from the layout each game lays out, so at 1400x520 every one of
  FreeCell's seven dealt cards is on screen with the switch off, which
  is the state this reproduced in.

  The regression check covers the switch OFF as well as on, so the
  disease is asserted and not only the symptom: `dealtColumnsFitTheTable`
  fails at 1400x438 with the switch off when the fix is reverted.
  **Layman:** Make the window wide and short and FreeCell's columns fall off the bottom edge -- with or without the legibility switch.
  Kind: fix.
  Source: in-session-2026-08-21 GHUB-0081 eyeball check.

- 💭 [GHUB-0087] **The six board games shrink the board by a fifth to print a caption the status bar already carries.**
  Owner's call, not a defect. Measured at each game's own smallest
  window, board height lost to the reserved band:
    Reversi 268 -> 204 px (23.9%)   Minesweeper 268 -> 204 px (23.9%)
    Draughts 308 -> 244 px (20.8%)  Snake 268 -> 214 px (20.1%)
    Chess 348 -> 284 px (18.4%)     2048 268 -> 245 px (8.6%)

  What is bought with it is a sentence that is already on screen one line
  below, in the status bar, word for word. On a board game the pieces ARE
  the thing being read. Minesweeper is the sharpest case: its entire
  content is single digits in cells, and the switch shrinks the cells by
  a quarter to print the mine count the status bar was already printing.

  The counter-example is in the same build. **Pinball takes no band** --
  it grows the two numbers it was already drawing on the backglass, and
  the table gives up a sliver rather than a fifth. Nothing moves and
  nothing is covered. That is the shape worth considering for these six.

  Against it: the caption is far larger than the status bar text, and the
  status bar is not read during play (which is why the captions exist).
  So this is a real trade with a case on both sides, and it has never
  been made deliberately.
  Deferred by the owner (2026-08-22), staying considered rather than planned: he will judge it by eye when he has a chance. **Do not re-raise it as an open question** -- it is not waiting on analysis, and nothing here can decide it. Whether a caption is worth a fifth of the board is a judgement about reading a board slowly, which is the owner's to make and no test's. The picture to judge it on is Reversi at a small window with large play on, which `--shot` (GHUB-0090) will now produce without opening a window:
    QT_QPA_PLATFORM=offscreen ./build/gameshub --shot /tmp/reversi.png --game reversi --size 300x360 --legible
  **Layman:** Turning legibility on makes the pieces smaller -- is that the trade we want?
  Kind: accessibility.
  Source: in-session-2026-08-21 GHUB-0081 eyeball check.

- ✅ [GHUB-0088] **The caption wraps on width and breaks its sentences in the wrong place.**
  At each game's smallest window the caption runs to two lines and breaks
  wherever the width runs out, with no sense of which words belong
  together. Seen by eye at the minimum for each:
    Reversi      "Your turn (Black). You 2 - 2" / "Computer"
    Minesweeper  "Click anywhere to start. Mines" / "left 40"
    2048         "Slide with the arrow keys Score 0" / "Highest 2 Best 0"
    Solitaire    "Klondike Foundations 0/52 Stock 24" / "Score 0"
  Reversi's is the worst: the score is orphaned from whose score it is.

  Spider is the opposite failure at the same size -- the sentence fits on
  one line but overruns its plate, so "Moves 0" sits flush against the
  right border with no padding while the left side has a comfortable
  margin.

  Breaking between the sentence's own phrases would cost nothing and
  reads far better slowly, which is the bar this project holds.
  Resolved (2026-08-22): Theme::wrapCaption breaks the caption at the joints the sentence already has -- the runs of two or more spaces every game separates its status phrases with -- and is what captionRect measures and paintCaption draws, so the plate that is measured is the plate that is drawn. Qt's word wrap stays on underneath, so a phrase wider than the plate still wraps. Spider's dangling separator spaces go with it: a break consumes the run rather than leaving it on the end of a line. The check asserts a property of the function rather than a figure this machine's fonts produce -- no phrase split across two lines, no line with leading or trailing space -- over what the fourteen games actually say at the smallest window each allows. Twelve have a joint to break at; Chess and Hearts caption in a single phrase.
  **Layman:** At small windows the caption splits mid-phrase -- "You 2 - 2" on one line and "Computer" on the next.
  Kind: accessibility.
  Source: in-session-2026-08-21 GHUB-0081 eyeball check.

- 💭 [GHUB-0089] **A tableau column that grows past the deal still fans off the bottom.**
  The residual GHUB-0083 and GHUB-0086 deliberately left standing, and
  it is filed as considered because the obvious fix has a price the
  owner should weigh rather than a session.

  Both games now solve card width against the DEALT board -- FreeCell's
  longest column is seven face-up cards, Klondike's is six face-down and
  one turned up. Play grows a column past that: in Klondike a turned card
  changes its fan step from 0.13 of a card height to 0.28, so a column of
  six down and one up costs 0.78 card heights of fan and the same seven
  cards all face up cost 1.68. Columns also collect cards -- a king and
  its run onto an emptied column is routine.

  Measured at Klondike's own minimum window (560x504, card 67.9 wide,
  width-bound): a fresh deal's deepest column ends about 34 px clear of
  the caption plate, and turning three of its six face-down cards puts it
  about 9 px past it.

  Sizing for the worst case is what this stops short of, and the cost is
  not small: FreeCell's smallest card already fell from 67.4 to 59.4 px
  sizing for a seven-card deal, and sizing for a column that can reach
  thirteen or more would take it well toward `CardArt::kFaceMinWidth` --
  at every window, for every deal, whether or not any column ever grows.
  The owner reads cards by their pip pattern, so that is his call.

  The alternative worth pricing first is a fan step that compresses when
  a column is too long for the table, which is what most solitaires do:
  the cards stay big and only an overlong column tightens. That keeps
  `cardWidth()` where it is and changes `fanStep()` into a function of
  the column's length and the room left.

  `dealtColumnsFitTheTable` in tests/uitest.cpp is explicit that it
  covers the deal and not growth, so this item is not silently covered.
  **Layman:** Klondike and FreeCell size their cards for the board they deal you; pile enough cards onto one column in play and its tail runs past the bottom edge again.
  Kind: fix.
  Source: in-session-2026-08-21 GHUB-0083/GHUB-0086 fix.

- ✅ [GHUB-0092] **Hearts' East pile fans off the right edge of the window.**
  Found by looking, in the first picture GHUB-0090's `--shot` flag ever
  took -- which is the argument for that flag in one line, since the
  GHUB-0081 eyeball check went over this same game and did not catch it.

  `opponentRect(3)` puts the pile's right edge at `width() - 14`, and
  `paintEvent` then fans up to six backs at `r.translated(i * 3.0, i * 2.0)`.
  So the sixth back reaches `width() + 1` and the frontmost card of the
  stack loses its right border. West is drawn from `x = 14` and fans away
  from its edge, so it is fine; East is the only seat that fans towards one.

  Cosmetic and about one pixel wide. Worth doing because it is the seat
  whose card count you read to know how the hand is going, and a card with
  a missing border reads as a rendering fault rather than as a design.

  Reproduce:
    QT_QPA_PLATFORM=offscreen ./build/gameshub --shot /tmp/h.png \
          --game hearts --size 1400x620 --legible
    magick /tmp/h.png -crop 200x260+1200+280 +repage -resize 300% /tmp/east.png
  Resolved (2026-08-22): East fans towards the centre of the table instead of towards the wall -- one sign, in a named fanOffset() the painter and the new opponentStackRect() share. Chosen over moving the pile left because it keeps the existing 14px margin exactly, and because West already fans inwards, so the two sides now mirror and each seat's front card faces the table. opponentStacksFitTheTable in tests/uitest.cpp asserts every seat's full-stack extent lies inside the widget, at four window shapes; put back the old direction and it reddens on seat 3 alone, by exactly one pixel at every size (621 of 620, 881 of 880, 1401 of 1400, 1001 of 1000). No pixel-diff evidence is recorded on purpose: the deal is random per launch, so a before-and-after count measured the shuffle rather than the fix -- 10633 changed pixels against a same-binary control of 6416. GHUB-0093 carries the seed that would make such a comparison mean something.
  **Layman:** In Hearts the stack of cards on the right is drawn slightly too far right, so its edge is cut off by the window.
  Kind: fix.
  Source: in-session-2026-08-22 first --shot of a game.

- ✅ [GHUB-0132] **Give the tiles an accessible name, and the board games a keyboard.**
  GameTile paints its own miniature and never calls setText, and
  setAccessibleName appears nowhere in src/ -- so QAccessibleButton
  has no name to report for any of the fourteen tiles on the page
  every player lands on first. Separately, Chess, Reversi, Draughts,
  Minesweeper, Canasta, Klondike, Spider, FreeCell and Pyramid have no
  keyPressEvent and no focus policy: Canasta's Space and Return
  shortcuts are gated on a selection only a mouse can make, so they
  cannot be reached at all.
  Resolved (2026-09-02) for the tiles; the keyboard half is split out
  as GHUB-0168 and is NOT done.

  Shipped: every tile carries setAccessibleName (the game's registered
  name) and setAccessibleDescription (its blurb), plus a tooltip, which
  costs nothing and helps a sighted reader once the grid scrolls. Two
  checks in uitest compare the names against the registry rather than
  counting them, so a tile named after the wrong game fails too. Proven
  red.

  Split rather than done, and why: keyboard play is a feature with real
  design choices in it -- what the cursor looks like, whether it answers
  the legibility switch, whether the card games get the same model --
  and those are the owner's to make. GHUB-0168 carries it, correctly
  scoped at TEN games rather than the nine listed here: Hearts has no
  keyPressEvent and no focus policy either. Measured across every view;
  only Pinball, Snake, Sudoku and 2048 have both.
  **Layman:** Screen readers see fourteen unnamed buttons, and several games can only be played with a mouse.
  Kind: accessibility.
  Source: review-code sweep 2026-08-31.

- ✅ [GHUB-0133] **Reversi's legal-move hints ignore the legibility switch.**
  draughtsview.cpp raises its hint alpha under the switch and says
  why: 60 is a hint you can look past, which is the wrong thing to be
  for the player the switch is for. Reversi's is a flat alpha 70 at
  radius cell*0.12 -- fainter than the value that reasoning rejected --
  and the hints are its primary play affordance. Its pass notice is
  also overwritten after 320ms, and a pass is the one event that
  leaves the board looking unchanged.
  Resolved (2026-09-02): the dots answer the switch -- alpha 70 to 190,
  radius 0.12 to 0.18 of a cell -- and the pass notice is now held in
  m_passNotice and carried in front of the following status line until
  a disc is actually placed, instead of being replaced 320ms later by
  the next turn's message.

  The measurement says something worse than the finding did. Isolating
  the dots by differencing the board with hints on and off, at one
  geometry: 772 pixels normal against 496 large BEFORE the fix. So
  turning large play on made Reversi's only affordance SMALLER, because
  the caption band comes off the height the board is laid out in and
  the dot is a fraction of a cell. After the fix, 772 against 1104.

  The first version of that check compared the two switch states
  directly and PASSED with the fix reverted -- the band moves the board,
  so every pixel differs whether or not the dots did. It was rewritten
  rather than trusted. The counts are printed and only the relationship
  is asserted, since the band depends on the platform's font.
  **Layman:** The dots showing where you can play stay faint when large play is on.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- ✅ [GHUB-0134] **The donate dialog shrinks its own text on a pixel-sized font.**
  QFont::pointSizeF() returns -1 when the size was set in pixels, and
  donatedialog.cpp adds to it. The legibility branch then produces a
  2.0pt dialog font and the heading an unconditional 3.0pt one. The
  same dialog dims its URL label with QPalette::Dark, a 3D-shadow role
  with no guaranteed contrast against the window, on the one address
  the partially sighted owner is asked to read.
  Resolved (2026-09-02): a growByPoints() helper grows whichever unit
  the font actually carries. QFont holds either a point size or a pixel
  size and answers -1 for the other, so the old arithmetic produced a
  2.0pt dialog and a 3.0pt heading on a pixel-sized font -- the heading
  unconditionally, so that half was wrong for every such reader rather
  than only in the large branch.

  The URL is no longer dimmed at all. It was painted in QPalette::Dark,
  a 3D-shadow role with no guaranteed contrast against the window, on
  the one address a partially sighted reader is asked to read; the
  button above it already carries the hierarchy that was for.

  Three checks in uitest, driven with the application font set in
  pixels and comparing in whichever unit the font carries -- so they
  assert what the code does and borrow nothing from the platform's font
  metrics. Two proven red; the third correctly does not fire with the
  bug present, because the non-large path never reached the broken
  arithmetic, and it is kept as the guard for that path.
  **Layman:** On some systems the Support dialog would come up microscopic, worst of all in the large-text branch.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0147] **The Hearts caption can still sit over the card you played.**
  GHUB-0084 again. captionArea() clamps its own height to zero when the\ngap runs out, but Theme::layoutCaption bails on zero WIDTH and anchors\nthe plate to the bottom of whatever area it gets, growing upward -- so\na gap too small to hold the plate does not shrink it, it puts it over\nthe seat-0 card. cardWidth() caps at 92, so past a certain width the\ntrick stops moving down while the hand stays anchored: at 1900x564,\nthe widest shape the hub allows at its floor, the gap is about 28px\nagainst a plate of about 44.\n\nA trickLift() that measured the plate and raised the trick by the\nshortfall was written, and REVERTED. It reddened the Windows leg\ntwice. The uitest check only requires no overlap once the plate FITS\nthe gap, and the lift made that true at 620x480 and 900x600 where it\nhad been false -- then the plate landed on the card anyway. The\nreported numbers say the lift and trickCardRect disagree about\ngeometry they each derive separately, and none of it reproduces on\nLinux, where the lift never fires at those shapes.\n\nWhoever takes this: measure on both platforms, and do not let the fix\nrecompute seat 0's rect a second time. The wintest box has real fonts\nand is where this shows up; scripts/wintest-ci.sh runs it, but read\nbuild/Testing/Temporary/LastTest.log rather than the console, because\nctest's progress redraw destroys the FAIL line.
  **Layman:** At wide, short windows the sentence on the table can cover your own card, and the obvious fix broke Windows twice.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0168] **Ten games can only be played with a mouse.**
  Split out of GHUB-0132, whose accessible-name half shipped. This is
  a feature with design choices in it, not a sweep fix, and pretending
  otherwise is how it would get built badly.

  TEN, not the nine the sweep listed -- Hearts has no keyPressEvent and
  no focus policy either. Measured across every view: only Pinball,
  Snake, Sudoku and 2048 have both. Canasta, Chess, Draughts, FreeCell,
  Hearts, Klondike, Minesweeper, Pyramid, Reversi and Spider have
  neither, and HubWindow::openGame calls setFocus() on them, which does
  nothing under the default NoFocus policy.

  Two groups, and they are not the same problem.

  The four board games -- Chess, Reversi, Draughts, Minesweeper -- share
  one shape: a cell cursor, arrow keys, Space or Return to act. Sudoku
  already does exactly this and is the pattern to copy rather than
  invent. Tractable, and the bigger win per line.

  The card games -- Klondike, Spider, FreeCell, Pyramid, Canasta -- and
  Hearts are harder, because their input is drag-and-drop and a keyboard
  equivalent needs a source-then-target model that does not exist yet.
  Canasta is the sharpest case: its Space and Return shortcuts are
  already written and are gated on a selection only a mouse can make, so
  they cannot be reached at all today.

  Open for the owner, and worth deciding before any of it is built:
  what the cursor looks like, whether it answers the legibility switch,
  and whether the card games get the same treatment or a different one.
  Doing the four board games first would be a sensible first slice.
  **Layman:** Ten of the fourteen games cannot be played from the keyboard at all, which matters most to the reader this app is built for.
  Kind: accessibility.
  Source: review-code sweep 2026-08-31, split from GHUB-0132.

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

- ✅ [GHUB-0098] **Catching them a minus is the family's rule and the family's word for it.**
  The rule itself already exists as canastaNeededToScore, and
  its tick reads "A side with no canasta counts nothing in its
  favour". Two things are wrong with that as it stands.

  It defaults OFF in the House set, and the owner told us on
  2026-08-24 that it is how his family plays -- so the default
  is wrong for the one rule set that exists to match them.

  And nothing anywhere uses their word for it. The owner calls
  it "catching them a minus", which is both shorter and much
  clearer than the tick's current sentence, and the end-of-hand
  message should say it when it happens.

  The tick's stored key stays as it is; renaming a key silently
  unticks the rule for anyone who has already set it.
  Resolved (2026-08-24): canastaNeededToScore now defaults ON in the
  House set, and its tick reads "a side with no canasta is caught a
  minus: its own melds count against it". When a hand ends on somebody
  playing out, the table says which side was caught, in words rather
  than in the status bar the owner never reads.

  canasta::caughtAMinus() is a free function on the same footing as
  handScoreFor, and canastaCaughtAMinus() checks the two against each
  other -- the phrase and the arithmetic are one claim, so a message
  that contradicted the score is what the test forbids.

  The stored settings key keeps its old spelling on purpose; renaming
  it would silently untick the rule for anyone who had already set it.
  **Layman:** Going out while the other side has no canasta counts their melds against them -- and the game should say so in the words the owner uses.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0099] **The computer never plays for a minus, even when one is sitting there.**
  Follows GHUB-0098, which makes catching a minus the House
  default. The rule being on changes the value of going out
  enormously and the AI does not know it: the opposing side's
  melds and red threes all swing from plus to minus the moment
  somebody plays out on them.

  So when canastaNeededToScore is on and the other side has no
  canasta, going out is worth far more than the 100-point bonus
  chooseDiscard and the going-out check currently price it at.
  The AI should press for it -- take the canasta it needs, and
  go rather than milk.

  Paired with the item below, which is the same judgement
  pointing the other way. Neither should be written without the
  other or the AI will simply always rush.
  Resolved (2026-08-24), with GHUB-0100 in the same change as the bullet requires. minusOnOffer(theirs, rules) computes what ending the hand right now takes off the other side: under canastaNeededToScore a side with no canasta has its melds and red threes SUBTRACTED rather than added (handScoreFor), so every point they show is worth two to us. It returns points, not a flag, so Ai::closingOut widens its reach in proportion to what is actually on offer rather than by a flat step -- capped at 4, because past a point the hand cannot be emptied faster however big the minus. Red threes only count where they have opened, since an unopened side is docked for them either way and there is nothing there to win. Zero when the rule is off or they already have their canasta, so play outside the House set is untouched. canastaMinusAgainstMilking checks it on hand-built positions, the way discardRisk and handScoreFor already are.
  **Layman:** When the other side has no canasta, our side should push to go out and catch them a minus -- right now it plays the same either way.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0100] **The computer plays out even while the pile is feeding it.**
  The owner's family calls it being fed: a position where the
  other side keeps handing you the pack, and every turn you
  stay in is worth more than the going-out bonus.

  The AI has no notion of it and will go out on the first legal
  chance. What it needs is a read on how likely the pile is to
  come back to it -- how many of the pile's ranks it has melds
  in, whether the pile is frozen against it, how big the pile
  has grown -- and to weigh that against going out.

  The deliberate tension with the item above is the whole
  design. A minus sitting on the table argues for going now; a
  pile that keeps feeding argues for staying. Both have to be
  written together, and the levels checked against each other
  afterwards -- canastaLevelsDiffer() is what caught Hard being
  weaker than Medium last time.
  Resolved (2026-08-24), paired with GHUB-0099 exactly as the bullet demands -- neither would have been safe alone. packWorthStayingFor(pile, hand, mine, frozen, rules) answers what staying in is worth, in points, and returns the three readings the bullet names: a pack under 8 cards is not worth staying for however takeable; a pack frozen against us -- or unopened, which Engine::canTakePile treats identically -- is not coming back at all unless we hold a pair of naturals, wild cards explicitly not counting; and otherwise it comes back on any throw into a rank we have down, so a quarter of the pack being ours is the bar. Because BOTH sides come back in points, closingOut weighs them against each other rather than ordering them: a minus bigger than the pack presses, a pack bigger than the minus milks by holding the reach at 3. With the House rule off and a thin pack both are zero and the old reach is left exactly as it was, which is what keeps Classic play unchanged.
  **Layman:** When we keep taking the pack hand after hand, we should stay in and keep milking them rather than ending the hand.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0101] **The computer freezes the pack while nobody has opened, which buys it nothing.**
  With pileFrozenUntilOpened on -- the House default -- a side
  that has not opened needs two matching cards from hand to
  take the pile, which is exactly what a frozen pile demands.
  So while NEITHER side has opened, freezing changes nothing
  about who can take the pack.

  It is not free either: the discard spends a two or a joker
  worth 20 or 50 in the hand, and the freeze outlives the
  position that made it pointless.

  The owner raises it at the 90 and 120 opening bands
  specifically, which is where it costs most -- opening is
  furthest away, so the wasted freeze sits longest. Ai's
  discard choice should stop paying for it there.

  Rules-grounded rather than a taste call: Engine::canTakePile
  treats an unopened side exactly as it treats a frozen pile,
  which is what makes the freeze redundant.
  Attempted 2026-08-24 and REVERTED: the premise does not hold. The
  two halves of Engine::canTakePile's `mustUseTwoNaturals` -- `m_frozen
  || (pileFrozenUntilOpened && !team.opened)` at canastaengine.cpp:931
  -- are not interchangeable, because they have different LIFETIMES.
  `!team.opened` ends the moment that side opens, which they control and
  will do. `m_frozen` is cleared in exactly one place, canastaengine.cpp
  :1154, inside takePile -- never when a side opens. So a freeze thrown
  while the opponents are unopened is still in force after they open. It
  is a pre-emptive lock, not a redundant one, and the bullet above reads
  its own key sentence backwards: "the freeze outlives the position that
  made it pointless" is the BENEFIT rather than the cost.

  Measured as well as reasoned. The guard was written (one condition in
  Ai::wantsToFreeze, skipping the freeze while the opposing team had not
  opened) with a two-arm self-test check that passed both ways, and the
  seeded ladder in canastaLevelsDiffer() moved against it on every rung
  the change could reach: hard v easy 23/24 +3670 -> 22/24 +3376, hard v
  medium 68/120 +208 -> 65/120 +52, and expert v hard 129/240 +135 ->
  113/240 -127, which fails "expert beats hard" outright. Medium v easy
  was unchanged, confirming the guard reached only the two levels
  wantsToFreeze serves. Both files reverted; the tree is back to 416
  checks green.

  What the owner actually raised is still real and unanswered: a two or
  a joker spent freezing is worth 20 or 50 in the hand. But it is not
  spent for nothing, so the fix is not a skip. If it is worth pursuing
  the shape would be a PRICE rather than a veto -- freeze only when the
  pile is already big enough that the pre-emptive lock is likely to pay
  -- and that is a tuned threshold, which is his call and would need
  measuring against the ladder rather than fitted to it.
  SUPERSEDED IN EFFECT (2026-08-24), and OPEN pending the owner's ruling.

  The instinct this bullet was filed for -- "do not spend care on a side
  that has not opened" -- shipped as GHUB-0121, aimed at the DISCARD
  instead of at the freeze. That is where it holds: an unopened side must
  open OFF the pack, so the meld it takes with has to clear its own
  minimum, and the throw is graded by that bar. A throw is spent the turn
  it is made, which is why the reasoning works there and failed here.

  What remains genuinely unanswered is only the FREEZE half: a two or a
  joker spent freezing is worth 20 or 50 in the hand, and the revert note
  above proposes a PRICE rather than a veto -- freeze only when the pack
  is already big enough that the pre-emptive lock is likely to pay. That
  is a tuned threshold, and GHUB-0110 has since established that the
  ladder cannot measure one at this effect size.

  ASKED of the owner 2026-08-24 and NOT ANSWERED: close this as superseded
  by GHUB-0121, or keep it open for the freeze-pricing idea alone. Do not
  decide it from the code; it is his call. If it stays open, note that
  GHUB-0113 already tightens wantsToFreeze on two sourced grounds (a
  per-hand budget, and not freezing a pack that is ours), so any pricing
  work belongs with that bullet rather than beside it.
  Resolved (2026-08-25). The owner was asked again on 2026-08-25 and
  DEFERRED the disposition -- but answered the substance, which turned the
  open question round. He did not want a price; he named when to freeze:
  "1. When your side will feed (continually give the pack away) the
  opposition. 2. When you are fishing (holding back cards that your team
  has already melded) and there is little risk of giving the pack away."
  And he asked for research, which is where the third came from.

  So what shipped is REASONS rather than the price this bullet proposed --
  a price on a wild card is a tuned threshold, and GHUB-0110 established
  that the ladder cannot measure one. freezeIsWorthTheWild is the free
  function, checked on figures, and Ai::wantsToFreeze now refuses a freeze
  that has none of its three:

    - their side has opened and ours has not, so the pack is their asset.
      From suitedgames.com/canasta/strategy, read 2026-08-25: "freeze when
      opponents have melded but you have not";
    - we are fishing -- holding two or more naturals of a rank our own side
      already has down;
    - feedPressure is a third of the hand or more. That is the owner's
      first trigger, and it has a precise meaning here rather than a vague
      one: discardRisk prices a throw into a rank they have melded and
      returns ZERO once the pack is frozen, so freezing does not merely
      deny them the pack, it makes this seat's own dangerous cards
      throwable. Asked through discardRisk rather than by reading melds, so
      the two cannot disagree about a rank canastaMakesRankSafe has closed.

  The fishing reason is not decoration and the run proved it. Built with
  only the other two, the GHUB-0122 check went red on its last assertion --
  the owner's own opening play keeps a pair of the rank it has just melded
  and then freezes, and with no fishing reason there was nothing to
  justify the freeze. That is his second trigger, arrived at from the
  other end.

  Also shipped, from the same research: no freeze while closingOut() is
  true. "Do not freeze when reaching go-out and freezing slows your tempo
  disproportionately" -- and the wild is worth more finishing a canasta
  than locking a pack this side has no turns left to come back for.

  Measured. Freezes across the suite's full games fall from 1061 to 764, a
  28% cut, with the gate refusing 6741 opportunities. Ladder for the record
  and not as the judge (GHUB-0110): hard v easy +4335 -> +3882, hard v
  medium 64 -> 63 of 120, expert v hard 122 -> 121 of 240, all against the
  GHUB-0122 state earlier the same day.

  canastaFreezeReasons holds all three on figures, plus feedPressure itself
  and the canastaMakesRankSafe carve-out. Suite green at 504 checks,
  ctest 6/6.

  The bullet's original claim stays REFUTED, and the 2026-08-24 revert note
  above is still the reason: a freeze thrown while the opposition is
  unopened outlives their opening, so it is a pre-emptive lock rather than
  a redundant one. Nothing here reinstates that veto.
  **Layman:** Throwing a joker to freeze the pack is wasted when neither side has opened yet -- the pack is already out of reach for both of them.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0102] **Twos are jokers too, and the game does not call them that.**
  The game's own vocabulary is "wild card", which is the
  standard term and not the owner's. His family has one word
  for both -- joker -- and tells them apart by size: a big
  joker is the 50-point one, a small joker is a two.

  This is worth more than a rename. It settles a question that
  was open when GHUB-0097 was written: the original request
  said a canasta "with a joker in it" shows a black card, and
  under the family's own vocabulary that plainly includes a
  two. So GHUB-0097's choice of any-wild was the right one and
  is now confirmed by the owner's own words rather than by
  inference from the scoring.

  Surfaces to change: the meld badge's star count, the House
  rules dialog rows, the refusal messages, and the game's help
  blurb. The status bar is not among them -- the owner never
  reads it.
  Resolved (2026-08-24). The game speaks the family's vocabulary now:
  "joker" for both kinds of wild card, and "pack" for the discard pile --
  the second carried over from GHUB-0104's body, which asked for it here.

  Changed: the six refusal messages in canastaengine.cpp a player actually
  reads, the two Draw-phase captions on the play surface, and six House
  dialog rows. The dialog blurb is where the vocabulary is taught, once --
  "A joker means either kind: the big joker, or a two -- the small joker."
  -- rather than glossing every row that mentions one.

  Two surfaces the bullet named are NOT changed, both deliberately. The
  meld badge's star already answers "is there a joker in there?" and its
  own comment says so, so the glyph stands and only the words around it
  moved. And the game has no help blurb to change: in-app rules are
  GHUB-0016 and unbuilt, and the hub tile reads "Melds and partners",
  which names no wild card.

  The status bar WAS changed ("pack FROZEN"), against the bullet's note
  that it is not a surface. The reason that note gives is that the owner
  never reads it, which is about effort rather than harm -- and leaving
  one string saying "pile" while every other surface says "pack" is the
  inconsistency this item exists to remove.

  No settings key was renamed. maxWilds, wildTake and wildsFewer are
  stored keys, and renaming one silently unticks the rule for anyone who
  already set it -- the same trap canastaToScore carries a comment about.

  Suite green, 6/6 under ctest.
  **Layman:** The family calls a two a small joker and the 50-point joker a big joker; the game says 'wild card' everywhere.
  Kind: ux.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0103] **The computer cannot fish, and cannot tell when it is being fished.**
  The owner's description, kept whole because the shape of it
  is the specification. Holding three or more of a rank, you
  throw them out one at a time until two are left -- or one and
  a joker -- hoping the seat that discards to you is watching
  what you throw and follows with that rank, which hands you
  the pack.

  Two things make it a judgement rather than a rule.

  The bait is only worth throwing while the seat ABOVE you is
  the one likely to copy it. That is the seat that discards
  immediately before your turn, so it is the only one whose
  follow-up you get to take.

  And it is paid for downstream: the seat BELOW you sees the
  same discard, and the rank you are advertising is one they
  may be able to take the pack with. So fishing is a bet whose
  cost lands on the opposite side of the table from its prize.

  Three ranks in hand going down to two is also exactly the
  shape chooseDiscard already reasons about badly -- it prices
  a discard by what it gives away and never by what it might
  attract. Both halves belong in the same pass: an AI that
  fishes but cannot see it being done to it is worse than one
  that does neither.

  Sits with GHUB-0099, GHUB-0100 and GHUB-0101 as one body of
  work on canastaai.cpp. Whatever lands, the levels have to be
  re-measured against each other afterwards --
  canastaLevelsDiffer() is what caught Hard playing weaker than
  Medium.
  Resolved (2026-08-25) for the FISHING half. The other half -- seeing it
  done to you -- is GHUB-0124, filed rather than left as a gap, and this
  bullet's own warning about shipping one without the other was checked
  against measurement rather than accepted or ignored. See below.

  fishingWorth(held, packSize, unseen) is the judgement, a free function
  checked on figures, and it offsets rather than removes the two penalties
  that stopped this seat ever throwing one of three -- the flat -30 and the
  -7 a card. Fishing has to win the discard on its merits against every
  other card in the hand. Zero below three held (the pair IS the key), zero
  on a pack under five (nothing worth advertising for) and zero where all
  eight of the rank are accounted for, because then no seat is holding one
  to follow with and the bait is an advertisement nobody can answer. It is
  skipped entirely on a free throw, where nothing can be taken and there is
  no follow-up to fish for.

  EXPERT ONLY, and that was forced by measurement rather than chosen for
  neatness. Given to Hard as well it inverted the ladder outright: expert v
  hard 115 -> 103 of 240, failing "expert has not fallen behind hard". The
  reason is this bullet's own point arriving from the other side. Hard
  reads the pack the OTHER way round (`safety -= 2.5 * shown`) and is
  therefore bait-resistant by accident, while Expert's `safety += 50 *
  countRank(pile, rank)` is precisely what bait aims at. So a fishing Hard
  beat a baitable Expert. Gated to Expert the rung moves the other way, 115
  -> 117 of 240.

  Which is also the honest answer to "an AI that fishes but cannot see it
  being done to it is worse than one that does neither": with the gate at
  Expert, the only level that fishes is the only level that can be fished,
  so it is exposed to nobody but itself, and the one rung that can see the
  change moved in its favour.

  The defence WAS built and reverted, and the diagnosis is on GHUB-0124:
  capping Expert's pile-follow term at two occurrences cost 7 games (117 ->
  110) and could never have worked, because a fisher throws them one at a
  time and the bait shows as one or two -- exactly where that count still
  means what it says. The tell is who threw them, and Engine::pile()
  records no such thing.

  The bullet's downstream-cost half needed no code: the seat below seeing
  the same discard is what discardRisk and the pack count already price,
  and they are subtracted from the same discard's score.

  canastaFishing holds the judgement on figures -- the three-card floor,
  the pack floor, the unseen floor and both slopes. Suite green at 564
  checks, ctest 6/6.
  **Layman:** Throwing away one of three matching cards to tempt the player above you into throwing the third -- the family calls it fishing, and the computer neither does it nor spots it.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0104] **The computer lays melds down while the pack is frozen, spending the pairs that would take it.**
  Rules-grounded rather than a taste call, which is what makes
  it checkable. Engine::canTakePile demands two matching cards
  from HAND against a frozen pile. A pair melded is a pair no
  longer in hand, so every meld laid down while the pack is
  frozen quietly spends the only key to it.

  So while the pack is frozen the AI should hold, and lay down
  only on one of two triggers the owner named:

    - it is going for the minus (GHUB-0099), where ending the
      hand is worth more than the pack; or
    - it has no realistic chance of taking the pack anyway --
      no pair that matches it, or the pile is out of reach for
      another reason.

  Terminology, worth carrying into GHUB-0102: the owner's
  family calls the pile the PACK. The game says "pile"
  everywhere, including in the refusal messages a player
  actually reads.

  Fourth of the AI items with GHUB-0099, GHUB-0100, GHUB-0101
  and GHUB-0103, and it interacts with all of them: holding
  cards back is the opposite instinct to going out, so the
  levels have to be re-measured against each other once they
  all land -- canastaLevelsDiffer() is what caught Hard playing
  weaker than Medium.
  Attempted 2026-08-24 and REVERTED. Unlike GHUB-0101 the premise here
  holds: canastaengine.cpp:933 wants naturalsOfTop >= 2 out of HAND
  against a frozen pile, so a melded pair really is a spent key.

  Note the hold itself already ships -- Ai::holdsWhileFrozen exists and
  the opening path already trims a frozen-pile opening down to the
  minimum the band asks for. What was missing was only this bullet's
  SECOND release trigger, no realistic chance of taking the pack. The
  narrowest rules-grounded form of it is that a rank the seat holds just
  ONE of can never be the two naturals, so holding it back buys nothing:
  one line, `if (naturals < 2) return false;`. A check proved it both
  ways from one position -- the lone ace goes down, three sevens stay
  back -- and passed.

  It still went back, because it makes the AI slightly WORSE. Measured
  on the seeded ladder at the shipped 240 games: expert v hard 129/240
  +135 -> 115/240 -121, failing "expert beats hard"; hard v medium
  68/120 -> 63/120; medium v easy moved too (21 -> 22), which is the
  expected signature since holdsWhileFrozen serves every level above
  Easy. Re-measured at 1200 games to rule out a small sample: 621/1200
  +36 baseline against 584/1200 -102 with the change, a difference of 37 games in 1200. That was first written up here as
  "about a 2-sigma shift, so the loss is real rather than noise", and
  that is WRONG: the standard error on a difference of two proportions
  at this size is about 0.020 against a difference of 0.031, so it is
  1.5 sigma and p is about 0.13. The measurement does not separate the
  change from neutral in either direction.

  Why holding the lone card wins is NOT established -- the effect is
  small and no mechanism was proved. Do not re-attempt this on the
  strength of the rules argument alone; it is correct about the pile and
  still loses. The measurement that would settle it needs the instrument
  filed alongside this note, because the ladder cannot separate a small
  gain from noise in either direction.
  Resolved (2026-08-24). Reinstated and shipped after the note above was
  corrected: the reading that sent it back the first time was a
  misapplied statistic, not a measured loss.

  What ships is this bullet's second release trigger only, in its
  narrowest rules-grounded form. Ai::holdsWhileFrozen now returns false
  where the seat holds fewer than two naturals of the rank, because
  Engine::canTakePile wants two matching naturals out of HAND against a
  frozen pile and one card can therefore never be the key to it. The
  hold itself, and the frozen-pile opening trim, already shipped.

  The first trigger -- lay down because we are going for the minus -- is
  NOT in this change. Ai::closingOut already covers most of it and the
  rest belongs with GHUB-0099, which is where the minus is priced.

  canastaAiKeepsOnlyRealKeysWhileFrozen in tests/selftest.cpp is what
  holds it, and it proves both halves from one position rather than
  asserting the release alone: seat 1 opens on five aces with the pile
  frozen, comes back holding one ace and three sevens, and must lay the
  lone ace down while keeping the sevens. It fails on the pre-change
  build. Suite green at 430 checks.

  Judged by position rather than by win count, per GHUB-0110. For the
  record the ladder still moves against this change -- expert v hard
  115/240 against a 129/240 baseline -- which is inside the tolerance
  that check now allows and is not distinguishable from noise at this
  sample size. If a later reading with real power shows the hold winning
  after all, this is the line to revisit.
  REOPENED (2026-08-24). The shipped rule was wrong and is reverted --
  code and check both out, expert v hard back to its 129/240 baseline,
  suite green at 431. The changelog entry is withdrawn; it never
  released.

  What was wrong. The release fired on `naturals < 2`, on the argument
  that one card can never be the two naturals a frozen pack wants. True
  about THIS turn and false about the hand: draw another of that rank and
  the lone card is a pair. The owner's correction is the option value --
  "if you have 10 cards in your hand you should still play as if you
  could take the pack, because when you pick up cards you more than
  likely are going to build up pairs". So the release is keyed on HAND
  SIZE, not on how many of a rank are held: keep building while the hand
  is big, stop building and play out when it is small.

  It also explains the measurement that was written off as noise. Holding
  really did win, by the owner's mechanism, and the 1.5-sigma reading was
  pointing the right way after all.

  Research agrees, and adds numbers (2026-08-24, sources listed on GHUB-0113):
  "meld the minimum needed cards, even if your hand can support more";
  "hold extra pairs after the initial meld" as currency -- from five of a
  rank, meld three and keep two; "each melded card reduces your
  flexibility for claiming the discard pile later"; "a meld of three or
  four cards is a placeholder, the score lives in the canasta". No source
  found supports laying a spare card down early. The opening trim already
  implements the first of these; the hand-size release is what is left.

  Two further conditions the owner named, neither implemented, both
  belonging here rather than in a new bullet because they are the same
  "when do I stop holding" question: if the pack is frozen and it looks
  like nobody will ever take it and the stock is running out, play out if
  the score is decent. And see GHUB-0114 for the opposite case.
  Resolved (2026-08-25). Reinstated on the owner's correction and shipped in
  the shape he named: the release is keyed on HAND SIZE, not on how many of a
  rank are held. Ai::holdsWhileFrozen now returns false once the hand is down
  to two-thirds of a deal (7 of 11), expressed against Rules::handSize so a
  house deal of thirteen moves the line with it.

  canastaAiPlaysOutWhenTheHandGetsSmall in tests/selftest.cpp holds it, and
  proves both halves from one position: the same three sevens stay back on a
  hand of twelve and go down on a hand of seven. It fails on the pre-change
  build. Suite green at 470 checks, ctest 6/6.

  Judged by position rather than by win count, per GHUB-0110 -- and the ladder
  reading is worth recording because it is NOT what the two reverted attempts
  suggested. holdsWhileFrozen serves Medium, Hard and Expert alike, so the only
  rung where this change lands asymmetrically is against Easy, and there it
  IMPROVED: hard v easy 23/24 +3489 -> 23/24 +3969. The two symmetric rungs
  moved down (hard v medium 70 -> 58 of 120, expert v hard 129 -> 115 of 240),
  which is 1.6 and 1.3 sigma respectively on a match where neither side gains
  anything from the change -- noise, not a strength signal. Both still pass
  notTheWeakerPlayer.

  NOT in this change: the third condition the owner named -- frozen pack,
  nobody will ever take it, stock running out, so play out if the score is
  decent. That is the same reading of the same position as GHUB-0114 with the
  opposite answer, and this bullet's own note says neither should be written
  without the other. It ships with GHUB-0114.
  Third condition SHIPPED (2026-08-25), with GHUB-0114 as this bullet
  required. Ai::holdsWhileFrozen now also releases once the stock is down to
  two rounds of the table, unless the hand is one GHUB-0114 would rather
  run dead -- the pack may never come round again that late, so cards held
  for it are cards about to be caught in hand. The exception matters: in a
  hand nobody is going to score, holding on costs nothing.

  That closes every condition this bullet named. The first -- lay down
  because we are going for the minus -- remains covered by Ai::closingOut
  and by GHUB-0107, which prices the first canasta under the minus rule and
  releases a melded rank towards it.
  **Layman:** A frozen pack can only be taken with two matching cards from your hand -- so melding those cards away is giving up on the pack without noticing.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0106] **Nothing counts how many of a rank are left, and two packs means eight.**
  SUGGESTED, NOT REQUESTED. Offered to the owner on
  2026-08-24 and not yet answered; filed as considered so it
  survives the session rather than as planned work.

  Two packs means eight of every rank. Once all eight are
  visible -- melded anywhere, in the pack, or thrown -- that
  rank is dead safe for the rest of the hand. Short of that,
  four kings on the opponents' table means only four exist
  anywhere else, which changes what a fifth one in hand is
  worth.

  The reason it is interesting here rather than merely
  correct: it is something a computer does perfectly and a
  person cannot, so it is a difficulty dial in its own right.
  Easy forgets, Expert never does -- which is a more honest
  way to make a level weak than making it play badly.

  Sits with GHUB-0099 to GHUB-0104 as canastaai.cpp work.
  Promoted (2026-08-24): owner ruled on it — into the Canasta AI pass with 0101..0104, 0113 and 0114.
  MOSTLY ALREADY BUILT -- read the code before planning this one
  (established 2026-08-24 while scoping the AI pass; no code written).

  Ai::seen(e, rank) already counts a rank across every visible place the
  bullet names: this seat's hand, the pack, and both teams' melds. Two
  callers already use it.

    - chooseDiscard, in the Hard/Expert block, opens with the comment
      "Count the pack. Eight of every rank exist" and scores a throw by
      `unseen = 8 - seen(...)`, both as a safety bonus capped at four seen
      and as a penalty weighted by pack size.
    - worthHolding bails on `seen(e, rank) >= 8` -- a rank nobody can
      still throw is no use as bait.

  The difficulty dial the bullet argues for also exists, in two steps
  rather than four: Easy and Medium never count at all, Hard and Expert
  count identically.

  So what is actually left is smaller than the bullet implies, and worth
  deciding on purpose rather than discovering mid-change: (a) whether Hard
  and Expert should differ here, which is the bullet's "Easy forgets,
  Expert never does" taken literally; and (b) whether an all-eight-seen
  rank deserves an explicit cliff, since today it is the top of a smooth
  term capped at four rather than a stated "this rank is dead safe".

  Neither is obviously worth doing, and both move numbers the ladder
  cannot measure (GHUB-0110). Treat this as a verify-and-close candidate
  unless the owner wants (a).
  Resolved (2026-08-25) as the verify-and-close the note above called for.
  Asked of the owner on 2026-08-25; he deferred it.

  Verified: the bullet's subject was already built, exactly as the note
  said, and NOTHING checked it -- not one assertion in the suite touched
  the count. That was the gap worth closing rather than the behaviour.

  Two free functions extracted from Ai::seen and from the discard's pack
  term, on the same footing as discardRisk and throwCaution, so both can be
  checked on a hand-built table. seenSoFar(hand, pack, one, two, rank)
  counts a rank wherever it shows; Ai::seen is now a one-line call to it.
  packCountSafety(unseen, pileSize) is what that count is worth on a throw.
  No behaviour changed -- the ladder reads identically on both sides of the
  extraction, to the point.

  canastaCountsThePack holds it: all eight of a rank found across hand,
  pack and both sides' melds; a rank showing twice; a rank showing nowhere;
  and the safety term monotone in unseen, past the cap, and rising with
  pack size.

  Decided rather than discovered, on the two questions the note left open.
  (a) Hard and Expert do NOT differ here, and should not: they already
  differ in three other places in chooseDiscard, and a fourth difference
  would be a change with no evidence behind it, since GHUB-0110 established
  the ladder cannot measure one this size. Easy and Medium still do not
  count the pack at all, which is the difficulty dial the bullet wanted.
  (b) No explicit cliff for an all-eight-seen rank, and the header now says
  why: unseen 0 is already the maximum of the term -- the bonus in full
  with no penalty against it -- so the cliff IS the top of the slope. A
  second statement of it would be two numbers to keep in step.

  Suite green at 511 checks, ctest 6/6.
  **Layman:** Once all eight cards of a rank have been seen, that rank is safe to throw forever -- a computer can track that perfectly and a person cannot.
  Kind: feature.
  Source: claude-suggestion-2026-08-24.

- ✅ [GHUB-0107] **Under the minus rule the FIRST canasta is insurance, and the AI prices it as 300 points.**
  SUGGESTED, NOT REQUESTED. Offered to the owner on
  2026-08-24 and not yet answered; filed as considered so it
  survives the session rather than as planned work.

  Follows from the owner's own rule rather than from general
  Canasta. With canastaNeededToScore on -- the House default
  since GHUB-0098 -- ending a hand with no canasta does not
  merely forfeit a bonus: handScoreFor inverts that side's
  melds and its red threes. So the first canasta is insurance
  against a swing far larger than the 300 it pays.

  Which argues for closing a cheap MIXED canasta early rather
  than holding out for the 500-point natural -- the opposite
  instinct to the one a scoring table alone suggests, and the
  mirror of GHUB-0099: one says press for the minus, this says
  buy protection from it.

  Both sides of that trade belong in the same pass.
  Promoted (2026-08-24): owner ruled on it — into the Canasta AI pass with 0101..0104, 0113 and 0114.
  Resolved (2026-08-25). Two edits, both gated on caughtAMinus so Classic
  plays exactly as it did -- confirmed, the ladder reads identically on
  both sides of this change, because canastaMatch builds a default Engine
  and canastaNeededToScore is off there.

  Ai::holdsWhileFrozen now releases a rank this side already has DOWN when
  caught a minus. That is the mechanism the bullet was really about: the
  frozen-pack hold was keeping cards back from the very canasta that stops
  the side's whole table being subtracted. Only for a melded rank -- those
  are the cards that shorten the road; a rank with nothing down yet is a
  road not started.

  closeFirstUnderAMinus orders the wild-card spend in chooseMelds nearest a
  canasta first, and it is the bullet's own "opposite instinct" made
  explicit: the meld closest to a canasta is also the likeliest to fill
  naturally, so closing it with a wild turns a 500 natural into a 300
  mixed. Worth it under the minus rule and not otherwise -- MEASURED, and
  this is the finding: sorting unconditionally cost a game of medium v easy
  and 200 points of its margin, which is that natural canasta being spent
  for nothing. The header carries the figure.

  canastaFirstCanastaIsInsurance holds it -- one frozen table played twice,
  Classic and minus, with a hand of NINE so the GHUB-0104 hand-size release
  cannot be what moves the card. The lone ace goes down under the rule and
  stays in hand without it. The minus arm failed first time for an honest
  reason worth keeping: a WILD up-card makes Engine::dealFrom turn a cover
  card, so the deal eats one card more than usual and every draw shifts
  down by one. The check says so where the stock is built.

  Suite green at 544 checks, ctest 6/6.
  **Layman:** If being caught with no canasta turns your whole table against you, getting any canasta down early is worth far more than its bonus.
  Kind: feature.
  Source: claude-suggestion-2026-08-24.

- 📋 [GHUB-0108] **How dangerous a throw is does not scale with how big the pack has grown.**
  SUGGESTED, NOT REQUESTED. Offered to the owner on
  2026-08-24 and not yet answered; filed as considered so it
  survives the session rather than as planned work.

  chooseDiscard weighs a card's danger as a fixed judgement.
  But the stake is the pack, and the pack grows all hand: the
  same throw that is nearly free on turn two can hand over
  fifteen cards on turn twenty.

  So the bar a throw has to clear should tighten with the size
  of the pack rather than being one number. Early in a hand
  almost anything is safe, which is also why the first-round
  rule the owner plays (noMeldingFirstRound) costs so little.

  Note the interaction with GHUB-0104 and Expert's existing
  +50 x countRank(pile, rank) term, which is already a
  pack-aware safety term -- this generalises it rather than
  adding a second one beside it.
  Promoted (2026-08-24): owner ruled on it — into the Canasta AI pass with 0101..0104, 0113 and 0114.
  Attempted 2026-08-24 and NOT shipped. The bullet's own instruction --
  "this generalises it rather than adding a second one beside it" -- was
  implemented literally: the per-term (1 + 0.12 * pileSize) came off the
  unseen reading in chooseDiscard and became one weight over the whole
  `safety` accumulator.

  It measured worse, and unlike GHUB-0101 and GHUB-0104 the loss is on the
  rung that can actually see it. Bisected against a measured baseline of
  hard v easy 23/24 +3489, hard v medium 70/120, expert v hard 129/240:

    - with GHUB-0121 only:  23/24 +3594, 67/120, 118/240
    - with this half added: 20/24 +2959, 67/120, 113/240

  hard v easy wins by thousands of points a game, so three games and a
  sixth of the margin there carries far more information than the same
  swing on expert v hard, which GHUB-0110 showed cannot separate anything.

  The cause is understood rather than guessed, which is why this is a
  finding and not just a failed try. discardRisk ALREADY scales with pack
  size -- `25.0 + 0.4 * pileSize` -- so weighting the accumulator scaled
  that term a second time and made feeding a near-canasta grow roughly
  quadratically with the pack, drowning the hand-value terms that decide
  an ordinary throw.

  So the bullet's premise is right and its prescription is wrong: the
  accumulator is not one thing that can take one weight, because one of
  its terms already carries the reading. A future attempt has to either
  take discardRisk's own pileSize term out first (it is checked directly
  by canastaDiscardRisk, so that is a visible change, not a quiet one), or
  weight only the terms that lack one -- Hard's -2.5 * shown and Expert's
  +50 * countRank -- and leave discardRisk alone.

  The single pack-size weight that DOES ship is the one that was always
  there, on the unseen term, and it now carries a comment pointing here.
  **Layman:** Handing over three cards is a nuisance; handing over fifteen can lose the hand -- so a throw should have to be safer as the pack grows.
  Kind: feature.
  Source: claude-suggestion-2026-08-24.

- ✅ [GHUB-0109] **Black threes are a one-turn block and the AI spends them at random.**
  SUGGESTED, NOT REQUESTED. Offered to the owner on
  2026-08-24 and not yet answered; filed as considered so it
  survives the session rather than as planned work.

  blackThreeBlocksPile is on in both rule sets, so a black
  three on top stops anyone taking the pack -- for exactly one
  turn, until it is covered. That makes it a timing card: best
  spent when the pack is fat AND the seat to the left is
  live, not whenever it turns up.

  The other half is that they are dead weight. A black three
  only melds on the turn its side goes out and never takes a
  wild, so a hand holding three of them is holding three cards
  it mostly cannot use -- which argues for spending them
  rather than hoarding them, and for spending them WELL.

  Smaller than the other AI items and independent of them.
  Promoted (2026-08-24): owner ruled on it — into the Canasta AI pass with 0101..0104, 0113 and 0114.
  Resolved (2026-08-25). ONE of the bullet's two halves ships, and the other
  was built, measured worse and reverted -- they pull in opposite
  directions, which the bullet did not say and the measurement did.

  Shipped: blackThreeWorth(packSize, caution), a free function, and the
  throw's bonus now runs through it. The pack-size half is the step the
  judgement already had, kept at its measured figures. The new half is the
  owner's second reading -- "the seat to the left is live". That seat is
  always an opponent, partners sitting opposite, so throwCaution grades it
  exactly as GHUB-0121 grades a dangerous throw: against a side that has
  not opened and is on the 120 band, a one-turn block buys almost nothing
  and the card is better kept.

  REVERTED, and this is the useful finding. The bullet's other half says
  black threes are dead weight -- they never take a wild and cannot be
  melded until a side goes out -- and argues for spending rather than
  hoarding them. Acting on it means exempting them from chooseDiscard's two
  "do not break up your holdings" penalties, which is what makes the AI
  hold them. Built, and it cost 10 games of expert v hard (115 -> 105 of
  240, margin -104 -> -364) and 6 of hard v medium (66 -> 60 of 120). Two
  independent rungs moving the same way, and expert v hard landed half a
  game above notTheWeakerPlayer's floor.

  Why: those penalties ARE the timing mechanism. They are what stops a
  black three being thrown the turn it turns up, which is the whole of
  "spent when the pack is fat, not whenever it turns up". Removing them
  implements the dead-weight half by destroying the timing half. The
  comment on the penalty now says so with the figures, so the next reader
  does not re-derive it.

  Also folded in, and behaviour-neutral: throwCaution is computed once per
  discard rather than once per card, and is 1.0 for Easy and Medium so
  those two read exactly as they did.

  canastaBlackThreeTiming holds it on figures -- fatter pack worth more,
  live seat worth more, and the two composed through throwCaution. Suite
  green at 514 checks, ctest 6/6.

  Ladder against the GHUB-0101 state: hard v easy +3882 -> +3861, hard v
  medium 63 -> 66 of 120, expert v hard 121 -> 115 of 240. Mixed and within
  noise; the position check is the judge (GHUB-0110).
  **Layman:** A black three stops the next player taking the pack for one turn -- worth saving for when the pack is fat and they look ready.
  Kind: feature.
  Source: claude-suggestion-2026-08-24.

- ✅ [GHUB-0113] **The computer will freeze the pack twice in a hand and freeze one it could take itself.**
  From published strategy, read 2026-08-24. Ai::wantsToFreeze already
  gets two things right: it will not freeze a pack under five cards
  ("freezing when the pile is small simply throws away a 20- or 50-point
  wild card"), and it insists on holding a pair of the top rank first
  ("freeze only if you hold natural pairs so you can break the freeze
  yourself"). Two it gets wrong.

  It has no per-hand budget. The advice is explicit and numeric: "do not
  freeze more than twice per hand". Nothing counts freezes, so a hand
  with spare wilds can spend three or four.

  And it never asks whether the pack was already ours. "Your team is
  positioned to claim the pile -- freezing costs you access too." A
  freeze locks everyone out, so freezing a pack we could take on this
  turn, or could take unfrozen with one card and a wild, throws away the
  thing we were trying to protect.

  Sources, all read 2026-08-24: rarepike.com/canasta/discard-pile-strategy,
  card-games.ca/strategies/ultimate-canasta-strategy-guide,
  suitedgames.com/canasta/strategy, pagat.com/rummy/canasta.html.
  The same four back the holding advice cited on GHUB-0104. NOTE for
  anyone reaching for them on the AI items: none of the four describes
  fishing, so GHUB-0103 has no published source and the owner's own
  description is its specification.
  Resolved (2026-08-25), and held back deliberately until GHUB-0122 shipped
  with it -- the owner's call on 2026-08-25, made on the measurement below.

  What ships. Ai::noteHand keeps a per-hand freeze count and
  freezeBudgetLeft caps it at the published two, the counter keyed on the
  stock GROWING rather than on Engine::handNumber(), because a new game
  restarts the numbering while the seats live for the whole session.
  freezeCostsUsThePack is the second half, a free function so it can be
  checked on figures: it asks packWorthStayingFor whether the pack is
  already coming back to us, but only of a side a freeze could actually
  cost -- a side that has not opened is held to two naturals out of hand by
  pileFrozenUntilOpened already, so a freeze takes nothing from it.

  The premise was MEASURED before any of it was written, and both halves
  were dead code at the time. Across the suite's full games the AI committed
  409 freezes; every single one came from a side that had not opened, and
  only 2 were a second freeze in a hand -- so the budget never bound, and
  the pack-already-ours guard could never fire. Without the not-opened
  carve-out it fired 263 times, every one of them wrongly.

  GHUB-0122 is what made them live, exactly as expected: freezes rise to
  1061, of which 224 come from an OPENED side, 29 are a second in a hand,
  the budget now refuses 10 third attempts and the guard now stops 29
  freezes. Neither figure is large and neither is zero.

  NOT shipped, because it cannot arise: "do not freeze a pack you could
  take on this turn". Ai::draw has already taken any pack it could take and
  wanted, and wantsPile wants every pack of three cards or more at the two
  levels that freeze, while a freeze needs five.

  canastaFreezeLimits holds both on figures -- the budget at 0, 1, 2 and 3,
  and the guard over three positions including the shut-out carve-out.
  Suite green at 497 checks, ctest 6/6.
  **Layman:** Throwing a joker to freeze the pack costs 20 or 50 points, and the computer does it more often than it is worth -- including when the pack was ours for the taking.
  Kind: feature.
  Source: canasta-strategy-research-2026-08-24.

- ✅ [GHUB-0114] **Losing badly, the computer should run the pack dead rather than let the hand score.**
  The owner's tactic, in his words: if the opponents have a high score
  and you can run the pack dead -- no cards left in the pick-up pile --
  and your own score will be low, it is better to run it dead than to
  play the hand out.

  It depends entirely on a HOUSE rule, and that is the thing to get right
  before any of it is written. Standard Canasta SCORES a hand the stock
  kills: pagat.com is explicit that when a player who wishes to draw
  cannot, "the play ends immediately and the hand is scored". This
  project's House set turns that round -- deadHandIfNobodyGoesOut is on
  by default, and canastaview.cpp already carries the reasoning, that
  scoring a hand nobody could finish rewards the side that sat on a
  frozen pile. So the tactic is worth points under House and worth
  nothing under Classic, and the AI must read the flag rather than assume
  it.

  Also from pagat, and needed for any of this to work: play continues
  with an empty stock for as long as each seat takes the previous
  discard and melds it. So running it dead is not simply drawing the last
  card.

  The pair with GHUB-0104's third condition -- frozen pack, stock running
  out, decent score, so play out -- is deliberate. Same reading of the
  same position, opposite answer depending on who is ahead, and like
  GHUB-0099 against GHUB-0100 neither should be written without the
  other.
  Resolved (2026-08-25), and shipped WITH GHUB-0104's third condition, which
  both bullets said must not be written without the other. They are the same
  position read two ways, and one function answers it for both.

  runTheHandDead(ourShowing, theirShowing, stockLeft, rules) is the
  judgement, a free function checked on figures. It reads the FLAG rather
  than assuming it, which the bullet named as the thing to get right:
  deadHandIfNobodyGoesOut makes a hand the stock kills void, while classic
  Canasta scores it where it stands (pagat.com), which hands the leader
  their points anyway -- so it returns false under Classic and the whole
  tactic costs nothing there.

  Three places act on it, via Ai::killingTheHand.

    - closingOut refuses outright. Going out is the one thing that
      certainly stops the hand dying.
    - wantsPile leaves the pack alone. Taking it does not touch the stock
      and drawing does, so drawing brings the end nearer. Ai::draw still
      takes it once the stock is gone: refusing THEN does not kill the
      hand, it stalls the turn, because Engine::startTurn ends a hand only
      where nobody can take the pack.
    - holdsWhileFrozen is GHUB-0104's third condition and the mirror --
      with the stock nearly gone the pack may never come back, so play out
      while the hand is worth something, UNLESS it is one we would rather
      kill.

  The showings are each side's table less the cards the reading seat holds,
  and the partner's hand is deliberately not passed. This AI sees its own
  cards, the pack and both tables; nothing in canastaai.cpp reads a hand it
  is not entitled to and this did not start.

  NOT built: the bullet also notes that with an empty stock play continues
  while each seat takes the previous discard and melds it, so running it
  dead is not simply drawing the last card. Nothing steers the DISCARD
  towards a card the next seat cannot take. That is a further step and is
  not needed for the tactic to work -- refusing to go out and refusing the
  pack is what empties the stock.

  canastaRunsThePackDead holds both halves: the judgement on figures either
  side of the rule, the stock window, the ownership of the hand and the
  going-out-bonus margin; then a seat that could take the pack and draws
  instead, which fails on a build with the guard mutated off.

  Suite green at 556 checks, ctest 6/6. Ladder against the GHUB-0107 state:
  medium v easy 23 -> 22 of 24, hard v easy +3861 -> +3844, hard v medium 66
  of 120 unchanged, expert v hard 115 -> 119 of 240. Mixed and within noise;
  note that runTheHandDead is inert on the ladder, which builds a default
  Engine with the rule off, so what moved there is GHUB-0104's stock
  release alone.
  **Layman:** When we are well behind and cannot win the hand, emptying the pick-up pile kills the hand so nobody scores it -- better than letting them bank a big one.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0120] **You go out by throwing your last card, and the game lets you go out by emptying your hand.**
  The owner's rule, in his words: "The last action to play out (end
  the round) is to throw away your last card. One exception ... is
  that if you have all four black 3's then you put the black 3's down
  as a meld to earn an extra 20 points when counting your score."

  What the engine does today, both routes live and neither gated by a
  rule flag. canastaengine.cpp:856 sets `goingOut` when a lay-down
  consumes the WHOLE hand, and :1168 then calls goOut(seat) -- so
  melding out with no discard is legal and the AI uses it
  (Ai::playAndDiscard returns early on "melding out ends the hand").
  canastaengine.cpp:1232 sets `goingOut` when a discard leaves the
  hand empty, which is the owner's route.

  So this is one new field in canasta::Rules and a refusal, not a
  branch -- the House set turns the meld-out route off, Classic keeps
  it. Same shape as every other house variation.

  Black threes are HALF there already: group() at :643 and
  validateGroups() at :787 refuse a black-three meld unless goingOut,
  so "you may only put them down on the way out" is enforced. What is
  absent is the four-of-them case earning their 20 as a meld rather
  than being caught for them in hand.

  OPEN, and it decides the shape: does laying the four black threes
  REPLACE the final discard -- hand empty, nothing thrown -- or do you
  lay them AND still throw a last card? Asked 2026-08-24, unanswered
  at filing. Under the first reading the exception really is an
  exception to "the last action is a discard"; under the second the
  discard rule is absolute and the exception is only about WHEN a
  black three may be melded, which the engine already allows. The two
  produce different refusals and a different AI end-game.

  Also unresolved: with the meld-out route closed, Ai::closingOut and
  the closing block in chooseMelds both have to leave a card to throw
  rather than emptying the hand, or the AI will spend the endgame
  proposing lay-downs the engine refuses.
  ANSWERED 2026-08-24, and it is neither reading the question offered.
  The owner: "You don't throw away anything, everything in your hand must
  be put down on melds and all that remains must be the four black 3's
  that you then lay down as a meld."

  So the exception is a full MELD-OUT, not a discard with a meld attached.
  The rule in two parts:

    - To go out you end by throwing your last card. A lay-down that
      empties the hand is refused.
    - UNLESS that lay-down puts every other card onto melds and lays all
      four black threes as a meld of their own. Then the hand ends empty
      with nothing thrown, and the threes bank their 20.

  Consequences for the build. The gate is on the LAY-DOWN that empties the
  hand, so it belongs where keepsADiscard already lives -- it is the same
  question ("must you keep a card to throw?") with a second answer. The
  exception has to inspect the GROUPS, not the hand: it fires only when a
  black-three meld of exactly four is among them.

  Not changed: black threes melding at all. group() and validateGroups()
  already allow a black-three meld only when going out, and minMeldSize
  lets three of them stand. The owner named four because four is what
  earns the 20; nothing he said forbids a three-card black meld alongside
  a discard, so that stays as it is.

  Not changed: scoring. A melded black three is already worth
  blackThreeValue on the table via Meld::value, so four of them are 20
  without a scoring branch.
  Resolved (2026-08-24), to the owner's clarified rule rather than to
  either reading the question offered him.

  One new field, canasta::Rules::goingOutNeedsADiscard, off in Classic and
  on by default in House (key "discardOut"). The gate is four lines in
  Engine::keepsADiscard, which already owned the question "must you keep a
  card to throw?" -- a lay-down leaving handAfter == 0 is refused unless
  laysFourBlackThrees() finds all four in one meld among the groups. Asked
  of the GROUPS rather than the hand, because it is the shape of the move
  that earns the exception.

  Placed BEFORE the requireCanastaToGoOut logic and falling through to it,
  so both rules bind at once: a hand that earns the black-three exception
  still may not go out without a canasta.

  Save format: tail 4 on the engine stream, view version 4 -> 5. A game
  saved by an older build still loads and comes back without the rule, per
  the tail's existing contract.

  Two checks, and the second is the one that matters. canastaHouseGoesOut
  OnAThrownCard builds one position and plays the identical lay-down under
  both rule sets -- seven aces open and make a canasta, then the whole
  remaining hand goes down. Classic allows it either way; House allows it
  only when the remainder is the four black threes. Proved by mutation:
  stubbing the gate to `if (false)` reddens "but otherwise refuses a
  lay-down that empties the hand", and it was restored from a copy rather
  than by git, since the tree carried other uncommitted work.

  The check first asserted the REFUSAL MESSAGE off canMeldCards, and that
  was wrong: canMeldCards validates into a local QString and never touches
  m_error, so lastError() after it is whatever an earlier move left. It
  now plays the move for real with meldCards, which both sets the error
  and proves the allowed arms actually end the hand.

  And goingOutNeedsADiscard was added to the four-computer full-game house
  run. That loop exists to catch a hand that cannot legally continue, and
  this rule REMOVES a legal move, which is how a seat ends up with nothing
  it may do. Four seeded games complete, so it does not strand one.

  NOT done, and it is the reason this bullet's own body flagged it: the AI
  is not taught the rule. Ai::chooseMelds will still propose a lay-down
  that empties the hand, the engine refuses it, and playAndDiscard falls
  through to its discard -- correct, but a wasted call each time, and the
  AI never deliberately goes out by meld. Left for the AI pass, where
  closingOut is being rewritten anyway.

  Suite green: 451 checks, 6/6 under ctest.
  **Layman:** At the family table the round ends when you throw your last card away -- melding your whole hand and stopping is not how it is done.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0121] **The computer throws safe cards at a side that cannot take the pack yet.**
  The owner: "What the computer currently does is (after the first
  round that is) throw away cards you have already thrown away even
  though my team hasn't opened yet. While a team is struggling to open
  there is no need to be careful about what you throw away until they
  open. Only then can they take the pack. This is less relevant when
  the opening score is 15 / 50 as it is a lot easier to open at that
  score. It gets more relevant at 90 and very relevant at 120."

  His premise checks out, and it is sharper than it first looks. An
  unopened side taking the pack must ALSO open with it -- the meld it
  makes has to clear their opening minimum. So the bar is not "two
  matching naturals", it is "two matching naturals that build a
  lay-down worth 120", which is why he grades it by band.

  Where it lands is NOT discardRisk. That function already returns 0
  the moment `theirs.meldOfRank(rank)` is null, and an unopened side
  has no melds at all -- so the feeding-their-meld term is already
  silent here. What is still firing against an unopened side is the
  pack-counting half of chooseDiscard: Hard's `safety -= 2.5 * shown`,
  Expert's `safety += 50 * countRank(pile, rank)`, and the shared
  `9.0 * (4 - min(4, unseen))` and `-0.9 * unseen * (1 + 0.12 *
  pileSize)` pair. None of those asks whether the opposition could use
  the pack if it were handed to them.

  So the shape is a DISCOUNT on the safety accumulator scaled by the
  opposing side's openRequirement -- near nothing at 15 and 50, real at
  90, heavy at 120 -- and not a veto. chooseDiscard already gathers
  every such judgement into `safety` precisely so it can be scaled or
  dropped in one place, which is what the first-round rule does today.

  This is the live form of the instinct GHUB-0101 was filed for and got
  wrong. That bullet aimed it at FREEZING, where the premise was false
  because a freeze outlives the unopened state. Aimed at the DISCARD it
  holds, because a throw is spent the turn it is made. Sits with
  GHUB-0108, which scales the same accumulator by pack size; both are
  modifiers on one number and should be written together.
  Resolved (2026-08-24). throwCaution(theirs, openRequirement) is a new
  free function beside discardRisk and packWorthStayingFor, and
  chooseDiscard multiplies the whole `safety` accumulator by it at Hard
  and Expert. Easy and Medium stay naive on purpose -- being careful when
  you need not be is a weaker player, not a broken one.

  It returns exactly 1.0 against an opened side, so the common position is
  untouched to the last decimal. Against an unopened side it is
  max(0.30, 1 - need/170): 0.91 at 15, 0.71 at 50, 0.47 at 90, floored at
  0.30 for 120 -- the owner's own grading. Floored rather than run to zero
  because opening off the pack is hard, not impossible, and it is exactly
  how a stuck side comes back in one move.

  canastaThrowCaution locks the curve on figures rather than on a position
  played into existence, which is the idiom handScoreFor and
  openRequirementFor already use: 1.0 when opened whatever the band,
  strictly decreasing across 15 > 50 > 90 > 120 while shut, and never
  below the floor.

  MEASURED, and the reason this shipped in half the form it was written
  in. The bullet was built together with GHUB-0108, which asked for one
  pack-size weight over the whole accumulator instead of the single term
  that carries it. Bisected on the ladder against a measured baseline of
  hard v easy 23/24 +3489, hard v medium 70/120, expert v hard 129/240
  +104:

    - both halves: 20/24 +2959, 67/120, 113/240 -133
    - this half alone: 23/24 +3594, 67/120, 118/240 -130

  So GHUB-0108's half is what costs three games and a sixth of the margin
  on hard v easy -- the rung with by far the most signal, since it wins by
  thousands rather than by dozens. Its cause is understood rather than
  guessed: discardRisk already scales with pack size, so weighting the
  accumulator scaled it a second time and made feeding a near-canasta
  roughly quadratic in the pack. That half is NOT shipped; see GHUB-0108.

  What this half does to expert v hard, honestly: 129 -> 118 of 240, about
  1 sigma on the difference and not separable from noise at that sample,
  per GHUB-0110. Both levels get the same discount, so it should not
  favour either systematically. The rung with real power -- hard v easy --
  is unchanged in count and slightly BETTER in margin (+3594 against
  +3489), which is the reading I would trust if the two disagreed.

  Suite green: 6/6 under ctest.
  **Layman:** While the other side still has to open, they can barely touch the pack -- so being careful what you throw at them is caution spent on nothing.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0122] **Opening on a joker to keep the pair that takes the pack, then freezing it.**
  The owner's second way of fishing, in his words: "say the opening
  score is 50 and you have a big joker (50 points) and four 8's. You
  open with the joker and two 8's and then freeze the pack. Or if the
  pack is already frozen then you only put out as little as possible to
  open the table for your team."

  The play is one move with two halves. Open on the MINIMUM, spending a
  joker to make the value up rather than spending naturals -- joker +
  two 8s is 70 against a 50 bar -- so the other two 8s stay in hand.
  Then freeze the pack yourself. A frozen pack wants two matching
  naturals out of hand, and you are now the only side at the table
  holding a pair.

  Half of it already ships and half does not. The frozen-pack case --
  "if the pack is already frozen then you only put out as little as
  possible" -- is the opening trim at canastaai.cpp:316-333, which
  sorts the groups cheapest-first and drops any the minimum does not
  need. GHUB-0104's note already records that as done. What is missing
  is that the trim fires only `if (m_level != Level::Easy &&
  e.pileFrozen())`. This play needs it when the pack is NOT yet frozen,
  because freezing it is the second half of the same move.

  Also missing: the trim drops whole ranks but never chooses to spend a
  WILD to keep a natural pair back. The wild-card branch below it only
  fires when the naturals alone fall short of the bar, so a hand that
  can open on naturals never reaches for the joker -- which is exactly
  the hand this play is about.

  Filed apart from GHUB-0103 deliberately, though the owner calls both
  fishing. That one is discard bait and lives in chooseDiscard; this
  one is opening shape plus a freeze and lives in chooseMelds' opening
  path and wantsToFreeze. Same instinct, different functions, and the
  freeze half has to agree with GHUB-0113's budget rather than fight
  it.
  Resolved (2026-08-25). Three edits, all in canastaai.cpp.

  The opening trim now runs whether or not the pack is frozen. It was
  `m_level != Level::Easy && e.pileFrozen()`; the frozen half is gone,
  because the advice behind it is general -- "meld the minimum needed
  cards, even if your hand can support more" -- and because this play needs
  it on a pack that is NOT yet frozen, freezing being the second half of
  the same move.

  The missing half the bullet named is built: a wild may now be spent to
  keep a natural PAIR back. One rank only, Hard and Expert only, and only
  where the pack is already five cards, the same bar a freeze asks for --
  keeping a key to a pack nobody would want is a wild card wasted. The
  wilds are spent DEAREST first here, the opposite of everywhere else in
  the file, because the substitution has to carry the value the pair took
  with it: joker plus two eights is 70 against a bar of 50, and a two would
  have made 40 and failed.

  And Ai::wantsToFreeze now takes ANY natural pair in hand as the key,
  where it wanted a pair of the current top card. That single condition
  ruled the whole play out: the freezing card goes on TOP of the pack, so
  the rank that will matter is one nobody has thrown yet. The published
  advice says pairs, plural, not the top rank -- "freeze only if you hold
  natural pairs so you can break the freeze yourself".

  canastaAiOpensOnAJokerToKeepThePair holds all three from one position,
  because the play is one move: seat 1 comes to a pack of five holding four
  eights, a joker and two twos, opens on joker plus two eights, keeps the
  pair, and freezes. Proved by mutation both ways -- disabling the
  substitution loses the opening and the pair, and restoring the top-rank
  freeze condition loses the freeze. Suite green at 497 checks, ctest 6/6.

  Ladder, for the record and not as the judge (GHUB-0110): hard v easy
  +3969 -> +4335, hard v medium 58 -> 64 of 120, expert v hard 115 -> 122
  of 240, all measured against this session's GHUB-0104 state.
  **Layman:** Open with as little as you can, using a joker to make up the points, so the matching pair stays in your hand -- then freeze the pack that only you can now take.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0123] **You win by REACHING the target, and both sides reaching it is a draw.**
  The owner's rule, in his words: "A winner isn't determined by who
  has the most score. It is determined by who gets to the total score
  (by default usually 5000). So, if both teams reach the total score,
  it is a draw."

  What the engine does today, at canastaengine.cpp:1341-1347 and
  Engine::winner(). The game ends when EITHER side is at or over the
  target and the two scores differ, and the winner is then simply the
  higher score. So a hand that carries both sides past 5000 declares
  the higher one the winner, where the house rule calls it a draw.
  An exact tie at or over the target already plays another hand, which
  this rule replaces: both sides reached it, so it is a draw either
  way.

  A house variation like every other, so a new Rules field rather than
  a branch -- off in Rules::classic(), on in the House set. It needs
  the House dialog row, the settings key, the Rules save tail and the
  game-over strings that today read "You win!" or "They win" and have
  no third thing to say.

  Engine::winner() has no way to say "nobody" -- it returns -1 for a
  game that is not over yet, and three view call sites plus two checks
  treat anything that is not 0 as team 1 winning. So a draw needs its
  own value rather than reusing -1, or a draw reads as a loss.
  Resolved (2026-08-25). Rules::bothReachingTargetIsADraw, off in
  Rules::classic() and on in the House set, with the dialog row, the
  `canasta/house/drawOnBoth` key, engine save tail 5 and view version 6.
  Engine::kDraw is what winner() answers when nobody won -- its own value
  rather than -1, which already means "still running", so the three view
  call sites that read `winner() == 0` cannot report a draw as a defeat.
  Recomputed from the scores rather than remembered, so a loaded game
  answers as a played one does.

  canastaWinningIsReachingTheTarget in tests/selftest.cpp holds it, built
  on the dead-hand position so the totals are exactly the cards the four
  seats were caught with and the target can be put where the check needs
  it. Four positions: both sides over on different totals (classic gives
  it to the higher score, House calls it a draw), one side over alone
  (unchanged either way), and the exact tie (classic deals another hand,
  House draws). Both draw assertions fail on a build with the rule
  mutated off. Suite green at 480 checks, ctest 6/6.

  Found on the way and fixed in the same change: Engine::kTail still read
  3 while save() wrote 4 pairs, from GHUB-0120. The default-argument path
  -- which the self-test's save round trip uses -- therefore stopped one
  pair short and silently dropped goingOutNeedsADiscard on the way back
  in. The view always passes an explicit tail, so no saved game was ever
  harmed. It is 5 now.
  **Layman:** The game is won by getting to 5000, not by being ahead -- and if both sides get there in the same hand, nobody wins.
  Kind: feature.
  Source: user-request-2026-08-25.

- ✅ [GHUB-0124] **The computer cannot tell WHO threw a card, so it cannot see fishing being done to it.**
  The half of GHUB-0103 that did not ship, filed rather than left as a
  gap. That bullet asked for both halves in one pass, on the grounds
  that "an AI that fishes but cannot see it being done to it is worse
  than one that does neither". The fishing half shipped on 2026-08-25
  and this did not.

  What was tried, and why it cannot work. Expert scores a throw with
  `safety += 50 * countRank(e.pile(), c.rank)` -- a rank the others
  have parted with is one they are unlikely to hold a pair of. That is
  precisely the term a fisher aims at. Capping it at two occurrences
  was built as the defence and measured WORSE: expert v hard 117 ->
  110 of 240. The diagnosis is the useful part. A fisher throws them
  ONE AT A TIME, so the bait shows up as one or two of a rank in the
  pack -- exactly where that count still means what it says. The cap
  therefore penalised the honest safety read and caught no bait at
  all.

  The real tell is WHO threw them, and Engine::pile() is a plain
  vector<Card> that records nothing of the sort. So the defence needs
  provenance the engine does not keep: which seat discarded each card
  in the pack. That is an engine change with a save tail and a view
  version behind it, which is why it is its own bullet rather than a
  line in GHUB-0103.

  Note that the fishing half was gated to EXPERT for exactly this
  reason. Given to Hard as well it inverted the ladder outright --
  expert v hard 115 -> 103 of 240, failing "expert has not fallen
  behind hard" -- because Hard reads the pack the other way round
  (`safety -= 2.5 * shown`) and is therefore bait-RESISTANT by
  accident, while Expert is the one level that can be fished. A fishing
  Hard beat a baitable Expert. Read that measurement before widening
  the level gate.

  A cheaper approximation exists if provenance is not wanted: the seat
  that discards immediately before this one always owns the TOP card
  of the pack, and an Ai could remember the pack between its own
  turns and attribute the four cards that appeared. It is per-seat
  state that a taken pack resets, and it was not attempted.
  Resolved (2026-08-25): the pack now records WHO threw each card.
  `Engine::m_pileFrom` runs parallel to `m_pile` and is kept in step at the three
  points the pile changes -- the deal (seat -1, nobody threw it), a discard (the
  discarding seat), and a take (both cleared). `pileThrownBy(i)` reads it.

  The defence is one line at the point of temptation. Expert's
  `safety += 50 * countRank(pile, rank)` becomes
  `safety += 50 * e.pileRankSources(rank)`, which counts SOURCES rather than
  cards: each seat that threw the rank counts once however many it threw, and the
  cards nobody threw count once between them.

  That shape is the whole finding, and it is not the cap this bullet ruled out. A
  fisher throws them ONE AT A TIME, so capping the raw count at two penalised the
  honest read and caught no bait -- measured at 117 -> 110 of 240. Counting
  sources leaves two seats letting a rank go worth the full +100 while two from
  the SAME seat is worth +50, because the second card is that seat telling you it
  holds more of them.

  The deal's up-card counts as one source rather than none, deliberately. It came
  out of the STOCK rather than a hand, so it is real information, and the seat that
  turned it cannot be fishing. Reading it as nothing instead broke
  `canastaFirstRoundAndPileOpening`, whose whole fixture is an up-card -- and the
  break was correct behaviour for a check about a different rule, which is how it
  was caught.

  Save tail 6 carries the provenance and the view's version goes 6 -> 7. A game
  saved by an older build still loads: it comes back with the pile marked unknown,
  which reads as one cautious source rather than several confident ones, so the
  defence turns itself off for that hand rather than arguing wrongly. That
  mattered here -- the alternative was bumping `kSaveVersion`, which refuses every
  existing save, and GHUB-0126 is a fresh reminder of what losing a player's game
  costs.

  Locked by `canastaSeesWhoThrewIt`, which plays five real discards -- and the
  turn order is the trap: it runs 1, 2, 3, 0, 1, so seat 1's second king is the
  FIFTH throw. Getting that wrong made the discards fail while the source counts
  still looked right, which is how the check first passed for the wrong reason.
  Mutation-checked: counting raw cards instead of sources turns it red.

  Ladder, against the 2026-08-25 baseline in CLAUDE.md: medium v easy 22/24 +2538
  and hard v easy 23/24 +3844 and hard v medium 66/120 +233 are all UNCHANGED,
  which is the check that matters -- the change is gated to Expert and the other
  rungs must not move. Expert v hard went 117/240 -71 to 121/240 +14. Per
  GHUB-0110 that is NOT evidence of improvement and must not be quoted as any; it
  is evidence of no collapse, which is all the ladder can say.

  NOT done, and deliberately: no second term penalising a throw INTO a rank a seat
  is fishing. Removing the false safety is the defence this bullet asked for;
  adding a penalty beside it is a separate judgement with no measurement behind it.
  The cheaper per-seat approximation this bullet described is moot now that the
  engine keeps the real thing.
  **Layman:** The computer can now fish -- throw one of three matching cards as bait -- but it still cannot spot anyone doing it to it, because it cannot see which player threw which card.
  Kind: feature.
  Source: in-session-2026-08-25.

- ✅ [GHUB-0125] **Starting Snake with the Right arrow was an instant game over.**
  A new snake was laid out with `push_front`, which puts the LAST square
  written at the front of the deque -- so the head ended up at the LEFT end
  of a snake whose direction is right, with its own body occupying the two
  squares ahead of it. The first step drove the head into its own neck and
  the game ended before it began.

  Reachable only by pressing Right or D as the first key, which is why it
  shipped. Any other arrow turned the snake off its own body and played
  normally, and Left was refused as a reversal before the game even started.

  Found the moment Snake's rules could be reached from a test at all
  (GHUB-0066), and it is the exact defect that bullet predicted: `tests/uitest.cpp`
  ALREADY pressed Right to start Snake, and only ever asserted that the clock
  was running. The snake was dead in that very check and nothing looked.

  Fixed by building the body with `push_back`. Locked in two places, both
  verified to go red against the original layout: `snakeDiesAtTheWall` in the
  self-test, which now measures the run from the head's real starting column,
  and a view-level check that pressing Right to start does not report a game
  over.

  That view-level check deliberately pumps for less than 200 ms. `gameOver()`
  opens a modal QMessageBox 200 ms after a death, and a modal dialog in an
  offscreen test does not fail, it hangs.
  **Layman:** Pressing the right arrow to start Snake killed you immediately; every other arrow was fine.
  Kind: fix.
  Source: in-session-2026-08-25 (found by GHUB-0066's Snake extraction).
  Lanes: snake.

- ✅ [GHUB-0126] **Undoing a move in FreeCell lost the card off the table.**
  A drag lifted its cards off their pile the moment the pointer moved past
  the threshold (`mouseMoveEvent`), but `pushUndo()` was called at DROP time,
  inside `mouseReleaseEvent`. So the snapshot was taken of a table the cards
  had already left. Undoing a completed move restored that snapshot, and the
  cards were on no pile at all -- simply gone.

  Worse than it looks, because FreeCell's `restoreState` demands the whole
  pack back: `matchesPack` refuses a save missing a card. So a player who
  undid a move and later closed the app got the save REFUSED on reload and a
  fresh deal instead, losing the game outright. Measured rather than reasoned:
  after one drag and one undo, `saveState()` produced a blob that a fresh view
  would not restore.

  Fixed by moving the snapshot into `FreeCellTable::lift()`, which banks it
  BEFORE the cards leave their pile. A drag put back down where it started
  calls `putBack()`, which drops that snapshot again -- otherwise the undo
  stack fills with moves nobody made.

  Locked in two places. `tests/uitest.cpp` drives a real drag, triggers the
  Undo action and requires the table to come back pixel for pixel; that check
  was written first and was RED before the fix. And `freecellUndoDoesNotLoseACard`
  in the self-test asks the same question of the rules directly, plus the
  abandoned-drag case, plus `matchesPack` after every step.

  Note this is the same shape as the trap CLAUDE.md already records about
  saving mid-drag -- "a run lifted in mid-drag has been erased from its pile
  and lives in m_drag until it is dropped". That trap was written about
  saveState and the undo path had the same hole.
  Also KLONDIKE (2026-08-25, same day). Extracting its core showed the identical
  ordering -- `mouseMoveEvent` lifted the run off its pile, `pushUndo()` ran inside
  `mouseReleaseEvent` -- so the same lost card was there and shipped. Fixed the
  same way: `KlondikeTable::lift()` banks the snapshot before the cards leave, and
  `putBack()` drops it when a drag is abandoned.

  Proved rather than assumed: with the original ordering put back into the
  extracted table, `klondikePlaysOutWithoutLosingACard` goes red on both of its
  pack checks. That check plays 60 random games and calls `matchesPack` after every
  move AND after every undo, which is the only place the loss shows.

  Klondike's undo is worth MORE than FreeCell's, because its stock recycles: a
  player who turns the stock too far routinely undoes to get back. And Klondike's
  save has the same `matchesPack` gate, so the same "close the app and lose the
  game" consequence applied.

  Why the UI test could not have found this one the way it found FreeCell's: the
  FreeCell check works because parking the bottom card of the first column in a
  free cell is legal in EVERY deal, so a fixed drag always makes a real move.
  Klondike has no such move -- whether a drag is legal depends on what was dealt --
  and nothing can seed a deal yet (GHUB-0093). The rules check does not care: it
  builds the position it needs.
  **Layman:** Undo in FreeCell made the card you had just moved disappear, and the game could then no longer be saved or resumed.
  Kind: fix.
  Source: in-session-2026-08-25 (found by GHUB-0066's FreeCell extraction).
  Lanes: freecell.

- 📋 [GHUB-0129] **Decide whether the Canasta opening prune should survive the extend pass.**
  chooseMelds prunes groups the opening minimum does not need, and its
  own comment says they are better in hand -- a rank the opposition
  would then stop throwing -- and says GHUB-0122 widened it to run on
  an unfrozen pile deliberately. The extend pass below then finds the
  same cards in `remaining` and lays them anyway. But the selftest
  asserts exactly that: with the pile open the sevens go down, and
  they are held only while it is frozen. Both are yours. A fix was
  written and REVERTED rather than edit either side to fit the other.
  **Layman:** Two parts of the Canasta AI disagree about whether to hold a rank back, and only you can say which is right.
  Kind: investigate.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0130] **Klondike accepts a drop back onto the column a run came from.**
  Spider refuses a same-column drop; Klondike does not, so the move
  scores and banks a no-op undo snapshot. Repeated, it inflates the
  persisted top score and evicts real states from the 200-deep
  history. Same shape: sendToFoundation will bounce an Ace between two
  empty foundations, and dealFromStock snapshots before checking the
  stock and waste are both empty.
  **Layman:** Lifting a card and putting it straight back counts as a move and eats your undo history.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0131] **Draughts cannot choose between two capture chains ending on one square.**
  English draughts lets you take any available chain. Two chains from
  one square to one landing square that capture different pieces are
  routine for a king; the first match in m_selectedMoves wins and the
  destination dot is drawn twice in the same place with no cue. The
  same file has no draw condition at all, so two lone kings shuffle
  forever and announceResult(Side) cannot express the outcome.
  **Layman:** When two different jump paths finish in the same place, the game silently picks one.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0148] **Canasta: red threes taken with the pile step past the no-legal-move guard.**
  canastaengine.cpp: placeRedThrees(seat, false) strips the red threes
  out of a taken pile AFTER keepsADiscard() has already sized the
  resulting hand. validateTake computes after = hand - layDown + pile
  - 1 from the raw pile, and keepsADiscard clears it at after >= 2.
  Land on after - reds == 1 with no canasta and the seat is stranded:
  canDiscard refuses and every meld refuses, which is the hang
  CLAUDE.md calls the expensive symptom. Land on 0 and goOut() takes
  the seat out with no canasta at all, bypassing requireCanastaToGoOut,
  which goOut performs no check of its own for. Reachable only when the
  deal turned a red three into the pile. Count the pile's red threes
  inside validateTake and subtract them before calling keepsADiscard.
  **Layman:** A rare take can strand the hand with nothing legal to do, or let a side go out when it should not be allowed to.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0149] **Canasta AI: several judgement terms rest on figures that are not what they claim.**
  canastaai.cpp, all MEDIUM, none fixed. "Eight of every rank" is
  hardcoded where 4 * r.decks is meant, and a save may carry decks 1
  to 8 -- under one deck worthHolding's cheapest gate is dead and the
  unseen count goes negative, which flips packCountSafety's second term
  positive with no bound. handShowing is docked our own hand's value on
  our side and not on theirs, so runTheHandDead's goingOutBonus margin
  is consumed by an ordinary hand and killingTheHand fires on level
  positions -- and it is gated on a House flag, so the ladder cannot
  see it. The GHUB-0122 pair-keeping opening conflates "cards still
  needed" with "wilds this meld may take", so it only ever fires on a
  group of exactly four. chooseDiscard calls front() on the hand with
  no guard. Hard and Expert's wantsPile thresholds are byte-identical
  under comments claiming a difference, and kEndgameStock has a diverged
  copy (< 8 against <= 8). The per-hand freeze budget is not persisted,
  so a save reloads with the ceiling refilled.
  **Layman:** A handful of the computer's decisions are built on numbers that are wrong in ways the strength ladder cannot see.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0150] **Canasta: pending rule changes take effect at the next game, not the next hand.**
  canastaengine.h documents pack size, jokers and hand size as taking
  effect "from the next hand". applyRules pins those three on m_rules
  and leaves the new values in m_pendingRules, which is consumed by
  newGame() and newGameFromStock() and by nothing else -- nextHand()
  calls deal(), which reads m_rules. Given the stated intent (nobody
  should have to abandon a game to correct a house rule) the code is
  the likely wrong side; if not, the comment is.
  **Layman:** Change the hand size mid-game and it does not apply until you start a whole new game, with nothing saying so.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0159] **Chess moves the game on by two paths, and its search wastes most of its budget.**
  chessview.cpp: engineMoveReady plays a move and then re-implements
  advance()'s over-check, refresh and announce block inline rather than
  calling it -- a second path through the function CLAUDE.md names as
  the single point that moves the game on. The two agree today; it is
  the structural reason the stale-timer CRITICAL was reachable, and
  closing it makes a recurrence harder. Separately, chessai.cpp's
  quiescence calls board.legalMoves() and then discards the quiet moves:
  roughly 35 moves fully legality-checked (each copying the board and
  running inCheck) to keep about 5 captures, inside a fixed NODE budget
  -- so Hard's ~1.2s buys several times less search than it could.
  Also in that file: setFromFen validates geometry but not the king
  count or the side not to move, so a position derived from a bad FEN
  can never be checkmate; plain char is passed to <cctype>; and the
  halfmove clock uses atoi, so an absurd value silently disables the
  fifty-move draw.
  **Layman:** The chess engine does the same job in two places, and throws away most of the thinking it pays for.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0160] **The remaining per-game defects the sweep found and this pass did not fix.**
  One place for the MEDIUM and LOW remainder, by game.
  HEARTS: the queen breaking hearts is a variant no document states;
  winner() breaks a tie by lowest seat, which is the human; playCard's
  return is discarded and the sound plays regardless; five public
  methods index before their guard.
  KLONDIKE/SPIDER: deactivate() clears the flights but not a lifted run,
  so the board locks against every further pick-up; Spider's height
  budget steps 2.2 where its deal reaches 2.85; the stock hit test runs
  before the columns and they overlap; double-click ignores the clicked
  index and sends the column's top card; undo and newGame do not clear
  m_flights.
  FREECELL/PYRAMID: FreeCell's restore does not validate the
  foundations; a no-op drop counts a move, and moves are the score;
  Pyramid's redeal glyph is the only on-surface cue and degrades to
  blank without a font.
  REVERSI/DRAUGHTS: the Draughts Snapshot lacks lastMove, so undo leaves
  the gold highlight pointing at a move that is gone; the board is
  heap-allocated and copied per search node where Reversi's is a
  std::array.
  MINESWEEPER/SUDOKU: Sudoku's default constructor generates a full
  puzzle that is always discarded (three generations to restore one
  save); pencil-mode delete is a visible no-op and clearMarks has no
  caller; Restart never restarts the tick; Minesweeper's difficulty is
  remembered and Sudoku's is not.
  2048: undo does not restore m_reachedTarget; trailing bytes past the
  sixteenth cell are accepted.
  **Layman:** A list of smaller game bugs, kept so none of them is lost.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0165] **Play-test the pinball flippers after the timing fix, and re-tune if they feel dead.**
  The units fix moved the flipper integration into the substep loop, so
  the measured angular speed falls from about 138 to 18 -- the true
  rate. The kick constants in collideFlipper were NOT re-tuned: 1.55 on
  the reflection, and min(swing * 26.0, 520.0), which used to saturate
  at 520 on every contact and now yields 468. The surface-velocity term
  is the one that changed most, dropping by the same factor.
  pinballLaunch and the containment checks still pass, and those are
  about the plunger rather than the flippers, so nothing in the suite
  says how the flippers now FEEL. If a swing reads as weak, those three
  constants are where to look, and CLAUDE.md's warning applies -- they
  may have been tuned by eye against the error. Take a --bench reading
  before and after any change.
  **Layman:** The flippers were fixed to keep proper time, and nobody has actually played them since.
  Kind: investigate.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0166] **Three Canasta house-rule promises have never been seen rendered.**
  verify-delivery ran over 0.5.0 and found no promise untrue, but three
  came back unverified rather than delivered, all for one reason:
  --shot photographs a game the MOMENT it opens, and a frozen pack, a
  stacked canasta and its colour badge do not exist at the deal.
  GHUB-0095 (the freezing card lies as a T), GHUB-0096 (finished
  canastas stack on the red threes, turned about) and GHUB-0097 (a
  stacked canasta names itself by the colour of its top card). Their
  uitest checks pass, which is real evidence, but nothing has put the
  rendered result in front of a person. CLAUDE.md records the throwaway
  harness that reaches them -- five source edits, reverted afterwards --
  and notes it found two defects no arithmetic in the project had
  flagged. GHUB-0093's --seed, or a --turns flag, would retire that
  harness and make these checkable for good.
  **Layman:** Three things the release promised about how Canasta looks cannot be photographed, so nobody has checked them by eye.
  Kind: test.
  Source: verify-delivery 0.5.0, 2026-08-31.

## The score book on a phone

A replacement for the paper score book the owner's family keeps at the table on
a Friday. It is NOT a companion to the desktop game and it does not play
Canasta: four people play with real cards, and the phone keeps the book. It
therefore does not reimplement the scoring -- they work each hand out
themselves, as they do now -- and the only numbers it shares with the game are
the opening minimums, guarded by scripts/scorepad-check.py.

- ✅ [GHUB-0115] **A score book for the phone, keeping one book on one device.**
  Built to the owner's own list. Per hand it takes each side's score --
  worked out at the table as now, so no scoring rule is duplicated -- plus
  who went out and the going-out bonus, and the house exact-cut bonus of
  50 to the side that cut when the deal used the cut up with no cards
  left over. It keeps the running totals, shows whose deal it is and who
  deals next, and shows each side's opening minimum against its current
  score.

  Four initials are entered in CLOCKWISE seating order; partners sit
  opposite, so the two sides are seats 1+3 and 2+4 and the setup screen
  shows the pairing it worked out rather than assuming it was understood.
  The dealer moves round clockwise from whoever is picked for the first
  hand. Any hand can be tapped and corrected or deleted, because a score
  book gets corrections.

  Built for a partially sighted reader at a table: large tabular figures,
  high contrast, tap targets that need no aiming, and an A-/A+ control
  that is remembered. It works with no signal once loaded, which is the
  point rather than a nicety -- a kitchen table on a Friday is where the
  signal is worst.

  scripts/scorepad-check.py is the drift guard, wired into ctest so it
  runs locally, on the pre-push hook and on both CI legs. The opening
  bands and the target and going-out defaults exist in the app AND in
  canastaengine.{h,cpp}; the script holds no copy of its own, reads both
  and fails when they disagree. Proved by mutation both ways -- a changed
  band and a renamed constant each exit 1.

  Superseded in one respect the same day: the owner wants the same book
  on all four phones, which no single-device store can do. See the bullet
  below. Everything here except where the score is KEPT survives that.
  Verified on a real phone by the owner, 2026-08-24: "I have already
  opened it on the phone and it works perfectly." That closes the gap
  every other check here left open -- everything before it was headless
  Chrome at a phone-sized viewport, which proves layout and arithmetic
  and proves nothing about a real device in a real hand, least of all for
  the reader this was built for.

  Live at https://milnet01.github.io/games-hub/scorepad/ since GitHub
  Pages was enabled on the repo (deploy from branch, master, root).
  **Layman:** Replaces the paper score book: enter each side's score for the hand and it keeps the totals, whose deal it is and what you need to open.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0116] **All four players should see the same book, and one device cannot do that.**
  The owner asked for this as "make it an app rather than a webpage",
  and that is worth recording carefully because it is not what decides
  it. A native Android app installed on four phones is four separate
  copies of the score, exactly as four copies of a web page are. Neither
  syncs on its own. What produces four matching screens is SHARED STATE,
  and that is an independent decision from how the thing is packaged.

  Three shapes, and the choice turns on the room rather than on taste.
  A server holding the book, joined by a room code -- works on any phone
  including an iPhone, needs a signal at the table, and needs somewhere
  to host it. Peer to peer over the local network or Bluetooth -- works
  with no internet at all, which suits a kitchen table, but is
  substantially more work and pins it to one platform. Or one phone keeps
  the book and the others only watch, which is much less machinery and
  may be all that is actually wanted.

  Open and blocking: is there wifi or a usable signal where they play, do
  all four need to ENTER scores or only see them, and are all four phones
  Android. Do not pick an architecture before those are answered -- the
  first rules out the server, the second rules out most of the work, and
  the third rules out a native Android answer.
  Progress (2026-08-24): built and pushed, deliberately NOT marked
  shipped. The code is complete and every part of it is verified in
  isolation -- module and worker parse, ctest 6/6, three screens shot
  headless at phone size, the fallback path exercised by there being no
  config to find. What has NOT happened is the only test that matters
  for this bullet: two devices actually in step. That needs a Firebase
  project, which needs the owner's Google login, so it cannot be done
  from here.

  Answers that shaped it, all from the owner: wifi at most places and
  mobile otherwise, so a hosted store is viable; ONE person enters and
  the rest watch, which removed the entire concurrent-write problem
  rather than solving it; all four phones Android, though the web page
  reaches an iPhone anyway and a substitute player is expected.

  Firebase Realtime Database over the owner's first suggestion of
  GitHub. GitHub is right for hosting -- Pages will serve this free over
  HTTPS -- and wrong for the live score, for three reasons worth keeping:
  a write needs a token no public page can hold, unauthenticated reads
  cap near 60/hour against three phones polling, and raw files are
  CDN-cached so watchers would lag behind the scorer.

  Remaining, and none of it is code: the owner creates the project and
  pastes the config block into scorepad/firebase-config.js, pastes the
  rules from scorepad/README.md, and enables Pages on the repo. Then two
  phones prove it.
  Resolved (2026-08-24). The test this bullet was held open for has now
  been run: two live clients against the owner's real database, one
  sharing and one joining. The watcher's phone reported totals 820/850,
  sides "A + D v K + V" and D to deal -- matching the scorer's book
  exactly, and none of it held locally by that device. Test rooms were
  deleted afterwards and the database left empty.

  Setup is done and needed less than expected. The Firebase project is
  CanastaScoreCard on the free Spark plan, Realtime Database in Belgium,
  and the rules from scorepad/README.md are published -- verified from
  outside rather than taken on trust: reading the whole database is
  refused, a four-letter room is allowed, a five-letter one is refused,
  and the same three answers hold for writes. Registering a web app
  turned out to be unnecessary: the SDK works with databaseURL alone,
  proved by probe rather than assumed, so firebase-config.js carries one
  line and no apiKey.

  Three harness mistakes are recorded on GHUB-0118 rather than here, all
  of the same shape -- a check that returned a confident wrong answer.
  The one worth carrying: --virtual-time-budget closes the page before
  real network traffic completes, so a WORKING write reads as broken.
  Anything testing this app against a live database must wait on real
  elapsed time.

  Still outstanding and not code: GitHub Pages is not yet enabled, so
  there is no address for the phones to open. That is the owner's click.
  **Layman:** Everyone at the table should see the same running score on their own phone, updating as hands are entered.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0117] **Past games are filed rather than wiped, with a record per person.**
  Finishing a game FILES it instead of clearing the book, and Past games
  shows the record. Kept per PERSON rather than per seat, because the
  owner said substitutes turn up and partnerships change -- so a record
  follows the player, not the chair. Games played and won, hands they
  went out on, and exact cuts their side was on, all of which the book
  already captured and none of which it was using. Plus best hand, best
  game, biggest winning margin and longest game, and the filed list with
  the winner marked in green.

  Everything is derived from the stored hands rather than tallied as the
  game goes, so correcting a hand corrects the record with it and there
  is no second set of numbers to drift. A game filed by mistake can be
  tapped to put it back in play.

  Checked against seeded data with a substitute in one game: A and D show
  3 played, V shows 2, J shows 1, and A's five going-out hands and three
  exact cuts were counted by hand off the fixture and matched.
  **Layman:** Finishing a game keeps it, and a dashboard shows who has won what across all the Fridays.
  Kind: feature.
  Source: user-request-2026-08-24.

- ✅ [GHUB-0118] **A phone joining the table shows a loading screen instead of an empty board.**
  Found by running two live clients rather than by reading the code. The
  Realtime Database socket genuinely takes a few seconds to come up --
  measured, the sequence is attached, connected=false, connected=true,
  then the first delivery -- and during that window a joining phone was
  rendering an empty board reading 0/0 against "? + ?". That is
  indistinguishable from failure at a table.

  It now shows a slow, large pulse and says what is happening, with a way
  out. Slow and large deliberately: a small fast spinner is harder for
  the owner to see and unpleasant besides, and the motion is dropped
  entirely under prefers-reduced-motion.

  A watcher that already holds last week's book is shown THAT rather than
  the spinner, and it refreshes in place when the live one lands. A stale
  board beats a spinner.

  Two bugs came out of the same testing, either of which would have
  stranded three of the four phones. A watcher's phone starts with no
  book, so it landed on the SETUP screen, which had no way to join at
  all -- there is now an "Or join the table" panel there. And the waiting
  screen was inside gameView(), so a phone in a room but holding no book
  fell through to setup regardless; it is its own view now, checked
  before the setup branch.
  **Layman:** While a phone is connecting it says so, rather than showing a blank scoreboard that looks broken.
  Kind: ux.
  Source: user-request-2026-08-24.

- 💭 [GHUB-0119] **A short address for the score book, blocked at the registrar.**
  The owner asked for csc.antsprojectshub.co.za instead of
  milnet01.github.io/games-hub/scorepad/. Everything on our side is
  ready: the subdomain is unused, the domain already resolves to GitHub
  Pages by A record, so it is one CNAME -- host `csc`, target
  `milnet01.github.io` -- plus the custom domain field in the repo's
  Pages settings.

  Blocked at TrueHost, who registered the domain: the Manage page offers
  Overview, Auto Renew, Nameservers, Registrar Lock, Contact Information,
  Private Nameservers, Cloudflare Push and AI Website Builder, and no DNS
  record editor.

  That was first written up here as TrueHost not offering record editing
  without a hosting package, and that is WRONG. Their support chat gave
  the real cause on 2026-08-24: "the domain's DNS zone is not available
  in the Client Area, so the support team must restore or create it
  before this option will work". The zone is missing or broken rather
  than withheld, the editor is expected back with it, and support have
  taken the CNAME as a ticket.

  Which introduces the risk this bullet now exists to guard. A zone that
  is RECREATED rather than restored may come back without the records
  that are serving the site today, and nothing warns anyone before the
  site goes dark. Captured 2026-08-24 while still live, and these are the
  whole exposure: A on the apex to 185.199.108.153, .109.153, .110.153
  and .111.153, and www as CNAME to milnet01.github.io. There are no MX
  records at all, so no email rides on this zone -- which is the single
  biggest thing that could have gone wrong here and does not apply.

  Three routes, and the ordering is deliberate. A support ticket asking
  TrueHost to add the one record: no risk, a day or two. Cloudflare Push,
  which would give full DNS control free and forever, but moves DNS for
  the WHOLE domain including the live website and the email -- NOT
  recommended, and it was declined here rather than deferred, because
  risking a working site for a shorter address to a Canasta scorepad is
  not a trade worth offering. Or leave it, which costs nothing: the
  address is typed once per phone and then lives on a home screen.

  Two things to know if this is picked up. Setting a custom domain on
  this repo makes the score book answer at csc.../scorepad/ rather than
  at the root, since Pages serves the whole repository -- landing the
  root on the score book needs either a redirect at the repo root or a
  Pages-from-workflow deploy publishing scorepad/ alone. And the working
  address keeps working either way, so nothing has to be switched over in
  a hurry.
  **Layman:** The score book works, but its web address is long; a shorter one needs a change the domain provider will not let us make ourselves.
  Kind: chore.
  Source: user-request-2026-08-24.

- 📋 [GHUB-0141] **Make the shared room safe to join and to leave.**
  The book-destroying join and the stranded Connecting panel are
  fixed. Still open: sanitise() whitelists fields but does not coerce
  types, so a room written with numeric names throws inside render()
  and freezes a watcher's phone -- reachable by anyone who guesses a
  four-letter code. Sharing claims a code with no existence check and
  nothing ever deletes a room, so a new share can land on a live book.
  Take over scoring makes a second scorer without demoting the first,
  and scorerId is written and read nowhere. A failed connection
  reports nothing at all.
  **Layman:** Sharing a game has a few ways to go wrong that nobody would spot at the table.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0142] **The service worker caches every successful GET, cross-origin included.**
  sw.js has no URL filter: the fetch handler caches any ok response
  and then serves cache-first forever. If the database client falls
  back to HTTP long-polling -- its normal behaviour on a restrictive
  network, which is the kitchen-table case the file is written for --
  those responses are cached and replayed, and sync breaks with no
  invalidation short of clearing site data. Cache same-origin plus the
  pinned SDK prefix only, skip the database origin, and pass the put
  promise to waitUntil.
  **Layman:** The offline cache can swallow the live database traffic and break syncing for good on that phone.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0143] **The score book README describes a setup that cannot work.**
  It says to open index.html in a phone browser and add it to the home
  screen. Neither the service worker nor the manifest installs from
  file:// -- both need a secure origin -- so the headline offline
  feature needs the directory hosted, and no step says so. The setup
  steps also omit bumping CACHE in sw.js, which its own comment says
  is required or the old copy is served forever. And its claim that
  the drift guard catches a house rule changed in the game is wrong:
  scorepad-check.py compares the phone against the Rules struct
  defaults, while the opening minimums are editable in the House
  dialog and stored in QSettings.
  **Layman:** Following the instructions as written would not give you a working offline score book.
  Kind: doc-fix.
  Source: review-code sweep 2026-08-31.

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

- ✅ [GHUB-0066] **Six games have no rules core, and they are exactly the six whose rules nothing tests.**
  CLAUDE.md opens the architecture section with the rule: every game is a rules
  core plus a view, the core never includes a widget, *that split is the reason the
  whole collection is testable without a display, and it is the rule to preserve
  when adding a game.* Eight games follow it. Six do not, and nobody noticed
  because the consequence is silent.

  Klondike, Spider, FreeCell, Pyramid, Snake and 2048 ship exactly one `.cpp` each
  — `klondikeview.cpp`, `spiderview.cpp`, and so on. There is no board, no engine,
  no grid: the rules live inside the widget. `GAME_CORE_SOURCES` lists thirteen
  files and none of them belongs to those six, and `gameshub_selftest` links only
  the cores. **So the self-test cannot reach their rules even in principle.**

  The coverage numbers fall out of that exactly, counting how often each game is
  named in `tests/selftest.cpp`: Canasta 378, Hearts 144, Chess 116, Draughts 43,
  Reversi and Sudoku 27, Minesweeper 19, Pinball 18 — then Spider 6, Klondike 1,
  and FreeCell, Pyramid, 2048 and Snake at **zero**. Snake is zero in the UI test
  as well, so nothing anywhere touches it. A keyword count is not a coverage
  measurement and should not be quoted as one, but the gap between 378 and 0 is
  not a measurement artefact, and it lands on precisely the six games named above.

  **This matters now rather than eventually, because three queued items all edit
  these games.** GHUB-0056 changes how their undo copies snapshots. GHUB-0052
  wants to fuzz the very `restoreState()` implementations they own. GHUB-0065 adds
  animation to their auto-moves. Three changes to six games whose rules no test
  would notice breaking.

  The fix is the rule the project already wrote down: lift the rules out into a
  core per game and let the self-test link it. The solitaires then get an obvious
  and powerful property, and the machinery for it is already built — deal, play
  legal moves at random until the game is won or stuck, and assert after every
  single move that the pack is still intact, which is exactly what
  `cardcodec::matchesPack` and `fitsPack` do for saves today. 2048's tiles must
  always be powers of two; Snake must never occupy a square twice and must die
  exactly when it leaves the grid or meets itself.

  Worth doing one game at a time, cheapest first, rather than as a six-game
  refactor. Pyramid and FreeCell are the smallest views; Klondike is the one most
  other code resembles.

  The UI test does construct and render all six, so *does it crash while
  painting* is covered. It is the rules that are not.
  Premise updated (2026-08-20). The sentence this bullet quotes from
  CLAUDE.md no longer reads that way: all three lanes of that file's
  cold gate found the same defect independently, and the architecture
  section now states the split as the rule for a new game AND names
  the six games that do not hold it, with the consequence spelled out
  — a rules check for one of them will not link until a core is
  extracted, and this bullet is cited there as the open item.

  So the quoted text is a record of what the document said when this
  was filed, not of what it says now. The item itself is unchanged and
  still open: six games still have no core, and their rules are still
  reachable only from a UI test. What changed is that the gap is
  written down where someone would hit it.
  Progress (2026-08-25): SNAKE done, one of six. `src/snake/snakeboard.cpp` is in
  GAME_CORE_SOURCES, the view is a clock and a painter over it, and the self-test
  has a Snake section for the first time -- it had zero mentions there AND zero in
  the UI test, so it was the one game in the collection nothing anywhere touched.

  The property check is the shape this bullet asked for: play sixty games, and
  before every single step work out from the current position what that step must
  do, then hold the core to it. It measures 168 meals and 17 deaths by
  self-collision, which is what makes the tail-square rule -- running into the
  square your tail is about to vacate is legal, unless you just ate -- actually
  exercised rather than merely present. A pure random walk managed 8 meals in 60
  games and reached none of it; the policy steers at the food 75% of the time and
  still never avoids death.

  **It found a shipped bug on its first run**, filed as GHUB-0125: the snake was
  built with `push_front`, so its head sat at the LEFT end of a snake heading
  right and the first step drove it into its own neck. Pressing Right to start was
  an instant game over. This bullet predicted exactly that, and the proof is that
  `tests/uitest.cpp` already pressed Right to start Snake and only ever checked
  that the clock was running -- the snake was dead in that very check.

  Five to go: 2048, Pyramid, FreeCell, Klondike, Spider.
  Resolved (2026-08-25): all six done, one game at a time, cheapest first --
  Snake, 2048, Pyramid, FreeCell, Klondike, Spider. `GAME_CORE_SOURCES` now lists
  nineteen files and every one of the fourteen games has a core the self-test can
  link. CLAUDE.md's architecture section is corrected: the split holds for all
  fourteen.

  **Two shipped bugs, both of the kind this bullet predicted, and both found the
  moment the rules could be reached.**

  GHUB-0125: Snake was built with `push_front`, so its head sat at the LEFT end of
  a snake heading right and its first step drove into its own neck. Pressing Right
  or D to start was an instant game over. `tests/uitest.cpp` ALREADY pressed Right
  to start Snake and only ever checked the clock was running -- the snake was dead
  in that very check.

  GHUB-0126: FreeCell and Klondike lifted a dragged run off its pile and then
  snapshotted for undo at DROP time, so undoing a finished move restored a table
  the cards had never been on and they were gone. Both games' saves demand the
  whole pack back, so an undo followed by closing the app threw the game away.
  Spider had the identical ordering and a hand-written patch working around it --
  so somebody hit this once, fixed it in one of three places, and never looked at
  the other two. In the cores the snapshot is banked at LIFT time and the patch is
  unnecessary rather than merely correct.

  The property tests are the shape this bullet asked for, and each one is checked
  against the pack after EVERY move: Klondike and FreeCell must hold the whole
  pack, Pyramid the cards on the table plus the cards taken, Spider two packs less
  thirteen for every run completed. Snake and 2048 have no pack, so theirs are the
  arithmetic ones -- every tile a power of two, death earned exactly by leaving the
  grid or meeting itself.

  Three things learned about writing these that are worth keeping:

    - A property test is only as good as its policy. Snake's random walk ate 8
      times in 60 games and never grew, so the tail-square rule was never
      exercised; steering at the food 75% of the time gets 168 meals and 17
      self-collisions. Pyramid cleared 0 of 120 games until its draw branch was
      fixed, then 1.
    - REPORT what random play cannot reach rather than asserting it. Spider
      completes 0 runs in 60 random games, and 2048 never reaches 2048. Both have
      a built position instead; a threshold would have been a check that passed by
      luck.
    - An assertion with a side effect changes what the checks after it look at.
      FreeCell's "a card above the run cannot be picked up" runs on a COPY, because
      a successful lift would take cards off the table and bank an undo.

  Three of the queued items this bullet named as its reason are now safe to do:
  GHUB-0056 (undo snapshots), GHUB-0052 (fuzzing restoreState) and GHUB-0065
  (animating auto-moves) all edit these games, and now a test would notice.
  **Layman:** Six of the fourteen games have their rules mixed into the drawing code, which is why the test suite cannot check them at all.
  Kind: refactor.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0075] **Nothing checks that a save written by an older build still loads.**
  Ten of the fourteen games save, and each stamps its own
  save-format version and refuses anything else —
  KlondikeView::restoreState returns false unless the quint32 it
  reads is 1. A refused save is not a crash: the hub keeps the fresh
  deal it already made.

  Which is exactly what makes this dangerous. The app still runs,
  nothing looks broken, and the player's half-finished game is gone
  without being told. docs/standards/versioning.md § 3 calls that a
  breaking change and requires a MAJOR for it — and § What checks this
  records, honestly, that nothing enforces it.

  What would: a stored corpus of save blobs, one per saving game,
  written by the build of the day and checked into the tree, with a
  test that restores each one and asserts it loads. A deliberate
  format change then reddens the suite and the author has to choose —
  bump the MAJOR, or write a migration — rather than finding out from
  a player.

  Not large: saveState() already produces the blob, so the corpus is
  generated once per game and the test is a loop. The judgement is
  whether a refused old save should ever be acceptable, and § 3 says
  it is not without a MAJOR.
  **Layman:** If a change quietly makes old saved games unreadable, the game you left half-finished just disappears and nobody is told.
  Kind: test.
  Source: in-session-2026-08-20 (docs/standards/versioning.md § 3).

- ✅ [GHUB-0090] **Nothing can photograph a game, so every layout question is answered as arithmetic.**
  GHUB-0081 found seven layout defects and every one of them was found by
  the owner's eye, because nothing else here can see. GHUB-0084, GHUB-0085
  and GHUB-0088 were then closed by measuring rectangles and reasoning
  about them -- correct, and blind. The suite renders every game to a
  QPixmap already, to force paintEvent through; it just throws the picture
  away.

  A `--shot <path>` flag that renders the game named by `--game` at a size
  given by `--size WxH` and exits. Offscreen, so it needs no display, no
  compositor and no injection tool -- which means it works unchanged on
  this machine under Wayland, on the wintest box over plain SSH, and on a
  CI runner. `--legible` forces the switch on for the shot without writing
  it to settings, because both states are what wants looking at.

  Watch two things this project already knows. `--version` is answered from
  argv before QApplication exists and must stay that way; a shot needs Qt
  up, so it belongs after the parser rather than beside that check. And the
  offscreen platform has no font environment on windows-2022, so a shot
  taken there is evidence about that runner and not about a Windows
  player.
  Resolved (2026-08-22): `--shot <file>` with `--size WxH` and `--legible`, taken before the launch counter is advanced, since photographing is not playing. Two guards found by executing every branch rather than reading them: an unknown `--game` REFUSES rather than falling back to the tile grid the way playing does, and a malformed `--size` refuses rather than silently using another size -- in both cases the picture would still have been written and would still have looked like an answer. Errors go to stderr directly rather than through qWarning, which prints nothing here. Two ctest cases cover it, the second asserting the refusal via WILL_FAIL. Its first picture found GHUB-0092, in a game the GHUB-0081 eyeball check had already been over.
  **Layman:** The app cannot take a picture of itself, so checking whether something looks right means either doing sums or asking you to look.
  Kind: implement.
  Source: user-request-2026-08-22.

- ✅ [GHUB-0091] **The Windows half of every push is unverified until GitHub runs it, and there is a Windows machine sitting right there.**
  `scripts/local-ci.sh` executes ci.yml's own steps and says on every run
  that it cannot drive MSVC. So the Windows leg -- the only place MSVC is
  exercised -- is checked by CI and nowhere else, and this project has
  already spent three red Windows runs learning that.

  The owner's `ssh wintest` box is a real Windows 10 22H2 machine and the
  owner has granted admin rights and permission to install on it
  (2026-08-22). Verified that day: no Qt, no cmake, no compiler on PATH.
  Installing the MSVC build tools plus a Qt 6 matching ci.yml's would make
  the Windows half testable before a push.

  **It does not replace the runner, and assuming it does would be the
  expensive mistake.** wintest is a real desktop with Segoe UI installed,
  while ci.yml runs under QT_QPA_PLATFORM=offscreen on windows-2022, where
  QFontDatabase::families() is EMPTY and the default face measures digits
  at 0.997 of an em. Anything derived from font metrics behaves differently
  on the two. This buys "what a Windows player sees", which is a real
  question here; it does not buy "what the runner does".

  Blocked-by: nothing. Sized as its own task rather than folded into game
  work -- multi-gigabyte unattended installers driven over SSH.
  Resolved (2026-08-24). `scripts/wintest-ci.sh` now builds and tests the
  Windows leg on the wintest box in one command: it ships a commit over
  with `git archive` piped to the box's own tar.exe, then runs
  `scripts/wintest-build.ps1`, which executes ci.yml's three Windows
  commands VERBATIM and supplies only the environment the runner gets from
  actions -- vcvars64 for msvc-dev-cmd, CMAKE_PREFIX_PATH and Qt's bin on
  PATH for install-qt-action.

  Verified end to end from a wiped directory: configure, build, and
  4/4 tests passing in 44 s. Four rather than six because `prepush` and
  `scorepad` are `if(UNIX)`-guarded in CMakeLists, which is what CI does
  too.

  What is on the box now, all reproducible from the two scripts plus this
  note. MSVC 19.44.35228.0 (VS 2022 Build Tools, toolset 14.44) at the
  default location; Qt 6.8.3 msvc2022_64 with qtmultimedia under C:\Qt;
  CMake 4.4.2 and Ninja 1.13.2 portable under C:\devtools, the same
  versions the Linux box builds with so tooling is never the difference.
  12.9 GB free afterwards, of 111 GB.

  THE OWNER'S SECOND DRIVE DOES NOT EXIST. He recalled the box having more
  than one; Win32_LogicalDisk reports only C:. That is why the MSVC
  install is two components rather than the VCTools workload with
  --includeRecommended.

  Four things fought back, all recorded because the next person hits them.

  winget is BROKEN on that box -- "Data required by the source is missing",
  and `winget source reset` leaves the winget source "Cancelled". Every
  install here goes to the official URL directly instead, which is more
  deterministic anyway. aqtinstall's standalone aqt.exe is what installs
  Qt, so the box needs no Python.

  The VS bootstrapper exited 87 (ERROR_INVALID_PARAMETER) writing no setup
  log at all, because it self-updates its installer to 4.9.50 -- a VS
  2026-era build -- before parsing arguments, and rejected the two
  component ids I had hardcoded. Pinning --channelUri to the VS 2022
  channel and asking for the VCTools workload worked first time. ci.yml
  pins windows-2022 for the same underlying reason.

  Three quoting traps, all one family: Windows sshd hands the remote
  command to cmd.exe. (1) `-Src 'C:\gameshub'` arrives WITH the quotes,
  because cmd does not strip single quotes and `powershell -File` takes
  its arguments literally -- every Test-Path then fails on a path that
  exists. Use double quotes. (2) `cmd /c ver` fails on a box that is up,
  mangled to `ver"`; the reachability probe uses powershell instead, which
  is what the build needs anyway. (3) A multi-line PowerShell command is
  silently not run, so the destination directory was never created and tar
  failed with "could not chdir". All three carry a comment saying why, so
  they are not tidied back.

  WHAT THIS DOES NOT BUY, and the bullet said so before the work started:
  wintest is a real desktop with Segoe UI. ci.yml runs the same binaries
  under QT_QPA_PLATFORM=offscreen on windows-2022, where
  QFontDatabase::families() is EMPTY and the default face measures digits
  at 0.997 of an em. Anything font-derived can pass here and fail there --
  which is exactly the shape of the three red Windows runs this project
  has already paid for. A green run here is not a reason to skip watching
  CI, and both scripts plus local-ci.sh's summary now say so where they
  are read.

  local-ci.sh no longer claims the Windows leg runs only on GitHub, in its
  header and in its closing line, since that is no longer the whole truth.
  **Layman:** Your spare Windows PC could test the Windows build before we push instead of three minutes after, once it has the build tools on it.
  Kind: test.
  Source: user-request-2026-08-22.

- 📋 [GHUB-0093] **Two pictures of the same card game are never the same picture, so nothing can be compared.**
  GHUB-0090's `--shot` lets a layout be LOOKED at, which is most of the
  value. It does not let one be COMPARED, and the difference was measured
  while fixing GHUB-0092: a before-and-after pixel count came back at 10633
  changed pixels, and the control -- two shots taken from the SAME binary
  -- came back at 6416. The deal is random per launch, so the number was
  measuring the shuffle.

  That matters because a pixel diff is the standard way to show a visual
  change did what was intended and nothing else, and here it is unavailable
  for the six card games, which are exactly the ones whose layouts are
  hardest to reason about.

  A `--seed <n>` that pins the shuffle for the shot would make two runs
  comparable. `shuffleCards` already takes its index from `rng()` directly
  and is a hand-written Fisher-Yates precisely so a seed means the same
  deal on every compiler, so the machinery is there; what is missing is a
  way to say which seed from the command line.

  Not needed for looking at a layout, which is what the flag is for today.
  Needed the moment anyone wants to prove a visual change is confined to
  the thing it was aimed at.
  **Layman:** Every launch deals a different hand, so two screenshots of the same game cannot be put side by side to see what a change did.
  Kind: enhancement.
  Source: in-session-2026-08-22 GHUB-0092 fix.

- 📋 [GHUB-0105] **The self-test failed once under ctest and has not failed again in forty-five runs.**
  Observed 2026-08-24 while shipping GHUB-0096. `ctest` reported
  `selftest ***Failed` once. The same binary had passed
  immediately before and has passed every time since: 25 direct
  runs, 6 ctest runs of selftest alone, 3 full ctest runs and a
  10-run ctest loop. No FAIL line was captured, so the failing
  check is unknown.

  What was ruled out. canastaMatch, the obvious suspect because
  its margins are thin (hard v medium won 68 of 120 against a
  bar of 61), is fully seeded -- Engine::newGame(seed), Ai::seed
  per seat, and nextHand() deals from the already-seeded m_rng.
  The reversi two-second search budget was measured at 4 ms, so
  it is not close. Neither is evidence-free, but neither
  explains it.

  Worth knowing while chasing it: a single unexplained red run
  is the same signal as a green run over a stale binary, and
  this project has a documented trap for exactly that -- chaining
  a build and a test on one shell line races the linker. That is
  NOT what happened here (build and run were separate calls),
  which is what makes it worth an item rather than a shrug.

  Not caused by that session's changes: the engine additions were
  new functions plus two display-only Rules fields, and the view
  is not linked into gameshub_selftest at all.

  Next step is to catch it with output rather than to guess:
  loop ctest with --output-on-failure and keep the log until it
  reproduces.
  **Layman:** One test run went red and nothing since has reproduced it, so something in the suite is not as repeatable as it looks.
  Kind: investigate.
  Source: in-session-2026-08-24.

- ✅ [GHUB-0110] **The ladder that guards every AI change is too blunt to measure one.**
  canastaLevelsDiffer() is the project's only judge of an AI change --
  CLAUDE.md says outright that any change to one level has to be
  re-measured against its neighbours, and it is what caught Hard playing
  weaker than Medium. Measured 2026-08-24, it has almost no headroom on
  its top rung.

  The numbers. "expert beats hard" plays 240 seeded games and asks only
  for a bare majority, so it needs 121. Baseline is 129 -- about +1.16
  sigma on 240 coin flips, eight games clear of failing. Re-run at 1200
  games the baseline is 621, or 51.75%, so Expert's true edge over Hard
  is roughly 1.75 points of win rate and the average score margin is +36
  a game against the +3000 the easy rungs show.

  Two consequences, and the lane runs into both. An edge that small
  needs roughly 3300 games to stand at 2 sigma, so at 240 the check
  passes partly on luck. And ANY perturbation of canastaai.cpp moves it
  by more than its whole margin: GHUB-0101's guard took it to 113/240
  and GHUB-0104's to 115/240, and those two changes are not the same
  kind of thing -- one was wrong on the rules and one was right on them.
  The instrument failed both identically, which is the definition of not
  measuring.

  GHUB-0099, GHUB-0100, GHUB-0103 and GHUB-0104 all edit canastaai.cpp,
  so every one of them lands in front of this check. GHUB-0102 does not
  -- it is vocabulary, and its surfaces are the view and the refusal
  strings -- so it is the one item of the six that can be built and
  judged today.

  Routes, none picked. Raise the game count -- honest but slow, and 3300
  games is roughly 25 seconds inside a suite that runs in three. Judge
  on average score margin rather than win count, which carries more
  information per game. Judge these items by targeted position checks
  instead, the way canastaAiHoldsWhileFrozen and canastaFirstRoundSafeThrow
  already do, and let the ladder guard only against a rung INVERTING.
  Or take the reading at face value and treat "Expert is barely stronger
  than Hard" as the defect to fix, which is what the six AI items are
  for anyway.

  The last is probably the real finding: the ladder is not only blunt,
  it is reporting that the top two rungs are nearly the same player.
  Resolved (2026-08-24), owner picking the third route: the ladder
  guards against a rung going BACKWARDS and no more, and what a single
  new judgement does is locked by a hand-built position instead.

  canastaLevelsDiffer()'s top two rungs now call notTheWeakerPlayer(),
  which asks only that the stronger level sit no more than two standard
  deviations below an even split -- sqrt(games) either side of games/2,
  so 105 of 240 for expert v hard and 49 of 120 for hard v medium. The
  two rungs against Easy keep the bare-majority claim, because they win
  by miles and the sample carries it.

  Proved it still catches what it is for, rather than catching nothing,
  by mutation on both call sites. Expert made never to take the pile:
  expert v hard falls to 10/240 at -3241 a game and the check reddens.
  Hard made never to take the pile: hard v medium falls to 6/120 at
  -2841 and reddens, and the untouched strict rung "hard beats easy"
  reddens at 1/24. Restored, and the suite is green at 416 checks with
  all five ctest tests passing.

  Left undone deliberately: CLAUDE.md's Canasta paragraph still
  describes the ladder without this bound. Nothing it says is now false
  -- each rung is still checked against the one below and it still
  catches a rung inverting -- but it under-describes, and adding the
  bound would change what a conformer does and so owes a review-contract
  gate. Same reason GHUB-0079 is deliberately unfixed. The reasoning
  lives in the comment above canastaLevelsDiffer, which is what a
  developer reads when the check stops them.
  **Layman:** The test that decides whether a change to the computer player helped cannot actually tell a small improvement from luck, so it blocks good changes and bad ones alike.
  Kind: investigate.
  Source: in-session-2026-08-24 GHUB-0101 and GHUB-0104 attempts.

- 📋 [GHUB-0138] **Wire legibility-check.py into ctest, and fail loudly when a checker is absent.**
  scripts/legibility-check.py has no add_test and no CI step, though
  GHUB-0017 names --thresholds as INV-4's test; it passes today only
  when run by hand. And CMakeLists registers the prepush and scorepad
  tests inside if(BASH_PROGRAM) and if(Python3_Interpreter_FOUND), so
  on a configure without either the test simply is not registered and
  ctest reports 100% passed over a smaller suite -- against
  local-ci.sh's own stated principle that an absent linter must be
  called out.
  **Layman:** Two checks can silently not run, and a green test report looks the same either way.
  Kind: test.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0139] **Give check-code a .clang-tidy, and reconsider suppressing unusedFunction.**
  clang-tidy has no config in the tree, so its resolved check set is
  empty: it prints usage and exits without reading a file. Eight lanes
  reported findings in its territory -- narrowing conversions, signed
  char to cctype, integer overflow on restore, unguarded front() and
  pop_back(). Separately, cppcheck runs with unusedFunction suppressed
  on every run, and five lanes found dead public symbols that no tool
  therefore decided: forgetHistory in four games, Minefield::reset,
  SudokuGrid::clearMarks, Ai::freezesThisHand, Board::fullmoveNumber.
  **Layman:** One of the three C++ analysers is analysing nothing at all.
  Kind: test.
  Source: review-code sweep 2026-08-31.

- 🚧 [GHUB-0140] **Lock the fixes that landed without a regression test.**
  The audit added and proved red: the Canasta House-rule tails, the
  seat's own discards, the Pyramid save bound and Snake's turn queue.
  The rest went in without one, and these are the behavioural ones
  worth locking: the three engine games' stale-timer guard, the six
  save-deletion fixes, Hearts' trick lift and Next Hand action,
  FreeCell's supermove source column and its fan compression, and
  Sudoku's toolbar sync. The stale-timer guard is the one to write
  first -- it is the run's only CRITICAL class and its symptom was a
  deadlocked board.
  Progress (2026-09-02): the stale-timer guard is locked. Twelve
  checks in tests/uitest.cpp, one set per engine game: the player's
  move puts the game on the computer's clock, a move left standing is
  answered, and a move taken back before the timer fires leaves the
  board untouched and the turn still the player's. The wait is derived
  from the reply the engine on that machine just took, so nothing is
  held against a wall-clock or font-derived constant.

  Proven red on the real tree at the pre-87d97c0 state: chess reported
  "Computer played d2-d4" with White the human, reversi's discs went
  2-2 to 3-3, and draughts came back "you must take". Six checks red
  with expected-against-actual.

  Measured, and it changes what this locks: reverting EITHER guard
  alone leaves the test green, because either one on its own still
  stops the engine moving. So the checks are pinned to the behaviour --
  the engine never plays your move -- and not to a line. Removing one
  guard leaves the app correct and the defence in depth gone, and
  nothing would notice.

  The other fixes named here are still open.
  **Layman:** Some of the audit fixes have nothing stopping them coming back.
  Kind: test.
  Source: review-code sweep 2026-08-31.

- 📋 [GHUB-0163] **smallestCardWidth cannot tell a game with no cards from a game that forgot.**
  gameview.h: the base returns 0.0 and the header documents 0 as "a
  game that draws no cards" -- so the inherited value and a forgotten
  override are the same value, and cardsKeepTheirFaces skips that game
  without saying so. Its checked >= 6 floor is already met by the six
  games that do override it, so a fifteenth card game that forgets is
  caught by nothing and ships drawing faces too small to read. This
  sweep found the related half: Hearts DID override it and returned the
  wrong card, publishing a floor overstated by a factor of 0.9, and
  nothing could see that either. Return -1.0 from the base meaning "not
  answered", have a genuinely cardless game say 0.0 explicitly, and let
  the check fail on -1.
  **Layman:** The check that stops a card being drawn too small to read skips any game that forgets to answer it, silently.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

- ✅ [GHUB-0164] **The last of the small findings, so the sweep leaves nothing unrecorded.**
  Everything the sweep found at LOW that no other item above carries.
  HUB: closeEvent stores state but never calls deactivate() on the
  current view, unlike showMenu and openGame; currentView() is a
  fourteen-entry string scan run on every statusChanged, including
  Pinball's per-frame ones.
  SCORES: names[qBound(0, difficulty, 2)] silently aliases an
  out-of-range difficulty onto the hard key, so a fourth Reversi level
  would share a stored best with the third.
  CANASTA ENGINE: dealFrom discards placeRedThrees' return where
  drawFromStock treats false as "stock ran dry"; meldableRanks can never
  return 3, so the four-black-threes finish -- the one ending
  goingOutNeedsADiscard exists to permit -- is never highlighted on the
  board, and the highlight is the affordance; freezeCardIndex can be -1
  while pileFrozen() is true after a restore, and the view consumes
  both.
  CANASTA VIEW: a click on an empty pile during Draw is a completely
  silent no-op where every other refusal explains itself; leaveEvent
  clears the two hover fields and not m_overButton; meldCardCentre
  allocates and sorts per call inside meldRankAt's double loop.
  DEAD SYMBOLS no tool decided, because cppcheck runs with
  unusedFunction suppressed: forgetHistory() in four games with zero
  callers including tests, Minefield::reset(), SudokuGrid::clearMarks(),
  Ai::freezesThisHand(), Board::fullmoveNumber(), Sound::volume() and
  setVolume(), GameView::lastStatus().
  Resolved (2026-09-02). Every finding has a disposition; two of them
  are deliberately not a code change and are named below rather than
  left looking like an oversight.

  FIXED. closeEvent now deactivates the view it is leaving, as every
  other page change does -- Pinball's ball could otherwise drain, and
  record a score, after the window was closed. The statusChanged
  filter compares against the stack's current widget instead of
  walking every entry comparing page names, which Pinball was driving
  once a frame. Scores' two key builders stop clamping an unknown
  level onto the last name, which made it SHARE that level's record; an
  unrecognised level gets a key of its own, wrong in name only and
  unable to corrupt a real one. dealFrom stops discarding
  placeRedThrees' return -- the existing short-stock guard covers the
  hands but not the replacements, and newGameFromStock can hand over
  exactly enough to deal and nothing spare. meldableRanks now offers
  rank 3 when the hand holds all four black threes, so the one finish
  goingOutNeedsADiscard exists to permit is marked; three checks in the
  selftest, the first proven red. A click on an empty pile during Draw
  says why instead of doing nothing. leaveEvent clears m_overButton,
  so the Lay Down button stops keeping its hover edge after the pointer
  has left the window.

  DEAD SYMBOLS. forgetHistory in four tables, Minefield::reset and
  Ai::freezesThisHand removed -- none had a caller anywhere, tests
  included. GameView::lastStatus() was listed as dead and IS NOT: the
  uitest reads it in six places. My first grep said otherwise because I
  truncated it with head; recounted without the pipe. SudokuGrid::
  clearMarks() was kept and given the caller that was duplicating it --
  set() wrote the same zero inline, so entering a digit now clears the
  marks through the one method that names the operation.

  KEPT ON PURPOSE, both now carrying a comment saying so.
  Sound::volume/setVolume are not dead: GHUB-0068's Preferences dialog
  names volume among the app-wide switches it will hold, so removing
  them would delete the API queued work needs.
  Board::fullmoveNumber() sits in a block of five position accessors,
  the other four of which are used, and is one of the six fields of the
  FEN this board reads; removing it alone leaves the position
  half-describable.

  NOT DONE, and why. freezeCardIndex can answer -1 while pileFrozen()
  is true after a restore -- the consumer degrades gracefully (no index
  matches, so no card is turned sideways) and the real fix is to stop
  the blob claiming it, which is GHUB-0127's job. meldCardCentre
  recomputing meldOrder per call was left alone: it is a hit-test and
  paint helper with no measured symptom, --bench cannot see it because
  a fresh deal has no melds, and CLAUDE.md's rule is to measure before
  and after rather than take one reading. Speculative surgery on it
  would be an orthogonal edit.
  **Layman:** The remaining minor items, written down so the list is complete.
  Kind: fix.
  Source: review-code sweep 2026-08-31.

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
  Three things this list does not say, added 2026-08-20 after the
  owner asked whether Backgammon was safe. It is — it is on the
  Standing rules "Safe, public domain" list by name — but the reason
  it is safe is not the whole licensing question.

  **The standing rules cover assets and rule TEXT, and not third-party
  CODE or data.** That is the gap, and it bites hardest on exactly the
  two games here with the hardest opponents. GNU Backgammon is a GNU
  project and therefore GPL, and its trained neural-net weights are
  the reason a strong backgammon bot is hard to write; GNU Go is GPL
  too. Lifting either the code or the weights into this MIT repository
  would be a licence violation, and "it is only a data file" is the
  form the mistake takes. A weak opponent written from scratch is
  compatible; a strong one borrowed is not. Same test as the assets
  rule, applied to code.

  **Backgammon would be the first game here with dice, and the dice
  land on a trap already documented.** cards/card.cpp explains why the
  shuffle is hand-written: the standard pins what mt19937 emits but
  not how std::shuffle or std::uniform_int_distribution consume it, so
  two implementations differ from identical state. A die roll must
  therefore come from rng() directly (rng() % 6) rather than
  std::uniform_int_distribution<int>(1, 6). It does not matter for a
  local game against the computer; it decides whether two machines can
  ever agree on a roll.

  **Every two-player game on this list grows GHUB-0080.** Backgammon,
  Nine Men's Morris, Gomoku, Four in a Row, Halma, Hnefatafl, Shogi
  and Xiangqi are all two-player, so each one added is another game
  that could take a human opponent — and GHUB-0080's scope is
  deliberately the games that already have one. Worth deciding
  together rather than separately.

  Also worth knowing when picking from this list: any new game should
  arrive with a rules core, because six of the fourteen do not have
  one and are untestable for it (GHUB-0066). A fresh game is the cheap
  time to get that right.

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
