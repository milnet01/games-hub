#include "mahjongplayer.h"

#include <algorithm>
#include <climits>
#include <tuple>

namespace mahjong {

namespace {

// Counts of the suit tiles and honours, which are all a reckoning needs.
using Counts = std::array<int, kFlowerFirst>;

bool numbered(Face t)
{
    const Suit s = suitOf(t);
    return s == Suit::Circles || s == Suit::Bamboo || s == Suit::Characters;
}

Counts countTiles(const std::vector<Face>& tiles)
{
    Counts c {};
    for (Face t : tiles)
        if (t > 0 && t < kFlowerFirst)
            ++c[t];
    return c;
}

struct Reckoning {
    int needed = 4;
    bool chows = true;
    int best = INT_MAX;
};

// The standard reckoning: each set saves two tiles, each pair or two-thirds of
// a run saves one, up to the number of sets still needed, and one pair is the
// hand's own. A tile is tried in a set, in a pair, in a part-set, and left
// alone, so the best of those readings is found.
void reckon(Counts& c, Face i, int sets, int partials, bool pair, Reckoning& r)
{
    while (i < kFlowerFirst && c[i] == 0)
        ++i;
    if (i >= kFlowerFirst) {
        const int usable = std::min(partials, r.needed - sets);
        r.best = std::min(r.best, 2 * (r.needed - sets) - usable - (pair ? 1 : 0));
        return;
    }
    const bool run = r.chows && numbered(i);
    const int rank = rankOf(i);
    if (c[i] >= 3 && sets < r.needed) {
        c[i] -= 3;
        reckon(c, i, sets + 1, partials, pair, r);
        c[i] += 3;
    }
    if (run && rank <= 7 && c[i + 1] > 0 && c[i + 2] > 0 && sets < r.needed) {
        --c[i];
        --c[i + 1];
        --c[i + 2];
        reckon(c, i, sets + 1, partials, pair, r);
        ++c[i];
        ++c[i + 1];
        ++c[i + 2];
    }
    if (!pair && c[i] >= 2) {
        c[i] -= 2;
        reckon(c, i, sets, partials, true, r);
        c[i] += 2;
    }
    if (sets + partials < r.needed) {
        if (c[i] >= 2) {
            c[i] -= 2;
            reckon(c, i, sets, partials + 1, pair, r);
            c[i] += 2;
        }
        if (run && rank <= 8 && c[i + 1] > 0) {
            --c[i];
            --c[i + 1];
            reckon(c, i, sets, partials + 1, pair, r);
            ++c[i];
            ++c[i + 1];
        }
        if (run && rank <= 7 && c[i + 2] > 0) {
            --c[i];
            --c[i + 2];
            reckon(c, i, sets, partials + 1, pair, r);
            ++c[i];
            ++c[i + 2];
        }
    }
    --c[i];
    reckon(c, i, sets, partials, pair, r);
    ++c[i];
}

// What the seat is building towards, worked out afresh each time from the
// tiles it holds: a hand worth the 3-fan minimum rarely happens by accident.
struct Plan {
    Suit suit = Suit::None; // the numbered suit it holds most of
    bool oneSuit = false;   // Mixed or Pure one suit, 3 or 7 fan
    bool pungs = false;     // All pungs, 3 fan
};

Plan planFor(const SeatTiles& s)
{
    std::array<int, 8> bySuit {};
    int honours = 0;
    std::vector<Face> all = s.concealed;
    bool chowLaid = false;
    int pairs = 0;
    for (const Meld& m : s.melds) {
        all.push_back(m.tile);
        all.push_back(m.tile);
        all.push_back(m.tile);
        if (m.kind == SetKind::Chow)
            chowLaid = true;
        else
            ++pairs;
    }
    for (Face t : all) {
        if (isHonour(t))
            ++honours;
        else if (numbered(t))
            ++bySuit[std::size_t(suitOf(t))];
    }
    Plan p;
    int most = 0;
    for (Suit suit : { Suit::Circles, Suit::Bamboo, Suit::Characters }) {
        if (bySuit[std::size_t(suit)] > most) {
            most = bySuit[std::size_t(suit)];
            p.suit = suit;
        }
    }
    const bool meldsFit = std::all_of(s.melds.begin(), s.melds.end(), [&p](const Meld& m) {
        return isHonour(m.tile) || suitOf(m.tile) == p.suit;
    });
    p.oneSuit = meldsFit && most + honours >= 10;
    const Counts c = countTiles(s.concealed);
    for (int n : c)
        if (n >= 2)
            ++pairs;
    p.pungs = !chowLaid && pairs >= 4;
    return p;
}

bool valueHonour(Face t, int seatWind, int prevailing)
{
    if (suitOf(t) == Suit::Dragon)
        return true;
    return suitOf(t) == Suit::Wind && (rankOf(t) == seatWind || rankOf(t) == prevailing);
}

bool offPlan(const Plan& plan, Face t)
{
    return plan.oneSuit && numbered(t) && suitOf(t) != plan.suit;
}

// How much a tile is worth keeping, beyond what the reckoning sees: the
// patterns that score.
double keepValue(const std::vector<Face>& tiles, Face t, const Plan& plan, int seatWind, int prevailing)
{
    const int n = int(std::count(tiles.begin(), tiles.end(), t));
    if (isHonour(t)) {
        if (valueHonour(t, seatWind, prevailing))
            return n >= 2 ? 6.0 : 1.5;
        return n >= 2 ? 3.0 : (plan.oneSuit ? 0.5 : 0.0);
    }
    double keep = 1.0 + (n >= 2 ? 2.0 : 0.0);
    if (plan.oneSuit && suitOf(t) == plan.suit)
        keep += 3.0;
    const int rank = rankOf(t);
    if (rank >= 3 && rank <= 7)
        keep += 0.5;
    const auto has = [&tiles](Face f) { return std::find(tiles.begin(), tiles.end(), f) != tiles.end(); };
    if ((rank > 1 && has(Face(t - 1))) || (rank < 9 && has(Face(t + 1))))
        keep += 1.0;
    return keep;
}

std::vector<Face> without(std::vector<Face> tiles, Face t, int count)
{
    for (int i = 0; i < count; ++i) {
        const auto at = std::find(tiles.begin(), tiles.end(), t);
        if (at != tiles.end())
            tiles.erase(at);
    }
    return tiles;
}

// The discard that leaves `tiles` best placed, with its reckoning.
std::pair<Face, int> bestDiscard(const std::vector<Face>& tiles, int melds, const Plan& plan, int seatWind,
                                 int prevailing)
{
    Face choice = 0;
    std::tuple<int, int, double> best { INT_MAX, INT_MAX, 0.0 };
    Face last = 0;
    for (Face t : tiles) {
        if (t == last)
            continue;
        last = t;
        const int after = tilesFromReady(without(tiles, t, 1), melds, !plan.pungs);
        const std::tuple<int, int, double> key { offPlan(plan, t) ? 0 : 1, after,
                                                 keepValue(tiles, t, plan, seatWind, prevailing) };
        if (choice == 0 || key < best) {
            best = key;
            choice = t;
        }
    }
    return { choice, std::get<1>(best) };
}

const Move* find(const std::vector<Move>& moves, Action action)
{
    for (const Move& m : moves)
        if (m.action == action)
            return &m;
    return nullptr;
}

} // namespace

int tilesFromReady(const std::vector<Face>& tiles, int melds, bool chows)
{
    Counts c = countTiles(tiles);
    Reckoning r;
    r.needed = std::max(0, 4 - melds);
    r.chows = chows;
    reckon(c, 1, 0, 0, false, r);
    return r.best;
}

Move chooseMove(const Hand& hand, int seat)
{
    const std::vector<Move> legal = hand.legalMoves(seat);
    if (legal.empty())
        return {};
    if (const Move* win = find(legal, Action::Win))
        return *win;

    const SeatTiles& s = hand.seat(seat);
    const Plan plan = planFor(s);
    const int wind = hand.seatWind(seat);
    const int prevailing = hand.prevailingWind();
    const int melds = int(s.melds.size());

    if (hand.phase() == Phase::Turn) {
        // A kong is a set that costs nothing and draws a tile, so take it.
        if (const Move* kong = find(legal, Action::ConcealedKong))
            return *kong;
        if (const Move* kong = find(legal, Action::AddedKong))
            return *kong;
        const Face throwAway = bestDiscard(s.concealed, melds, plan, wind, prevailing).first;
        return { std::uint8_t(seat), Action::Discard, throwAway, 0 };
    }

    const Move pass { std::uint8_t(seat), Action::Pass, 0, 0 };
    if (hand.phase() != Phase::Claims)
        return pass;

    const int now = tilesFromReady(s.concealed, melds, !plan.pungs);
    const Face t = hand.offered();
    const bool wanted = valueHonour(t, wind, prevailing) || plan.pungs
        || (plan.oneSuit && (isHonour(t) || suitOf(t) == plan.suit));
    for (Action set : { Action::Kong, Action::Pung }) {
        const Move* claim = find(legal, set);
        if (claim == nullptr)
            continue;
        const std::vector<Face> rest = without(s.concealed, t, set == Action::Kong ? 3 : 2);
        // A kong draws its replacement; a pung must throw a tile.
        const int after = set == Action::Kong ? tilesFromReady(rest, melds + 1, !plan.pungs)
                                              : bestDiscard(rest, melds + 1, plan, wind, prevailing).second;
        if (valueHonour(t, wind, prevailing) || (wanted && after < now))
            return *claim;
    }
    // A chow gives up the concealed hand's fan, so only a seat building one
    // suit -- whose fan does not depend on staying concealed -- takes one.
    if (plan.oneSuit && !plan.pungs && suitOf(t) == plan.suit) {
        for (const Move& m : legal) {
            if (m.action != Action::Chow)
                continue;
            std::vector<Face> rest = s.concealed;
            for (int k = 0; k < 3; ++k)
                if (Face(m.chowLow + k) != t)
                    rest = without(rest, Face(m.chowLow + k), 1);
            if (bestDiscard(rest, melds + 1, plan, wind, prevailing).second < now)
                return m;
        }
    }
    return pass;
}

} // namespace mahjong
