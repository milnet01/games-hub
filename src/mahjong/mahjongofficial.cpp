#include "mahjongofficial.h"

#include <algorithm>
#include <cstdlib>
#include <optional>

namespace mahjong {

namespace {

// One row of GHUB-0243 § 4.4: the pattern, its points, and what it is not
// scored with. The † entries of the spec are here too, unmarked.
struct Row {
    Fan fan;
    int value;
    std::vector<Fan> excludes;
};

const std::vector<Row>& rows()
{
    using F = Fan;
    static const std::vector<Row> all {
        // 88
        { F::BigFourWinds, 88,
          { F::BigThreeWinds, F::AllPungs, F::PrevailingWindPung, F::SeatWindPung, F::TerminalOrHonourPung,
            F::SmallFourWinds } },
        { F::BigThreeDragons, 88, { F::TwoDragonPungs, F::DragonPung, F::SmallThreeDragons } },
        { F::AllGreen, 88, {} },
        { F::NineGates, 88, { F::PureOneSuit, F::ConcealedHand, F::TerminalOrHonourPung, F::NoHonours } },
        { F::FourKongs, 88,
          { F::SingleWait, F::ThreeKongs, F::TwoConcealedKongs, F::TwoMeldedKongs, F::ConcealedKong, F::MeldedKong,
            F::AllPungs } },
        { F::SevenShiftedPairs, 88, { F::PureOneSuit, F::ConcealedHand, F::SingleWait, F::SevenPairs, F::NoHonours } },
        { F::ThirteenOrphans, 88, { F::AllTypes, F::ConcealedHand, F::SingleWait } },
        // 64
        { F::AllTerminals, 64,
          { F::AllPungs, F::OutsideHand, F::TerminalOrHonourPung, F::NoHonours, F::AllTerminalsAndHonours } },
        { F::SmallFourWinds, 64, { F::BigThreeWinds, F::TerminalOrHonourPung } },
        { F::SmallThreeDragons, 64, { F::DragonPung, F::TwoDragonPungs } },
        { F::AllHonours, 64, { F::AllPungs, F::OutsideHand, F::TerminalOrHonourPung, F::AllTerminalsAndHonours } },
        { F::FourConcealedPungs, 64,
          { F::AllPungs, F::ConcealedHand, F::ThreeConcealedPungs, F::TwoConcealedPungs } },
        { F::PureTerminalChows, 64,
          { F::SevenPairs, F::PureOneSuit, F::AllChowsNoHonours, F::PureDoubleChow, F::TwoTerminalChows,
            F::NoHonours } },
        // 48
        { F::QuadrupleChow, 48, { F::PureShiftedPungs, F::TileHog, F::PureDoubleChow, F::PureTripleChow } },
        { F::FourPureShiftedPungs, 48, { F::PureTripleChow, F::AllPungs, F::PureShiftedPungs } },
        // 32
        { F::FourPureShiftedChows, 32, { F::ShortStraight, F::PureShiftedChows } },
        { F::ThreeKongs, 32, { F::TwoConcealedKongs, F::TwoMeldedKongs, F::ConcealedKong, F::MeldedKong } },
        { F::AllTerminalsAndHonours, 32, { F::AllPungs, F::TerminalOrHonourPung, F::OutsideHand } },
        // 24
        { F::SevenPairs, 24, { F::ConcealedHand, F::SingleWait } },
        { F::GreaterHonoursKnitted, 24, { F::AllTypes, F::ConcealedHand, F::LesserHonoursKnitted } },
        { F::AllEvenPungs, 24, { F::AllPungs, F::AllSimples, F::NoHonours } },
        { F::PureOneSuit, 24, { F::NoHonours } },
        { F::PureTripleChow, 24, { F::PureShiftedPungs, F::PureDoubleChow } },
        { F::PureShiftedPungs, 24, { F::PureTripleChow } },
        { F::UpperTiles, 24, { F::NoHonours, F::UpperFour } },
        { F::MiddleTiles, 24, { F::NoHonours, F::AllSimples } },
        { F::LowerTiles, 24, { F::NoHonours, F::LowerFour } },
        // 16
        { F::PureStraight, 16, {} },
        { F::ThreeSuitedTerminalChows, 16,
          { F::TwoTerminalChows, F::NoHonours, F::AllChowsNoHonours, F::PureDoubleChow } },
        { F::PureShiftedChows, 16, {} },
        { F::AllFives, 16, { F::AllSimples } },
        { F::TriplePung, 16, {} },
        { F::ThreeConcealedPungs, 16, { F::TwoConcealedPungs } },
        // 12
        { F::LesserHonoursKnitted, 12, { F::AllTypes, F::ConcealedHand } },
        { F::KnittedStraight, 12, {} },
        { F::UpperFour, 12, { F::NoHonours } },
        { F::LowerFour, 12, { F::NoHonours } },
        { F::BigThreeWinds, 12, {} },
        // 8
        { F::MixedStraight, 8, {} },
        { F::ReversibleTiles, 8, { F::OneVoidedSuit } },
        { F::MixedTripleChow, 8, {} },
        { F::MixedShiftedPungs, 8, {} },
        { F::ChickenHand, 8, {} },
        { F::LastTile, 8, { F::SelfDrawn } },
        { F::LastTileClaim, 8, {} },
        { F::KongReplacement, 8, { F::SelfDrawn } },
        { F::RobbingAKong, 8, { F::LastOfItsKind } },
        { F::TwoConcealedKongs, 8, { F::ConcealedKong, F::TwoConcealedPungs } },
        // 6
        { F::AllPungs, 6, {} },
        { F::MixedOneSuit, 6, {} },
        { F::MixedShiftedChows, 6, {} },
        { F::AllTypes, 6, {} },
        { F::MeldedHand, 6, { F::SingleWait } },
        { F::TwoDragonPungs, 6, { F::DragonPung } },
        // 4
        { F::OutsideHand, 4, {} },
        { F::FullyConcealed, 4, { F::SelfDrawn, F::ConcealedHand } },
        { F::TwoMeldedKongs, 4, { F::MeldedKong } },
        { F::LastOfItsKind, 4, {} },
        // 2
        { F::DragonPung, 2, {} },
        { F::PrevailingWindPung, 2, {} },
        { F::SeatWindPung, 2, {} },
        { F::ConcealedHand, 2, {} },
        { F::AllChowsNoHonours, 2, { F::NoHonours } },
        { F::TileHog, 2, {} },
        { F::DoublePung, 2, {} },
        { F::TwoConcealedPungs, 2, {} },
        { F::ConcealedKong, 2, {} },
        { F::AllSimples, 2, { F::NoHonours } },
        // 1
        { F::PureDoubleChow, 1, {} },
        { F::MixedDoubleChow, 1, {} },
        { F::ShortStraight, 1, {} },
        { F::TwoTerminalChows, 1, {} },
        { F::TerminalOrHonourPung, 1, {} },
        { F::MeldedKong, 1, {} },
        { F::OneVoidedSuit, 1, {} },
        { F::NoHonours, 1, {} },
        { F::EdgeWait, 1, {} },
        { F::ClosedWait, 1, {} },
        { F::SingleWait, 1, {} },
        { F::SelfDrawn, 1, {} },
        { F::Flower, 1, {} },
    };
    return all;
}

const Row* rowFor(Fan fan)
{
    for (const Row& r : rows())
        if (r.fan == fan)
            return &r;
    return nullptr;
}

// A chow by its lowest tile, or a pung by its tile: suit 0-2 and rank 1-9.
struct Run {
    int suit = 0;
    int rank = 0;
};

Run runOf(Face t)
{
    return { int(suitOf(t)) - int(Suit::Circles), rankOf(t) };
}

using Pattern = std::optional<Fan>;

int valueOf(const Pattern& p)
{
    return p ? ChineseOfficialRules::fanValue(*p) : 0;
}

bool allSameSuit(const std::vector<Run>& r)
{
    return std::all_of(r.begin(), r.end(), [&r](const Run& x) { return x.suit == r.front().suit; });
}

bool allDifferentSuits(const std::vector<Run>& r)
{
    std::array<bool, 3> seen {};
    for (const Run& x : r) {
        if (seen[std::size_t(x.suit)])
            return false;
        seen[std::size_t(x.suit)] = true;
    }
    return true;
}

std::vector<int> sortedRanks(const std::vector<Run>& r)
{
    std::vector<int> ranks;
    ranks.reserve(r.size());
    for (const Run& x : r)
        ranks.push_back(x.rank);
    std::sort(ranks.begin(), ranks.end());
    return ranks;
}

// Each rank `step` above the last.
bool shifted(const std::vector<int>& ranks, int step)
{
    for (std::size_t i = 1; i < ranks.size(); ++i)
        if (ranks[i] != ranks[i - 1] + step)
            return false;
    return true;
}

Pattern chowPair(const Run& a, const Run& b)
{
    if (a.suit == b.suit) {
        if (a.rank == b.rank)
            return Fan::PureDoubleChow;
        if (std::abs(a.rank - b.rank) == 3)
            return Fan::ShortStraight;
        if (std::min(a.rank, b.rank) == 1 && std::max(a.rank, b.rank) == 7)
            return Fan::TwoTerminalChows;
        return std::nullopt;
    }
    return a.rank == b.rank ? Pattern(Fan::MixedDoubleChow) : std::nullopt;
}

Pattern chowTriple(const std::vector<Run>& r)
{
    const std::vector<int> ranks = sortedRanks(r);
    if (allSameSuit(r)) {
        if (shifted(ranks, 0))
            return Fan::PureTripleChow;
        if (ranks == std::vector<int> { 1, 4, 7 })
            return Fan::PureStraight;
        if (shifted(ranks, 1) || shifted(ranks, 2))
            return Fan::PureShiftedChows;
        return std::nullopt;
    }
    if (!allDifferentSuits(r))
        return std::nullopt;
    if (ranks == std::vector<int> { 1, 4, 7 })
        return Fan::MixedStraight;
    if (shifted(ranks, 0))
        return Fan::MixedTripleChow;
    if (shifted(ranks, 1))
        return Fan::MixedShiftedChows;
    return std::nullopt;
}

Pattern pungPair(const Run& a, const Run& b)
{
    return a.suit != b.suit && a.rank == b.rank ? Pattern(Fan::DoublePung) : std::nullopt;
}

Pattern pungTriple(const std::vector<Run>& r)
{
    const std::vector<int> ranks = sortedRanks(r);
    if (allSameSuit(r))
        return shifted(ranks, 1) ? Pattern(Fan::PureShiftedPungs) : std::nullopt;
    if (!allDifferentSuits(r))
        return std::nullopt;
    if (shifted(ranks, 0))
        return Fan::TriplePung;
    if (shifted(ranks, 1))
        return Fan::MixedShiftedPungs;
    return std::nullopt;
}

// § 4.5's account-once rule over one kind of set: a four-set pattern alone;
// else the better of (a) a three-set pattern and the fourth set once with one
// of its three, and (b) two-set patterns chosen so they never form a loop.
std::vector<Fan> combine(const std::vector<Run>& sets, const Pattern& four,
                         Pattern (*pair)(const Run&, const Run&), Pattern (*triple)(const std::vector<Run>&))
{
    if (four)
        return { *four };
    const int n = int(sets.size());
    std::vector<Fan> best;
    int bestValue = 0;

    for (int skip = 0; n >= 3 && skip < (n == 4 ? 4 : 1); ++skip) {
        std::vector<Run> three;
        for (int i = 0; i < n; ++i)
            if (n == 3 || i != skip)
                three.push_back(sets[std::size_t(i)]);
        const Pattern p = triple(three);
        if (!p)
            continue;
        std::vector<Fan> found { *p };
        int value = valueOf(p);
        if (n == 4) {
            Pattern extra;
            for (const Run& r : three) {
                const Pattern q = pair(sets[std::size_t(skip)], r);
                if (valueOf(q) > valueOf(extra))
                    extra = q;
            }
            if (extra) {
                found.push_back(*extra);
                value += valueOf(extra);
            }
        }
        if (value > bestValue) {
            best = found;
            bestValue = value;
        }
    }

    struct Edge {
        int a, b;
        Fan fan;
    };
    std::vector<Edge> edges;
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (const Pattern p = pair(sets[std::size_t(i)], sets[std::size_t(j)]))
                edges.push_back({ i, j, *p });
    for (unsigned mask = 1; mask < (1u << edges.size()); ++mask) {
        std::array<int, 4> root { 0, 1, 2, 3 };
        const auto find = [&root](int x) {
            while (root[std::size_t(x)] != x)
                x = root[std::size_t(x)];
            return x;
        };
        std::vector<Fan> found;
        int value = 0;
        bool loop = false;
        for (std::size_t e = 0; e < edges.size() && !loop; ++e) {
            if (!(mask & (1u << e)))
                continue;
            const int ra = find(edges[e].a);
            const int rb = find(edges[e].b);
            loop = ra == rb;
            root[std::size_t(ra)] = rb;
            found.push_back(edges[e].fan);
            value += ChineseOfficialRules::fanValue(edges[e].fan);
        }
        if (!loop && value > bestValue) {
            best = found;
            bestValue = value;
        }
    }
    return best;
}

bool holdsTerminalOrHonour(const Meld& m)
{
    if (m.kind == SetKind::Chow)
        return rankOf(m.tile) == 1 || rankOf(m.tile) == 7;
    return isTerminal(m.tile) || isHonour(m.tile);
}

bool holdsFive(const Meld& m)
{
    if (!isNumbered(m.tile))
        return false;
    const int r = rankOf(m.tile);
    return m.kind == SetKind::Chow ? r >= 3 && r <= 5 : r == 5;
}

bool isReversible(Face t)
{
    const int r = rankOf(t);
    switch (suitOf(t)) {
    case Suit::Circles: return r <= 5 || r == 8 || r == 9;
    case Suit::Bamboo: return r == 2 || r == 4 || r == 5 || r == 6 || r == 8 || r == 9;
    case Suit::Dragon: return r == 3; // white
    default: return false;
    }
}

} // namespace

int ChineseOfficialRules::fanValue(Fan fan)
{
    const Row* r = rowFor(fan);
    return r ? r->value : 0;
}

const std::vector<Fan>& ChineseOfficialRules::excludes(Fan fan)
{
    static const std::vector<Fan> none;
    const Row* r = rowFor(fan);
    return r ? r->excludes : none;
}

std::vector<ScoreItem> ChineseOfficialRules::fanList() const
{
    std::vector<ScoreItem> all;
    for (const Row& r : rows())
        all.push_back({ r.fan, r.value });
    return all;
}

HandScore ChineseOfficialRules::score(const WinContext& win) const
{
    std::vector<Fan> found;
    const auto add = [&found](Fan f) { found.push_back(f); };

    // What the tiles are, whatever the shape.
    std::array<int, kFlowerFirst> count {};
    std::array<bool, 3> suits {};
    bool honours = false, winds = false, dragons = false;
    bool allTerminal = true, allHonour = true, allTerminalOrHonour = true, simples = true;
    bool green = true, reversible = true;
    int lowRank = 10, highRank = 0;
    for (Face t : win.tiles) {
        if (t == 0 || t >= kFlowerFirst)
            continue;
        ++count[t];
        green = green && isGreen(t);
        reversible = reversible && isReversible(t);
        if (isHonour(t)) {
            honours = true;
            winds = winds || suitOf(t) == Suit::Wind;
            dragons = dragons || suitOf(t) == Suit::Dragon;
            allTerminal = false;
            simples = false;
            continue;
        }
        allHonour = false;
        suits[std::size_t(runOf(t).suit)] = true;
        lowRank = std::min(lowRank, rankOf(t));
        highRank = std::max(highRank, rankOf(t));
        if (isTerminal(t))
            simples = false;
        else
            allTerminal = allTerminalOrHonour = false;
    }
    const int suitCount = int(std::count(suits.begin(), suits.end(), true));
    const bool setsOrPairs = win.shape == Shape::Sets || win.shape == Shape::SevenPairs;

    if (green)
        add(Fan::AllGreen);
    if (setsOrPairs && allTerminal)
        add(Fan::AllTerminals);
    if (setsOrPairs && allHonour)
        add(Fan::AllHonours);
    if (setsOrPairs && allTerminalOrHonour)
        add(Fan::AllTerminalsAndHonours);
    if (suitCount == 1 && !honours)
        add(Fan::PureOneSuit);
    if (suitCount == 1 && honours)
        add(Fan::MixedOneSuit);
    if (!honours && lowRank >= 7)
        add(Fan::UpperTiles);
    if (!honours && lowRank >= 4 && highRank <= 6)
        add(Fan::MiddleTiles);
    if (!honours && highRank <= 3)
        add(Fan::LowerTiles);
    if (!honours && lowRank >= 6)
        add(Fan::UpperFour);
    if (!honours && highRank <= 4)
        add(Fan::LowerFour);
    if (reversible)
        add(Fan::ReversibleTiles);
    if (suitCount == 3 && winds && dragons)
        add(Fan::AllTypes);
    if (simples)
        add(Fan::AllSimples);
    if (!honours)
        add(Fan::NoHonours);
    if (suitCount == 2)
        add(Fan::OneVoidedSuit);
    // All four of a suit tile, none of them a kong.
    for (Face f = 1; f < kWindFirst; ++f) {
        const bool konged = std::any_of(win.sets.begin(), win.sets.end(),
                                        [f](const Meld& m) { return m.kind == SetKind::Kong && m.tile == f; });
        if (count[f] == 4 && !konged)
            add(Fan::TileHog);
    }

    switch (win.shape) {
    case Shape::SevenDifferentPairs: break; // Riichi's; shapeAllowed refuses it here
    case Shape::ThirteenOrphans: add(Fan::ThirteenOrphans); break;
    case Shape::SevenPairs: {
        add(Fan::SevenPairs);
        const int faces = int(std::count_if(count.begin(), count.end(), [](int n) { return n > 0; }));
        if (suitCount == 1 && !honours && faces == 7 && highRank - lowRank == 6)
            add(Fan::SevenShiftedPairs);
        break;
    }
    case Shape::HonoursAndKnitted: {
        const bool allSeven = std::all_of(count.begin() + kWindFirst, count.begin() + kFlowerFirst,
                                          [](int n) { return n > 0; });
        add(allSeven ? Fan::GreaterHonoursKnitted : Fan::LesserHonoursKnitted);
        int knitted = 0;
        for (Face f = 1; f < kWindFirst; ++f)
            knitted += count[f];
        if (knitted == 9)
            add(Fan::KnittedStraight);
        break;
    }
    case Shape::KnittedStraight:
        add(Fan::KnittedStraight);
        break;
    case Shape::Sets: break;
    }

    // The patterns of sets and the pair: Sets, and the set of a knitted straight.
    if (win.shape == Shape::Sets || win.shape == Shape::KnittedStraight) {
        std::vector<Run> chows, pungs;
        int windPungs = 0, dragonPungs = 0, pungCount = 0, kongs = 0, concealedKongs = 0;
        int concealedPungs = 0, claimedSets = 0;
        bool outside = holdsTerminalOrHonour({ SetKind::Pung, win.pair, true });
        bool fives = holdsFive({ SetKind::Pung, win.pair, true });
        bool evenPungs = isNumbered(win.pair) && rankOf(win.pair) % 2 == 0;
        for (const Meld& m : win.sets) {
            outside = outside && holdsTerminalOrHonour(m);
            fives = fives && holdsFive(m);
            if (!m.concealed)
                ++claimedSets;
            if (m.kind == SetKind::Chow) {
                chows.push_back(runOf(m.tile));
                evenPungs = false;
                continue;
            }
            ++pungCount;
            if (m.concealed)
                ++concealedPungs;
            if (m.kind == SetKind::Kong) {
                ++kongs;
                if (m.concealed)
                    ++concealedKongs;
            }
            evenPungs = evenPungs && isNumbered(m.tile) && rankOf(m.tile) % 2 == 0;
            if (isNumbered(m.tile)) {
                pungs.push_back(runOf(m.tile));
                if (isTerminal(m.tile))
                    add(Fan::TerminalOrHonourPung);
            } else if (suitOf(m.tile) == Suit::Dragon) {
                ++dragonPungs;
                add(Fan::DragonPung);
            } else {
                ++windPungs;
            }
        }
        // Wind pungs: the seat and the round's wind score their own pattern, and
        // three or more are Big three winds; any other is a pung of honours.
        for (const Meld& m : win.sets) {
            if (m.kind == SetKind::Chow || suitOf(m.tile) != Suit::Wind)
                continue;
            const bool seat = rankOf(m.tile) == win.seatWind;
            const bool round = rankOf(m.tile) == win.prevailingWind;
            if (seat)
                add(Fan::SeatWindPung);
            if (round)
                add(Fan::PrevailingWindPung);
            if (!seat && !round && windPungs < 3)
                add(Fan::TerminalOrHonourPung);
        }
        const Suit pairSuit = suitOf(win.pair);
        if (windPungs == 4)
            add(Fan::BigFourWinds);
        if (windPungs == 3 && pairSuit == Suit::Wind)
            add(Fan::SmallFourWinds);
        if (windPungs >= 3)
            add(Fan::BigThreeWinds);
        if (dragonPungs == 3)
            add(Fan::BigThreeDragons);
        if (dragonPungs == 2 && pairSuit == Suit::Dragon)
            add(Fan::SmallThreeDragons);
        if (dragonPungs >= 2)
            add(Fan::TwoDragonPungs);

        const bool fourSets = win.shape == Shape::Sets;
        if (fourSets && pungCount == 4)
            add(Fan::AllPungs);
        if (int(chows.size()) == int(win.sets.size()) && !honours)
            add(Fan::AllChowsNoHonours);
        if (fourSets && outside)
            add(Fan::OutsideHand);
        if (fourSets && fives)
            add(Fan::AllFives);
        if (fourSets && pungCount == 4 && evenPungs)
            add(Fan::AllEvenPungs);

        // Nine gates: 1112345678999 of one suit held, won on any tile of it.
        if (fourSets && !win.claimedASet && kongs == 0 && suitCount == 1 && !honours && win.winningTile != 0) {
            std::array<int, 10> byRank {};
            for (Face t : win.tiles)
                ++byRank[std::size_t(rankOf(t))];
            --byRank[std::size_t(rankOf(win.winningTile))];
            if (byRank == std::array<int, 10> { 0, 3, 1, 1, 1, 1, 1, 1, 1, 3 })
                add(Fan::NineGates);
        }

        // Kongs, and pungs held concealed.
        if (kongs == 4)
            add(Fan::FourKongs);
        else if (kongs == 3)
            add(Fan::ThreeKongs);
        else if (kongs == 2 && concealedKongs == 2)
            add(Fan::TwoConcealedKongs);
        else if (kongs == 2) {
            add(Fan::TwoMeldedKongs);
            if (concealedKongs == 1)
                add(Fan::ConcealedKong);
        } else if (kongs == 1)
            add(concealedKongs == 1 ? Fan::ConcealedKong : Fan::MeldedKong);
        if (concealedPungs == 4)
            add(Fan::FourConcealedPungs);
        else if (concealedPungs == 3)
            add(Fan::ThreeConcealedPungs);
        else if (concealedPungs == 2)
            add(Fan::TwoConcealedPungs);

        // Four sets laid down, won on a discard that pairs the last tile.
        if (fourSets && claimedSets == 4 && win.winningIn == -1 && !win.selfDrawn)
            add(Fan::MeldedHand);

        // The patterns that combine sets, chows first, then pungs.
        Pattern fourChows;
        if (chows.size() == 4) {
            const std::vector<int> ranks = sortedRanks(chows);
            const Run pair = isNumbered(win.pair) ? runOf(win.pair) : Run { -1, 0 };
            if (allSameSuit(chows) && shifted(ranks, 0))
                fourChows = Fan::QuadrupleChow;
            else if (allSameSuit(chows) && (shifted(ranks, 1) || shifted(ranks, 2)))
                fourChows = Fan::FourPureShiftedChows;
            else if (allSameSuit(chows) && ranks == std::vector<int> { 1, 1, 7, 7 } && pair.suit == chows[0].suit
                     && pair.rank == 5)
                fourChows = Fan::PureTerminalChows;
            else if (pair.rank == 5) {
                // Three-suited terminal chows: 1-2-3 and 7-8-9 in each other suit.
                bool fits = true;
                for (int suit = 0; suit < 3; ++suit) {
                    std::vector<int> mine;
                    for (const Run& r : chows)
                        if (r.suit == suit)
                            mine.push_back(r.rank);
                    std::sort(mine.begin(), mine.end());
                    fits = fits && (suit == pair.suit ? mine.empty() : mine == std::vector<int> { 1, 7 });
                }
                if (fits)
                    fourChows = Fan::ThreeSuitedTerminalChows;
            }
        }
        for (Fan f : combine(chows, fourChows, chowPair, chowTriple))
            add(f);
        Pattern fourPungs;
        if (pungs.size() == 4 && allSameSuit(pungs) && shifted(sortedRanks(pungs), 1))
            fourPungs = Fan::FourPureShiftedPungs;
        for (Fan f : combine(pungs, fourPungs, pungPair, pungTriple))
            add(f);

        // How the hand waited, when one face alone would have finished it.
        if (win.waits == 1 && win.winningIn == -1) {
            add(Fan::SingleWait);
        } else if (win.waits == 1 && win.winningIn >= 0 && win.winningIn < int(win.sets.size())) {
            const Meld& m = win.sets[std::size_t(win.winningIn)];
            if (m.kind == SetKind::Chow) {
                const int at = win.winningTile - m.tile;
                if (at == 1)
                    add(Fan::ClosedWait);
                else if ((at == 2 && rankOf(m.tile) == 1) || (at == 0 && rankOf(m.tile) == 7))
                    add(Fan::EdgeWait);
            }
        }
    }

    // How the hand was won.
    if (!win.claimedASet)
        add(win.selfDrawn ? Fan::FullyConcealed : Fan::ConcealedHand);
    if (win.selfDrawn)
        add(Fan::SelfDrawn);
    if (win.lastTile)
        add(Fan::LastTile);
    if (win.lastTileClaim)
        add(Fan::LastTileClaim);
    if (win.kongReplacement)
        add(Fan::KongReplacement);
    if (win.robbedKong)
        add(Fan::RobbingAKong);
    if (win.lastOfItsKind)
        add(Fan::LastOfItsKind);

    // Strike what a scored pattern's row rules out (book § 3.9.1, Non-Repeat).
    std::vector<Fan> struck;
    for (Fan f : found)
        for (Fan x : excludes(f))
            struck.push_back(x);
    HandScore s;
    for (Fan f : found)
        if (std::find(struck.begin(), struck.end(), f) == struck.end())
            s.items.push_back({ f, fanValue(f) });
    if (s.items.empty())
        s.items.push_back({ Fan::ChickenHand, fanValue(Fan::ChickenHand) });
    std::stable_sort(s.items.begin(), s.items.end(),
                     [](const ScoreItem& a, const ScoreItem& b) { return a.value > b.value; });

    for (Face b : win.bonus)
        if (suitOf(b) == Suit::Flower || suitOf(b) == Suit::Season)
            s.items.push_back({ Fan::Flower, fanValue(Fan::Flower) });

    for (const ScoreItem& item : s.items) {
        s.total += item.value;
        if (item.fan != Fan::Flower)
            s.qualifying += item.value;
    }
    s.points = s.total;
    return s;
}

std::array<int, 4> ChineseOfficialRules::settle(const WinContext& win, const HandScore& s) const
{
    // Book § 3.9.1: every loser pays the base; whoever let the tile go, or all
    // three on a self-draw, pays the hand's points as well.
    std::array<int, 4> pay {};
    if (win.winner < 0 || win.winner > 3)
        return pay;
    const bool onADiscard = win.payer >= 0 && win.payer < 4;
    for (int seat = 0; seat < 4; ++seat) {
        if (seat == win.winner)
            continue;
        const int owed = kBase + (!onADiscard || seat == win.payer ? s.points : 0);
        pay[std::size_t(seat)] -= owed;
        pay[std::size_t(win.winner)] += owed;
    }
    return pay;
}

} // namespace mahjong
