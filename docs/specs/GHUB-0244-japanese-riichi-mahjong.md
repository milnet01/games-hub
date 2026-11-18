<!-- ants-spec-format: 1 -->
# GHUB-0244 — Japanese Riichi rules for four-player Mahjong

**Status:** accepted (2026-11-18).
**Kind:** feature.
**Source:** ROADMAP GHUB-0244 (user-request-2026-11-11; the owner's decisions
of 2026-11-18, recorded in § 3).
**Blocked by:** GHUB-0243.

**Layman:** four-player Mahjong gains the Japanese rules, chosen from the
same Rule set menu as Hong Kong and Chinese Official. You can call "riichi"
when one tile from winning, bonus tiles called dora add to a win, and a hand
needs at least one named pattern to win. Every pattern, the bonus tiles and
the points are written on the table in words.

## 1. Goal

The Rule set menu of the `Mahjong` game lists Japanese Riichi after Chinese
Official. Under it the game follows the European Mahjong Association's riichi
rule book (§ 2): 136 tiles and no flowers, a dead wall with dora indicators,
riichi, furiten, a win only with at least one yaku, and the book's han,
minipoints and payments. Counters and riichi sticks carry from hand to hand.
Everyone starts on 30,000 points, and the game ends with the book's placement
bonus. A Full game is the East and South rounds. The computers declare riichi
when they can and aim for a hand with a yaku. The table says in words who has
declared riichi, which tiles are dora, when you cannot win on a thrown tile,
and each yaku of a win with its han, the minipoints and the points.

## 2. Problem

1. **`RuleSetId` has no value 3.** `mahjong::ruleSets()` lists Hong Kong and
   Chinese Official; the comment on `RuleSetId` keeps 3 for this item.
2. **The wall keeps no reserve.** `Hand::wallLeft` counts every tile not yet
   drawn. Riichi keeps fourteen tiles back as a dead wall, which supplies
   kong replacements and shows the dora indicators (book § 2.6, § 3.3.4).
3. **No move declares riichi.** `Action` holds the discard, the claims, the
   kongs, Win and Pass. A riichi is a declaration made with a discard, and it
   changes what that seat may do for the rest of the hand.
4. **Nothing stops a win on a thrown tile.** `Hand::openClaims` offers Win to
   any seat whose hand the tile completes at the minimum. The book refuses it
   to a furiten player (§ 3.3.9).
5. **One seat wins a discard.** `Hand::settleClaims` gives it to the first seat
   after the discarder. The book lets every claimant win (§ 3.3.1).
6. **A drawn hand moves no points, and nothing carries between hands.**
   `Hand::drawHand` clears the result. The book has noten payments (§ 3.4.2),
   counters (§ 3.4.4) and riichi sticks left on the table (§ 3.3.10).
7. **The game knows two lengths and starts at zero.** `Game::over` ends a
   full game after four winds; `Game` starts every total at 0. The book starts
   at 30,000, plays East and South (§ 3.1, § 3.5), and applies a placement
   bonus at the end (§ 3.7.1).
8. **The dealer question is one flag.** `RuleSet::dealerRepeats()` cannot say
   "the dealer keeps the deal after winning, or after a drawn hand where the
   dealer was ready" (§ 3.4.5).
9. **Scoring has no minipoints.** `HandScore` has fan, a total and points.
   Riichi pays from han and minipoints (§ 4.1).
10. **The computer does not know a hand needs a yaku.** `chooseMove` claims a
    pung or a chow whenever its plan likes one. An open hand with no yaku
    cannot win under Riichi.

Source for every rule here: the European Mahjong Association, *Rules for
Japanese Mahjong*, 2025 edition, version 1.1, cited as "book" with its section
numbers.
Source: <http://mahjong-europe.org/portal/images/docs/Riichi-rules-2025-EN.pdf>.
It replaces the 2016 edition.

## 3. Scope decisions (agreed with the user)

- **The EMA riichi rule book, as written.** No red fives, and the tile art is
  unchanged. Every rule here comes from the book; where the book is silent,
  § 3's last bullets say so. The owner, 2026-11-18.
- **Full game is East and South.** Under Riichi, Full game plays the East and
  South rounds; East round stays the short game. The owner, 2026-11-18.
- **The computer attacks and does not defend.** It declares riichi when it
  can and aims for a hand with a yaku. It plays no safe tile against a riichi.
  Defence is a follow-up (§ 9). The owner, 2026-11-18.
- **A ready hand is always announced ready at a drawn hand.** The book lets
  a ready player announce "not ready" instead (§ 3.4.2); the table offers no
  such choice, so a drawn hand needs no extra step. The owner, 2026-11-18,
  taking the recommendation.
- **The East round ends as a full game does.** The book has no short game.
  At the end of an East round the leftover sticks and the placement bonus are
  applied as the book applies them after South (§ 4.9). The author's call.
- **Penalties never arise.** The book's chombo and dead hand punish a false
  call (§ 3.3.14, § 3.4.6, chapter 6). The table offers only legal moves, so
  no player can make one. The tournament time limit (§ 7.4) does not apply.
- **Where the book is silent, this spec decides, and says so.** Three
  places: a player below 1,000 points may still declare riichi, since the
  book lets a score go below zero (§ 4.1.4); the game has no early stop for
  a leading dealer in the last hand, since the book ends the game only when
  the deal passes (§ 3.5); and liability (§ 4.6) counts only sets claimed
  from a discard as "called".

## 4. Design

### 4.1 Where the parts live

| Part | Where | Half |
|------|-------|------|
| `RiichiRules`, its yaku list, minipoints and payments | `src/mahjong/` riichi rules files, new | core |
| `TableRules`, the new `RuleSet` members, `Shape::SevenDifferentPairs`, `waitingTiles` | `src/mahjong/mahjongrules.*` | core |
| The dead wall, riichi, furiten, several winners, counters and sticks, the game's length and end | `src/mahjong/mahjongtable.*` | core |
| The computer's riichi and yaku aims | `src/mahjong/mahjongplayer.*` | core |
| `fanName` and `fanMeaning` for the new `Fan` values, and `FullyConcealed`'s sentence | `src/mahjong/mahjongtablewords.cpp` | view |
| The menu label, the table words, the rules text, the save bound | `src/mahjong/mahjongtableview.*` | view |

