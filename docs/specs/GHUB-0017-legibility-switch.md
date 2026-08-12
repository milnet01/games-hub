# GHUB-0017 — A hub-owned legibility switch every game reads

**Status:** spec draft (2026-08-12).
**Kind:** accessibility.
**Source:** ROADMAP GHUB-0017 (owner request; shape agreed 2026-08-11, start
agreed 2026-08-12).

Layman: one switch in the hub makes cards, pieces and numbers big enough and
sharp enough to read across all fourteen games, instead of Canasta being the
only game that was ever adjusted.

## 1. Goal

After this ships the hub owns a single persisted preference — **Large cards /
high contrast** — that every game can read and is told about when it changes,
and `CardArt`'s face-drawing threshold is a named constant a game can compute
against instead of a literal buried in an `if`. 2048's unreadable tiles are
fixed on the way past.

**What this change does NOT do is make any game look different** (§4.6). The
guarantee that no game draws a card too small to show its pips arrives one game
at a time afterwards, and each of those passes is judged by eye (§3.2, §9).
Claiming it here would be claiming the switch does the work the passes do.

Today Canasta is the only game that has been adjusted for a partially sighted
player, that adjustment is hardcoded rather than chosen, and the six games that
draw cards can all render them below the width at which `CardArt` gives up on
the face entirely.

## 2. Problem

The owner is partially sighted and **reads a card by its pip pattern, not by
the corner index**. That is the constraint this whole item exists to serve, and
it is the constraint the codebase currently violates in the most direct way
possible.

**2.1 `CardArt` stops drawing the face below 46 pixels, and all six card games
can go below it.** `src/cards/cardart.cpp::paintFace` returns early — leaving
only the corner index — under a bare literal:

```
$ grep -n "r.width() < 46" src/cards/cardart.cpp
216:    if (r.width() < 46)
```

The card-width floors of the four solitaires are all under that number. **This
grep finds four of the six, and the two it misses are the point** — it matches
only the `std::max(3N.0, …)` form, so neither `std::clamp` game appears:

```
$ grep -rn "return std::max(3[0-9]\.0, std::min" src/*/[a-z]*view.cpp
src/freecell/freecellview.cpp:199:    return std::max(32.0, std::min(byWidth, byHeight));
src/pyramid/pyramidview.cpp:206:    return std::max(30.0, std::min(byWidth * 1.6, byHeight));
src/klondike/klondikeview.cpp:241:    return std::max(34.0, std::min(byWidth, byHeight));
src/spider/spiderview.cpp:220:    return std::max(30.0, std::min(byWidth, byHeight));
```

So `KlondikeView::cardWidth()` (34), `FreeCellView::cardWidth()` (32),
`SpiderView::cardWidth()` (30) and `PyramidView::cardWidth()` (30) each have a
legal range in which every card on the table is a blank rectangle with a small
letter in the corner — precisely the state the owner cannot read. Spider is the
likeliest to reach it, laying ten columns across the width.

**Hearts and Canasta are the two that clamp upward, and both still floor below
46.** `HeartsView::cardWidth()` is
`std::clamp(std::min(byWidth, byHeight), 40.0, 92.0)` — floor 40.
`CanastaView::cardWidth()` is `std::clamp(…, 34.0, 88.0)` — floor 34.

**Canasta is worse than its floor suggests, and its own comment is wrong about
it.** `canastaview.cpp` says melds are drawn "not so small that the shared card
art gives up on the face". They are: `kMeldScale = 0.74`, so a meld clears 46
pixels only when `cardWidth()` is at least 62.2 — the top half of a `[34, 88]`
clamp. At the floor a meld card is 25.2 pixels. Opponent hands at 0.8 clear it
only above 57.5. `CLAUDE.md`'s trap entry states the true position ("a stack of
slivers rather than cards, and has to name them some other way"), which is why
melds carry a "K ×5" badge at all — a badge that would be pointless if the
comment were right.

