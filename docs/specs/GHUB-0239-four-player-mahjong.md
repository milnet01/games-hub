<!-- ants-spec-format: 1 -->
# GHUB-0239 — Four-player Mahjong, Hong Kong rules, with a rule-set chooser

**Status:** accepted (2026-11-11).
**Kind:** feature.
**Source:** ROADMAP GHUB-0239 (user-request-2026-11-10; the owner's
decisions of 2026-11-11, recorded in § 3).
**Blocker for:** GHUB-0243 (Chinese Official rules), GHUB-0244 (Japanese
Riichi rules).

**Layman:** a new game, Mahjong, where you play the traditional four-seat
table game against three computer players. It plays Hong Kong rules first.
Every scoring feature of a win is written out in words, and a score sheet
keeps the running totals.

## 1. Goal

The hub has a game with the id `Mahjong`, in the Cards tab, beside Canasta.
You sit at the bottom of a four-seat table against three computer
players. Tiles are drawn from a wall, discarded, and claimed to make sets. A
player goes out with four sets and a pair. A win is scored under Hong Kong
rules: each scoring feature is named on the table with its value, and the
points move between the players' running totals. A Rule set menu lists Hong
Kong, remembers the choice across a restart, and is where GHUB-0243 and
GHUB-0244 add their rule sets later. The tiles look like Mahjong Solitaire's.

## 2. Problem

1. **There is no four-player Mahjong.** The only tile game is Mahjong
   Solitaire, `MahjongView` with `mahjong::Board`, a matching puzzle.
2. **The tile art cannot be reused where it stands.** The face drawing —
   `pipPattern`, `paintCirclePip`, `paintBambooPip`, `paintDiamondPip`,
   `paintArrow`, `paintFlame`, `paintTree`, `paintFrame`, `paintFlower`,
   `paintSeason` — sits in an anonymous namespace in
   `src/mahjong/mahjongview.cpp`. `MahjongView::paintTile` mixes it with
   Solitaire's heap geometry: it reads `faceRect(slot)`, `depth()`,
   `mahjong::layout()` and `m_picked`.
3. **The tile identity is tied to the Solitaire board.** `Face`, `Suit`,
   `suitOf`, `rankOf` and `fullSet` live in `src/mahjong/mahjongboard.h` with
   `Board`, `Slot` and `layout()`. A new core including that header gets
   Solitaire's heap with them.
4. **Mahjong has many rule sets, and they differ most in scoring.** The owner
   wants all of them, chosen by the player (§ 3). Hong Kong scoring alone
   varies from table to table. Fan values, the minimum to win and the payment
   table all differ by house. Source:
   <https://en.wikipedia.org/wiki/Hong_Kong_mahjong_scoring_rules>.
   Source: <https://sloperama.com/mjfaq/mjfaq17.html>.
   So the numbers this game plays by must be written down once and checked,
   or they drift from what the rules text tells the player.

## 3. Scope decisions (agreed with the user)

- **Every rule set, chosen by the player, built in stages.** The owner,
  2026-11-11. This item ships Hong Kong and the chooser. Chinese Official is
  GHUB-0243; Japanese Riichi is GHUB-0244.
- **American (NMJL) rules are left out.** Its winning hands come from a card
  the league reprints every year and holds the copyright to. Agreed with the
  owner 2026-11-11.
- **Full scoring.** The owner deferred the choice, 2026-11-11, and took the
  recommendation: each win lists its scoring features in words on the table,
  totals them, and keeps running totals across hands, as Canasta does.
- **The numbers in § 4.4 and § 4.5** — the fan list, the 3-fan minimum, the
  10-fan limit, the points table and who pays — are common Hong Kong
  practice, proposed by the author and confirmed by the owner 2026-11-11.
- **A full game of four winds by default.** The owner, 2026-11-11: "It must
  emulate the full experience." An East round stays in the menu as the
  shorter choice.
- **The Cards tab**, beside Canasta. The owner, 2026-11-11.