### 4.2 The interface

```cpp
namespace mahjong {
enum class RuleSetId : std::uint8_t { HongKong = 1, ChineseOfficial = 2, JapaneseRiichi = 3 };

enum class Shape : std::uint8_t {
    // ... every value it has today, and:
    SevenDifferentPairs, // seven pairs, no two alike (book § 4.2.2)
};

// How the table plays under a rule set, beyond scoring. Each default is how
// Hong Kong and Chinese Official play, so neither changes.
struct TableRules {
    int deadWall = 0;           // tiles kept back at the end of the wall
    int startingPoints = 0;     // each seat's total before the first hand
    int fullGameRounds = 4;     // winds a Full game plays
    int mostKongs = 0;          // kongs declared in the deal, all seats together; 0 is no limit
    int riichiStake = 0;        // 0: no riichi
    bool doraIndicators = false;
    bool furiten = false;
    bool severalWinners = false;     // every seat claiming Win on a tile wins
    bool kongsAfterADraw = false;    // concealed and added kongs only in a turn begun with a draw
    bool addAHeldTile = false;       // an added kong may use any tile held, not only the one drawn
    bool noSwapCalling = false;      // book § 3.3.2
    bool robConcealedKong = false;   // for Thirteen orphans only
    bool liability = false;          // book § 3.3.7
};

// How a hand ended, for the dealer and the counters.
struct HandEnd {
    bool drawn = false;
    bool dealerWon = false;   // the dealer is one of the winners
    bool dealerReady = false; // at a drawn hand, the dealer is ready
};

enum class Limit : std::uint8_t { None, Mangan, Haneman, Baiman, Sanbaiman, Yakuman };

struct HandScore {
    // ... every field it has today, and:
    int fu = 0;                 // minipoints after rounding; 0 where unused
    Limit tier = Limit::None;
};

struct WinContext {
    // ... every field it has today, and:
    bool riichi = false;
    bool doubleRiichi = false;
    bool ippatsu = false;
    // Won before any claim or kong, and before the winner's first discard.
    bool firstGoRound = false;
    std::vector<Face> doraIndicators; // face up when the hand ended
    std::vector<Face> uraIndicators;  // under them; empty unless riichi or double riichi
    int counters = 0;
    int liable = -1;                  // the seat liable under § 4.6, else -1
};

class RuleSet {
public:
    // ... every member it has today except dealerRepeats(), and:
    virtual TableRules table() const { return {}; }
    virtual bool dealerKeepsDeal(const HandEnd& end) const = 0;
    virtual int countersAfter(int counters, const HandEnd& end) const { return 0; }
    // What each seat gains or pays at a drawn hand; `ready` by seat.
    virtual std::array<int, 4> settleDraw(const std::array<bool, 4>& ready) const { return {}; }
    // The final scores from the totals once the game is over.
    virtual std::array<int, 4> finalScores(const std::array<int, 4>& totals) const { return totals; }
};

// mahjongtable.h
enum class Action : std::uint8_t {
    // ... every value it has today, and after Pass:
    Riichi, // declare riichi and throw `tile`
};
// What a hand is told when it starts. Points, not sticks.
struct HandStart {
    std::array<int, 4> totals {};
    int counters = 0;
    int sticks = 0; // riichi stakes left on the table, in points
};
struct HandResult {
    // ... every field it has today, and:
    std::vector<std::pair<WinContext, HandScore>> alsoWon; // § 4.5
    std::array<bool, 4> ready {}; // at a drawn hand, by seat
    int sticksAfter = 0;          // in points
};

// Every face that would complete a hand of `concealed` (one tile short) and
// `melds` sets under any shape `rules` allows, the minimum aside. A face of
// which the hand and its sets already hold all four is not one (book § 3.3.8).
std::vector<Face> waitingTiles(const RuleSet& rules, const std::vector<Face>& concealed,
                               const std::vector<Meld>& melds);

// The riichi rules files.
class RiichiRules : public RuleSet {
public:
    // ... every RuleSet member, and:
    // A yakuman item's value: above any han a § 4.7 row carries.
    static constexpr int kYakuman = 100;
    // The han a § 4.7 row scores with a set claimed; 0 for a concealed-only row.
    static int openValue(Fan fan);
    // § 4.6's basic points for `han` and the rounded `fu`; 8,000 when `han` is kYakuman.
    static int basicPoints(int han, int fu);
};
}
```

**`dealerRepeats()` is replaced, not kept beside the new member.** Hong Kong
keeps the deal when `drawn` or `dealerWon`; Chinese Official never; Riichi
when `dealerWon`, or `drawn` and `dealerReady` (book § 3.4.5). `Game::settle`
and `MahjongTableView::endOfHandLines` read it where they read
`dealerRepeats()`.

**`better` compares points first, then total.** Under Hong Kong and Chinese
Official, points never fall as the total rises, so the reading kept does not
change. Under Riichi a reading with more han can pay less than one with more
minipoints; the book always takes the highest-scoring reading (§ 3.4.3).

**The table reads only the interface.** `Hand` and `Game` ask `table()`,
`dealerKeepsDeal`, `countersAfter`, `settleDraw` and `finalScores`. Nothing in
`Hand` or `Game` names a rule set.

### 4.3 The wall and the dora

**136 tiles.** `bonusTilesInPlay()` is false and `chowAllowed()` true.
`shapeAllowed` allows `Sets`, `SevenDifferentPairs` and `ThirteenOrphans`.
Chinese Official's `shapeAllowed`, which allows every shape today, refuses
`SevenDifferentPairs`.