**So a floor on `cardWidth()` is not the contract.** A game must hold
`kFaceMinWidth` at **the smallest scale it actually draws a card at**, which for
Canasta is 0.74 and not 1.0. Any per-game pass that raises `cardWidth()` alone
leaves Canasta's melds exactly as unreadable as before.

**2.2 The threshold is an unnamed literal, so no game can ask about it.**
`46` appears once, in an `if`, not as a constant. A game has no way to compute
"is my layout about to produce faceless cards?", which is why every one of the
floors above was chosen without reference to it.

**2.3 Contrast is mostly already fine — the problem is size, and one real
bug.** This was measured rather than assumed. WCAG 2.2 asks 4.5:1 for normal
text and 3:1 for large text and for graphical objects
(`Source: https://www.w3.org/WAI/WCAG21/Understanding/non-text-contrast.html`).

**Provenance, stated because it is a known trap:** these ratios were produced
by a *prototype* script during drafting, not by `scripts/legibility-check.py`
(§4.5), which does not exist yet. The colour pairs were transcribed from the
source by hand, so a transcription error would show up here as a real-looking
number. **The first job of §4.5's script is to reproduce this table**; where it
disagrees, the table is wrong and gets corrected, and that correction is a
fold-back row in §12 rather than a silent edit.

**The `Required` column is the contract, not a comment** — §4.5's script reads
this table's thresholds and exits non-zero only on a pair marked required.
Everything at 3.0 is a graphical object or large text; nothing here is required
at 4.5, because no pair in this table is small body text.

| Pair | Ratio | Required | Verdict |
|------|-------|----------|---------|
| `CardArt` `kRed` on `kFaceBottom` | 5.01 | 3.0 | passes |
| `CardArt` `kBlack` on `kFaceBottom` | 14.11 | 3.0 | passes |
| Sudoku pencil mark `#6d6a5e` on `kPaper` | 4.88 | 3.0 | passes |
| Sudoku `kOwnInk` on `kPaper` | 4.76 | 3.0 | passes |
| Minesweeper `kNumberColours[1]` on `kDug` | 4.35 | 3.0 | passes |
| **2048 ink on tile 8** | **1.72** | **3.0** | **fails** |
| **2048 ink on tile 512** | **1.50** | **3.0** | **fails** |
| **2048 ink on tile 2048** | **1.58** | **3.0** | **fails** |
| 2048 ink on the `default:` tile (v > 2048) | 10.57 | 3.0 | passes |

**Minesweeper's 4.35 is the row to notice**: it is required at 3.0 and passes,
so the script stays green on unchanged code. Requiring it at 4.5 would make a
new script fail on a game nobody has complained about, which is how a check
gets disabled.

Two conclusions, and they point in different directions. The card and Sudoku
inks **pass** — so Sudoku's pencil marks are hard to read because of their
size, not their colour (`markFont.setPointSizeF(cell * 0.20)` against
`cell * 0.52` for a real digit, drawn into a cell third), and the same is true
of the cards. But `inkFor()` returns near-white for every tile above 4, and
**fails 3:1 on every enumerated tile from 8 to 2048** — the mid-orange and
mid-yellow ones. It passes on the `default:` arm, which is near-black and only
reached above 2048. So the failure is the whole range a player actually plays
through, and it stops exactly where the tiles stop being listed. That is
unreadable for anyone and is a defect independent of any preference.

**2.4 There is no mechanism to hang a preference on.** `src/gameview.h`
declares `gameActions()`, `activate()`, `deactivate()`, `saveState()`,
`restoreState()` and `statusChanged` — and no hook for a settings change. The
hub persists only `window/geometry/*` and `saved/*`, both per game;
`HubWindow::buildChrome()` creates `m_soundAction` as an app-wide *control*
that is `setChecked(true)` on every launch and never stored. So there is no
app-wide persisted setting in the project at all, and nothing to broadcast one
with except the `Sound::instance()` singleton, which games pull from rather
than being pushed to. `CanastaView` reading `canasta/sortHand` in its
constructor is the only precedent for a game picking a preference up.

