<!-- ants-spec-format: 1 -->
# GHUB-0043 — Tell the player a new version exists, and install it on Linux

**Status:** accepted (2026-11-04).
**Kind:** feature.
**Source:** ROADMAP GHUB-0043 (user-request-2026-08-20; four decisions taken
with the owner then, recorded in § 3).
**Pairs with:** GHUB-0025 (the release workflow this adds a signing step to).

**Layman:** Games checks GitHub for a newer release once a day if you let it,
shows what changed in every version you missed, and on Linux replaces itself
and restarts when you say yes.

## 1. Goal

A player running a downloaded copy learns that a newer version exists without
visiting the releases page. They see the notes for every version between
theirs and the newest. On the Linux AppImage, **Update now** downloads the new
file, checks its signature, swaps it in and restarts the app. On Windows the
same offer opens the download page. Every other kind of install — a
`cmake --install` copy, a build directory, a distribution package, a Flatpak —
has no update code in it at all.

## 2. Problem

1. **A download is frozen at the version fetched.** Nothing in the app knows
   a release exists. `SECURITY.md` § What it touches says so in as many words:
   "There is no telemetry and no update check."
2. **That sentence becomes false the day this ships**, and it is the sentence
   that tells a reporter which findings matter. It has to be rewritten in the
   same change (§ 11).
3. **Running downloaded code is the most dangerous thing this app could
   do.** The download must be proven to be the project's own before it
   replaces anything. GHUB-0054's build attestations prove provenance to a
   person running `gh attestation verify`. An app cannot check those offline
   without carrying a Sigstore client, so the app needs its own signature
   (§ 8).
4. **finbreak shipped this and found two traps in the field** (its spec,
   `finbreak/docs/specs/FIBR-0054.md`, D8). Re-executing in place over the
   running AppImage fails, because the old image is still mounted: the app
   closed and never came back. And the new process must wait for the old one
   to exit rather than assume it has.

## 3. Scope decisions (agreed with the user)

Taken with the owner on 2026-08-20 and recorded in the GHUB-0043 roadmap body:

- **Linux AppImage installs itself; Windows offers the download page.** The
  Windows file is a portable zip, so installing means replacing a directory.
  That is a separate item, not part of this one.
- **Releases are signed with Ed25519.** A public-domain verifier ships in the
  tree, so no new library is needed. The private key never enters the
  repository. A download that does not verify is deleted, never installed.
- **The first launch asks whether to check automatically.** Nothing reaches
  the network before that answer. **Help → Check for updates** works whichever
  way it was answered.
- **One automatic check a day, not one a launch.**

**Still open, and the owner's (Waiting-on: user, 2026-11-04):** who generates
and holds the signing key, and whether it may be stored as a GitHub Actions
secret. The recommendation on the roadmap item stands: the owner generates it
on this machine, keeps the private half offline, and adds the secret
themselves, so no session ever holds it. **Nothing here waits on that
answer.** The code is built and tested against throwaway keys. Until the real
public key is compiled in, the updater stays inert (INV-2).

## 4. Design

### 4.1 Where the code lives, and the build switch that removes it

A new CMake option decides whether any update code is built:

```cmake
option(GAMESHUB_UPDATER "Build the self-updater (release downloads only)" OFF)
```

- **OFF** (the default, and every `cmake --install`, OBS and Flatpak build):
  none of `src/update/*` is compiled into `gameshub`, and no update menu
  entry or setting exists. **This does not remove `Qt6::Network`
  from the binary**: it is already a direct dependency today, pulled in through
  `Qt6::Multimedia`'s link interface (`readelf -d build/gameshub` lists
  `libQt6Network.so.6`, checked 2026-11-04). The guarantee is that no code of
  ours calls it.
- **ON**: `release.yml` passes it for both downloads. `ci.yml` passes it
  too, so the networked half compiles and its offline tests run on both
  platforms. The Linux leg also configures a second tree with the default,
  builds only `gameshub` in it, and runs INV-2's `nm` check there.

The work splits the way the games do: a part with no network and no widgets
that the self-test can drive, and a part that talks to the network and the
screen.

