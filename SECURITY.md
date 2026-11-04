# Security

Genre: record

Games is a single-player desktop game collection. It is worth being honest
about how small its attack surface is, because that is what tells you which
reports matter.

**This section is about the desktop app.** `scorepad/` is a separate thing —
a phone score book that talks to a hosted database — and § The score book
below covers it. The two have different answers, and a reader who takes the
claims below as the whole repository's will get the score book wrong.

## What it touches

- **The network, in the downloads only, and only if you agree.** The Linux
  AppImage and the Windows zip can check for a new version (GHUB-0043). A
  copy installed any other way -- built from source, or from a distribution
  or Flatpak package -- contains no network code of ours at all.
  - **What is fetched, from where:** the list of releases, from
    `api.github.com`. On Linux, **Update now** also fetches the new AppImage
    and its signature from `github.com`, which redirects the download to
    `release-assets.githubusercontent.com`. Nothing about you is sent; the
    request names the app and its version, and nothing else.
  - **On whose consent:** the first launch asks whether to check once a day,
    and nothing is fetched before you answer. Answer no and only Help →
    Check for updates reaches the network. The answer can be changed in
    Settings → Preferences.
  - **How a download is checked:** the release workflow signs each download
    with an Ed25519 key whose public half is compiled into the app. The app
    replaces itself only with a file whose signature verifies against that
    key, and deletes one that does not. Until the project's key exists the
    updater makes no request at all.
  - There is no telemetry.
- **No accounts, no passwords, no personal data.** There is nothing to log
  in to.
- **The files it writes** are its own: window sizes, best scores, the house
  rules you set in Canasta, and a saved game per game. They live where
  QSettings puts them for your platform — `~/.config/GamesHub/` on Linux,
  the registry under `HKCU\Software\GamesHub` on Windows. On Linux, an
  AppImage that updates itself also replaces its own file, after writing the
  new one under a temporary name beside it.
- **The inputs it parses that it did not write** are three. The update
  check's list of releases, which is read as JSON, capped at 1 MiB, and
  never trusted to name anything but a version and a GitHub address. A
  downloaded update, which is never run or unpacked before its signature
  verifies. And a saved game, if
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

**Last checked: 2026-11-04**, against Qt's
[list of known vulnerabilities](https://wiki.qt.io/List_of_known_vulnerabilities_in_Qt_products),
for the downloads' Qt 6.11.3. One advisory's range covers 6.11.3:
CVE-2026-15037, XML injection in QDom, affected "before Qt 6.12.0". QDom is
in the Qt XML module, which this app does not link (`CMakeLists.txt` finds
Widgets, Multimedia and Concurrent only), so it is not reachable here.

The downloads stay on 6.11.3 for now, because 6.12.0 is no better: the list
names it under two advisories that 6.11.3 is clear of. CVE-2026-100140 is in
Qt5Compat's XML parser, which this app does not link. CVE-2026-105924 is in
converting CBOR to JSON, which nothing in `src/` does. Both are fixed in
6.12.1, which fixes CVE-2026-15037 as well, and the downloads move to it once
Qt publishes it (GHUB-0213).
