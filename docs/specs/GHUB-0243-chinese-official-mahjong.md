<!-- ants-spec-format: 1 -->
# GHUB-0243 — Chinese Official rules for four-player Mahjong

**Status:** accepted (2026-11-18).
**Kind:** feature.
**Source:** ROADMAP GHUB-0243 (user-request-2026-11-11; the owner's decision
of 2026-11-18, recorded in § 3).
**Blocked by:** GHUB-0239.  **Blocker for:** GHUB-0244.

**Layman:** four-player Mahjong gains the tournament rules, chosen from the
same Rule set menu as Hong Kong. A hand needs 8 points to win, from a list of
81 named patterns, and the rules screen says in one plain sentence what each
pattern is, for both rule sets.

## 1. Goal

The Rule set menu of the `Mahjong` game lists Chinese Official beside Hong
Kong. Under it, a hand wins with at least 8 points, flowers aside, scored
from the 81 patterns of the 2006 Mahjong Competition Rules. Seven pairs,
Thirteen orphans and the knitted hands win as well as four sets and a pair.
Each pattern a win holds is named on the table with its points, and the
patterns that the rule book says cannot be scored together are not. The deal
passes after every hand, and payment is 8 points from every loser plus the
hand's value from whoever pays it. The computers aim for 8 points and win
hands. The rules screen lists every pattern of the rule set in force with a
plain sentence saying what it is, under Hong Kong as well.

## 2. Problem

1. **`RuleSetId` has one value.** `mahjong::ruleSets()` lists Hong Kong
   alone, and the comment on `RuleSetId` keeps 2 for this item.
2. **`bestWin` reads two shapes only.** It splits a hand into four sets and a
   pair, and tries Thirteen orphans, flagged by `WinContext::thirteenOrphans`.
   Chinese Official also wins with Seven pairs and the knitted hands (§ 4.3).
3. **`WinContext` cannot say how the hand waited.** Five patterns need it:
   Edge, Closed and Single wait score only when one tile alone would have
   finished the hand; Last tile needs the winning tile's other three copies
   on view; Last tile claim needs a win on the final discard. `Hand::winFor`
   sets `lastTile` only for a tile drawn from the wall, by the owner's ruling
   of 2026-11-12 for Hong Kong.
4. **The dealer repeats in every rule set.** `Game::settle` passes the deal
   only when someone other than the dealer wins. The rule book passes it after
   every hand: "the dealer should pass the dice to the right, regardless of
   whether he wins the hand or not" (§ 3.4.8).
5. **The minimum is read against the whole total.** `Hand::turnMoves` and
   the claim code compare `HandScore::total` with `minimumToWin()`. Chinese
   Official counts flower points in the total but not toward the 8 (§ 3.4.22,
   § 3.9.1).
6. **`HongKongRules::fanList` lists every `Fan`.** It loops over `kFanCount`,
   so a `Fan` added for another rule set would land in Hong Kong's list.
7. **The view's words assume Hong Kong.** `MahjongTableView::rulesText` prints
   a points table from `limit()` and `points()` and a Hong Kong payment
   paragraph; `endOfHandLines` says "fan" and "points" as Hong Kong means
   them. `MahjongTableView::fanName` names a pattern and nothing says what it
   is.
8. **The computer plays toward 3 fan.** `chooseMove`'s `Plan` knows one suit
   and all pungs. Neither reaches 8 points on its own often enough.

Source for every rule here: the World Mahjong Organization's *Mahjong
Competition Rules* (2006), the English rule book, cited by section.
Source: <https://mahjong.dk/hubfs/Mahjong%20documents/MCR/MCR_Rulebook_2006.pdf>.
Source for the account-once reading in § 4.5:
<https://sloperama.com/mjfaq/mjfaq22.html>.

## 3. Scope decisions (agreed with the user)

- **Every pattern gets a plain sentence on the rules screen, for both rule
  sets.** The owner, 2026-11-18, taking the recommendation.
- **The tournament rules as written.** The roadmap item asks for "the
  tournament rule set" and "the official list". So flowers stay in play, one
  point each and none toward the minimum; the deal passes every hand; payment
  is the book's (§ 4.6). The author's reading, stated to the owner
  2026-11-18 before drafting.
- **The 2006 rule book, not the 2014 revision.** The 2006 English book is the
  one this author could read; the 2014 text was not found in English.