- `src/update/updatelogic.*` — QtCore only, in a library of its own. It is
  always built and always linked into `gameshub_selftest`, and linked into
  `gameshub` and `gameshub_uitest` only with `GAMESHUB_UPDATER`. It is not
  in `GAME_CORE_SOURCES`, because `gameshub_core` is linked into every
  `gameshub`. Version parsing and comparison, reading the releases JSON,
  choosing assets, collecting notes, the once-a-day rule, `detectInstall()`
  and the Ed25519 verifier. It reads no settings and no environment: the
  running version, the clock, the stored `update/*` values, the platform,
  the environment variables, the running executable's path and the key all
  arrive as arguments. `GAMESHUB_VERSION` is defined only for the app and
  view targets, and a supplied clock is what lets INV-7 be tested.
- Everything under `src/update/*` is in namespace `update`, which INV-2's
  `nm` check counts. The vendored verifier keeps its own `crypto_` names,
  which the same check counts too.
- `src/update/updater.*`, `src/update/updatedialog.*` — built only with
  `GAMESHUB_UPDATER`. The network calls, the first-run question, the offer
  dialog, the download, the swap and the relaunch.

### 4.2 What the release publishes

For every asset it uploads, `release.yml` also uploads a detached signature
named after it:

```
GamesHub-<version>-x86_64.AppImage       the download
GamesHub-<version>-x86_64.AppImage.sig   64 raw bytes: Ed25519 over the file's exact bytes
GamesHub-<version>-windows-x64.zip
GamesHub-<version>-windows-x64.zip.sig
```

The signature is the raw 64-byte RFC 8032 signature over the whole file — no
hashing step first, no armour, no header. The publish job makes it with
OpenSSL, which the runner already has:

```bash
openssl pkeyutl -sign -rawin -inkey key.pem -in "$f" -out "$f.sig"
```

The private key comes from a repository secret, `UPDATE_SIGNING_KEY`, holding
the PEM that `openssl genpkey -algorithm ed25519` writes. It is written to a
file for the step and deleted after it.

**Before uploading, the job proves the round trip.** It verifies each `.sig`
against the public key derived from the secret. Then it compares that public
key with the one compiled into the app (§ 4.3). A mismatch fails the publish:
a release signed with a key the app does not carry could never be installed.

**With no secret set, the job signs nothing, says so in the log, and
publishes as today.** The app then finds no `.sig` and offers nothing (INV-3).
This is the state until the owner answers § 3's open question.

### 4.3 The public key

`updatekey.h`, a new header beside `src/update/updatelogic.*`, holds the
32-byte public key as a byte array. **It
ships empty.** An empty key makes the updater inert everywhere (INV-2), so the
feature can be merged before the owner has a key. Filling it in is a one-line
change, made in the same commit that first sets the secret.

The `release.yml` comparison in § 4.2 reads this file, so its form is fixed.
It is one line, in one of two states:

```cpp
inline constexpr std::array<unsigned char, 0> kUpdatePublicKey{};
inline constexpr std::array<unsigned char, 32> kUpdatePublicKey{0x1a, 0x2b, /* … 32 bytes */};
```

No other line in the file names `kUpdatePublicKey`. The workflow takes the
`0x..` tokens on that line, in order. None means empty. Otherwise there
must be 32, equal to the last 32 bytes of `openssl pkey -pubout -outform
DER` run on the secret. With the secret set, an empty key is a mismatch and
fails the publish. The app treats `kUpdatePublicKey.empty()` as inert.

### 4.4 Which installs may update

Decided once at start-up, by `detectInstall()`:

| Build | Runtime state | Result |
|---|---|---|
| `GAMESHUB_UPDATER` OFF | any | no update code exists |
| ON, empty public key | any | inert: no check, no menu entry |
| ON, Linux | `$APPIMAGE` names an existing file, and the running executable is inside `$APPDIR` | **AppImage**: check, offer, install |
| ON, Linux | anything else | inert |
| ON, Windows | — | **Zip**: check, offer, open the download page |

"Inert" means no network request is ever made and the update menu entry and
setting are not shown.

### 4.5 Consent and the once-a-day rule

Settings keys, all under `update/`:

| Key | Meaning |
|---|---|
| `update/auto` | absent: not asked yet. `true` / `false`: the player's answer |
| `update/lastCheck` | when the last automatic check was **attempted**, ISO-8601 UTC |
| `update/skipped` | a version the player chose to skip, e.g. `1.4.0` |

- **The first launch with an updating install asks**, in a modal dialog
  once the window is up: check automatically, yes or no. Being modal, it
  leaves the Help menu unreachable until it is answered. No request leaves before the answer. The
  answer is stored and can be changed in Preferences (GHUB-0068), where the
  switch joins the others.