**The dead wall is the last `deadWall` tiles of the shuffled wall.**
`wallLeft()` counts the live wall only: the tiles between the front and the
back, less `deadWall`. A kong's replacement is drawn from the back, as now,
so the live wall shrinks by one with each kong. That is the book's "the last
tile in the wall becomes part of the dead wall" (§ 3.3.4). Where it sits in
the shuffle changes nothing, since the shuffle is uniform.

**Fixed places, counted from the wall's last tile as built** (book § 2.6,
§ 2.7). They do not move as replacements are drawn:

| From the last tile | Holds |
|--------------------|-------|
| 1st to 4th | the replacement tiles, 1st first |
| 5th | the dora indicator |
| 6th | its ura indicator |
| 7th, 9th, 11th, 13th | the kan dora indicators, in order |
| 8th, 10th, 12th, 14th | their ura indicators |

`Hand::arranged` lays its test wall out the same way, so a test places an
indicator by position.

**Revealing.** The dora indicator is face up from the deal. Each kong
declared turns the next kan dora indicator face up before its replacement is
drawn, for all three kinds (book § 3.3.3, § 3.3.4). An added kong that is
robbed turns none (§ 3.3.13). The ura indicators under every face-up
indicator are read at a win by a seat that declared riichi, and only then
(§ 3.3.10).

**What an indicator points to** (book § 2.7): the next tile of its suit, 9 to
1; red dragon to white, white to green, green to red; east to south, west,
north and back to east. Each tile of a winning hand scores one han per
indicator pointing at it (§ 4.1).

**The end of the wall** (book § 3.4.1). With the live wall empty no kong may
be declared. A kong made with one tile left takes its replacement and leaves
no last draw. The last discard may be claimed only for a win, as now.

### 4.4 A turn under Riichi

**Riichi** (book § 3.3.10). A new `Action::Riichi` is one move: declare and
throw `tile`. It is offered for each tile whose throw leaves the hand ready,
when no set has been claimed (a concealed kong is allowed), and at least one
tile remains in the live wall. A seat's total does not limit it (§ 3).

- **The stake.** 1,000 points (`riichiStake`) move from the seat to the
  table's sticks once nobody wins on the tile thrown. A win on that tile
  voids the declaration and nothing is staked.
- **After it.** The seat throws each tile it draws unless it wins on it or
  makes a kong. It claims nothing but a win. A concealed kong is offered only
  when the drawn tile is the fourth of a concealed pung, the waiting tiles
  do not change, and in every winning hand those three tiles are a pung.
- **Ippatsu** stands from the declaration to the seat's next discard. Any
  claim of a set, or any kong that stands, ends it; an added kong that is
  robbed does not (book § 4.2.1).
- **Double riichi**: a declaration on the seat's first discard, with no claim
  or kong before it (book § 4.2.2).

**Kongs** (book § 3.3.4, § 3.3.5). A concealed or added kong only in a turn
that began with a draw (`kongsAfterADraw`). An added kong may use the fourth
tile from the hand, not only the one just drawn (`addAHeldTile`). No fifth
kong in a deal, counting every seat's (`mostKongs`).

**No swap-calling** (book § 3.3.2). After claiming a tile, the seat may not
throw that face. After a chow, it may not throw the face that would make the
same run from the other end: having claimed the 3 for 4-5, not the 6; the 6
for 4-5, not the 3. A claim is not offered when no throw would then be legal.

**Robbing** (book § 3.3.13). An added kong is robbed as now. A concealed kong
opens the rob window only to a seat whose Thirteen orphans it completes.

### 4.5 Furiten and several winners

**Furiten** (book § 3.3.9). A seat is refused Win on a thrown tile, or on a
robbed kong, while any of these holds:

- **Its own discards.** Any face of `waitingTiles` is one it has thrown this
  hand, including a throw another seat claimed. The new per-seat list of every
  throw is kept beside `SeatTiles::pool`, which holds only the unclaimed ones.
- **Passed.** A tile was thrown, or a kong offered, that completes its hand,
  and it did not win on it — whether or not the hand had a yaku. This ends at
  its next draw or claim.
- **Passed after riichi.** The same, after it declared riichi: it lasts to
  the end of the hand.

A furiten seat may still win on a tile it draws. `Hand::furiten(seat)`
answers for the view, and says which of the three holds, so § 4.11 can pick
its sentence.

**Several winners** (book § 3.3.1, § 4.1). Every seat that answers Win on one
tile wins it, the discarder paying each. `HandResult::win` and `score` stay
the first winner after the discarder; a new `HandResult::alsoWon` holds the
others, in turn order.

### 4.6 Payments, counters and sticks

**Basic points** (book § 4.1.2). Under 5 han: minipoints × 2^(han + 2), and
above 1,900 it is 2,000. At 5 han and over:

| Han | Name | Basic points |
|-----|------|-------------:|
| 5 | Mangan | 2,000 |
| 6 or 7 | Haneman | 3,000 |
| 8 to 10 | Baiman | 4,000 |
| 11 or more | Sanbaiman | 6,000 |
| — | Yakuman | 8,000 |

There is no count to yakuman: 13 han is Sanbaiman. `HandScore::points` is the
basic points, and 0 when the hand has no yaku. Below yakuman (§ 4.8 step 1),
`total` is the han with the dora and `qualifying` the han without them.
`minimumToWin()` is 1.

**Who pays** (§ 4.1.2), each payment rounded up to the next 100:

- On a discard or a robbed kong, the discarder pays 4 × basic, or 6 × when
  the winner is the dealer.
- Self-drawn by the dealer, each other seat pays 2 × basic. Self-drawn by
  another seat, the dealer pays 2 × and the other two 1 × each.
- **Counters** (§ 3.4.4) add 300 per counter on a discard, paid to every
  winner, or 100 from each payer on a self-draw.
- **Liability** (§ 3.3.7). A seat that threw the tile claimed to complete a
  third dragon set, or a fourth wind set, when the other two (three) were
  already claimed sets, is `liable`. If that hand wins Big three dragons or
  Big four winds: self-drawn, the liable seat pays all of it; on another
  seat's discard, the two split it evenly and the discarder alone pays the
  counters. Whether the liable seat also pays the counters on a self-draw the
  book does not say; this spec has it pay them.

