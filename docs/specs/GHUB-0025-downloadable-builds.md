# GHUB-0025 — Publish Games Hub as a downloadable file for Linux and Windows

**Status:** spec draft (2026-08-12).
**Kind:** release.
**Source:** ROADMAP GHUB-0025 (user request, 2026-08-12).

Layman: pushing a version tag puts two files on the GitHub releases page —
one for Linux, one for Windows — that someone can download and run without
installing anything else.

## 1. Goal

Pushing a tag `v<X.Y.Z>` publishes a GitHub release carrying exactly two
files: a single-file AppImage for Linux and a portable zip for Windows,
each self-contained enough to run on a machine with no Qt on it. Separately,
every push to `master` and every pull request builds and runs the whole test
suite on both operating systems, so a change that breaks Windows is caught by
the machine rather than at release time.

## 2. Problem

Fourteen finished games exist only as `./build/gameshub` on one openSUSE
machine, and nothing about the project is set up to change that.

1. **There is no continuous integration.** `.github/` does not exist
   (`ls -a` at the repo root lists no `.github`). Every check this project
   has — `gameshub_selftest`, `gameshub_uitest`, `ctest` — runs only when a
   human remembers to run it, on one machine, on one compiler.
2. **The project has never been built for Windows, or by any compiler other
   than the local GCC.** One concrete blocker is already known: `M_PI` is
   used 7 times, on one line in each of five files (`rg -o 'M_PI' src/ |
   wc -l` → `7`; `rg -c 'M_PI' src/` → `1` for each of
   `src/pinball/pinballview.cpp`, `src/pinball/pinballtable.cpp`,
   `src/canasta/canastaview.cpp`, `src/hubwindow.cpp` and
   `src/minesweeper/minesweeperview.cpp`, two of those lines using it
   twice). MSVC's `<cmath>` does not define
   `M_PI` unless `_USE_MATH_DEFINES` is defined before it is included, so
   those five translation units do not compile there. Nothing else obviously
   non-portable is in the tree: `rg -n '#include <(unistd|sys/|fcntl|dirent|pthread)'`
   over `src/` returns no matches.
3. **There has never been a release.** `git tag` returns nothing, and
   `CHANGELOG.md` has an `[Unreleased]` section with three entries and no
   version block below it. `CMakeLists.txt` declares
   `project(gameshub VERSION 0.2.0 LANGUAGES CXX)`, a number no artifact
   anywhere has ever carried.
4. **What install rules exist are Linux desktop integration, not
   distribution.** `CMakeLists.txt` installs `packaging/gameshub.desktop.in`
   (via `configure_file`) into `${CMAKE_INSTALL_DATAROOTDIR}/applications`
   and `packaging/gameshub.svg` into the hicolor icon theme. That puts the
   hub in one machine's application menu; it produces nothing anyone can
   download.

## 3. Scope decisions (agreed with the user)

- **The Windows artifact is a portable zip, not a single `.exe`.** Stated to
  the user 2026-08-12 with the trade-off named: a genuinely single Windows
  executable requires a statically linked Qt, which is a multi-hour build per
  CI run and carries an LGPL obligation to supply relinkable objects. The
  user's ask was "a standalone file" — a zip is one file to download that
  needs no installer and no Qt on the target. Reversible: §8 records what
  changing it would cost.
- **Linux gets a true single file.** AppImage, because it is the only Linux
  format that is one executable file with no store, no package manager and
  no root.
- **macOS is not attempted.** The user named Windows and Linux.

## 4. Design

### 4.1 One version number

`CMakeLists.txt`'s `project(gameshub VERSION ...)` stays the single source.
It already reaches the binary — `target_compile_definitions(gameshub PRIVATE
GAMESHUB_VERSION="${PROJECT_VERSION}")`, read by `main()` in `src/main.cpp`
as `app.setApplicationVersion(...)`, and surfaced by the
`parser.addVersionOption()` that is already there.

A release tag is `v<PROJECT_VERSION>`. The release workflow refuses to
publish when the two disagree:

