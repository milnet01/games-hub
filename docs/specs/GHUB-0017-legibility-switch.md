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
and turning it on is enough to guarantee that no game draws a playing card too
small to show its pips. Today Canasta is the only game that has been adjusted
for a partially sighted player, that adjustment is hardcoded rather than
chosen, and the six games that draw cards can all render them below the width
at which `CardArt` gives up on the face entirely.

## 2. Problem

The owner is partially sighted and **reads a card by its pip pattern, not by
the corner index**. That is the constraint this whole item exists to serve, and
it is the constraint the codebase currently violates in the most direct way
possible.

**2.1 `CardArt` stops drawing the face below 46 pixels, and four games can go
below it.** `src/cards/cardart.cpp::paintFace` returns early — leaving only the
corner index — under a bare literal:

```
$ grep -n "r.width() < 46" src/cards/cardart.cpp
216:    if (r.width() < 46)
```

The card-width floors of the four solitaires are all under that number:

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
`HeartsView::cardWidth()` is the only card game that clamps upward,
`std::clamp(std::min(byWidth, byHeight), 40.0, 92.0)`, and its floor of 40 is
still below 46. `CanastaView::cardWidth()` clamps to `[34, 88]` and then draws
melds at `kMeldScale = 0.74`, which its own comment explains is chosen to stay
above the threshold.

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

| Pair | Ratio | Verdict |
|------|-------|---------|
| `CardArt` `kRed` on `kFaceBottom` | 5.01 | passes 4.5 |
| `CardArt` `kBlack` on `kFaceBottom` | 14.11 | passes 4.5 |
| Sudoku pencil mark `#6d6a5e` on `kPaper` | 4.88 | passes 4.5 |
| Sudoku `kOwnInk` on `kPaper` | 4.76 | passes 4.5 |
| Minesweeper `kNumberColours[1]` on `kDug` | 4.35 | passes 3 only |
| **2048 `inkFor(v)` for v ≥ 8, on tile 8** | **1.72** | **fails** |
| **2048 `inkFor(v)` for v ≥ 8, on tile 512** | **1.50** | **fails** |

Two conclusions, and they point in different directions. The card and Sudoku
inks **pass** — so Sudoku's pencil marks are hard to read because of their
size, not their colour (`markFont.setPointSizeF(cell * 0.20)` against
`cell * 0.52` for a real digit, drawn into a cell third), and the same is true
of the cards. But `Twenty48View::inkFor()` returns near-white for every tile
above 4, over mid-orange and mid-yellow tiles, and **fails 3:1 at every such
tile**. That is unreadable for anyone and is a defect independent of any
preference.

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
   call, made here. §2.3 measures it failing 3:1 at every tile from 8 upward;
   a tile nobody can read is a bug, and putting the fix behind a preference
   would leave it broken for everyone who never finds the switch.
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
text `"🔍 Large"` / `"🔍 Normal"`, toggling `Legibility::instance().setEnabled()`.
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

**A card game in large mode must satisfy two things together, and the second is
the one that is easy to miss.** Raising the floor in `cardWidth()` to
`kFaceMinWidth` guarantees a readable card and *also* guarantees the layout
overflows the widget once the window is small enough — the row cost is fixed,
so a wider card means a wider row. So each card game must also raise its
`minimumSize` to the size at which its own layout yields exactly
`kFaceMinWidth`, and lower it again when the switch goes off. Qt then refuses
to shrink the window past it, and a stored `window/geometry/<page>` smaller
than the new minimum is clamped on restore rather than honoured.

