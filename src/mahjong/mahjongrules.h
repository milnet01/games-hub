#pragma once

#include "mahjongtiles.h"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

// The rule sets four-player Mahjong is played by (GHUB-0239 § 4.6): what a
// hand needs to win, how it scores and who pays. Hong Kong is the first; the
// interface is where Chinese Official (GHUB-0243) and Japanese Riichi
// (GHUB-0244) plug in. No Qt and no words: the view owns every sentence.
namespace mahjong {

// Stored in settings and saves, so a value never changes meaning. 3 is kept
// for GHUB-0244.
enum class RuleSetId : std::uint8_t { HongKong = 1, ChineseOfficial = 2 };

// How the fourteen tiles make a winning hand (GHUB-0243 § 4.3). A rule set
// says which it allows.
enum class Shape : std::uint8_t {
    Sets,              // four sets and a pair
    SevenPairs,        // seven pairs; four alike are two pairs
    ThirteenOrphans,   // one of each terminal and honour, one doubled
    KnittedStraight,   // a knitted straight, one set and a pair
    HonoursAndKnitted, // fourteen different singles: honours and knitted tiles
};

enum class SetKind : std::uint8_t { Chow, Pung, Kong };

// A set, laid down or held. A chow names its lowest tile. A concealed set was
// never claimed from a discard: a concealed kong, or a set still in the hand.
struct Meld {
    SetKind kind = SetKind::Pung;
    Face tile = 0;
    bool concealed = false;

    bool operator==(const Meld&) const = default;
};

// One scoring pattern. Hong Kong's come first, in GHUB-0239 § 4.4's order
// and then its limit hands; then the Chinese Official patterns no Hong Kong
// one already means (GHUB-0243 § 4.4). A Fan means one thing under every
// rule set that scores it, so the view has one name and one sentence for it.
enum class Fan : std::uint8_t {
    SelfDrawn,
    ConcealedHand,
    NoBonusTiles,
    OwnFlower,
    OwnSeason,
    AllFourFlowers,
    AllFourSeasons,
    AllChows,
    DragonPung,
    SeatWindPung,
    PrevailingWindPung,
    LastTile,
    KongReplacement,
    RobbingAKong,
    AllPungs,
    MixedOneSuit,
    SmallThreeDragons,
    PureOneSuit,
    BigThreeDragons,
    // The limit hands.
    SmallFourWinds,
    BigFourWinds,
    AllHonours,
    AllTerminals,
    ThirteenOrphans,
    NineGates,
    FourConcealedPungs,
    FourKongs,
    HeavenlyHand,
    EarthlyHand,
    // Chinese Official, by value as GHUB-0243 § 4.4 lists them.
    AllGreen,
    SevenShiftedPairs,
    PureTerminalChows,
    QuadrupleChow,
    FourPureShiftedPungs,
    FourPureShiftedChows,
    ThreeKongs,
    AllTerminalsAndHonours,
    SevenPairs,
    GreaterHonoursKnitted,
    AllEvenPungs,
    PureTripleChow,
    PureShiftedPungs,
    UpperTiles,
    MiddleTiles,
    LowerTiles,
    PureStraight,
    ThreeSuitedTerminalChows,
    PureShiftedChows,
    AllFives,
    TriplePung,
    ThreeConcealedPungs,
    LesserHonoursKnitted,
    KnittedStraight,
    UpperFour,
    LowerFour,
    BigThreeWinds,
    MixedStraight,
    ReversibleTiles,
    MixedTripleChow,
    MixedShiftedPungs,
    ChickenHand,
    LastTileClaim,
    TwoConcealedKongs,
    MixedShiftedChows,
    AllTypes,
    MeldedHand,
    TwoDragonPungs,
    OutsideHand,
    FullyConcealed,
    TwoMeldedKongs,
    LastOfItsKind,
    AllChowsNoHonours,
    TileHog,
    DoublePung,
    TwoConcealedPungs,
    ConcealedKong,
    AllSimples,
    PureDoubleChow,
    MixedDoubleChow,
    ShortStraight,
    TwoTerminalChows,
    TerminalOrHonourPung,
    MeldedKong,
    OneVoidedSuit,
    NoHonours,
    EdgeWait,
    ClosedWait,
    SingleWait,
    Flower,
};
constexpr int kFanCount = int(Fan::Flower) + 1;

struct ScoreItem {
    Fan fan;
    int value;
};

struct HandScore {
    std::vector<ScoreItem> items;
    int total = 0; // after the limit
    // The part of total the minimum is read against: Chinese Official leaves
    // the flowers out of it.
    int qualifying = 0;
    bool limit = false;
    int points = 0;
};

// Everything about how a hand was won that a rule set's scoring reads.
struct WinContext {
    Shape shape = Shape::Sets;
    // The sets, laid down and held alike, and the pair. Four sets for Sets,
    // one for KnittedStraight; empty and 0 for the shapes with neither.
    std::vector<Meld> sets;
    Face pair = 0;
    // KnittedStraight and HonoursAndKnitted: the lowest tile of each suit's
    // knitted run (1-4-7, 2-5-8 or 3-6-9), circles, bamboo, characters; else 0.
    std::array<Face, 3> knitted {};
    // Which part of this reading holds the winning tile: an index into sets,
    // -1 for the pair, or -2 for none (a shape with no sets, or the knitted
    // tiles).
    int winningIn = -2;
    // How many different faces would have finished the hand before the
    // winning tile came, under any shape the rule set allows.
    int waits = 0;
    // Every tile of the hand, the bonus tiles aside.
    std::vector<Face> tiles;
    std::vector<Face> bonus;
    Face winningTile = 0;
    bool selfDrawn = false;
    // A chow, pung or kong was claimed from a discard.
    bool claimedASet = false;
    int seatWind = 1;       // 1 east, 2 south, 3 west, 4 north
    int prevailingWind = 1; // the same
    bool lastTile = false;      // won on the wall's last tile, drawn yourself
    bool lastTileClaim = false; // won on the discard after the wall's last tile
    bool lastOfItsKind = false; // the winning tile's three others are on view
    bool kongReplacement = false;
    bool robbedKong = false;
    bool heavenly = false;
    bool earthly = false;
    int winner = -1;
    // Who pays: the discarder, or the player whose kong was robbed. -1 when
    // the winner drew the tile.
    int payer = -1;
};

class RuleSet
{
public:
    virtual ~RuleSet() = default;
    virtual RuleSetId id() const = 0;
    virtual bool chowAllowed() const = 0;
    virtual bool bonusTilesInPlay() const = 0;
    virtual int minimumToWin() const = 0;
    // The most a hand can score, and the points a total of `fan` is worth (0
    // below the minimum). The rules text reads both from here.
    virtual int limit() const = 0;
    virtual int points(int fan) const = 0;
    virtual HandScore score(const WinContext& win) const = 0;
    // Every pattern this rule set scores, with its value, in the order the
    // rules text lists them.
    virtual std::vector<ScoreItem> fanList() const = 0;
    // What each seat gains (or, negative, pays) for `win`. Sums to zero.
    virtual std::array<int, 4> settle(const WinContext& win, const HandScore& s) const = 0;
    // Whether a hand of this shape may win at all.
    virtual bool shapeAllowed(Shape shape) const = 0;
    // Whether the dealer keeps the deal after winning or after a drawn hand.
    virtual bool dealerRepeats() const = 0;
};

// Hong Kong Old Style, § 4.4 and § 4.5: 3 fan to win, 10 the limit.
class HongKongRules : public RuleSet
{
public:
    static constexpr int kMinimum = 3;
    static constexpr int kLimit = 10;