- **An automatic check runs only when `update/auto` is `true` and 24 hours
  have passed since `update/lastCheck`.** The attempt time is written before
  the request goes out, so a check that fails still counts and the app does
  not retry every launch while offline.
- **Help → Check for updates** always checks, because asking is consent for
  that one request. It ignores `update/skipped` and says "You have the newest
  version" when that is so.
- **The updater starts only when `main()` starts it**, after the command
  line is parsed, and never under `--shot`, where the hub is already not
  remembering (`HubWindow::setRemembering`). `HubWindow`'s constructor
  starts nothing. The updater is given its install kind, its key and its
  network access, which `main()` fills from `detectInstall()`,
  `kUpdatePublicKey` and the real network. So a test builds an updater as
  an AppImage or Zip install, with a throwaway key and a counting fake
  (INV-1, INV-4). Every other
  test builds none, so it makes no request and sees no first-run question.

### 4.6 The check

One HTTPS GET of `https://api.github.com/repos/milnet01/games-hub/releases`,
with a 30-second timeout and the reply capped at 1 MiB. From the reply:

- Drafts and pre-releases are ignored.
- A tag is a version only when it is `v` followed by exactly three dot-separated
  decimal numbers. Anything else is skipped, never guessed at.
- The newest version wins. It is offered only when it is **strictly newer**
  than the running `GAMESHUB_VERSION`, is not `update/skipped` (automatic
  checks only), and the release carries both the install's asset and its
  `.sig`.
- The notes shown are the bodies of **every** release newer than the running
  version, newest first, each under its version and title.

Any failure — no network, a timeout, a non-200 status, JSON that does not
parse, a reply over the cap — ends an automatic check silently, as if up to
date. A Help-menu check says in one sentence that the server could not be
reached.

### 4.7 The offer

A dialog over the tile grid or the game, never a status-bar line (the status
bar is not read during play). It names the running and new versions, shows
the collected notes in a scrolling read-only box at the app's legible size,
and offers three buttons:

- **Update now** (AppImage) or **Open the download page** (Zip).
- **Later** — nothing is stored; the next due check offers it again.
- **Skip this version** — stores `update/skipped`.

The download-page button opens the release's `html_url` only if it is an
`https://github.com/` address.

### 4.8 Install on Linux

In this order, and stopping at the first failure:

1. Download the AppImage and its `.sig` into the directory holding
   `$APPIMAGE`, under temporary names. Same directory, so the final rename
   is atomic. The download is capped at 200 MiB, and a progress bar shows
   it with a Cancel button.
2. Verify the signature over the downloaded bytes against the compiled-in
   key. **On failure, delete both files and stop.** Nothing is installed.
3. Mark the file executable and rename it over `$APPIMAGE`.
4. Start a detached waiter, then quit through the normal exit path. The
   waiter is `/bin/sh` in its own session. It waits until this process's
   PID has gone, then starts the new `$APPIMAGE` with `APPIMAGE`, `APPDIR`
   and `ARGV0` removed from its environment. **The normal exit is what saves
   the game in progress** (`HubWindow::closeEvent` banks it), so the update
   loses no play.

A directory the player cannot write to fails at step 1. That error offers the
download page instead.

## 5. Invariants

- **INV-1** — **No request leaves the machine without consent.** An updating
  install makes no network request before `update/auto` has an answer, and
  none automatically while it is `false`. While it is `false`, only Help →
  Check for updates reaches the network.
  *Test:* `tests/uitest.cpp`, with the updater's network access replaced by a
  counting fake: a hub with `update/auto` absent, then `false`, makes zero
  requests. Answering yes makes one. Help → Check for updates with `false`
  makes one.
  *Breaks when:* the check starts before the first-run question is
  answered, or reads an absent key as yes.

- **INV-2** — **Only a build with the switch contains update code, and only
  the AppImage and Zip installs of one carrying a key run it.** A build
  without `GAMESHUB_UPDATER` contains nothing from `src/update/*` (§ 4.1). A
  build with it, but with an empty key or outside an AppImage on Linux,
  makes no request and shows no update menu entry or setting.
  *Test:* the self-test checks `detectInstall()` against every row of
  § 4.4's table after the first, on both platforms' rows, with the platform,
  environment, executable path and key supplied by the test. The first row
  is the `nm` check's: on a default build,
  `nm -C --defined-only build/gameshub | grep -c -e 'update::' -e 'crypto_'` → `0`
  (run 2026-11-04 on today's build; the same command counting `HubWindow::`
  is the control that it sees our symbols). CI's Linux leg runs it on its
  second, default tree (§ 4.1).
  *Breaks when:* a distribution or Flatpak build turns the switch on, or the
  detection treats a build directory as an AppImage.

