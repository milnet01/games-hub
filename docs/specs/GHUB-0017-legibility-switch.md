# GHUB-0017 — A hub-owned legibility switch every game reads

**Status:** accepted (2026-08-13) — the review gate reached its three-loop
cap without an empty loop; §12 records why that is growth rather than an
unsettled contract. **§4's mechanism was built on 2026-08-14** and §12 carries
a fold-back row for the three clauses that survived the gate and were proved
wrong by the build. **All fourteen per-game passes (§9) have landed** —
Canasta and Sudoku on 2026-08-19, each with a fold-back row of its own, and the
remaining twelve on 2026-08-20 as GHUB-0071. §4.6 therefore no longer describes
the tree: it was a statement about the mechanism change, and no game ignores
the switch now. `everyGameAnswersTheSwitch` in `tests/uitest.cpp` is what holds
that true — it renders each game with the switch off and on and asserts the
picture changed and then went back, so the claim is a check rather than a
count.
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

**What this change does NOT do is make any game respond to the switch** (§4.6).
The guarantee that no game draws a card too small to show its pips arrives one
game at a time afterwards, and each of those passes is judged by eye (§3.2,
§9). Claiming it here would be claiming the switch does the work the passes do.
**2048's ink is the one deliberate exception** — it changes for everyone,
switch or no switch, because it is a defect and not a preference (§3.4, §4.6).

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
(§4.5). The colour pairs were transcribed from the
source by hand, so a transcription error would show up here as a real-looking
number. **The first job of §4.5's script is to reproduce this table**; where it
disagrees, the table is wrong and gets corrected, and that correction is a
fold-back row in §12 rather than a silent edit.

**Run 2026-08-14: every row reproduces to the stated decimal.** The one
disagreement was the script's, not the table's — its array parser skipped the
four-argument `QColor(0, 0, 0, 0)` at `kNumberColours[0]` and so measured the
wrong element for the Minesweeper row. §12's fold-back row has it.

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

**Minesweeper's 4.35 is the row to notice**: required at 3.0 it passes, so
that row stays green. Requiring it at 4.5 would fail a game nobody has
complained about, which is how a check gets disabled.

**The script as a whole is RED on unchanged code, and is meant to be** — the
three 2048 rows fail their own 3.0 threshold until §4.7 lands. Its first green
run is the one that proves §4.7 works. An implementer who expects green on the
pre-fix tree will conclude their script is broken and relax the thresholds,
which is the same disabling by another route.

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
production surface existing only to be asserted on. `applyLegibility` is
declared `public` alongside the other `GameView` virtuals, because it is part
of the contract a game implements rather than an internal detail.

**The probes are free-standing — deliberately not added to the hub.**
`HubWindow`'s public surface is only its constructor, `openGameNamed()` and
`gameNames()`; its `QStackedWidget` is private, so a test cannot install a
probe as a page, and this spec asks for no hook to let it. That costs nothing,
because a free-standing probe is the *stronger* fixture: it is connected purely
by the `GameView` base constructor, so it stays green under the design in §4.2
and goes red under any implementation that instead has the hub walk its own
pages — which is exactly the bug INV-2 exists to catch.

**The connection is made in the base constructor, so every constructed game is
connected whether or not it is on screen.** Games are built lazily by
`HubWindow::openGame`, so at any moment some are constructed and some are not;
connecting only the visible one would leave a game that was opened earlier
still laid out for the old setting when the player returns to it. A game
constructed *after* the change reads the current value at build time and needs
no notification.

### 4.3 `HubWindow` gains the control

`HubWindow::buildChrome()` creates `m_legibilityAction` immediately after
`m_soundAction`, checkable, toggling `Legibility::instance().setEnabled()`.
**The order matters and is the opposite of the precedent beside it:**

```cpp
m_legibilityAction = new QAction(QStringLiteral("🔍 Normal"), this);
m_legibilityAction->setCheckable(true);
connect(m_legibilityAction, &QAction::toggled, this, [this](bool on) {
    Legibility::instance().setEnabled(on);
    m_legibilityAction->setText(on ? QStringLiteral("🔍 Large")
                                   : QStringLiteral("🔍 Normal"));
});
m_legibilityAction->setChecked(Legibility::instance().enabled());  // AFTER
m_toolBar->addAction(m_legibilityAction);
```

