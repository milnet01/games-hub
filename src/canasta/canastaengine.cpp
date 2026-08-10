#include "canastaengine.h"

#include <algorithm>
#include <utility>

namespace canasta {

namespace {

// Adds groups onto a team's melds, merging each into the existing meld of that
// rank. Shared by the real commit and by the "would this give us a canasta?"
// check that going out needs, so the two can never disagree.
void applyGroups(Team& t, const std::vector<Meld>& groups)
{
    for (const Meld& g : groups) {
        auto it = std::find_if(t.melds.begin(), t.melds.end(),
                               [&](const Meld& m) { return m.rank == g.rank; });
        if (it == t.melds.end()) {
            t.melds.push_back(g);
            continue;
        }
        it->cards.insert(it->cards.end(), g.cards.begin(), g.cards.end());
    }
}

} // namespace

bool isWild(const Card& c) { return c.rank == kJoker || c.rank == 2; }
bool isRedThree(const Card& c) { return c.rank == 3 && isRed(c); }
bool isBlackThree(const Card& c) { return c.rank == 3 && !isRed(c); }

int cardValue(const Card& c, const Rules& rules)
{
    if (c.rank == kJoker) return rules.jokerValue;
    if (c.rank == 2) return rules.wildTwoValue;
    if (c.rank == kAce) return rules.aceValue;
    if (c.rank == 3) return isRed(c) ? rules.redThreeValue : rules.blackThreeValue;
    return c.rank >= 8 ? rules.highCardValue : rules.lowCardValue;
}

int openRequirementFor(int score, const Rules& rules)
{
    if (score < 0)
        return rules.openMinBelowZero;
    if (score < 1500)
        return rules.openMinUnder1500;
    if (score < 3000)
        return rules.openMinUnder3000;
    return rules.openMinAbove3000;
}

int handScoreFor(const Team& t, const std::vector<Card>& handOne, const std::vector<Card>& handTwo,
                 bool wentOut, bool concealed, const Rules& rules)
{
    int s = 0;
    for (const Meld& m : t.melds) {
        s += m.value(rules);
        if (m.isCanasta(rules))
            s += m.isNatural(rules) ? rules.naturalCanastaBonus : rules.mixedCanastaBonus;
    }

    // Red threes swing both ways: a bonus to a side that has melded, and the
    // same figure against a side that never did.
    const int n = int(t.redThrees.size());
    const int red = n == 4 ? rules.allRedThreesValue : n * rules.redThreeValue;
    s += t.opened ? red : -red;

    if (wentOut)
        s += concealed ? rules.concealedGoingOutBonus : rules.goingOutBonus;

    for (const std::vector<Card>* h : { &handOne, &handTwo })
        for (const Card& c : *h)
            s -= cardValue(c, rules);
    return s;
}

int Meld::naturals() const
{
    return int(std::count_if(cards.begin(), cards.end(),
                             [](const Card& c) { return !isWild(c); }));
}

int Meld::wilds() const
{
    return int(std::count_if(cards.begin(), cards.end(), isWild));
}

int Meld::value(const Rules& r) const
{
    int v = 0;
    for (const Card& c : cards)
        v += cardValue(c, r);
    return v;
}

const Meld* Team::meldOfRank(int rank) const
{
    auto it = std::find_if(melds.begin(), melds.end(),
                           [&](const Meld& m) { return m.rank == rank; });
    return it == melds.end() ? nullptr : &*it;
}

bool Team::hasCanasta(const Rules& r) const
{
    return std::any_of(melds.begin(), melds.end(),
                       [&](const Meld& m) { return m.isCanasta(r); });
}

// ---------------------------------------------------------------------------

Engine::Engine(Rules rules)
    : m_rules(std::move(rules))
    , m_pendingRules(m_rules)
{
}

bool Engine::fail(const QString& why) const
{
    m_error = why;
    return false;
}

void Engine::newGame()
{
    newGame(std::random_device {}());
}

void Engine::newGame(unsigned seed)
{
    m_rng.seed(seed);
    m_rules = m_pendingRules;
    for (Team& t : m_teams)
        t = Team {};
    m_hand = 0;
    // Who deals first is arbitrary; after that it goes round the table.
    m_dealer = int(m_rng() % unsigned(kSeats));
    deal();
}

void Engine::nextHand()
{
    if (m_phase != Phase::HandOver)
        return;
    m_dealer = (m_dealer + 1) % kSeats;
    deal();
}

int Engine::winner() const
{
    if (m_phase != Phase::GameOver)
        return -1;
    return m_teams[0].score >= m_teams[1].score ? 0 : 1;
}

void Engine::newGameFromStock(std::vector<Card> stock, int dealer)
{
    m_rules = m_pendingRules;
    for (Team& t : m_teams)
        t = Team {};
    m_hand = 0;
    m_dealer = ((dealer % kSeats) + kSeats) % kSeats;
    dealFrom(std::move(stock));
}

void Engine::deal()
{
    std::vector<Card> stock = makeDeck(m_rules.decks, 4, m_rules.jokers);
    shuffleCards(stock, m_rng);
    dealFrom(std::move(stock));
}

void Engine::dealFrom(std::vector<Card> stock)
{
    ++m_hand;
    m_outSeat = -1;
    m_outConcealed = false;
    m_frozen = false;
    m_pile.clear();
    m_error.clear();
    m_hasMelded.fill(false);
    for (auto& h : m_hands)
        h.clear();

    for (int t = 0; t < kTeams; ++t) {
        Team& team = m_teams[std::size_t(t)];
        team.melds.clear();
        team.redThrees.clear();
        team.opened = false;
        team.handScore = 0;
        // The opening minimum is fixed by the score you start the hand on, so
        // it cannot move under you as the hand is played.
        m_openReq[std::size_t(t)] = openRequirementFor(team.score, m_rules);
    }

    m_stock = std::move(stock);

    const int first = (m_dealer + 1) % kSeats;
    for (int i = 0; i < m_rules.handSize; ++i) {
        for (int s = 0; s < kSeats; ++s) {
            m_hands[std::size_t((first + s) % kSeats)].push_back(m_stock.back());
            m_stock.pop_back();
        }
    }

    // Red threes dealt into a hand go straight down, each replaced from stock.
    for (int s = 0; s < kSeats; ++s)
        placeRedThrees((first + s) % kSeats, true);

    // The up-card starts the pile. A wild or a red three under it freezes the
    // pile, and another card is turned to cover it.
    while (!m_stock.empty()) {
        const Card c = m_stock.back();
        m_stock.pop_back();
        m_pile.push_back(c);
        if (!isWild(c) && !isRedThree(c))
            break;
        m_frozen = true;
    }

    m_current = first;
    startTurn();
}

bool Engine::placeRedThrees(int seat, bool replace)
{
    std::vector<Card>& h = m_hands[std::size_t(seat)];
    Team& t = m_teams[std::size_t(teamOf(seat))];

    for (std::size_t i = 0; i < h.size();) {
        if (!isRedThree(h[i])) {
            ++i;
            continue;
        }
        t.redThrees.push_back(h[i]);
        h.erase(h.begin() + long(i));
        if (!replace)
            continue;
        if (m_stock.empty())
            return false;
        // The replacement lands at the back, so a red three drawn as a
        // replacement is still found by this same sweep.
        h.push_back(m_stock.back());
        m_stock.pop_back();
    }
    return true;
}

void Engine::startTurn()
{
    m_meldedBeforeTurn = m_hasMelded[std::size_t(m_current)];
    // With the stock gone, play continues only while someone can still take
    // the pile. When nobody can, the hand is over where it stands.
    if (m_stock.empty() && !canTakePileAtAll()) {
        scoreHand();
        return;
    }
    m_phase = Phase::Draw;
}

void Engine::advanceSeat()
{
    m_current = (m_current + 1) % kSeats;
}

void Engine::endTurn()
{
    advanceSeat();
    startTurn();
}

// ---------------------------------------------------------------------------
// Lay-down validation
// ---------------------------------------------------------------------------

bool Engine::handContains(int seat, const std::vector<Card>& cards) const
{
    std::vector<Card> pool = m_hands[std::size_t(seat)];
    for (const Card& c : cards) {
        auto it = std::find(pool.begin(), pool.end(), c);
        if (it == pool.end())
            return false;
        pool.erase(it);
    }
    return true;
}

void Engine::removeFromHand(int seat, const std::vector<Card>& cards)
{
    std::vector<Card>& h = m_hands[std::size_t(seat)];
    for (const Card& c : cards) {
        auto it = std::find(h.begin(), h.end(), c);
        if (it != h.end())
            h.erase(it);
    }
}

int Engine::layDownValue(const std::vector<Card>& cards) const
{
    int v = 0;
    for (const Card& c : cards)
        v += cardValue(c, m_rules);
    return v;
}

bool Engine::group(const std::vector<Card>& cards, bool goingOut, int targetRank,
                   std::vector<Meld>& out, QString& error) const
{
    out.clear();
    if (cards.empty()) {
        error = QStringLiteral("Nothing selected.");
        return false;
    }

    std::vector<Card> wilds;
    // Insertion-ordered rather than sorted, so the same selection always
    // produces the same groups — the self-test depends on that.
    std::vector<std::pair<int, std::vector<Card>>> naturals;

    for (const Card& c : cards) {
        if (isRedThree(c)) {
            error = QStringLiteral("A red three is a bonus card, not one you meld.");
            return false;
        }
        if (isWild(c)) {
            wilds.push_back(c);
            continue;
        }
        if (isBlackThree(c) && !goingOut) {
            error = QStringLiteral("Black threes can only be melded on the turn you go out.");
            return false;
        }
        auto it = std::find_if(naturals.begin(), naturals.end(),
                               [&](const auto& e) { return e.first == c.rank; });
        if (it == naturals.end())
            naturals.push_back({ c.rank, { c } });
        else
            it->second.push_back(c);
    }

    // Where the wild cards go. Named explicitly if the caller said so;
    // otherwise the one natural rank on the table, if there is exactly one.
    int wildRank = targetRank;
    if (!wilds.empty() && wildRank < 0) {
        if (naturals.size() == 1) {
            wildRank = naturals.front().first;
        } else if (naturals.empty()) {
            if (!m_rules.wildCardMeldsAllowed) {
                error = QStringLiteral("Say which meld the wild card is joining.");
                return false;
            }
            out.push_back(Meld { kJoker, wilds });
            return true;
        } else {
            // Two ranks and a wild card: there is no way to tell which it
            // belongs to, and guessing wrong silently is worse than asking.
            error = QStringLiteral("Lay one rank at a time when you are using a wild card.");
            return false;
        }
    }

    if (naturals.empty() && wilds.empty()) {
        error = QStringLiteral("Nothing selected.");
        return false;
    }
    if (!wilds.empty() && wildRank == 3) {
        error = QStringLiteral("Black threes never take a wild card.");
        return false;
    }

    bool placedWilds = false;
    for (auto& entry : naturals) {
        Meld m { entry.first, entry.second };
        if (!wilds.empty() && entry.first == wildRank) {
            m.cards.insert(m.cards.end(), wilds.begin(), wilds.end());
            placedWilds = true;
        }
        out.push_back(std::move(m));
    }
    // Wild cards joining a meld already on the table bring no natural of their
    // own, so they form a group by themselves and merge on commit.
    if (!wilds.empty() && !placedWilds)
        out.push_back(Meld { wildRank, wilds });
    return true;
}

bool Engine::validateGroups(int team, const std::vector<Meld>& groups, bool goingOut,
                            QString& error) const
{
    const Team& t = m_teams[std::size_t(team)];
    for (const Meld& g : groups) {
        Meld merged { g.rank, {} };
        if (const Meld* existing = t.meldOfRank(g.rank))
            merged.cards = existing->cards;
        merged.cards.insert(merged.cards.end(), g.cards.begin(), g.cards.end());

        if (g.rank == 3) {
            if (!goingOut) {
                error = QStringLiteral("Black threes can only be melded on the turn you go out.");
                return false;
            }
            if (merged.wilds() > 0) {
                error = QStringLiteral("Black threes never take a wild card.");
                return false;
            }
        }

        if (merged.size() < m_rules.minMeldSize) {
            error = QStringLiteral("A meld needs at least %1 cards; %2 has %3.")
                        .arg(m_rules.minMeldSize)
                        .arg(rankLabel(g.rank))
                        .arg(merged.size());
            return false;
        }
        if (merged.wilds() > m_rules.maxWildsPerMeld) {
            error = QStringLiteral("At most %1 wild cards in one meld.").arg(m_rules.maxWildsPerMeld);
            return false;
        }
        // A wild-only meld has no naturals by definition, so the floor below
        // applies only to ordinary melds.
        if (g.rank != kJoker && merged.naturals() < m_rules.minNaturalsPerMeld) {
            error = QStringLiteral("A meld needs at least %1 real cards.")
                        .arg(m_rules.minNaturalsPerMeld);
            return false;
        }
    }
    return true;
}

bool Engine::teamWouldHaveCanasta(int team, const std::vector<Meld>& groups) const
{
    Team copy = m_teams[std::size_t(team)];
    applyGroups(copy, groups);
    return copy.hasCanasta(m_rules);
}

void Engine::commit(int team, const std::vector<Meld>& groups)
{
    applyGroups(m_teams[std::size_t(team)], groups);
}

bool Engine::validateMeld(const std::vector<Card>& cards, int targetRank,
                          std::vector<Meld>& groups, QString& error) const
{
    const int seat = m_current;
    const int t = teamOf(seat);

    if (!handContains(seat, cards)) {
        error = QStringLiteral("Those cards are not in your hand.");
        return false;
    }
    const bool goingOut = m_hands[std::size_t(seat)].size() == cards.size();
    if (!group(cards, goingOut, targetRank, groups, error))
        return false;
    if (!validateGroups(t, groups, goingOut, error))
        return false;

    if (!m_teams[std::size_t(t)].opened) {
        const int need = m_openReq[std::size_t(t)];
        const int have = layDownValue(cards);
        if (have < need) {
            error = QStringLiteral("Your side needs %1 to open, and that is only %2.")
                        .arg(need)
                        .arg(have);
            return false;
        }
    }
    if (!keepsADiscard(t, m_hands[std::size_t(seat)].size() - cards.size(), groups, error))
        return false;
    return true;
}

bool Engine::validateTake(const std::vector<Card>& layDown, std::vector<Meld>& groups,
                          QString& error) const
{
    if (m_pile.empty()) {
        error = QStringLiteral("The discard pile is empty.");
        return false;
    }
    const Card top = m_pile.back();
    if (isWild(top)) {
        error = QStringLiteral("A wild card on top stops the pile being taken.");
        return false;
    }
    if (m_rules.blackThreeBlocksPile && isBlackThree(top)) {
        error = QStringLiteral("A black three on top stops the pile being taken.");
        return false;
    }

    const int seat = m_current;
    const int t = teamOf(seat);
    if (!handContains(seat, layDown)) {
        error = QStringLiteral("Those cards are not in your hand.");
        return false;
    }

    int naturalsOfTop = 0;
    int wildsOffered = 0;
    for (const Card& c : layDown) {
        if (isWild(c))
            ++wildsOffered;
        else if (c.rank == top.rank)
            ++naturalsOfTop;
    }

    // A side that has not opened is in the same position as a frozen pile: it
    // has to hold two matching cards of its own.
    const Team& team = m_teams[std::size_t(t)];
    const bool mustUseTwoNaturals = m_frozen || !team.opened;

    if (mustUseTwoNaturals) {
        if (naturalsOfTop < 2) {
            error = m_frozen
                ? QStringLiteral("The pile is frozen: you need two matching cards from your hand.")
                : QStringLiteral("Until your side has opened, you need two matching cards from your hand.");
            return false;
        }
    } else {
        const bool byExtending = team.meldOfRank(top.rank) != nullptr
            && m_rules.unfrozenPileTakeableByExtending;
        const bool byPair = naturalsOfTop >= 2;
        const bool byWild = m_rules.unfrozenPileTakeableWithWild && naturalsOfTop >= 1
            && wildsOffered >= 1;
        if (!byExtending && !byPair && !byWild) {
            error = QStringLiteral("You have no way to use the %1 on top.").arg(rankLabel(top.rank));
            return false;
        }
    }

    // Taking the pile empties your hand only when you lay all of it down and
    // the pile is the single top card.
    const bool goingOut = m_hands[std::size_t(seat)].size() == layDown.size() && m_pile.size() == 1;

    std::vector<Card> combined = layDown;
    combined.push_back(top);
    // Wild cards laid down with a pile take belong to the top card's rank —
    // that is the whole point of the move, so it needs no separate answer.
    if (!group(combined, goingOut, top.rank, groups, error))
        return false;

    // The top card must actually be used; grouping puts it wherever its rank
    // went, so its group existing is the check.
    if (std::none_of(groups.begin(), groups.end(),
                     [&](const Meld& m) { return m.rank == top.rank; })) {
        error = QStringLiteral("The top card has to be melded.");
        return false;
    }
    if (!validateGroups(t, groups, goingOut, error))
        return false;

    if (!team.opened) {
        const int need = m_openReq[std::size_t(t)];
        // Only the top card counts from the pile; the cards under it do not.
        const int have = layDownValue(combined);
        if (have < need) {
            error = QStringLiteral("Your side needs %1 to open, and that is only %2.")
                        .arg(need)
                        .arg(have);
            return false;
        }
    }
    const std::size_t after = m_hands[std::size_t(seat)].size() - layDown.size()
        + m_pile.size() - 1;
    if (!keepsADiscard(t, after, groups, error))
        return false;
    return true;
}

// The rules leave one shape of position unplayable: down to a single card with
// no canasta, you may not go out, and discarding that card would be going out.
// Nothing legal remains, and the turn cannot be finished. So the lay-down that
// would get you there is what has to be refused.
bool Engine::keepsADiscard(int team, std::size_t handAfter, const std::vector<Meld>& groups,
                           QString& error) const
{
    if (!m_rules.requireCanastaToGoOut)
        return true;
    if (handAfter >= 2)
        return true;
    if (teamWouldHaveCanasta(team, groups))
        return true;

    error = handAfter == 0
        ? QStringLiteral("Your side needs a canasta before anyone can go out.")
        : QStringLiteral("Without a canasta you have to keep a card to discard.");
    return false;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

bool Engine::canDrawFromStock() const
{
    return m_phase == Phase::Draw && !m_stock.empty();
}

bool Engine::drawFromStock()
{
    if (m_phase != Phase::Draw)
        return fail(QStringLiteral("It is not time to draw."));
    if (m_stock.empty())
        return fail(QStringLiteral("The stock is empty."));

    m_error.clear();
    m_hands[std::size_t(m_current)].push_back(m_stock.back());
    m_stock.pop_back();

    // A red three drawn goes down at once and is replaced. Running out of
    // stock while replacing ends the hand.
    if (!placeRedThrees(m_current, true)) {
        scoreHand();
        return true;
    }
    m_phase = Phase::Play;
    return true;
}

bool Engine::canTakePile(const std::vector<Card>& layDown) const
{
    if (m_phase != Phase::Draw)
        return false;
    std::vector<Meld> groups;
    QString error;
    return validateTake(layDown, groups, error);
}

bool Engine::canTakePileAtAll() const
{
    std::vector<Card> ignored;
    return findPileTake(ignored);
}

bool Engine::findPileTake(std::vector<Card>& out) const
{
    out.clear();
    if (m_pile.empty())
        return false;

    const int seat = m_current;
    const std::vector<Card>& h = m_hands[std::size_t(seat)];
    const Card top = m_pile.back();

    std::vector<Card> matching;
    std::vector<Card> wilds;
    for (const Card& c : h) {
        if (isWild(c))
            wilds.push_back(c);
        else if (c.rank == top.rank)
            matching.push_back(c);
    }
    // Cheapest wild first, so a joker is not spent where a two would do.
    std::sort(wilds.begin(), wilds.end(), [&](const Card& a, const Card& b) {
        return cardValue(a, m_rules) < cardValue(b, m_rules);
    });

    std::vector<std::vector<Card>> candidates;
    if (matching.size() >= 2)
        candidates.push_back(matching);
    if (!matching.empty() && !wilds.empty())
        candidates.push_back({ matching.front(), wilds.front() });
    candidates.push_back({});

    std::vector<Meld> groups;
    QString error;
    for (const std::vector<Card>& base : candidates) {
        if (validateTake(base, groups, error)) {
            out = base;
            return true;
        }
        // The usual reason a legal-looking take fails is the opening minimum.
        // Laying whole melds down alongside it is how a real player makes up
        // the difference, so try that before giving up.
        std::vector<Card> augmented = base;
        for (int rank = kAce; rank <= kKing; ++rank) {
            if (rank == 3 || rank == 2 || rank == top.rank)
                continue;
            std::vector<Card> same;
            for (const Card& c : h) {
                if (c.rank == rank
                    && std::find(augmented.begin(), augmented.end(), c) == augmented.end())
                    same.push_back(c);
            }
            if (int(same.size()) < m_rules.minMeldSize)
                continue;
            augmented.insert(augmented.end(), same.begin(), same.end());
            if (validateTake(augmented, groups, error)) {
                out = augmented;
                return true;
            }
        }
    }
    return false;
}

bool Engine::takePile(const std::vector<Card>& layDown)
{
    if (m_phase != Phase::Draw)
        return fail(QStringLiteral("It is not time to draw."));

    std::vector<Meld> groups;
    QString error;
    if (!validateTake(layDown, groups, error))
        return fail(error);

    m_error.clear();
    const int seat = m_current;
    const int t = teamOf(seat);

    removeFromHand(seat, layDown);

    std::vector<Card> taken = m_pile;
    m_pile.clear();
    m_frozen = false;
    taken.pop_back(); // the top card is in the melds, not the hand

    commit(t, groups);
    m_teams[std::size_t(t)].opened = true;
    m_hasMelded[std::size_t(seat)] = true;

    std::vector<Card>& h = m_hands[std::size_t(seat)];
    h.insert(h.end(), taken.begin(), taken.end());
    // A red three buried in the pile goes down, but earns no replacement — the
    // replacement is a draw, and this was not one.
    placeRedThrees(seat, false);

    if (h.empty()) {
        goOut(seat);
        return true;
    }
    m_phase = Phase::Play;
    return true;
}

bool Engine::canMeldCards(const std::vector<Card>& cards, int targetRank) const
{
    if (m_phase != Phase::Play)
        return false;
    std::vector<Meld> groups;
    QString error;
    return validateMeld(cards, targetRank, groups, error);
}

bool Engine::meldCards(const std::vector<Card>& cards, int targetRank)
{
    if (m_phase != Phase::Play)
        return fail(QStringLiteral("Draw before you lay anything down."));

    std::vector<Meld> groups;
    QString error;
    if (!validateMeld(cards, targetRank, groups, error))
        return fail(error);

    m_error.clear();
    const int seat = m_current;
    const int t = teamOf(seat);

    removeFromHand(seat, cards);
    commit(t, groups);
    m_teams[std::size_t(t)].opened = true;
    m_hasMelded[std::size_t(seat)] = true;

    if (m_hands[std::size_t(seat)].empty())
        goOut(seat);
    return true;
}

bool Engine::canDiscard(const Card& c) const
{
    if (m_phase != Phase::Play)
        return false;
    if (!handContains(m_current, { c }))
        return false;
    if (isRedThree(c))
        return false;
    if (m_hands[std::size_t(m_current)].size() == 1 && m_rules.requireCanastaToGoOut
        && !m_teams[std::size_t(teamOf(m_current))].hasCanasta(m_rules))
        return false;
    return true;
}

bool Engine::discard(const Card& c)
{
    if (m_phase != Phase::Play)
        return fail(QStringLiteral("Draw before you discard."));
    const int seat = m_current;
    if (!handContains(seat, { c }))
        return fail(QStringLiteral("That card is not in your hand."));
    if (isRedThree(c))
        return fail(QStringLiteral("A red three is never discarded."));

    const bool goingOut = m_hands[std::size_t(seat)].size() == 1;
    if (goingOut && m_rules.requireCanastaToGoOut
        && !m_teams[std::size_t(teamOf(seat))].hasCanasta(m_rules))
        return fail(QStringLiteral("Your side needs a canasta before you can go out."));

    m_error.clear();
    removeFromHand(seat, { c });
    m_pile.push_back(c);
    // A wild card thrown on the pile freezes it for everyone until it is taken.
    if (isWild(c))
        m_frozen = true;

    if (goingOut) {
        goOut(seat);
        return true;
    }
    endTurn();
    return true;
}

void Engine::goOut(int seat)
{
    m_outSeat = seat;
    m_outConcealed = !m_meldedBeforeTurn;
    scoreHand();
}

std::vector<int> Engine::meldableRanks(int seat) const
{
    // Ranks that could form or extend a meld, ignoring the opening minimum —
    // the status line carries that, and dimming a whole hand because the total
    // is four points short reads as a bug rather than a rule.
    const Team& t = m_teams[std::size_t(teamOf(seat))];
    const std::vector<Card>& h = m_hands[std::size_t(seat)];

    int wilds = 0;
    for (const Card& c : h)
        if (isWild(c))
            ++wilds;

    std::vector<int> out;
    for (int rank = kAce; rank <= kKing; ++rank) {
        if (rank == 3 || rank == 2)
            continue;
        const int count = int(std::count_if(h.begin(), h.end(),
                                            [&](const Card& c) { return c.rank == rank; }));
        if (count == 0)
            continue;
        const Meld* existing = t.meldOfRank(rank);
        if (existing != nullptr && t.opened) {
            out.push_back(rank);
            continue;
        }
        if (count >= m_rules.minMeldSize
            || (count >= m_rules.minNaturalsPerMeld && count + wilds >= m_rules.minMeldSize))
            out.push_back(rank);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Scoring
// ---------------------------------------------------------------------------

void Engine::scoreHand()
{
    for (int t = 0; t < kTeams; ++t) {
        Team& team = m_teams[std::size_t(t)];
        const bool wentOut = m_outSeat >= 0 && teamOf(m_outSeat) == t;
        team.handScore = handScoreFor(team, m_hands[std::size_t(t)],
                                      m_hands[std::size_t(t + 2)], wentOut, m_outConcealed,
                                      m_rules);
        team.score += team.handScore;
    }

    const bool reached = m_teams[0].score >= m_rules.targetScore
        || m_teams[1].score >= m_rules.targetScore;
    // A tie on or over the target plays another hand rather than declaring a
    // joint winner.
    m_phase = (reached && m_teams[0].score != m_teams[1].score) ? Phase::GameOver
                                                                : Phase::HandOver;
}

int Engine::cardsInPlay() const
{
    int n = int(m_stock.size()) + int(m_pile.size());
    for (const auto& h : m_hands)
        n += int(h.size());
    for (const Team& t : m_teams) {
        n += int(t.redThrees.size());
        for (const Meld& m : t.melds)
            n += m.size();
    }
    return n;
}

} // namespace canasta
