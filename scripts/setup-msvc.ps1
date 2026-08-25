# Put the MSVC toolchain into the environment of every step that follows.
#
# Without a developer command prompt CMake cannot find cl.exe, and the Ninja
# generator has no toolchain discovery of its own -- so this is what lets the
# Linux and Windows legs share one build recipe. It also puts Visual Studio's
# own ninja.exe on PATH, via vcvarsall's CommonExtensions\Microsoft\CMake\Ninja
# entry (measured on a 2022 BuildTools install, not assumed).
#
# This was ilammy/msvc-dev-cmd until GHUB-0031. That action is unmaintained
# -- last release 2024-01, still declaring node20, which GitHub is retiring
# -- and it did exactly what is done here: run vcvarsall.bat and export the
# variables it changed. Doing it directly is fewer moving parts, and one
# less third-party action running against workflows that publish binaries
# strangers download.
#
# It lives in a script rather than inline in both ci.yml and release.yml
# because a second copy of a build step is a copy that drifts, and then the
# release builds differently from the thing CI proved.

$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "no vswhere.exe at $vswhere" }

$install = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $install) { throw 'no Visual Studio install carries the C++ toolset' }

$vcvars = Join-Path $install 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path $vcvars)) { throw "no vcvarsall.bat at $vcvars" }

# Only the variables vcvarsall actually CHANGED are exported. Writing the
# whole environment back over itself would work but hides what the step did,
# and exporting nothing has to be a failure rather than a silent no-op: an
# empty export leaves cl.exe missing and surfaces later as a confusing CMake
# error about no compiler, several steps away from the cause.
$exported = cmd /c "call `"$vcvars`" x64 >nul && set" |
    Where-Object { $_ -match '^([^=]+)=(.*)$' -and
                   [Environment]::GetEnvironmentVariable($matches[1]) -ne $matches[2] }
if (-not $exported) { throw "$vcvars changed no variables; the toolchain is not set up" }

$exported | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
Write-Host "exported $($exported.Count) variables from $vcvars"