    RuleSetId id() const override { return RuleSetId::HongKong; }
    bool chowAllowed() const override { return true; }
    bool bonusTilesInPlay() const override { return true; }
    int minimumToWin() const override { return kMinimum; }
    int limit() const override { return kLimit; }
    int points(int fan) const override { return pointsFor(fan); }
    HandScore score(const WinContext& win) const override;
    std::vector<ScoreItem> fanList() const override;
    std::array<int, 4> settle(const WinContext& win, const HandScore& s) const override;
    bool shapeAllowed(Shape shape) const override
    {
        return shape == Shape::Sets || shape == Shape::ThirteenOrphans;
    }
    bool dealerRepeats() const override { return true; }

    // § 4.4's value for one pattern: every limit hand is worth the limit.
    static int fanValue(Fan fan);
    // § 4.5's table: the points a total of `fan` is worth, 0 below the minimum.
    static int pointsFor(int fan);
};

// Every rule set this build has, in menu order.
const std::vector<RuleSetId>& ruleSets();
// Whether a stored value names one of them.
bool knownRuleSet(int value);
const RuleSet& ruleSet(RuleSetId id);

// The best-scoring way to read a hand as a win under `rules`, or nothing when
// the tiles make no winning hand of any shape the rule set allows.
// `concealed` holds the winning tile; `melds` are the sets laid down,
// concealed kongs among them. `how` carries the flags, the winds, the bonus
// tiles and the seats; its shape, sets, pair, tiles, winningIn and waits are
// filled in here. The minimum is NOT applied -- that is the caller's question.
std::optional<std::pair<WinContext, HandScore>> bestWin(const RuleSet& rules,
                                                        const std::vector<Face>& concealed,
                                                        const std::vector<Meld>& melds,
                                                        WinContext how);

// Whether `tile` is a dragon, wind, or a one or nine of a numbered suit.
bool isHonour(Face tile);
bool isTerminal(Face tile);
bool isNumbered(Face tile);

} // namespace mahjong
