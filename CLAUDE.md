# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A Qt 6 Widgets game collection — a hub window holding fourteen games: Chess,
Reversi, Draughts, Minesweeper, Solitaire, Spider, FreeCell, Pyramid, Sudoku,
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

# Warnings are on for every build (-Wall -Wextra, /W4 on MSVC) and are fatal
# only under -DGAMESHUB_WERROR=ON, which CI sets on the LINUX leg alone --
# MSVC's /W4 is a different set and GHUB-0185 owns measuring it. Turn it on
# locally before pushing code, or the Linux leg is where you find out.
# Release builds also harden: stack protector, FORTIFY_SOURCE, full RELRO,
# non-executable stack and PIE. Neither switch changes what the code does.
# `readelf -h build/gameshub` saying DYN is the only thing that proves PIE
# landed -- CMAKE_POSITION_INDEPENDENT_CODE alone compiles -fPIE and links no
# -pie, and every other hardening check still passes while it is missing.
# GAMESHUB_SANITIZE turns the hardening off, deliberately: ASan instruments the
# same paths and the two then report each other.

# Every source is compiled ONCE, into gameshub_core or gameshub_views, and the
# objects are linked into all three executables. Until GHUB-0187 the three
# targets listed GAME_CORE_SOURCES and GAME_VIEW_SOURCES directly, so a core
# file went to the compiler three times and a view file twice. Measured on this
# machine with ccache and mold off, which is how CI builds: a cold build fell
# from about 58 s to about 29 s, and 126 build steps to 65.
#
# The object libraries are not a faster compiler. They are the same work done
# fewer times, so nothing about the output moves -- and the hardening still
# lands, which is checked on the binary rather than assumed: readelf says DYN,
# BIND_NOW and a non-executable stack.
#
# They also turn the core/view split from a convention into something the
# compiler enforces. gameshub_core links Qt6::Core alone, so a rules core that
# includes a widget no longer builds -- verified by adding one and watching it
# fail. Before, only gameshub_selftest's link line said so, about its own copy
# of the file.
#
# The configure step picks up ccache and mold when they are installed and says
# so; -DGAMESHUB_FAST_BUILD=OFF turns both off. Neither changes what is built.
# ccache replays compilations it has already done, so a repeated cold build is
# a couple of seconds whatever the object count; the figures above are what a
# machine that has never built this pays, which is every CI run.
#
# Two things worth knowing. CI has neither ccache nor mold, so a runner builds
# the plain way and the shipped artifacts are linked by GNU ld -- if a local
# build is green and CI is not, the toolchain is one of the differences. And
# the peak is a single compiler process at roughly 660 MB, unchanged by the
# above, so -j on a machine short of memory is worth setting by hand: ninja
# defaults to cores plus two.

# --game takes the REGISTERED name, which is what the tile shows. Klondike is
# registered as "Solitaire" (Klondike is its blurb), and an unknown name warns
# and opens the tile grid rather than failing.

# Photograph a game instead of playing it. Needs no display, no compositor and
# no injection tool, so it works here under Wayland, over plain SSH, and on a
# CI runner. --legible turns large play on for the shot without writing it to
# settings; --seed pins the deal so two shots can be compared; --turns plays the
# game forward first. Use it before reasoning about a layout: this is the only
# thing in the project that can SEE one.
QT_QPA_PLATFORM=offscreen ./build/gameshub --shot /tmp/hearts.png \
      --game hearts --size 1400x620 --legible

# Unlike playing, an unknown --game REFUSES rather than falling back to the
# grid; a malformed --size refuses rather than picking another size; and --turns
# refuses any game that does not override the hook, which today is all but
# Canasta. A picture of the wrong thing is the
# one failure a screenshot cannot survive: it still gets written, and it still
# looks like an answer.

cd build && ctest --output-on-failure   # both test binaries, the --shot runs,
                                        # the hook test, and two Python checks
                                        # -- scorepad (the phone score book's
                                        # numbers against the game's) and
                                        # legibility (the kFaceMinWidth
                                        # threshold)
cmake --install build                   # refresh the installed copy
```

**A shot is taken the moment a game opens, which for a card game is a deal and
nothing else — so `--turns` plays it forward first.** Anything that only exists
once a hand has been played (Canasta's melds, its canasta stack, a frozen pack)
is otherwise invisible.

```bash
# A frozen pack, mid-hand, with the House rules in force.
XDG_CONFIG_HOME=/tmp/shot QT_QPA_PLATFORM=offscreen ./build/gameshub \
      --shot /tmp/canasta.png --game canasta --size 1400x760 --seed 5 --turns 44
