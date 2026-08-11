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

int Ai::seen(const Engine& e, int rank) const
{
    int n = countRank(e.hand(e.currentSeat()), rank);
    n += countRank(e.pile(), rank);
    for (int t = 0; t < kTeams; ++t)
        if (const Meld* m = e.team(t).meldOfRank(rank))
            n += countRank(m->cards, rank);
    return n;
}

bool Ai::worthHolding(const Engine& e, int rank, int naturals) const
{
    if (m_level != Level::Expert)
        return false;

    const Team& mine = e.team(teamOf(e.currentSeat()));
    // A rank already on the table is no use as bait — the opponents can see it
    // and will not feed it — so put every one of them down.
    if (mine.meldOfRank(rank) != nullptr)
        return false;
    // With a canasta down and a thinning hand the game is about going out, and
    // cards held back for bait are cards that stop you.
    if (mine.hasCanasta(e.rules()) && int(e.hand(e.currentSeat()).size()) <= 8)
        return false;
    // Holding three of a rank keeps the pile takeable the moment one is thrown,
    // and the bigger the pile the more that is worth. Below four cards it is
    // not worth the points sitting in your hand.
    if (int(e.pile().size()) < 4)
        return false;
    // Bait only works if somebody can still throw one. A rank with all eight
    // accounted for will never come, so those points belong on the table.
    if (seen(e, rank) >= 8)
        return false;
    // Never hold so much that going out becomes impossible, and never hold a
    // rank the opponents have shown they are collecting.
    if (naturals > 4 || e.team(teamOf(e.currentSeat()) ^ 1).meldOfRank(rank) != nullptr)
        return false;
    return int(e.hand(e.currentSeat()).size()) > 5;
}

bool Ai::closingOut(const Engine& e, std::size_t inHand) const
{
    if (m_level == Level::Easy)
        return false;
    const Rules& r = e.rules();
    const Team& mine = e.team(teamOf(e.currentSeat()));
    if (!mine.hasCanasta(r))
        return false; // going out is not even legal yet

    // Catching the other side without a canasta is the biggest swing in the
    // game — under the house rule where a side with none counts nothing in its
    // favour, it is the difference between their melds paying them and costing
    // them. So the hand is worth ending sooner against a side that has none.
    const bool theyAreShort = !e.team(teamOf(e.currentSeat()) ^ 1).hasCanasta(r);
    // Expert reads the position sooner and starts closing from further out.
    const std::size_t reach = m_level == Level::Expert ? (theyAreShort ? 12u : 7u)
                                                       : (theyAreShort ? 8u : 5u);
    return inHand <= reach;
}

