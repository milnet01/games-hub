# GHUB-0025 — Publish Games Hub as a downloadable file for Linux and Windows

**Status:** accepted (2026-08-12).
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

A release tag is `v<PROJECT_VERSION>`. The first release under this spec
bumps `project(gameshub VERSION 0.2.0` to `0.3.0` — that bump is part of this
work, not a later step — and is tagged `v0.3.0`.

The `verify` job in §4.3 refuses the tag when the two disagree. It is its own
job, which both build jobs declare in `needs:`, so INV-2's "before anything is
built" holds; putting it in `publish` would run it after both builds, and
duplicating it in each build job would make it two rules:

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

Steps: `actions/checkout`; `jurplel/install-qt-action` with
`version: 6.8.3` and `modules: qtmultimedia`; **on the Windows leg only,
`ilammy/msvc-dev-cmd`**; `cmake -S . -B build -G Ninja
-DCMAKE_BUILD_TYPE=Release`; `cmake --build build`; `ctest --test-dir build
--output-on-failure`. Qt 6.8.3 satisfies the existing
`find_package(Qt6 6.5 REQUIRED ...)` floor.

The `msvc-dev-cmd` step is not optional and is the whole reason Ninja can be
used on both legs: outside a developer command prompt CMake cannot find
`cl.exe`, and the Ninja generator has no equivalent of the Visual Studio
generator's own toolchain discovery, so configure fails with "No
CMAKE_CXX_COMPILER could be found". The alternative — `-G "Visual Studio 17
2022"` on Windows — needs no extra action but is multi-config, which moves
the executable to `build\Release\gameshub.exe` and forces `--config Release`
on every build and test line. One action buys one build recipe for both
platforms.

`ctest` already sets `QT_QPA_PLATFORM=offscreen` for the UI test
(`set_tests_properties(uitest PROPERTIES ENVIRONMENT ...)`), so both test
binaries run headless on both runners with no workflow-side environment
fiddling.
The Linux runner additionally needs `libgl1-mesa-dev libxkbcommon-x11-0
libxcb-cursor0` installed with `apt-get`, because the Qt that
`install-qt-action` unpacks there links them and finds nothing bundled
beside it.

**The AppImage does not carry those libraries either, and INV-5's container
must install them.** linuxdeploy honours the AppImage community
excludelist, which names the whole libglvnd set (`libGL.so.1`,
`libOpenGL.so.0`, `libGLX.so.0`, `libGLdispatch.so.0`), `libxcb.so.1` and
`libX11.so.6` among 53 entries deliberately left to the host, on the grounds that a
bundled graphics stack breaks against the host driver. So "self-contained"
means *carries its own Qt*, not *runs on an empty filesystem*: any machine
that can run a desktop application already has them, and a stock
`ubuntu:24.04` container does not. §4.6's container installs that baseline
and nothing Qt, which is what keeps the test honest.

Both runners also need Ninja. It is not guaranteed on the Ubuntu image, so
the Linux leg adds `ninja-build` to its `apt-get` line; on Windows the
`msvc-dev-cmd` step puts Visual Studio's own `ninja.exe` on `PATH`.

### 4.3 `.github/workflows/release.yml` — a tag becomes two files

Triggers on `push: tags: ['v*']`. The workflow declares
`permissions: contents: read` at the top and raises it to
`contents: write` on the `publish` job alone, which is what `gh release
create` needs; a repository whose default `GITHUB_TOKEN` is read-only fails
at the publish step otherwise, and nothing else in the workflow needs write.

**Every action is pinned to a commit SHA**, with its version in a trailing
comment, and `actions/checkout` is given `persist-credentials: false`. Both
are `zizmor` requirements and both earn it here: these workflows publish
binaries that strangers download, so a moved tag on a third-party action
would run arbitrary code against them, and a token left in `.git/config`
travels into any artifact built from that workspace.

Four jobs: `verify`, then `linux` and `windows` in parallel (both
`needs: verify`), then `publish`.