`m_soundAction` does `setChecked(true)` **before** `connect`, which is
harmless there because its state is hardcoded on and its construction text is
already the on-label. Copy that order here and a player who had turned the
switch on launches with a checked button reading "🔍 Normal" — the inverted
control this section exists to prevent. Connecting first makes `setChecked()`
emit `toggled` and sync the label; the construction text is the off-label so
the unchecked case is right too.

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

**The script holds the pair LIST and the thresholds; it reads every colour
VALUE out of the source.** Each entry names a constant and the file defining it
(`kRed` in `src/cards/cardart.cpp`, `kPaper` in `src/sudoku/sudokuview.cpp`,
and so on); the script greps the definition and parses the three components,
and **exits non-zero if a named constant cannot be found**. That is the whole
point: §2.3's pairs were transcribed by hand, and a script holding its own copy
of the same hex could only ever disagree by arithmetic — it could never catch
the transcription error §2.3 names as its first job, and it would stop tracking
the source the day a colour constant changed.

Thresholds come from §2.3's `Required` column; the script exits non-zero when a
pair falls below its own. **Its 2048 pairs are the post-§4.7 ones** — §4.7
replaces `inkFor()` outright, so the pre-fix near-white ink no longer exists in
the product, and a script carrying it would fail forever on merged code. §2.3's
2048 rows are retained as the *pre-fix* measurement, the "seen failing"
evidence for INV-7, and are not what the script asserts.

`--thresholds` runs INV-4's grep instead, so one script owns both mechanical
legibility checks.

`tests/uitest.cpp` gets the 2048 check directly against the game's own
`tileColour()` and `inkFor()` — **free functions in an anonymous namespace in
`src/twenty48/twenty48view.cpp`, which INV-7 requires be declared in
`src/twenty48/twenty48view.h` and moved out of it.** Until that happens the
test cannot reference them at all. Locking it in a compiled test rather than
only in the script matters because the script is run by hand and `ctest` is
not.

### 4.6 What the switch does *not* do here

**No game changes appearance in response to the switch.** `Legibility` ships
with the switch, the hook and the exported threshold; the fourteen games read
it one at a time afterwards (§9). That is deliberate — the mechanism is the
part every game binds to, and it is worth getting a cold read on before
fourteen games are written against it.

**§4.7's 2048 ink fix is the exception and is not gated by the switch at all**
(§3.4). It ships here, it changes what every player sees, and `Legibility` is
not consulted for it. So "nothing looks different" is true of the *switch* and
false of *this change*, and the two must not be conflated.

**This is why §5 carries no invariant about card size.** An invariant asserting
that cards clear `kFaceMinWidth` would be red on all six card views the day it
landed, because no view is adapted here; one asserting the switch is reversible
would be vacuously green, because nothing changes to be reversed. Both belong
to the first game's pass, and §9 records them there.

### 4.7 The 2048 ink rule, stated exactly

**`tileColour()` does not change. `inkFor()` is replaced outright**, and the
replacement is a luminance test rather than a value test:

```cpp
// src/twenty48/twenty48view.h — declared here so uitest.cpp can reach it
// (INV-7); moved out of twenty48view.cpp's anonymous namespace.
QColor tileColour(int value);
QColor inkFor(int value);

// Exported for INV-7: the test must assert against the SAME formula the
// product uses, or it checks its own arithmetic and a wrong inkFor() stays
// green. This is the third and last home for the WCAG maths -- here, and
// scripts/legibility-check.py's Python transcription of it.
double relativeLuminance(const QColor& c);
double contrastRatio(const QColor& a, const QColor& b);

// src/twenty48/twenty48view.cpp
QColor inkFor(int value)
{
    // Which ink reads on a tile is a property of the TILE's brightness, not of
    // its number. Keying on the value meant every tile from 8 up got near-white
    // ink over a mid-tone colour, at half the contrast a reader needs.
    return relativeLuminance(tileColour(value)) > 0.20
             ? QColor(0x26, 0x23, 0x1d)      // near-black, for the light tiles
             : QColor(0xf9, 0xf6, 0xf2);     // near-white, for the dark one
}
```