```bash
tag="${GITHUB_REF_NAME#v}"
cmake_version=$(sed -n 's/^project(gameshub VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
[ "$tag" = "$cmake_version" ] || { echo "tag $tag != CMakeLists $cmake_version"; exit 1; }
```

### 4.2 `.github/workflows/ci.yml` — build and test both platforms

Triggers on push to `master` and on pull requests. One matrix job:

| `os` | Qt arch | Notes |
|------|---------|-------|
| `ubuntu-24.04` | `linux_gcc_64` | not `ubuntu-22.04`: deprecation begins 2026-09-17 |
| `windows-2022` | `win64_msvc2022_64` | MSVC, the default Qt Windows build |

Steps: `actions/checkout@v4`; `jurplel/install-qt-action@v4` with
`version: 6.8.3` and `modules: qtmultimedia`; `cmake -S . -B build -G Ninja
-DCMAKE_BUILD_TYPE=Release`; `cmake --build build`; `ctest --test-dir build
--output-on-failure`. Qt 6.8.3 satisfies the existing
`find_package(Qt6 6.5 REQUIRED ...)` floor.

`ctest` already sets `QT_QPA_PLATFORM=offscreen` for the UI test
(`set_tests_properties(uitest PROPERTIES ENVIRONMENT ...)`), so both test
binaries run headless on both runners with no workflow-side environment
fiddling.
The Linux runner additionally needs `libxkbcommon-x11-0` and friends for Qt
to load its platform plugins at all; installed with `apt-get install -y
libgl1-mesa-dev libxkbcommon-x11-0 libxcb-cursor0`.

### 4.3 `.github/workflows/release.yml` — a tag becomes two files

Triggers on `push: tags: ['v*']`. Three jobs.

**`linux`** — same build as CI, then:

```bash
cmake --install build --prefix AppDir/usr
linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt \
  --desktop-file AppDir/usr/share/applications/gameshub.desktop \
  --icon-file AppDir/usr/share/icons/hicolor/scalable/apps/gameshub.svg \
  --output appimage
```

`cmake --install` already lays down exactly the three things an AppDir
needs — the binary, the `.desktop` file and the icon — because §2.4's
desktop-integration rules put them in the standard prefix-relative
locations. The install prefix must be set at configure time
(`-DCMAKE_INSTALL_PREFIX=/usr`), not passed to `--install --prefix`,
because `configure_file` bakes the absolute `Exec=` path into the
`.desktop` file; `--prefix` on the install line would relocate the binary
while leaving `Exec=` pointing at the configure-time path. Output is
renamed to `GamesHub-<version>-x86_64.AppImage`.

**`windows`** — same build, then:

```powershell
cmake --install build --prefix stage
windeployqt --release --no-translations --no-system-d3d-compiler stage\bin\gameshub.exe
Compress-Archive -Path stage\bin\* -DestinationPath GamesHub-<version>-windows-x64.zip
```

**`publish`** — needs both, downloads their artifacts and calls
`softprops/action-gh-release@v2` with the two files and the `[Unreleased]`
changelog body.

### 4.4 The MSVC maths constant

Replace all 7 `M_PI` uses with `std::numbers::pi` from `<numbers>`, which
C++20 provides on every compiler and which this project already targets
(`set(CMAKE_CXX_STANDARD 20)`). The alternative — defining
`_USE_MATH_DEFINES` in `CMakeLists.txt` — is one line instead of an edit in
each of five files, but it leaves a build flag load-bearing for a maths
constant, which is invisible at the call site.

### 4.5 What "self-contained" is checked to mean

Both release jobs smoke-test their own artifact before it is published:

- Linux, in a container with no Qt:
  `docker run --rm -v "$PWD:/w" ubuntu:24.04 /w/GamesHub-*.AppImage
  --appimage-extract-and-run --version`, with `QT_QPA_PLATFORM=offscreen`.