**At a drawn hand** (§ 3.4.2), `settleDraw`: one ready seat takes 1,000 from
each other; two take 1,500 each from the two not ready; three take 1,000 each
from the one. None or four ready: nothing moves. A seat is ready when
`waitingTiles` is not empty, which counts a wait whose tiles are all in view
(§ 3.3.8). A riichi seat is always ready.

**Counters** (§ 3.4.4), `countersAfter`: one more after a hand the dealer won,
or after a drawn hand; none after a hand others won and the dealer did not.

**Riichi sticks** (§ 3.3.10). Every stake goes to the table. A winner takes
the sticks: with several winners, each winner's own stake from this hand
returns to it, and the rest go to the first winner after the discarder.
After a drawn hand the sticks stay. `Hand` is told the counters, the sticks
and the totals when it starts, through a `HandStart` argument to its
constructor and to `Hand::arranged`, defaulting to none of each.

`HandResult::payments` is each seat's net change over the whole hand: every
winner's payment, the stakes and the sticks taken. `Game` adds it to the
totals, as now, and
`HandResult::sticksAfter` holds what is left on the table. **Across every
hand, the four totals plus the sticks sum to four times 30,000.**

### 4.7 The yaku

Han concealed, han with a set claimed, the `Fan`, and what it means. A `Fan`
in *italics* is an existing one, reused because it means the same here
(GHUB-0243 § 3, one word per meaning); its Means cell says its sentence
briefly. One sentence changes for every rule set: `FullyConcealed`'s becomes
"No set claimed, and won on a tile you drew.", since a concealed kong is laid
down yet claims nothing. "—" in the open column: concealed
hands only. "Pung" includes a kong. The meanings are the book's § 4.2,
reworded plainly.

| Han | Open | `Fan` | Means |
|----:|-----:|-------|-------|
| 1 | — | `Riichi` | Declared riichi |
| 1 | — | `Ippatsu` | Won before your next discard after riichi, with no claim or kong between |
| 1 | — | *`FullyConcealed`* | No set claimed, won on a tile you drew |
| 1 | — | `Pinfu` | Four chows and a pair of no value, won on a two-sided wait |
| 1 | — | *`PureDoubleChow`* | Two identical chows |
| 1 | 1 | *`AllSimples`* | No ones, nines or honours |
| 1 | 1 | *`DragonPung`* | A pung of dragons, each |
| 1 | 1 | *`SeatWindPung`* | A pung of your own wind |
| 1 | 1 | *`PrevailingWindPung`* | A pung of the round's wind |
| 1 | 1 | *`KongReplacement`* | Won on a kong's replacement tile |
| 1 | 1 | *`RobbingAKong`* | Won on a tile added to make a kong |
| 1 | 1 | *`LastTile`* | Won on the last tile of the wall, drawn yourself |
| 1 | 1 | *`LastTileClaim`* | Won on the tile thrown after the wall's last tile was drawn |
| 2 | — | `DoubleRiichi` | Riichi declared on your first discard, nothing claimed before |
| 2 | — | `SevenDifferentPairs` | Seven pairs, no two alike |
| 2 | 1 | *`MixedTripleChow`* | The same chow in all three suits |
| 2 | 1 | *`PureStraight`* | 1-2-3, 4-5-6 and 7-8-9 of one suit |
| 2 | 1 | `HalfOutsideHand` | A one, nine or honour in every set and the pair; at least one chow and one honour |
| 2 | 2 | *`TriplePung`* | Pungs of one number in all three suits |
| 2 | 2 | *`ThreeConcealedPungs`* | Three pungs, none of them claimed |
| 2 | 2 | *`ThreeKongs`* | Three kongs |
| 2 | 2 | *`AllPungs`* | Four pungs and a pair |
| 2 | 2 | *`SmallThreeDragons`* | Pungs of two dragons and a pair of the third |
| 2 | 2 | *`AllTerminalsAndHonours`* | Only ones, nines and honours |
| 3 | — | `TwicePureDoubleChow` | Two pairs of identical chows |
| 3 | 2 | *`MixedOneSuit`* | One suit and honours |
| 3 | 2 | `FullOutsideHand` | A one or nine in every set and the pair; at least one chow; no honours |
| 5 | — | `BlessingOfMan` | Won on a discard before your first turn, nothing claimed before |
| 6 | 5 | *`PureOneSuit`* | Every tile of one suit |

Yakuman, concealed or not as the meaning says: *`ThirteenOrphans`*,
*`NineGates`*, *`HeavenlyHand`*, `BlessingOfEarth` (won on your own first
draw, nothing claimed and no kong before it), *`FourConcealedPungs`*,
*`FourKongs`*, *`AllGreen`*, *`AllTerminals`*, *`AllHonours`*,
*`BigThreeDragons`*, *`SmallFourWinds`*, *`BigFourWinds`*. A yakuman item
carries the value `RiichiRules::kYakuman`, which the list sorts by and the
view reads as "yakuman"; it is never added to han.

Not yaku, scored after them: `Dora`, one han per dora in the hand, and
`UraDora`, the same for the ura indicators.

**The new `Fan` values come after `Flower`**, and `kFanCount` follows the
last. A `Fan` is never stored, so the order is free. `HongKongRules::fanList`
and `ChineseOfficialRules::fanList` already list their own rows only.

**Where the book's words were read closely.** Mixed one suit needs an honour,
since a hand of one suit with none is Pure one suit. Small three dragons
scores its dragon pungs as `DragonPung` as well, as the book says. All terminals and honours adds All pungs or
Seven different pairs, as the book says.

### 4.8 Counting a win

`RiichiRules::score` counts one reading.

1. **A yakuman reading** scores its yakuman items alone, with no dora: tier
   `Yakuman`, `total` and `qualifying` both `kYakuman`, so it may win, and
   `points` 8,000. Two yakuman pay as one (§ 4.2: not cumulative).
