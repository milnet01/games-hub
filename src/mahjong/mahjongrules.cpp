#include "mahjongrules.h"

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

namespace {

using Counts = std::array<int, kLastFace + 1>;

bool numbered(Face f)
{
    const Suit s = suitOf(f);
    return s == Suit::Circles || s == Suit::Bamboo || s == Suit::Characters;
}

bool isPungLike(const Meld& m)
{
    return m.kind == SetKind::Pung || m.kind == SetKind::Kong;
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

bool better(const HandScore& a, const HandScore& b)
{
    return a.total != b.total ? a.total > b.total : a.points > b.points;
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
    std::vector<ScoreItem> all;
    for (int f = 0; f < kFanCount; ++f)
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
    if (win.thirteenOrphans) {
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
    static const std::vector<RuleSetId> all { RuleSetId::HongKong };
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
    switch (id) {
    case RuleSetId::HongKong: return hongKong;
    }
    return hongKong;
}

// ---------------------------------------------------------------------------
// Reading a hand as a win
// ---------------------------------------------------------------------------

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

    std::optional<std::pair<WinContext, HandScore>> best;
    const auto consider = [&](const WinContext& ctx) {
        HandScore s = rules.score(ctx);
        if (!best || better(s, best->second))
            best = std::make_pair(ctx, std::move(s));
    };

    if (melds.empty() && isThirteenOrphans(c)) {
        WinContext ctx = how;
        ctx.thirteenOrphans = true;
        consider(ctx);
    }

    const bool onADiscard = !how.selfDrawn && how.payer >= 0;
    for (Face pair = 1; pair < kFlowerFirst; ++pair) {
        if (c[pair] < 2)
            continue;
        c[pair] -= 2;
        std::vector<std::vector<Meld>> readings;
        std::vector<Meld> sets;
        decompose(c, sets, needed, readings);
        c[pair] += 2;
        for (std::vector<Meld>& reading : readings) {
            // A pung finished by a discard was not held concealed: the tile
            // came from another player. Where the winning tile could be the
            // pair instead, that reading is tried as well, under its own pair.
            if (onADiscard && pair != how.winningTile) {
                for (Meld& m : reading) {
                    if (isPungLike(m) && m.tile == how.winningTile) {
                        m.concealed = false;
                        break;
                    }
                }
            }
            WinContext ctx = how;
            ctx.pair = pair;
            ctx.sets = melds;
            ctx.sets.insert(ctx.sets.end(), reading.begin(), reading.end());
            consider(ctx);
        }
    }
    return best;
}

} // namespace mahjong