- **One word per meaning.** Where a Chinese Official pattern means exactly
  what a Hong Kong `Fan` means, it reuses that `Fan` and its words, so Full
  flush reads "Pure one suit" under both. The author's call: one sentence per
  meaning, and the owner reads the words, not the book's names.
- **No seat changes between rounds.** The book's tournament seating is for
  fairness between strangers; you always sit at the bottom.

## 4. Design

### 4.1 Where the parts live

| Part | Where | Half |
|------|-------|------|
| `ChineseOfficialRules`, its fan list and its exclusions | `src/mahjong/mahjongofficial.*`, new | core |
| `RuleSet` additions, the shapes in `bestWin` | `src/mahjong/mahjongrules.*` | core |
| The win flags, the dealer, the minimum | `src/mahjong/mahjongtable.*` | core |
| `fanName` and `fanMeaning` for every `Fan` | a new view file in `src/mahjong/` holding those two `MahjongTableView` members | view |
| The rules text, the end of a hand, the menu label | `src/mahjong/mahjongtableview.cpp` | view |

### 4.2 The interface

```cpp
namespace mahjong {
enum class RuleSetId : std::uint8_t { HongKong = 1, ChineseOfficial = 2 };

// How the fourteen tiles make a winning hand.
enum class Shape : std::uint8_t {
    Sets,              // four sets and a pair
    SevenPairs,        // seven pairs; four alike are two pairs
    ThirteenOrphans,   // one of each terminal and honour, one doubled
    KnittedStraight,   // a knitted straight (§ 4.3), one set and a pair
    HonoursAndKnitted, // fourteen different singles, § 4.3
};

struct WinContext {
    // ... every field it has today, except thirteenOrphans, which Shape replaces
    Shape shape = Shape::Sets;
    // KnittedStraight and HonoursAndKnitted: the lowest tile of each suit's
    // knitted run (1-4-7, 2-5-8 or 3-6-9), one per suit; else all 0.
    std::array<Face, 3> knitted {};
    // Which part of this reading holds the winning tile: an index into sets,
    // or -1 for the pair, or -2 when the shape has no sets.
    int winningIn = -2;
    // How many different faces would have finished the hand before the
    // winning tile came, under any shape the rule set allows.
    int waits = 0;
    bool lastTileClaim = false; // won on the discard after the wall's last tile
    bool lastOfItsKind = false; // the winning tile's three others are on view
};

struct HandScore {
    // ... every field it has today, and:
    int qualifying = 0; // the part of total the minimum is read against
};

class RuleSet {
public:
    // ... every member it has today, and:
    virtual bool shapeAllowed(Shape shape) const = 0;
    // Whether the dealer keeps the deal after winning or after a drawn hand.
    virtual bool dealerRepeats() const = 0;
};
}
```

Hong Kong allows `Sets` and `ThirteenOrphans`, repeats the dealer, and sets
`qualifying` to `total`. Its scoring reads `shape` where it read
`thirteenOrphans`, and ignores the new flags. **`HongKongRules::fanList`
lists § 4.4 of GHUB-0239 only**, not every `Fan`.

**The table reads only the interface.** `Hand` asks `shapeAllowed` through
`bestWin`, compares `qualifying` with `minimumToWin()`, and `Game::settle`
asks `dealerRepeats()`. Nothing in `Hand` or `Game` names a rule set.

### 4.3 Reading a hand

`bestWin` tries every shape the rule set allows, every way the tiles allow,
and keeps the best score. A reading is scored once for each part of it that
could hold the winning tile, with `winningIn` set to that part (book § 3.9.1
5(4), the freedom to choose the higher score).

- **Sets.** As today. On a discard, the set holding the winning tile counts as
  claimed: a pung is not concealed, a chow costs nothing. A reading no part of
  which holds the winning tile — only a test that names none — is kept whole,
  with `winningIn` at -2.
- **Seven pairs.** No sets laid down; fourteen tiles in seven pairs, four
  alike counting as two.
- **Thirteen orphans.** As today, under `Shape::ThirteenOrphans`.
- **Knitted straight.** A knitted run is three tiles of one suit three apart:
  1-4-7, 2-5-8 or 3-6-9. A knitted straight is all three runs, each in a
  different suit — nine tiles, six ways to assign the runs. Held concealed,
  plus one set (which may be laid down) and a pair.