- **INV-3** — **Only a strictly newer, well-formed, signed release is
  offered.** The check offers nothing for an equal or older version, a
  malformed tag, a draft or pre-release, a skipped version (automatic checks
  only), or a release missing the install's asset or its `.sig`.
  *Test:* `tests/selftest.cpp`, on canned releases JSON: newer → offered;
  equal, older, `v1.4`, `v1.4.0-rc.1`, `latest` → not offered; `.sig` missing
  → not offered; skipped → not offered automatically, offered from the Help
  menu.
  *Breaks when:* the version compare is textual (`1.10.0` sorting before
  `1.9.0`), or a missing signature is treated as optional.

- **INV-4** — **Only a file whose signature verifies is ever installed.** A
  signature or file that fails verification, or a download cut short, leaves
  `$APPIMAGE` byte-for-byte as it was, and the temporary files are deleted.
  *Test:* `tests/selftest.cpp` runs the verifier on the RFC 8032 § 7.1 test
  vectors, then flips one byte of the message and one of the signature and
  expects both to fail. `tests/uitest.cpp` drives an install against a
  throwaway key into a scratch directory standing in for `$APPIMAGE`: a
  tampered download leaves the original unchanged and no temporary file
  behind.
  *Breaks when:* the verifier checks a hash of the file instead of its exact
  bytes, or the rename happens before verification.

- **INV-5** — **The relaunch waits for the old process, and the game in
  progress is saved first.** The new image starts only after the old PID has
  exited, from an environment without the old `APPIMAGE`, `APPDIR` and
  `ARGV0`, and the exit before it went through `HubWindow::closeEvent`.
  *Test:* manual, on a real AppImage: install `1.x.0` over an older build
  that carries the real key, mid-game; the app closes, reopens on the new version, and the game
  resumes. No automated test can mount an AppImage on the CI runners.
  *Breaks when:* the app re-executes itself in place (the finbreak field
  failure), or quits by a path that skips `closeEvent`.

- **INV-6** — **Each release's signatures match the key the app carries.**
  When the signing secret is set, every uploaded asset has a `.sig` that
  verifies, and the secret's public key equals `updatekey.h`'s (§ 4.3).
  *Test:* `release.yml`, the round-trip step in § 4.2; a mismatch fails the
  publish before anything is uploaded.
  *Breaks when:* the key is rotated in the secret but not in the source, or
  the reverse.

- **INV-7** — **The automatic check runs at most once a day, and failing
  counts.** With `update/auto` true, a second launch within 24 hours of the
  last attempt makes no request, whether that attempt succeeded or not.
  *Test:* `tests/selftest.cpp`, on the once-a-day rule with a supplied clock:
  23 h after an attempt → not due; 24 h → due; no previous attempt → due.
  *Breaks when:* the time is written only on success, so an offline machine
  retries every launch.

## 6. Failure modes

| What breaks | What happens |
|---|---|
| No network, DNS failure, timeout | Automatic check: nothing shown. Help menu: "The update server could not be reached." |
| GitHub rate-limits the API (HTTP 403/429) | Treated as any other failure. One request a day per player is far below the limit for unauthenticated calls. |
| The release has no `.sig` (no secret yet, or a failed upload) | Not offered (INV-3). |
| The signature does not verify | Download deleted; the dialog says the file could not be verified and nothing changed. |
| The disk fills, or the directory is read-only | Temporary files deleted, `$APPIMAGE` untouched; the dialog offers the download page. |
| The app is killed after the rename, before relaunch | The new version is already in place and starts next time. |
| The waiter cannot start | The new file is in place; the dialog says to restart by hand. |
| Linux host lacks the TLS library Qt loads at run time | The check fails as "could not be reached". `release.yml` asserts the TLS plugin is bundled (§ 11); the host's OpenSSL is not bundled. |

## 7. Tests

- `tests/selftest.cpp` — INV-2 (detection table), INV-3 (version and asset
  rules on canned JSON), INV-4 (verifier on RFC 8032 vectors and tampers),
  INV-7 (once-a-day rule). No network.
