#include "mahjongrules.h"

#include "mahjongofficial.h"
#include "mahjongriichi.h"

#include <algorithm>

namespace mahjong {

bool isHonour(Face tile)
{
    const Suit s = suitOf(tile);
    return s == Suit::Wind || s == Suit::Dragon;
}

bool isTerminal(Face tile)
{
    const Suit s = suitOf(tile);
    const bool numbered = s == Suit::Circles || s == Suit::Bamboo || s == Suit::Characters;
    return numbered && (rankOf(tile) == 1 || rankOf(tile) == 9);
}

bool isGreen(Face t)
{
    if (t == kDragonFirst + 1)
        return true;
    if (suitOf(t) != Suit::Bamboo)
        return false;
    const int r = rankOf(t);
    return r == 2 || r == 3 || r == 4 || r == 6 || r == 8;
}

bool isNumbered(Face f)
{
    const Suit s = suitOf(f);
    return s == Suit::Circles || s == Suit::Bamboo || s == Suit::Characters;
}

namespace {

using Counts = std::array<int, kLastFace + 1>;

bool numbered(Face f)
{
    return isNumbered(f);
}

// Every way to read the tiles counted in `c` as exactly `needed` sets. The
// lowest tile left must start a pung or a chow, so each branch is one of
// those two and nothing is found twice.
void decompose(Counts& c, std::vector<Meld>& sets, int needed, std::vector<std::vector<Meld>>& out)
{
    Face first = 0;
    for (Face f = 1; f < kFlowerFirst; ++f) {
        if (c[f] > 0) {
            first = f;
            break;
        }
    }
    if (first == 0) {
        if (int(sets.size()) == needed)
            out.push_back(sets);
        return;
    }
    if (int(sets.size()) >= needed)
        return;
    if (c[first] >= 3) {
        c[first] -= 3;
        sets.push_back({ SetKind::Pung, first, true });
        decompose(c, sets, needed, out);
        sets.pop_back();
        c[first] += 3;
    }
    if (numbered(first) && rankOf(first) <= 7 && c[first + 1] > 0 && c[first + 2] > 0) {
        --c[first];
        --c[first + 1];
        --c[first + 2];
        sets.push_back({ SetKind::Chow, first, true });
        decompose(c, sets, needed, out);
        sets.pop_back();
        ++c[first];
        ++c[first + 1];
        ++c[first + 2];
    }
}

void addMeldTiles(std::vector<Face>& tiles, const Meld& m)
{
    if (m.kind == SetKind::Chow) {
        tiles.insert(tiles.end(), { m.tile, Face(m.tile + 1), Face(m.tile + 2) });
        return;
    }
    tiles.insert(tiles.end(), m.kind == SetKind::Kong ? 4 : 3, m.tile);
}

const std::vector<Face>& orphans()
{
    static const std::vector<Face> faces = [] {
        std::vector<Face> f;
        for (Face first : { kCirclesFirst, kBambooFirst, kCharactersFirst }) {
            f.push_back(first);
            f.push_back(Face(first + 8));
        }
        for (Face h = kWindFirst; h < kFlowerFirst; ++h)
            f.push_back(h);
        return f;
    }();
    return faces;
}

bool isThirteenOrphans(const Counts& c)
{
    int total = 0;
    for (Face f : orphans()) {
        if (c[f] < 1)
            return false;
        total += c[f];
    }
    int all = 0;
    for (int n : c)
        all += n;
    return total == 14 && all == 14;
}

// Points first: under Riichi a reading with more han can pay less than one
// with more minipoints, and the book takes the higher-scoring (GHUB-0244
// § 4.2). Under the other rule sets points never fall as the total rises.
bool better(const HandScore& a, const HandScore& b)
{
    return a.points != b.points ? a.points > b.points : a.total > b.total;
}

} // namespace

// ---------------------------------------------------------------------------
// Hong Kong
// ---------------------------------------------------------------------------

int HongKongRules::pointsFor(int fan)
{
    // § 4.5. Below the minimum a hand cannot win, so it is worth nothing.
    static constexpr int kPoints[] = { 8, 16, 24, 32, 48, 64, 96, 128 };
    if (fan < kMinimum)
        return 0;
    return kPoints[std::min(fan, kLimit) - kMinimum];
}

int HongKongRules::fanValue(Fan fan)
{
    switch (fan) {
    case Fan::AllFourFlowers:
    case Fan::AllFourSeasons: return 2;
    case Fan::AllPungs:
    case Fan::MixedOneSuit: return 3;
    case Fan::SmallThreeDragons: return 5;
    case Fan::PureOneSuit: return 7;
    case Fan::BigThreeDragons: return 8;
    default: break;
    }
    return fan >= Fan::SmallFourWinds ? kLimit : 1;
}

std::vector<ScoreItem> HongKongRules::fanList() const
{
    // GHUB-0239 § 4.4 only: the Fan values after EarthlyHand are Chinese
    // Official's.
    std::vector<ScoreItem> all;
    for (int f = 0; f <= int(Fan::EarthlyHand); ++f)
        all.push_back({ Fan(f), fanValue(Fan(f)) });
    return all;
}

HandScore HongKongRules::score(const WinContext& win) const
{
    HandScore s;
    const auto add = [&s](Fan fan) { s.items.push_back({ fan, fanValue(fan) }); };

    int windPungs = 0;
    int dragonPungs = 0;
    int pungs = 0;
    int kongs = 0;
    int chows = 0;
    bool allConcealedPungs = true;
    bool seatWindPung = false;
    bool prevailingWindPung = false;
    for (const Meld& m : win.sets) {
        if (m.kind == SetKind::Chow) {
            ++chows;
            continue;
        }
        ++pungs;
        if (m.kind == SetKind::Kong)
            ++kongs;
        if (!m.concealed)
            allConcealedPungs = false;
        if (suitOf(m.tile) == Suit::Wind) {
            ++windPungs;
            seatWindPung = seatWindPung || rankOf(m.tile) == win.seatWind;
            prevailingWindPung = prevailingWindPung || rankOf(m.tile) == win.prevailingWind;
        }
        if (suitOf(m.tile) == Suit::Dragon)
            ++dragonPungs;
    }
    const Suit pairSuit = suitOf(win.pair);

    bool honours = false;
    bool allHonours = !win.tiles.empty();
    bool allTerminals = !win.tiles.empty();
    std::array<bool, 4> suits {}; // indexed by Suit: circles, bamboo, characters; 0 unused
    for (Face t : win.tiles) {
        if (isHonour(t))
            honours = true;
        else
            allHonours = false;
        if (!isTerminal(t))
            allTerminals = false;
        if (numbered(t))
            suits[std::size_t(suitOf(t))] = true;
    }
    const int numberedSuits = int(std::count(suits.begin(), suits.end(), true));

    // The limit hands first: each is worth the limit alone.
    std::vector<Fan> limits;
    if (win.heavenly)
        limits.push_back(Fan::HeavenlyHand);
    if (win.earthly)
        limits.push_back(Fan::EarthlyHand);
    if (win.shape == Shape::ThirteenOrphans) {
        limits.push_back(Fan::ThirteenOrphans);
    } else {
        if (windPungs == 4)
            limits.push_back(Fan::BigFourWinds);
        else if (windPungs == 3 && pairSuit == Suit::Wind)
            limits.push_back(Fan::SmallFourWinds);
        if (allHonours)
            limits.push_back(Fan::AllHonours);
        if (allTerminals)
            limits.push_back(Fan::AllTerminals);
        if (kongs == 4)
            limits.push_back(Fan::FourKongs);
        else if (pungs == 4 && allConcealedPungs)
            limits.push_back(Fan::FourConcealedPungs);
        // Nine gates: 1112345678999 in one suit plus any tile of it, held
        // concealed throughout -- so no set laid down and no kong.
        if (!win.claimedASet && win.tiles.size() == 14 && numberedSuits == 1 && !honours) {
            std::array<int, 10> byRank {};
            for (Face t : win.tiles)
                ++byRank[std::size_t(rankOf(t))];
            bool gates = byRank[1] >= 3 && byRank[9] >= 3;
            for (int r = 2; r <= 8; ++r)
                gates = gates && byRank[std::size_t(r)] >= 1;
            if (gates && kongs == 0)
                limits.push_back(Fan::NineGates);
        }
    }
    if (!limits.empty()) {
        for (Fan f : limits)
            add(f);
        s.total = kLimit;
        s.qualifying = s.total;
        s.limit = true;
        s.points = pointsFor(s.total);
        return s;
    }

    if (win.selfDrawn)
        add(Fan::SelfDrawn);
    else if (!win.claimedASet)
        add(Fan::ConcealedHand);

    int flowers = 0;
    int seasons = 0;
    bool ownFlower = false;
    bool ownSeason = false;
    for (Face b : win.bonus) {
        if (suitOf(b) == Suit::Flower) {
            ++flowers;
            ownFlower = ownFlower || rankOf(b) == win.seatWind;
        } else if (suitOf(b) == Suit::Season) {
            ++seasons;
            ownSeason = ownSeason || rankOf(b) == win.seatWind;
        }
    }
    if (win.bonus.empty())
        add(Fan::NoBonusTiles);
    if (ownFlower)
        add(Fan::OwnFlower);
    if (ownSeason)
        add(Fan::OwnSeason);
    if (flowers == 4)
        add(Fan::AllFourFlowers);
    if (seasons == 4)
        add(Fan::AllFourSeasons);

    // A pair "scores nothing" when it is not a dragon, your wind or the
    // prevailing wind.
    const bool pairScores = pairSuit == Suit::Dragon
        || (pairSuit == Suit::Wind
            && (rankOf(win.pair) == win.seatWind || rankOf(win.pair) == win.prevailingWind));
    if (chows == 4 && !pairScores)
        add(Fan::AllChows);

    if (dragonPungs == 3)
        add(Fan::BigThreeDragons);
    else if (dragonPungs == 2 && pairSuit == Suit::Dragon)
        add(Fan::SmallThreeDragons);
    else
        for (int i = 0; i < dragonPungs; ++i)
            add(Fan::DragonPung);
    if (seatWindPung)
        add(Fan::SeatWindPung);
    if (prevailingWindPung)
        add(Fan::PrevailingWindPung);

    if (win.lastTile)
        add(Fan::LastTile);
    if (win.kongReplacement)
        add(Fan::KongReplacement);
    if (win.robbedKong)
        add(Fan::RobbingAKong);
    if (pungs == 4)
        add(Fan::AllPungs);
    if (numberedSuits == 1 && !honours)
        add(Fan::PureOneSuit);
    else if (numberedSuits == 1 && honours)
        add(Fan::MixedOneSuit);

    int sum = 0;
    for (const ScoreItem& item : s.items)
        sum += item.value;
    s.total = std::min(sum, kLimit);
    s.qualifying = s.total;
    s.points = pointsFor(s.total);
    return s;
}

std::array<int, 4> HongKongRules::settle(const WinContext& win, const HandScore& s) const
{
    std::array<int, 4> pay {};
    if (win.winner < 0 || win.winner > 3)
        return pay;
    if (win.payer >= 0 && win.payer < 4) {
        // On a discard, or a robbed kong, the one who let the tile go pays.
        pay[std::size_t(win.payer)] -= s.points;
        pay[std::size_t(win.winner)] += s.points;
        return pay;
    }
    for (int seat = 0; seat < 4; ++seat) {
        if (seat == win.winner)
            continue;
        pay[std::size_t(seat)] -= s.points;
        pay[std::size_t(win.winner)] += s.points;
    }
    return pay;
}

// ---------------------------------------------------------------------------
// The registry
// ---------------------------------------------------------------------------

const std::vector<RuleSetId>& ruleSets()
{
    static const std::vector<RuleSetId> all { RuleSetId::HongKong, RuleSetId::ChineseOfficial,
                                              RuleSetId::JapaneseRiichi };
    return all;
}

bool knownRuleSet(int value)
{
    for (RuleSetId id : ruleSets())
        if (int(id) == value)
            return true;
    return false;
}

const RuleSet& ruleSet(RuleSetId id)
{
    static const HongKongRules hongKong;
    static const ChineseOfficialRules chineseOfficial;
    static const RiichiRules riichi;
    switch (id) {
    case RuleSetId::HongKong: return hongKong;
    case RuleSetId::ChineseOfficial: return chineseOfficial;
    case RuleSetId::JapaneseRiichi: return riichi;
    }
    return hongKong;
}

// ---------------------------------------------------------------------------
// Reading a hand as a win (GHUB-0243 § 4.3)
// ---------------------------------------------------------------------------

namespace {

// The six ways to give the knitted runs 1-4-7, 2-5-8 and 3-6-9 to the three
// suits: perm[run] is the suit, 0 circles, 1 bamboo, 2 characters.
using KnitPerm = std::array<int, 3>;
constexpr std::array<KnitPerm, 6> kKnitPerms { { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 },
                                                 { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } } };

Face suitFirst(int suit)
{
    return suit == 0 ? kCirclesFirst : suit == 1 ? kBambooFirst : kCharactersFirst;
}

// The nine tiles of the knitted straight `perm` makes.
std::array<Face, 9> knittedTiles(const KnitPerm& perm)
{
    std::array<Face, 9> tiles {};
    for (int run = 0; run < 3; ++run)
        for (int step = 0; step < 3; ++step)
            tiles[std::size_t(run) * 3 + std::size_t(step)] = Face(suitFirst(perm[std::size_t(run)]) + run + 3 * step);
    return tiles;
}

// WinContext::knitted for `perm`: each suit's lowest knitted tile, in suit order.
std::array<Face, 3> knittedLows(const KnitPerm& perm)
{
    std::array<Face, 3> lows {};
    for (int run = 0; run < 3; ++run)
        lows[std::size_t(perm[std::size_t(run)])] = Face(suitFirst(perm[std::size_t(run)]) + run);
    return lows;
}

int tileCount(const Counts& c)
{
    int n = 0;
    for (int k : c)
        n += k;
    return n;
}

// Whether the tiles in `c` make `needed` sets and a pair.
bool setsComplete(Counts c, int needed)
{
    for (Face pair = 1; pair < kFlowerFirst; ++pair) {
        if (c[pair] < 2)
            continue;
        c[pair] -= 2;
        std::vector<std::vector<Meld>> readings;
        std::vector<Meld> sets;
        decompose(c, sets, needed, readings);
        c[pair] += 2;
        if (!readings.empty())
            return true;
    }
    return false;
}

bool isSevenPairs(const Counts& c)
{
    if (tileCount(c) != 14)
        return false;
    return std::all_of(c.begin(), c.end(), [](int n) { return n % 2 == 0; });
}

bool isSevenDifferentPairs(const Counts& c)
{
    return tileCount(c) == 14 && std::all_of(c.begin(), c.end(), [](int n) { return n == 0 || n == 2; });
}

bool holdsAll(const Counts& c, const std::array<Face, 9>& tiles)
{
    return std::all_of(tiles.begin(), tiles.end(), [&c](Face t) { return c[t] > 0; });
}

// Fourteen different tiles, every numbered one a tile of `perm`'s knitted
// straight.
bool fitsHonoursAndKnitted(const Counts& c, const KnitPerm& perm)
{
    if (tileCount(c) != 14 || std::any_of(c.begin(), c.end(), [](int n) { return n > 1; }))
        return false;
    const std::array<Face, 9> knit = knittedTiles(perm);
    for (Face f = 1; f < kWindFirst; ++f)
        if (c[f] > 0 && std::find(knit.begin(), knit.end(), f) == knit.end())
            return false;
    return true;
}

// Whether `c`, with `melds` sets laid down beside it, is a winning hand of any
// shape `rules` allows -- the scoring and the minimum aside.
bool complete(const RuleSet& rules, const Counts& c, int melds)
{
    const int needed = 4 - melds;
    if (rules.shapeAllowed(Shape::Sets) && setsComplete(c, needed))
        return true;
    if (melds == 0 && rules.shapeAllowed(Shape::SevenPairs) && isSevenPairs(c))
        return true;
    if (melds == 0 && rules.shapeAllowed(Shape::SevenDifferentPairs) && isSevenDifferentPairs(c))
        return true;
    if (melds == 0 && rules.shapeAllowed(Shape::ThirteenOrphans) && isThirteenOrphans(c))
        return true;
    for (const KnitPerm& perm : kKnitPerms) {
        if (melds <= 1 && rules.shapeAllowed(Shape::KnittedStraight)) {
            const std::array<Face, 9> knit = knittedTiles(perm);
            if (holdsAll(c, knit)) {
                Counts rest = c;
                for (Face t : knit)
                    --rest[t];
                if (setsComplete(rest, needed - 3))
                    return true;
            }
        }
        if (melds == 0 && rules.shapeAllowed(Shape::HonoursAndKnitted) && fitsHonoursAndKnitted(c, perm))
            return true;
    }
    return false;
}

bool holdsTile(const Meld& m, Face t)
{
    return m.kind == SetKind::Chow ? t >= m.tile && t <= m.tile + 2 : m.tile == t;
}

} // namespace