## 3. Scope decisions (agreed with the user)

1. **One switch, not two.** "Large cards / high contrast" is a single control,
   approved by the owner on 2026-08-11. Two independent switches would be more
   flexible and were not chosen; the owner wants one thing to turn on.
2. **Start now, one game at a time.** Owner, 2026-08-12. This spec covers only
   the shared mechanism; the per-game passes follow it individually, each shown
   to the owner before the next is started (§9).
3. **Default off.** Preference call, made here and not by the owner: the app is
   published for strangers to download (GHUB-0025), so the shipped default is
   the current appearance, and the owner turns it on once — it persists, so he
   is asked once ever. The alternative, defaulting on, is in §8.
4. **2048's ink is fixed unconditionally, not behind the switch.** Preference
   call, made here. §2.3 measures it failing 3:1 on every enumerated tile from
   8 to 2048; a tile nobody can read is a bug, and putting the fix behind a
   preference would leave it broken for everyone who never finds the switch.
   The `default:` tile above 2048 already passes and is not touched.
5. **Card size is the lever, not card colour.** Follows from §2.3's
   measurements rather than from preference: the inks already pass, so "high
   contrast" buys little on the cards and "large" buys everything.

## 4. Design

### 4.1 `src/legibility.h` / `.cpp` — the setting

A singleton mirroring `Sound`, which is the project's only existing broadcast
precedent, plus the signal `Sound` lacks.

```cpp
// src/legibility.h
class Legibility : public QObject
{
    Q_OBJECT
public:
    static Legibility& instance();

    // True when the player has asked for large, high-contrast play.
    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);          // stores and emits; a no-op if unchanged

Q_SIGNALS:
    void changed(bool enabled);

private:
    Legibility();
    bool m_enabled = false;
};
```

`Legibility()` reads `QSettings().value("display/legibility", false).toBool()`.
`setEnabled()` writes the same key and emits `changed`. The key sits at the top
level beside `window/` and `saved/` rather than under a game's group, because
it is the first genuinely app-wide setting (§2.4).

`Legibility` goes in `GAME_VIEW_SOURCES`, not `GAME_CORE_SOURCES`: it is
QtCore-only today, but the split in `CMakeLists.txt` exists so `gameshub_selftest`
links no widgets, and nothing in a rules core should be reading a display
preference.

### 4.2 `GameView` gains one hook

```cpp
// src/gameview.h — added to the existing virtuals
    // Called when the hub's legibility switch changes, and never at
    // construction: a game reads Legibility::instance().enabled() itself when
    // it builds. The default repaints, which is enough for a game that reads
    // the setting inside paintEvent; a game that caches geometry overrides it.
    virtual void applyLegibility(bool enabled) { Q_UNUSED(enabled); updateGeometry(); update(); }
```

`GameView` currently inherits its constructors (`using QWidget::QWidget;`).
That is replaced with an explicit `explicit GameView(QWidget* parent = nullptr);`
which connects `Legibility::instance().changed` to `applyLegibility`. Every
existing game already delegates as `GameView(parent)`, so no game's constructor
changes.

**A test needs a way to see the call, and nothing in the design provides one**
— the default body only repaints, and §4.6 guarantees no game's appearance
changes in this change, so no rendered pixel differs between a notified view
and an unnotified one. So `tests/uitest.cpp` declares the observer explicitly:

```cpp
// tests/uitest.cpp — the only thing that can observe INV-2.
class LegibilityProbe : public GameView
{
public:
    using GameView::GameView;
    void applyLegibility(bool enabled) override { ++calls; last = enabled; }
    int  calls = 0;
    bool last  = false;
};
```

It lives in the test, not in `GameView`: a counter on the base class would be
production surface existing only to be asserted on. **`applyLegibility` must
therefore be `public` or `protected`, not private** — a private virtual cannot
be overridden from a subclass outside the class.