**`verify`** — checks out and runs two assertions, both before any build:
§4.1's tag-versus-`CMakeLists.txt` comparison, and that `CHANGELOG.md`
contains a `## [<version>]` heading for that version. The second exists
because §11 requires the release commit to close `[Unreleased]` into a
numbered block; if that was forgotten, `publish` would otherwise ship a
release with an empty body, and by then the tag is public. Failing here
costs a deleted tag and nothing else.

**`linux`** — same build as CI, configured with
`-DCMAKE_INSTALL_PREFIX=/usr`, then:

```bash
# linuxdeploy and its Qt plugin are two separate downloads; --plugin qt
# finds the second by name on PATH. Neither ships with the runner.
for t in linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage; do
  curl -fsSLO "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/$t"
  chmod +x "$t"
done
export PATH="$PWD:$PATH"
export APPIMAGE_EXTRACT_AND_RUN=1   # the runner has no FUSE 2
export EXTRA_PLATFORM_PLUGINS=libqoffscreen.so   # for §4.6's running check

DESTDIR="$PWD/AppDir" cmake --install build

# The installed desktop file carries Exec=/usr/bin/gameshub, an absolute path
# baked in by configure_file. Inside an AppImage that would name the HOST's
# binary, so the AppRun linuxdeploy generates must see a bare name instead.
sed -i 's|^Exec=.*|Exec=gameshub|' AppDir/usr/share/applications/gameshub.desktop

./linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt \
  --executable AppDir/usr/bin/gameshub \
  --desktop-file AppDir/usr/share/applications/gameshub.desktop \
  --icon-file AppDir/usr/share/icons/hicolor/scalable/apps/gameshub.svg
# INV-7 and INV-8 assert against AppDir here, before it is packaged
./linuxdeploy-x86_64.AppImage --appdir AppDir --output appimage
```

`APPIMAGE_EXTRACT_AND_RUN=1` is not optional: GitHub's `ubuntu-24.04` image
carries no FUSE 2, so every AppImage — linuxdeploy, its Qt plugin and the
`appimagetool` linuxdeploy spawns — would otherwise fail to mount itself.
Exporting it once covers all three.

`cmake --install` already lays down exactly the three things an AppDir
needs — the binary, the `.desktop` file and the icon — because §2.4's
desktop-integration rules put them in the standard prefix-relative
locations. **`DESTDIR`, not `--prefix`:** the prefix must be `/usr` at
configure time, because `configure_file` bakes the absolute `Exec=` path
into the `.desktop` file, and `--prefix` on the install line would relocate
the binary while leaving `Exec=` pointing at the configure-time path.
`DESTDIR` stages the whole `/usr` tree under `AppDir/` without touching the
paths compiled into it, which is exactly what an AppDir is. Verified
locally 2026-08-12: `DESTDIR=… cmake --install` produced `usr/bin/gameshub`,
`usr/share/applications/gameshub.desktop` and the hicolor icon, with
`Exec=/usr/bin/gameshub`.

**linuxdeploy is called twice on purpose.** The first call populates the
AppDir; the second packages it. A single call with `--output appimage` does
both at once and leaves no moment at which a deployed-but-unpackaged tree
exists, which is the moment INV-7 and INV-8 assert against. Output is
renamed to `GamesHub-<version>-x86_64.AppImage`.

**`windows`** — same build, then, deliberately *not* via `cmake --install`,
because that puts the executable under `bin/` and a portable zip wants it at
the top level:

```powershell
mkdir dist
copy build\gameshub.exe dist\
copy LICENSE,packaging\THIRD-PARTY.md dist\
xcopy /e /i packaging\licenses dist\licenses
windeployqt --release --no-translations --no-system-d3d-compiler `
            --compiler-runtime dist\gameshub.exe