std::optional<std::pair<WinContext, HandScore>> bestWin(const RuleSet& rules,
                                                        const std::vector<Face>& concealed,
                                                        const std::vector<Meld>& melds,
                                                        WinContext how)
{
    const int needed = 4 - int(melds.size());
    if (needed < 0 || int(concealed.size()) != needed * 3 + 2)
        return std::nullopt;
    Counts c {};
    for (Face t : concealed) {
        if (t == 0 || t >= kFlowerFirst)
            return std::nullopt;
        ++c[t];
    }

    how.claimedASet = std::any_of(melds.begin(), melds.end(), [](const Meld& m) { return !m.concealed; });
    how.tiles = concealed;
    for (const Meld& m : melds)
        addMeldTiles(how.tiles, m);

    // Every reading of the tiles, once for each part of it that could hold
    // the winning tile: the freedom to choose the higher score (book § 3.9.1).
    std::vector<WinContext> readings;
    const bool onADiscard = !how.selfDrawn && how.payer >= 0;
    // A reading no part of which holds the winning tile -- only a test that
    // names none -- is kept whole, with winningIn left at -2.
    const auto place = [&](const WinContext& ctx) {
        const std::size_t before = readings.size();
        if (ctx.pair != 0 && ctx.pair == how.winningTile) {
            WinContext p = ctx;
            p.winningIn = -1;
            readings.push_back(p);
        }
        for (std::size_t i = melds.size(); i < ctx.sets.size(); ++i) {
            const Meld& m = ctx.sets[i];
            if (!holdsTile(m, how.winningTile))
                continue;
            // Two alike sets read the same either way; one is enough.
            if (std::find(ctx.sets.begin() + std::ptrdiff_t(melds.size()), ctx.sets.begin() + std::ptrdiff_t(i), m)
                != ctx.sets.begin() + std::ptrdiff_t(i))
                continue;
            WinContext p = ctx;
            p.winningIn = int(i);
            // A pung finished by a discard was not held concealed: the tile
            // came from another player.
            if (onADiscard && m.kind != SetKind::Chow)
                p.sets[i].concealed = false;
            readings.push_back(p);
        }
        if (readings.size() == before)
            readings.push_back(ctx);
    };

    if (rules.shapeAllowed(Shape::Sets)) {
        for (Face pair = 1; pair < kFlowerFirst; ++pair) {
            if (c[pair] < 2)
                continue;
            c[pair] -= 2;
            std::vector<std::vector<Meld>> found;
            std::vector<Meld> sets;
            decompose(c, sets, needed, found);
            c[pair] += 2;
            for (const std::vector<Meld>& reading : found) {
                WinContext ctx = how;
                ctx.shape = Shape::Sets;
                ctx.pair = pair;
                ctx.sets = melds;
                ctx.sets.insert(ctx.sets.end(), reading.begin(), reading.end());
                place(ctx);
            }
        }
    }
    if (melds.empty() && rules.shapeAllowed(Shape::SevenPairs) && isSevenPairs(c)) {
        WinContext ctx = how;
        ctx.shape = Shape::SevenPairs;
        readings.push_back(ctx);
    }
    if (melds.empty() && rules.shapeAllowed(Shape::SevenDifferentPairs) && isSevenDifferentPairs(c)) {
        WinContext ctx = how;
        ctx.shape = Shape::SevenDifferentPairs;
        ctx.winningIn = -1; // a pair wait, always
        readings.push_back(ctx);
    }
    if (melds.empty() && rules.shapeAllowed(Shape::ThirteenOrphans) && isThirteenOrphans(c)) {
        WinContext ctx = how;
        ctx.shape = Shape::ThirteenOrphans;
        readings.push_back(ctx);
    }
    for (const KnitPerm& perm : kKnitPerms) {
        const std::array<Face, 9> knit = knittedTiles(perm);
        if (melds.size() <= 1 && rules.shapeAllowed(Shape::KnittedStraight) && holdsAll(c, knit)) {
            Counts rest = c;
            for (Face t : knit)
                --rest[t];
            for (Face pair = 1; pair < kFlowerFirst; ++pair) {
                if (rest[pair] < 2)
                    continue;
                rest[pair] -= 2;
                std::vector<std::vector<Meld>> found;
                std::vector<Meld> sets;
                decompose(rest, sets, needed - 3, found);
                rest[pair] += 2;
                for (const std::vector<Meld>& reading : found) {
                    WinContext ctx = how;
                    ctx.shape = Shape::KnittedStraight;
                    ctx.knitted = knittedLows(perm);
                    ctx.pair = pair;
                    ctx.sets = melds;
                    ctx.sets.insert(ctx.sets.end(), reading.begin(), reading.end());
                    place(ctx);
                    if (std::find(knit.begin(), knit.end(), how.winningTile) != knit.end())
                        readings.push_back(ctx); // the winning tile in the knitted run
                }
            }
        }
        if (melds.empty() && rules.shapeAllowed(Shape::HonoursAndKnitted) && fitsHonoursAndKnitted(c, perm)) {
            WinContext ctx = how;
            ctx.shape = Shape::HonoursAndKnitted;
            ctx.knitted = knittedLows(perm);
            readings.push_back(ctx);
        }
    }
    if (readings.empty())
        return std::nullopt;

    // How many faces would have finished the hand before the winning tile
    // came. A face of which all four are already in the hand cannot come.
    int waits = 0;
    if (how.winningTile > 0 && how.winningTile < kFlowerFirst && c[how.winningTile] > 0) {
        Counts held {};
        for (Face t : how.tiles)
            if (t < kFlowerFirst)
                ++held[t];
        Counts before = c;
        --before[how.winningTile];
        for (Face f = 1; f < kFlowerFirst; ++f) {
            if (held[f] - (f == how.winningTile ? 1 : 0) >= 4)
                continue;
            ++before[f];
            if (complete(rules, before, int(melds.size())))
                ++waits;
            --before[f];
        }
    }

    std::optional<std::pair<WinContext, HandScore>> best;
    for (WinContext& ctx : readings) {
        ctx.waits = waits;
        HandScore s = rules.score(ctx);
        if (!best || better(s, best->second))
            best = std::make_pair(ctx, std::move(s));
    }
    return best;
}