**The connection is made in the base constructor, so every constructed game is
connected whether or not it is on screen.** Games are built lazily by
`HubWindow::openGame`, so at any moment some are constructed and some are not;
connecting only the visible one would leave a game that was opened earlier
still laid out for the old setting when the player returns to it. A game
constructed *after* the change reads the current value at build time and needs
no notification.

### 4.3 `HubWindow` gains the control

`HubWindow::buildChrome()` creates `m_legibilityAction` immediately after
`m_soundAction`, checkable, `setChecked(Legibility::instance().enabled())`,
toggling `Legibility::instance().setEnabled()`.

**The label names the CURRENT state, never the action** — matching
`m_soundAction`, which reads `"🔊 Sound"` while sound is on and `"🔇 Muted"`
while it is off. So: **checked → `"🔍 Large"`, unchecked → `"🔍 Normal"`.**
The toolbar is `Qt::ToolButtonTextOnly`, so this text is the entire affordance,
and the opposite convention — a button reading "Large" meaning *click for
large* — is equally common and would ship the control inverted.
It is added after the expanding spacer, so it sits at the far right beside the
sound switch and is never displaced by `HubWindow::setGameActions()`, which
inserts a game's own actions *before* `m_soundSeparator`.

The action is app-wide, so it is visible on the tile grid as well as inside a
game — unlike every other toolbar entry, which belongs to one game.

### 4.4 `CardArt` exports the threshold

```cpp
// src/cards/cardart.h
namespace CardArt {
    // Below this width paintFace draws only the corner index — the pips and
    // court letters are dropped, because they are unreadable smaller. A game
    // that lays cards out must not go below it while Legibility is on.
    inline constexpr double kFaceMinWidth = 46.0;
}
```

`paintFace`'s `if (r.width() < 46)` becomes `if (r.width() < kFaceMinWidth)`.
This is the number's only definition afterwards (INV-4).

**A card game in large mode must satisfy three things together, and only the
first is obvious.**

1. **Floor `cardWidth()` at `kFaceMinWidth` — divided by the smallest scale the
   game draws a card at, not at 1.0.** Canasta draws melds at `kMeldScale =
   0.74` and opponent hands at 0.8, so its floor must be `46 / 0.74` = 62.2 for
   a meld to show a face at all (§2.1). A game drawing everything at full size
   needs plain `kFaceMinWidth`.
2. **Raise `minimumSize` to match.** A wider card means a wider row — the row
   cost is fixed — so raising the floor alone guarantees the layout overflows
   the widget once the window is small enough. Qt then refuses to shrink past
   the new minimum, and a stored `window/geometry/<page>` smaller than it is
   clamped on restore rather than honoured.
3. **Lower both again when the switch goes off**, or the window keeps a
   minimum it no longer needs.

