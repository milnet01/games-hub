# GHUB-0063 — Light whoever is playing

**Status:** accepted (2026-09-13). Amended 2026-09-21 with the owner's answers
to § 15 — the light now crosses over and is shaped to its area. No review gate
on the amendment, at the owner's instruction.
**Kind:** implement.
**Source:** ROADMAP GHUB-0063 (the owner's suggestion, 2026-08-20).

**Pairs with:** GHUB-0047.

**Layman:** a soft gold light settles on whoever is playing, so you can see
whose turn it is without reading anything.

## 1. Goal

In Hearts, Canasta, Chess, Draughts and Reversi, the seat or side whose turn
it is carries a soft gold light with a gold outline. When the turn passes
during play, the leaving light fades out as the arriving one fades in, once,
and the table then holds still. It is stronger when the legibility switch is
on. A game at rest still asks for no repaints.

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

- **The light crosses over rather than jumping, and it is big** — the owner,
  2026-09-21 (§ 15).

The calls in § 4 that were this spec's rather than the owner's went to him on
2026-09-21. § 15 records what he said.

## 4. Design

### 4.1 The shared painter

In `src/theme.h`, beside `Theme::paintDropShadow`:

```cpp
// How long an arriving light takes to come up, and a leaving one to go out.
inline constexpr int kTurnLightFadeMs = 300;

// How deep a board game's band is, outside the frame. Deeper than any
// board's kFrameWidth, so the light reads as a light rather than as a
// second frame.
inline constexpr int kTurnBandDepth = 26;

// A soft light filling `area`: one radial gradient in kGold shaped to the
// ellipse inscribed in `area`, plus a gold outline of that ellipse so the
// cue survives any colour vision. Both scale with `level` (0 to 1).
// `legible` raises the gradient's peak and doubles the outline width. Hands
// the painter back as it found it.
void paintTurnLight(QPainter& p, const QRectF& area, double level, bool legible);
```

One gradient, never a computed blur, on the precedent of
`Theme::paintDropShadow`. A `level` of 0 draws nothing.

**The light is shaped to its area, not drawn as a circle inside it.** The
painter translates to the area's centre, scales by its half-width and
half-height, and draws a unit radial gradient. An oblong area therefore gives
an ellipse and a square one a circle, and the outline is that same ellipse.
The owner asked for a bigger light and for a shape other than a small circle
(2026-09-21); § 4.5 gives each view an area worth filling, and this is what
fills it.

**The pool is flat most of the way out and then falls off**, rather than
peaking at the centre. On a card game the centre of the area is under the
cards, so a centre-peaked gradient hides its brightest part and shows only its
faintest — which photographs as a wireframe oval rather than as a light.
Settled by taking the shot before and after.

### 4.2 What each view reports

In `src/gameview.h`:

```cpp
struct TurnLight {
    int seat = -1;              // -1 when it is nobody's turn
    double level = 0.0;         // 0 to 1: how far the arriving light has come
    int leavingSeat = -1;       // -1 when no light is on its way out
    double leavingLevel = 0.0;  // 1 down to 0: what is left of it
};

// Whose turn is lit, how far its light has come up, and whose light is
// still going out behind it. The default is "nobody", which is right for
// every game without turns.
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

### 4.3 The cross-fade

The state and its stepping live in ONE place — `TurnLightState`, in
`src/gameview.h` beside `TurnLight`. Each of the five views holds one of
those plus its own `m_turnFades`:

```cpp
TurnLightState m_turn;
bool m_turnFades = false;   // true between activate() and deactivate()
```

`TurnLightState` carries the four fields below and the rules that move them.
Five copies of identical stepping is what `coding.md` § 1.3 rules out, and all
a view supplies is which seat § 4.2's rules light and how long its own tick
was.

```cpp
int seat = -1;
double level = 0.0;
int leavingSeat = -1;
double leavingLevel = 0.0;
```

Each view recomputes the lit seat in `refresh()`, by § 4.2's rules.

- **A different seat, while `m_turnFades` is true:** the seat being replaced
  moves to `m_leavingSeat` keeping the level it had reached, and `m_turnSeat`
  takes the new seat at level 0. The two then cross — one rising, one
  falling.
- **A different seat, while it is false:** `m_turnSeat` takes the seat at
  level 1 and the leaving fields go to their defaults. A game opened,
  restored or photographed shows its light without waiting. The owner's call,
  2026-09-21.
- **Nobody's turn, while `m_turnFades` is true:** the lit seat moves to
  `m_leavingSeat` and goes out with nothing arriving behind it.
- **Nobody's turn, while it is false:** all four fields go to their defaults
  at once.

**A leaving light never becomes the arriving one.** If the turn comes back to
`m_leavingSeat` mid-cross, that seat restarts as `m_turnSeat` from level 0 and
the leaving fields clear. One leaving slot is enough: a second turn change
replaces the first light before it has finished going out, so two are never
leaving at once.

The owner chose the cross-fade (2026-09-21) over the leaving light going out
at once. § 8 keeps the argument against it, which stands and was overruled.

The levels are **stepped, never clocked**. Each tick adds its own length in
milliseconds divided by `kTurnLightFadeMs` to the arriving level and takes the
same off the leaving one, stopping at 1 and at 0. Canasta's `kTick` is in
seconds, so its step is `kTick * 1000 / kTurnLightFadeMs`. A level worked out
from elapsed time would change between two renders of a stopped game, which
`everyGameAnswersTheSwitch` in `tests/uitest.cpp` fails as restless.

- **Hearts, Chess, Draughts and Reversi** get a `QTimer* m_turnTimer` at 16 ms.
  It stops itself once nothing is still moving: the arriving light has reached
  1 or there is none, and the leaving light has reached 0. `KlondikeView`'s
  `m_flightTimer` stops itself the same way.
- **Canasta** rides its existing 16 ms `m_timer`. `tick()` advances both levels
  and sets `redraw` while either is still moving, as it does for
  `m_celebrate`.

Canasta's `animating()` stays flights only, so the cross-fade never delays a
computer turn.

### 4.4 The game contract

- **`hasPendingAnimation()` does not report the fade.** The fade can be stopped
  and picked up where it was, and `GameView::hasPendingAnimation`'s own comment
  says such an animation answers false. Nothing in the app waits on it either:
  `--shot` processes events and grabs. § 4.3's full-level rule is what keeps a
  shot lit.
- `activate()` sets `m_turnFades` and restarts `m_turnTimer` when either level
  is part-way. `deactivate()` clears `m_turnFades`, stops `m_turnTimer`, and
  leaves both levels where they are. It freezes, per `docs/design.md` § The
  game contract.
- `applyLegibility()` touches none of the five fields. The switch is read
  live, at paint time, and passed to `paintTurnLight` as `legible`.
- `CanastaView::advanceForShot` sets `m_turnLevel` to 1 and clears the leaving
  fields after its final `refresh()`, when `m_turnSeat` is 0 or more. That
  `refresh()` sees the seat the turns moved to, and would otherwise start a
  cross-fade the shot catches half-way. A run that ends between hands or at
  game over stays unlit.
- No save format changes. The light is not saved.

### 4.5 Where each light is painted

Each view paints the light before the cards or pieces over it. Every area
below is oblong, so every light is an ellipse (§ 4.1).

- **Hearts:** seats 1 to 3 over `opponentStackRect(seat)`; seat 0 over the
  smallest rectangle holding every `handCardRect(i)` of the hand. Each area is
  grown by **half of `cardWidth()`** on every side and then clipped to the
  view's own rect, so the light spreads well past the cards and still cannot
  run off the table. The gold label colour stays.
- **Canasta:** the static glow in `paintTable` is replaced. The area is the
  square centred on `seatAnchor(seat)` with half-width `cardHeight() * 2.2`,
  clipped to `tableRect()`. Clipping an edge seat's square against the table
  is what makes its light elliptical. The active plate's stronger edge in
  `paintOpponents` stays.
- **Chess, Draughts, Reversi:** each `boardRect()` reserves a strip
  `Theme::turnBandDepth(room)` deep outside the frame, above and below:

  ```cpp
  const int depth = Theme::turnBandDepth(height() - band);
  const int available = std::min(width(), height() - band - 2 * depth)
                        - 2 * (kFrameWidth + 4);
  ```

  **The ellipse is three times that depth, positioned so exactly `depth` of it
  shows outside the frame and the rest is hidden behind the board.** The hidden
  part is what does the work: it gives the visible arc the gentle curvature of
  a big light while costing the board only the strip that shows. A flat lens
  the depth of the strip photographed as a drawn line.

  Your band shows below the frame's bottom edge; the computer's above its top
  edge. Each is wider than the frame by `depth` on each side. A band never
  covers the frame or the caption band `GameView::captionBand` reserves.

  **Chess spends an extra `kFrameWidth` BELOW only**, clearing the file
  letters' ink, and reserves it symmetrically so the board stays centred
  between the two bands. The rank numbers are drawn to the left, so the top
  needs no such room. The allowance is fixed rather than measured, so a board
  far larger than a desktop window offers would eventually reach it.

  **What this costs:** the board gives up `2 * depth` of height, plus
  `kFrameWidth` in Chess. At 900x760 a Chess board goes from 712 to about 616
  pixels — roughly 13%. That is the price of § 8's rejection of laying the band
  ON the frame, and it is a real one for a player who reads the board slowly.

  **Chess's file letters overhang the frame.** They are drawn with
  `Qt::TextDontClip`, and under the switch at larger board sizes their ink runs
  past the frame's bottom edge. Chess's lower band starts below that ink, and
  the strip Chess reserves grows by the overhang.

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

- **INV-2** — The cross-fade finishes — arriving light at 1, leaving light
  gone — and the view then asks for no repaints.
  *Test:* `tests/uitest.cpp`, block `turnLightComesUpAndHolds`. Show and
  activate the view, install a `PaintCounter`, and drive a turn change that
  lands on your seat, which then waits. Pump events until `turnLight().level`
  is 1 and its `leavingSeat` is -1, failing after `kTurnLightFadeMs` plus a
  margin, and assert the counter saw at least one paint during the cross-fade.
  Then count paint events over 200 ms and assert zero. On the four views with
  `m_turnTimer`, also assert `activeTimers(view)` is zero. The positive
  control is what makes the zero mean anything. The fixture isolates the
  cross-fade's stop condition.
  *Breaks when:* either light pulses, recomputes from a clock, or the timer
  stops on the arriving light alone and freezes the leaving one part-way.

- **INV-3** — A deactivated view's picture does not change while its light is
  part-way up.
  *Test:* `tests/uitest.cpp`, block `turnLightFreezesWhenLeft`. Activate the
  view and change the turn. Pump one tick and assert `turnLight().seat` is 0 or
  more and its level is strictly between 0 and 1. Deactivate at once, take
  three renders 25 ms apart, and assert all three are identical and that
  neither the arriving nor the leaving level has moved. The fixture isolates
  the stepped levels:
  `everyGameAnswersTheSwitch` settles first, so it never deactivates mid-fade.
  *Breaks when:* the level is worked out from elapsed time, or
  `deactivate()` leaves `m_turnTimer` running.

- **INV-4** — A seat set while the view is not active is lit at level 1 at
  once, with nothing leaving.
  *Test:* `tests/uitest.cpp`, block `turnLightStartsLitWhenNotActive`. For each
  of the five views, construct it and start a game without calling
  `activate()`; assert `turnLight().seat` is 0 or more, its level is 1, and its
  `leavingSeat` is -1. The fixture isolates the `m_turnFades` branch of § 4.3.
  *Breaks when:* the cross-fade starts whatever `m_turnFades` says, so a game
  opened or a `--shot` shows no light or a half-lit one.

- **INV-5** — The legibility switch changes the light, and leaves its state
  alone.
  *Test:* `tests/uitest.cpp`, block `turnLightAnswersTheSwitch`. Paint
  `Theme::paintTurnLight` alone onto a transparent image at level 1, with
  `legible` false and then true, and assert the two images differ. Then, for
  each view, activate it, change the turn, pump one tick so the level is
  part-way, and deactivate. Turn the switch on and off again. Assert
  `turnLight()`'s seat and level are unchanged, and that the renders before and
  after are identical. The first half isolates `legible`, because every view's
  layout already moves with the switch; the second isolates the fade state,
  which a level already at 1 would hide.
  *Breaks when:* `paintTurnLight` ignores `legible`, or `applyLegibility()`
  touches the fade state.

- **INV-6** — The light is not colour alone: at level 1 its outline is drawn,
  and it is the ellipse inscribed in the area rather than the area's border.
  *Test:* `tests/uitest.cpp`, inside `turnLightAnswersTheSwitch`: paint
  `Theme::paintTurnLight` alone onto a transparent image over a known oblong
  rectangle. Assert a pixel on the ellipse — the midpoint of the rectangle's
  top edge — is more opaque than one just inside it, where the gradient fades.
  Assert a corner pixel of the rectangle is fully transparent, which no
  rectangular outline could leave bare. The fixture isolates the outline from
  the gradient, and the shape from the area.
  *Breaks when:* the painter draws the gradient only, or outlines the
  rectangle instead of the ellipse.

- **INV-7** — When the turn passes during play, both lights are up at once and
  the leaving one is on its way down.
  *Test:* `tests/uitest.cpp`, block `turnLightCrossesOver`. Activate the view,
  settle so the lit seat is at level 1, then drive one turn change. Pump a
  single tick and assert `leavingSeat` is the seat that was lit, `seat` is the
  new one, and both levels are strictly between 0 and 1. Pump another tick and
  assert the leaving level has fallen and the arriving level has risen. The
  fixture isolates § 4.3's hand-over: settling first is what makes the leaving
  level start at 1, so a fall is unambiguous.
  *Breaks when:* the leaving light is dropped at once rather than faded, or
  both fields track the same level.

## 6. Failure modes

- **The turn passes faster than the cross-fade.** A computer seat that moves
  within `kTurnLightFadeMs` hands its part-grown light straight to the leaving
  slot, dropping whatever was already there. Two lights are lost mid-travel
  and the third arrives from 0. Acceptable: Hearts, Canasta and the board
  games each pause before a computer move today.
- **A turn passes between `activate()` and a `--shot` grab.** Both seats are
  photographed part-way, because `--shot` does not wait (§ 4.4).
- **The window is resized mid-cross.** Both areas are recomputed at paint time
  from the view's own rect helpers, so the lights follow.
- **The game ends mid-cross.** `refresh()` finds nobody's turn, so the lit
  seat starts leaving and the arriving fields clear; the timer stops once the
  leaving level reaches 0.
- **Canasta's `applyLegibility` clears `m_flights`.** It leaves both lights
  alone (§ 4.4), so a switch mid-deal does not restart a cross-fade.

## 7. Tests

All in `tests/uitest.cpp`, under the offscreen platform.

- `turnLightFollowsTheTurn` — INV-1.
- `turnLightComesUpAndHolds` — INV-2.
- `turnLightFreezesWhenLeft` — INV-3.
- `turnLightStartsLitWhenNotActive` — INV-4.
- `turnLightAnswersTheSwitch` — INV-5 and INV-6.
- `turnLightCrossesOver` — INV-7.

Each is to be seen failing before the feature exists. The accessor lands
first, returning the base class's "nobody" for every view, and the INV-1,
INV-3, INV-4 and INV-7 blocks go red against it.

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
- **~~Fading the old light out while the new one fades in.~~ Overruled by the
  owner, 2026-09-21, and now § 4.3.** The argument was that it needs a second
  level and that the arriving light is what draws the eye. The second level
  turned out to be one field, not one per seat, because only one light is ever
  leaving. The owner wants the turn to read as passing rather than jumping.
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
| INV-7 | `tests/uitest.cpp` block `turnLightCrossesOver`, an offscreen UI test |
| A leaving game stops `m_turnTimer` | Partial: `tests/uitest.cpp` block `gamesStopTheirClocks`, an offscreen UI test, asserts no timer runs after leaving. It sees a fade left running only when the game was left part-way through one |
| A board game's band stays off its frame, the caption band and Chess's file letters | **nothing mechanical** — whoever implements it reads a `--shot` of each board game with the switch on, Chess at a large window |
| INV-1's game-over limb — a finished game lights nobody | **nothing mechanical**. Reaching a finished Chess, Hearts or Canasta means playing one out, which costs more than this suite spends on any check. `turnLightFollowsTheTurn` covers the -1 rule in the form that is cheap — every game with no turns answers it — and the game-over limb rests on reading `refresh()` |
| A test clicking a board square reads the view's own `boardRect()` | **nothing mechanical**. Three inline copies of that arithmetic went stale on this change and reddened eleven checks; `ChessProbe`, `ReversiProbe` and `DraughtsProbe` in `tests/uitest.cpp` are the replacement, and nothing stops a fourth copy being written |
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

Four new `QTimer`s, one per view outside Canasta, each stopped once nothing
is still moving. One `QRadialGradient` per lit seat per paint, so two while a
cross-fade is running and one the rest of the time. No new dependency.

## 15. Owner's answers

Asked and answered 2026-09-21. Nothing here is open.

- **Where the board games' light goes** — yours below the board, the
  computer's above it. As drafted (§ 4.5).
- **How long the fade takes** — `kTurnLightFadeMs` as drafted. Chess's
  computer starts its reply `kThinkDelayMs` after its turn begins, which is
  sooner, so in Chess the cross-fade is often cut short (§ 6). Accepted.
- **Whether the old light fades out** — **yes, it cross-fades.** This changed
  the spec: § 4.3 is rewritten, § 4.2 carries a leaving seat and level, INV-2,
  INV-3 and INV-4 name them, INV-7 is new, and § 8 records the overruled
  argument.
- **What lights while you choose cards to pass** — you. As drafted (§ 4.2).
- **Whether opening a game fades its light in** — no, it opens lit. As
  drafted (§ 4.3).

Two further calls the owner made the same day, neither of them among the
questions asked:

- **The light is bigger, and need not be round.** § 4.1 shapes it to the
  ellipse inscribed in its area, and § 4.5 grows every area: Hearts by half a
  card's width on each side, Canasta's half-width from `cardHeight() * 1.5` to
  `cardHeight() * 2.2`, and the board games' band from `kFrameWidth` to the
  new `kTurnBandDepth`.
- **No review gate on this amendment.** The owner asked not to run
  `review-contract` for a change of this size. Recorded here and in the commit
  body, per `CLAUDE.md` § Review history.