## 4. Design

### 4.1 Where the parts live

All in `src/mahjong/`, beside Mahjong Solitaire. New files are named here by
their role; their final names are settled at the build.

| Part | Half | Holds |
|------|------|-------|
| tile identity | core | `Face`, `Suit`, the `k…First` constants, `suitOf`, `rankOf`, `matchGroup`, `matches`, `fullSet`, moved out of `mahjongboard.h`, which then includes it |
| table engine | core | the wall, the deal, turns, claims, kongs, bonus tiles, the move list, hand and game state |
| rule sets | core | the `RuleSet` interface and `HongKongRules` |
| computer player | core | discard and claim choices |
| tile art | view | the face drawing, moved out of `mahjongview.cpp` |
| table view | view | `MahjongTableView`, a `GameView` |

The engine names everything inside `namespace mahjong`, as Solitaire's
board does. `CMakeLists.txt` adds the four core parts to `GAME_CORE_SOURCES`
and the two view parts to `GAME_VIEW_SOURCES`. **Tile names in words** —
"Five of Bamboo", "Red Dragon", "East Wind" — are display text, so they live
in the tile art, not in the core.

### 4.2 Shared tile art

```cpp
namespace mahjong {
// Draws one tile's face — the ivory face and its markings — into `face`.
// The body, edge, shadow and any highlight stay with the caller.
void paintTileFace(QPainter& p, const QRectF& face, Face tile, bool legible);
// The tile in words, for the table's sentences ("Five of Bamboo").
QString tileName(Face tile);
}
```

`MahjongView::paintTile` keeps its body, shadow and picked highlight, and calls
`paintTileFace` for the face. **Mahjong Solitaire must look exactly as it does
today** (INV-12). The move is a cut and paste, not a redraw.

### 4.3 The table engine

**The save is the moves, as Chess's is** (`docs/design.md` § Saves). The wall
is built from a seed, so a hand is its seed plus its move list. A restore
replays every move through the engine and refuses one that is not legal there.

```cpp
namespace mahjong {
enum class Action : std::uint8_t {
    Draw, Discard, Chow, Pung, Kong /* claimed */, ConcealedKong,
    AddedKong, Win, Pass
};
struct Move {
    std::uint8_t seat;
    Action action;
    Face tile;            // the tile discarded, claimed, konged or won on
    std::uint8_t chowLow; // the lowest rank of a chow, else 0
};
}
```

**The turn.** The player to move draws from the front of the wall. A flower or
season drawn is set aside face up and replaced from the back of the wall, at
once and repeatedly. A concealed kong or an added kong is also replaced from
the back. Then the player discards one tile, declares a win, or makes a kong.

**Claims on a discard.** Every other seat may claim it:

| Claim | Who | Needs |
|-------|-----|-------|
| Win | any seat | the tile completes the hand at or above the rule set's minimum |
| Kong | any seat | three of the tile concealed |
| Pung | any seat | two of the tile concealed |
| Chow | the next seat only | two concealed tiles making a run of three in one suit |

**Priority: Win beats Kong and Pung, which beat Chow.** Two Win claims go to
the first seat after the discarder in turn order. After a Pung or Kong claim
the turn jumps to the claimer, skipping any seat between. A claimed set is
laid face up and is no longer concealed.

**Robbing a kong.** When a player adds a drawn tile to their pung, any other
seat whose hand that tile completes may win on it. Only Win may be claimed.

**A winning hand** is four sets (chow, pung or kong) and a pair, or Thirteen
Orphans (§ 4.4). Seven pairs is not a winning hand in Hong Kong rules.

**A drawn hand.** The wall keeps no reserve. When it is empty and the last
discard is not claimed for a win, the hand is drawn. No points move.

**The deal and the dealer.** East deals. Each player starts with thirteen
tiles, and East draws first. The dealer keeps the deal after a win or a drawn
hand. Otherwise the deal passes to the next seat. When all four have dealt and
lost the deal, the prevailing wind moves on, East to South to West to North.

