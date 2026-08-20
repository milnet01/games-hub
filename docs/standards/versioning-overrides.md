# Versioning Overrides — Games Hub

Answers the two questions `~/.claude/standards/versioning.md` deliberately
refuses to answer for a project — **§ 3, what a breaking change can break
here**, and **§ 4, what would make this `1.0`** — and records the local facts a
conformer needs under its §§ 5 and 7. § 3's answer is required of every
project; § 4's only while the project is `0.x`.

**Everything else is the global standard's and is deliberately not restated** —
which level to bump, the `0.x` shift, the security carve-out, the three ordered
changelog tests, the `-rc.N` spelling. A rule stated twice is two rules that
will disagree. Read that file first; this one only adds what is local.

## 1. Breaking surfaces

Global § 3 asks each project to name what a user relies on, so *"has something
that used to work stopped working?"* has a referent. **These are the surfaces
that recur here, not a closed list** — global § 3's last bullet governs
anything they miss.

**A saved game.** Ten of the fourteen games save. **Changing what a game's
`saveState()` writes is breaking — whether or not the stamped version moves**,
and leaving the stamp behind makes it worse rather than smaller.

Each saving game stamps a `quint32` and refuses a mismatch:
`KlondikeView::restoreState` returns `false` unless it reads `1`. **A refused
save is silent** — `hubwindow.cpp` keeps the fresh deal it already dealt, so
the app runs, nothing looks wrong, the player is told nothing, and closing the
game writes the fresh state over the old blob. **Passing the gate wrongly is worse, and
whether anything catches it is luck rather than design.** The guards past the
stamp are about the cards, not the format: Klondike's `readPile` failing, and
`cardcodec::matchesPack` refusing a pack with a card missing or doubled. Those
catch a change that shifts the stream. **A field appended at the end shifts
nothing** — the old layout reads back clean, the extra bytes go unread, and the
game restores a state that is silently missing whatever the new field carried.
The four games with no pack lean on their own core's `restore()` in the same
incidental way.

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

## 2. What would make this `1.0`

**The release that ships the last of the six items below IS `1.0.0`**, and
MAJOR stays 0 until then. Both halves are stated, and the first is worded as an
identity rather than as *"the first release after"*: a floor alone lets a
satisfied condition sit unacted on at `0.9.x` forever — global § 8's inert
leading zero by another route — while *"after"* reads two ways and one of them
forces an extra release, possibly carrying nothing, just to reach 1.0. Owner's call,
2026-08-20, on a list of five; GHUB-0075 was added by this document's own cold
gate, which found that § 1's silent-loss path had no guard. **The table is the
condition** — checkable by someone else, which is what global § 4 asks for, and
a prose bar alongside it would be a second condition that disagrees.

They are these six because they are the two ways this project can currently
let a stranger down: it can lose their saved game, and it can hand them a
binary they have no way to trust. GHUB-0067 and GHUB-0075 are the first;
GHUB-0054, GHUB-0050, GHUB-0031 and GHUB-0053 are the second — a build you can
verify, made from inputs that were pinned, on a runtime that still exists, by a
compiler that was allowed to object. **New games do not gate it** — a
collection can always grow, and waiting for a fifteenth is how a leading zero
goes inert.

| Item | What it fixes |
|------|---------------|
| GHUB-0067 | a save survives a clean exit and nothing else, and two copies of the app overwrite each other |
| GHUB-0075 | nothing checks that a save written by an older build still loads, so § 1's silent loss has no guard |
| GHUB-0054 | a downloaded release cannot be checked against what the workflow built |
| GHUB-0050 | the release workflow downloads two unpinned binaries and runs them |
| GHUB-0031 | the Windows build rides an action GitHub is deprecating the runtime under |
| GHUB-0053 | the build asks the compiler for no warnings and no hardening |

## 3. This project does not cut release candidates

**No candidate builds, and therefore no `-rc.N` tags.** Owner's call,
2026-08-20: there are no testers to hand one to, and a candidate nobody
installs is a release cycle bought for nothing.

Global § 5 still governs the suffix if that ever changes. **It would be work
rather than a flag**, and the shape of it is recorded in GHUB-0076 so nobody
has to measure it twice. **Today a candidate tag never gets that far**: it is
rejected at both of `verify`'s checks — the comparison against `CMakeLists.txt`
and the `CHANGELOG.md` heading grep — and `verify` exits before the publish
step runs. **If those two were relaxed, it would then publish as the latest
release**, because `gh release create` carries no `--prerelease`. That third
change is the one easiest to miss, precisely because the first two hide it.

## 4. Version lines that are not the app's version

Global § 7's case, here: **ten save-format versions, one per saving game.**
There is no single "save version" for the app and nothing should invent one.

They are absent from `.claude/bump.json` on purpose, and `$note_save_versions`
records why, as global § 7 requires.

## What checks this

| Rule | What catches a breach |
|------|----------------------|
| § 1 — a change to what `saveState()` writes is breaking | **nothing** — no test loads a save written by an older build, so neither a moved stamp nor an unmoved one is caught. Tracked by GHUB-0075 |
| § 1 — a settings key is not renamed | **nothing** — a renamed key reads as absent and falls back to its default, which is indistinguishable from a first run |
| § 1 — `--game` names stay stable | **nothing.** Every caller of `openGameNamed` in `tests/uitest.cpp` iterates `gameNames()` and none checks the return value, so the tests follow a rename rather than catching one. Measured 2026-08-20 |
| § 1 — the `--version` output keeps its prefix | `release.yml`'s smoke tests — `grep -q '^Games '` on Linux, `Select-String -Pattern '^Games '` on Windows. The prefix is guarded; the version it prints is not |
| § 1 — the `-v` alias keeps existing | **nothing** — both smoke legs invoke `--version` only, so deleting `-v` leaves every check green |
| § 1 — keyboard shortcuts stay stable | **nothing** — no test presses a key it does not already know about |
| § 1 — install targets, option names and the configure-time `CMAKE_INSTALL_PREFIX` contract | **nothing** — nothing configures or installs the project with non-default options |
| § 2 — the `1.0` condition | **nothing** — tracked by the six items in its table |
| § 3 — no candidate is cut | `release.yml` rejects a suffixed tag at both its `verify` checks — by accident rather than by intent, but the effect is the rule's |
| § 4 — save versions stay out of the recipe | `.claude/bump.json` lists only `CMakeLists.txt` and `README.md`, so a release cannot walk them; `post_check` verifies those two against `CHANGELOG.md` |

## Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 1 | 2026-08-20 | 3, cold — genre pinned `standard`; packet carried global §§ 3, 4, 5 and 7 in full, the `verify` job, `bump.json`, Klondike's `restoreState`, the settings-key dump and the `openGameNamed` call sites | 3 | 3 | 1 | n/a | **Seven verified, seven fixed.** **All three lanes independently found the same defect**: § 2 stated the `1.0` bar as a two-clause sentence AND as a five-item table, and three of the items served neither clause — so with two items shipped one maintainer cuts `1.0.0` and another refuses. The table is now the condition and the prose its rationale. The gate also **added a sixth item**: the owner's five did not cover the loss path § 1 itself describes, a save silently refused by a changed build, so GHUB-0075 joined them — a change to the owner's chosen bar, surfaced rather than slid in. **The sharpest Q1 was my own false measurement**: § 3 recorded *"Measured … `cmake_version=0.5.0`"* when the tree says `0.4.0`, so the run demonstrated a rejection without isolating the suffix as its cause. Replaced with a derivation from the `sed` pattern, executed: a suffixed source line still yields the bare triple. **A lane found the `verify` job uses `$tag` twice** — the second is the `CHANGELOG` heading grep — where the document quoted one, so a conformer would have patched a third of the problem. **Two restatements of global rules** (§ 5's spelling and placement, § 7's clock clause) cut to citations, which a case-2 override is required to do. `minesweeper/level` fell outside every settings family named. Two lane open questions resolved clean: `saved/<game>` is real (built by `saveKey()`, so absent from a literal-key grep — a packet artefact), and the security carve-out and changelog tests do live in global § 2. |
| 2 | 2026-08-20 | 3, cold — identical brief, packet rebuilt from disk and widened with global §§ 2 and 8 and the dynamic-key builders | 1 | 3 | 2 | n/a | **Six verified, six fixed.** **The best finding was one lane's and it was a hole rather than a wording slip**: § 1 keyed the save rule on the STAMP moving, when the dangerous case is the opposite — change what `saveState()` writes and leave the stamp at `1`, and an old blob passes the version gate. Reworded to fire on the format changing whether or not the stamp moves. **Two lanes found the surfaces list closed** — *"these four are what a player can rely on"* — against global § 3's *"a surface nobody wrote down is still a surface"*. **A lane found the integrator half of global § 2's definition dropped twice**, which matters here because GHUB-0044 and GHUB-0045 are distribution packaging: an install target or the configure-time prefix contract is a surface no player sees. **Another found § 2 stated a floor and no trigger**, so a satisfied condition could sit at `0.9.x` indefinitely. **A lane found the CLI surface missing entirely** and its open question turned out to matter more than the finding: `--version` prints `Games <version>` and `release.yml` asserts that prefix on both artifacts, so it was already a contract nobody had written down. Its second open question — does the publish step key off a suffix — found that `gh release create` carries **no `--prerelease` at all**, making the release-candidate work three changes rather than two. **Mid-loop the owner decided this project cuts no candidates**, so § 3 became three lines and GHUB-0076 was parked as considered with the measurement intact rather than deleted. |
| 3 | 2026-08-20 | 3, cold — identical brief, packet rebuilt from disk and widened with the publish step, both smoke legs and `main.cpp`'s argv loop | 2 | 1 | 2 | n/a | **Five verified, five fixed. Cap reached (3 for a standard), and it is a VIOLENT cap: all five landed on text THIS RUN wrote**, checked against the earlier loops' fixes rather than recalled. **Do not re-run this gate on this document.** **Size is not the cause** — at 139 lines two cold reads reached all of it easily. The cause is that the document was still being AUTHORED during the gate: loop 1 rewrote § 2's bar, loop 2 rewrote § 1's save rule and added a surface, and the owner's no-candidates decision rewrote § 3 between loops 2 and 3. Each loop therefore read largely new text, which is a run reviewing a draft rather than a draft converging. **Two lanes independently found the same Q1, and it was a claim I had made one loop earlier**: *"dropping `-v` fails the release"* is false — both smoke legs invoke `--version` only, so deleting the alias leaves every check green. It now has its own **nothing** row. **A lane found a sequence that cannot happen**: § 3 said a candidate would be *"rejected twice and then published as the latest release"*, when `verify` exits before the publish step runs. Split into today's behaviour and the conditional. **A lane found the `1.0` trigger ambiguous** — *"the first release after the last item has shipped"* reads as either the completing release or the one following it, and one reading forces an extra release carrying nothing; now worded as an identity. **And a lane found the packaging surface I added in loop 2 had no checks row**, while every other § 1 rule had one. **Two open questions settled by measurement rather than argument**: all ten saving games stamp and refuse a mismatch, not just Klondike; and Klondike's downstream guards (`readPile`, `matchesPack`) do catch a change that shifts the stream — so loop 2's *"no check downstream is looking for"* was too strong, and the honest claim is that a field appended at the END reads back clean, which is luck rather than design. **Route: ship.** |
