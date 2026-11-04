# Third-party software in this download

Genre: record

Games itself is MIT-licensed — see `LICENSE` beside this file. Every game
here is a traditional one whose rules are in the public domain, the code is
written from scratch, and the sound effects are synthesised by a script in
the repository rather than sampled from anywhere.

Two things in this download are not ours.

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

## TweetNaCl

The signature check that proves an update is this project's own is taken from
TweetNaCl, version 20140427, cut down to Ed25519 verification
(`src/update/ed25519.cpp` in the source).

- **Licence:** public domain. <https://tweetnacl.cr.yp.to/> says so: "TweetNaCl
  is a self-contained public-domain C library".
- **Modified:** yes. Only verification is kept; the message is hashed as it
  arrives instead of being held whole; a signature whose S is not below the
  group order is refused, as RFC 8032 § 5.1.7 requires.
- **Linking:** compiled into the program.