```

`--turns` runs the game's own turns with every seat played by its computer,
**seat 0 included** — the board otherwise stops dead on your turn, which reads
as the flag not working.

`GameView::advanceForShot` is the hook, it returns false by default, and
**that default is the refusal**: a game refuses because it has not overridden
the hook, never because it has no turns. Chess and Hearts have turns and
computer opponents, and Snake has a clock that moves the game on without you;
all three refuse today, because Canasta is still the only override. Making a game photographable mid-play IS writing one.

Its contract is **synchronous, unanimated and silent**, which is a rule about
how the turns are DRIVEN rather than about tidying up afterwards.

**Drive them through the rules core, never through the game's own presentation
path.** That path advances the engine right enough — it is the animation it
adds that cannot survive: it launches flights, plays sounds and sets a pause
between turns, and those finish under a `QTimer` that cannot fire inside a
synchronous call. An override built on it therefore returns with cards still in
the air, and photographs them half-way to somewhere. **Then settle before
returning** — clear pending flights, selection, pauses and any celebration, and
re-sort.

Nothing catches either breach: the ctest case photographs Canasta only, and a
wrong picture still looks like an answer.

`--seed` pins the shuffle, so two runs deal the same cards and a before-and-after
pixel count measures the change rather than the deal. It reaches every deal,
board and computer seat through `src/dealseed.h`, and must be set before a game
is built — the seeds are member initialisers.

**So a new game takes its randomness from `dealSeed()`, as a member
initialiser**, and a game that reaches for `std::random_device` or
`QRandomGenerator` instead drops silently out of `--seed`'s reach. Nothing
catches that: the seeded ctest case never compares two shots, so it passes
either way.

**Point `XDG_CONFIG_HOME` at a scratch directory holding a hand-written
`GamesHub/Games.conf` for anything that depends on a stored setting**, so the
owner's real settings are never touched. Canasta's House set needs `useHouse=true`
as well as the `house\...` keys, **both under a `[canasta]` group**: the rule set
in force is a separate setting, and without it the toolbar comes up Classic and a
house-rule layout never appears. The keys are the ones `canastaview.cpp` writes,
not the `Rules` field names — `teeFreeze`, not `freezeCardMakesATee`.

This replaced a throwaway harness of five source edits that had to be rebuilt and
reverted each time. It found a four-canasta stack whose badges landed on top of
one another, and that stack hanging over the edge of its band — neither flagged
by any arithmetic in the project.

Run a test binary directly for its per-check output — `ctest` only reports
pass/fail:

```bash
./build/gameshub_selftest                             # all game rules
QT_QPA_PLATFORM=offscreen ./build/gameshub_uitest     # widgets and hub
QT_QPA_PLATFORM=offscreen ./build/gameshub_uitest --bench   # frame cost alone
QT_QPA_PLATFORM=offscreen ./build/gameshub_uitest --write-saves  # see below
tests/pre-push-test.sh                                # which arm the hook takes
python3 scripts/scorepad-check.py                     # score book vs the game
python3 scripts/legibility-check.py --thresholds      # kFaceMinWidth is unique
```

The last two are pure Python and run as ctest cases, so a check skipped here is
caught there. **The registered count differs by platform, and three cases
separate the extremes** — the two Python ones, which need an interpreter CMake
will use, and the hook test, which is bash and Unix-only. Linux registers all
of them; a Windows runner registers every one but the hook test; a Windows box
whose only `python.exe` is the Store stub registers neither Python case either.
The configure step says when it drops the Python pair.

**An absent checker registers a FAILING case rather than none at all, and that
is the rule a new tool-gated case has to follow.** A configure with no bash or
no python used to drop the test silently, and ctest then reported everything
passing over a shorter suite — the same silent skip `scripts/local-ci.sh`
refuses by name. So the `else()` branch is not optional: `prepush_needs_bash`,
`scorepad_needs_python` and `legibility_needs_python` exist to be red.

**A case is genuinely dropped only where the platform is exempted on purpose,
and there are two of those, not one.** The Python pair where there is no
interpreter CMake will use AND the platform is not Unix — configure prints why.
**That is not a Windows exemption**: the guard is `if(Python3_Interpreter_FOUND)`
with no platform test at all, so a Windows runner that HAS an interpreter runs
both — which is how a Linux-only `grep` pipeline inside one of them reddened
the Windows leg six times. And the hook test on any non-Unix platform, which is why its stub
sits INSIDE `if(UNIX)` rather than beside it: bash is what it runs, so a red
stub on Windows would be an alarm with nothing behind it. Put a new Unix-only
case's `else()` inside the same guard. **Count them with `ctest
-N` rather than against a figure here** — the number moves whenever a case is
added, and a stale one sends you hunting for a case that was never registered
on that platform.

**`--bench` is the only thing here that can time a frame**, and it is what
makes a painting change provable. It prints ms/frame for Canasta mid-deal and
at rest, a full Klondike tableau, a FreeCell board and the tile grid, and
**reports rather than asserts** — a frame time is a property of the machine.
Take it before and after, never one reading in isolation.

`CMAKE_INSTALL_PREFIX` must be set at **configure** time, not passed to
`cmake --install --prefix`. The `.desktop` file bakes in an absolute `Exec`
path via `configure_file`, so a late `--prefix` installs the binary correctly
while leaving `Exec=/usr/local/bin/gameshub` pointing at nothing.

The panel launcher runs the **installed** copy, so re-run `cmake --install`
after changing code or the pinned icon keeps launching the old build.

## Run the pipeline locally before pushing

```bash
git config core.hooksPath .githooks   # once per clone
scripts/local-ci.sh                   # or just push; the hook runs it
```

**`scripts/local-ci.sh` reads its steps out of `.github/workflows/ci.yml`
rather than restating them.** That is the whole point: a hand-written mirror
of a pipeline drifts, and then passes locally for a build that fails on
GitHub. It executes the workflow's own `run:` blocks in the workflow's own
order, and **stops on any step it has no rule for** — a new action added to
`ci.yml` fails the local run until `STEP_RULES` in the script accounts for
it, because a silently skipped step is exactly the drift being prevented.

Four things a plain run does not cover, and it says so every time rather than
implying coverage. **The Windows leg does not run here** — nothing on Linux
drives MSVC, so that half is verified by CI and nowhere else. The `uses:` steps
are stood in for by this machine's own Qt and Ninja rather than executed. And
the **sanitizer** and **clang-tidy** legs are off unless asked for:
`--with-sanitizers` and `--with-tidy`. The `pre-push` hook passes neither, so
the push gate does not touch them either — § Releasing has the whole list of
what CI runs.

The `pre-push` hook runs it automatically. A push touching only `.md` files,
`docs/`, `.gitignore` or the licence texts runs the workflow linters and stops; anything
touching code, CMake or a workflow runs the full pipeline. `SKIP_LOCAL_CI=1
git push` bypasses it when you mean to.

**The hook reads one line per ref and must accumulate across all of them.**
`git push --follow-tags` sends the tag *last*, and a new ref has no remote sha
— so a hook that let the last ref decide diffed a bare sha against the working
tree, found a clean tree, and called a release push a documentation change. It
ran lint-only on every release and every new branch, silently, because a push
succeeds either way. `tests/pre-push-test.sh` is the guard: it drives real
pushes at a throwaway remote and asserts which arm each one takes. Keep the
docs-only arm in it — that path is a feature, and the obvious "fix" of always
running the full pipeline deletes it.

**Two traps this script hit while being written**, both of which produce a
green run that checked nothing. `$(...)` strips NUL bytes, so the
NUL-separated step list came back empty and the run "passed" having executed
zero steps — hence the `STEPS_RUN` guard. And a newline-separated record
splits a multi-line `run:` block mid-body, so a step executes only its first
line, silently.

## Committing

**One commit per roadmap item where the work splits cleanly; group them when
splitting would produce a commit that does not build.** Owner's call,
2026-08-20, settling a question that had been decided in practice several
times and written down nowhere. The grouped case is real rather than
hypothetical: GHUB-0041 and GHUB-0042 both rebuilt `HubWindow::buildChrome()`,
so either half alone was a broken tree.

**Either way the BODY names every ID the commit covers, one at a time.** The
subject may abbreviate a run as `GHUB-0066..0070`, and this history does — but
that form contains no literal `GHUB-0067`, so a tool grepping for one finds
nothing. The body is what makes a grouped commit auditable, and every grouped
commit here already enumerates its IDs there. Check the body, not the subject,
before believing an ID never shipped.

## Releasing

Two workflows in `.github/workflows/`, contract in
`docs/specs/GHUB-0025-downloadable-builds.md`. `ci.yml` builds and runs
`ctest` -- every registered case, not just the two binaries -- on `ubuntu-24.04`
and `windows-2022` for every push **to `master`** and every pull request. That
includes the Python checks, which is how a Linux-only `grep` pipeline in one of
them reddened the Windows leg six times (GHUB-0171). A push to any other branch runs nothing, so
the Windows leg — the only place MSVC is exercised — does not run on branch
work until a pull request opens. `release.yml`
turns a tag into a Linux AppImage and a Windows zip on the releases page.

**`ci.yml` runs more than the two test binaries.** Three job definitions —
`build`, `sanitizers` and `tidy` — but `build` is a matrix, so GitHub reports
**four** checks: the Linux build, the Windows build, `Saved-game fuzz
(ASan/UBSan)` and `clang-tidy`. All on the same triggers.
`scripts/local-ci.sh` skips the last two unless given `--with-sanitizers` and
`--with-tidy`, and prints that it skipped them.

**A plain local run is green against the Linux build alone** — it drives no
MSVC, no sanitizer and no analyser. `scripts/wintest-ci.sh` is the only local
route to the Windows leg, and it exits without running anything when the box is
off. **The `tidy` job pins clang-tidy and a developer machine does not**, which
is deliberate: check families only grow, so whatever is installed locally is
the stricter of the two and CI should never be the first to see a finding.
That also means a local `--with-tidy` run is not proof the pinned version
agrees.

**The pinned version is what "stay at zero" measures against** — owner's call,
2026-09-06, closing GHUB-0182. Findings a newer local clang-tidy reports are
filed, not treated as breakage: check families only grow, so measuring the rule
against whatever happens to be installed would let an LLVM upgrade break it with
no code change. **So a local sweep is NOT expected to come back empty**, and
does not today: `performance-use-std-move` findings stand on save/restore paths
by deliberate decision, being outside what GHUB-0182 named and unmeasurable on
paths that run once. Do not "fix" them to reach a clean run. `bugprone-signed-bitwise` IS at zero across `src/`
and is worth keeping there.

**CI lints `src/*.cpp` only, so `tests/` is not analysed at all.** Its
fixed-seed findings are deliberate -- the seeds are what make two runs
comparable -- and cannot redden CI.

**Releases are NAMED as well as numbered, from 2026-09-08.** SemVer refuses to
say how BIG a release was. That is deliberate — it is why the first number
means "something broke" rather than "something large happened" — but it leaves
nothing saying a release mattered. A name says that without corrupting the
number that carries the warning. Put a `**Theme:**` line at the top of the
changelog section and `cut-release` reads it into the release title:
`1.1.0 — Play without a mouse`. Owner's call, 2026-09-08.

It lives here rather than in `docs/standards/versioning-overrides.md` because
it is a release PRACTICE and not a versioning rule. The machine-wide versioning
standard's § 9 routes anything it names no home for — a release cadence, and
this — to wherever the project keeps its own practices, which is this file.

Cutting a release is three edits, a check and a tag, **in this order**:

1. Bump `project(gameshub VERSION ...)` in `CMakeLists.txt`. **Which number**
   is `~/.claude/standards/versioning.md` § 2's to decide, with this project's
   breaking surfaces in `docs/standards/versioning-overrides.md` § 1. That
   standard's § 4 shifts the ladder down only inside `0.x`, and this project
   left `0.x` at `1.0.0` — so a new game is a MINOR.
2. Bump `Current version X.Y.Z` in `README.md`.
3. Close `## [Unreleased]` in `CHANGELOG.md` into `## [X.Y.Z] - <date>`, and
   leave a fresh empty `[Unreleased]` above it.
4. Run `.claude/bump.json`'s `post_check` and see it print `version X.Y.Z
   consistent`. **Nothing else runs it** — not a hook, not `local-ci.sh`, not
   the workflow — so a step skipped above is caught here or nowhere.
5. Commit, then `git tag vX.Y.Z && git push --follow-tags`.

**A roadmap ID on a changelog bullet LINE claims that item shipped.**
`cut-release` stops the release unless the roadmap shows every such item
shipped. An ID in the bullet's continuation prose is a cross-reference and
passes, so an item you only mention goes there, never on the bullet line.
`~/.claude/skills/cut-release/SKILL.md` owns the rule; GHUB-0065 hit it.

**`.claude/bump.json` is the one enumeration of version-bearing files** — it
names `CMakeLists.txt` and `README.md`, and its `post_check` is what catches
drift between them and the changelog. Read it rather than this list if the two
ever disagree; a file added there and not here is how this list goes stale.

The tag's `v` prefix is stripped and compared against `CMakeLists.txt`, and
the changelog must already have a `## [X.Y.Z]` block — the `verify` job
checks both before either build starts, because the release notes are read
from that block and a forgotten step would otherwise publish an empty one.
Get it wrong and the fix is to delete the tag; nothing is published.
**That job does NOT look at `README.md`**, which is what step 4 is for: skip
both and the release page still advertises the previous version, with nothing
having said so.

Every action is pinned to a commit SHA with the version in a trailing
comment. That is not decoration: these workflows publish binaries that
strangers download, and a moved tag on a third-party action would run
arbitrary code against them. `actionlint`, `yamllint` and `zizmor` all pass
clean and are the check before pushing a workflow edit.

## Core rules

**How the code is shaped, and why, is `docs/design.md`.** It holds the parts,
the `GameView` contract every game follows, saves, legibility, and a section
per game. Read the section for what you are changing before you change it.

**A game is a rules core plus a view, and the core never includes a widget.**
That split is what makes a game's rules testable without a display, and it is
the rule to hold when adding one.

- `CMakeLists.txt` splits `GAME_CORE_SOURCES` from `GAME_VIEW_SOURCES`, and
  `gameshub_selftest` links only the cores. **Two things put a file in the view
  half, not one.** Pulling in QtWidgets is the obvious one. The other is being
  something a rules core has no business reading — a display preference, a
  stored score, a session counter — **even when the file is QtCore-only**, and
  that is not a style rule: it is what keeps the self-test from acquiring a
  dependency on stored state. `legibility.cpp`, `scores.cpp` and `donate.cpp`
  are all QtCore-only and all live in the view half; `legibility.cpp` carries
  the reason inline. Judging by the QtWidgets test alone puts a new preference
  store in the core half, where it links, passes, and tells you nothing.

  **Since GHUB-0187 the compiler catches the FIRST of those two and still not
  the second, and the difference is the whole point of this bullet.**
  `gameshub_core` links `Qt6::Core` alone, so a core file that includes a
  widget does not build. A core file that reads a stored score compiles
  perfectly, links, and passes — exactly as before. Do not read "the split is
  enforced now" as covering the half that has never had a mechanical check.

**The owner is partially sighted, and reads cards by their pip pattern rather
than the corner index.** That is a design constraint, not a preference. It is
why melds put their wild cards first, why melded cards are drawn at 0.74 rather
than at the smallest scale that fits (below 46 pixels wide `CardArt::paintFace`
gives up on the face entirely), why the computer's pause is nearly a second,
and why the last discard is spelled out in words under the centre of the table
rather than left to be read off the pile. Anything added here is checked
against "can this be read slowly?" before "does this look neat?".

## Traps worth knowing

These bite any change, whatever game it touches. A trap about one game or one
shared part is in `docs/design.md`, with that game or part.

**Comparing two shots of a card game needs `--seed`, and without it the number
you get is the shuffle.** The deal is random per launch, so two runs of one
build differ substantially — two confident wrong readings came of that before it
was spotted (GHUB-0093), and both figures that item records are UNSEEDED, so
neither is a noise floor for a seeded run. With the same seed two runs are
byte-identical, measured with `cmp`, which is what makes a pixel diff mean
anything: any difference at all is the change. A
deterministic surface like the tile grid still matches itself with no seed at
all.

**`QStringLiteral` takes a UTF-16 literal, so UTF-8 escape bytes in one build
the wrong string.** `QStringLiteral("\xf0\x9f\x94\x8a")` is not the speaker
emoji; it is four separate UTF-16 code units, and it compiles and compares
clean against nothing. In a test that is worse than a wrong answer: a search
for it matches no line, the loop asserts over an empty set, and the check
passes over the very defect it was written for. Paste the character itself, or
key on something that is not text at all -- an object name, as
`theHubHasNamesToReadOut` does after being caught this way.

**`slots` is a Qt keyword macro and expands to nothing.** A local named
`slots` compiles as `const int = ...` and the error points at the `=`, which
reads as a parser bug rather than a name collision. `signals` and `emit` are
the same. Canasta's meld layout hit this.

**`windows-2022` under `QT_QPA_PLATFORM=offscreen` has no font environment at
all, and this is the number to remember: `QFontDatabase::families()` returns
EMPTY and the default face measures digits at 0.997 of an em** — the full em
box, which is a headless stub rather than any real typeface. Anything derived
from font metrics therefore degrades to its floor on that runner and must be
allowed to. It is not evidence about what a Windows *player* sees: a real
desktop has Segoe UI and behaves like this machine.

**The rule the three red runs taught: a test may ASSERT what the code does, and
must only REPORT what the platform happens to provide.** Each rewrite of Sudoku's pencil-mark test put
a fresh environment constant into an assertion — a tuned ratio, then a growth
multiple, then a minimum font count — and each passed locally and failed on
`windows-2022`. If a number describes the machine rather than the change, print
it and assert something else.

**A UI check that clicks a Canasta card has to wait twice** — once for the turn
to come round to the human, and again for the cards to land, since the board
ignores clicks while anything is animating. Testing only the first produced a
suite that failed about one run in three.

**A seed does not mean the same deal on two compilers, so `shuffleCards` is
a hand-written Fisher-Yates.** The standard pins down what `std::mt19937`
emits but not how `std::shuffle` consumes it, and `std::uniform_int_distribution`
is unspecified the same way — so libstdc++ and MSVC's library produce
different permutations from identical state. Every seeded check here would
then play a different game on Windows. That is not theory: Canasta's AI
strength ladder passed on this machine and failed on the Windows runner with
no difference in the engine at all, which reads as a broken AI rather than a
broken shuffle. `card.cpp` now draws its index from `rng()` directly.
**`minefield.cpp` and `sudokugrid.cpp` still call `std::shuffle`** — nothing
asserts their sequence across platforms today, but a new test that seeds
either one needs the same treatment first.

**`QSettings` has no file on Windows.** It writes to the registry there, so
`QFile::exists(QSettings().fileName())` is false however well saving works —
a persistence check has to construct a fresh `QSettings` and read the value
back instead. That assertion cost a red Windows run for a feature that was
working.

**`M_PI` does not exist on MSVC, so this codebase uses `std::numbers::pi`.**
MSVC's `<cmath>` defines `M_PI` only if `_USE_MATH_DEFINES` was defined
before it was included, so the seven uses that lived here compiled on GCC
and would have failed the Windows build. `<numbers>` is C++20, which the
project already targets, and it needs no build flag to be load-bearing for a
maths constant. A new painter reaching for `M_PI` is caught only by the
Windows CI job, minutes later.

**`--version` is answered from `argv` before `QApplication` exists, and it
must stay that way.** `qt_add_executable` sets `WIN32_EXECUTABLE`, so the
Windows binary is a GUI-subsystem process: `QCommandLineParser::showVersion()`
puts the version in a *message box* there instead of on stdout, and the
release workflow's smoke test would hang waiting for a dialog no runner can
close. Going through `argv` also means `--version` needs no display and no
platform plugin anywhere, which is what makes it usable as a packaging check
at all. Folding it back into the parser looks tidier and breaks the release.

**An AppImage is not a closed box — linuxdeploy deliberately leaves 53
libraries to the host**, `libGL.so.1`, `libxcb.so.1` and `libX11.so.6` among
them, because a bundled graphics stack breaks against the host driver. So
"self-contained" here means *carries its own Qt*, not *runs on an empty
filesystem*, and the release workflow's clean-room container installs that
baseline before testing. Adding anything Qt to that container would make the
test prove nothing.

**Each smoke test runs the artifact twice, and only the second run loads
Qt.** `--version` returns before `QApplication` exists, so on its own it
proves the file unpacks and reports the right version and nothing more — a
bundle with no platform plugin passed every gate that way, which is how 0.3.0
shipped carrying only `xcb`. The second run starts `--game spider` under
`QT_QPA_PLATFORM=offscreen` and **passes only if the app is still alive when
the timeout fires**, so `timeout`'s own 124 is the success status and any exit
of the app's own is the failure. **On Windows liveness alone is not enough,
and assuming it was would pass the failure the check exists to catch:** a
release build with no console does not exit when the platform plugin will not
load — Qt shows a blocking message box first (guarded by `!isDebugBuild() &&
!GetConsoleWindow()`) and reaches `qFatal` only when someone dismisses it,
which on a runner nobody does. So that run adds `-NoNewWindow` and asserts the
process owns no top-level window; under offscreen a healthy app opens none, so
a window is the dialog. Both artifacts bundle the offscreen plugin
for it: Linux via `EXTRA_PLATFORM_PLUGINS=libqoffscreen.so` (not
`EXTRA_QT_PLUGINS`, a deprecated alias for `EXTRA_QT_MODULES` that matches
modules and would silently match nothing), Windows by copying
`qoffscreen.dll` from the Qt install beside `windeployqt`. **The running
check cannot see a missing desktop plugin**, and that is not a detail: it
asks for offscreen, so it loads `libqoffscreen.so` / `qoffscreen.dll` and
never touches `libqxcb.so` / `qwindows.dll` — a bundle missing the plugin
every player needs starts fine under it. The staged-tree assertions name both
platform plugins for exactly that reason, and they are the *only* guard for
the desktop one. They use different paths per platform: `linuxdeploy` keeps
Qt's `plugins/<group>/` layout, while `windeployqt` mirrors each group into
the deployment root. A missing *multimedia* plugin is likewise caught only
there — the app runs in silence rather than failing.

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
source with `CMAKE_AUTORCC ON`. **`tests/uitest.cpp` asserting every effect is
present and non-trivial in size is the check.** `assets/sounds/` is 17 files,
and the compiled resource object matches it. **No byte figure is kept here.**
The binary's own total grows with every game added, so it never said anything
on its own — and a stale one is worse than none, because the sentence reads as
a comparison somebody can make: two runs of this gate spent a finding on
exactly that. Measure the directory when you need the number.

**QPainter's `drawRect` fills with the current brush as well as outlining it.**
A brush set for one thing leaks into everything drawn after it: the flag's red
brush turned every dug Minesweeper square red, and only from the first flag
onward, because painting runs row by row. Set the brush — or `Qt::NoBrush` —
immediately before a shape call rather than trusting what came before.

**Sounds are generated, never sampled.** `tools/make_sounds.py` synthesises all
17 effects from a fixed seed, so re-running it is byte-identical. That is a
licensing decision rather than a stylistic one — see `ROADMAP.md` § Standing
rules before adding any asset.

## Review history

This file has been through `review-contract` as a standard. The loop log is
kept in `docs/claude-md-review-2026-08-20.md` rather than here, because a table
appended to forever is a cost every session pays and almost none reads.

**When the gate is owed, owner's call 2026-09-04: when an edit changes what
the prose SAID, not when it only adds to it.** Applied to the six edits
standing at that date, three qualified and three did not — and the two that
had been filed as owing a review turned out to be pure additions. Record the
answer either way in the commit body, since a considered no and never having
asked look identical otherwise.

**The 2026-09-04 run ended at its cap with a violent verdict** — four of the
final loop's five findings landed on text that run had itself written. Its
routing therefore says do not re-run this gate on the document as it stands;
GHUB-0180 carries the split it points at instead. That bar lapses with the
text it was measured against, so an edit changing direction re-arms the gate
normally. GHUB-0180 made that split on 2026-09-10: the per-game notes and
the design reasoning moved to `docs/design.md`, which is gated on its own.

## Testing notes

Wayland blocks synthetic clicks and no injection tool is installed, so GUI
testing is: construct widgets offscreen, `render()` them into a QPixmap to
force `paintEvent` through, and click the hub's tiles via `QPushButton::click`.
Anything needing real pointer input has to be verified by eye instead.

The self-test is where game logic gets proven — it plays 200 random Reversi
games, 20 full AI Hearts games, 18 full AI Canasta games at three strengths,
and flies pinballs. Prefer adding a check there over a UI test.

**`tests/saves/` is a committed corpus of real saves, one per saving game, and
the UI test restores every one of them.** A save-format change that refuses the
old format is not a crash — the hub keeps the fresh deal it just made, the app
runs, nothing looks broken, and the player's half-finished game is gone without
being told. `docs/standards/versioning-overrides.md` § 1 names that a breaking
change; this corpus is what enforces it. **When it reddens, the choice is write
a migration or take the break deliberately — never regenerate the corpus to
make the check green**, which deletes the only evidence the format moved.

**The deliberate break has an end state, or it is just a red suite forever.**
Replace that game's corpus file in the SAME commit that records the break, so
the diff shows the old save going and the reason for it arriving together.
`--write-saves` is how you do that; what the rule forbids is reaching for it
with nothing recorded.

**It is not a reason to cut `1.0.0`.** § 2 of that same document keeps MAJOR at
0 until the last of its six items ships, so a reddened corpus inside `0.x` is
not a MAJOR bump waiting to happen — what a breaking change costs at this
version is the versioning standard's answer, not this file's. `--write-saves` rewrites it and is
for adding a game, not for making the check pass.

The corpus is written by `startedSave()`, which is also what the mutation fuzz
seeds from: most games answer "nothing worth keeping" for a board nobody has
moved in, so it pokes at the surface until something is worth saving, and
freezes the clock first because Minesweeper and Sudoku save a running elapsed
figure. Snake and Pinball offer no save and are absent by design.

**A test that builds a position with a WILD or a red three as the up-card gets
one fewer card in the stock than it looks.** `Engine::dealFrom` covers such a
turn-up with another card off the stock, so the pack starts at two and every
subsequent draw shifts down by one — a card placed at `kBelowCount - 5` for a
seat's second draw arrives at `kBelowCount - 6` instead. The symptom is a check
whose hand simply does not contain the card it was built around, which reads as
a broken AI. Cost a debug cycle on `canastaFirstCanastaIsInsurance`; both that
check and `canastaAiOpensOnAJokerToKeepThePair` say which case they are in
where their stock is built.

**`canastaLevelsDiffer()` prints its four rungs on every run, and what the
ladder reads today is the baseline for judging tomorrow's change** — the figures
are re-read by running the suite, never assumed from a handoff. As of
2026-09-02, after GHUB-0129: medium v easy 22/24 +2538, hard v easy 22/24
+3547, hard v medium 69/120 +424, expert v hard 121/240 -1.

**Two rungs moved that day — hard v medium and expert v hard — and both causes
are known rather than suspected.**
GHUB-0148 made validateTake subtract the pile's red threes before the
no-legal-move guard, so a take that would strand the seat on one card is now
refused -- and the AI made some of those takes. That alone took expert v hard
from 121/240 +14 to 116/240 -183. GHUB-0129 then widened `worthHolding` to Hard
and graded its pile floor, which put hard v medium up from 66/120 +236 and
expert v hard back to level. **Two different things get called noise here, and
they pull opposite ways.** Run to run there is none: the shuffle is seeded and
hand-written, so two consecutive runs give the same figures to the digit and a
rung that moved really did move. Across DEALS there is plenty — 24 or 120 games
is a small sample — which is what GHUB-0110 settled and why a moved rung is not
by itself evidence that a level got stronger. Do not read the GHUB-0148 dip as a
regression introduced by an AI change; it was the price of a correctness fix.
**A rung that does NOT move is the useful reading when a change is gated to
some levels** — GHUB-0124 touched Expert alone, so the first three not moving is
what said so, and GHUB-0129 touched Hard and Expert, so medium v easy standing
unchanged to the digit is what says that. Two things
that reading does NOT mean. The absolute numbers are not a target — GHUB-0110
settled that the ladder cannot separate a small change from noise, so a check
against a hand-built position is the judge and this is context. And **the ladder
plays default Rules**: `canastaMatch` builds a bare `ca::Engine`, so
`canastaNeededToScore`, `deadHandIfNobodyGoesOut` and every other House flag are
OFF there. Anything gated on a house rule is invisible to it, and a flat reading
is then evidence of nothing at all.

Two patterns from Canasta worth reusing. `Engine::newGameFromStock()` deals
from a stock the caller supplies, so a check can build an exact position
instead of hunting for a seed that produces one. And the scoring table and the
opening bands are free functions (`handScoreFor`, `openRequirementFor`) rather
than private methods, so they can be checked directly on a hand-built position
rather than one played into existence.