- **Honours and knitted.** No sets laid down; fourteen different tiles, each
  an honour or a tile of the three knitted runs under one assignment.

**`waits` is counted once per call.** `bestWin` takes one copy of the winning
tile out and tries each of the 34 faces in its place; `waits` is how many make
a hand of any allowed shape, the minimum aside. A hand that was not one tile
from complete cannot happen here, since a win was found.

**The flags come from the table.** `Hand::winOn` sets `lastTileClaim` when the
wall is empty and the win is on a discard. Both `winFor` and `winOn` set
`lastOfItsKind` when the other three copies of the winning tile lie in the
pools or in sets laid face up; a concealed kong is face down and does not
count. `kongReplacement` is false when the tile won on replaced a flower that
a kong's replacement drew (book, Out with replacement tile).

### 4.4 The fan list

Points, the `Fan` that carries it, what it means, and what it is not scored
with. A `Fan` in *italics* is Hong Kong's, reused (§ 3). An entry in the last
column marked † is not in the book's list for that pattern; it is inevitably
implied, so the book's Non-Repeat principle (§ 3.9.1 5(1)) rules it out.

"Element" means a set or the pair, or in Seven pairs a pair. "Pung" includes
a kong. "Set laid down" excludes a concealed kong.

| Pts | `Fan` | Means | Not scored with |
|----:|-------|-------|-----------------|
| 88 | *`BigFourWinds`* | A pung of each of the four winds | Big three winds, All pungs, Prevalent wind, Seat wind, Pung of terminals or honours, Little four winds† |
| 88 | *`BigThreeDragons`* | A pung of each of the three dragons | Two dragon pungs, Dragon pung, Little three dragons† |
| 88 | `AllGreen` | Only 2, 3, 4, 6 and 8 of bamboo and the green dragon | — |
| 88 | *`NineGates`* | 1112345678999 of one suit held concealed, won on any tile of that suit | Full flush, Concealed hand, Pung of terminals or honours, No honours† |
| 88 | *`FourKongs`* | Four kongs | Single wait, Three kongs†, Two concealed kongs†, Two melded kongs†, Concealed kong†, Melded kong†, All pungs† |
| 88 | `SevenShiftedPairs` | Seven pairs of one suit, each one higher than the last | Full flush, Concealed hand, Single wait, Seven pairs†, No honours† |
| 88 | *`ThirteenOrphans`* | One of every one, nine, wind and dragon, and a second of one of them | All types, Concealed hand, Single wait |
| 64 | *`AllTerminals`* | Only ones and nines | All pungs, Outside hand, Pung of terminals or honours, No honours, All terminals and honours† |
| 64 | *`SmallFourWinds`* | Pungs of three winds and a pair of the fourth | Big three winds, Pung of terminals or honours |
| 64 | *`SmallThreeDragons`* | Pungs of two dragons and a pair of the third | Dragon pung, Two dragon pungs |
| 64 | *`AllHonours`* | Only winds and dragons | All pungs, Outside hand, Pung of terminals or honours, All terminals and honours† |
| 64 | *`FourConcealedPungs`* | Four pungs, none of them claimed | All pungs, Concealed hand, Three concealed pungs†, Two concealed pungs† |
| 64 | `PureTerminalChows` | Two 1-2-3 chows and two 7-8-9 chows of one suit, and a pair of fives of it | Seven pairs, Full flush, All chows, Pure double chow, Two terminal chows, No honours† |
| 48 | `QuadrupleChow` | Four identical chows | Pure shifted pungs, Tile hog, Pure double chow, Pure triple chow† |
| 48 | `FourPureShiftedPungs` | Four pungs of one suit, each one higher than the last | Pure triple chow, All pungs, Pure shifted pungs† |
| 32 | `FourPureShiftedChows` | Four chows of one suit, each one higher than the last, or each two higher | Short straight, Pure shifted chows† |
| 32 | `ThreeKongs` | Three kongs | Two concealed kongs†, Two melded kongs†, Concealed kong†, Melded kong† |
| 32 | `AllTerminalsAndHonours` | Only ones, nines, winds and dragons | All pungs, Pung of terminals or honours, Outside hand† |
| 24 | `SevenPairs` | Seven pairs | Concealed hand, Single wait |
| 24 | `GreaterHonoursKnitted` | All seven honours, and seven tiles of a knitted straight, all different | All types, Concealed hand, Lesser honours and knitted tiles† |
| 24 | `AllEvenPungs` | Pungs and the pair, all of 2, 4, 6 or 8 | All pungs, All simples, No honours† |
| 24 | *`PureOneSuit`* | Every tile of one suit | No honours |
| 24 | `PureTripleChow` | Three identical chows | Pure shifted pungs, Pure double chow |
| 24 | `PureShiftedPungs` | Three pungs of one suit, each one higher than the last | Pure triple chow |
| 24 | `UpperTiles` | Only sevens, eights and nines | No honours, Upper four† |
| 24 | `MiddleTiles` | Only fours, fives and sixes | No honours, All simples |
| 24 | `LowerTiles` | Only ones, twos and threes | No honours, Lower four† |
| 16 | `PureStraight` | 1-2-3, 4-5-6 and 7-8-9 of one suit | — |
| 16 | `ThreeSuitedTerminalChows` | 1-2-3 and 7-8-9 in each of two suits, and a pair of fives of the third | Two terminal chows, No honours, All chows, Pure double chow |
| 16 | `PureShiftedChows` | Three chows of one suit, each one higher than the last, or each two higher | — |
| 16 | `AllFives` | A five in every element | All simples |
| 16 | `TriplePung` | Pungs of one number in all three suits | — |
| 16 | `ThreeConcealedPungs` | Three pungs, none of them claimed | Two concealed pungs† |
| 12 | `LesserHonoursKnitted` | Fourteen different tiles, honours and tiles of a knitted straight | All types, Concealed hand |
| 12 | `KnittedStraight` | 1-4-7, 2-5-8 and 3-6-9, each in a different suit | — |
| 12 | `UpperFour` | Only sixes to nines | No honours |
| 12 | `LowerFour` | Only ones to fours | No honours |
| 12 | `BigThreeWinds` | Pungs of three winds | — |
| 8 | `MixedStraight` | 1-2-3, 4-5-6 and 7-8-9, each in a different suit | — |
| 8 | `ReversibleTiles` | Only tiles that look the same upside down: 1, 2, 3, 4, 5, 8 and 9 of circles, 2, 4, 5, 6, 8 and 9 of bamboo, and the white dragon | One voided suit |
| 8 | `MixedTripleChow` | The same chow in all three suits | — |
| 8 | `MixedShiftedPungs` | Pungs in all three suits, each one higher than the last | — |
| 8 | `ChickenHand` | A hand that scores nothing else, flowers aside | — |
| 8 | *`LastTile`* | Won on the last tile of the wall, drawn yourself | Self-drawn |
| 8 | `LastTileClaim` | Won on the discard after the last tile of the wall | — |
| 8 | *`KongReplacement`* | Won on a kong's replacement tile | Self-drawn |
| 8 | *`RobbingAKong`* | Won on a tile another player added to a pung | Last of its kind |
| 8 | `TwoConcealedKongs` | Two kongs, neither claimed | Concealed kong†, Two concealed pungs† |
| 6 | *`AllPungs`* | Four pungs and a pair | — |
| 6 | *`MixedOneSuit`* | One suit and honours | — |
| 6 | `MixedShiftedChows` | A chow in each suit, each one higher than the last | — |
| 6 | `AllTypes` | Circles, bamboo, characters, winds and dragons all in the hand | — |
| 6 | `MeldedHand` | Four sets laid down, won on a discard that pairs your last tile | Single wait |
| 6 | `TwoDragonPungs` | Pungs of two dragons | Dragon pung† |
| 4 | `OutsideHand` | A one, a nine or an honour in every element | — |
| 4 | `FullyConcealed` | No set laid down, won on a tile you drew | Self-drawn†, Concealed hand† |
| 4 | `TwoMeldedKongs` | Two kongs, at least one claimed | Melded kong† |
| 4 | `LastOfItsKind` | Won on a tile whose other three are already on view | — |
| 2 | *`DragonPung`* | A pung of dragons, each | — |
| 2 | *`PrevailingWindPung`* | A pung of the round's wind | — |
| 2 | *`SeatWindPung`* | A pung of your own wind | — |
| 2 | *`ConcealedHand`* | No set laid down, won on a discard | — |
| 2 | `AllChowsNoHonours` | Four chows and a pair, no honours; a knitted straight counts as three chows | No honours |
| 2 | `TileHog` | All four of one suit tile, none of them a kong, each | — |
| 2 | `DoublePung` | Pungs of one number in two suits | — |
| 2 | `TwoConcealedPungs` | Two pungs, neither claimed | — |
| 2 | `ConcealedKong` | A kong of four tiles you drew | — |
| 2 | `AllSimples` | No ones, nines or honours | No honours† |
| 1 | `PureDoubleChow` | Two identical chows | — |
| 1 | `MixedDoubleChow` | The same chow in two suits | — |
| 1 | `ShortStraight` | Two chows of one suit making six in a row | — |
| 1 | `TwoTerminalChows` | 1-2-3 and 7-8-9 of one suit | — |
| 1 | `TerminalOrHonourPung` | A pung of ones, nines or winds, each | — |
| 1 | `MeldedKong` | A kong claimed, or made by adding to a pung | — |
| 1 | `OneVoidedSuit` | Tiles of exactly two of the three suits | — |
| 1 | `NoHonours` | No winds or dragons | — |
| 1 | `EdgeWait` | Waited only on the 3 of 1-2 or the 7 of 8-9 | — |
| 1 | `ClosedWait` | Waited only on the middle of a chow | — |
| 1 | `SingleWait` | Waited only on the tile that made the pair | — |
| 1 | *`SelfDrawn`* | Won on a tile you drew | — |
| 1 | `Flower` | A flower or season, each; not counted toward the 8 | — |