The per-game arithmetic differs (Klondike's row cost is `7 + 6·0.14`, Spider's
is ten columns plus gaps, Pyramid's is the `kRows * 0.62 + 0.4` expression) and
belongs to each game's own pass, not here. What this spec fixes is the
obligation: **floor, smallest scale and minimum size move together, or not at
all.** §9 records it as the contract each pass inherits; there is deliberately
no invariant for it here, because this change adapts no game (§4.6).

### 4.5 `scripts/legibility-check.py` — the numbers become an output

The table in §2.3 is generated, not transcribed. The script holds the
foreground/background pairs **and each pair's required threshold, taken from
§2.3's `Required` column**, computes the WCAG 2.2 relative-luminance ratio, and
exits non-zero when a pair falls below its own threshold. `--thresholds` runs
INV-4's grep instead, so one script owns both mechanical legibility checks.

`tests/uitest.cpp` gets the 2048 check directly against the game's own
`tileColour()` and `inkFor()` — **free functions in an anonymous namespace in
`src/twenty48/twenty48view.cpp`, which INV-7 requires be declared in
`src/twenty48/twenty48view.h` and moved out of it.** Until that happens the
test cannot reference them at all. Locking it in a compiled test rather than
only in the script matters because the script is run by hand and `ctest` is
not.

### 4.6 What the switch does *not* do here

It changes no game's appearance in this change. `Legibility` ships with the
switch, the hook, the exported threshold and the 2048 fix; the fourteen games
read it one at a time afterwards (§9). That is deliberate — the mechanism is
the part every game binds to, and it is worth getting a cold read on before
fourteen games are written against it.

**This is why §5 carries no invariant about card size.** An invariant asserting
that cards clear `kFaceMinWidth` would be red on all six card views the day it
landed, because no view is adapted here; one asserting the switch is reversible
would be vacuously green, because nothing changes to be reversed. Both belong
to the first game's pass, and §9 records them there.

## 5. Invariants

- **INV-1** — The switch survives a restart: a value written by `setEnabled()`
  is read back by a freshly constructed `QSettings`, on both platforms.
  *Test:* `tests/uitest.cpp`, block `legibilityPersists` — set, destroy, construct a
  new `QSettings`, read `display/legibility`.
  *Breaks when:* the value is held in the singleton and never written; or
  written under a per-game group so a second game reads a different setting;
  or checked with `QFile::exists(QSettings().fileName())`, which is false on
  Windows however well saving works (`CLAUDE.md`, Traps).

- **INV-2** — Every *constructed* game is notified, not only the visible one.
  *Test:* `tests/uitest.cpp`, block `legibilityReachesBackgroundGames` — build
  two `LegibilityProbe`s (§4.2), add both to the hub's stack, show one, toggle
  the switch, and assert **both** probes' counters read 1.
  *Breaks when:* the hub connects `currentView()` instead of connecting in the
  `GameView` base constructor — the bug is invisible until the player reopens
  a game they had already visited.

- **INV-3** — *withdrawn — moved to the per-game pass contract (§9).* It
  asserted that a card game's layout fits inside its own minimum
  size with the switch on. That is a real obligation and it is stated in §4.4,
  but it cannot be an invariant **of this change**: §4.6 ships no game
  adaptation, so the test would be red on every one of the six views the day it
  landed. It is also unwritable as it stood — `cardWidth()` is `private` on all
  six views and none exposes its laid-out rects, so a test in `uitest.cpp`
  cannot call either. Whichever game's pass lands first owns making its own
  layout observable, and carries this invariant with its own number.

- **INV-4** — The 46-pixel threshold has exactly one definition, in
  `cardart.h`; no other source states it as a literal.
  *Test:* `scripts/legibility-check.py --thresholds`, whose grep is
  `grep -rn "46\.0\|< *46[^0-9x]" src --include=*.cpp --include=*.h | grep -v "0x" | grep -v kFaceMinWidth`
  → no output.
  *Breaks when:* a second game hardcodes 46 in its own floor rather than
  including the constant, and the two drift when the threshold is retuned.
  **Seen failing against current code**: the grep returns
  `src/cards/cardart.cpp:216:    if (r.width() < 46)` today, and is empty once
  §4.4 lands. (An earlier draft of this clause counted matches *inside*
  `cardart.cpp` and expected `1` — which is satisfied by the defect and
  violated by the fix. It was caught by running it.)

- **INV-5** — On a machine with no stored value the switch is off, and the
  stored key is `display/legibility`.
  *Test:* `tests/uitest.cpp`, block `legibilityDefaultsOff` — clear the key, construct,
  assert `enabled() == false`.
  *Breaks when:* the key is renamed in a later release, which silently turns
  the switch off for a player who had turned it on — the old key is still in
  their settings file and nothing reads it.

- **INV-6** — *withdrawn — moved to the per-game pass contract (§9).* It
  asserted that turning the switch off restores the previous
  appearance exactly, tested by comparing two `renderOf()` images. Against this
  change it is **vacuously true**: §4.6 ships no game adaptation, so nothing
  can fail to be restored, and a test that cannot fail is not a contract. It
  becomes falsifiable the moment a game's pass changes something, which is
  where it belongs and where it does the work of making the passes safe to do
  one at a time.

- **INV-7** — Every tile value `tileColour()` enumerates, **and its `default:`
  arm**, has ink meeting 3:1 against its own tile colour, with the switch in
  either position.
  *Test:* `tests/uitest.cpp`, block `twenty48InkIsReadable` — for
  `{2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096}`, assert
  `ratio(inkFor(v), tileColour(v)) >= 3.0`. The trailing 4096 is the
  `default:` arm, which a loop over the enumerated cases alone never reaches —
  and it is the one value in the list that passes today.
  *Requires a code change this spec asks for:* `tileColour()` and `inkFor()`
  are free functions in an **anonymous namespace** in
  `src/twenty48/twenty48view.cpp` and appear in no header, so `uitest.cpp`
  cannot call them at all. Declare both in `src/twenty48/twenty48view.h` and
  move them out of the anonymous namespace. Without that the test does not
  compile, and nothing else in this spec would have said so.
  *Breaks when:* the near-white ink is used over a mid-tone tile. **Seen
  failing against current code**: 1.72 at tile 8, 1.50 at tile 512, 1.58 at
  tile 2048 (§2.3).

## 6. Failure modes

- **The window cannot get big enough for the minimum size.** On a small display
  the raised minimum may exceed the screen. Qt will honour the minimum and let
  the window run off-screen. The game must therefore raise its minimum only to
  what `kFaceMinWidth` actually requires, never to a comfortable size — and a
  game whose minimum would exceed a 1024×768 desktop is a finding for that
  game's pass, not something the switch can resolve.
- **A stored geometry smaller than the new minimum.** Qt clamps on restore, so
  the game opens larger than it was left. Accepted: that is the switch doing
  its job. The stored value is not rewritten, so turning the switch off returns
  the window to its old size.
- **A game overrides `applyLegibility` and forgets to call `update()`.** The
  setting changes and nothing repaints until the next event. The default
  implementation does both; an override that does neither is caught by nothing
  here, so this is a `nothing` row in §10. It is the first per-game pass that
  gains a test able to see it, since only then does a repaint change anything.
- **`Legibility::instance()` touched from a rules core.** It links only into
  the view half (§4.1), so this is a link error, not a runtime surprise.
- **The switch is toggled mid-animation.** Canasta has card flights in the air
  with destinations computed from the old geometry. `applyLegibility` must be
  treated by Canasta's pass as a re-layout point, not a repaint; its flights
  index into `Meld::cards` and the hand, so a resize mid-flight can put a card
  in the air and its destination in different places (`CLAUDE.md`, Traps).

## 7. Tests

All seven invariants are locked in `tests/uitest.cpp`, which is where widget
behaviour lives; none belongs in `gameshub_selftest`, which links no widgets
and cannot construct a view. `QColor` is QtGui, so even the contrast arithmetic
in INV-7 belongs here rather than in the rules half.

**`tests/uitest.cpp` has no test-function registry** — it is one `main()` with
inline blocks guarded by a `check(bool, const char*)` helper. The names below
are the labels passed to `check`, not symbols; nothing in this spec asks for a
registry to be built.

| Block | Locks | Notes |
|-------|-------|-------|
| `legibilityDefaultsOff` | INV-5 | clears the key first, so it is not order-dependent |
| `legibilityPersists` | INV-1 | fresh `QSettings`, never `QFile::exists` |
| `legibilityReachesBackgroundGames` | INV-2 | needs the `LegibilityProbe` of §4.2 |
| `twenty48InkIsReadable` | INV-7 | needs the two colour functions exported first |

INV-4 is a grep, run by `scripts/legibility-check.py --thresholds` rather than
by a compiled test. **INV-3 and INV-6 are withdrawn to the per-game passes
(§5, §9) and have no test here** — neither could run against this change: one
would be red on six views that this change does not touch, the other vacuously
green because nothing changes appearance.

Every block must be seen failing against pre-fix code before it is believed.
INV-7 already has been (§2.3). **INV-2 is the one to watch**: assert the probe's
counter, not that a repaint happened, or it passes against a hub that notifies
only the visible view.

## 8. Alternatives considered (and rejected)

- **Use Qt's own high-contrast support.** Qt gained platform high-contrast
  handling in 6.10 (`Source: https://www.qt.io/blog/high-contrast-mode-in-qt-6.10`).
  Releases are built against Qt 6.8.3 (`.github/workflows/ci.yml`, `version:`),
  and `CMakeLists.txt` sets a floor of `find_package(Qt6 6.5 REQUIRED)` — so it
  is unavailable to anything shipped, and it would in any case restyle Qt
  widgets rather than the custom-painted boards where the whole problem lives.
- **Two switches, size and contrast separately.** More precise, and rejected by
  the owner in favour of one control (§3.1). §2.3's measurements make it a
  weaker option than it looked: the contrast half has few honest customers once
  2048 is fixed unconditionally.
- **Default the switch on.** It is right for this machine's owner and wrong for
  the published build, and the setting persists, so defaulting off costs him a
  single click ever (§3.3).
- **Push the setting into each game's constructor and skip the signal.** Games
  are constructed lazily and live for the session, so a game built before the
  toggle would never learn — INV-2 is exactly this bug.
- **Scale the whole window with `QT_SCALE_FACTOR`.** Enlarges everything
  uniformly, including chrome, and does nothing about a card whose *layout* put
  ten columns across the width. It also cannot be toggled at runtime.
- **Lower `CardArt`'s 46-pixel threshold so small cards keep their pips.** This
  inverts the problem: the threshold is honest, and pips below it were measured
  unreadable when it was chosen. Drawing them smaller would produce a card that
  looks complete and cannot be read, which is worse than an obviously bare one.

## 9. Out of scope

- **The fourteen per-game legibility passes** — each is a display change inside
  one game, done individually and shown to the owner before the next starts
  (§3.2). Tracked by GHUB-0017 itself, which stays open after this ships.
  **Fourteen, not thirteen: Canasta needs a pass too.** Its existing adjustment
  is hardcoded rather than switched, §2.1 shows its melds fall below
  `kFaceMinWidth` across most of its clamp range, and §6 requires it to treat
  the toggle as a re-layout point because of its card flights.
- **INV-3 and INV-6**, withdrawn from §5 above. Whichever game's pass lands
  first states them against its own layout, with its own invariant numbers, and
  owns making that layout observable to a test — `cardWidth()` is `private` on
  all six card views today and none exposes its laid-out rects.
- **Hearts having no record of the trick** — `HeartsView::refresh` never names
  the led suit, the led card or who led, and the cards are swept after
  `kTrickPauseMs = 900`. That is a missing feature rather than a size or
  contrast setting, and it needs its own bullet.
- **Draughts announcing no move at all, and Chess announcing only the
  computer's** — same reasoning.
- **Reading-paced timings** — Snake's `m_speedMs` floor of 60 ms, Hearts'
  `kTrickPauseMs` and `kAiDelayMs`, Pinball's fixed 16 ms physics frame. Whether
  the switch should slow them is a real question and belongs to each game's
  pass, where it can be judged by eye.
- **`Theme::paintFelt`'s unconditional centre glow and edge vignette**, which
  reduce contrast at the table edges for every felt game. Worth revisiting once
  a game's pass demonstrates it matters.

## 10. What checks this

| Rule | What catches a breach |
|------|----------------------|
| INV-1 | `tests/uitest.cpp`, block `legibilityPersists` |
| INV-2 | `tests/uitest.cpp`, block `legibilityReachesBackgroundGames` |
| INV-3 | *withdrawn to the per-game passes (§9)* — **nothing** checks it in this change, correctly: there is nothing yet to check |
| INV-4 | `scripts/legibility-check.py --thresholds`; also a compile error if the literal is removed and nothing defines the constant |
| INV-5 | `tests/uitest.cpp`, block `legibilityDefaultsOff` |
| INV-6 | *withdrawn to the per-game passes (§9)* — **nothing**, same reason |
| INV-7 | `tests/uitest.cpp`, block `twenty48InkIsReadable` |
| §2.3's ratio table is reproducible | `scripts/legibility-check.py` — but **only after it is written**; the table's numbers come from a prototype until then (§2.3) |
| §4.3 the action sits after the spacer and survives `setGameActions()` | **nothing** — no test inspects toolbar order; a game's actions displacing it would be visible only by eye |
| §4.3 the label matches the state rather than the action | **nothing** — no test reads the action's text |
| §6 an override that forgets to repaint | **nothing** — and INV-6, which would have caught the restore half, is withdrawn |
| **Whether a game actually reads better with the switch on** | **nothing** — this is the owner's eye, per game, and no assertion substitutes for it. It is the reason §3.2 shows him each game before starting the next |

## 11. Cross-doc impact

- `CLAUDE.md` — the Architecture section's `GameView` contract gains
  `applyLegibility`; the Traps section's `CardArt::paintFace` entry cites
  `kFaceMinWidth` instead of a bare 46. (The same entry's "Canasta's melds are
  0.62" was corrected to `kMeldScale` = 0.74 while writing this spec — the
  number was wrong, and the file already said 0.74 correctly ten lines later.)
- `CHANGELOG.md` — an Added entry for the switch, a Fixed entry for 2048's ink.
- `ROADMAP.md` — GHUB-0017 to 🚧 when implementation starts; it stays open
  through the per-game passes.
- `README.md` — the switch is player-visible and belongs in whatever the README
  says about playing. No screenshot change (GHUB-0028 is parked).

## 12. Cold-eyes loop log

| Loop | Date | Lanes | Q1 | Q2 | Q3 | Q4 | Outcome |
|------|------|-------|----|----|----|----|---------|
| 1 | 2026-08-13 | 3 cold (general-purpose), genre pinned `spec` | 5 | 2 | 2 | 3 | 12 verified, all fixed; 1 dismissed. **Q1:** §2.1's heading said four games can fall below 46 px when all six can — the grep behind it matches only the `std::max` form and misses both `std::clamp` games; "Hearts is the only card game that clamps upward" (Canasta does too); Canasta's melds described as staying above the threshold on the strength of a code comment that is itself false — 0.74 × 34 = 25.2, and clearing 46 needs `cardWidth() ≥ 62.2`; "fails at every tile from 8 upward" (the `default:` arm above 2048 passes at 10.57); `tileColour`/`inkFor` cited as `Twenty48View::` members when they are free functions in an **anonymous namespace**, callable from no test. **Q2:** INV-3 and INV-6 contradicted §4.6's "changes no game's appearance" — one would be red on six views the day it landed, the other vacuously green; per-game passes counted as thirteen in two places and fourteen in a third. **Q3:** which label goes with which switch state was unstated (the toolbar is text-only, so it is the whole affordance); §4.5's script was to fail on "any pair marked as required" with nothing marking any. **Q4:** INV-7's "every tile" against a loop that never reaches `default:`; INV-2 asserted a call nothing could observe; and — found while verifying, not by a lane — INV-3's test was **unwritable at all**, `cardWidth()` being `private` on all six card views with no laid-out-rect accessor anywhere. INV-3 and INV-6 withdrawn to the per-game pass contract rather than weakened. **Dismissed:** the `uitest.cpp::name` test-identifier convention, raised as an open question by all three lanes and by none as a finding — a builder resolves it either way, so it answers no question; the notation was tidied in passing anyway. **Open questions resolved clean, counted nowhere:** §4.2's "every game delegates as `GameView(parent)`" holds 14/14; INV-4's grep survives its own fix; `gameshub_uitest` links both source halves. **Collateral from this loop's own fixes:** §1 was left promising a guarantee this change does not deliver, and §6 still cited withdrawn INV-6 — both corrected before the commit. |
