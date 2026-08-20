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

### ⚡ Performance

Nothing in this collection is slow in the way a spreadsheet is slow. The cost

shows up in the two places a game can afford it least: a window that stops

answering while the computer thinks, and a laptop fan that comes on and stays

on.

Every item here was found by reading the source on 2026-08-20, and the first one

carries a measurement rather than an opinion. The last one exists because three

of the four cannot be proved fixed with anything this project currently owns —

which is the reason it is filed rather than an excuse for not filing the rest.

- 📋 [GHUB-0046] **A game you have left keeps playing itself, and Pinball costs a fifth of a CPU core to do it.**
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
  **Layman:** Open Pinball once and the fan stays on for the rest of the session, even while you are playing Chess.
  Kind: fix.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0047] **The computer thinks on the drawing thread, so the window stops answering while it does.**
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
  **Layman:** When the computer is working out its move the whole window freezes -- it cannot even be resized until it finishes.
  Kind: perf.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0048] **Every card is drawn from scratch, every frame, and nothing is ever cached.**
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
  **Layman:** The card games redraw all fifty-odd cards sixty times a second, even the ones sitting still.
  Kind: perf.
  Source: in-session-2026-08-20.

- 📋 [GHUB-0049] **Nothing in the project can measure a frame, so no painting fix can be proved.**
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

- 📋 [GHUB-0052] **Ten hand-audited parsers, and nothing but hands checking them.**
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

- 📋 [GHUB-0056] **Undo copies the whole table on every move, and copies it again to undo one.**
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

- 📋 [GHUB-0065] **One game animates and thirteen teleport.**
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
  **Layman:** Only Canasta shows cards moving; everywhere else a card is simply somewhere else the next time you look.
  Kind: ux.
  Source: in-session-2026-08-20.

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

- 📋 [GHUB-0066] **Six games have no rules core, and they are exactly the six whose rules nothing tests.**
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
  **Layman:** Six of the fourteen games have their rules mixed into the drawing code, which is why the test suite cannot check them at all.
  Kind: refactor.
  Source: in-session-2026-08-20.

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