- Windows, with the Qt install removed from `PATH`:
  `$env:PATH = 'C:\Windows\System32'; .\stage\bin\gameshub.exe --version`.

Both must print `Games <version>` and exit 0.

## 5. Invariants

- **INV-1** — The version the binary reports is the version CMake was
  configured with; there is no second place to edit.
  *Test:* `QT_QPA_PLATFORM=offscreen ./build/gameshub --version` →
  `Games 0.2.0`, matching `project(gameshub VERSION 0.2.0` in
  `CMakeLists.txt`.
  *Breaks when:* a second version literal is introduced — a hardcoded
  string in `main.cpp`, or a version written into a workflow file.

- **INV-2** — A tag never publishes artifacts whose version disagrees with
  it.
  *Test:* the `Check tag matches CMake version` step in
  `.github/workflows/release.yml`; pushing `v9.9.9` against a 0.3.0
  `CMakeLists.txt` fails the job before anything is built.
  *Breaks when:* the tag is cut before `project(... VERSION ...)` is bumped —
  the ordinary release mistake.

- **INV-3** — Both test binaries run on both operating systems for every
  change, not just Linux.
  *Test:* `.github/workflows/ci.yml` runs `ctest --test-dir build
  --output-on-failure` under a matrix containing both `ubuntu-24.04` and
  `windows-2022`; the run appears twice on every commit's check list.
  *Breaks when:* a matrix entry is dropped, or a step is given an
  `if: runner.os == 'Linux'` guard that skips the tests rather than a
  platform-specific setup detail.

- **INV-4** — No translation unit depends on a maths constant MSVC does not
  define.
  *Test:* `rg -o 'M_PI|M_E|M_SQRT2' src/ | wc -l` → `0` (it is `7` before
  the §4.4 change).
  *Breaks when:* a new painter is written with `M_PI`, which compiles
  silently on this machine's GCC and fails only in the Windows CI job.

- **INV-5** — The published Linux file runs on a machine with no Qt
  installed.
  *Test:* the `Smoke-test the AppImage` step in
  `.github/workflows/release.yml` runs it inside a stock `ubuntu:24.04`
  container and requires `Games <version>` on stdout.
  *Breaks when:* `linuxdeploy --plugin qt` stops finding a library — most
  likely after a Qt version bump changes where a dependency lives.

- **INV-6** — The published Windows file runs with no Qt on `PATH`.
  *Test:* the `Smoke-test the portable build` step in
  `.github/workflows/release.yml` scrubs `PATH` to `C:\Windows\System32`
  before running `gameshub.exe --version`.
  *Breaks when:* `windeployqt` is run against the wrong binary, or a new Qt
  module is linked that it does not know to copy.

- **INV-7** — Sound still works in the packaged builds, which needs the Qt
  Multimedia backend plugin and not just the `Qt6Multimedia` library.
  *Test:* both release jobs assert the staged tree contains at least one
  file under its `plugins/multimedia` directory before packaging.
  *Breaks when:* a deployment tool bundles linked libraries only — the app
  then starts, paints and plays in silence, which no `--version` check can
  see.

## 6. Failure modes

| Assumption | When it breaks | What happens |
|---|---|---|
| MSVC's only complaint is `M_PI` | Fourteen games have never seen this compiler | The Windows CI job goes red on first push. Fix and re-push; nothing is published, because publishing is tag-triggered and CI is not. |
| The Qt mirrors serve 6.8.3 | Mirror outage, or Qt withdraws the version | `install-qt-action` fails; the job is re-run or the version pinned forward. No artifact is published from a partial build. |
| `linuxdeploy --plugin qt` finds the multimedia backend | Qt moves it, or the plugin lags a Qt release | INV-7 fails the job before publishing. |
| The AppImage's glibc floor is low enough | Built on `ubuntu-24.04` (glibc 2.39) | It will not start on Debian 12 or Ubuntu 22.04. Not fixed — documented in the README, see §10. |
| The tag matches the CMake version | Human cuts the tag first | INV-2 fails the job. |

## 7. Tests

