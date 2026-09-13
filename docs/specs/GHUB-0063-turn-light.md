# GHUB-0063 — Light whoever is playing

**Status:** spec draft (2026-09-13).
**Kind:** implement.
**Source:** ROADMAP GHUB-0063 (the owner's suggestion, 2026-08-20).

**Pairs with:** GHUB-0047.

**Layman:** a soft gold light settles on whoever is playing, so you can see
whose turn it is without reading anything.

## 1. Goal

In Hearts, Canasta, Chess, Draughts and Reversi, the seat or side whose turn
it is carries a soft gold light with a gold outline. When the turn passes
during play, the new light fades in once and then holds still. It is stronger
when the legibility switch is on. A game at rest still asks for no repaints.

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

The calls in § 4 that are this spec's rather than the owner's are listed in
§ 15 for the owner to confirm.

## 4. Design

### 4.1 The shared painter

In `src/theme.h`, beside `Theme::paintDropShadow`:

```cpp
// How long the light takes to come up when the turn passes.
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
| `HeartsView` | 0 to 3, the engine's | `m_engine.currentPlayer()` while Playing; 0 while Passing |
| `CanastaView` | 0 to 3, the engine's | `m_engine.currentSeat()` |
| `ChessView` | 0 you, 1 the computer | `m_game.toMove()` against `m_human` |
| `DraughtsView` | 0 you, 1 the computer | `m_toMove` against `m_human` |
| `ReversiView` | 0 you, 1 the computer | `m_toMove` against `m_human` |

**Passing lights you.** Your three cards are the move being waited on:
`HeartsView::confirmPass` chooses the computers' passes at the moment you
confirm yours.

**Nobody's turn** is:

- in every game, a game that is over;
- in Hearts, a hand that is over (`HandOver`), and a trick that is complete —
  `m_engine.trickComplete()` or `m_awaitingCollect`;
- in Canasta, any phase but Draw and Play.

Hearts has to read `trickComplete()` as well as the flag. `step()` calls
`refresh()` after the card that completes a trick and only then sets
`m_awaitingCollect`, so the flag alone is never set when `refresh()` looks.

### 4.3 The fade

Each of the five views holds:

```cpp
int m_turnSeat = -1;
double m_turnLevel = 0.0;
bool m_turnFades = false;   // true between activate() and deactivate()
```

Each view recomputes the lit seat in `refresh()`, by § 4.2's rules.

- **A different seat, while `m_turnFades` is true:** `m_turnSeat` takes the
  seat and `m_turnLevel` starts at 0, so the light fades in.
- **A different seat, while it is false:** the level is set to 1 at once. A
  game opened, restored or photographed shows its light without waiting.
- **Nobody's turn:** both fields go to their defaults at once.

The previous seat's light goes out at once; only an arriving light fades.

The level is **stepped, never clocked**. Each tick adds its own length in
milliseconds divided by `kTurnLightFadeMs`, and stops at 1. Canasta's `kTick`
is in seconds, so its step is `kTick * 1000 / kTurnLightFadeMs`. A level worked
out from elapsed time would change between two renders of a stopped game,
which `everyGameAnswersTheSwitch` in `tests/uitest.cpp` fails as restless.

- **Hearts, Chess, Draughts and Reversi** get a `QTimer* m_turnTimer` at 16 ms.
  It stops itself when the level reaches 1 or nobody's turn clears the fields,
  as `KlondikeView`'s `m_flightTimer` stops itself.
- **Canasta** rides its existing 16 ms `m_timer`. `tick()` advances the level
  and sets `redraw` only while the level is below 1, as it does for
  `m_celebrate`.

Canasta's `animating()` stays flights only, so the fade never delays a
computer turn.

### 4.4 The game contract

- **`hasPendingAnimation()` does not report the fade.** The fade can be stopped
  and picked up where it was, and `GameView::hasPendingAnimation`'s own comment
  says such an animation answers false. Nothing in the app waits on it either:
  `--shot` processes events and grabs. § 4.3's full-level rule is what keeps a
  shot lit.
- `activate()` sets `m_turnFades` and restarts `m_turnTimer` when a fade is
  part-way. `deactivate()` clears `m_turnFades`, stops `m_turnTimer`, and
  leaves the level where it is. It freezes, per `docs/design.md` § The game
  contract.
- `applyLegibility()` does not touch `m_turnSeat`, `m_turnLevel` or
  `m_turnFades`. The switch is read live, at paint time, and passed to
  `paintTurnLight` as `legible`.
- `CanastaView::advanceForShot` sets `m_turnLevel` to 1 after its final
  `refresh()`, immediately before returning. That `refresh()` sees the seat the
  turns moved to, and would otherwise start a fade the shot never shows.
- No save format changes. The light is not saved.

### 4.5 Where each light is painted

Each view paints the light before the cards or pieces over it.

- **Hearts:** seats 1 to 3 over `opponentStackRect(seat)`; seat 0 over the
  smallest rectangle holding every `handCardRect(i)` of the hand. Each area is
  grown by a quarter of `cardWidth()` on every side, so the gradient and the
  outline show in the margin around the cards. The gold label colour stays.
- **Canasta:** the static glow in `paintTable` is replaced. The area is the
  square centred on `seatAnchor(seat)` with half-width `cardHeight() * 1.5`,
  clipped to `tableRect()`. The active plate's stronger edge in
  `paintOpponents` stays.
- **Chess, Draughts, Reversi:** each `boardRect()` reserves a strip
  `kFrameWidth` deep outside the frame, above and below:

  ```cpp
  const int available = std::min(width(), height() - band - 2 * kFrameWidth)
                        - 2 * (kFrameWidth + 4);
  ```

  Your band fills the strip below the frame's bottom edge; the computer's fills
  the strip above its top edge. Each is as wide as the frame. So a band never
  covers the frame, Chess's file letters on it, or the caption band
  `GameView::captionBand` reserves.

## 5. Invariants

These clauses cannot run yet; the surfaces they test are what this spec
creates. Each names the rule its fixture isolates.

- **INV-1** — While someone can move, `turnLight().seat` is that seat. While
  nobody can, it is -1.
  *Test:* `tests/uitest.cpp`, block `turnLightFollowsTheTurn`. For each of the
  five views: open a game, settle, and assert the seat matches § 4.2. Then drive
  a turn change and assert the seat moved. Hearts: in Passing assert 0; play
  onto a complete trick and assert -1. The fixture isolates § 4.2's rules as
  `refresh()` applies them.
  *Breaks when:* a view keeps lighting the last mover through a pause, lights a
  stale seat during Passing, or lights `m_human` whatever the turn.

- **INV-2** — A light reaches level 1, and the view then asks for no repaints.
  *Test:* `tests/uitest.cpp`, block `turnLightComesUpAndHolds`. Activate the
  view, drive a turn change, and pump events until `turnLight().level` is 1,
  failing after `kTurnLightFadeMs` plus a margin. Then count paint events with
  `PaintCounter` over 200 ms on a view waiting for the player, and assert
  zero. The fixture isolates the fade's stop condition.
  *Breaks when:* the fade pulses, recomputes from a clock, or never stops its
  timer.

- **INV-3** — A deactivated view's picture does not change while its light is
  part-way up.
  *Test:* `tests/uitest.cpp`, block `turnLightFreezesWhenLeft`. Activate the
  view and change the turn. Pump one tick and assert `turnLight().seat` is 0 or
  more and its level is strictly between 0 and 1. Deactivate at once, take
  three renders 25 ms apart, and assert all three are identical and the level
  has not moved. The fixture isolates the stepped level:
  `everyGameAnswersTheSwitch` settles first, so it never deactivates mid-fade.
  *Breaks when:* the level is worked out from elapsed time, or
  `deactivate()` leaves `m_turnTimer` running.

- **INV-4** — A seat set while the view is not active is lit at level 1 at
  once.
  *Test:* `tests/uitest.cpp`, block `turnLightStartsLitWhenNotActive`. For each
  of the five views, construct it and start a game without calling
  `activate()`; assert `turnLight().seat` is 0 or more and its level is 1. The
  fixture isolates the `m_turnFades` branch of § 4.3.
  *Breaks when:* the fade starts whatever `m_turnFades` says, so a game opened
  or a `--shot` shows no light.

- **INV-5** — The legibility switch changes the light, and turning it back
  restores the light exactly.
  *Test:* `tests/uitest.cpp`, block `turnLightAnswersTheSwitch`. Paint
  `Theme::paintTurnLight` alone onto a transparent image at level 1, with
  `legible` false and then true, and assert the two images differ. Then, for
  each view at level 1, render with the switch off, on, and off again, and
  assert the first and third renders are identical. The first half isolates
  `legible`, because every view's layout already moves with the switch.
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
- **A turn passes between `activate()` and a `--shot` grab.** That seat is
  photographed part-way up, because `--shot` does not wait (§ 4.4).
- **The window is resized mid-fade.** The area is recomputed at paint time
  from the view's own rect helpers, so the light follows.
- **The game ends mid-fade.** `refresh()` finds nobody's turn and clears both
  fields; the timer stops on its next tick.
- **Canasta's `applyLegibility` clears `m_flights`.** It leaves the light
  alone (§ 4.4), so a switch mid-deal does not restart the fade.

## 7. Tests

All in `tests/uitest.cpp`, under the offscreen platform.

- `turnLightFollowsTheTurn` — INV-1.
- `turnLightComesUpAndHolds` — INV-2.
- `turnLightFreezesWhenLeft` — INV-3.
- `turnLightStartsLitWhenNotActive` — INV-4.
- `turnLightAnswersTheSwitch` — INV-5 and INV-6.

Each is to be seen failing before the feature exists. The accessor lands
first, returning the base class's "nobody" for every view, and the INV-1,
INV-3 and INV-4 blocks go red against it.

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
- **Making `--shot` wait for the fade.** The app deliberately never waits on an
  animation (`GameView::hasPendingAnimation`'s comment), and lighting a seat at
  full when the view is not active needs no waiting.
- **Laying the board games' band on the frame.** It would cover Chess's file
  letters with gold on gold, on the one board he reads by its notation.

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
| INV-4 | `tests/uitest.cpp` block `turnLightStartsLitWhenNotActive`, an offscreen UI test |
| INV-5 | `tests/uitest.cpp` block `turnLightAnswersTheSwitch`, an offscreen UI test |
| INV-6 | `tests/uitest.cpp` block `turnLightAnswersTheSwitch`, an offscreen UI test |
| A leaving game stops `m_turnTimer` | Partial: `tests/uitest.cpp` block `gamesStopTheirClocks`, an offscreen UI test, asserts no timer runs after leaving. It sees a fade left running only when the game was left part-way through one |
| A board game's band stays off its frame and the caption band | **nothing mechanical** — whoever implements it reads a `--shot` of each board game with the switch on |
| The owner can tell whose turn it is at a glance | **nothing mechanical** — the owner, playing |

## 11. Cross-doc impact

- `docs/design.md` § The game contract — `turnLight()`, and that the fade is
  not reported by `hasPendingAnimation()`.
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

- **Where the board games' light goes.** This spec puts yours below the board
  and the computer's above it, because that is where you sit. The board gives
  up a frame's width top and bottom to make the room.
- **How long the fade takes.** § 4.1 sets `kTurnLightFadeMs`: long enough to
  draw the eye, short enough to finish before the quickest computer move.
- **Whether the old light should fade out** rather than go out at once.
- **What lights while you choose cards to pass.** This spec lights you.
- **Whether opening a game should fade its light in** rather than show it at
  once.
