# Versioning Overrides — Games Hub

Answers the two questions `~/.claude/standards/versioning.md` deliberately
refuses to answer for a project — **§ 3, what a breaking change can break
here**, and **§ 4, what would make this `1.0`** — and records the local facts a
conformer needs under its §§ 5 and 7. § 3's answer is required of every
project. **§ 4's was required while this project was `0.x`, and that ended:
`1.0.0` shipped on 2026-09-08**, so § 2 below is a record of a discharged
condition rather than a bar still to clear.

**Everything else is the global standard's and is deliberately not restated** —
which level to bump, the `0.x` shift, the security carve-out, the three ordered
changelog tests, the `-rc.N` spelling. A rule stated twice is two rules that
will disagree. Read that file first; this one only adds what is local.

## 1. Breaking surfaces

Global § 3 asks each project to name what a user relies on, so *"has something
that used to work stopped working?"* has a referent. **These are the surfaces
that recur here, not a closed list** — global § 3's last bullet governs
anything they miss.

**A saved game.** Twelve of the fourteen games save. **Changing what a game's
`saveState()` writes is breaking — whether or not the stamped version moves**,
and leaving the stamp behind makes it worse rather than smaller.

Each saving game stamps a `quint32`, and **eleven of the twelve refuse a
mismatch outright** — `KlondikeView::restoreState` returns `false` unless it
reads `1`. **Canasta is the exception, and it is the game where that matters
most**: it accepts any version from 1 up to its derived `kBlobVersion` and
defaults the fields an older blob predates, so appending a tail there is a
MIGRATION rather than a break. Prefer that route to taking one — reading this
paragraph as universal is how a Canasta save change gets cut as a MAJOR for a
change under which nothing a player had stops working. **A refused
save is silent** — `hubwindow.cpp` keeps the fresh deal it already dealt, so
the app runs, nothing looks wrong, the player is told nothing, and closing the
game writes the fresh state over the old blob. **Passing the gate wrongly is worse, and
whether anything catches it is luck rather than design.** The guards past the
stamp are about the cards, not the format: Klondike's `readPile` failing, and
`cardcodec::matchesPack` refusing a pack with a card missing or doubled. Those
catch a change that shifts the stream. **A field appended at the end shifts
nothing** — the old layout reads back clean, the extra bytes go unread, and the
game restores a state that is silently missing whatever the new field carried.
The games with no pack lean on their own core's `restore()` in the same
incidental way, and there are **five** — Minesweeper, Reversi, Draughts, Sudoku
and 2048. Chess and Canasta have no pack check either and are not among them:
Chess replays its move list and Canasta serialises its engine, which § 1's
save paragraph above describes.

**The settings store.** Renaming a key, or changing what a value means, loses
the setting without saying so. The families are `display/legibility`,
`donate/ask` and `donate/launches`; `window/geometry/<page>` and `saved/<game>`;
the per-game keys, which are scores AND remembered preferences (`chess/wins`,
`sudoku/best_time_*`, `freecell/best_moves`, `minesweeper/level` and the rest);
and Canasta's `canasta/house/*` and `canasta/target`. **Adding a
key with a default is not breaking** — nothing that used to work stops.

**The command line, both the flags and what they print.** `--game <name>`
takes the name the tile shows, so renaming a registered game breaks a launcher,
a script or a desktop file someone has pinned — Klondike is registered as
`Solitaire`, and that is the name the flag takes. `--version` and `-v` print
`Games <version>` on stdout, and **the prefix is already a contract**:
`release.yml` asserts it against both artifacts — `grep -q '^Games '` on Linux,
`Select-String -Pattern '^Games '` on Windows — so changing it fails the
release. **`-v` is not covered.** Both legs invoke `--version` only, so
deleting the alias leaves every check green and breaks only whatever a packager
or a launcher wrote.

**The keyboard shortcuts**, which are in a player's fingers rather than in a
document.