**Why a luminance test and not a corrected value cut-off.** The obvious minimal
fix — widen the `value <= 4` arm so more tiles get the existing dark
`#776e65` — does not work: that ink fails 3:1 on tiles 8 (2.70), 16 (2.23),
32 (1.90), 64 (1.57) and 2048 (2.94). No cut-off on the *value* fixes it,
because the tiles are not ordered by brightness — 64 is the darkest at
L = 0.279 while 128 jumps back up to L = 0.639. Only a darker ink clears every
light tile, and only a luminance test picks the right ink for the `default:`
arm, which is near-black (L = 0.042) and needs the white.

Ratios under this rule, every value `tileColour()` can return:

| Tile | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 | 1024 | 2048 | `default:` |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Ratio | 12.49 | 12.01 | 8.46 | 6.98 | 5.94 | 4.91 | 10.27 | 10.00 | 9.68 | 9.44 | 9.22 | 10.57 |

**It changes two tiles that already passed.** Tiles 2 and 4 currently use
`#776e65` at 3.98 and 3.83; under this rule they become near-black at 12.49 and
12.01. That is a visible change to two tiles nobody complained about, accepted
because one rule that is correct for every tile — including any added later —
beats a rule with two special cases carved out to preserve a shade. `0.20` is
chosen well below the darkest light tile (64, at 0.279) and well above the dark
one (0.042), so no tile sits near the boundary.

`relativeLuminance()` and `contrastRatio()` are the WCAG 2.2 formulae, are new,
live beside `inkFor` and are **exported in the block above** — INV-7 calls
`contrastRatio` rather than re-deriving it. `scripts/legibility-check.py` (§4.5)
holds the only other copy, in Python.

## 5. Invariants

- **INV-1** — The switch survives a restart: a value written by `setEnabled()`
  is read back by a freshly constructed `QSettings`, on both platforms.
  *Test:* `tests/uitest.cpp`, block `legibilityPersists` — set, destroy, construct a
  new `QSettings`, read `display/legibility`.
  *Breaks when:* the value is held in the singleton and never written; or
  written under a per-game group so a second game reads a different setting; or
  **the key is renamed in a later release**, which silently turns the switch
  off for a player who had turned it on — the old key sits in their settings
  file and nothing reads it. This block is the only one that can see a rename,
  because it names the literal key on both sides of the round trip;
  or checked with `QFile::exists(QSettings().fileName())`, which is false on
  Windows however well saving works (`CLAUDE.md`, Traps).

- **INV-2** — Every *constructed* game is notified, not only the visible one.
  *Test:* `tests/uitest.cpp`, block `legibilityReachesBackgroundGames` — build
  two free-standing `LegibilityProbe`s (§4.2), `show()` one and leave the other
  unshown, then **drive the switch to a known state and back**:
  `setEnabled(false)`, record each probe's `calls`, `setEnabled(true)`, and
  assert **both** counters rose by exactly one. Absolute counts cannot be
  asserted — `setEnabled()` is a no-op when unchanged (§4.1), so an earlier
  block that left the switch on makes a bare `setEnabled(true)` emit nothing.
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
  `grep -rnE '(^|[^0-9A-Fa-fx.])46(\.0)?([^0-9A-Fa-f]|$)' src --include=*.cpp --include=*.h | grep -v kFaceMinWidth | grep -vE '^[^:]+:[0-9]+: *//'`
  → no output. The three exclusions each earn their place: the leading class
  rejects hex digits and a decimal point so `0x46` and `h * 0.46` do not match
  (there are five of the latter in `src/`), `kFaceMinWidth` drops the
  definition line itself, and the last drops comment lines, two of which
  legitimately discuss the number in `canastaview.cpp`.
  **An earlier form of this clause filtered with `grep -v "0x"`, which drops
  the WHOLE line** — so `if (w < 46.0 && c == 0xff)` was invisible to it. A
  test that a violation can hide from is not a test; verified by running both
  forms against that exact string.
  *Breaks when:* a second game hardcodes 46 in its own floor rather than
  including the constant, and the two drift when the threshold is retuned.
  **Seen failing against current code**: the grep returns
  `src/cards/cardart.cpp:216:    if (r.width() < 46)` today, and is empty once
  §4.4 lands. (An earlier draft of this clause counted matches *inside*
  `cardart.cpp` and expected `1` — which is satisfied by the defect and
  violated by the fix. It was caught by running it.)

