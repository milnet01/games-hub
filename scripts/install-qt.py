#!/usr/bin/env python3
"""Install the prebuilt Qt the workflows build against (GHUB-0196).

Usage: install-qt.py <host> <arch>      e.g. linux linux_gcc_64
                                             windows win64_msvc2022_64

Replaces jurplel/install-qt-action. That action was pinned by SHA, but its
action.yml runs `jurplel/install-qt-action/action@v4`, a movable tag, so the
code that installed Qt in the job building the downloads was whatever that
tag pointed at on the day. Everything this script runs is fixed:

- aqtinstall comes from AQT_SRC, a git URL pinned to one commit;
- its dependencies are held to the exact versions in CONSTRAINTS. PyPI never
  lets a published version's files be replaced, so an exact version cannot
  change under us. A constraints entry a platform does not need is ignored.

Reads QT_VERSION and AQT_SRC from the environment, which each workflow sets
once at the top. Installs to $RUNNER_TEMP/Qt, where ci.yml's cache step
looks, and skips the download when that install is already present.

Then does what the action's environment step did: Qt's bin on PATH, its lib
on LD_LIBRARY_PATH (Linux), and QT_ROOT_DIR / QT_PLUGIN_PATH. CMake finds Qt
through PATH, linuxdeploy-plugin-qt finds qmake there, and release.yml finds
windeployqt there.
"""
import glob
import os
import subprocess
import sys

CONSTRAINTS = """\
backports.zstd==1.7.0
beautifulsoup4==4.15.0
brotli==1.2.0
bs4==0.0.2
certifi==2026.7.22
charset-normalizer==3.5.1
defusedxml==0.7.1
humanize==4.16.0
idna==3.20
inflate64==1.0.4
multivolumefile==0.2.3
patch-ng==1.19.1
psutil==7.2.2
py7zr==1.1.3
pybcj==1.0.8
pycryptodomex==3.23.0
pyppmd==1.3.1
requests==2.34.2
semantic-version==2.10.0
soupsieve==2.10
texttable==1.7.0
typing_extensions==4.16.0
urllib3==2.8.0
"""


def run(*cmd):
    print('+', ' '.join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def append(var, line):
    with open(os.environ[var], 'a', encoding='utf-8') as f:
        f.write(line + '\n')


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    host, arch = sys.argv[1:]
    version = os.environ['QT_VERSION']
    source = os.environ['AQT_SRC']
    tmp = os.environ.get('RUNNER_TEMP') or os.getcwd()
    out = os.path.join(tmp, 'Qt')

    def installs():
        exe = 'qmake.exe' if host == 'windows' else 'qmake'
        return glob.glob(os.path.join(out, version, '*', 'bin', exe))

    if installs():
        print(f'Qt {version} already in {out} -- skipping the download')
    else:
        venv = os.path.join(tmp, 'aqt-venv')
        run(sys.executable, '-m', 'venv', venv)
        bindir = 'Scripts' if os.name == 'nt' else 'bin'
        py = os.path.join(venv, bindir, 'python')
        cfile = os.path.join(tmp, 'aqt-constraints.txt')
        with open(cfile, 'w', encoding='utf-8') as f:
            f.write(CONSTRAINTS)
        run(py, '-m', 'pip', 'install', '--quiet', '-c', cfile, source)
        run(py, '-m', 'aqt', 'install-qt', host, 'desktop', version, arch,
            '-m', 'qtmultimedia', '--outputdir', out)

    found = installs()
    if len(found) != 1:
        sys.exit(f'expected one Qt {version} install under {out}, found {found}')
    root = os.path.dirname(os.path.dirname(found[0]))
    print(f'Qt root: {root}')

    # Outside Actions (a local trial run) there is no GITHUB_ENV to write.
    if 'GITHUB_ENV' not in os.environ:
        return
    append('GITHUB_PATH', os.path.join(root, 'bin'))
    append('GITHUB_ENV', f'QT_ROOT_DIR={root}')
    append('GITHUB_ENV', f'QT_PLUGIN_PATH={os.path.join(root, "plugins")}')
    if host == 'linux':
        lib = os.path.join(root, 'lib')
        old = os.environ.get('LD_LIBRARY_PATH')
        append('GITHUB_ENV', f'LD_LIBRARY_PATH={old}:{lib}' if old else f'LD_LIBRARY_PATH={lib}')


if __name__ == '__main__':
    main()
