# Third-party software in this download

Games itself is MIT-licensed — see `LICENSE` beside this file. Every game
here is a traditional one whose rules are in the public domain, the code is
written from scratch, and the sound effects are synthesised by a script in
the repository rather than sampled from anywhere.

One thing in this download is not ours.

## Qt 6

This build bundles the Qt 6 libraries and plugins it needs, so that it runs
without Qt being installed on your machine.

- **Licence:** LGPL-3.0. Full text in `licenses/LGPL-3.0.txt`, together with
  `licenses/GPL-3.0.txt`, which LGPL-3.0 incorporates by reference.
- **Modified:** no. The Qt binaries here are the ones The Qt Company
  publishes, used unchanged.
- **Linking:** dynamic. The Qt libraries sit beside the executable as
  separate files, so you can replace them with your own build of the same Qt
  version — which is the relinking right the LGPL exists to protect.
- **Source:** the matching sources are published at
  <https://download.qt.io/archive/qt/>, under the directory for the version
  named in the build. Run `gameshub --version` for the Games version; the Qt
  version is recorded in the release notes for that download.

Qt is a trademark of The Qt Company Ltd. Nothing here is endorsed by them.