2. **Otherwise the yaku**, by § 4.7, concealed or open by
   `WinContext::claimedASet`. A win on a discard leaves the hand concealed;
   only the pung the winning tile finishes counts as claimed (book § 4.2),
   as `bestWin` already marks it. Struck: `Riichi`
   beside `DoubleRiichi`; `LastTile` beside `KongReplacement`;
   `PureDoubleChow` beside `TwicePureDoubleChow`.
3. **No yaku, no win**: `qualifying` 0 and `points` 0.
4. **Dora and ura dora**, from `doraIndicators`, and `uraIndicators` when
   `riichi` or `doubleRiichi`.
5. **`BlessingOfMan` stands alone**, no other yaku and no dora (§ 4.2.4). A
   hand holding it is scored both ways and the higher kept.
6. **Minipoints** (§ 4.1.1), unless the shape is Seven different pairs, which
   is 25 and not rounded. 20, plus 10 for a concealed hand won on a discard,
   plus 2 for self-drawn unless `Pinfu` scores. Each pung by § 4.1.1's table
   (2 and 4 for 2 to 8, claimed and concealed; double for ones, nines and
   honours; a kong four times its pung). A pair of dragons, or of the seat or
   round wind, 2, and 2 only when it is both winds. 2 for an edge, closed or
   pair wait in this reading, whatever else it waited on. An open hand at
   exactly 20 is given 2. Round up to the next 10.
7. **Basic points** by § 4.6's table, `tier` set at 5 han and over, or at
   4 han 30 or more, or 3 han 60 or more, whose basic points pass 1,900.

`bestWin` scores every reading and every part the winning tile could finish,
and keeps the best by `better` (§ 4.2). Seven different pairs and Twice pure
double chow hold the same tiles; both are scored and the higher kept. The book's
scoring example 8 scores such a hand as Twice pure double chow, not both.

**Pinfu needs a two-sided wait in this reading**: the winning tile is at
either end of its chow, and not the 3 of 1-2-3 or the 7 of 7-8-9.

### 4.9 The game

- **Starting totals** are `startingPoints`: 30,000 each (book § 1.4).
- **Length.** Full game is `fullGameRounds` winds: East and South. East round
  is one wind. `Game::over` counts deals passed, as now; under Riichi the
  dealer keeps the deal by § 4.2, so a round can run past four hands. **No
  seat going below zero ends the game** (§ 3.7, § 4.1.4).
- **The game's end.** `Game` keeps the counters and the sticks between hands.
  When the game is over, the sticks left go to the seat on the top total,
  split evenly and rounded down on a tie (§ 3.7). Then `finalScores`: each
  total less 30,000, plus 15,000, 5,000, −5,000 and −15,000 by place; tied
  seats share their places' sum evenly (§ 3.7.1). `Game::leaders()` still
  reads the totals; with the sticks and the placement bonus both given by
  place, the order is the same.
- **`Game::replay`** is unchanged: the counters, sticks and totals are what
  the replay gives.

### 4.10 The computer

**It attacks, as § 3 says, in three ways added to its Hong Kong play.**

- **It declares riichi whenever it may**, throwing the tile that leaves the
  most faces to wait on, and among equals one it is not furiten on.
- **It claims a set only when the hand keeps a yaku it can finish**: a pung
  of a dragon, its seat wind or the round's wind; one suit, with or without
  honours; all simples, when the hand and the claim hold no one, nine or
  honour; or all pungs. Otherwise it stays concealed, so riichi is its yaku.
- **It keeps toward that yaku.** With all simples as the plan it throws ones,
  nines and honours first; the one-suit and all-pungs plans are as now.

It does not defend: it never chooses a tile because a riichi seat may win on
it. It always wins when Win is offered, as now. Win is offered only with a
yaku and outside furiten, so it never makes a false call.

### 4.11 The view

- **The menu** lists "Japanese Riichi rules" after "Chinese Official rules",
  from `ruleSets()`.
- **Riichi on the table.** A seat that has declared has "riichi" on its
  plate's line beside its score, and the score already shows the stake gone.
  The sentence under the centre says "Right declares riichi and throws the
  Five of Bamboo." The tile thrown with a declaration is drawn turned sideways
  in its pool, as the book lays it (§ 3.3.10).
- **Dora** along the top of the centre, after the wall count: each face-up
  indicator drawn as a tile no smaller than a pool tile, and never below
  `mahjong::kTileMinWidth` with large play on, and in words: "Dora: the Five
  of Bamboo (shown by the Four)." The counters and the sticks on the table
  follow in words: "2 counters, 1 riichi stick on the table."
- **Furiten** is said beside your buttons while it holds: "You cannot win on
  a thrown tile: you threw one you are waiting on." or "...: you passed a
  winning tile." It is said on the table, not only in the status bar.
- **After riichi, and after a claim,** only the tiles you may throw can be
  picked. The sentence says why: "You declared riichi: throw the tile you
  drew, or win." A picked tile that a riichi may go with adds a button,
  "Riichi, throwing the Five of Bamboo", beside "Throw the Five of Bamboo".
- **The end of a hand** lists, for each winner in turn: who won and on what,
  the ura indicators face up for a riichi winner, one line per yaku with its
  han ("Riichi — 1 han", "Big three dragons — yakuman"), the dora lines, then
  "Total: 3 han, 40 fu" or "Mangan: 5 han", the counters and sticks taken, and
  each seat's payment. A drawn hand names the ready seats, shows their tiles
  face up as the book has them revealed (§ 3.4.2), and gives the payments and
  whether the dealer keeps the deal.
- **The end of the game** lists each seat's total, then its final score with
  the placement bonus ("You: 41,300 points, final +26,300").
