# Build and test the Windows leg on the wintest box. Driven by
# scripts/wintest-ci.sh, which ships the tree over and invokes this.
#
# It runs ci.yml's three Windows commands VERBATIM -- Configure, Build, Test --
# for the same reason scripts/local-ci.sh reads its steps out of the workflow: a
# hand-written mirror of a pipeline drifts, and then passes locally for a build
# that fails on GitHub. What this script supplies is the ENVIRONMENT those
# commands run in, which on the runner comes from two actions:
#
#   scripts/setup-msvc.ps1     -> vcvars64.bat (was ilammy/msvc-dev-cmd
#                                 until GHUB-0031)
#   jurplel/install-qt-action  -> CMAKE_PREFIX_PATH, and Qt's bin on PATH
#
# The toolchain it expects is what GHUB-0091 installed: MSVC x64 from VS 2022
# Build Tools, Qt 6.8.3 msvc2022_64 under C:\Qt, and CMake + Ninja under
# C:\devtools. Every one of those is checked before anything is built, because a
# missing piece otherwise surfaces as a confusing CMake error much later.
# -Werror mirrors ci.yml's Windows matrix row, and wintest-ci.sh reads it out
# of that workflow rather than either script restating it. Without it this
# configured with the option OFF while CI configured it ON, so the box returned
# green for a build the runner would have failed -- which is the exact drift
# this file's header says it exists to prevent (GHUB-0185).
param([string]$Src = 'C:\gameshub', [string]$Werror = 'ON')
$ErrorActionPreference = 'Stop'

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { Write-Error "vswhere not found -- no Visual Studio installer on this box"; exit 2 }
$inst = & $vswhere -products '*' -all -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                   -property installationPath -latest
if (-not $inst) { Write-Error "no MSVC x64 toolchain installed"; exit 2 }
$vcvars = Join-Path $inst 'VC\Auxiliary\Build\vcvars64.bat'

$cmakeExe = Get-ChildItem 'C:\devtools\cmake' -Filter 'cmake.exe' -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1
if (-not $cmakeExe) { Write-Error "cmake not found under C:\devtools\cmake"; exit 2 }
$cmakeBin = $cmakeExe.DirectoryName
$ninjaBin = 'C:\devtools\ninja'
$qt = 'C:\Qt\6.8.3\msvc2022_64'

foreach ($p in @($vcvars, "$ninjaBin\ninja.exe", "$qt\bin\Qt6Core.dll", $Src)) {
    if (-not (Test-Path $p)) { Write-Error "missing: $p"; exit 2 }
}

Write-Output "MSVC : $inst"
Write-Output "Qt   : $qt"
Write-Output "CMake: $cmakeBin"

# vcvars64 sets its variables in one cmd session and nowhere else, so the whole
# recipe has to run as a single batch file rather than as separate calls.
$bat = @"
@echo off
call "$vcvars" >nul || exit /b 1
set "PATH=$cmakeBin;$ninjaBin;$qt\bin;%PATH%"
set "CMAKE_PREFIX_PATH=$qt"
cd /d "$Src" || exit /b 1
echo === Configure
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DGAMESHUB_WERROR=$Werror || exit /b 1
echo === Build
cmake --build build || exit /b 1
echo === Test
ctest --test-dir build --output-on-failure || exit /b 1
echo === Windows leg passed
"@
$batPath = Join-Path $env:TEMP 'wintest-build.bat'
Set-Content -Path $batPath -Value $bat -Encoding ASCII
& cmd /c $batPath
exit $LASTEXITCODE
