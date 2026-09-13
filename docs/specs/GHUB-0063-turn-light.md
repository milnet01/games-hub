# GHUB-0063 — Light whoever is playing

**Status:** spec draft (2026-09-13).
**Kind:** implement.
**Source:** ROADMAP GHUB-0063 (the owner's suggestion, 2026-08-20).

**Pairs with:** GHUB-0047.

**Layman:** a soft gold light settles on whoever is playing, so you can see
whose turn it is without reading anything.

## 1. Goal

In Hearts, Canasta, Chess, Draughts and Reversi, the seat or side whose turn
it is carries a soft gold light with a gold outline. The light fades in once
when the turn arrives and then holds still. It is stronger when the
legibility switch is on. A game at rest still asks for no repaints.

## 2. Problem

1. **Whose turn it is, is mostly text.** `ChessView`, `DraughtsView` and
   `ReversiView` build "Your move." or "Computer thinking…" in `refresh()`.
   That text reaches the board only through `GameView::paintStatusCaption`,
   which draws nothing while the legibility switch is off.
2. **Hearts recolours a label, and has no label for you.**
   `HeartsView::paintEvent` paints an opponent's name in `0xffd54f` when
   `m_engine.currentPlayer()` is that seat. Seat 0 has no name label, so your
   own turn has no mark on the table beyond the centre text.
3. **Canasta already glows, but not the way the owner asked.**
   `CanastaView::paintTable` paints a static `QRadialGradient` in
   `Theme::kGold` at `seatAnchor(m_engine.currentSeat())` during the Draw and
   Play phases. It appears at full strength at once and has no outline. The
   roadmap bullet's description of Canasta as text-only is out of date.
4. **The owner does not read the status bar during play** (stated
   2026-08-19), and is partially sighted (`CLAUDE.md` § Core rules). A cue that
   must be read is a cue that is missed.

## 3. Scope decisions (agreed with the user)

- **A light, not more words** — the owner, 2026-08-20 (GHUB-0063).
- **Anything that matters during play goes on the play surface** — the owner,
  2026-08-19.
- **Games are expected to look good and animate** — the owner, 2026-08-10.

The bullet's four design constraints are the owner's request as filed and are
taken as given: the glow is not the only cue; it strengthens under the
legibility switch; the blur is faked; it fades in once and does not pulse.

Three calls in § 4 are this spec's, not the owner's. § 15 lists them for the
owner to confirm.

## 4. Design

### 4.1 The shared painter

In `src/theme.h`, beside `Theme::paintDropShadow`:

```cpp
// How long the light takes to come up when the turn arrives.
inline constexpr int kTurnLightFadeMs = 300;

// A soft light over `area`: one radial gradient in kGold, plus a gold
// outline of `area` so the cue survives any colour vision. Both scale with
// `level` (0 to 1). `legible` raises the gradient's peak and doubles the
// outline width. Hands the painter back as it found it.
void paintTurnLight(QPainter& p, const QRectF& area, double level, bool legible);
```

One gradient, never a computed blur, on the precedent of
`Theme::paintDropShadow`. A `level` of 0 draws nothing.

### 4.2 What each view reports

In `src/gameview.h`:

```cpp
struct TurnLight {
    int seat = -1;       // -1 when it is nobody's turn
    double level = 0.0;  // 0 to 1: how far the fade has come
};

// Whose turn is lit, and how far its light has come up. The default is
// "nobody", which is right for every game without turns.
virtual TurnLight turnLight() const { return {}; }
```

Seat numbers are each game's own:

| View | Seats | Whose turn |
|------|-------|------------|
| `HeartsView` | 0 to 3, the engine's | `m_engine.currentPlayer()` |
| `CanastaView` | 0 to 3, the engine's | `m_engine.currentSeat()` |
| `ChessView` | 0 you, 1 the computer | `m_game.toMove()` against `m_human` |
| `DraughtsView` | 0 you, 1 the computer | `m_toMove` against `m_human` |
| `ReversiView` | 0 you, 1 the computer | `m_toMove` against `m_human` |

**Nobody's turn** is: the game is over; Hearts is pausing on a finished trick
(`m_awaitingCollect`); Canasta is outside its Draw and Play phases.

### 4.3 The fade

Each of the five views holds the lit seat and a level:

```cpp
int m_turnSeat = -1;
double m_turnLevel = 0.0;
```

When `refresh()` finds a different seat whose turn it is, it sets
`m_turnSeat` to that seat and `m_turnLevel` to 0. When it finds nobody's turn,
it sets both to their defaults at once. The previous seat's light goes out at
once; only the arriving light fades.

The level is **stepped, never clocked**. Each tick adds the tick length
divided by `kTurnLightFadeMs`, and stops at 1. A level worked out from elapsed
time would change between two renders of a stopped game, which
`everyGameAnswersTheSwitch` in `tests/uitest.cpp` fails as restless.

- **Hearts, Chess, Draughts and Reversi** get a `QTimer* m_turnTimer` at 16 ms.
  It stops itself when the level reaches 1, as `KlondikeView`'s
  `m_flightTimer` does.
- **Canasta** rides its existing 16 ms `m_timer`. `tick()` advances the level
  and sets `redraw` only while the level is below 1, as it does for
  `m_celebrate`.

Canasta's `animating()` stays flights only, so the fade never delays a
computer turn.

### 4.4 The game contract

- `hasPendingAnimation()` answers true while `m_turnSeat >= 0` and
  `m_turnLevel < 1`, in addition to what it answers today. A test's `settle()`
  and a `--shot` then never catch a half-lit seat.
- `deactivate()` stops `m_turnTimer` and leaves the level where it is. It
  freezes, per `docs/design.md` § The game contract. `activate()` restarts
  the timer only when a fade is part-way.
- `applyLegibility()` does not touch `m_turnSeat` or `m_turnLevel`. The switch
  is read live, at paint time, and passed to `paintTurnLight` as `legible`.
- `CanastaView::advanceForShot` sets `m_turnLevel` to 1 before returning, with
  the other settling it already does.
- No save format changes. The light is not saved; a restored game lights its
  current seat and fades it in once.

### 4.5 Where each light is painted

Each view paints the light before the cards or pieces over it.

- **Hearts:** seats 1 to 3 over `opponentRect(seat)`; seat 0 over the smallest
  rectangle holding every `handCardRect(i)` of the hand. The gold label colour
  stays.
- **Canasta:** the static glow in `paintTable` is replaced. The area is the
  square centred on `seatAnchor(seat)` with half-width `cardHeight() * 1.5`,
  so the light sits where the glow sat. The active plate's stronger edge in
  `paintOpponents` stays.
- **Chess, Draughts, Reversi:** a band along the board's edge — your seat on
  the bottom edge, the computer's on the top. The band sits just outside
  `boardRect()`, as wide as the board, and a twentieth of the board's height
  deep. It never overlaps the caption band `GameView::captionBand` reserves.

## 5. Invariants

These clauses cannot run yet; the surfaces they test are what this spec
creates. Each names the rule its fixture isolates.

- **INV-1** — While someone can move, `turnLight().seat` is that seat. While
  nobody can, it is -1.
  *Test:* `tests/uitest.cpp`, block `turnLightFollowsTheTurn`. For each of the
  five views: open a game, settle, and assert the seat matches the engine's
  whose-turn call from § 4.2. Then drive a turn change and assert the seat
  moved. Hearts: settle onto a finished trick and assert -1. The fixture
  isolates the seat bookkeeping in `refresh()`.
  *Breaks when:* a view keeps lighting the last mover through a pause, or
  lights `m_human` whatever the turn.

- **INV-2** — A light reaches level 1, and the view then asks for no repaints.
  *Test:* `tests/uitest.cpp`, block `turnLightComesUpAndHolds`. After a turn
  change, pump events until `turnLight().level` is 1, failing after
  `kTurnLightFadeMs` plus a margin. Then count paint events with
  `PaintCounter` over 200 ms on a view waiting for the player, and assert
  zero. The fixture isolates the fade's stop condition.
  *Breaks when:* the fade pulses, recomputes from a clock, or never stops its
  timer.

- **INV-3** — A deactivated view's picture does not change while its light is
  part-way up.
  *Test:* `tests/uitest.cpp`, block `turnLightFreezesWhenLeft`. Change the
  turn, deactivate at once, and take three renders 25 ms apart; assert all
  three are identical and `turnLight().level` is below 1. The fixture isolates
  the stepped level: `everyGameAnswersTheSwitch` settles first, so it never
  deactivates mid-fade.
  *Breaks when:* the level is worked out from elapsed time, or
  `deactivate()` leaves `m_turnTimer` running.

- **INV-4** — `hasPendingAnimation()` is true while a light is part-way up.
  *Test:* `tests/uitest.cpp`, inside `turnLightComesUpAndHolds`: immediately
  after the turn change, assert it is true; after the level reaches 1, assert
  it is false on a view with no flights. The fixture isolates the § 4.4
  addition.
  *Breaks when:* the fade is left out of `hasPendingAnimation()`, so a
  `--shot` photographs a half-lit seat.

- **INV-5** — The legibility switch changes the light, and turning it back
  restores the light exactly.
  *Test:* `tests/uitest.cpp`, block `turnLightAnswersTheSwitch`. At level 1,
  render the light's area with the switch off, on, and off again; assert the
  first two differ and the first and third are identical. The fixture crops to
  the light's area, because other parts of every game already change with the
  switch.
  *Breaks when:* `paintTurnLight` ignores `legible`, or `applyLegibility()`
  touches the fade state.

- **INV-6** — The light is not colour alone: at level 1 its outline is drawn.
  *Test:* `tests/uitest.cpp`, inside `turnLightAnswersTheSwitch`: paint
  `Theme::paintTurnLight` alone onto a transparent image over a known
  rectangle, and assert pixels on that rectangle's border are opaque while
  pixels just inside the gradient's fade are not. The fixture isolates the
  outline from the gradient.
  *Breaks when:* the painter draws the gradient only.

## 6. Failure modes

- **The turn passes faster than the fade.** A computer seat that moves within
  `kTurnLightFadeMs` restarts the fade for the next seat. The light jumps
  rather than blending. Acceptable: Hearts, Canasta and the board games each
  pause before a computer move today.
- **The window is resized mid-fade.** The area is recomputed at paint time
  from the view's own rect helpers, so the light follows.
- **The game ends mid-fade.** `refresh()` finds nobody's turn and clears both
  fields; the timer stops on its next tick.
- **A band would overlap the caption.** § 4.5 forbids it. A board game whose
  frame leaves no room above the caption band shrinks the light's band rather
  than drawing over the caption.
- **Canasta's `applyLegibility` clears `m_flights`.** It leaves the light
  alone (§ 4.4), so a switch mid-deal does not restart the fade.

## 7. Tests

All in `tests/uitest.cpp`, under the offscreen platform.

- `turnLightFollowsTheTurn` — INV-1.
- `turnLightComesUpAndHolds` — INV-2 and INV-4.
- `turnLightFreezesWhenLeft` — INV-3.
- `turnLightAnswersTheSwitch` — INV-5 and INV-6.

Each is to be seen failing before the feature exists. The accessor lands
first, returning the base class's "nobody" for every view, and INV-1's block
goes red against it.

`everyGameAnswersTheSwitch` and `gamesStopTheirClocks` already run over these
five views and must stay green.

## 8. Alternatives considered (and rejected)

- **A pulsing light.** It repaints every frame for as long as the turn lasts,
  turning a still screen into a busy one. That undoes GHUB-0046's work by
  another route.
- **A real blur** (`QGraphicsBlurEffect`). A blur per frame costs far more than
  one gradient, and `Theme::paintDropShadow` already records that a faked blur
  is invisible at this size.
- **More words, in the status bar or the caption.** They still have to be
  read, and the owner does not look at the status bar during play.
- **`QVariantAnimation`.** Nothing in `src/` uses one, and it advances from a
  clock, so a stopped game's picture could change between renders.
- **Fading the old light out while the new one fades in.** It needs a level per
  seat. The arriving light is what draws the eye.
- **Tinting the pieces of the side to move.** He reads the board by piece
  shape; tinting every piece changes what he reads rather than adding a cue.

## 9. Out of scope

- Turning the light off from a settings screen — deferred; not yet queued.
  GHUB-0068 is where such a control would live.
- A light for single-player games with no turns — nothing to light.
- Showing progress while the computer thinks, beyond the light on its seat —
  deferred; not yet queued.

## 10. What checks this

| Rule | What catches a breach |
|------|----------------------|
| INV-1 | `tests/uitest.cpp` block `turnLightFollowsTheTurn`, an offscreen UI test |
| INV-2 | `tests/uitest.cpp` block `turnLightComesUpAndHolds`, an offscreen UI test |
| INV-3 | `tests/uitest.cpp` block `turnLightFreezesWhenLeft`, an offscreen UI test |
| INV-4 | `tests/uitest.cpp` block `turnLightComesUpAndHolds`, an offscreen UI test |
| INV-5 | `tests/uitest.cpp` block `turnLightAnswersTheSwitch`, an offscreen UI test |
| INV-6 | `tests/uitest.cpp` block `turnLightAnswersTheSwitch`, an offscreen UI test |
| A leaving game stops `m_turnTimer` | Partial: `tests/uitest.cpp` block `gamesStopTheirClocks`, an offscreen UI test, asserts no timer runs after leaving. It sees a fade left running only when the game was left part-way through one |
| The band never overlaps the caption band | **nothing mechanical** — whoever implements it reads a `--shot` of each board game with the switch on |
| The owner can tell whose turn it is at a glance | **nothing mechanical** — the owner, playing |

## 11. Cross-doc impact

- `docs/design.md` § The game contract — `turnLight()`, and the fade inside
  `hasPendingAnimation()`.
- `docs/design.md` § Canasta — the static glow is replaced.
- `CHANGELOG.md` — one Added entry.
- `ROADMAP.md` — GHUB-0063's body describes Canasta as text-only; annotate it
  when the spec is accepted.

## 12. Cold-eyes loop log

Rows live in `../reviews/GHUB-0063-turn-light-loop-log.md`.

## 13. Resource cost

Four new `QTimer`s, one per view outside Canasta, each stopped once its light
is up. One `QRadialGradient` per paint of a lit seat. No new dependency.

## 15. Open questions

For the owner, since each is a preference rather than a deduction:

- **Where the board games' light goes.** This spec puts yours on the bottom
  edge and the computer's on the top, because that is where you sit.
- **How long the fade takes.** § 4.1 sets `kTurnLightFadeMs`: long enough to
  draw the eye, short enough to finish before the quickest computer move.
- **Whether the old light should fade out** rather than go out at once.