- **The rules text** reads the rule set in force. For Riichi: the tiles with
  no flowers, the dead wall and dora, riichi, furiten, a yaku needed to win,
  the yaku by han with their open values from `RiichiRules::openValue`,
  minipoints and the points table from `RiichiRules::basicPoints`, who pays,
  counters, sticks, the dealer and the game's end. Every value comes from
  `RiichiRules`, as GHUB-0249 has the Hong Kong points table come from the
  rule set. The shared "How it plays" sentences that state a Hong Kong rule
  — the flowers, two wins going to one player, adding only the tile just
  drawn, a drawn hand moving no points — follow the rule set in force too.
- **Words.** Each new sentence goes through `tr()`. The book's Japanese names
  — riichi, dora, furiten, han, fu, mangan, yakuman — are used where the rules
  text defines them in plain words first.

### 4.12 Saves and settings

**The save layout does not change, and `kBlobVersion` stays 1.**
`MahjongTableView::encodeSave` writes the rule set, the length, the first
dealer, then each hand's seed and four bytes per move: seat, action, tile and
a chow's lowest tile. Riichi adds one stored value: `Action::Riichi`,
appended after `Action::Pass`, so every existing value keeps its number. A
declaration is one move, its tile the tile thrown. **Nothing else is
stored.** The counters, the sticks, the totals, the dora, furiten and who was
ready at a drawn hand are what replaying the moves gives, as `docs/design.md`
§ Mahjong says of the totals and the dealer today.

**`MahjongTableView::restoreState` must take the new value.** It refuses a
move whose action is above `Action::Pass`. That bound becomes the last
`Action`. Left as it is, every Riichi save holding a declaration is refused
whole.

**Old saves read as before.** A Hong Kong or Chinese Official save holds no
`Action::Riichi`, so it reads and replays unchanged; `tests/saves/Mahjong.bin`
is that check. An `Action::Riichi` inside such a save is not in
`legalMoves()`, so the replay refuses the save.

**A Riichi save in an older build is refused** by `knownRuleSet(3)` before
any move is read, and the hub keeps its fresh game (GHUB-0239 § 6).

**Settings.** `mahjong4/ruleSet` takes 3. `mahjong4/length` is unchanged.

## 5. Invariants

These clauses cannot run yet; the surfaces they test are what this spec
creates. Each names the rule its fixture isolates. The self-test blocks build
their positions with `Hand::arranged`.

- **INV-1** — `RiichiRules::fanList()` holds exactly § 4.7's rows and its
  yakuman, with their han, and `RiichiRules::openValue` matches the Open
  column; every new `Fan` has a non-empty `fanName` and `fanMeaning`.
  *Test:* `tests/selftest.cpp` checks each row's values; `tests/uitest.cpp`
  checks the words for every `Fan`.
  *Breaks when:* a row is missed, or an open value is the concealed one.

- **INV-2** — Each yaku scores on a hand holding it and not on a near miss.
  *Test:* `tests/selftest.cpp`, one fixture per row of § 4.7 and per yakuman
  through `bestWin`. The near miss changes one tile, the way of winning or a
  claimed set, and loses the item. Concealed-only rows are also tried with a
  set claimed, and lose the item; open rows score the Open value.
  *Breaks when:* a yaku is read by the wrong test, or ignores `claimedASet`.

- **INV-3** — The struck pairs of § 4.8 are not scored together, and two
  yakuman pay as one.
  *Test:* `tests/selftest.cpp`: double riichi scores no `Riichi`; a kong
  replacement drawn as the wall's last tile scores `KongReplacement` and no
  `LastTile`; Twice pure double chow scores no Pure double chow; a hand with
  Blessing of man and 6 han of other yaku keeps the 6; All honours with Big
  three dragons pays 8,000 basic points.
  *Breaks when:* step 2's strikes or step 1's single yakuman are skipped.

- **INV-4** — Minipoints follow § 4.8 step 6.
  *Test:* `tests/selftest.cpp`, hands with known values: pinfu self-drawn
  20; pinfu on a discard 30; the same tiles with a chow claimed, on a discard,
  30 (22 rounded); seven different pairs 25; a concealed hand on a discard
  with a closed wait and a concealed pung of nines, 20 + 10 + 2 + 8 = 40; a
  pair of east as dealer in the East round, 2.
  *Breaks when:* the open 20 is not raised, or a double-wind pair counts 4.

- **INV-5** — Basic points and payments follow § 4.6.
  *Test:* `tests/selftest.cpp`, against the book's § 4.1.3 tables: 1 han 30 fu
  on a discard, 1,000 from a non-dealer win and 1,500 from the dealer's; the
  same self-drawn, 300 and 500, and 500 from each; 4 han 30 fu, 8,000; 3 han
  60 fu, 8,000; 13 han, 24,000 (Sanbaiman, no yakuman); a yakuman, 32,000
  and 48,000. Every figure is a non-dealer's on a discard unless it says
  otherwise. Payments sum to zero.
  *Breaks when:* basic points of 1,920 are not raised to 2,000, or 13 han
  counts as yakuman.

- **INV-6** — A hand needs a yaku: dora alone never win.
  *Test:* `tests/selftest.cpp`. A complete concealed hand with three dora and
  no yaku, on a discard, is offered no Win; the same tiles after a riichi are.
  Both are complete, so only the yaku rule refuses the first.
  *Breaks when:* `qualifying` counts dora.

- **INV-7** — The dora follow § 4.3.
  *Test:* `tests/selftest.cpp`: indicators 9 of circles, red dragon, white
  dragon, green dragon and north point to the 1 of circles, white, green, red
  and east; a concealed kong turns the second indicator face up before the
  replacement is drawn; an added kong that is robbed turns none; the ura
  indicators score for a riichi winner and not for a winner without riichi
  holding the same tiles.
  *Breaks when:* the dragon order is the tile order, or ura dora score
  without riichi.