std::vector<Face> waitingTiles(const RuleSet& rules, const std::vector<Face>& concealed,
                               const std::vector<Meld>& melds)
{
    std::vector<Face> waits;
    if (int(concealed.size()) != (4 - int(melds.size())) * 3 + 1)
        return waits;
    Counts c {};
    for (Face t : concealed) {
        if (t == 0 || t >= kFlowerFirst)
            return waits;
        ++c[t];
    }
    Counts held = c;
    for (const Meld& m : melds) {
        std::vector<Face> tiles;
        addMeldTiles(tiles, m);
        for (Face t : tiles)
            ++held[t];
    }
    for (Face f = 1; f < kFlowerFirst; ++f) {
        if (held[f] >= 4)
            continue;
        ++c[f];
        if (complete(rules, c, int(melds.size())))
            waits.push_back(f);
        --c[f];
    }
    return waits;
}

bool pungInEveryReading(const std::vector<Face>& concealed, int needed, Face tile)
{
    Counts c {};
    for (Face t : concealed)
        if (t > 0 && t < kFlowerFirst)
            ++c[t];
    bool any = false;
    for (Face pair = 1; pair < kFlowerFirst; ++pair) {
        if (c[pair] < 2)
            continue;
        c[pair] -= 2;
        std::vector<std::vector<Meld>> found;
        std::vector<Meld> sets;
        decompose(c, sets, needed, found);
        c[pair] += 2;
        for (const std::vector<Meld>& reading : found) {
            any = true;
            const Meld pung { SetKind::Pung, tile, true };
            if (std::find(reading.begin(), reading.end(), pung) == reading.end())
                return false;
        }
    }
    return any;
}

} // namespace mahjong