# INV-7 and INV-8 assert against dist\ here, before it is packaged
Compress-Archive -Path dist\* -DestinationPath GamesHub-<version>-windows-x64.zip
```

`--compiler-runtime` is deliberate: without it the zip relies on the MSVC
redistributable already being installed, which INV-6 cannot detect because
the runner has it system-wide. The user would meet a missing-DLL dialog that
no gate in this pipeline can see.

Both build jobs end with `actions/upload-artifact`, naming the file they
produced; that is what `publish` downloads.

**`publish`** — needs both, downloads their artifacts with
`actions/download-artifact` and calls the runner's own `gh release create`
with the two files and a release body extracted from the `## [<version>]`
block of `CHANGELOG.md` matching the tag — **not** `[Unreleased]`. `gh`
rather than a third-party publish action: it is already on the runner, so
it is one less dependency holding write access to this repository's
releases. Per §11 the release commit closes
`[Unreleased]` into a numbered block before the tag is cut, so at tag time
`[Unreleased]` is empty and reading it would publish blank release notes.

### 4.4 The MSVC maths constant

Replace all 7 `M_PI` uses with `std::numbers::pi` from `<numbers>`, which
C++20 provides on every compiler and which this project already targets
(`set(CMAKE_CXX_STANDARD 20)`). The alternative — defining
`_USE_MATH_DEFINES` in `CMakeLists.txt` — is one line instead of an edit in
each of five files, but it leaves a build flag load-bearing for a maths
constant, which is invisible at the call site.

### 4.5 Bundling Qt is a licence change, not just a packaging one

Until now the project has distributed source only, and `README.md` says so:
"The app links Qt 6 dynamically, which Qt offers under LGPL-3.0 (or LGPL-2.1
with the Qt Company exception). Qt itself is neither bundled nor modified
here." Both artifacts in §4.3 bundle Qt, so that sentence stops being true
and LGPL-3.0 §4's conditions start applying to every download.

Dynamic linking already satisfies the relinking condition; what is missing is
the paperwork. Four files travel beside the binary in both artifacts, of
which only one is new:

- `LICENSE` — **already in the tree**, the project's own MIT text. It has
  never been installed anywhere; it is part of the four because a download
  carries no repository to read it from.
- `packaging/licenses/LGPL-3.0.txt` and `packaging/licenses/GPL-3.0.txt` —
  **already in the tree**, fetched verbatim from gnu.org. LGPL-3.0
  incorporates the GPL by reference and requires a copy of both. This work
  adds their install rules, not the files.
- `packaging/THIRD-PARTY.md` — **new**. Names the bundled Qt version, states
  that it is unmodified, and links to the matching source archive on
  `download.qt.io`.