- **INV-8** — The live wall leaves fourteen tiles, and every tile is
  somewhere once.
  *Test:* `tests/selftest.cpp`, seeded four-computer Riichi hands: at a drawn
  hand the wall holds exactly fourteen undrawn tiles; after every move the
  tiles in play are `fullSet()` less the flowers and seasons (GHUB-0239
  INV-1's check). An arranged hand with two kongs ends with the same fourteen.
  *Breaks when:* the reserve is not subtracted, or a kong does not move the
  live wall's end.

- **INV-9** — Furiten refuses a win on a thrown tile and only that.
  *Test:* `tests/selftest.cpp`, one seat waiting on 3 and 6 of bamboo, with a
  yaku on a thrown 6 and none on a thrown 3. Having
  thrown a 3 that another seat claimed, it is offered no Win on a thrown 6,
  and is offered Win on a drawn 6. Having passed a 3 with no yaku, it is
  refused a 6 thrown before its next draw, and offered one after it. Having
  passed a 3 after riichi, it is refused every thrown winning tile to the end.
  *Breaks when:* claimed throws are left out, or temporary furiten never ends.

- **INV-10** — Riichi follows § 4.4.
  *Test:* `tests/selftest.cpp`, arranged hands. A ready concealed hand is
  offered `Action::Riichi` with each ready-keeping throw and no other. The
  same hand with a chow claimed, or with the live wall empty, is offered none.
  After the declaration its only discard is the drawn tile, and a thrown tile
  offers it Win and Pass only. A win on the declaring throw stakes nothing:
  the declarer pays only the win, and its 1,000 is not staked. A win
  before the declarer's
  next discard scores `Ippatsu`; with a pung claimed between, it does not. A
  concealed kong that would change the waits is not offered.
  *Breaks when:* any one condition of § 4.4 is missing.

- **INV-11** — Claims and kongs follow § 4.4 and § 4.5.
  *Test:* `tests/selftest.cpp`: two seats waiting on one tile both win it and
  both are paid; after claiming the 3 of circles for 4-5, throwing the 3 or
  the 6 is refused and the 4 is not; an added kong uses a fourth tile held
  since the deal; a fifth kong is not offered; a concealed kong offers a rob
  to a seat waiting with Thirteen orphans and to no other.
  *Breaks when:* the first winner alone is paid, or a rule of § 4.4 is
  Hong Kong's.

- **INV-12** — A drawn hand pays the noten payments, and the counters, sticks
  and dealer follow § 4.6.
  *Test:* `tests/selftest.cpp`, arranged hands played to an empty wall with
  one, two, three, none and all seats ready: the one ready seat takes 3,000 in
  all; two take 1,500 each; three take 1,000 each; none and all move nothing; the counters rise by one; the dealer keeps the deal
  only when ready; a riichi stake stays on the table into the next hand and
  goes to its winner. Across seeded games the totals plus the sticks stay at
  120,000 until the game's end shares out the sticks.
  *Breaks when:* a drawn hand moves nothing, or the sticks are lost.

- **INV-13** — Counters pay as § 4.6 and reset after another seat's win.
  *Test:* `tests/selftest.cpp`, a game with two counters: a discard win takes
  600 more; a self-draw 200 more from each payer; with two winners the
  discarder pays both 600; the counters are 0 after a non-dealer win and 3
  after a dealer win.
  *Breaks when:* the counters are paid once among several winners.

- **INV-14** — Liability follows § 4.6.
  *Test:* `tests/selftest.cpp`: a seat holding two claimed dragon pungs
  claims the third from seat 2, then draws its Big three dragons: seat 2 pays
  it all. Won instead on seat 3's discard: seats 2 and 3 pay half each, and
  seat 3 alone the counters. With one dragon pung concealed, nobody is liable.
  *Breaks when:* liability is not applied, or a concealed set counts as
  called.

- **INV-15** — The game's length and end follow § 4.9.
  *Test:* `tests/selftest.cpp`, seeded four-computer Riichi games at both
  lengths: each ends; a full game's last hand is in the South round and the
  deal has passed eight times; an East round's, four. A game where a seat's
  total falls below zero plays on. `finalScores` of totals 40,000, 30,000,
  30,000 and 20,000 is +25,000, 0, 0 and −25,000; the leftover sticks join
  the top total first.
  *Breaks when:* the game ends after four winds, ends on a bust, or a tie
  takes one place's bonus.

- **INV-16** — Hong Kong and Chinese Official play as before.
  *Test:* `tests/selftest.cpp`, the existing blocks unchanged, among them
  GHUB-0239 INV-1, INV-2, INV-8 and INV-9 and GHUB-0243 INV-9.
  *Breaks when:* a `TableRules` default is not the old behaviour.

- **INV-17** — Four-computer Riichi games end, every win has a yaku, some
  hands are won and the computers declare riichi.
  *Test:* `tests/selftest.cpp`, the seeded games of INV-15: every win's
  `qualifying` is at least 1; at least one hand in five is won; at least one
  riichi is declared. It prints the hands won and the riichi declared.
  *Breaks when:* the computer opens hands with no yaku, so nearly every hand
  draws.

- **INV-18** — The menu lists the three rule sets, Riichi is remembered, a
  Riichi save restores, and the old corpus still does.
  *Test:* `tests/uitest.cpp`: the menu holds three entries; set Riichi, build a
  fresh view, assert it is in force; play to a seat's riichi, save, restore
  into a fresh view, compare `saveState()`; change a Hong Kong save's
  discard to `Action::Riichi` and assert `restoreState` refuses it. The
  corpus check restores `tests/saves/Mahjong.bin`.
  *Breaks when:* `restoreState`'s bound still ends at `Action::Pass`.

- **INV-19** — Riichi, the dora, furiten and a win's han and fu are on the
  table in words.
  *Test:* `tests/uitest.cpp`, a probe view on an arranged Riichi hand: after a
  declaration the sentence names the seat and riichi; the centre's words name
  each dora; a furiten seat 0 gets its sentence; after a win the end-of-hand
  lines hold every item's `fanName`, "han" and "fu". `python3
  scripts/translatable-check.py` exits 0.
  *Breaks when:* any of these is said only in the status bar, or not at all.

## 6. Failure modes

- **A Riichi save in an older build** — refused, and the hub keeps its fresh
  game (§ 4.12).
- **No legal throw after a claim** — cannot happen: § 4.4 offers no claim
  that would leave none.
- **The computer seldom finishes a hand with a yaku** — INV-17 notices. Hands
  then draw; the dealer may keep the deal, but a drawn hand with the dealer
  not ready passes it, so the game ends.
- **A long game** — a dealer who keeps winning or staying ready keeps the
  deal, as under Hong Kong. `kMostHands` in `mahjongtableview.cpp` bounds what
  a save may hold.
- **The book is silent** — on the three points § 3 names, and on § 4.6's
  counters under liability. This spec decides each; a different reading is a
  change to that section.
- **Scoring cost** — the per-turn work grows: `waitingTiles` for each seat's
  furiten and for each riichi throw. Not measured; the build measures the
  self-test's time before and after, as GHUB-0243 did.

## 7. Tests

| Invariant | Where |
|-----------|-------|
| INV-1 to INV-17 | `tests/selftest.cpp`, new blocks; the core alone |
| INV-1, INV-18, INV-19 | `tests/uitest.cpp`, offscreen |

Each self-test block is seen to fail first by breaking the rule its fixture
isolates — INV-6 by counting dora in `qualifying`, INV-8 by a `deadWall` of 0,
INV-9 by leaving claimed throws out, INV-12 by a `settleDraw` that pays
nothing — then seen to pass.

## 8. Alternatives considered (and rejected)

- **Riichi as branches in `Hand`.** `docs/design.md` § Mahjong says a rule
  set arrives as a `RuleSet` and the table never asks which. `TableRules`
  keeps that: the table asks what to do, never who it is.
- **One virtual per table rule.** A member on `RuleSet` per field, each
  overridden only by Riichi. One struct of plain values is read in one call.
- **A new save version carrying the counters and sticks.** They are what the
  replay gives; storing them would let a save disagree with itself, which
  `docs/design.md` § Mahjong rules out.
- **Head bump** — only the first winner after the discarder. The 2025 book
  lets every claimant win.
- **Abortive draws** (nine terminals, four winds, four riichi, four kongs).
  Not in the 2025 book; four kongs play on (§ 3.3.5).
- **Red fives.** The book uses none (§ 1.3), and the owner kept the tile art.
- **Counted yakuman at 13 han.** The book stops at Sanbaiman.

## 9. Out of scope

- The computer defending against a riichi — GHUB-0256.
- The hub tile's blurb, which names Hong Kong rules only — deferred; not yet
  queued.
- Computer strengths — deferred; not yet queued (GHUB-0239 § 9).
- The tournament time limit, chombo and dead hands — § 3.

## 10. What checks this

| Rule | What catches a breach |
|------|----------------------|
| INV-1 | `tests/selftest.cpp`, the yaku-list block; `tests/uitest.cpp`, the words block |
| INV-2 | `tests/selftest.cpp`, the yaku blocks |
| INV-3 | `tests/selftest.cpp`, the strike block |
| INV-4 | `tests/selftest.cpp`, the minipoints block |
| INV-5 | `tests/selftest.cpp`, the payment block |
| INV-6 | `tests/selftest.cpp`, the yaku-needed block |
| INV-7 | `tests/selftest.cpp`, the dora block |
| INV-8 | `tests/selftest.cpp`, the dead-wall block and the four-computer games |
| INV-9 | `tests/selftest.cpp`, the furiten block |
| INV-10 | `tests/selftest.cpp`, the riichi block |
| INV-11 | `tests/selftest.cpp`, the claims block |
| INV-12 | `tests/selftest.cpp`, the drawn-hand block and the four-computer games |
| INV-13 | `tests/selftest.cpp`, the counters block |
| INV-14 | `tests/selftest.cpp`, the liability block |
| INV-15 | `tests/selftest.cpp`, the four-computer games and the final-score block |
| INV-16 | `tests/selftest.cpp`, the existing Hong Kong and Chinese Official blocks |
| INV-17 | `tests/selftest.cpp`, the four-computer games |
| INV-18 | `tests/uitest.cpp`, the rule-set block and the save corpus check |
| INV-19 | `tests/uitest.cpp`, the Riichi words block; `scripts/translatable-check.py` |
| § 4.7's open values against the book | **Partial:** INV-1 checks the table against the code; that the table matches the book is a reading nothing re-checks |
| § 4.10 how well the computer plays | **nothing** — INV-17 checks it wins, not that it plays well |
| The words read well, slowly | **nothing** — judged by eye on a `--shot` |

## 11. Cross-doc impact

These clauses change, and are annotated at the build, as GHUB-0243 annotated
GHUB-0239:

- `docs/specs/GHUB-0239-four-player-mahjong.md` — § 4.3: the wall's reserve,
  the added kong's tile and two winners are Hong Kong's, with Riichi's in this
  spec; INV-1 counts the tiles a rule set plays with; INV-2's "two Win claims
  go to the first seat" is Hong Kong's and Chinese Official's; INV-8's "no
  points move" likewise; INV-11's menu holds three since this item; § 4.6's
  interface points here.
- `docs/specs/GHUB-0243-chinese-official-mahjong.md` — § 4.2 and INV-9:
  `dealerRepeats()` becomes `dealerKeepsDeal`; INV-12's menu holds three;
  § 4.4's `FullyConcealed` meaning reads "no set claimed".
- `tests/uitest.cpp` — the existing rule-set check counts two menu entries;
  it counts three.
- `docs/design.md` — § Mahjong: the third rule set, `TableRules`, the dead
  wall, the computer's riichi.
- `CHANGELOG.md` — the release entry.
- `ROADMAP.md` — through the roadmap verbs only.

## 12. Cold-eyes loop log

Rows live in `../reviews/GHUB-0244-japanese-riichi-mahjong-loop-log.md`.

## 13. Resource cost

No new dependency. `Game` keeps two more numbers, the counters and the
sticks; `Hand` keeps each seat's throws and its riichi and furiten state. The
save does not grow beyond one move per declaration.

## 14. Open questions

None. The one the draft raised is settled in § 3.
