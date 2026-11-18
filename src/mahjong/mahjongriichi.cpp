#include "mahjongriichi.h"

#include <algorithm>

namespace mahjong {

namespace {

// One row of GHUB-0244 § 4.7: the han concealed and with a set claimed, 0
// where the row is for concealed hands only.
struct Row {
    Fan fan;
    int concealed;
    int open;
};

constexpr int kY = RiichiRules::kYakuman;

// § 4.7's order: the yaku by han, then the yakuman.
const std::vector<Row>& rows()
{
    using F = Fan;
    static const std::vector<Row> all {
        { F::Riichi, 1, 0 },
        { F::Ippatsu, 1, 0 },
        { F::FullyConcealed, 1, 0 },
        { F::Pinfu, 1, 0 },
        { F::PureDoubleChow, 1, 0 },
        { F::AllSimples, 1, 1 },
        { F::DragonPung, 1, 1 },
        { F::SeatWindPung, 1, 1 },
        { F::PrevailingWindPung, 1, 1 },
        { F::KongReplacement, 1, 1 },
        { F::RobbingAKong, 1, 1 },
        { F::LastTile, 1, 1 },
        { F::LastTileClaim, 1, 1 },
        { F::DoubleRiichi, 2, 0 },
        { F::SevenDifferentPairs, 2, 0 },
        { F::MixedTripleChow, 2, 1 },
        { F::PureStraight, 2, 1 },
        { F::HalfOutsideHand, 2, 1 },
        { F::TriplePung, 2, 2 },
        { F::ThreeConcealedPungs, 2, 2 },
        { F::ThreeKongs, 2, 2 },
        { F::AllPungs, 2, 2 },
        { F::SmallThreeDragons, 2, 2 },
        { F::AllTerminalsAndHonours, 2, 2 },
        { F::TwicePureDoubleChow, 3, 0 },
        { F::MixedOneSuit, 3, 2 },
        { F::FullOutsideHand, 3, 2 },
        { F::BlessingOfMan, 5, 0 },
        { F::PureOneSuit, 6, 5 },
        // The yakuman: concealed only where the meaning says so.
        { F::ThirteenOrphans, kY, 0 },
        { F::NineGates, kY, 0 },
        { F::HeavenlyHand, kY, 0 },
        { F::BlessingOfEarth, kY, 0 },
        { F::FourConcealedPungs, kY, 0 },
        { F::FourKongs, kY, kY },
        { F::AllGreen, kY, kY },
        { F::AllTerminals, kY, kY },
        { F::AllHonours, kY, kY },
        { F::BigThreeDragons, kY, kY },
        { F::SmallFourWinds, kY, kY },
        { F::BigFourWinds, kY, kY },
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

int roundUpTo(int value, int step)
{
    return (value + step - 1) / step * step;
}

bool isTerminalOrHonour(Face t)
{
    return isTerminal(t) || isHonour(t);
}

bool holdsTerminalOrHonour(const Meld& m)
{
    if (m.kind == SetKind::Chow)
        return rankOf(m.tile) == 1 || rankOf(m.tile) == 7;
    return isTerminalOrHonour(m.tile);
}

// What a reading's sets and tiles hold, counted once for every test below.
struct Facts {
    std::vector<Meld> chows;
    int pungs = 0; // kongs among them
    int kongs = 0;
    int concealedPungs = 0;
    int dragonPungs = 0;
    int windPungs = 0;
    bool seatWindPung = false;
    bool prevailingWindPung = false;
    bool honours = false;
    bool simples = true; // no one, nine or honour
    bool allHonours = true;
    bool allTerminals = true;
    bool allTerminalsAndHonours = true;
    bool allGreen = true;
    int numberedSuits = 0;
    bool pairScores = false; // a dragon, the seat's wind or the round's
};

Facts factsOf(const WinContext& win)
{
    Facts f;
    for (const Meld& m : win.sets) {
        if (m.kind == SetKind::Chow) {
            f.chows.push_back(m);
            continue;
        }
        ++f.pungs;
        if (m.kind == SetKind::Kong)
            ++f.kongs;
        if (m.concealed)
            ++f.concealedPungs;
        if (suitOf(m.tile) == Suit::Dragon)
            ++f.dragonPungs;
        if (suitOf(m.tile) == Suit::Wind) {
            ++f.windPungs;
            f.seatWindPung = f.seatWindPung || rankOf(m.tile) == win.seatWind;
            f.prevailingWindPung = f.prevailingWindPung || rankOf(m.tile) == win.prevailingWind;
        }
    }
    std::array<bool, 4> suits {}; // indexed by Suit: circles, bamboo, characters
    for (Face t : win.tiles) {
        const bool honour = isHonour(t);
        f.honours = f.honours || honour;
        f.allHonours = f.allHonours && honour;
        f.allTerminals = f.allTerminals && isTerminal(t);
        f.allTerminalsAndHonours = f.allTerminalsAndHonours && isTerminalOrHonour(t);
        f.simples = f.simples && !isTerminalOrHonour(t);
        f.allGreen = f.allGreen && isGreen(t);
        if (isNumbered(t))
            suits[std::size_t(suitOf(t))] = true;
    }
    f.numberedSuits = int(std::count(suits.begin(), suits.end(), true));
    if (win.tiles.empty()) // only a test's bare context; no "all" holds of nothing
        f.allHonours = f.allTerminals = f.allTerminalsAndHonours = f.allGreen = false;
    const Suit pairSuit = suitOf(win.pair);
    f.pairScores = win.pair != 0
        && (pairSuit == Suit::Dragon
            || (pairSuit == Suit::Wind
                && (rankOf(win.pair) == win.seatWind || rankOf(win.pair) == win.prevailingWind)));
    return f;
}

bool nineGates(const WinContext& win, const Facts& f)
{
    if (win.claimedASet || f.kongs > 0 || f.numberedSuits != 1 || f.honours || win.tiles.size() != 14)
        return false;
    std::array<int, 10> byRank {};
    for (Face t : win.tiles)
        ++byRank[std::size_t(rankOf(t))];
    bool gates = byRank[1] >= 3 && byRank[9] >= 3;
    for (int r = 2; r <= 8; ++r)
        gates = gates && byRank[std::size_t(r)] >= 1;
    return gates;
}

std::vector<Fan> yakumanOf(const WinContext& win, const Facts& f)
{
    std::vector<Fan> y;
    if (win.shape == Shape::ThirteenOrphans)
        y.push_back(Fan::ThirteenOrphans);
    if (nineGates(win, f))
        y.push_back(Fan::NineGates);
    if (win.firstGoRound && win.selfDrawn && !win.claimedASet)
        y.push_back(win.seatWind == 1 ? Fan::HeavenlyHand : Fan::BlessingOfEarth);
    if (f.pungs == 4 && f.concealedPungs == 4)
        y.push_back(Fan::FourConcealedPungs);
    if (f.kongs == 4)
        y.push_back(Fan::FourKongs);
    if (f.allGreen)
        y.push_back(Fan::AllGreen);
    if (f.allTerminals)
        y.push_back(Fan::AllTerminals);
    if (f.allHonours)
        y.push_back(Fan::AllHonours);
    if (f.dragonPungs == 3)
        y.push_back(Fan::BigThreeDragons);
    if (f.windPungs == 4)
        y.push_back(Fan::BigFourWinds);
    else if (f.windPungs == 3 && suitOf(win.pair) == Suit::Wind)
        y.push_back(Fan::SmallFourWinds);
    return y;
}

// Pinfu's wait: the winning tile at either end of its chow, and not the 3 of
// 1-2-3 nor the 7 of 7-8-9.
bool twoSided(const WinContext& win)
{
    if (win.winningIn < 0 || std::size_t(win.winningIn) >= win.sets.size())
        return false;
    const Meld& m = win.sets[std::size_t(win.winningIn)];
    if (m.kind != SetKind::Chow)
        return false;
    return (win.winningTile == m.tile && rankOf(m.tile) != 7)
        || (win.winningTile == m.tile + 2 && rankOf(m.tile) != 1);
}

// How many pairs of identical chows the reading holds: 2 is Twice pure double
// chow, 1 Pure double chow.
int chowPairs(std::vector<Meld> chows)
{
    std::sort(chows.begin(), chows.end(), [](const Meld& a, const Meld& b) { return a.tile < b.tile; });
    int pairs = 0;
    for (std::size_t i = 0; i + 1 < chows.size();) {
        if (chows[i].tile == chows[i + 1].tile) {
            ++pairs;
            i += 2;
        } else {
            ++i;
        }
    }
    return pairs;
}

bool hasChowIn(const std::vector<Meld>& chows, Face low)
{
    return std::any_of(chows.begin(), chows.end(), [low](const Meld& m) { return m.tile == low; });
}

bool mixedTripleChow(const Facts& f)
{
    for (int r = 1; r <= 7; ++r)
        if (hasChowIn(f.chows, Face(kCirclesFirst + r - 1)) && hasChowIn(f.chows, Face(kBambooFirst + r - 1))
            && hasChowIn(f.chows, Face(kCharactersFirst + r - 1)))
            return true;
    return false;
}

bool pureStraight(const Facts& f)
{
    for (Face first : { kCirclesFirst, kBambooFirst, kCharactersFirst })
        if (hasChowIn(f.chows, first) && hasChowIn(f.chows, Face(first + 3)) && hasChowIn(f.chows, Face(first + 6)))
            return true;
    return false;
}

bool triplePung(const WinContext& win)
{
    const auto pungOf = [&win](Face t) {
        return std::any_of(win.sets.begin(), win.sets.end(),
                           [t](const Meld& m) { return m.kind != SetKind::Chow && m.tile == t; });
    };
    for (int r = 1; r <= 9; ++r)
        if (pungOf(Face(kCirclesFirst + r - 1)) && pungOf(Face(kBambooFirst + r - 1))
            && pungOf(Face(kCharactersFirst + r - 1)))
            return true;
    return false;
}

// A one, nine or honour in every set and the pair, with at least one chow.
bool outside(const WinContext& win, const Facts& f)
{
    if (win.shape != Shape::Sets || f.chows.empty() || !isTerminalOrHonour(win.pair))
        return false;
    return std::all_of(win.sets.begin(), win.sets.end(), holdsTerminalOrHonour);
}

int doraIn(const std::vector<Face>& tiles, const std::vector<Face>& indicators)
{
    int n = 0;
    for (Face ind : indicators)
        n += int(std::count(tiles.begin(), tiles.end(), RiichiRules::doraFrom(ind)));
    return n;
}

// GHUB-0244 § 4.8 step 6.
int minipoints(const WinContext& win, bool pinfu)
{
    if (win.shape == Shape::SevenDifferentPairs)
        return 25;
    int fu = 20;
    if (!win.claimedASet && !win.selfDrawn)
        fu += 10;
    if (win.selfDrawn && !pinfu)
        fu += 2;
    for (const Meld& m : win.sets) {
        if (m.kind == SetKind::Chow)
            continue;
        int pung = m.concealed ? 4 : 2;
        if (isTerminalOrHonour(m.tile))
            pung *= 2;
        if (m.kind == SetKind::Kong)
            pung *= 4;
        fu += pung;
    }
    const Suit pairSuit = suitOf(win.pair);
    if (pairSuit == Suit::Dragon
        || (pairSuit == Suit::Wind && (rankOf(win.pair) == win.seatWind || rankOf(win.pair) == win.prevailingWind)))
        fu += 2;
    // An edge, closed or pair wait in this reading.
    if (win.winningIn == -1) {
        fu += 2;
    } else if (win.winningIn >= 0 && std::size_t(win.winningIn) < win.sets.size()) {
        const Meld& m = win.sets[std::size_t(win.winningIn)];
        if (m.kind == SetKind::Chow
            && (win.winningTile == m.tile + 1 || (win.winningTile == m.tile + 2 && rankOf(m.tile) == 1)
                || (win.winningTile == m.tile && rankOf(m.tile) == 7)))
            fu += 2;
    }
    if (win.claimedASet && fu == 20)
        fu = 22;
    return roundUpTo(fu, 10);
}

bool scoresHigher(const HandScore& a, const HandScore& b)
{
    return a.points != b.points ? a.points > b.points : a.total > b.total;
}

// Every yaku of a reading below yakuman, with the dora (§ 4.8 steps 2 to 7).
HandScore countYaku(const WinContext& win, const Facts& f)
{
    HandScore s;
    const bool open = win.claimedASet;
    const auto add = [&s, open](Fan fan, int times = 1) {
        const Row* r = rowFor(fan);
        const int value = r == nullptr ? 0 : open ? r->open : r->concealed;
        for (int i = 0; i < times && value > 0; ++i)
            s.items.push_back({ fan, value });
    };

    if (win.doubleRiichi)
        add(Fan::DoubleRiichi);
    else if (win.riichi)
        add(Fan::Riichi);
    if (win.ippatsu && (win.riichi || win.doubleRiichi))
        add(Fan::Ippatsu);
    if (win.selfDrawn)
        add(Fan::FullyConcealed);
    const bool pinfu = win.shape == Shape::Sets && f.chows.size() == 4 && !f.pairScores && twoSided(win) && !open;
    if (pinfu)
        add(Fan::Pinfu);
    const int chowTwins = chowPairs(f.chows);
    if (chowTwins == 2)
        add(Fan::TwicePureDoubleChow);
    else if (chowTwins == 1)
        add(Fan::PureDoubleChow);
    if (f.simples)
        add(Fan::AllSimples);
    if (f.dragonPungs == 2 && suitOf(win.pair) == Suit::Dragon)
        add(Fan::SmallThreeDragons);
    add(Fan::DragonPung, f.dragonPungs);
    if (f.seatWindPung)
        add(Fan::SeatWindPung);
    if (f.prevailingWindPung)
        add(Fan::PrevailingWindPung);
    if (win.kongReplacement)
        add(Fan::KongReplacement);
    else if (win.lastTile)
        add(Fan::LastTile);
    if (win.robbedKong)
        add(Fan::RobbingAKong);
    if (win.lastTileClaim)
        add(Fan::LastTileClaim);
    if (win.shape == Shape::SevenDifferentPairs)
        add(Fan::SevenDifferentPairs);
    if (mixedTripleChow(f))
        add(Fan::MixedTripleChow);
    if (pureStraight(f))
        add(Fan::PureStraight);
    if (outside(win, f))
        add(f.honours ? Fan::HalfOutsideHand : Fan::FullOutsideHand);
    if (triplePung(win))
        add(Fan::TriplePung);
    if (f.concealedPungs == 3)
        add(Fan::ThreeConcealedPungs);
    if (f.kongs == 3)
        add(Fan::ThreeKongs);
    if (f.pungs == 4)
        add(Fan::AllPungs);
    if (f.allTerminalsAndHonours)
        add(Fan::AllTerminalsAndHonours);
    if (f.numberedSuits == 1)
        add(f.honours ? Fan::MixedOneSuit : Fan::PureOneSuit);

    int han = 0;
    for (const ScoreItem& i : s.items)
        han += i.value;
    if (han == 0) {
        s.items.clear();
        return s; // no yaku, no win: qualifying and points stay 0
    }
    s.qualifying = han;
    const int dora = doraIn(win.tiles, win.doraIndicators);
    if (dora > 0)
        s.items.push_back({ Fan::Dora, dora });
    const int ura = win.riichi || win.doubleRiichi ? doraIn(win.tiles, win.uraIndicators) : 0;
    if (ura > 0)
        s.items.push_back({ Fan::UraDora, ura });
    s.total = han + dora + ura;
    s.fu = minipoints(win, pinfu);
    s.points = RiichiRules::basicPoints(s.total, s.fu);
    s.tier = RiichiRules::tierFor(s.total, s.fu);
    s.limit = s.tier != Limit::None;
    return s;
}

} // namespace

int RiichiRules::openValue(Fan fan)
{
    const Row* r = rowFor(fan);
    return r == nullptr ? 0 : r->open;
}

int RiichiRules::basicPoints(int han, int fu)
{
    if (han >= kYakuman)
        return 8000;
    if (han >= 11)
        return 6000;
    if (han >= 8)
        return 4000;
    if (han >= 6)
        return 3000;
    if (han == 5)
        return 2000;
    if (han <= 0)
        return 0;
    const int basic = fu * (1 << (han + 2));
    return basic > 1900 ? 2000 : basic;
}

Limit RiichiRules::tierFor(int han, int fu)
{
    if (han >= kYakuman)
        return Limit::Yakuman;
    if (han >= 11)
        return Limit::Sanbaiman;
    if (han >= 8)
        return Limit::Baiman;
    if (han >= 6)
        return Limit::Haneman;
    return han > 0 && basicPoints(han, fu) == 2000 ? Limit::Mangan : Limit::None;
}

Face RiichiRules::doraFrom(Face indicator)
{
    const Suit s = suitOf(indicator);
    const int r = rankOf(indicator);
    switch (s) {
    case Suit::Circles:
    case Suit::Bamboo:
    case Suit::Characters: return r == 9 ? Face(indicator - 8) : Face(indicator + 1);
    case Suit::Wind: return r == 4 ? kWindFirst : Face(indicator + 1);
    case Suit::Dragon: {
        // Red, green, white is the tile order; red to white, white to green
        // and green to red is the book's.
        constexpr Face red = kDragonFirst;
        constexpr Face green = kDragonFirst + 1;
        constexpr Face white = kDragonFirst + 2;
        return indicator == red ? white : indicator == white ? green : red;
    }
    default: return 0;
    }
}

std::vector<ScoreItem> RiichiRules::fanList() const
{
    std::vector<ScoreItem> all;
    for (const Row& r : rows())
        all.push_back({ r.fan, r.concealed });
    return all;
}

TableRules RiichiRules::table() const
{
    TableRules t;
    t.deadWall = 14;
    t.startingPoints = kStartingPoints;
    t.fullGameRounds = 2;
    t.mostKongs = 4;
    t.riichiStake = kRiichiStake;
    t.doraIndicators = true;
    t.furiten = true;
    t.severalWinners = true;
    t.kongsAfterADraw = true;
    t.addAHeldTile = true;
    t.noSwapCalling = true;
    t.robConcealedKong = true;
    t.liability = true;
    return t;
}

HandScore RiichiRules::score(const WinContext& win) const
{
    const Facts f = factsOf(win);

    // Step 1: a yakuman reading scores its yakuman alone, and two pay as one.
    const std::vector<Fan> yakuman = yakumanOf(win, f);
    if (!yakuman.empty()) {
        HandScore s;
        for (Fan y : yakuman)
            s.items.push_back({ y, kYakuman });
        s.total = kYakuman;
        s.qualifying = kYakuman;
        s.points = basicPoints(kYakuman, 0);
        s.tier = Limit::Yakuman;
        s.limit = true;
        return s;
    }

    HandScore s = countYaku(win, f);
    // Step 5: Blessing of man stands alone, no other yaku and no dora; the
    // hand is scored both ways and the higher kept.
    if (win.firstGoRound && !win.selfDrawn && !win.claimedASet) {
        HandScore man;
        const int han = rowFor(Fan::BlessingOfMan)->concealed;
        man.items.push_back({ Fan::BlessingOfMan, han });
        man.total = han;
        man.qualifying = han;
        man.fu = minipoints(win, false);
        man.points = basicPoints(han, man.fu);
        man.tier = tierFor(han, man.fu);
        man.limit = true;
        if (scoresHigher(man, s))
            return man;
    }
    return s;
}

std::array<int, 4> RiichiRules::settle(const WinContext& win, const HandScore& s) const
{
    std::array<int, 4> pay {};
    if (win.winner < 0 || win.winner > 3 || s.points <= 0)
        return pay;
    const int dealer = (win.winner - (win.seatWind - 1) + 4) % 4;
    const bool dealerWins = win.winner == dealer;
    const int basic = s.points;
    const auto take = [&pay, &win](int from, int amount) {
        pay[std::size_t(from)] -= amount;
        pay[std::size_t(win.winner)] += amount;
    };
    const bool liable = win.liable >= 0 && win.liable < 4 && win.liable != win.winner
        && std::any_of(s.items.begin(), s.items.end(), [](const ScoreItem& i) {
               return i.fan == Fan::BigThreeDragons || i.fan == Fan::BigFourWinds;
           });

    if (win.payer >= 0 && win.payer < 4) {
        const int value = roundUpTo(basic * (dealerWins ? 6 : 4), 100);
        const int counters = kCounterOnDiscard * win.counters;
        if (liable && win.liable != win.payer) {
            // The liable seat and the discarder split the hand evenly; the
            // discarder alone pays the counters.
            take(win.liable, value / 2);
            take(win.payer, value - value / 2 + counters);
        } else {
            take(win.payer, value + counters);
        }
        return pay;
    }
    // Self-drawn: the dealer pays twice what the others do.
    for (int seat = 0; seat < 4; ++seat) {
        if (seat == win.winner)
            continue;
        const int share = roundUpTo(basic * (dealerWins || seat == dealer ? 2 : 1), 100);
        // Under liability the liable seat pays every share, counters too.
        take(liable ? win.liable : seat, share + kCounterEach * win.counters);
    }
    return pay;
}

std::array<int, 4> RiichiRules::settleDraw(const std::array<bool, 4>& ready) const
{
    std::array<int, 4> pay {};
    const int n = int(std::count(ready.begin(), ready.end(), true));
    if (n == 0 || n == 4)
        return pay;
    for (std::size_t seat = 0; seat < 4; ++seat)
        pay[seat] = ready[seat] ? kNotenTotal / n : -kNotenTotal / (4 - n);
    return pay;
}

std::array<int, 4> RiichiRules::finalScores(const std::array<int, 4>& totals) const
{
    // § 3.7.1: each total less the start, plus the bonus for its place; tied
    // seats share their places' bonuses evenly.
    static constexpr std::array<int, 4> kPlace { 15000, 5000, -5000, -15000 };
    std::array<int, 4> out {};
    for (std::size_t seat = 0; seat < 4; ++seat) {
        int above = 0;
        int tied = 0;
        for (int other : totals) {
            above += other > totals[seat] ? 1 : 0;
            tied += other == totals[seat] ? 1 : 0;
        }
        int bonus = 0;
        for (int place = above; place < above + tied; ++place)
            bonus += kPlace[std::size_t(place)];
        out[seat] = totals[seat] - kStartingPoints + bonus / tied;
    }
    return out;
}

} // namespace mahjong