Nothing in `tests/selftest.cpp` or `tests/uitest.cpp` changes, and nothing is
added to them: this work has no game logic in it, and both binaries already
constitute the check that the CI matrix runs. What is new is *where* they
run — a second operating system.

INV-1 and INV-4 are checkable locally by the commands in their clauses.
INV-2, INV-3, INV-5, INV-6 and INV-7 are workflow steps, and are verified by
a real run: the first push exercises INV-3 and INV-4, and the first tag
exercises the rest. Each must be seen to fail against pre-fix code — INV-4's
grep returns 7 before the `std::numbers::pi` change and 0 after, and the
Windows compile is red before it and green after.

## 8. Alternatives considered (and rejected)

- **A statically linked Qt, giving one Windows `.exe`.** Rejected: building
  Qt from source is hours per CI run, and static LGPL linking obliges the
  project to publish relinkable object files. Revisit only if the zip proves
  to be a real barrier for the user.
- **An NSIS or MSI installer.** Rejected: the ask was a standalone file, and
  an installer is the opposite of one.
- **Flatpak or Snap for Linux.** Rejected: neither is a single downloadable
  file, and both want a store account.
- **The runner's `apt` Qt instead of `install-qt-action`.** Rejected:
  `ubuntu-24.04` packages Qt 6.4, below this project's
  `find_package(Qt6 6.5 REQUIRED ...)` floor.
- **`ubuntu-22.04` runners, for a lower glibc floor and wider AppImage
  reach.** Rejected: GitHub begins deprecating that image on 2026-09-17,
  so the release pipeline would need rebuilding within weeks.
- **`_USE_MATH_DEFINES` in `CMakeLists.txt` instead of
  `std::numbers::pi`.** Rejected: see §4.4 — one line, but it makes a build
  flag load-bearing for a maths constant.
- **Building the release artifacts on this machine by hand.** Rejected: no
  Windows toolchain here, and a release nobody but this machine can cut is
  the problem being solved.

## 9. Out of scope

- macOS builds — no roadmap id; the user named two platforms.
- Code signing and notarisation. Unsigned artifacts will draw a SmartScreen
  warning on Windows; documented in the README rather than fixed.
- Auto-update inside the app.
- Any change to game behaviour. GHUB-0007 and GHUB-0009 stay queued.

## 10. What checks this

| Rule | What catches a breach |
|------|----------------------|
| INV-1 | The INV-5 and INV-6 smoke tests, which require the artifact to print `Games <tag version>`; combined with INV-2 that chains binary → CMake → tag |
| INV-2 | `.github/workflows/release.yml`, `Check tag matches CMake version` |
| INV-3 | `.github/workflows/ci.yml` matrix; a missing platform is visible on the commit's check list |
| INV-4 | The `windows-2022` CI job's compile step; `rg 'M_PI' src/` locally |
| INV-5 | `.github/workflows/release.yml`, `Smoke-test the AppImage` |
| INV-6 | `.github/workflows/release.yml`, `Smoke-test the portable build` |
| INV-7 | `.github/workflows/release.yml`, the `plugins/multimedia` assertion in both packaging jobs |
| The AppImage runs on distributions older than the build runner | **nothing** — glibc 2.39 is a hard floor and no check tests an older system; stated as a requirement in the README instead |
| The packaged games actually play | **nothing** — the smoke tests prove the binary starts and reports its version, not that fourteen games work. The test suite covers that on the same commit, but not inside the artifact |

## 11. Cross-doc impact

- `README.md` — a download section naming the two files, the glibc floor,
  the SmartScreen warning; the build-from-source instructions stay.
- `CHANGELOG.md` — `[Unreleased]` closes as `0.3.0`.
- `CLAUDE.md` — the release procedure and the MSVC trap.
- `ROADMAP.md` — GHUB-0025 flips to shipped.
- `SECURITY.md` — new; the project starts accepting downloads from
  strangers, which is when a report route needs to exist
  (`~/.claude/standards/documentation.md` §5.3).

## 12. Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