- `tests/uitest.cpp` — INV-1 (consent, with a counting fake in place of the
  network) and INV-4's install half (scratch directory, throwaway key). Built
  only with `GAMESHUB_UPDATER`, which CI sets.
- `release.yml` — INV-6, on every tagged release.
- Manual — INV-5, once, on the second release that carries the real key.
  The first keyed release is the earliest build that can verify a download,
  so it is the one updated from.

Each automated test is seen failing first: against a verifier that accepts
everything, a compare that is textual, and a check that ignores
`update/auto`.

## 8. Alternatives considered (and rejected)

- **GitHub build attestations instead of a key** (GHUB-0054's mechanism).
  Checking one needs a Sigstore client and network access to its
  transparency log. That is far more code than an Ed25519 verifier, for an
  app that otherwise has no network code at all.
- **A SHA-256 checksum file.** It proves the file arrived intact, not who
  made it: anyone who can replace the file can replace its checksum.
- **AppImageUpdate / zsync deltas.** Another tool and format for a file
  under 50 MB. A full download is simpler and is what finbreak ships.
- **Re-executing in place.** finbreak shipped this, and it failed in the
  field because the running image stays mounted.
- **Checking on every launch.** Rejected by the owner; one a day is enough.

## 9. Out of scope

- Installing on Windows (replacing the unpacked zip's directory) — deferred;
  not yet queued.
- Delta downloads, background polling, a pre-release channel — not planned.
- Distribution and Flatpak updates — their own package managers own them
  (GHUB-0044, GHUB-0045).

## 10. What checks this

| Rule | What catches a breach |
|------|----------------------|
| INV-1 | `tests/uitest.cpp`, the consent block |
| INV-2 | `tests/selftest.cpp`, the detection block; CI's `nm` check on a default build |
| INV-3 | `tests/selftest.cpp`, the release-selection block |
| INV-4 | `tests/selftest.cpp`, the verifier block; `tests/uitest.cpp`, the install block |
| INV-5 | **nothing automated** — no runner can mount an AppImage; the manual recipe in INV-5 |
| INV-6 | `release.yml`, the signing round-trip step |
| INV-7 | `tests/selftest.cpp`, the once-a-day block |
| § 4.5's "not during `--shot`" | **nothing** — the shot ctest case would pass either way, since offscreen runs have no network fake; review only |

## 11. Cross-doc impact

- `SECURITY.md` § What it touches — three bullets are rewritten in the same
  change.
  - "No network": what is fetched (the releases list and, on Update now, two
    files), from where, on whose consent (§ 4.5), and how a download is
    verified (§ 4.8). The hosts are `api.github.com` and `github.com`, whose
    asset downloads redirect to `release-assets.githubusercontent.com`
    (`curl -sI` on the v1.3.0 AppImage, 2026-11-04).
  - "The one input it parses that it did not write": the releases JSON and
    the downloaded file join the saved game.
  - "The files it writes": an AppImage install also replaces `$APPIMAGE` and
    writes two temporary files beside it.
- `docs/specs/GHUB-0025-downloadable-builds.md` — amended when this is built:
  INV-7's plugin assertion gains the `tls` plugin directory for both
  downloads, and § 4.3 gains the signing step.
- `docs/design.md` § Legibility (the stored-state families) and
  `docs/standards/versioning-overrides.md` — `update/*` joins the families.
- `README.md` — a short paragraph on updates, and that Windows opens the
  download page.
- `CHANGELOG.md` — an Added entry.

## 12. Cold-eyes loop log

Rows live in `../reviews/GHUB-0043-self-update-loop-log.md`.

## 13. Resource cost

No new Qt module: `Qt6::Network` is already in the binary through
`Qt6::Multimedia` (§ 4.1). The release builds call it; no other build does. No new
third-party library: the Ed25519 verifier is a small public-domain routine
beside `src/update/updatelogic.*`, its licence recorded in `packaging/THIRD-PARTY.md`. A check
holds at most 1 MiB in memory; a download at most 200 MiB on disk beside
`$APPIMAGE`, deleted on any failure.

## 14. Open questions

- **The signing key's custody** — the owner's (§ 3). Inert until answered.
- **Which public-domain Ed25519 verifier.** TweetNaCl and Monocypher (CC0
  option) both qualify; the choice is the implementer's, recorded in
  `packaging/THIRD-PARTY.md`.
