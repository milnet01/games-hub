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
# -Defines is every -D flag on ci.yml's Configure line, with the Windows matrix
# row filled in, as NAME=VALUE,NAME=VALUE. wintest-ci.sh reads it out of the
# workflow; neither script restates it. A hand copy here drifted twice, each
# time returning green for a build the runner would have built differently:
# werror off (GHUB-0185), then the self-updater left out (GHUB-0235).
param([string]$Src = 'C:\gameshub', [Parameter(Mandatory)][string]$Defines)
$ErrorActionPreference = 'Stop'

# The same shape wintest-ci.sh enforces. Checked again here because each value
# is pasted into a batch file below, where a stray character would be a command.
$flags = foreach ($d in $Defines.Split(',')) {
    if ($d -notmatch '^[A-Za-z_][A-Za-z0-9_]*(:[A-Z]+)?=[A-Za-z0-9_.-]+$') {
        Write-Error "not a NAME=VALUE CMake flag: '$d'"; exit 2
    }
    "-D$d"
}
$flags = $flags -join ' '

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
echo cmake -S . -B build -G Ninja $flags
cmake -S . -B build -G Ninja $flags || exit /b 1
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