**Not surfaces**, however large the diff: class and file names, the rules-core
and view split, which half of `CMakeLists.txt` a file links into, and anything
else no user and no integrator can observe. **Integrator is the half worth
holding onto** — global § 2's definition names them, and this project is
heading for distribution packaging (GHUB-0044, GHUB-0045), so an install
target, an option name, or the configure-time `CMAKE_INSTALL_PREFIX` contract
is a surface even though no player will ever see it.

## 2. What made this `1.0`, and what changed when it landed

**`1.0.0` shipped on 2026-09-08**, carrying GHUB-0054 and GHUB-0053 — the
last two of the six below to be cleared, which are not the table's last two
rows. The condition is discharged. This section is kept as the record of it —
what the bar was, why those six, and when each was cleared — because the table
is the only place that says so.

**Global § 4's `0.x` shift lapsed with that release, and saying so is the one
thing here a conformer still has to act on.** That section applies only while
MAJOR is `0`; from `1.0.0` the global ladder governs unshifted. The visible
consequence for this project is the reverse of what three releases here had got
used to: **a new game is a MINOR now, where it was a PATCH.** The ladder itself
is global § 2's and is deliberately not restated — read it there, and do not
infer a level from any tag cut before 2026-09-08.

**`cut-release` follows the same lapse.** Its `### Added` level floor is skipped
while MAJOR is `0` and is live here from now on, so a patch step carrying an
addition is stopped rather than waved through.

They were these six because they were the two ways this project could let a
stranger down: it could lose their saved game, and it could hand them a binary
they had no way to trust. GHUB-0067 and GHUB-0075 are the first; GHUB-0054,
GHUB-0050, GHUB-0031 and GHUB-0053 are the second — a build you can verify,
made from inputs that were pinned, on a runtime that still exists, by a
compiler that was allowed to object. **New games did not gate it** — a
collection can always grow, and waiting for a fifteenth is how a leading zero
goes inert. Owner's call, 2026-08-20, on a list of five; GHUB-0075 was added by
this document's own cold gate, which found that § 1's silent-loss path had no
guard.

| Item | What it fixed | Cleared in |
|------|---------------|------------|
| GHUB-0067 | a save survived a clean exit and nothing else, and two copies of the app overwrote each other | 0.6.0 |
| GHUB-0075 | nothing checked that a save written by an older build still loads, so § 1's silent loss had no guard | 0.6.0 |
| GHUB-0054 | a downloaded release could not be checked against what the workflow built | 1.0.0 |
| GHUB-0050 | the release workflow downloaded two unpinned binaries and ran them | 0.6.0 — **no `CHANGELOG.md` bullet ever claimed it**, found 2026-09-08 while writing this table. The fix is in `release.yml` and the roadmap bullet is ✅; 0.6.0's section is a published record and is not rewritten to add it |
| GHUB-0031 | the Windows build rode an action GitHub was deprecating the runtime under | 0.5.0 |
| GHUB-0053 | the build asked the compiler for no warnings and no hardening | 1.0.0 |

## 3. This project does not cut release candidates

**No candidate builds, and therefore no `-rc.N` tags.** Owner's call,
2026-08-20: there are no testers to hand one to, and a candidate nobody
installs is a release cycle bought for nothing.

Global § 5 still governs the suffix if that ever changes. **It would be work
rather than a flag**, and the shape of it is recorded in GHUB-0076 so nobody
has to measure it twice. **Today a candidate tag never gets that far.** `verify` holds **three**
checks and a suffixed tag fails the first: `${GITHUB_REF_NAME#v}` is
`1.0.0-rc.1` against `CMakeLists.txt`'s `1.0.0`. Behind it sit the
`CHANGELOG.md` heading grep and an `awk` that fails when the section that
heading names is empty. **Relax all three and it would then publish as the
latest release**, because `gh release create` carries no `--prerelease` — and
that fourth change is the one easiest to miss, precisely because the three in
front of it hide it.

**The `awk` is the one that hides longest**, because it keys on the exact
`## [$tag]` heading. A grep relaxed to accept `v1.0.0-rc.1` against a
`## [1.0.0]` section leaves the `awk` matching no heading and failing on empty
notes — a stop that arrives after the two that look like the whole guard.

## 4. Version lines that are not the app's version