The per-game arithmetic differs (Klondike's row cost is `7 + 6·0.14`, Spider's
is ten columns plus gaps, Pyramid's is the `kRows * 0.62 + 0.4` expression) and
belongs to each game's own pass, not here. What this spec fixes is the
obligation: **floor and minimum size move together, or not at all** (INV-3).

### 4.5 `scripts/legibility-check.py` — the numbers become an output

The table in §2.3 is generated, not transcribed. The script holds the
foreground/background pairs, computes the WCAG 2.2 relative-luminance ratio,
and exits non-zero if any pair marked as required falls below its threshold.
`tests/uitest.cpp` gets the same check over `Twenty48View::tileColour()` and
`Twenty48View::inkFor()` directly, so the one pair that is a live defect is
locked by a test rather than by a script somebody remembers to run (INV-7).

### 4.6 What the switch does *not* do here

It changes no game's appearance in this change. `Legibility` ships with the
switch, the hook, the exported threshold and the 2048 fix; the fourteen games
read it one at a time afterwards (§9). That is deliberate — the mechanism is
the part every game binds to, and it is worth getting a cold read on before
thirteen games are written against it.

## 5. Invariants

- **INV-1** — The switch survives a restart: a value written by `setEnabled()`
  is read back by a freshly constructed `QSettings`, on both platforms.
  *Test:* `tests/uitest.cpp::legibilityPersists` — set, destroy, construct a
  new `QSettings`, read `display/legibility`.
  *Breaks when:* the value is held in the singleton and never written; or
  written under a per-game group so a second game reads a different setting;
  or checked with `QFile::exists(QSettings().fileName())`, which is false on
  Windows however well saving works (`CLAUDE.md`, Traps).

- **INV-2** — Every *constructed* game is notified, not only the visible one.
  *Test:* `tests/uitest.cpp::legibilityReachesBackgroundGames` — open two
  games so both are constructed, return to the tile grid, toggle the switch,
  and assert both views received `applyLegibility`.
  *Breaks when:* the hub connects `currentView()` instead of connecting in the
  `GameView` base constructor — the bug is invisible until the player reopens
  a game they had already visited.

- **INV-3** — With the switch on, every card game's layout **fits inside its
  own minimum size**: at `minimumSize()`, the laid-out row is no wider than the
  widget.
  *Test:* `tests/uitest.cpp::legibilityFitsAtMinimum` — for each of the six
  views that include `cardart.h`, enable, `resize(minimumSize())`, and assert
  `cardWidth() >= CardArt::kFaceMinWidth` **and** that the rightmost laid-out
  card's right edge is within `width()`.
  *Breaks when:* a game raises its `cardWidth()` floor to `kFaceMinWidth` and
  leaves `minimumSize` alone — the cards then become readable and the last
  column walks off the right-hand edge. Stating only the first half would be
  vacuous: `std::max(kFaceMinWidth, …)` satisfies it by construction.

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
  *Test:* `tests/uitest.cpp::legibilityDefaultsOff` — clear the key, construct,
  assert `enabled() == false`.
  *Breaks when:* the key is renamed in a later release, which silently turns
  the switch off for a player who had turned it on — the old key is still in
  their settings file and nothing reads it.

- **INV-6** — Turning the switch off restores the previous appearance exactly.
  *Test:* `tests/uitest.cpp::legibilityIsReversible` — `renderOf(view)` with
  the switch off, toggle on, toggle off, `renderOf` again, assert the two
  `QImage`s are equal.
  *Breaks when:* a game applies large mode by mutating cached state it never
  restores — a re-deal at a different size, a font kept on the painter, a
  minimum size raised and not lowered. This is the invariant that makes the
  thirteen per-game passes safe to do one at a time.

- **INV-7** — Every 2048 tile's ink meets 3:1 against its own tile colour, with
  the switch in either position.
  *Test:* `tests/uitest.cpp::twenty48InkIsReadable` — for every value in
  `tileColour()`'s switch, assert `ratio(inkFor(v), tileColour(v)) >= 3.0`.
  *Breaks when:* the near-white ink is used over a mid-tone tile. **Seen
  failing against current code**: 1.72 at tile 8, 1.50 at tile 512 (§2.3).

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
  implementation does both; an override that does neither is caught by INV-6
  only if it also fails to restore, so this is a `nothing` row in §10.
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

| Test | Locks | Notes |
|------|-------|-------|
| `legibilityDefaultsOff` | INV-5 | clears the key first, so it is not order-dependent |
| `legibilityPersists` | INV-1 | fresh `QSettings`, never `QFile::exists` |
| `legibilityReachesBackgroundGames` | INV-2 | needs two games constructed |
| `legibilityFitsAtMinimum` | INV-3 | loops the six card views |
| `legibilityIsReversible` | INV-6 | uses the existing `renderOf()` helper |
| `twenty48InkIsReadable` | INV-7 | seen failing before the fix |

INV-4 is a grep, run by `scripts/legibility-check.py`'s sibling check rather than
by a compiled test.

Every one of these must be seen failing against pre-fix code before it is
believed. INV-7 already has been (§2.3). INV-3's fixture is the one to watch:
assert the fit, not just the width, or it passes against an implementation that
never raises a minimum size.

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

- **The thirteen per-game legibility passes** — each is a display change inside
  one game, done individually and shown to the owner before the next starts
  (§3.2). Tracked by GHUB-0017 itself, which stays open after this ships.
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
| INV-1 | `tests/uitest.cpp::legibilityPersists` |
| INV-2 | `tests/uitest.cpp::legibilityReachesBackgroundGames` |
| INV-3 | `tests/uitest.cpp::legibilityFitsAtMinimum` |
| INV-4 | `scripts/legibility-check.py` (grep check); also a compile error if the literal is removed and nothing defines the constant |
| INV-5 | `tests/uitest.cpp::legibilityDefaultsOff` |
| INV-6 | `tests/uitest.cpp::legibilityIsReversible` |
| INV-7 | `tests/uitest.cpp::twenty48InkIsReadable` |
| §4.3 the action sits after the spacer and survives `setGameActions()` | **nothing** — no test inspects toolbar order; a game's actions displacing it would be visible only by eye |
| §6 an override that forgets to repaint | **nothing** — INV-6 catches it only when it also fails to restore |
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
