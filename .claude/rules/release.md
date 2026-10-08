---
paths:
  - "CHANGELOG.md"
  - "CMakeLists.txt"
  - "README.md"
  - "SECURITY.md"
  - ".claude/bump.json"
  - ".github/**"
  - "packaging/**"
  - "scripts/install-qt.py"
---

## Releasing

Two workflows in `.github/workflows/`, contract in
`docs/specs/GHUB-0025-downloadable-builds.md`. `ci.yml` builds and runs
`ctest` -- every registered case, not just the two binaries -- on `ubuntu-24.04`
and `windows-2022` for every push **to `master`** and every pull request —
**unless it touches documentation alone**: `paths-ignore` skips the same set the
pre-push hook treats as docs, and no test reads a file on it. That
includes the Python checks, which is how a Linux-only `grep` pipeline in one of
them reddened the Windows leg six times (GHUB-0171). A push to any other branch runs nothing, so
the Windows leg — the only place MSVC is exercised — does not run on branch
work until a pull request opens. `release.yml`
turns a tag into a Linux AppImage and a Windows zip on the releases page.

**`ci.yml` runs more than the two test binaries.** Four job definitions —
`lint`, `build`, `sanitizers` and `tidy` — but `build` is a matrix, so GitHub
reports **five** checks: `Workflow lint`, the Linux build, the Windows build,
`Saved-game fuzz (ASan/UBSan)` and `clang-tidy`. All on the same triggers.
`scripts/local-ci.sh` runs the lint job first, skips the last two unless given
`--with-sanitizers` and `--with-tidy`, and prints that it skipped them.

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

**Before step 1, check the Qt the downloads bundle** (GHUB-0055). Look up
`QT_VERSION` from either workflow on
[Qt's list of known vulnerabilities](https://wiki.qt.io/List_of_known_vulnerabilities_in_Qt_products).
Write the date and what you found into `SECURITY.md` § Bundled Qt, replacing
the previous check. A pinned Qt named in an advisory gets a roadmap item for
the bump. The CI actions need no step here: `.github/dependabot.yml` has
Dependabot propose their updates monthly.

**Dependabot cannot see the Qt installer's pins** (GHUB-0196): `AQT_SRC` in
both workflows, and the `CONSTRAINTS` list in `scripts/install-qt.py`. Check
[aqtinstall's releases](https://github.com/miurahr/aqtinstall/releases) at
the same time. `AQT_SRC` points at an unreleased commit only because 3.3.0
cannot find Qt 6.11 for Windows; a release that can should replace it.

**Then run the release workflow by hand before tagging**: Actions tab →
Release → Run workflow. It builds, tests and smoke-tests both downloads and
publishes nothing. A packaging fault is found there rather than by the tag.

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
clean. CI's `lint` job runs all three on every push and pull request
(GHUB-0051), and `local-ci.sh` runs that same job, so they are also the check
before pushing a workflow edit. Their settings are `.yamllint`; the versions
are pinned in the job.

## Packaging traps

Moved from `CLAUDE.md` § Traps worth knowing.

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