Global § 7's case, here: **twelve save-format versions, one per saving game.**
There is no single "save version" for the app and nothing should invent one.

They are absent from `.claude/bump.json` on purpose, and `$note_save_versions`
records why, as global § 7 requires.

## What checks this

| Rule | What catches a breach |
|------|----------------------|
| § 1 — a change to what `saveState()` writes is breaking | `savesFromOlderBuildsStillLoad` in `tests/uitest.cpp`, which restores the committed `tests/saves/` corpus — one blob per saving game. A change that REFUSES an old save reddens it. GHUB-0075 delivered this in 0.6.0, and § 2's table records it. **A field appended at the END is still uncaught**: the old layout reads back clean, the extra bytes go unread, and the game restores a state silently missing whatever the new field carried — § 1 above describes exactly that, and nothing tests it |
| § 1 — a settings key is not renamed | **nothing** — a renamed key reads as absent and falls back to its default, which is indistinguishable from a first run |
| § 1 — `--game` names stay stable | **partly, and not by design.** Several `openGameNamed` call sites pass a literal — Canasta, Chess, Hearts, Pyramid, Spider, Sudoku — and `anInterruptedDragPutsTheRunBack<V>` passes Solitaire, then fails when `findChild<V*>()` returns null. An unknown name opens the tile grid instead of a game, so no view is built and that case goes red: renaming any name above is caught. The callers that iterate `gameNames()` follow a rename rather than catching it, so every other registered name is uncaught. Re-measured 2026-09-08 |
| § 1 — the `--version` output keeps its prefix | `release.yml`'s smoke tests — `grep -q '^Games '` on Linux, `Select-String -Pattern '^Games '` on Windows. The prefix is guarded; the version it prints is not |
| § 1 — the `-v` alias keeps existing | **nothing** — both smoke legs invoke `--version` only, so deleting `-v` leaves every check green |
| § 1 — keyboard shortcuts stay stable | **nothing** — no test presses a key it does not already know about |
| § 1 — install targets, option names and the configure-time `CMAKE_INSTALL_PREFIX` contract | **nothing** — nothing configures or installs the project with non-default options |
| § 2 — the `1.0` condition | discharged 2026-09-08; nothing to check. **What is unchecked now is the lapse**: no test and no script asserts that a level was chosen on the unshifted ladder, and `cut-release`'s floor catches only an addition under a PATCH step |
| § 3 — no candidate is cut | `release.yml` rejects a suffixed tag at the first of `verify`'s three checks — by accident rather than by intent, but the effect is the rule's |
| § 4 — save versions stay out of the recipe | `.claude/bump.json` lists only `CMakeLists.txt` and `README.md`, so a release cannot walk them; `post_check` verifies those two against `CHANGELOG.md` |

## Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 1 | 2026-08-20 | 3, cold — genre pinned `standard`; packet carried global §§ 3, 4, 5 and 7 in full, the `verify` job, `bump.json`, Klondike's `restoreState`, the settings-key dump and the `openGameNamed` call sites | 3 | 3 | 1 | n/a | **Seven verified, seven fixed.** **All three lanes independently found the same defect**: § 2 stated the `1.0` bar as a two-clause sentence AND as a five-item table, and three of the items served neither clause — so with two items shipped one maintainer cuts `1.0.0` and another refuses. The table is now the condition and the prose its rationale. The gate also **added a sixth item**: the owner's five did not cover the loss path § 1 itself describes, a save silently refused by a changed build, so GHUB-0075 joined them — a change to the owner's chosen bar, surfaced rather than slid in. **The sharpest Q1 was my own false measurement**: § 3 recorded *"Measured … `cmake_version=0.5.0`"* when the tree says `0.4.0`, so the run demonstrated a rejection without isolating the suffix as its cause. Replaced with a derivation from the `sed` pattern, executed: a suffixed source line still yields the bare triple. **A lane found the `verify` job uses `$tag` twice** — the second is the `CHANGELOG` heading grep — where the document quoted one, so a conformer would have patched a third of the problem. **Two restatements of global rules** (§ 5's spelling and placement, § 7's clock clause) cut to citations, which a case-2 override is required to do. `minesweeper/level` fell outside every settings family named. Two lane open questions resolved clean: `saved/<game>` is real (built by `saveKey()`, so absent from a literal-key grep — a packet artefact), and the security carve-out and changelog tests do live in global § 2. |
| 2 | 2026-08-20 | 3, cold — identical brief, packet rebuilt from disk and widened with global §§ 2 and 8 and the dynamic-key builders | 1 | 3 | 2 | n/a | **Six verified, six fixed.** **The best finding was one lane's and it was a hole rather than a wording slip**: § 1 keyed the save rule on the STAMP moving, when the dangerous case is the opposite — change what `saveState()` writes and leave the stamp at `1`, and an old blob passes the version gate. Reworded to fire on the format changing whether or not the stamp moves. **Two lanes found the surfaces list closed** — *"these four are what a player can rely on"* — against global § 3's *"a surface nobody wrote down is still a surface"*. **A lane found the integrator half of global § 2's definition dropped twice**, which matters here because GHUB-0044 and GHUB-0045 are distribution packaging: an install target or the configure-time prefix contract is a surface no player sees. **Another found § 2 stated a floor and no trigger**, so a satisfied condition could sit at `0.9.x` indefinitely. **A lane found the CLI surface missing entirely** and its open question turned out to matter more than the finding: `--version` prints `Games <version>` and `release.yml` asserts that prefix on both artifacts, so it was already a contract nobody had written down. Its second open question — does the publish step key off a suffix — found that `gh release create` carries **no `--prerelease` at all**, making the release-candidate work three changes rather than two. **Mid-loop the owner decided this project cuts no candidates**, so § 3 became three lines and GHUB-0076 was parked as considered with the measurement intact rather than deleted. |
| 3 | 2026-08-20 | 3, cold — identical brief, packet rebuilt from disk and widened with the publish step, both smoke legs and `main.cpp`'s argv loop | 2 | 1 | 2 | n/a | **Five verified, five fixed. Cap reached (3 for a standard), and it is a VIOLENT cap: all five landed on text THIS RUN wrote**, checked against the earlier loops' fixes rather than recalled. **Do not re-run this gate on this document.** **Size is not the cause** — at 139 lines two cold reads reached all of it easily. The cause is that the document was still being AUTHORED during the gate: loop 1 rewrote § 2's bar, loop 2 rewrote § 1's save rule and added a surface, and the owner's no-candidates decision rewrote § 3 between loops 2 and 3. Each loop therefore read largely new text, which is a run reviewing a draft rather than a draft converging. **Two lanes independently found the same Q1, and it was a claim I had made one loop earlier**: *"dropping `-v` fails the release"* is false — both smoke legs invoke `--version` only, so deleting the alias leaves every check green. It now has its own **nothing** row. **A lane found a sequence that cannot happen**: § 3 said a candidate would be *"rejected twice and then published as the latest release"*, when `verify` exits before the publish step runs. Split into today's behaviour and the conditional. **A lane found the `1.0` trigger ambiguous** — *"the first release after the last item has shipped"* reads as either the completing release or the one following it, and one reading forces an extra release carrying nothing; now worded as an identity. **And a lane found the packaging surface I added in loop 2 had no checks row**, while every other § 1 rule had one. **Two open questions settled by measurement rather than argument**: all ten saving games stamp and refuse a mismatch, not just Klondike; and Klondike's downstream guards (`readPile`, `matchesPack`) do catch a change that shifts the stream — so loop 2's *"no check downstream is looking for"* was too strong, and the honest claim is that a field appended at the END reads back clean, which is luck rather than design. **Route: ship.** |
| 4 | 2026-09-08 | 3, cold — genre pinned `standard`; packet carried global §§ 2, 4 and 9, `bump.json`, the `verify` job, the publish step, both smoke legs, `CLAUDE.md` § Releasing, and the per-item changelog extraction for all six § 2 rows | 3 | 0 | 0 | n/a | **Three verified, three fixed; none dismissed. Loop 1 of this run — armed by the § 2 rewrite recording that `1.0.0` landed and global § 4's `0.x` shift lapsed.** **All three lanes independently found the same first two, and none of the three landed on text this run wrote** — § 4 said *ten* save-format versions against § 1's *twelve* in the same document (twelve `saveState()` overrides measured in `src/`), and § 1 said *four* games with no pack where there are five, none named. **The third came from two lanes and is the one with a cost downstream**: § 3 said `verify` holds two checks, and it holds three — the third an `awk` failing on empty notes, keyed to the exact `## [$tag]` heading, so an implementer of GHUB-0076 budgeting *relax two, add `--prerelease`* is stopped by a fourth. Lane A resolved two of lane C's open questions rather than filing them: `-v` exists in `main.cpp`'s argv loop, and `gh release create` carries no `--prerelease`. **Gate span (1c): the whole run's findings fell OUTSIDE the change that armed it — 0 of 3 — so this loop was audit rather than gate, and the audit is what paid.** One orchestrator-found precision fix to this run's own text (*the last two items in the table* read as the last two rows). One `out_of_scope`: `.claude/bump.json`'s `$note_save_versions` carried the same *ten*, corrected there and noted on GHUB-0078 rather than by rewriting its shipped body. |
| 5 | 2026-09-08 | 3, cold — identical brief; scrubbed copy and subject line count rebuilt, but the rest of the packet was NOT rebuilt from disk, which cost a false finding (below) | 1 | 1 | 0 | n/a | **Two verified, two fixed; one dismissed as unverified. Loop 2 of this run. Neither verified finding landed on text this run wrote — both were in the `What checks this` table, which loop 1 read past.** **All three lanes found the same Q2**: row 1 said a `saveState()` change is caught by **nothing** and is *Tracked by GHUB-0075*, while § 2's table three sections up records GHUB-0075 as cleared in 0.6.0. The code settles it against the row — `savesFromOlderBuildsStillLoad` restores twelve committed corpus blobs. One lane named the consequence the others did not: a conformer meeting that red check believes it unrelated and regenerates the corpus with `--write-saves`, which `CLAUDE.md` § Testing notes forbids by name. **The Q1 came from one lane and is the better catch for being narrow**: row 3 claimed *every* caller of `openGameNamed` iterates `gameNames()` and none checks its effect — false on both halves; eleven call sites pass a literal, and `anInterruptedDragPutsTheRunBack<V>` fails on a null `findChild`, so renaming any of seven games already reddens the suite. **The dismissal is the orchestrator's defect, not a lane's**: two lanes filed `bump.json` still reading *ten*, which loop 1 had corrected — they were handed a stale packet window because the rebuild re-measured the subject's length and nothing else. Phase 2 says rebuild the WHOLE packet from disk; it was not, and two lanes reasoned correctly from bad input. Packet rebuilt wholesale before loop 3. |
| 6 | 2026-09-08 | **1, cold — not 3.** Packet rebuilt WHOLESALE from disk this time, carrying the current `bump.json`, global §§ 2, 3, 4 and 7, the `verify` job, the publish command, the save-corpus guard and Klondike's version check | 1 | 0 | 0 | n/a | **One verified, one fixed. CAP REACHED (3 for a standard); the deferred tail is empty and the run ships.** **Loop ran at ONE lane, not three, because the user was closing the session** — a deliberate reduction, so this loop bought one roll of a nondeterministic read rather than three, and its clean areas are correspondingly weaker evidence than loops 4 and 5's. **The finding is the run's sharpest**: § 1 said *each saving game stamps a `quint32` and refuses a mismatch*, and eleven of the twelve do — `version != 1` — while **Canasta accepts any version up to its derived `kBlobVersion` and defaults the tail an older blob predates**. So the one game with the most elaborate save already ships the migration route, and a conformer reading the sentence as universal would cut a MAJOR for a change under which nothing a player had stops working. **CALM cap: 0 of 1 landed on text this run wrote**, and across all three loops 0 of 6 did — every finding was pre-existing, so this gate was an audit throughout and the § 2 rewrite that armed it drew none. Gate span (1c): 0 of 6 inside it. |