**The game's end.** The game ends after the East round, or after all four
winds, by the length setting (§ 4.6). The highest running total wins, and a
shared top total is a shared win, as `HeartsEngine::winnerIsShared` asks.

### 4.4 The Hong Kong fan list

Fan are additive unless a line says it includes another. A hand needs **3
fan** to win. **10 fan is the limit**: a limit hand scores 10, and no hand
scores more.

| Pattern | Fan |
|---------|-----|
| Self-drawn — won on a tile you drew | 1 |
| Concealed hand — no claimed set, won on a discard | 1 |
| No flowers or seasons | 1 |
| Own flower — the flower numbered for your seat (East 1) | 1 |
| Own season — the season numbered for your seat | 1 |
| All four flowers | 2 |
| All four seasons | 2 |
| All chows — four chows and a pair that scores nothing | 1 |
| Pung or kong of a dragon, each | 1 |
| Pung or kong of your seat wind | 1 |
| Pung or kong of the prevailing wind | 1 |
| Won on the last tile of the wall | 1 |
| Won on a kong's replacement tile | 1 |
| Robbing a kong | 1 |
| All pungs — four pungs or kongs | 3 |
| Mixed one suit — one suit with honours | 3 |
| Small three dragons — two dragon pungs and a dragon pair; includes the two dragon pungs | 5 |
| Pure one suit — one suit, no honours | 7 |
| Big three dragons — three dragon pungs; includes them | 8 |

