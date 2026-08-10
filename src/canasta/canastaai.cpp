#include "canastaai.h"

#include <algorithm>

namespace canasta {

namespace {

int pileValue(const Engine& e)
{
    int v = 0;
    for (const Card& c : e.pile())
        v += cardValue(c, e.rules());
    return v;
}

int countRank(const std::vector<Card>& h, int rank)
{
    return int(std::count_if(h.begin(), h.end(),
                             [&](const Card& c) { return c.rank == rank; }));
}

} // namespace

bool Ai::wantsPile(const Engine& e) const
{
    const int n = int(e.pile().size());
    const int v = pileValue(e);
    const Team& mine = e.team(teamOf(e.currentSeat()));

    switch (m_level) {
    case Level::Easy:
        // Plays its own hand and barely watches the table.
        return n >= 7;
    case Level::Medium:
        return v >= 30 || n >= 5;
    case Level::Hard:
        // Opening off the pile is usually the strongest move in the game, so it
        // is worth taking a thin pile to do it.
        return !mine.opened || v >= 15 || n >= 3;
    }
    return false;
}

bool Ai::draw(Engine& e)
{
    std::vector<Card> take;
    const bool canTake = e.findPileTake(take);
    // With the stock gone, taking the pile is the only way to keep playing.
    const bool must = e.stockCount() == 0;

    if (canTake && (must || wantsPile(e)) && e.takePile(take))
        return true;
    if (e.drawFromStock())
        return false;
    // Neither worked: take the pile on any terms rather than stall the hand.
    if (canTake)
        e.takePile(take);
    return true;
}

std::vector<std::pair<std::vector<Card>, int>> Ai::chooseMelds(const Engine& e) const
{
    const int seat = e.currentSeat();
    const int t = teamOf(seat);
    const Team& mine = e.team(t);
    const Rules& r = e.rules();

    std::vector<std::pair<std::vector<Card>, int>> out;
    std::vector<Card> remaining = e.hand(seat);

    const auto naturalsOf = [&](int rank) {
        std::vector<Card> v;
        for (const Card& c : remaining)
            if (c.rank == rank && !isWild(c))
                v.push_back(c);
        return v;
    };
    const auto wildsAvailable = [&] {
        std::vector<Card> v;
        for (const Card& c : remaining)
            if (isWild(c))
                v.push_back(c);
        // Spend a two before a joker; they are worth the same in a meld and the
        // joker is worth more in hand.
        std::sort(v.begin(), v.end(), [&](const Card& a, const Card& b) {
            return cardValue(a, r) < cardValue(b, r);
        });
        return v;
    };
    const auto consume = [&](const std::vector<Card>& cards) {
        for (const Card& c : cards) {
            auto it = std::find(remaining.begin(), remaining.end(), c);
            if (it != remaining.end())
                remaining.erase(it);
        }
    };
    const auto valueOf = [&](const std::vector<Card>& cards) {
        int v = 0;
        for (const Card& c : cards)
            v += cardValue(c, r);
        return v;
    };

    if (!mine.opened) {
        const int need = e.openRequirement(t);

        // Everything that already stands on its own, laid down together — the
        // minimum is measured across the whole opening, not per meld.
        std::vector<Card> lay;
        for (int rank = kKing; rank >= kAce; --rank) {
            if (rank == 3 || rank == 2)
                continue;
            const std::vector<Card> n = naturalsOf(rank);
            if (int(n.size()) >= r.minMeldSize)
                lay.insert(lay.end(), n.begin(), n.end());
        }

        if (!lay.empty() && valueOf(lay) >= need) {
            consume(lay);
            out.push_back({ lay, -1 });
        } else {
            // Short of the minimum, a wild card can turn a pair into a meld.
            // Hard holds its last wild back for a canasta instead.
            const std::vector<Card> wilds = wildsAvailable();
            const bool spendWild = m_level != Level::Hard || wilds.size() > 1;
            bool opened = false;
            for (int rank = kKing; rank >= kAce && spendWild && !opened; --rank) {
                if (rank == 3 || rank == 2)
                    continue;
                std::vector<Card> cand = naturalsOf(rank);
                if (int(cand.size()) < r.minNaturalsPerMeld)
                    continue;
                for (const Card& w : wilds) {
                    if (int(cand.size()) >= r.minMeldSize && valueOf(cand) >= need)
                        break;
                    if (int(cand.size()) - countRank(cand, rank) >= r.maxWildsPerMeld)
                        break;
                    cand.push_back(w);
                }
                if (int(cand.size()) >= r.minMeldSize && valueOf(cand) >= need) {
                    consume(cand);
                    out.push_back({ cand, rank });
                    opened = true;
                }
            }
            if (!opened)
                return out; // cannot open this turn; hold everything
        }
    }

    // Extend what is already down, and lay anything new that stands alone.
    for (int rank = kAce; rank <= kKing; ++rank) {
        if (rank == 3 || rank == 2)
            continue;
        const std::vector<Card> n = naturalsOf(rank);
        if (n.empty())
            continue;
        // A rank with nothing down yet needs a whole meld's worth; one already
        // on the table takes any number of extra cards.
        if (mine.meldOfRank(rank) == nullptr && int(n.size()) < r.minMeldSize)
            continue;
        consume(n);
        out.push_back({ n, -1 });
    }

    // Wild cards to finish a canasta. Worth 300 at least, which beats anything
    // a wild card does sitting in your hand.
    if (m_level != Level::Easy) {
        for (const Meld& m : mine.melds) {
            if (m.rank == 3 || m.isCanasta(r))
                continue;
            const int short_ = r.canastaSize - m.size();
            const int room = r.maxWildsPerMeld - m.wilds();
            const std::vector<Card> wilds = wildsAvailable();
            if (short_ <= 0 || short_ > room || short_ > int(wilds.size()))
                continue;
            const std::vector<Card> spend(wilds.begin(), wilds.begin() + short_);
            consume(spend);
            out.push_back({ spend, m.rank });
        }
    }
    return out;
}

bool Ai::wantsToFreeze(const Engine& e, Card& wild) const
{
    if (m_level != Level::Hard || e.pileFrozen() || e.pile().empty())
        return false;

    const int seat = e.currentSeat();
    const std::vector<Card>& h = e.hand(seat);
    // Freezing is only worth 20 points of wild card when the pile is big enough
    // to be worth coming back for, and only if we hold the pair that takes it.
    if (int(e.pile().size()) < 5)
        return false;
    if (countRank(h, e.pile().back().rank) < 2)
        return false;

    int spare = 0;
    for (const Card& c : h)
        if (isWild(c))
            ++spare;
    if (spare < 2)
        return false;

    for (const Card& c : h) {
        if (c.rank == 2) { // spend the cheap wild, never the joker
            wild = c;
            return true;
        }
    }
    return false;
}

Card Ai::chooseDiscard(const Engine& e) const
{
    const int seat = e.currentSeat();
    const std::vector<Card>& h = e.hand(seat);
    const Rules& r = e.rules();
    const Team& mine = e.team(teamOf(seat));
    const Team& theirs = e.team(teamOf(seat) ^ 1);
    const int pileSize = int(e.pile().size());

    Card best = h.front();
    double bestScore = -1e9;

    for (const Card& c : h) {
        if (!e.canDiscard(c))
            continue;

        // Higher is more willing to throw.
        double score = -double(cardValue(c, r));

        if (m_level != Level::Easy) {
            // Do not break up your own holdings.
            score -= 7.0 * (countRank(h, c.rank) - 1);
            if (mine.meldOfRank(c.rank) != nullptr)
                score -= 22.0;
            if (isWild(c))
                score -= 45.0;

            // Feeding a rank the opponents have melded hands them the pile.
            if (theirs.meldOfRank(c.rank) != nullptr && !e.pileFrozen())
                score -= 25.0 + 0.4 * pileSize;

            // A black three cannot be melded until someone goes out, so it is
            // nearly free to throw, and it shuts the pile down for a round.
            if (isBlackThree(c))
                score += 12.0 + (pileSize >= 4 ? 10.0 : 0.0);
        }

        if (m_level == Level::Hard) {
            // The bigger the pile, the more it costs to hand it over, so lean
            // harder on ranks nobody has shown an interest in.
            const int shown = countRank(e.pile(), c.rank);
            score -= 2.5 * shown;
            if (isWild(c))
                score -= 25.0;
        }

        // A little noise so the same hand is not played identically every time.
        score += double(m_rng() % 100) / (m_level == Level::Easy ? 20.0 : 100.0);

        if (score > bestScore) {
            bestScore = score;
            best = c;
        }
    }
    return best;
}

void Ai::playAndDiscard(Engine& e)
{
    for (const auto& [cards, rank] : chooseMelds(e)) {
        e.meldCards(cards, rank);
        // Melding out ends the hand; there is nothing left to discard.
        if (e.phase() != Engine::Phase::Play)
            return;
    }

    Card wild;
    if (wantsToFreeze(e, wild) && e.discard(wild))
        return;

    if (e.discard(chooseDiscard(e)))
        return;

    // The scored choice was refused for some reason; take the first card the
    // engine will accept rather than leave the turn unfinished.
    const std::vector<Card> hand = e.hand(e.currentSeat());
    for (const Card& c : hand)
        if (e.discard(c))
            return;
}

} // namespace canasta
