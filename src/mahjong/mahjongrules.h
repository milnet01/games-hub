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

// Stored in settings and saves, so a value never changes meaning. 2 and 3 are
// kept for GHUB-0243 and GHUB-0244.
enum class RuleSetId : std::uint8_t { HongKong = 1 };

enum class SetKind : std::uint8_t { Chow, Pung, Kong };

// A set, laid down or held. A chow names its lowest tile. A concealed set was
// never claimed from a discard: a concealed kong, or a set still in the hand.
struct Meld {
    SetKind kind = SetKind::Pung;
    Face tile = 0;
    bool concealed = false;

    bool operator==(const Meld&) const = default;
};

// One scoring pattern. The order is § 4.4's, then the limit hands.
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
};
constexpr int kFanCount = int(Fan::EarthlyHand) + 1;

struct ScoreItem {
    Fan fan;
    int value;
};

struct HandScore {
    std::vector<ScoreItem> items;
    int total = 0; // after the limit
    bool limit = false;
    int points = 0;
};

// Everything about how a hand was won that a rule set's scoring reads.
struct WinContext {
    // The four sets, laid down and held alike, and the pair. Empty and 0 for
    // Thirteen orphans, which has neither.
    std::vector<Meld> sets;
    Face pair = 0;
    // Every tile of the hand, the bonus tiles aside.
    std::vector<Face> tiles;
    std::vector<Face> bonus;
    Face winningTile = 0;
    bool thirteenOrphans = false;
    bool selfDrawn = false;
    // A chow, pung or kong was claimed from a discard.
    bool claimedASet = false;
    int seatWind = 1;       // 1 east, 2 south, 3 west, 4 north
    int prevailingWind = 1; // the same
    bool lastTile = false;
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
    virtual HandScore score(const WinContext& win) const = 0;
    // Every pattern this rule set scores, with its value, in the order the
    // rules text lists them.
    virtual std::vector<ScoreItem> fanList() const = 0;
    // What each seat gains (or, negative, pays) for `win`. Sums to zero.
    virtual std::array<int, 4> settle(const WinContext& win, const HandScore& s) const = 0;
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
    HandScore score(const WinContext& win) const override;
    std::vector<ScoreItem> fanList() const override;
    std::array<int, 4> settle(const WinContext& win, const HandScore& s) const override;

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
// the tiles make no winning hand at all: four sets and a pair, or Thirteen
// orphans. `concealed` holds the winning tile; `melds` are the sets laid down,
// concealed kongs among them. `how` carries the flags, the winds, the bonus
// tiles and the seats; its sets, pair and tiles are filled in here. The
// minimum is NOT applied -- that is the caller's question.
std::optional<std::pair<WinContext, HandScore>> bestWin(const RuleSet& rules,
                                                        const std::vector<Face>& concealed,
                                                        const std::vector<Meld>& melds,
                                                        WinContext how);

// Whether `tile` is a dragon, wind, or a one or nine of a numbered suit.
bool isHonour(Face tile);
bool isTerminal(Face tile);

} // namespace mahjong