- **INV-5** — On a machine with no stored value the switch is off, and the
  stored key is `display/legibility`.
  *Test:* `tests/uitest.cpp`, block `legibilityDefaultsOff` — **must be the
  first block in `main()`, before any `GameView` is constructed.** Clear
  `display/legibility` from a fresh `QSettings`, then assert
  `Legibility::instance().enabled()` is false.
  *Why the position is part of the clause:* `Legibility` is a singleton whose
  private constructor reads `QSettings` **once**, at first use — and §4.2 has
  every `GameView` constructor connect to it, so constructing any game
  instantiates it. A block that clears the key afterwards is asserting a value
  cached earlier, and would stay green after the default flipped to on. No
  `reloadFromSettings()` is added for this: a method existing only so a test
  can re-read is production surface the product does not need.
  *Breaks when:* the block drifts down `main()` below a `GameView`
  construction — after which it asserts whatever the store happened to hold at
  process start rather than the default, so it stops being a statement about
  the default at all.
  **What it does NOT do is stay green under a flipped default, which this
  clause claimed until implementation: measured 2026-08-14, the drifted block
  goes red too.** `Scores::clear()` is `QSettings::clear()`, which wipes the
  whole store rather than only the scores, so the suite starts every run with
  no stored value and a drifted block reads the flipped default exactly as a
  correctly-placed one does. The position rule earns its place by making the
  block *deterministic* — independent of which block wrote last — not by being
  the only thing that can see a default flip.
  *Deliberately NOT owned here:* a key **rename**. This block clears
  `display/legibility` and asserts false — but on the test suite's own fresh
  settings scope an unset key of *any* name also yields false, so the clear is
  a no-op and the block stays green under a rename. INV-1 owns that, because it
  writes and reads the literal key back.

- **INV-6** — *withdrawn — moved to the per-game pass contract (§9).* It
  asserted that turning the switch off restores the previous
  appearance exactly, tested by comparing two `renderOf()` images. Against this
  change it is **vacuously true**: §4.6 ships no game adaptation, so nothing
  can fail to be restored, and a test that cannot fail is not a contract. It
  becomes falsifiable the moment a game's pass changes something, which is
  where it belongs and where it does the work of making the passes safe to do
  one at a time.