The rows give 81 patterns; a test counts them (INV-1). Points, meanings and
the book's exclusions are from book § 3.8.1 and its appendix; the meanings are
reworded plainly. Reversible tiles' tile list is the book's.

**Where the book's own list was not followed.** Three-suited terminal chows
lists Pure double chow, which that hand cannot hold; its two chow pairs are
Mixed double chows, and § 4.5's four-chow rule keeps them unscored. Four kongs
"Points for concealed pungs may be added" and Three kongs "may combine with
Three Concealed Pungs if the Kongs are all concealed" are followed as written.

**The minimum, and no limit.** `minimumToWin()` is 8, and `qualifying` is the
total less the `Flower` items. There is no limit: `limit()` returns 0, which
the view reads as "none".

### 4.5 Counting a reading

`ChineseOfficialRules::score` counts one reading in five steps.

1. **The patterns of the whole hand and of single sets**, by the table's
   meanings: the shape patterns, the suit and number patterns, the honour
   patterns, each `DragonPung`, `TerminalOrHonourPung` and `TileHog`, the
   kong and concealed-pung patterns (below), and the patterns of how the hand
   was won.
2. **The patterns that combine sets**, by the account-once rule (below).
3. **Strike every pattern a scored pattern's row rules out.**
4. **`ChickenHand`** when nothing is left but flowers.
5. **One `Flower` item per bonus tile.**