`CMakeLists.txt` installs the two licence texts into
`${CMAKE_INSTALL_DOCDIR}/licenses`, and `LICENSE` and `THIRD-PARTY.md` into
`${CMAKE_INSTALL_DOCDIR}` itself. **The `licenses/` subdirectory is part of
the contract, not incidental** — INV-8 asserts that layout, and a flat
install into DOCDIR would fail it. The Windows job reproduces the same shape
in `dist\`, per §4.3.

### 4.6 What "self-contained" is checked to mean

Both release jobs smoke-test their own artifact before it is published, and
each does it **twice: once for `--version`, and once by starting the app.**
The second run is the only one that loads a Qt plugin, because §4.7 makes
`--version` return before `QApplication` is constructed. A `--version`-only
gate therefore proves the file unpacks and reports the right version, and
says nothing about whether it can open a window — which is how the 0.3.0
AppImage shipped carrying no offscreen plugin with every gate green.

The running check starts the hub straight into one game under a timeout, and
**passes only if the app is still alive when the timeout fires.** An app that
cannot load its platform plugin exits at once, so "still running" is the
signal; the timeout's own `124` is the expected status and any other status
is the failure. Twenty seconds is far longer than a start-up that works and
far shorter than a CI job cares about.

- Linux, in a container with no Qt:

  ```bash
  docker run --rm -v "$PWD:/w" ubuntu:24.04 sh -c '
    set -e
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends \
      libegl1 libgl1 libglx0 libglvnd0 libopengl0 libdrm2 \
      libx11-6 libx11-xcb1 libxcb1 libfontconfig1 libfreetype6 \
      libgpg-error0 libcom-err2 fonts-dejavu-core
    app=$(ls /w/GamesHub-*.AppImage)
    "$app" --appimage-extract-and-run --version
    rc=0
    QT_QPA_PLATFORM=offscreen timeout -s TERM 20 \
      "$app" --appimage-extract-and-run --game spider || rc=$?
    [ "$rc" -eq 124 ]'
  ```

  Host libraries, no Qt — installing anything Qt here would void what the
  test proves. **The list is derived rather than guessed, and it has to
  be:** intersect the excludelist with the sonames the binary links and 23
  libraries come back, most already in the base image, the rest covered by
  13 of these packages. Verified in a real `ubuntu:24.04` on 2026-08-12 by
  checking all 23 resolve. Building it up one CI failure at a time is the
  trap — the excluded GL stack alone is four libglvnd packages plus
  `libegl1`, so each attempt buys exactly one more library and a full
  release run.

  `fonts-dejavu-core` is the fourteenth and the one exception to that
  derivation: it is data rather than a linked library, so no soname scan
  finds it. `libfontconfig1` pulls in the configuration but recommends the
  fonts, and `--no-install-recommends` drops them, leaving a font database
  with nothing in it. Every real desktop has fonts and an empty database is
  not what this test is about.

  The offscreen plugin the second run needs is bundled by
  `EXTRA_PLATFORM_PLUGINS=libqoffscreen.so` at §4.3's deploy step.
  `linuxdeploy-plugin-qt` deploys `libqxcb.so` unconditionally and reads that
  variable for any platform plugin beyond it, as a full soname. It is **not**
  `EXTRA_QT_PLUGINS`, which is a deprecated alias for `EXTRA_QT_MODULES` and
  matches Qt *modules*, so an offscreen value there matches nothing and fails
  silently.

- Windows, from the artifact rather than the staging tree, with the Qt
  install removed from `PATH`:

  ```powershell
  Expand-Archive GamesHub-<version>-windows-x64.zip -DestinationPath unzipped
  $env:PATH = 'C:\Windows\System32'
  $p = Start-Process .\unzipped\gameshub.exe -ArgumentList '--version' `
         -Wait -PassThru -NoNewWindow -RedirectStandardOutput ver.txt
  if ($p.ExitCode -ne 0) { throw "exit $($p.ExitCode)" }
  if (-not (Select-String -Path ver.txt -Pattern '^Games ')) { throw 'no version line' }

  $env:QT_QPA_PLATFORM = 'offscreen'
  $run = Start-Process .\unzipped\gameshub.exe -ArgumentList '--game','spider' `
           -PassThru -RedirectStandardError run-err.txt
  Start-Sleep -Seconds 20
  if ($run.HasExited) { throw "exited with $($run.ExitCode)" }
  Stop-Process -Id $run.Id -Force
  ```

Both must write `Games <version>` and exit 0 from the first run, and still be
running after the second. The Windows side reads the version back from a
redirected file rather than from the pipeline, for the subsystem reason §4.7
gives; the running check needs no such handle, because what it reads is
whether the process is alive rather than anything it printed.

**Windows gets its offscreen plugin by an explicit copy, not by a variable.**
`windeployqt` chooses the plugins it copies, so §4.3's staging step takes
`platforms\qoffscreen.dll` from the Qt installation beside `windeployqt`
itself — located from `Get-Command windeployqt`, which is the same Qt the
build linked against — and copies it into `dist\platforms\`. A copy that was
already there is simply overwritten.

The Linux command runs under `sh -c` because `docker run` executes its
argument directly with no shell, so `GamesHub-*.AppImage` would otherwise be
taken as a literal filename. **The step's own shell sets `pipefail`
explicitly**: a GitHub `run:` step defaults to `bash -e` without it, so
`docker run … | tee` would report `tee`'s status, and the version line from
the first run would satisfy the `grep` for a second run that had died.

### 4.7 `--version` is answered before Qt starts

Today `src/main.cpp` constructs `QApplication` and then `HubWindow` before
`parser.process(app)` ever runs, so `--version` builds the entire hub window
to print one line. That is merely wasteful on Linux. On Windows it is fatal
to both smoke tests: `qt_add_executable` sets `WIN32_EXECUTABLE ON`
(`QtExecutableHelpers.cmake`, verified in the installed Qt 6 on 2026-08-12),
which makes the binary a GUI-subsystem application, and
`QCommandLineParser::showVersion()` then renders a **message box** instead of
writing to stdout. The Windows job would capture no output and wait for a
dialog nobody can close.

So `main()` answers `-v` / `--version` from `argv` and returns, before
`QApplication` is constructed:

```cpp
for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "--version" || arg == "-v") {
        std::printf("Games %s\n", GAMESHUB_VERSION);
        return 0;
    }
}
```

`parser.addVersionOption()` stays, so `--help` still lists the option; the
early return simply means the parser's own version path is never reached.
The string is `Games <version>` either way — the same text
`QCommandLineParser` produced from `setApplicationName("Games")` — so
nothing a user sees changes on Linux.

**The early return removes the dialog but not the subsystem, and that
changes how Windows must observe it.** `WIN32_EXECUTABLE` stays ON — turning
it off would pop a console window behind the hub every time a player
launches the game, which is a worse bug than the one being fixed. A
GUI-subsystem process has no console attached, so `printf` reaches nothing
when stdout *is* a console, and PowerShell does not block on such a process,
so `$LASTEXITCODE` is not reliable either. What does work is an explicitly
redirected handle: `Start-Process -Wait -RedirectStandardOutput` gives the
child a real file handle, which `printf` writes to normally, and `-Wait`
makes the exit code observable. §4.6's Windows check is written that way for
this reason and not as a style choice.

## 5. Invariants

- **INV-1** — The version the binary reports is the version CMake was
  configured with; there is no second place to edit.
  *Test:* `./build/gameshub --version` prints `Games ` followed by whatever
  `project(gameshub VERSION …)` currently declares — stated as an equality
  rather than a literal, because §4.1's bump moves it inside this same
  change. Observed `Games 0.2.0` before the bump (run with
  `QT_QPA_PLATFORM=offscreen`, which §4.7 makes unnecessary).
  *Breaks when:* a second version literal is introduced — a hardcoded
  string in `main.cpp`, or a version written into a workflow file.

- **INV-2** — A tag never publishes artifacts whose version disagrees with
  it.
  *Test:* the `Check tag matches CMake version` step of the `verify` job in
  `.github/workflows/release.yml`, which both build jobs declare in
  `needs:`; pushing `v9.9.9` against a 0.3.0 `CMakeLists.txt` fails `verify`
  and neither build job starts.
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

- **INV-5** — The published Linux file both reports its version and **starts
  Qt** on a machine with no Qt installed.
  *Test:* the `Smoke-test the AppImage` step in
  `.github/workflows/release.yml` runs it inside an `ubuntu:24.04`
  container carrying §4.6's host libraries and fonts and no Qt at all. It
  requires `Games <version>` on stdout from `--version`, and then runs
  `--game spider` under `QT_QPA_PLATFORM=offscreen` and `timeout 20`,
  requiring the timeout's `124` — the app still running — rather than any
  exit of its own.
  *Breaks when:* `linuxdeploy --plugin qt` stops finding a library — most
  likely after a Qt version bump changes where a dependency lives — or the
  AppImage stops carrying the offscreen plugin the running half needs.

- **INV-6** — The published Windows file does the same with no Qt on `PATH`.
  *Test:* the `Smoke-test the portable build` step in
  `.github/workflows/release.yml` expands the finished zip into `unzipped\`,
  scrubs `PATH` to `C:\Windows\System32`, and runs
  `unzipped\gameshub.exe --version` through `Start-Process -Wait -PassThru
  -RedirectStandardOutput`, asserting exit 0 and a `Games ` line in the
  redirected file. It then starts `--game spider` under
  `QT_QPA_PLATFORM=offscreen` without `-Wait`, sleeps 20 seconds, and fails
  if `HasExited`. It tests the artifact, not the staging tree it was made
  from, and it reads a file rather than stdout because §4.7's binary is
  GUI-subsystem.
  *Breaks when:* `windeployqt` is run against the wrong binary, or a new Qt
  module is linked that it does not know to copy.

- **INV-7** — The packaged builds carry the Qt *plugins* they need, not just
  the Qt libraries they link: the platform plugin, without which the app
  cannot open a window at all, and the multimedia backend, without which it
  plays in silence.
  *Test:* both release jobs assert, at the §4.3 marker before packaging,
  that their staged tree holds **both platform plugins by name** — the one
  the app runs on and the one §4.6's running check runs on, so
  `libqxcb.so` + `libqoffscreen.so` on Linux and `qwindows.dll` +
  `qoffscreen.dll` on Windows — and at least one file in the `multimedia`
  directory. **The two tools use different layouts:** `linuxdeploy` keeps
  Qt's, so it is `AppDir/usr/plugins/{platforms,multimedia}/`, while
  `windeployqt` mirrors each plugin group into the deployment root, so it is
  `dist\{platforms,multimedia}\`.
  *Breaks when:* a deployment tool bundles linked libraries only. INV-5 and
  INV-6's running halves would also catch a missing platform plugin, since
  the app cannot start without one — but they catch it as a process that
  died, where this catches it by name, before packaging. A missing
  *multimedia* plugin is caught here and nowhere else: the app runs in
  silence rather than failing, so no smoke test can see it.

- **INV-8** — Neither artifact ships Qt without the licence text LGPL-3.0 §4
  requires.
  *Test:* both release jobs assert, at the §4.3 marker before packaging,
  that all four files are present at these exact paths — on Linux
  `AppDir/usr/share/doc/gameshub/{LICENSE,THIRD-PARTY.md}` and
  `AppDir/usr/share/doc/gameshub/licenses/{LGPL-3.0.txt,GPL-3.0.txt}`; on
  Windows `dist\{LICENSE,THIRD-PARTY.md}` and
  `dist\licenses\{LGPL-3.0.txt,GPL-3.0.txt}`. The `licenses/` subdirectory
  is present on both sides; only the parent differs.
  *Breaks when:* the Windows job gains a file to copy and the copy line is
  not updated — the Linux side cannot break this way, because
  `cmake --install` carries the whole `${CMAKE_INSTALL_DOCDIR}` group.

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
INV-2, INV-3, INV-5, INV-6, INV-7 and INV-8 are workflow steps, and are
verified by a real run: the first push exercises INV-3 and INV-4, and the
first tag exercises the rest.

**Only INV-3 and INV-4 have a pre-fix state to fail against**, and both are
seen to: INV-4's grep returns 7 before the `std::numbers::pi` change and 0
after, and the Windows compile is red before it and green after. The other
six assert properties of workflows and artifacts that do not exist until
this work creates them, so there is no "before" in which to watch them go
red; their first real run is their first observation.

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
| INV-2 | `.github/workflows/release.yml`, the `verify` job both build jobs `needs:` |
| INV-3 | `.github/workflows/ci.yml` matrix; a missing platform is visible on the commit's check list |
| INV-4 | The `windows-2022` CI job's compile step; `rg 'M_PI' src/` locally |
| INV-5 | `.github/workflows/release.yml`, `Smoke-test the AppImage` |
| INV-6 | `.github/workflows/release.yml`, `Smoke-test the portable build` |
| INV-7 | `.github/workflows/release.yml`, the named-platform-plugin + `multimedia` assertion in both packaging jobs, at each one's own path; the platform half is caught again, later and less legibly, by INV-5 and INV-6's running checks |
| INV-8 | `.github/workflows/release.yml`, the licence-file assertion in both packaging jobs |
| The AppImage runs on distributions older than the build runner | **nothing** — glibc 2.39 is a hard floor and no check tests an older system; stated as a requirement in the README instead |
| The packaged games actually play | **partly** — the smoke tests start the hub inside the artifact and open one game, so a bundle that cannot run at all is caught; that fourteen games play *correctly* is covered by the test suite on the same commit, not inside the artifact |
| Sound works inside the artifact | **nothing** — INV-7 proves the multimedia plugin is present, and the smoke-test container has no audio device by design, so a plugin that is bundled but broken would reach a user as silence |

## 11. Cross-doc impact

- `README.md` — a download section naming the two files, the glibc floor,
  the SmartScreen warning; the build-from-source instructions stay. Its
  Licence section's closing sentence ("Qt itself is neither bundled nor
  modified here") becomes false with the first release and is rewritten
  per §4.5.
- `packaging/THIRD-PARTY.md` — new, per §4.5.
  `packaging/licenses/LGPL-3.0.txt` and `packaging/licenses/GPL-3.0.txt`
  already exist; this work only adds their install rules.
- `CHANGELOG.md` — `[Unreleased]` closes as `0.3.0`.
- `CLAUDE.md` — the release procedure and the MSVC trap.
- `ROADMAP.md` — GHUB-0025 flips to shipped.
- `SECURITY.md` — new; the project starts accepting downloads from
  strangers, which is when a report route needs to exist
  (`~/.claude/standards/documentation.md` §5.3).

## 12. Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 1 | 2026-08-12 | 2 | 1 | 3 | 6 | 1 | 11 verified, all fixed; 1 dismissed (INV-4's pre-change count of 7 challenged by both lanes, confirmed correct — `M_E` and `M_SQRT2` are 0 in `src/`). Both lanes independently found the same six defects. Largest: §4.3's AppDir install contradicted its own prose, fixed to `DESTDIR` staging and verified locally. |
| 2 | 2026-08-12 | 2 | 3 | 3 | 5 | 0 | 11 verified, all fixed; 0 dismissed. Four were loop-1 collateral (a "three new files" claim two of which loop 1 had created, a `licenses/` layout INV-8 and §4.5 disagreed on, INV-1's hardcoded `0.2.0` invalidated by the bump loop 1 added, and a claim that linuxdeploy bundles libGL — refuted against the 53-entry AppImage excludelist). The rest were real gaps: PowerShell `copy` cannot take three positional arguments; linuxdeploy and its Qt plugin are two separate downloads and the runner has no FUSE; and `qt_add_executable` sets `WIN32_EXECUTABLE ON`, so `--version` would have opened a message box on Windows instead of printing — §4.7 added to answer it before Qt starts. |
| 3 | 2026-08-12 | 2 | 1 | 2 | 4 | 2 | 9 verified, all fixed; 0 dismissed; nothing deferred. Loop cap reached. Both lanes found loop 2's §4.7 fix incomplete: the argv early-return removes the message box but not the GUI subsystem, so stdout still reaches nothing — answered with `Start-Process -RedirectStandardOutput`. Loop 2's fix also made both smoke tests stop loading Qt at all, so a missing platform plugin would have shipped green; INV-7 now asserts `platforms` as well as `multimedia`. Also: `--compiler-runtime`, a `verify` check that the changelog block exists, and an absolute `Exec=` that would have made the AppRun launch the host's binary. Findings concentrate in the Windows and AppImage paths — the half nobody can test on this machine — which is the argument for CI as the real verifier rather than for a fourth loop. |
| 3-impl | 2026-08-12 | none — no reviewer dispatched | — | — | — | — | Implementation fold-back (`/write-code` Step 8). Building it falsified three things three cold loops could not see. **§4.6's smoke-test container listed four packages and needed seven**: the excludelist drops the whole libglvnd set, so `libgl1` alone left the AppImage dying on `libOpenGL.so.0`; the real run is what found it, and the corrected list is confirmed against an `ubuntu:24.04` container. **§4.2's and §4.3's action pins were stale or wrong** — `actions/checkout` is v7 not v4, `upload-artifact` v7, `download-artifact` v8, and the Qt plugin is a *separate repository* from linuxdeploy, so the spec's single download loop would have 404'd. All are now SHA-pinned. **`softprops/action-gh-release` was dropped entirely** for the runner's own `gh release`, on zizmor's advice: one less third-party action holding write access to this repository's releases. |
