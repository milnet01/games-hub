# Security

Games is a single-player desktop game collection. It is worth being honest
about how small its attack surface is, because that is what tells you which
reports matter.

**This section is about the desktop app.** `scorepad/` is a separate thing —
a phone score book that talks to a hosted database — and § The score book
below covers it. The two have different answers, and a reader who takes the
claims below as the whole repository's will get the score book wrong.

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

## The score book

`scorepad/` is a phone score book for a real table, and it does not share the
claims above. It loads the Firebase SDK from `gstatic.com`, and sharing a game
opens a subscription to a hosted Realtime Database and writes the players'
initials, every hand's scores and a per-device id to it. A room is named by a
four-letter code with no password, so anyone who guesses one can read that
room, and — depending on the deployed database rules, which are not in this
repository — write to it. `scorepad/README.md` sets out the trade and how to
lock the rules down.

Nothing here reaches the desktop app: the two share numbers, not a process.

## Reporting something

Open a [security advisory](https://github.com/milnet01/games-hub/security/advisories/new),
or a normal issue if you would rather do it in the open. For the desktop app —
no network, no credentials — most findings are fine to discuss publicly. A
finding about the score book's rooms or database rules is not: send that as an
advisory, because a live room code is somebody's game in progress.

Please include what you did, what happened, and which download or commit you
were on (`gameshub --version` prints it).

## Which versions get fixes

The most recent release. This is a hobby project with one maintainer and no
support commitment; older versions are not patched.

## Bundled Qt

The downloads bundle Qt 6, so a Qt vulnerability is inherited here. Those
are reported to The Qt Project under its
[security policy](https://wiki.qt.io/Qt_Project_Security_Policy), not
here — but do open an issue if a release is still shipping a Qt version with
a known advisory, because bumping it is this project's job.

**Last checked: 2026-09-13**, against Qt's
[list of known vulnerabilities](https://wiki.qt.io/List_of_known_vulnerabilities_in_Qt_products).
The downloads carry Qt 6.8.3, and that list names 6.8.3 in advisories fixed
by later Qt releases. Each one is in a part of Qt the app does not link, or
needs outside input the app does not read: an image, an SVG or XML file,
markdown, a `data:` URL or a network connection. The app's one outside input
is a saved game, which it parses itself. The bump is planned anyway.