**Kongs and concealed pungs.** Four kongs score `FourKongs`, three
`ThreeKongs`. Two concealed kongs score `TwoConcealedKongs`; two claimed score
`TwoMeldedKongs`; one of each scores `TwoMeldedKongs` and `ConcealedKong` (6
points, book Two melded kongs). One kong scores `ConcealedKong` or
`MeldedKong`. Separately, a concealed pung or concealed kong counts toward
`TwoConcealedPungs`, `ThreeConcealedPungs` and `FourConcealedPungs`.

**Pungs that are already a pattern.** A wind pung scored as the seat or
prevailing wind, or inside Big three winds, scores no `TerminalOrHonourPung`.
A dragon pung is never one; it scores `DragonPung` unless a three-dragon or
two-dragon pattern holds it. This is per pung: another pung of ones or nines
in the same hand still scores, as the book's Two melded kongs example shows.
Where a row's last column names Pung of terminals or honours, the whole
pattern is struck, as the book lists it.

**Waits.** With `waits == 1` only: `winningIn == -1` scores `SingleWait`; the
middle of a chow scores `ClosedWait`; the 3 of 1-2-3 or the 7 of 7-8-9 scores
`EdgeWait`. With more than one wait, none (book, Edge, Closed and Single wait).

**The account-once rule** (book § 3.9.1 5(5): "When you have combined some sets
to create a fan, you can only combine any remaining sets once with a set that
has already been used"). The chows are taken first, then the pungs, each the
same way; no pattern mixes a chow with a pung.

- **Four-set patterns** take all four and leave no combining pattern among
  them: `QuadrupleChow`, `FourPureShiftedChows`, `PureTerminalChows`,
  `ThreeSuitedTerminalChows`, `FourPureShiftedPungs`.
- **Otherwise, the better of two ways.** (a) Three sets making a three-set
  pattern — `PureTripleChow`, `PureStraight`, `PureShiftedChows`,
  `MixedStraight`, `MixedTripleChow`, `MixedShiftedChows`, or
  `PureShiftedPungs`, `TriplePung`, `MixedShiftedPungs` — score its highest,
  and the fourth set may make one two-set pattern with one of the three.
  (b) Two-set patterns only — `PureDoubleChow`, `MixedDoubleChow`,
  `ShortStraight`, `TwoTerminalChows`, `DoublePung` — chosen so that each
  pattern brings in a set no earlier one used. Equivalently, the chosen pairs
  never form a loop, so four sets score at most three.
- The knitted straight is not three chows here; it is its own pattern.

Example from the FAQ: 2-3-4 and 5-6-7 in both circles and characters hold two
Mixed double chows and two Short straights, and score three of them (3
points), since the fourth would close a loop.

### 4.6 Payment and the deal

`ChineseOfficialRules::settle`, book § 3.9.1 3: **on a discard, or a robbed
kong, the one who let the tile go pays 8 plus the hand's points, and the other
two pay 8 each. Self-drawn, each of the three pays 8 plus the points.**
`HandScore::points` is the hand's points, flowers included. Payments sum to
zero.

`dealerRepeats()` is false: the deal passes after every hand, won or drawn,
so an East round is four hands and a full game sixteen. `Game::over` already
counts deals passed.

### 4.7 The computer

**It plays as under Hong Kong, unchanged.** Any complete hand that scores
nothing else is a Chicken hand, worth 8, so nearly every complete hand
qualifies, and the computer's Hong Kong play reaches one often. Measured at
the build: 20 of 24 hands won across six seeded East rounds, which INV-11
prints. It always wins when it can, as now.

### 4.8 The view

- **The menu** lists "Chinese Official rules" after "Hong Kong rules", from
  `ruleSets()`.
- **`fanMeaning(Fan)`** gives each `Fan` one plain sentence, from the
  table's Means column for Chinese Official and from GHUB-0239 § 4.4 for Hong
  Kong. A reused `Fan` has one sentence, true under both.
- **The rules text** reads the rule set in force. Its pattern list is grouped
  by value, highest first, each line the name, the value and the meaning. The
  minimum, the limit, the payment and the dealer paragraphs are the view's
  sentences for the rule set in force; Chinese Official's say 8 points and no limit, the
  payment of § 4.6, the deal passing every hand, and that flowers count but
  not toward the 8.
- **The end of a hand** says "points" for Chinese Official's values ("Seven
  pairs — 24 points", "Total: 30 points"), and "fan" as now for Hong Kong.

### 4.9 Saves and settings

No format change. A save stores the `RuleSetId` already, and
`knownRuleSet(2)` becomes true. A Hong Kong save restores as before. A
Chinese Official save read by a build before this one is refused, as
GHUB-0239 § 6 says of any unknown rule set.

## 5. Invariants

These clauses cannot run yet; the surfaces they test are what this spec
creates. Each names the rule its fixture isolates.

- **INV-1** — `ChineseOfficialRules::fanList()` holds exactly the § 4.4 rows,
  with their points, and every one has a non-empty `fanName` and
  `fanMeaning`.
  *Test:* `tests/selftest.cpp` counts the list and checks each row's points;
  `tests/uitest.cpp` checks the words for every `Fan` of both lists.
  *Breaks when:* a row is missed, or a `Fan` has no meaning.

- **INV-2** — Each pattern scores on a hand that holds it, and not on one
  that does not.
  *Test:* `tests/selftest.cpp`, one fixture hand per § 4.4 row through
  `bestWin`, asserting the item. Each fixture isolates its row's meaning: a
  near miss beside it (one tile changed, or the way of winning) loses that
  item. Four kongs, Three kongs, Two concealed kongs, Two melded kongs,
  Concealed kong, Melded kong and Thirteen orphans have no near miss; their
  sets are laid down as the pattern.
  *Breaks when:* a pattern is detected by the wrong test, so its near miss
  still scores it.

- **INV-3** — A pattern ruled out by a scored pattern's row is not scored.
  *Test:* `tests/selftest.cpp`. Big four winds with a seat-wind pung scores no
  Seat wind, Big three winds or All pungs; Fully concealed scores no
  Self-drawn; Seven pairs scores no Concealed hand. Each fixture would score
  the struck pattern without step 3 of § 4.5.
  *Breaks when:* the exclusions are not applied, or applied by value rather
  than by the table.

- **INV-4** — The account-once rule: four chows in two suits of 2-3-4 and
  5-6-7 score three of their four two-chow patterns; 1-2-3, 4-5-6, 7-8-9 and
  a second 1-2-3 of one suit score Pure straight and exactly one two-chow
  pattern.
  *Test:* `tests/selftest.cpp`, both hands, counting the two-chow items. Without
  the loop rule the first scores 4; without the one-more-once rule the second
  scores none or several.
  *Breaks when:* every pair of chows is scored, or none beyond the three-set
  pattern.

- **INV-5** — Wait patterns score only when one face would have finished the
  hand.
  *Test:* `tests/selftest.cpp`. A hand holding 1-2 of bamboo, won on the 3,
  scores Edge wait; the same hand with 4-5 of bamboo for 1-2, waiting on 3 or
  6, scores no wait pattern. Only `waits` differs.
  *Breaks when:* the wait is read from the reading alone.

- **INV-6** — The minimum is 8 without flowers.
  *Test:* `tests/selftest.cpp`. A complete hand worth 7 points plus two
  flowers is offered no Win on a discard; the same hand drawn yourself, 9
  points, is. Both are complete, so only the minimum rule can refuse the
  first.
  *Breaks when:* flowers count toward the minimum.

- **INV-7** — Seven pairs, Thirteen orphans and both knitted shapes win
  under Chinese Official and not under Hong Kong.
  *Test:* `tests/selftest.cpp`, each shape through `bestWin` under both rule
  sets: scored under Chinese Official, nothing under Hong Kong save Thirteen
  orphans.
  *Breaks when:* `bestWin` tries a shape without asking `shapeAllowed`.

- **INV-8** — Payments follow § 4.6 and sum to zero.
  *Test:* `tests/selftest.cpp`: a 10-point win on a discard takes 18 from the
  discarder and 8 from each other seat; self-drawn, 18 from each. The seeded
  games assert the zero sum after every hand.
  *Breaks when:* the other two pay nothing on a discard win, as Hong Kong.

- **INV-9** — Under Chinese Official the deal passes after every hand, so an
  East round is four hands; Hong Kong's dealer still repeats.
  *Test:* `tests/selftest.cpp`, seeded four-computer East rounds under both
  rule sets: Chinese Official ends after exactly four hands, each with a new
  dealer; GHUB-0239 INV-8 and INV-9 still hold for Hong Kong.
  *Breaks when:* `Game::settle` ignores `dealerRepeats()`.

- **INV-10** — Last tile claim, Last of its kind and Out with replacement
  tile follow § 4.3.
  *Test:* `tests/selftest.cpp`, arranged hands: a win on the discard after
  the wall empties scores Last tile claim; a win on a tile whose three others
  lie in the pools scores Last of its kind, and with two there does not; a win
  on a flower's replacement after a kong is self-drawn and not Out with
  replacement tile.
  *Breaks when:* fewer than three on view count, or a flower replacement
  counts as a kong's.

- **INV-11** — Four-computer games under Chinese Official end, every win
  qualifies, and some hands are won.
  *Test:* `tests/selftest.cpp`, six seeded East rounds: each ends; every win's
  `qualifying` is at least 8; at least one hand in five is won. It prints the
  number won.
  *Breaks when:* the computer stops completing hands, so every hand draws.

- **INV-12** — The menu lists both rule sets, Chinese Official is remembered,
  and a Chinese Official save restores.
  *Test:* `tests/uitest.cpp`: the menu holds two entries; set Chinese Official,
  build a fresh view, assert it is in force; save mid-hand and restore into a
  fresh view, comparing `saveState()`.
  *Breaks when:* the setting or the save falls back to Hong Kong.

## 6. Failure modes

- **A Chinese Official save in an older build** — refused, and the hub keeps
  its fresh game (GHUB-0239 § 6).
- **Two readings of one hand score the same** — either is right; `bestWin`
  keeps the first, and the total is the same.
- **A hand reads as two shapes** — Seven pairs and Pure terminal chows hold
  the same tiles. Both readings are scored and the higher kept, as the book's
  freedom-to-choose principle says.
- **The book is ambiguous** — the account-once rule is the book's most argued
  rule (the FAQ cited in § 2). § 4.5 states one reading and INV-4 pins it; a
  different reading is a change to § 4.5.
- **The computer rarely reaches 8** — INV-11 notices. Hands then draw, and
  the deal still passes, so the game still ends.
- **Scoring cost** — `waits` adds 34 shape checks to each `bestWin` that
  finds a win. Measured at the build: the whole self-test ran in 4.4 to 4.6
  seconds against 4.2 to 4.5 before, while playing six more games.

## 7. Tests

| Invariant | Where |
|-----------|-------|
| INV-1 to INV-11 | `tests/selftest.cpp`, new blocks; the core alone |
| INV-1, INV-12 | `tests/uitest.cpp`, offscreen |

Each self-test block is seen to fail first by breaking the rule its fixture
isolates — INV-3 by skipping § 4.5 step 3, INV-4 by scoring every chow pair,
INV-6 by counting flowers — then seen to pass.

## 8. Alternatives considered (and rejected)

- **A Mahjong scoring library.** Open-source Chinese Official calculators
  exist, but a dependency for one rule set is more than this project carries
  anywhere else, and its exclusions would be its authors' reading rather
  than one written here and tested.
- **A separate `Fan` for every Chinese Official pattern.** Twenty-one would
  duplicate Hong Kong ones word for word, and two sentences for one meaning
  drift (§ 3).
- **Exclusions by "a higher pattern hides the lower ones in its tiles".** Not
  what the book does: it lists exclusions per pattern, and some lower
  patterns combine with higher ones on the same tiles (Nine gates with Pure
  straight, in its appendix).
- **The tournament seat changes.** § 3: no strangers to be fair to.
- **A points limit**, as Hong Kong's 10 fan. The book has none.

## 9. Out of scope

- Telling you how much a complete hand is worth when Win is not offered —
  deferred; not yet queued. Hong Kong has the same gap.
- The 2014 revision of the rules — deferred; not yet queued.
- Japanese Riichi rules — tracked by GHUB-0244.
- Computer strengths — deferred; not yet queued (GHUB-0239 § 9).

## 10. What checks this

| Rule | What catches a breach |
|------|----------------------|
| INV-1 | `tests/selftest.cpp`, the fan-list block; `tests/uitest.cpp`, the words block |
| INV-2 | `tests/selftest.cpp`, the pattern blocks |
| INV-3 | `tests/selftest.cpp`, the exclusion block |
| INV-4 | `tests/selftest.cpp`, the account-once block |
| INV-5 | `tests/selftest.cpp`, the wait block |
| INV-6 | `tests/selftest.cpp`, the minimum block |
| INV-7 | `tests/selftest.cpp`, the shapes block |
| INV-8 | `tests/selftest.cpp`, the payment block and the four-computer games |
| INV-9 | `tests/selftest.cpp`, the four-computer games |
| INV-10 | `tests/selftest.cpp`, the last-tile block |
| INV-11 | `tests/selftest.cpp`, the four-computer games |
| INV-12 | `tests/uitest.cpp`, the rule-set block |
| § 4.4 † exclusions | **Partial:** INV-3 samples them; a † entry that is wrong is caught only where a fixture holds both patterns |
| § 4.7 how well the computer plays | **nothing** — INV-11 checks it wins, not that it plays well |
| The plain sentences read well | **nothing** — judged by eye |

## 11. Cross-doc impact

- `docs/specs/GHUB-0239-four-player-mahjong.md` — annotated: INV-8 and INV-9
  hold under Hong Kong; INV-11's breaks-when names this item; § 4.3's winning
  hand and § 4.6's interface point here.
- `docs/design.md` — § Mahjong: the second rule set, the shapes, the words.
- `CHANGELOG.md` — the release entry.
- `ROADMAP.md` — through the roadmap verbs only.

## 12. Cold-eyes loop log

Rows live in `../reviews/GHUB-0243-chinese-official-mahjong-loop-log.md`.

## 13. Resource cost

No new dependency and no new state. The fan list is a fixed table.