**Limit hands, each 10:** Small four winds (three wind pungs and a wind pair),
Big four winds (four wind pungs), All honours, All terminals (only ones and
nines), Thirteen orphans (one of each one, nine, wind and dragon, and one more
of any of them), Nine gates (1112345678999 in one suit, concealed, plus any
tile of that suit), Four concealed pungs, Four kongs, Heavenly hand (East wins
on the deal) and Earthly hand (a player wins on East's first discard).

Source for the patterns and their usual values:
<https://en.wikipedia.org/wiki/Hong_Kong_mahjong_scoring_rules>.

### 4.5 Points and who pays

| Fan | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|-----|---|---|---|---|---|---|---|----|
| Points | 8 | 16 | 24 | 32 | 48 | 64 | 96 | 128 |

**Won on a discard: the discarder pays the points**, and nobody else pays.
**Self-drawn: each of the other three pays the points.** Every hand's payments
sum to zero. Running totals start at 0 and may go below it.

### 4.6 The rule-set interface, and the settings

```cpp
namespace mahjong {
enum class RuleSetId : std::uint8_t { HongKong = 1 }; // 2, 3 kept for GHUB-0243, GHUB-0244

enum class Fan : std::uint8_t { /* one entry per § 4.4 line */ };
struct ScoreItem { Fan fan; int value; };
struct HandScore {
    std::vector<ScoreItem> items;
    int total = 0;        // after the limit
    bool limit = false;
    int points = 0;
};
struct WinContext;        // the hand, its sets, how it was won, seat, winds, flowers

class RuleSet {
public:
    virtual ~RuleSet() = default;
    virtual RuleSetId id() const = 0;
    virtual bool chowAllowed() const = 0;
    virtual bool bonusTilesInPlay() const = 0;
    virtual int minimumToWin() const = 0;
    virtual HandScore score(const WinContext& win) const = 0;
    virtual std::array<int, 4> settle(const WinContext& win, const HandScore& s) const = 0;
};
const RuleSet& ruleSet(RuleSetId id);
}
```

**The core names a pattern by `Fan`; the view owns its words.** The view maps
each `Fan` to a `tr()` sentence, so the core carries no display text. The
interface holds what Hong Kong needs. GHUB-0243 and GHUB-0244 extend it where
their rules need more — Riichi's declaration and dora are not designed here.

**Settings**, read and written as `CanastaView` does with `QSettings`:

| Key | Values | Default | When it takes effect |
|-----|--------|---------|----------------------|
| `mahjong4/ruleSet` | a `RuleSetId` | Hong Kong | the next new game |
| `mahjong4/length` | `east`, `full` | `full` | the next new game |

A stored value this build does not know falls back to the default. A change
made mid-game is stored and announced on the table ("Hong Kong rules from the
next game."); the game in progress keeps its rule set, because its running
totals were scored under it. Canasta differs on purpose: its
`CanastaView::applyRules` changes the hand in progress. **The Rule set menu
lists only rule sets this build has.**

### 4.7 The table on screen

- **Your hand** runs along the bottom, face up and sorted by suit and rank,
  with the drawn tile set a little apart. The three computer hands are tile
  backs. Each seat's claimed sets and bonus tiles lie face up in front of it.
- **The discards** lie in the centre in one pool per seat, in the order played.
  **The last discard is drawn larger and spelled out in words under the
  centre** — "West discarded the Five of Bamboo." — as Canasta does.
- **The seat wind of each player, the prevailing wind, the dealer and the tiles
  left in the wall** are written on the table, not only in the status bar.
- **A claim waits for you, with no timeout.** When you can claim a discard, the
  game stops and shows a button for each claim you may make, and Pass, on the
  table beside your hand. When you cannot, play goes on after the computer
  pause. A chow with more than one run asks which run.
- **The end of a hand** shows on the table: the winner's tiles laid out face up,
  then one line per scoring item in words with its value ("Mixed one suit — 3
  fan"), the total, the points, and each seat's payment. A drawn hand says so.
  A Next hand button carries on.
- **The running totals** are painted on the table at all times, as
  `CanastaView::paintScores` paints Canasta's. **A score sheet** listing each
  hand's result opens from a game action; Canasta has no such sheet.
- **The computer's pause** is `kAiPause` in `canastaview.cpp`, 0.95 seconds.
- **Rules** (`rulesText()`) gives how to play and the whole fan list of the
  rule set in force, from the same `Fan` words the end of hand uses.

**Legibility.** `smallestCardWidth()` returns `0.0`: the game draws no cards.
Your own hand and the last discard are drawn at the largest size the window
allows. Pool tiles may be smaller, because the last discard is in words.
`minimumSizeHint()` is set so your hand's tiles stay readable at the smallest
window. With the switch on, the caption and the end-of-hand panel follow
`GHUB-0017-legibility-switch.md`: the switch is read live, at paint time.

**Turn light.** `turnLight()` reports the seat to move, 0 to 3, by the
engine's seats, with 0 being you, per `GHUB-0063-turn-light.md` § 4.2. It
is -1 while a claim is being waited on, between hands and once the game is
over.

**Keyboard.** The view sets `Qt::StrongFocus` (`docs/design.md` § The game
contract). Left and right move along your hand, Return discards, and each
claim button has a key. The keys are chosen at the build. They leave out P,
which waits on a separate owner decision.

**Words.** Every word a player reads goes through `tr()`, per
`GHUB-0161-translatable-text.md`. The game's id `Mahjong` is fixed; its tile
label is translated.

### 4.8 The computer player

One strength in this item. It works out how many tiles each candidate discard
leaves the hand from ready, and throws the one that leaves it closest,
breaking ties by keeping tiles that score — dragons, its own and the
prevailing wind, and the suit it holds most of. It claims a pung or kong of a
scoring tile, or one that brings it closer to ready. It claims a chow only
when it is not chasing All pungs. **It plays toward the 3-fan minimum**: a
hand that would complete below 3 fan keeps drawing rather than declaring. It
always wins when it can.

### 4.9 The hub entry

`HubWindow`'s game list gains `Mahjong` in `Category::Cards`, with the label
`tr("Mahjong")`, the blurb `tr("Four players, Hong Kong rules")` and a tile
painted from the shared tile art. `--game Mahjong` opens it, and its save is
`saved/Mahjong`. `tests/saves/` gains a `Mahjong.bin` through `--write-saves`,
which `.claude/rules/tests.md` permits for adding a game.

## 5. Invariants

These clauses cannot run yet; the surfaces they test are what this spec
creates. Each names the rule its fixture isolates.

- **INV-1** — Every tile is somewhere, once. At every step of a hand, the wall,
  the four hands, the claimed sets, the bonus tiles set aside and the discard
  pools together hold exactly `fullSet()`.
  *Test:* `tests/selftest.cpp`, a block playing seeded four-computer hands and
  checking the count after every move. The fixture isolates the bookkeeping
  rule: every move it plays is legal, so only a tile counted twice or lost can
  fail it.
  *Breaks when:* a claimed discard stays in the pool as well as joining a set.

- **INV-2** — Claims are settled Win, then Kong or Pung, then Chow; two Win
  claims go to the first seat after the discarder.
  *Test:* `tests/selftest.cpp`, a block with set-up hands. Seat 0 discards a
  tile that seat 1 can chow and seat 3 can pung: seat 3 takes it. Seats 2 and 3
  can both win on a discard by seat 0: seat 2 takes it. Each claim is first
  shown legal on its own, so the fixture isolates the priority rule.
  *Breaks when:* claims are settled in seat order, so the chow wins.

- **INV-3** — Only the seat after the discarder may chow.
  *Test:* `tests/selftest.cpp`. Seat 2 holds the two tiles that make a run with
  seat 0's discard and is offered no chow; seat 1 with the same tiles is. Only
  the seat rule differs between the two.
  *Breaks when:* any seat may chow.

- **INV-4** — A complete hand below the minimum cannot win, and Win is not
  offered for it.
  *Test:* `tests/selftest.cpp`. One complete hand of four chows and a pair,
  with a flower not your own: won on a discard it is worth 2 fan (concealed
  hand, all chows) and is refused; with no flower and self-drawn it is worth 3 (self-drawn, concealed
  is not counted on a self-draw, no flowers, all chows) and is accepted. The
  hand is complete in both, so only the minimum rule can refuse it.
  *Breaks when:* completeness alone is taken as a win.

- **INV-5** — `HongKongRules::score` gives each § 4.4 pattern its listed fan,
  applies the includes, and caps the total at 10.
  *Test:* `tests/selftest.cpp`, one fixture hand per § 4.4 line and per limit
  hand, each asserting the `Fan` items and the total; plus a hand holding
  Pure one suit, All pungs and Self-drawn, whose 11 fan is scored 10.
  *Breaks when:* Small three dragons is scored 7 by adding its dragon pungs, or
  a hand scores past the limit.

- **INV-6** — Payments follow § 4.5 and every hand's payments sum to zero.
  *Test:* `tests/selftest.cpp`: a 4-fan win on a discard moves 16 from the
  discarder only; a 4-fan self-drawn win moves 16 from each other seat. The
  seeded four-computer games assert the zero sum after every hand.
  *Breaks when:* a discard win also charges the other two seats.

- **INV-7** — A tile added to a pung can be robbed for a win, and nothing else
  can be claimed on it.
  *Test:* `tests/selftest.cpp`. Seat 1 adds the Five of Circles to its pung;
  seat 2, waiting on it, may claim Win and is offered no Pung; the win carries
  the Robbing a kong item.
  *Breaks when:* an added kong is final, or offers a pung.

- **INV-8** — An empty wall with no win draws the hand: no points move and the
  dealer keeps the deal.
  *Test:* `tests/selftest.cpp`, a seeded hand played to an empty wall with every
  win declined by the fixture.
  *Breaks when:* a drawn hand passes the deal.

- **INV-9** — The deal passes only when the dealer does not win, and the game
  ends after the East round or after four winds, by `mahjong4/length`.
  *Test:* `tests/selftest.cpp`, seeded four-computer games at both lengths,
  asserting the prevailing wind and dealer after each hand and that each game
  ends.
  *Breaks when:* the deal passes after a dealer's win, so an East round ends
  early.

- **INV-10** — A save restores the same table, and a save holding a move that
  is not legal is refused.
  *Test:* `tests/uitest.cpp`: save mid-hand, restore into a fresh view, compare
  `saveState()`; then change one discard in the blob to a tile the seat does not
  hold and assert `restoreState` returns false. The new save in `tests/saves/`
  restores in the corpus check.
  *Breaks when:* a restore trusts the moves without replaying them.

- **INV-11** — The rule set is remembered, an unknown stored value falls back
  to Hong Kong, and the menu lists only rule sets this build has.
  *Test:* `tests/uitest.cpp`: set `mahjong4/ruleSet` to an unknown value, build
  the view, and assert Hong Kong is in force and the menu holds one entry.
  *Breaks when:* the menu lists Chinese Official before GHUB-0243 ships it.

- **INV-12** — Mahjong Solitaire looks exactly as before the tile art moved.
  *Test:* a `--shot` of `--game "Mahjong Solitaire" --seed 1` before the move
  and after it, compared pixel for pixel per `.claude/rules/shots.md`.
  *Breaks when:* the cut and paste changes a colour, a size or an order.

- **INV-13** — The last discard and every scoring line are on the table in
  words.
  *Test:* `tests/uitest.cpp`: after a discard, the view's sentence names the
  seat and the tile by `tileName`; after a win, every `Fan` in the score has a
  non-empty word and appears in the end-of-hand text.
  `python3 scripts/translatable-check.py` exits 0.
  *Breaks when:* a new `Fan` entry has no word, so its line is blank.

- **INV-14** — The turn light follows § 4.7.
  *Test:* `tests/uitest.cpp`'s turn-light block, extended to this view: lit on
  the seat to move, -1 while a claim waits and between hands.
  *Breaks when:* the light stays on the discarder through a claim.

- **INV-15** — Four-computer games end, every win is at or above the minimum,
  and some hands are won.
  *Test:* `tests/selftest.cpp`, seeded four-computer East rounds: each ends;
  each win's total is at least 3; across the seeds at least one hand in five is
  won.
  *Breaks when:* the computer never steers toward 3 fan, so every hand draws.

- **INV-16** — When you can claim, the game waits for you; when you cannot,
  it does not stop.
  *Test:* `tests/uitest.cpp`: a set-up table where your seat can pung a
  discard shows the claim buttons and makes no move however long the timers
  run; Pass resumes play. A table where you cannot claim moves on.
  *Breaks when:* the claim window times out, or a window opens for a claim
  you cannot make, which tells you nothing is coming.

## 6. Failure modes

- **A rule-set value from a later build** — a saved game or setting naming
  Chinese Official read by this build: the setting falls back (INV-11); the
  save is refused, and the hub keeps the fresh game it made.
- **The wall's seed and the moves disagree** — a hand-edited save: the replay
  meets an illegal move and refuses the whole save (INV-10).
- **The computer cannot find a legal discard** — cannot happen: a hand after a
  draw always holds fourteen tiles, and any one may be thrown.
- **No seat can ever win** — the minimum makes many complete hands worthless.
  INV-15 is what notices: a computer that ignores fan draws every hand.
- **A small window** — the hand's tiles shrink. `minimumSizeHint()` stops the
  window before they stop reading; the pool may go smaller, and the last
  discard is in words.
- **The owner changes the numbers after § 14** — every number is in § 4.4 and
  § 4.5 and in `HongKongRules` alone; INV-5 and INV-6 move with them.

## 7. Tests

| Invariant | Where |
|-----------|-------|
| INV-1, INV-2, INV-3, INV-4, INV-5, INV-6, INV-7, INV-8, INV-9, INV-15 | `tests/selftest.cpp`, new blocks; the core alone |
| INV-10, INV-11, INV-13, INV-14, INV-16 | `tests/uitest.cpp`, offscreen |
| INV-10 | `tests/saves/`, the corpus check |
| INV-12 | a `--shot` pair, by hand, before the art moves and after |
| INV-13 | `scripts/translatable-check.py` |

Each self-test block is seen to fail first by breaking the rule its fixture
isolates — INV-2 by settling claims in seat order, INV-4 by dropping the
minimum — then seen to pass. The self-test needs no display, so the rules are
proved there, as Hearts' and Canasta's are.

## 8. Alternatives considered (and rejected)

- **All three rule sets before the first release.** The owner chose stages: a
  playable game sooner, each rule set tested before the next.
- **One engine per rule set.** The drawing, discarding and claiming are shared;
  three engines would be three copies of it. Rejected for the `RuleSet`
  interface.
- **Saving the table rather than the moves.** `docs/design.md` § Saves prefers
  the move list where a game has one, and Mahjong does: the replay re-checks
  every move. A table save would need its own re-check of each hand.
- **A copy of the tile art in the new view.** Two copies drift. Moving it keeps
  one drawing, and INV-12 proves the move changed nothing.
- **Scoring words in the core.** The core would carry display text and the
  translatable check would reach it. The core names a pattern by `Fan`; the
  view owns the words.
- **Computer strengths, as Canasta has four.** Deferred: one honest strength
  first, and levels once the game is played.
- **A claim window with a countdown.** It hurries a partially sighted player,
  which the owner's pacing rule forbids.

## 9. Out of scope

- Chinese Official rules — tracked by GHUB-0243.
- Japanese Riichi rules — tracked by GHUB-0244.
- American (NMJL) rules — not to be built; § 3.
- House-rule editing, as Canasta's House set — deferred; not yet queued.
- Computer strengths — deferred; not yet queued.
- All eight bonus tiles as an instant win — deferred; not yet queued.
- Playing against people — the 1.8.0 section of `ROADMAP.md` carries it.

## 10. What checks this

| Rule | What catches a breach |
|------|----------------------|
| INV-1 | `tests/selftest.cpp`, the tile-count block |
| INV-2 | `tests/selftest.cpp`, the claim-priority block |
| INV-3 | `tests/selftest.cpp`, the chow-seat block |
| INV-4 | `tests/selftest.cpp`, the minimum block |
| INV-5 | `tests/selftest.cpp`, the fan-list block |
| INV-6 | `tests/selftest.cpp`, the payment block |
| INV-7 | `tests/selftest.cpp`, the robbing block |
| INV-8 | `tests/selftest.cpp`, the drawn-hand block |
| INV-9 | `tests/selftest.cpp`, the dealer block |
| INV-10 | `tests/uitest.cpp` and the `tests/saves/` corpus check |
| INV-11 | `tests/uitest.cpp`, the rule-set block |
| INV-12 | **Partial:** a `--shot` pair taken by hand at the build; nothing re-checks it after |
| INV-13 | `tests/uitest.cpp` and `scripts/translatable-check.py` |
| INV-14 | `tests/uitest.cpp`, the turn-light block |
| INV-15 | `tests/selftest.cpp`, the four-computer games |
| INV-16 | `tests/uitest.cpp`, the claim-window block |
| § 4.7 legibility | **nothing** — whether a tile reads is judged by eye; the owner's own look is the check |
| § 4.8 how well the computer plays | **nothing** — INV-15 checks it wins, not that it plays well |

## 11. Cross-doc impact

- `docs/design.md` — a § Mahjong section for the table game, and § Mahjong
  Solitaire names the shared tile art.
- `CLAUDE.md` — the game list in § What this is.
- `README.md` — its game list.
- `CHANGELOG.md` — the release entry.
- `ROADMAP.md` — through the roadmap verbs only.

## 12. Cold-eyes loop log

Rows live in `../reviews/GHUB-0239-four-player-mahjong-loop-log.md`.

## 13. Resource cost

The engine holds one hand's moves and the score sheet's rows, both bounded by a
game's length. No new dependency.

## 14. Open questions

None. The three the draft asked — the numbers, the game length and the hub
tab — were answered by the owner on 2026-11-11 and are recorded in § 3.