- **INV-7** — Every tile value `tileColour()` enumerates, **and its `default:`
  arm**, has ink meeting 3:1 against its own tile colour. The switch is not
  mentioned because §4.7's rule does not consult it — that is the invariant's
  point, not an omission.
  *Test:* `tests/uitest.cpp`, block `twenty48InkIsReadable` — for
  `{2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096}`, assert
  `contrastRatio(inkFor(v), tileColour(v)) >= 3.0`. The trailing 4096 is the
  `default:` arm, which a loop over the enumerated cases alone never reaches.
  **Three of the twelve pass today** — 2 and 4, which take the dark
  `#776e65` arm, and 4096, which is near-black under near-white ink. The other
  nine are the defect.
  *Requires a code change this spec asks for:* `tileColour()` and `inkFor()`
  are free functions in an **anonymous namespace** in
  `src/twenty48/twenty48view.cpp` and appear in no header, so `uitest.cpp`
  cannot call them at all. Declare all four of `tileColour`, `inkFor`,
  `relativeLuminance` and `contrastRatio` in `src/twenty48/twenty48view.h` and
  move them out of the anonymous namespace (§4.7's block). Without that the
  test does not compile, and nothing else in this spec would have said so.
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
- **A stored geometry smaller than the new minimum — and the enlarged size is
  then saved over it.** Qt clamps on restore, so the game opens larger than it
  was left; that much is the switch doing its job. What is **not** acceptable
  is what happens next: `HubWindow::rememberPage()` writes
  `QSettings().setValue(geometryKey(m_page), saveGeometry())` unconditionally
  on every page change, so the clamped-larger geometry overwrites the stored
  one, and turning the switch off later leaves the window permanently enlarged.
  **A game's pass must therefore preserve the pre-clamp geometry and restore it
  when the switch goes off** — it cannot rely on the stored value being
  untouched. This is a per-game obligation like the floor itself (§9), and it
  is the second half of withdrawn INV-6's reversibility claim.
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

**Four of the seven invariants are locked in `tests/uitest.cpp`** — INV-1,
INV-2, INV-5 and INV-7. INV-4 is a grep (below), and INV-3 and INV-6 are
withdrawn to the per-game passes (§5, §9). Nothing belongs in
`gameshub_selftest`, which links no widgets and cannot construct a view;
`QColor` is QtGui, so even INV-7's contrast arithmetic belongs here rather than
in the rules half.

**`tests/uitest.cpp` has no test-function registry** — it is one `main()` with
inline blocks guarded by a `check(bool, const char*)` helper. The names below
are the labels passed to `check`, not symbols; nothing in this spec asks for a
registry to be built.

| Block | Locks | Notes |
|-------|-------|-------|
| `legibilityDefaultsOff` | INV-5 | **must be first in `main()`** — the singleton caches at first use |
| `legibilityPersists` | INV-1 | fresh `QSettings`, never `QFile::exists` |
| `legibilityReachesBackgroundGames` | INV-2 | needs the `LegibilityProbe` of §4.2 |
| `twenty48InkIsReadable` | INV-7 | needs §4.7's four functions exported first |

INV-4 is a grep, run by `scripts/legibility-check.py --thresholds` rather than
by a compiled test. **INV-3 and INV-6 are withdrawn to the per-game passes
(§5, §9) and have no test here** — neither could run against this change: one
would be red on six views that this change does not touch, the other vacuously
green because nothing changes appearance.

**Every block must be seen failing before it is believed — but only INV-7 can
fail against *pre-fix* code.** `Legibility`, `applyLegibility` and
`LegibilityProbe` do not exist yet, so INV-1, INV-2 and INV-5 do not compile
against the current tree, let alone fail. Each is instead seen failing against
a deliberately broken implementation, named here so the step is not quietly
skipped:

| Block | Break it by |
|-------|-------------|
| `legibilityPersists` | having `setEnabled()` update `m_enabled` and not write QSettings |
| `legibilityReachesBackgroundGames` | connecting in `HubWindow::openGame` for `currentView()` instead of in the `GameView` base constructor |
| `legibilityDefaultsOff` | flipping `Legibility()`'s default to `true` — **not** by moving the block, see INV-5 |
| `twenty48InkIsReadable` | nothing — current code fails it already (§2.3) |

**INV-2 is the one to watch**: assert the probe's counter delta, not that a
repaint happened, or it passes against a hub that notifies only the visible
view.

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
| 2 | 2026-08-13 | 3 cold (general-purpose), genre pinned `spec` | 2 | 2 | 2 | 3 | 9 verified, all fixed; 0 dismissed. **None of loop 1's findings reappeared**, which is the evidence those fixes held. **The one that mattered:** §1, §3.4, §4.6 and INV-7 all required a 2048 ink fix and **no section of §4 ever said what the fix was** — three implementers would have invented three different palettes, all satisfying INV-7. Now §4.7, stated as code: `tileColour()` unchanged, `inkFor()` replaced by a **luminance** test rather than a value test. Measurement drove that: widening the existing `value <= 4` arm cannot work, because the current dark `#776e65` itself fails on tiles 8 (2.70), 16 (2.23), 32 (1.90), 64 (1.57) and 2048 (2.94), and the tiles are not ordered by brightness — 64 is the darkest at L=0.279 while 128 jumps back to L=0.639. **Q1:** "the one value in the list that passes today" — three do (2, 4 and the `default:` arm); "a private virtual cannot be overridden from a subclass outside the class" — false in C++, access control governs calling, not overriding. **Q2:** §1 and §4.6 both said no game looks different while §3.4 changes 2048's ink for everyone — an implementer resolving it would have gated the fix behind the switch, which §3.4 forbids; §7 still opened "All seven invariants are locked in `tests/uitest.cpp`" seventeen lines above its own four-row table. **Q3:** the 2048 fix unspecified (above); INV-2 never pinned the switch's starting state, and `setEnabled()` is a no-op when unchanged, so a bare `setEnabled(true)` after an earlier block emits nothing. **Q4:** INV-5's block could not work at all — `Legibility`'s constructor is private and reads `QSettings` once at first use, and §4.2 has every `GameView` constructor instantiate it, so clearing the key later asserts a cached value; INV-2's "add both to the hub's stack" is impossible, `HubWindow` exposing only its constructor, `openGameNamed()` and `gameNames()`; INV-7 claimed "with the switch in either position" against a loop that never touches the switch. **Four of the nine landed on text loop 1's own fixes added** — the §7 lead sentence, the §1 guarantee, the "one value" count and the C++ claim — which is the expected shape and the reason the loop is run cold rather than briefed. **Open questions resolved clean, counted nowhere:** all three lanes queried whether §2.1's second grep block was real output, since the packet showed those functions with their expressions inlined — the **document is verbatim** (`sed -n 199p src/freecell/freecellview.cpp` matches exactly) and the packet was the paraphrase, so the finding was evidence against the packet, as the brief says it should be; and `tests/uitest.cpp` already runs under `GamesHubTest`/`GamesSelfTest`, so INV-1 and INV-5 never touch a player's real settings. |
| 3 | 2026-08-13 | 3 cold (general-purpose), genre pinned `spec` | 1 | 1 | 3 | 3 | 8 verified, all fixed; 0 dismissed. **Cap loop** — `--max-loops` 3; the gate did NOT reach an empty loop. Nothing from loops 1 or 2 reappeared. **Q1:** §6 claimed "the stored value is not rewritten, so turning the switch off returns the window to its old size" — false: `HubWindow::rememberPage()` writes `QSettings().setValue(geometryKey(m_page), saveGeometry())` unconditionally on every page change, so a window Qt clamped larger has that larger size saved over the old one, and the player's window stays permanently enlarged. Preserving pre-clamp geometry is now a per-game obligation in §9. **Q2:** §2.3 said the script "stays green on unchanged code" when its own table marks three rows Required 3.0 / fails — it is red until §4.7 lands, and an implementer trusting the sentence would have relaxed the thresholds to get green. **Q3:** INV-7 asserted `ratio(...)` and nothing defined `ratio`; §4.7 mentioned `relativeLuminance` but exported only `tileColour` and `inkFor`, so the test would have hand-rolled a *third* copy of the WCAG maths and asserted its own arithmetic — all four functions are now exported and INV-7 calls `contrastRatio`. §4.5 never said where the script gets its colour VALUES: holding its own hex copy, it could only disagree with §2.3 by arithmetic and could never catch the hand-transcription error §2.3 names as its first job — it now reads each named constant from source and exits non-zero if one is missing, and carries the post-§4.7 inks rather than the pre-fix ones. §4.3 never fixed the action's initial text or the `setChecked`/`connect` order, and the `m_soundAction` precedent beside it does `setChecked` FIRST — copying it would launch a player who had the switch on with a checked button reading "🔍 Normal", the exact inversion §4.3 exists to prevent. **Q4:** INV-5's *Breaks when* claimed it caught a key rename, which it cannot — on the suite's fresh scope an unset key of any name yields false, so the clear is a no-op; ownership moved to INV-1, which names the literal key on both sides of a round trip. §7 required every block be "seen failing against pre-fix code", impossible for INV-1/2/5 since `Legibility`, `applyLegibility` and `LegibilityProbe` do not exist pre-change; each now names the deliberate break that must be seen instead. And — found while verifying, not by a lane — INV-4's own grep filtered with `grep -v "0x"`, which drops the WHOLE line, so `if (w < 46.0 && c == 0xff)` was invisible to it; both forms were run against that string before the replacement was written. **Open questions resolved clean, counted nowhere:** no file under `src/` other than `cardart.cpp:216` states 46 as a bare decimal (the five `0.46` hits and two comments are excluded by the new grep, which was run); `CLAUDE.md`'s "0.62" was in fact corrected, a lane's session copy being stale. **Assessment at the cap, per the gate's own rule:** this is document *growth*, not an unsettled contract. The mechanism — the `Legibility` singleton, the `GameView` hook, `kFaceMinWidth`, the toolbar action — has not been challenged since loop 1 by any lane. What keeps producing findings is the prose and test clauses each fix pass adds: 434 → 557 → 643 → 727 lines, with a growing share of each loop's findings landing on the previous loop's additions (4 of 9 in loop 2; 3 of 8 here). A fourth loop would likely return a smaller number of the same kind. Shipping. |
| fold-back | 2026-08-14 | implementation (§4.1–§4.7 built and green) | — | — | — | — | **Three corrections the build proved, folded back per `write-spec`'s obligation. Not a review loop and not gated: the code exists, so a cold read before implementation has nothing left to protect.** **INV-5's *Breaks when* was false** — it claimed a drifted block "stays green after the default flipped to on"; measured both ways, the drifted block goes red too, because `Scores::clear()` is `QSettings::clear()` and wipes the whole store, so the suite starts every run with no stored value. The position rule is kept, for the reason that actually holds: it makes the block deterministic rather than dependent on which block wrote last. §7's break table now names the flipped default instead. **INV-1's block needed `contains()` on both halves** — `QVariant().toBool()` is false, so the "off" assertion as first written was satisfied by the key being ABSENT, and a `setEnabled()` that wrote nothing at all would have passed it. **§2.3's hand-transcribed table is exactly right, and the script was wrong first** — `scripts/legibility-check.py` initially read `kNumberColours[1]` as 5.63 against the table's 4.35. The table was not the error: the array parser matched only three-argument `QColor(...)`, silently skipping the four-argument `QColor(0, 0, 0, 0)` at index 0 and shifting every later index by one, so it measured the green `[2]` and printed a plausible number. Fixed; all seventeen pairs now reproduce §2.3 and §4.7 to the stated decimal. **Also recorded:** `Scores::clear()` wiping the whole settings scope is test-only — no product code calls it — so nothing was changed there. |
| fold-back | 2026-08-19 | implementation (Canasta's per-game pass — § 9's first, carrying withdrawn INV-3 and INV-6) | — | — | — | — | **Two §2.1 claims the build corrected, and the shape the withdrawn invariants actually took. Not a review loop and not gated: the code exists, so a cold read before implementation has nothing left to protect.** **§2.1's four solitaires cannot reach the floors it names.** The section reasons from the floor *constants* — Spider 30, Pyramid 30, FreeCell 32, Klondike 34 — and concludes Spider is "the likeliest to reach it". None of them can. Every card view calls `setMinimumSize(minimumSizeHint())` in its constructor, so the smallest card each can actually be driven to is Klondike 67.9, FreeCell 67.4, Pyramid 68.6, Spider 54.2 and Hearts 52.5, all clear of `kFaceMinWidth`. **Canasta is the only game that draws a faceless card at a window it can actually reach**, at 37.1 px in its melds at 720×560. **And §2.1 counts the opponents' hands as a second surface when they are not one** — `paintOpponents` draws them face **down** (`paintCard(…, false, 0.8)` → `CardArt::paintBack`), so the face threshold never applied to them and the "clear it only above 57.5" figure decides nothing. `kMeldScale` is therefore the game's whole smallest-scale contract. **§4.4's three-part obligation is what shipped, in that order and for its stated reason.** Growing the melds in place was tried first and does not work: a meld card wide enough to show a face makes a seven-card canasta ~130 px tall at the minimum window, against the 107 px `bandFor()` allows, and the overflow reaches the stock and discard row — so the *floor alone* really does guarantee an overflowing layout, exactly as §4.4 item 2 says. `minimumSizeHint()` returns 900×656 under the switch, both terms of `cardWidth()` solving to ~62.7 there. **The floor of §4.4 item 1 is redundant while that minimum is right, and is kept knowingly** — removing it and re-running the suite moved no assertion, because at the raised minimum the table already gives a card more than `kLegibleCardWidth`. `cardsFitTable()` is what asserts it stays redundant; the day it goes red the floor has started clamping, which is a card the table has no room for. **§6's flights bullet resolved by landing them, not by re-aiming them.** `Flight::to` is a `QPointF` captured at launch, and the engine is already updated when a flight starts (`suppressed()` is what hides the card at the far end), so `applyLegibility` clears `m_flights` and every card is simply where it was going. **§6's geometry bullet is a `window()->resize()` back to the pre-clamp size**, kept in the view rather than relying on the stored value, as that bullet requires. **INV-3 as first written would have passed a broken build** — asserting only the meld width, it stayed green with the minimum-size branch removed, because the floor clamps the width whatever the window is. It now asserts `cardsFitTable()` too, and each assertion was seen red against its own deliberate break. |
| fold-back | 2026-08-19 | implementation (Sudoku's per-game pass — § 9's second, after Canasta) | — | — | — | — | **What §2.3 measured is right and what it cites has moved; and the size ceiling was not where the arithmetic said.** **§2.3's `markFont.setPointSizeF(cell * 0.20)` no longer exists as written** — the literal is now `kMarkRatio` in an anonymous namespace beside `kMarkRatioLegible`, and the ratio the switch selects is 0.29. The measurement behind the sentence stands: contrast was never Sudoku's problem, size was. **The pass changes the font and nothing else — no geometry, no minimum size.** Sudoku has no card threshold and no cliff of Canasta's kind; the marks scale smoothly with the window, so a minimum-size pass of Canasta's shape would have been invisible at any window a player actually uses. **`Qt::TextDontClip` is what the pass turns on, and without it the change is worse than nothing.** `drawText` clips to the rectangle it is given, each mark gets a cell third, and the font's LINE box — not the digit's ink — is what has to fit that third. So 0.20 was already near the ceiling: raising the ratio alone clips the top off every mark. The flag hands over the room between the ink and the line box, and it is set in both states because at 0.20 it changes nothing. **The ratio is measured, not chosen.** The app font's digits are ~0.685 of an em, so ink lands at ~2.74 × ratio as a fraction of the cell third: 0.29 → 0.795, 0.30 → 0.822, and at the smallest window the whole-pixel rounding of a 34 px cell takes 0.30 to 0.882, past what `marksFitCell()` allows. 0.30 was tried first and went red there. **Marks are bold as well as bigger** — at 9-10 pt stroke weight buys more than the extra points. **INV-6 lands here as it was originally written**, two renders that must match with a third between them that must not, and it needed a per-view latch to be seen failing: a `static` one latched during the earlier block and made all three renders symmetric, which is a bad break rather than a passing test. **INV-3 is Canasta's and is not restated** — Sudoku draws no cards. `marksFitCell()` is this pass's equivalent and plays the part `cardsFitTable()` plays there: it is what stops the size growing past the layout that has to hold it. Five uitest assertions, each seen red against its own deliberate break. selftest, uitest and ctest 3/3 all green. |
| fold-back | 2026-08-19 | implementation (Sudoku's pass, corrected by the Windows CI leg) | — | — | — | — | **The mark ratio the row above records as "measured, not chosen" was measured on the wrong thing, and CI said so.** 0.29 was the largest that fits *this machine's* font, and the ceiling is not a property of this codebase — it is how tall the platform draws a digit. Measured on the owner's Windows box: Segoe UI 0.728 of an em, Arial 0.731, Tahoma 0.760, against 0.685 here. Under Segoe UI a 0.29 mark comes to 0.845 of its cell third in exact arithmetic — inside the 0.85 limit — and tips past it once `tightBoundingRect` rounds to whole pixels at the smallest cell. The `windows-2022` leg of `ci.yml` went red on exactly the assertion written to catch it, while `ubuntu-24.04` passed. **No local run could have caught it, and that is not drift:** `scripts/local-ci.sh` executes `ci.yml`'s own `run:` blocks, but nothing on Linux drives MSVC, which the script says on every run. **The fix removes the constant rather than re-tuning it.** `markFont()` now probes the font once for its ink-per-point, takes the analytic size, and steps down until the ink *measured at the size it will be drawn at* fits — so the mark is the largest each platform allows rather than a number correct in one place and wrong or timid elsewhere. `marksFitAt(pointSize)` was added because with the size solved, "it fits" is true by construction: the assertion with teeth is "a step larger would not fit". **And the regression test is now a portability test** — it loops over the machine's own font families, locally spanning 0.49 to 0.99 of an em, which brackets every Windows candidate; restoring the tuned constant reddens it locally. §2.3's measured contrast figures are untouched and remain correct. |
