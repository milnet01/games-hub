# Security

Games is a single-player desktop game collection. It is worth being honest
about how small its attack surface is, because that is what tells you which
reports matter.

## What it touches

- **No network.** Nothing in the app opens a socket, fetches a URL or phones
  home. There is no telemetry and no update check.
- **No accounts, no passwords, no personal data.** There is nothing to log
  in to.
- **The files it writes** are its own: window sizes, best scores, the house
  rules you set in Canasta, and a saved game per game. They live where
  QSettings puts them for your platform — `~/.config/GamesHub/` on Linux,
  the registry under `HKCU\Software\GamesHub` on Windows.
- **The one input it parses that it did not write** is a saved game, if
  someone hands you a settings file. Every game re-checks a save before
  restoring it rather than trusting it — a card game confirms the whole pack
  came back, Chess replays the move list through its own move generator, and
  a board that could not have been reached is refused. A save that fails
  those checks is discarded and a new game starts.

## Reporting something

Open a [security advisory](https://github.com/milnet01/games-hub/security/advisories/new),
or a normal issue if you would rather do it in the open — for a game
collection with no network and no credentials, most findings are fine to
discuss publicly.

Please include what you did, what happened, and which download or commit you
were on (`gameshub --version` prints it).

## Which versions get fixes

The most recent release. This is a hobby project with one maintainer and no
support commitment; older versions are not patched.

## Bundled Qt

The downloads bundle Qt 6, so a Qt vulnerability is inherited here. Those
are reported to [The Qt Project](https://www.qt.io/product/security), not
here — but do open an issue if a release is still shipping a Qt version with
a known advisory, because bumping it is this project's job.
