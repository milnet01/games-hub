#include "canastaai.h"

#include <algorithm>

namespace canasta {

namespace {

int pileValue(const std::vector<Card>& pile, const Rules& r)
{
    int v = 0;
    for (const Card& c : pile)
        v += cardValue(c, r);
    return v;
}

int countRank(const std::vector<Card>& h, int rank)
{
    return int(std::count_if(h.begin(), h.end(),
                             [&](const Card& c) { return c.rank == rank; }));
}

// What a side's table is worth as things stand, less the cards THIS seat is
// holding. The partner's hand is deliberately not passed: this AI sees its own
// cards, the pack and both tables, and nothing else. Nothing in this file reads
// a hand it is not entitled to, and GHUB-0114 does not start.
int handShowing(const Team& t, const std::vector<Card>& ourCards, const Rules& r)
{
    return handScoreFor(t, ourCards, {}, false, false, r);
}

// What one card still in somebody's hand is worth against them, taken off this
// seat's own cards because they are the only hand it can see. Used to dock the
// three hidden hands at the rate the visible one sets.
double perCardHeld(const std::vector<Card>& ourCards, const Rules& r)
{
    if (ourCards.empty())
        return 0.0;
    int v = 0;
    for (const Card& c : ourCards)
        v += cardValue(c, r);
    return double(v) / double(ourCards.size());
}

} // namespace

// Two rounds of the table. The same figure chooseDiscard already calls late in
// the hand, and for the same reason: past it there are too few turns left for
// the pack to come back or for a plan to change.
constexpr int kEndgameStock = 8;

double discardRisk(const Team& theirs, int rank, int pileSize, bool frozen, const Rules& r)
{
    const Meld* m = theirs.meldOfRank(rank);
    if (m == nullptr)
        return 0.0;
    // A frozen pile needs a matching PAIR out of hand before the top card is
    // worth anything, which is rare enough that feeding their meld is an
    // ordinary throw rather than a dangerous one.
    if (frozen)
        return 0.0;
    // And a rank they have already closed cannot take the pile off anybody once
    // the house rule says so, which makes it the safest card in the hand.
    if (r.canastaMakesRankSafe && m->isCanasta(r))
        return -20.0;

    // Handing the pile over at all, which costs more the bigger it is.
    double risk = 25.0 + 0.4 * double(pileSize);
    // On top of that, the closer their meld stands to a canasta the bigger the
    // gift: six of a rank on the table is one card short of 500 points, three
    // is not. This is what picks the least damaging throw when the hand holds
    // nothing safe at all — feed the meld with furthest to go.
    const int away = r.canastaSize - m->size();
    if (away > 0)
        risk += 140.0 / double(away);
    return risk;
}

int minusOnOffer(const Team& theirs, const Rules& r)
{
    if (!caughtAMinus(theirs, r))
        return 0;

    int showing = 0;
    for (const Meld& m : theirs.melds)
        showing += m.value(r);
    // Red threes only swing where they have opened. handScoreFor docks an
    // unopened side for them whatever happens, so there is nothing there to win.
    if (theirs.opened) {
        const int n = int(theirs.redThrees.size());
        showing += n == 4 ? r.allRedThreesValue : n * r.redThreeValue;
    }
    // Once for the points they do not get, once for the same points taken off.
    return 2 * showing;
}

int packWorthStayingFor(const std::vector<Card>& pile, const std::vector<Card>& hand,
                        const Team& mine, bool frozen, const Rules& r)
{
    // A thin pack is not worth staying in for however takeable it is.
    if (int(pile.size()) < 8)
        return 0;

    // Frozen against us — or unopened, which Engine::canTakePile treats exactly
    // the same way — wants two naturals out of hand, so the pack is coming back
    // only if we are already holding a pair. Twos are wild and threes cannot be
    // melded, so neither is a key to anything.
    if (frozen || (r.pileFrozenUntilOpened && !mine.opened)) {
        for (int rank = kAce; rank <= kKing; ++rank) {
            if (rank == 2 || rank == 3)
                continue;
            if (countRank(hand, rank) >= 2)
                return pileValue(pile, r);
        }
        return 0;
    }

    // Otherwise it comes back on any throw into a rank we have down, so the
    // more of the pack sits in ranks we have already melded the likelier the
    // next throw into it is ours. A quarter of the pack is the bar.
    int ours = 0;
    for (const Card& c : pile)
        if (!isWild(c) && mine.meldOfRank(c.rank) != nullptr)
            ++ours;
    return ours * 4 >= int(pile.size()) ? pileValue(pile, r) : 0;
}

double throwCaution(const Team& theirs, int openRequirement)
{
    // A side that has already opened takes the pack on the ordinary terms, so
    // there is nothing to discount and the throw is judged exactly as it was.
    if (theirs.opened)
        return 1.0;

    // A side that has NOT opened has to open OFF the pack to take it: the meld
    // it takes with must clear its own minimum too. That is a far higher bar
    // than holding two matching cards, and it rises with the band they are on —
    // so being careful what you throw at them is caution spent on something
    // that mostly cannot happen.
    //
    // Floored rather than run to nothing. A big lay-down off the pack is hard,
    // not impossible, and it is precisely how a side stuck all hand comes back
    // in one move.
    return std::max(0.30, 1.0 - double(openRequirement) / 170.0);
}

double fishingWorth(int held, int packSize, int unseen)
{
    // Fishing is throwing one of three or more of a rank, one at a time, until
    // a pair is left -- so the seat that discards to you reads the rank as safe,
    // follows with it, and hands you the pack (GHUB-0103). Below three there is
    // no bait to throw: the pair IS the key and breaking it buys nothing.
    if (held < 3)
        return 0.0;
    // Nothing to win. The same five cards a freeze asks for, and for the same
    // reason -- a thin pack is not worth advertising a rank for.
    if (packSize < 5)
        return 0.0;
    // And nobody left to take the bait. If every card of the rank is already
    // accounted for -- melded, in the pack, or in this hand -- then no seat is
    // holding one to follow with, and the throw is an advertisement nobody can
    // answer.
    if (unseen <= 0)
        return 0.0;
    return (8.0 + 1.2 * double(std::min(packSize, 15))) * std::min(1.0, double(unseen) / 3.0);
}

int packWorthHoldingFor(int unseen)
{
    // Seven at one unseen, down to a floor of four at three or more. The pack
    // is the prize and the cards held back are the cost, so the two move
    // against each other. The floor is where this began as a flat figure.
    return std::max(4, 9 - 2 * std::min(3, unseen));
}

bool runTheHandDead(int ourShowing, int theirShowing, int stockLeft, const Rules& r)
{
    // It depends ENTIRELY on the house rule. deadHandIfNobodyGoesOut makes a
    // hand the stock kills void, so neither side scores it; classic Canasta
    // scores such a hand where it stands — pagat.com is explicit that when a
    // player who wishes to draw cannot, "the play ends immediately and the hand
    // is scored" — which hands the leader their points anyway. So the flag is
    // read rather than assumed.
    if (!r.deadHandIfNobodyGoesOut)
        return false;
    // And only in sight of the end. Steering a whole hand towards a dead stock
    // is a different and far larger judgement than this; two rounds of the
    // table is the window where refusing to finish actually kills it.
    if (stockLeft > kEndgameStock)
        return false;
    // Only when the hand as it stands is theirs, and by enough to be worth
    // throwing away. The two figures move every turn, so a hand ahead by ten
    // points is not one to burn.
    return theirShowing > ourShowing + r.goingOutBonus;
}

bool closeFirstUnderAMinus(const Meld& a, const Meld& b)
{
    // Nearest a canasta first. Size rather than the shortfall because
    // canastaSize is the same for both and cancels.
    return a.size() > b.size();
}

double blackThreeWorth(int packSize, double caution)
{
    // A black three blocks the pack for exactly one turn — until it is covered
    // — so what it buys is whatever would have been taken in that turn. Two
    // readings, and they are the two the owner named: how fat the pack is, and
    // whether the seat to the left is live. That seat is always an opponent,
    // partners sitting opposite, so throwCaution's grading of how live their
    // side is answers the second exactly as it answers a dangerous throw.
    //
    // The pack-size half is the step this already had — a block is worth much
    // more once there is a pack behind it — kept at its measured figures rather
    // than smoothed into a curve, which measured worse.
    return (12.0 + (packSize >= 4 ? 10.0 : 0.0)) * caution;
}

double feedPressure(const std::vector<Card>& hand, const Team& theirs, const Rules& r)
{
    if (hand.empty())
        return 0.0;
    int feeders = 0;
    for (const Card& c : hand)
        if (!isWild(c) && c.rank != 3 && discardRisk(theirs, c.rank, 0, false, r) > 0.0)
            ++feeders;
    return double(feeders) / double(hand.size());
}

bool freezeIsWorthTheWild(const std::vector<Card>& hand, const Team& mine, const Team& theirs,
                          const Rules& r)
{
    // Their side is in and ours is not, so the pack is THEIR asset and the
    // freeze makes them find a natural pair for it. Published strategy leads
    // with this one: "freeze when opponents have melded but you have not."
    if (theirs.opened && !mine.opened)
        return true;

    // We are fishing — holding back naturals of a rank our own side already has
    // down. The owner's second trigger, and the one his opening play creates on
    // purpose: GHUB-0122 keeps a pair of the very rank it has just melded, and
    // the freeze is what makes that pair the only key to the pack.
    for (int rank = kAce; rank <= kKing; ++rank) {
        if (rank == 2 || rank == 3)
            continue;
        int held = 0;
        for (const Card& c : hand)
            if (c.rank == rank && !isWild(c))
                ++held;
        if (held >= 2 && mine.meldOfRank(rank) != nullptr)
            return true;
    }

    // Or this hand is going to keep feeding them whatever it throws. The
    // owner's first trigger, and it subsumes the "little risk" qualifier on his
    // second: where the risk is high this fires anyway, so both readings of a
    // fishing hand end in the same answer.
    //
    // A third of the hand is the bar. It is a judgement rather than a
    // measurement — GHUB-0110 settled that the ladder cannot separate a
    // threshold this size from noise, so nothing here is fitted to it.
    return feedPressure(hand, theirs, r) >= 1.0 / 3.0;
}

bool freezeCostsUsThePack(const std::vector<Card>& pile, const std::vector<Card>& hand,
                          const Team& mine, const Rules& r)
{
    // A side that has not opened is ALREADY held to two naturals out of hand by
    // pileFrozenUntilOpened, so a freeze takes nothing from it and everything
    // from an opened opponent. packWorthStayingFor reads that same shut-out
    // position as "worth staying for", which is true and is not the question
    // here — so it is asked only of a side the freeze could actually cost.
    //
    // Measured, because the carve-out is not a nicety: without it this stopped
    // 263 of the 409 freezes the suite's full games made before GHUB-0122, and
    // every one of them wrongly.
    if (r.pileFrozenUntilOpened && !mine.opened)
        return false;
    return packWorthStayingFor(pile, hand, mine, false, r) > 0;
}

bool freezeBudgetLeft(int freezesThisHand)
{
    // "Do not freeze more than twice per hand" — rarepike.com, card-games.ca,
    // suitedgames.com and pagat.com, read 2026-08-24 and listed on GHUB-0113.
    return freezesThisHand < 2;
}

void Ai::noteHand(const Engine& e)
{
    // A fresh deal is the one moment the stock GROWS; inside a hand it only
    // ever shrinks, and taking the pack does not touch it at all. Keyed on that
    // rather than on Engine::handNumber(), because a new game restarts the
    // numbering while these seats live for the whole session — so the number
    // alone would carry a spent budget into the next game.
    const int stock = e.stockCount();
    if (stock > m_lastStock)
        m_freezes = 0;
    m_lastStock = stock;
}

int seenSoFar(const std::vector<Card>& hand, const std::vector<Card>& pile, const Team& one,
              const Team& two, int rank)
{
    int n = countRank(hand, rank) + countRank(pile, rank);
    for (const Team* t : { &one, &two })
        if (const Meld* m = t->meldOfRank(rank))
            n += countRank(m->cards, rank);
    return n;
}

double packCountSafety(int unseen, int pileSize)
{
    // The fewer that are unseen, the less likely the pair that would punish the
    // throw. Capped at four, because past that the rank is simply live.
    double safety = 9.0 * (4 - std::min(4, unseen));
    // Weighted by what handing the pack over would actually cost. This is the
    // one pack-size weight in the discard, and GHUB-0108's ask to generalise it
    // over the whole accumulator was tried and measured WORSE — see that bullet
    // before moving it again.
    safety -= 0.9 * double(unseen) * (1.0 + 0.12 * double(pileSize));
    return safety;
}

bool Ai::killingTheHand(const Engine& e) const
{
    const int seat = e.currentSeat();
    const Rules& r = e.rules();
    const int us = teamOf(seat);
    const Team& mine = e.team(us);
    const Team& theirs = e.team(us ^ 1);

    // Cards still held count AGAINST the side holding them when a hand is
    // scored where it stands, which is the whole reason to kill one. Only this
    // seat's own hand is visible, so the other three are docked at the same
    // per-card rate rather than not at all -- docking ours alone made our
    // figure systematically the lower of the two, and runTheHandDead's margin
    // was then eaten by an ordinary hand (GHUB-0149).
    //
    // Card COUNTS are public at a real table, so reading them is not the seat
    // looking at hands it cannot see.
    const std::vector<Card>& ours = e.hand(seat);
    const double perCard = perCardHeld(ours, r);
    int ourHeld = 0;
    int theirHeld = 0;
    for (int s = 0; s < kSeats; ++s)
        (teamOf(s) == us ? ourHeld : theirHeld) += int(e.hand(s).size());

    const int ourShowing =
        handShowing(mine, ours, r) - int(perCard * double(ourHeld - int(ours.size())));
    const int theirShowing = handShowing(theirs, {}, r) - int(perCard * double(theirHeld));
    return runTheHandDead(ourShowing, theirShowing, e.stockCount(), r);
}

int Ai::seen(const Engine& e, int rank) const
{
    return seenSoFar(e.hand(e.currentSeat()), e.pile(), e.team(0), e.team(1), rank);
}

bool Ai::worthHolding(const Engine& e, int rank, int naturals) const
{
    // Hard as well as Expert (GHUB-0129). Without it Hard pruned its opening
    // for the pack and then laid the same cards on the same turn, so the prune
    // decided nothing there at all. Easy and Medium keep the play they have --
    // Easy is meant to make real mistakes, and taking those away flattens the
    // ladder.
    if (m_level != Level::Hard && m_level != Level::Expert)
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
    // Bait only works if somebody can still throw one. A rank with every card
    // accounted for will never come, so those points belong on the table. How
    // many that is comes off the pack: a save may carry one deck, and against a
    // hardcoded eight this gate could never fire at all (GHUB-0149).
    const int unseen = 4 * e.rules().decks - seen(e, rank);
    if (unseen <= 0)
        return false;
    // Holding a rank keeps the pile takeable the moment one is thrown, and the
    // bigger the pile the more that is worth -- graded by how live the rank
    // still is rather than the flat four it was (GHUB-0129).
    if (int(e.pile().size()) < packWorthHoldingFor(unseen))
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
    // Losing the hand badly with the stock nearly gone, the owner's tactic is
    // to run the pack dead rather than let it score (GHUB-0114). Going out is
    // the one thing that certainly stops that, so it is refused outright.
    if (killingTheHand(e))
        return false;

    // Catching the other side without a canasta is the biggest swing in the
    // game — under the house rule where a side with none counts nothing in its
    // favour, it is the difference between their melds paying them and costing
    // them. So the hand is worth ending sooner against a side that has none.
    const Team& theirs = e.team(teamOf(e.currentSeat()) ^ 1);
    const bool theyAreShort = !theirs.hasCanasta(r);
    // Expert reads the position sooner and starts closing from further out.
    std::size_t reach = m_level == Level::Expert ? (theyAreShort ? 12u : 7u)
                                                 : (theyAreShort ? 8u : 5u);

    // The two halves of one judgement, and they pull opposite ways on purpose.
    // A minus sitting on the table argues for going NOW; a pack that keeps
    // feeding us argues for staying, because every turn we stay in is worth
    // more than the going-out bonus. Both come back in POINTS, so they are
    // weighed against each other rather than ordered — and with the house rule
    // off and a thin pack both are zero, which leaves the reach above exactly
    // as it was.
    const int minus = minusOnOffer(theirs, r);
    const int pack = packWorthStayingFor(e.pile(), e.hand(e.currentSeat()), mine, e.pileFrozen(), r);

    if (minus > pack && minus > r.goingOutBonus) {
        // Press, in proportion to what is actually on offer rather than by a
        // flat step. Capped, because past a point the hand cannot be emptied
        // any faster however much the minus is worth.
        reach += std::size_t(std::min(4, minus / std::max(1, 2 * r.goingOutBonus)));
    } else if (pack > minus) {
        // Milk it. Held above zero rather than switched off, so this slows the
        // hand down rather than stopping it ending at all.
        reach = std::min(reach, std::size_t(3));
    }

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
    // Under the minus rule the FIRST canasta is insurance rather than a bonus,
    // and it is worth far more than the 300 it pays (GHUB-0107). A side that
    // ends the hand without one has its melds AND its red threes taken off its
    // score rather than added — handScoreFor — so every point it has showing is
    // worth two to the other side. caughtAMinus is that exact position, named.
    //
    // So progress towards it beats holding a rank back for the pack. Only for a
    // rank already on our table: those are the cards that shorten the road to a
    // canasta, and a rank with nothing down yet is a road not started.
    if (caughtAMinus(mine, r) && mine.meldOfRank(rank) != nullptr)
        return false;
    // GHUB-0104's third condition, and the mirror of GHUB-0114 -- the same
    // position read the other way. With the stock nearly gone the pack may
    // never come back at all, so cards held for it are about to be caught in
    // hand: play out while the hand is still worth something. Unless it is one
    // we would rather kill, where nothing is going to be scored and holding on
    // costs nothing.
    if (e.stockCount() <= kEndgameStock && !killingTheHand(e))
        return false;
    // The owner's rule, and it is about the HAND rather than the rank: "if you
    // have 10 cards in your hand you should still play as if you could take the
    // pack, because when you pick up cards you more than likely are going to
    // build up pairs". A big hand has draws still to come, so a single card of
    // a rank is a pair waiting to happen and holding it back is holding a real
    // key. A small hand has no such future — stop building, and put the points
    // on the table before the hand ends with them still in it.
    //
    // Two-thirds of a deal rather than a bare number, so a house rule dealing
    // thirteen moves the line with it. An earlier attempt keyed this on how
    // many of the RANK were held (`naturals < 2`) and was reverted: that is
    // true about this turn and false about the hand. See GHUB-0104.
    if (int(e.hand(e.currentSeat()).size()) * 3 <= r.handSize * 2)
        return false;
    return true;
}

bool Ai::wantsPile(const Engine& e) const
{
    // Running the hand dead means emptying the stock, and taking the pack does
    // not touch it. So the pack is left alone while there is a choice — draw
    // instead, and bring the end nearer (GHUB-0114). Ai::draw still takes it
    // once the stock is gone, because refusing then does not kill the hand, it
    // stalls the turn: Engine::startTurn ends a hand only where the pack cannot
    // be taken by anybody.
    if (killingTheHand(e))
        return false;

    const int n = int(e.pile().size());
    const int v = pileValue(e.pile(), e.rules());
    const Team& mine = e.team(teamOf(e.currentSeat()));

    switch (m_level) {
    case Level::Easy:
        // Plays its own hand and barely watches the table.
        return n >= 7;
    case Level::Medium:
        return v >= 30 || n >= 5;
    case Level::Hard:
    case Level::Expert:
        // One test, deliberately shared. Opening off the pile is usually the
        // strongest move in the game, so a thin pile is worth taking to do it,
        // and past that both levels want pile control.
        //
        // These were two arms with two comments describing a difference the
        // expressions did not have -- byte-identical, so the ladder could not
        // say which half was wrong (GHUB-0149). Merged rather than given an
        // invented threshold: separating them is a strength change and belongs
        // with the judgement work, not with a correctness fix.
        return !mine.opened || v >= 15 || n >= 3;
    }
    return false;
}

bool Ai::draw(Engine& e)
{
    noteHand(e);
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

        // Lay only what the minimum asks for. Everything else is better in
        // hand: it is a rank the opposition would then stop throwing, and the
        // pack is the thing worth waiting for. Cheapest ranks come out of the
        // lay-down first, so the points that do go down are the ones least
        // wanted back.
        //
        // This used to run only while the pile was frozen. It runs always now
        // (GHUB-0122), because the advice is general — "meld the minimum needed
        // cards, even if your hand can support more", "a meld of three or four
        // cards is a placeholder, the score lives in the canasta" — and because
        // the owner's opening play needs it on a pack that is NOT yet frozen:
        // freezing it is the second half of the same move.
        if (m_level != Level::Easy) {
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

        // Keep the pair that takes the pack (GHUB-0122). Opening on the
        // minimum is only half the owner's play; the other half is WHAT the
        // minimum is made of. From four eights and a joker against a bar of
        // 50, laying the joker and two eights clears it at 70 and leaves the
        // other two eights in hand — and two matching naturals in hand are the
        // only thing that takes a frozen pack. Open on the four eights instead
        // and the bar is not even cleared, let alone the pair kept.
        //
        // The joker rather than the two, deliberately: the substitution has to
        // carry the value the pair took with it, and twenty would not have
        // made the bar. So the wilds are spent dearest first here, which is the
        // opposite of everywhere else in this file.
        //
        // One rank only. Keeping one pair back is the play; keeping several
        // spends a wild card apiece for keys to the same lock. And only where
        // there is a pack worth holding a key to — the same five cards a freeze
        // asks for, since freezing it is the move this sets up.
        if ((m_level == Level::Hard || m_level == Level::Expert) && int(e.pile().size()) >= 5) {
            std::vector<Card> wilds = wildsAvailable();
            std::reverse(wilds.begin(), wilds.end()); // dearest first
            int total = 0;
            for (const std::vector<Card>& g : groups)
                total += valueOf(g);

            for (std::vector<Card>& g : groups) {
                // No early exit on an empty wild pile: a group big enough to
                // spare its pair and still be legal needs no wild at all, and
                // the want check below is what says so.
                std::vector<Card> cand(g.begin(), g.end() - std::min<std::size_t>(2, g.size()));
                if (int(cand.size()) < r.minNaturalsPerMeld)
                    continue; // cannot spare a pair and still be a meld
                // Two different figures, and conflating them was GHUB-0149: how
                // many cards the meld is still SHORT of legal, and how many
                // wilds it may hold at all. Taking the smaller of the two and
                // then demanding at least one meant a five-group -- which
                // spares its pair and is already legal on three naturals --
                // asked for a wild it had no room for, so this only ever fired
                // on a group of exactly four.
                const int want = std::max(0, r.minMeldSize - int(cand.size()));
                if (want > wildRoom(int(cand.size())) || want > int(wilds.size()))
                    continue;
                cand.insert(cand.end(), wilds.begin(), wilds.begin() + want);
                if (total - valueOf(g) + valueOf(cand) < need)
                    continue; // the pair cannot be spared and the bar still met
                total += valueOf(cand) - valueOf(g);
                g = cand;
                break;
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
    //
    // CHEAPEST first, but ONLY while caught a minus (GHUB-0107). It is the
    // opposite of the ordinary instinct and deliberately so: the meld closest
    // to a canasta is also the one likeliest to fill naturally, and closing it
    // with a wild card turns a 500-point natural into a 300-point mixed one.
    // Under the minus rule that trade is worth making, because a side ending
    // the hand with no canasta at all has everything it has showing taken off
    // its score instead of added — insurance beats the better canasta later.
    //
    // Left unsorted otherwise, and that was measured: sorting unconditionally
    // cost a game of medium v easy and 200 points of its margin, which is the
    // natural canasta being spent.
    if (m_level != Level::Easy) {
        std::vector<const Meld*> order;
        for (const Meld& m : mine.melds)
            order.push_back(&m);
        if (caughtAMinus(mine, r))
            std::stable_sort(order.begin(), order.end(),
                             [](const Meld* a, const Meld* b) {
                                 return closeFirstUnderAMinus(*a, *b);
                             });
        for (const Meld* mp : order) {
            const Meld& m = *mp;
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

    // Two a hand is the ceiling (GHUB-0113). Every freeze costs a wild card
    // worth 20 or 50 in the hand, and past the second the pack is being locked
    // more often than it is being won.
    if (!freezeBudgetLeft(m_freezes))
        return false;

    const int seat = e.currentSeat();
    const std::vector<Card>& h = e.hand(seat);
    const Rules& r = e.rules();
    const Team& mine = e.team(teamOf(seat));
    const Team& theirs = e.team(teamOf(seat) ^ 1);
    // Freezing is only worth 20 points of wild card when the pile is big enough
    // to be worth coming back for, and only if we hold the pair that takes it.
    if (int(e.pile().size()) < 5)
        return false;
    // "Freeze only if you hold natural pairs so you can break the freeze
    // yourself" — and that is ANY pair, not a pair of whatever happens to be on
    // top now. The freezing card goes on top of the pack, so the rank this seat
    // has to match is one nobody has thrown yet; the key is a pair in hand
    // waiting for its rank to come round. Reading it as the current top card
    // ruled out the owner's whole opening play (GHUB-0122), which keeps two
    // eights back and then freezes a pack showing something else entirely.
    bool holdsAKey = false;
    for (int rank = kAce; rank <= kKing && !holdsAKey; ++rank)
        if (rank != 2 && rank != 3 && countRank(h, rank) >= 2)
            holdsAKey = true;
    if (!holdsAKey)
        return false;
    // Never freeze a pack this side is already positioned to claim: a freeze
    // locks everyone out, us included, so it throws away the very thing it was
    // meant to protect. packWorthStayingFor asks exactly that question and
    // answers in points, nonzero meaning the pack is coming back to us as
    // things stand.
    //
    // The other half of that advice — do not freeze a pack you could take on
    // THIS turn — cannot arise and so is not coded. Ai::draw has already taken
    // any pack it could take and wanted, and wantsPile wants every pack of
    // three cards or more at these two levels, while a freeze needs five.
    // Never freeze a pack this side is already positioned to claim: a freeze
    // locks everyone out, us included, so it throws away the very thing it was
    // meant to protect. packWorthStayingFor asks exactly that question and
    // answers in points, nonzero meaning the pack is coming back to us as
    // things stand.
    //
    // The carve-out is not a nicety and was measured: a side that has not
    // opened is ALREADY held to two naturals out of hand by
    // pileFrozenUntilOpened, so a freeze takes nothing from it and everything
    // from an opened opponent — and packWorthStayingFor reads that same
    // shut-out position as "worth staying for". Without the carve-out this
    // guard stopped 263 of the 409 freezes the suite's full games make, every
    // one of them wrongly.
    //
    // The other half of the advice — do not freeze a pack you could take on
    // THIS turn — cannot arise and so is not coded. Ai::draw has already taken
    // any pack it could take and wanted, and wantsPile wants every pack of
    // three cards or more at these two levels, while a freeze needs five.
    if (freezeCostsUsThePack(e.pile(), h, mine, r))
        return false;
    // Not while the hand is being closed out. "Do not freeze when reaching
    // go-out and freezing slows your tempo disproportionately" — and the wild
    // card is worth more finishing a canasta than locking a pack this side has
    // no more turns to come back for.
    if (closingOut(e, h.size()))
        return false;
    if (!freezeIsWorthTheWild(h, mine, theirs, r))
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
    // Nothing thrown this turn can be taken. See Engine::discardCannotBeTaken()
    // for why the fourth seat of the first round is NOT covered — its throw is
    // the one that is live, because the seat after it is the first seat playing
    // a second time.
    const bool freeThrow = e.discardCannotBeTaken();
    // How live the other side is. It grades the safety of a throw (GHUB-0121)
    // and what a black three's one turn of block is worth (GHUB-0109). Easy and
    // Medium stay naive on purpose, so for them it is simply 1.0 and the two
    // judgements read as they always did.
    const double caution = (m_level == Level::Hard || m_level == Level::Expert)
        ? throwCaution(theirs, e.openRequirement(teamOf(seat) ^ 1))
        : 1.0;

    // A seat with no cards has nothing to throw, and front() on an empty hand
    // is undefined rather than merely wrong. Engine::playAndDiscard never asks
    // in that state; this is the guard that says so out loud (GHUB-0149).
    if (h.empty())
        return {};
    Card best = h.front();
    double bestScore = -1e9;

    for (const Card& c : h) {
        if (!e.canDiscard(c))
            continue;

        // Higher is more willing to throw.
        double score = -double(cardValue(c, r));

        // Everything about whether this card would hand the pile over goes here
        // rather than straight onto the score, because there is one turn in the
        // hand where the whole question is moot. Applied in full below unless
        // the throw cannot be taken, so play outside the first round is
        // unchanged to the last decimal.
        double safety = 0.0;

        if (m_level != Level::Easy) {
            // Do not break up your own holdings. Black threes are NOT exempt,
            // and that was measured rather than assumed (GHUB-0109): exempting
            // them on the argument that a run of them is dead weight cost 10
            // games of expert v hard and 6 of hard v medium. This penalty is
            // what stops a black three being thrown the turn it turns up, which
            // is what makes it a timing card at all.
            score -= 7.0 * (countRank(h, c.rank) - 1);
            if (mine.meldOfRank(c.rank) != nullptr)
                score -= 22.0;
            if (isWild(c))
                score -= 45.0;

            // Feeding a rank the opponents have melded hands them the pile —
            // and the closer that meld is to a canasta, the bigger the gift.
            safety -= discardRisk(theirs, c.rank, pileSize, e.pileFrozen(), r);

            if (isBlackThree(c)) {
                if (freeThrow)
                    // A black three buys exactly one thing: it stops the next
                    // seat taking the pile. That seat cannot take it anyway, so
                    // throwing one now spends the block and gets nothing back,
                    // while holding it keeps the block for a turn that matters.
                    // Small enough that a hand with nothing else legal to throw
                    // still throws it.
                    score -= 15.0;
                else
                    // Cannot be melded until someone goes out, so it is nearly
                    // free to throw, and it shuts the pack down for a round —
                    // worth more the fatter the pack and the liver the seat to
                    // the left, rather than the flat step this used to be.
                    score += blackThreeWorth(pileSize, caution);
            }
        }

        if (m_level == Level::Hard || m_level == Level::Expert) {
            // The bigger the pile, the more it costs to hand it over, so lean
            // harder on ranks nobody has shown an interest in.
            const int shown = countRank(e.pile(), c.rank);
            safety -= 2.5 * shown;
            if (isWild(c))
                score -= 25.0;
        }

        if (m_level == Level::Expert) {
            // A rank the others have already parted with is one they are
            // unlikely to hold a pair of, so it is SAFER to throw than a rank
            // nobody has let go. Hard reads the pile the other way round, the
            // way most players do.
            //
            // This term is exactly what a fishing seat aims at (GHUB-0103), and
            // it now counts SOURCES rather than cards: each seat that threw
            // one counts once however many it threw (GHUB-0124).
            //
            // That is the whole defence, and the shape of it matters. Capping
            // the raw count at two was tried first and cost 7 games of expert v
            // hard, 117 -> 110 of 240: a fisher throws them ONE AT A TIME, so
            // the bait shows as one or two of a rank -- precisely where a raw
            // count still means what it says -- and the cap only penalised the
            // honest read. The tell was never HOW MANY, it is WHO. Two of a
            // rank from two different seats is the table genuinely letting it
            // go; two from the SAME seat is one player telling you it holds
            // more of them, and the second is no evidence of safety at all.
            //
            // Cards nobody threw -- the deal's up-card -- count once between
            // them, because they came out of the stock rather than out of a
            // hand. So a pile restored from a save written before the
            // provenance existed reads as one cautious source rather than as
            // several confident ones.
            safety += 50.0 * double(e.pileRankSources(c.rank, seat));
        }

        if (m_level == Level::Hard || m_level == Level::Expert) {
            // Count the pack. Four of every rank per deck exist; the ones this
            // seat can see are the ones nobody else can be holding. A rank
            // wholly accounted for cannot take the pile off anybody, which
            // makes it the safest card there is — and the fewer that are
            // unseen, the less likely the pair that would punish the throw.
            //
            // Off the pack rather than a hardcoded eight (GHUB-0149): a save
            // may carry one deck, and the difference went negative there, which
            // flipped packCountSafety's second term positive with no bound.
            const int ofRank = 4 * r.decks;
            safety += packCountSafety(ofRank - seen(e, c.rank), pileSize);

            // Never break a pair while an unpaired card would do, and never
            // feed a rank this seat is holding as bait for the pile.
            if (countRank(h, c.rank) >= 3)
                score -= 30.0;
            // Unless breaking it IS the play. Three of a rank going down to two
            // is bait, and the penalty above -- along with the -7 a card apiece
            // higher up -- is what stopped this seat ever throwing it. Offset
            // rather than removed: fishing has to win the discard on its
            // merits, against every other card in the hand.
            //
            // Nothing thrown on a free turn can be taken by anybody, so there
            // is no follow-up to fish for either.
            if (!freeThrow && m_level == Level::Expert)
                score += fishingWorth(countRank(h, c.rank), pileSize, ofRank - seen(e, c.rank));
            // A wild card thrown away is a canasta thrown away.
            if (isWild(c))
                score -= 60.0;
            // Late in the hand the pile matters less than not being caught
            // holding points, so the ranking flattens towards raw value.
            if (e.stockCount() <= kEndgameStock)
                score -= 0.5 * double(cardValue(c, r));
        }

        // How much that whole judgement is worth here. Only the two levels that
        // read the pack this closely get it: Easy and Medium stay naive on
        // purpose, and a player who is careful when they need not be is a
        // weaker player rather than a broken one.
        safety *= caution;

        // Nothing thrown now can be taken, so every judgement above about
        // handing the pile over is measuring a risk that cannot happen. Dropped
        // rather than reversed: with no meld on the table there is no such
        // thing as a dangerous rank yet, so there is nothing to aim at — what
        // is left is the honest question of which card this hand least wants,
        // which is what the terms outside `safety` already answer.
        if (!freeThrow)
            score += safety;

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
    noteHand(e);
    for (const auto& [cards, rank] : chooseMelds(e)) {
        e.meldCards(cards, rank);
        // Melding out ends the hand; there is nothing left to discard.
        if (e.phase() != Engine::Phase::Play)
            return;
    }

    Card wild;
    if (wantsToFreeze(e, wild) && e.discard(wild)) {
        ++m_freezes;
        return;
    }

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