bool Ai::holdsWhileFrozen(const Engine& e, int rank, int naturals) const
{
    if (m_level == Level::Easy || !e.pileFrozen())
        return false;

    const Rules& r = e.rules();
    const Team& mine = e.team(teamOf(e.currentSeat()));
    // Completing a canasta is worth more than any pile.
    if (const Meld* m = mine.meldOfRank(rank))
        if (m->size() + naturals >= r.canastaSize)
            return false;
    // And ending the hand beats keeping cards back for a pile you will not get
    // the chance to take.
    if (closingOut(e, e.hand(e.currentSeat()).size()))
        return false;
    return true;
}

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
    case Level::Expert:
        // Pile control is the game, but a pile so thin it is not worth the
        // tempo is one to leave. Measured: taking every pile going is slightly
        // worse than being choosy about the smallest ones.
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

        // How many wild cards a rank may take, which is not always the flat
        // ceiling: a house rule can require a meld to keep more real cards than
        // wild ones, and an opening built without checking that is refused by
        // the engine and leaves the side never opening at all.
        const auto wildRoom = [&](int naturals) {
            int allow = r.maxWildsPerMeld;
            if (r.wildsFewerThanNaturals)
                allow = std::min(allow, naturals - 1);
            return std::max(0, allow);
        };

        // Everything that already stands on its own, laid down together — the
        // minimum is measured across the whole opening, not per meld.
        std::vector<std::vector<Card>> groups;
        for (int rank = kKing; rank >= kAce; --rank) {
            if (rank == 3 || rank == 2)
                continue;
            const std::vector<Card> n = naturalsOf(rank);
            if (int(n.size()) >= r.minMeldSize)
                groups.push_back(n);
        }

        // Opening while the pile is frozen, lay only what the minimum asks for.
        // Everything else is better in hand: it is a rank the opposition would
        // then stop throwing, and the frozen pile is the thing worth waiting
        // for. Cheapest ranks come out of the lay-down first, so the points
        // that do go down are the ones least wanted back.
        if (m_level != Level::Easy && e.pileFrozen()) {
            std::sort(groups.begin(), groups.end(),
                      [&](const std::vector<Card>& a, const std::vector<Card>& b) {
                          return valueOf(a) < valueOf(b);
                      });
            int total = 0;
            for (const std::vector<Card>& g : groups)
                total += valueOf(g);
            for (auto it = groups.begin(); it != groups.end();) {
                const int without = total - valueOf(*it);
                if (without >= need) {
                    total = without;
                    it = groups.erase(it);
                } else {
                    ++it;
                }
            }
        }

        std::vector<Card> lay;
        for (const std::vector<Card>& g : groups)
            lay.insert(lay.end(), g.begin(), g.end());

        // Still short: wild cards turn pairs into melds, and those go down
        // BESIDE the complete ones rather than instead of them. Opening on a
        // single rank alone is a much higher bar than the rules set, and it is
        // what kept a side sitting on 50 for a whole hand.
        if (valueOf(lay) < need) {
            std::vector<Card> wilds = wildsAvailable();
            // Every level spends wild cards to open. Hard used to hold its last
            // one back for a canasta, which measured worse than opening does:
            // being open is what lets you take the pile, and the pile is where
            // canastas come from.
            for (int rank = kKing; rank >= kAce && valueOf(lay) < need; --rank) {
                if (rank == 3 || rank == 2 || wilds.empty())
                    continue;
                const std::vector<Card> n = naturalsOf(rank);
                if (int(n.size()) >= r.minMeldSize || int(n.size()) < r.minNaturalsPerMeld)
                    continue; // already down there, or too few to build on

                std::vector<Card> cand = n;
                int room = std::min(wildRoom(int(n.size())), r.minMeldSize - int(n.size()));
                while (room > 0 && !wilds.empty()) {
                    cand.push_back(wilds.front());
                    wilds.erase(wilds.begin());
                    --room;
                }
                if (int(cand.size()) < r.minMeldSize)
                    continue; // could not be made into a meld after all
                lay.insert(lay.end(), cand.begin(), cand.end());
            }
        }

        // The engine has the final word: if what was built is not a legal
        // opening, hold everything rather than throw refusals at it all hand.
        if (lay.empty() || valueOf(lay) < need || !e.canMeldCards(lay, -1))
            return out;
        consume(lay);
        out.push_back({ lay, -1 });
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
        // Expert keeps some ranks in hand as bait for the pile rather than
        // laying down everything it legally can.
        if (worthHolding(e, rank, int(n.size())))
            continue;
        // And nobody above Easy feeds the table while the pile is frozen.
        if (holdsWhileFrozen(e, rank, int(n.size())))
            continue;
        consume(n);
        out.push_back({ n, -1 });
    }

    // Closing out. With a canasta down and the hand nearly gone, a wild card is
    // worth far more turning a leftover pair into a meld — that is what empties
    // the hand, and going out catches the other side holding everything they
    // have. The lower levels never do this, and hands drag on as a result.
    if (m_level != Level::Easy && m_level != Level::Medium && closingOut(e, remaining.size())) {
        for (int rank = kAce; rank <= kKing; ++rank) {
            if (rank == 3 || rank == 2)
                continue;
            const std::vector<Card> n = naturalsOf(rank);
            if (n.empty() || int(n.size()) >= r.minMeldSize)
                continue;
            if (mine.meldOfRank(rank) != nullptr)
                continue; // already covered by the extend pass above
            if (int(n.size()) < r.minNaturalsPerMeld)
                continue;

            std::vector<Card> wilds = wildsAvailable();
            int room = r.minMeldSize - int(n.size());
            if (r.wildsFewerThanNaturals)
                room = std::min(room, int(n.size()) - 1);
            if (room <= 0 || int(wilds.size()) < room)
                continue;

            std::vector<Card> cand = n;
            cand.insert(cand.end(), wilds.begin(), wilds.begin() + room);
            if (!e.canMeldCards(cand, rank))
                continue;
            consume(cand);
            out.push_back({ cand, rank });
        }
    }

    // Wild cards to finish a canasta. Worth 300 at least, which beats anything
    // a wild card does sitting in your hand.
    if (m_level != Level::Easy) {
        for (const Meld& m : mine.melds) {
            if (m.rank == 3 || m.isCanasta(r))
                continue;
            const int short_ = r.canastaSize - m.size();
            int room = r.maxWildsPerMeld - m.wilds();
            // The same ratio rule binds here: a meld may have to keep more real
            // cards than wild ones however close the canasta is.
            if (r.wildsFewerThanNaturals)
                room = std::min(room, m.naturals() - m.wilds() - 1);
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
    if ((m_level != Level::Hard && m_level != Level::Expert) || e.pileFrozen()
        || e.pile().empty())
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

        if (m_level == Level::Hard || m_level == Level::Expert) {
            // The bigger the pile, the more it costs to hand it over, so lean
            // harder on ranks nobody has shown an interest in.
            const int shown = countRank(e.pile(), c.rank);
            score -= 2.5 * shown;
            if (isWild(c))
                score -= 25.0;
        }

        if (m_level == Level::Expert) {
            // A rank the others have already parted with is one they are
            // unlikely to hold a pair of, so it is SAFER to throw than a rank
            // nobody has let go. Hard reads the pile the other way round, the
            // way most players do.
            score += 50.0 * double(countRank(e.pile(), c.rank));
        }

        if (m_level == Level::Hard || m_level == Level::Expert) {
            // Count the pack. Eight of every rank exist; the ones this seat can
            // see are the ones nobody else can be holding. A rank with all
            // eight accounted for cannot take the pile off anybody, which makes
            // it the safest card there is — and the fewer that are unseen, the
            // less likely the pair that would punish the throw.
            const int unseen = 8 - seen(e, c.rank);
            score += 9.0 * (4 - std::min(4, unseen));
            // Weighted by what handing the pile over would actually cost.
            score -= 0.9 * double(unseen) * (1.0 + 0.12 * pileSize);

            // Never break a pair while an unpaired card would do, and never
            // feed a rank this seat is holding as bait for the pile.
            if (countRank(h, c.rank) >= 3)
                score -= 30.0;
            // A wild card thrown away is a canasta thrown away.
            if (isWild(c))
                score -= 60.0;
            // Late in the hand the pile matters less than not being caught
            // holding points, so the ranking flattens towards raw value.
            if (e.stockCount() < 8)
                score -= 0.5 * double(cardValue(c, r));
        }

        // A little noise so the same hand is not played identically every time.
        // Expert plays the card it thinks is best, every time: variety is a
        // handicap, and at the top of the ladder that is the wrong trade.
        if (m_level != Level::Expert)
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
