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
        if (it == t.melds.end())
            it = t.melds.insert(t.melds.end(), Meld { g.rank, {} });
        it->cards.insert(it->cards.end(), g.cards.begin(), g.cards.end());
        // Wild cards to the front. Nothing in the rules depends on the order,
        // but a meld has to say what it is made of at a glance, and a joker
        // buried between two sixes is exactly what is hard to see.
        std::stable_sort(it->cards.begin(), it->cards.end(), sortsBefore);
    }
}

// Position in the fan. Low sorts left. Jokers and twos lead because they are
// the wild cards and a player keeps them together; the rest run downward from
// the ace, which is what "sorted by value" means at a Canasta table.
int fanOrder(const Card& c)
{
    if (isJoker(c)) return 0;
    if (c.rank == 2) return 1;
    if (c.rank == kAce) return 2;
    return 16 - c.rank; // K, Q, J, 10 … 4, 3
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

bool sortsBefore(const Card& a, const Card& b)
{
    const int ka = fanOrder(a);
    const int kb = fanOrder(b);
    if (ka != kb)
        return ka < kb;
    return int(a.suit) < int(b.suit);
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
    // Whether this side's cards count for it or against it. Classic asks only
    // that it opened; the house rule asks for a canasta, and a side that never
    // made one has its melds subtracted like the cards still in its hands.
    const bool counts = !rules.canastaNeededToScore || t.hasCanasta(rules);

    int s = 0;
    for (const Meld& m : t.melds) {
        s += counts ? m.value(rules) : -m.value(rules);
        if (m.isCanasta(rules))
            s += m.isNatural(rules) ? rules.naturalCanastaBonus : rules.mixedCanastaBonus;
    }

    // Red threes swing both ways: a bonus to a side whose cards count, and the
    // same figure against a side whose do not.
    const int n = int(t.redThrees.size());
    const int red = n == 4 ? rules.allRedThreesValue : n * rules.redThreeValue;
    s += (counts && t.opened) ? red : -red;

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

bool caughtAMinus(const Team& t, const Rules& rules)
{
    // Exactly the condition handScoreFor turns a side's own cards around on,
    // named. Kept as one expression in one place so the two can never drift:
    // a message that says "caught a minus" while the score says otherwise is
    // worse than no message.
    return rules.canastaNeededToScore && !t.hasCanasta(rules);
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

void Engine::applyRules(Rules r)
{
    m_pendingRules = r;

    // The deal is already on the table, so the numbers that made it stand.
    r.decks = m_rules.decks;
    r.jokers = m_rules.jokers;
    r.handSize = m_rules.handSize;
    m_rules = std::move(r);
    // The opening minimums are deliberately NOT recomputed: each side's is
    // fixed when the hand is dealt, and moving it mid-hand would change what a
    // player is playing towards after they had started.
}

void Engine::nextHand()
{
    if (m_phase != Phase::HandOver)
        return;
    // THIS is the next hand, so this is where the three numbers applyRules had
    // to pin -- pack size, jokers and hand size -- finally take effect. They
    // cannot change under cards already dealt, which is why applyRules carries
    // them on as dealt; but only newGame() consumed m_pendingRules, so in
    // practice they waited for a whole new GAME. The header promises the next
    // hand, and promises it so that nobody has to abandon a game to correct a
    // house rule. m_rules differs from m_pendingRules in those three alone,
    // applyRules having already applied everything else at once, so the whole
    // assignment is the same edit and mirrors newGame().
    m_rules = m_pendingRules;
    m_dealer = (m_dealer + 1) % kSeats;
    deal();
}

// ---------------------------------------------------------------------------
// Saving a game in progress
// ---------------------------------------------------------------------------

namespace {

// Bumped whenever the shape below changes. An older save is then refused
// rather than misread, and the player gets a fresh game instead of a wrong one.
constexpr quint32 kSaveVersion = 1;

void writeCard(QDataStream& out, const Card& c)
{
    out << qint8(c.suit) << qint16(c.rank) << c.faceUp << qint8(c.deck);
}

Card readCard(QDataStream& in)
{
    qint8 suit = 0;
    qint16 rank = 0;
    bool faceUp = false;
    qint8 deck = 0;
    in >> suit >> rank >> faceUp >> deck;
    // The same bounds cardcodec::readCard applies. Canasta keeps its own reader
    // because the format differs -- a wider rank field, so the two are not
    // interchangeable without breaking every stored game -- but it validated
    // NOTHING, on the one input SECURITY.md names as untrusted. A rank outside
    // the pack is then consumed by every rank-keyed lookup in the engine and
    // the AI.
    if (suit < 0 || suit > 3 || rank < kJoker || rank > kKing || deck < 0 || deck > 7)
        in.setStatus(QDataStream::ReadCorruptData);
    return Card { Suit(suit), int(rank), faceUp, int(deck) };
}

void writeCards(QDataStream& out, const std::vector<Card>& cards)
{
    out << qint32(cards.size());
    for (const Card& c : cards)
        writeCard(out, c);
}

std::vector<Card> readCards(QDataStream& in)
{
    qint32 n = 0;
    in >> n;
    std::vector<Card> cards;
    // A count from a corrupt file must not be trusted into an allocation; two
    // packs plus jokers is 108, and nothing here can legitimately exceed that.
    // Truncating at the ceiling and carrying on left the stream DESYNCHRONISED
    // and the load then survived or failed by luck of the downstream count
    // check, so an impossible count is corruption rather than a short read.
    if (n < 0 || n > 512) {
        in.setStatus(QDataStream::ReadCorruptData);
        return cards;
    }
    cards.reserve(std::size_t(n));
    for (qint32 i = 0; i < n && in.status() == QDataStream::Ok; ++i)
        cards.push_back(readCard(in));
    return cards;
}

void writeRules(QDataStream& out, const Rules& r)
{
    out << r.name << qint32(r.targetScore) << qint32(r.handSize) << qint32(r.decks)
        << qint32(r.jokers) << qint32(r.canastaSize) << qint32(r.minMeldSize)
        << qint32(r.maxWildsPerMeld) << qint32(r.minNaturalsPerMeld) << qint32(r.openMinBelowZero)
        << qint32(r.openMinUnder1500) << qint32(r.openMinUnder3000) << qint32(r.openMinAbove3000)
        << qint32(r.redThreeValue) << qint32(r.allRedThreesValue) << qint32(r.naturalCanastaBonus)
        << qint32(r.mixedCanastaBonus) << qint32(r.goingOutBonus) << qint32(r.concealedGoingOutBonus)
        << qint32(r.jokerValue) << qint32(r.wildTwoValue) << qint32(r.aceValue)
        << qint32(r.highCardValue) << qint32(r.lowCardValue) << qint32(r.blackThreeValue)
        << r.requireCanastaToGoOut << r.blackThreeBlocksPile << r.wildCardMeldsAllowed
        << r.unfrozenPileTakeableWithWild << r.unfrozenPileTakeableByExtending
        << r.wildsFewerThanNaturals << r.canastaMakesRankSafe << r.noMeldingFirstRound
        << r.pileMeldCountsToOpen;
}

Rules readRules(QDataStream& in)
{
    Rules r;
    qint32 v = 0;
    const auto num = [&](int& field) {
        in >> v;
        field = int(v);
    };
    in >> r.name;
    num(r.targetScore);
    num(r.handSize);
    num(r.decks);
    num(r.jokers);
    num(r.canastaSize);
    num(r.minMeldSize);
    num(r.maxWildsPerMeld);
    num(r.minNaturalsPerMeld);
    num(r.openMinBelowZero);
    num(r.openMinUnder1500);
    num(r.openMinUnder3000);
    num(r.openMinAbove3000);
    num(r.redThreeValue);
    num(r.allRedThreesValue);
    num(r.naturalCanastaBonus);
    num(r.mixedCanastaBonus);
    num(r.goingOutBonus);
    num(r.concealedGoingOutBonus);
    num(r.jokerValue);
    num(r.wildTwoValue);
    num(r.aceValue);
    num(r.highCardValue);
    num(r.lowCardValue);
    num(r.blackThreeValue);
    in >> r.requireCanastaToGoOut >> r.blackThreeBlocksPile >> r.wildCardMeldsAllowed
        >> r.unfrozenPileTakeableWithWild >> r.unfrozenPileTakeableByExtending
        >> r.wildsFewerThanNaturals >> r.canastaMakesRankSafe >> r.noMeldingFirstRound
        >> r.pileMeldCountsToOpen;
    return r;
}

// A pack no deal could have produced. `decks` and `jokers` are read straight
// off an untrusted stream and are then multiplied out to size the pack that
// load()'s wholeness check compares against -- so a hostile figure overflows
// that multiply, which is undefined behaviour, before anything gets the chance
// to refuse it. Bounding them here rather than widening the multiply is the
// honest fix: a save claiming 905,969,666 decks is not a rounding problem, it
// is not a Canasta game. The house rules dialog offers two decks; eight is
// already far past anything anyone plays.
//
// Found by GHUB-0052's fuzzer under UndefinedBehaviorSanitizer. The same
// mutant scored a clean pass in an ordinary build, which is exactly why that
// bullet insisted the harness be run under sanitizers.
bool plausiblePack(const Rules& r)
{
    // handSize is bounded here because dealFrom() pops kSeats * handSize cards
    // off the stock without checking it holds them: a saved blob claiming a
    // large hand walks an empty vector. The dialog clamps 7..15; this is the
    // path a hand-edited or corrupt file takes, and it has no clamp of its own.
    return r.decks >= 1 && r.decks <= 8 && r.jokers >= 0 && r.jokers <= 64
        && r.handSize >= 1 && r.handSize <= 20;
}

void writeTeam(QDataStream& out, const Team& t)
{
    out << qint32(t.melds.size());
    for (const Meld& m : t.melds) {
        out << qint32(m.rank);
        writeCards(out, m.cards);
    }
    writeCards(out, t.redThrees);
    out << qint32(t.score) << qint32(t.handScore) << t.opened;
}

Team readTeam(QDataStream& in)
{
    Team t;
    qint32 n = 0;
    in >> n;
    for (qint32 i = 0; i < n && i < 32 && in.status() == QDataStream::Ok; ++i) {
        qint32 rank = 0;
        in >> rank;
        t.melds.push_back(Meld { int(rank), readCards(in) });
    }
    t.redThrees = readCards(in);
    qint32 score = 0;
    qint32 handScore = 0;
    in >> score >> handScore >> t.opened;
    t.score = int(score);
    t.handScore = int(handScore);
    return t;
}

} // namespace

void Engine::save(QDataStream& out) const
{
    out << kSaveVersion;
    writeRules(out, m_rules);
    writeRules(out, m_pendingRules);
    for (const std::vector<Card>& h : m_hands)
        writeCards(out, h);
    for (const Team& t : m_teams)
        writeTeam(out, t);
    for (const int req : m_openReq)
        out << qint32(req);
    for (const bool melded : m_hasMelded)
        out << melded;
    out << m_meldedBeforeTurn;
    writeCards(out, m_stock);
    writeCards(out, m_pile);
    out << qint32(m_phase) << qint32(m_hand) << qint32(m_dealer) << qint32(m_current)
        << qint32(m_turnsTaken) << qint32(m_outSeat) << m_outConcealed << m_frozen;

    // Rules added after the format was first written go on the end rather than
    // in with the others, so a game saved before they existed still loads and
    // simply comes back without them. `tail` in load() says how many of these
    // the stream carries.
    out << m_rules.canastaNeededToScore << m_pendingRules.canastaNeededToScore;   // tail 1
    out << m_rules.pileFrozenUntilOpened << m_pendingRules.pileFrozenUntilOpened; // tail 2
    out << m_rules.deadHandIfNobodyGoesOut
        << m_pendingRules.deadHandIfNobodyGoesOut; // tail 3
    out << m_rules.goingOutNeedsADiscard << m_pendingRules.goingOutNeedsADiscard; // tail 4
    out << m_rules.bothReachingTargetIsADraw
        << m_pendingRules.bothReachingTargetIsADraw; // tail 5
    // tail 6: who threw each card of the pile. Not a rule pair -- see kTail.
    out << qint32(m_pileFrom.size());
    for (const qint8 seat : m_pileFrom)
        out << seat;
    out << m_rules.freezeCardMakesATee << m_pendingRules.freezeCardMakesATee; // tail 7
    out << m_rules.canastasStackOnRedThrees
        << m_pendingRules.canastasStackOnRedThrees; // tail 8
}

bool Engine::load(QDataStream& in, int tail)
{
    quint32 version = 0;
    in >> version;
    if (version != kSaveVersion)
        return false;

    // Read into a copy, so a stream that runs out part way leaves the game that
    // is already on the table alone.
    Engine e;
    e.m_rules = readRules(in);
    e.m_pendingRules = readRules(in);
    // Before any arithmetic touches them -- see plausiblePack.
    if (!plausiblePack(e.m_rules) || !plausiblePack(e.m_pendingRules))
        return false;
    for (std::vector<Card>& h : e.m_hands)
        h = readCards(in);
    for (Team& t : e.m_teams)
        t = readTeam(in);
    for (int& req : e.m_openReq) {
        qint32 v = 0;
        in >> v;
        req = int(v);
    }
    for (std::size_t i = 0; i < e.m_hasMelded.size(); ++i) {
        bool melded = false;
        in >> melded;
        e.m_hasMelded[i] = melded;
    }
    in >> e.m_meldedBeforeTurn;
    e.m_stock = readCards(in);
    e.m_pile = readCards(in);
    qint32 phase = 0;
    qint32 hand = 0;
    qint32 dealer = 0;
    qint32 current = 0;
    qint32 turns = 0;
    qint32 outSeat = 0;
    in >> phase >> hand >> dealer >> current >> turns >> outSeat >> e.m_outConcealed >> e.m_frozen;
    if (in.status() != QDataStream::Ok)
        return false;
    if (phase < 0 || phase > qint32(Phase::GameOver) || current < 0 || current >= kSeats
        || dealer < 0 || dealer >= kSeats)
        return false;

    e.m_phase = Phase(phase);
    e.m_hand = int(hand);
    e.m_dealer = int(dealer);
    e.m_current = int(current);
    e.m_turnsTaken = int(turns);
    e.m_outSeat = int(outSeat);

    // The tail: rules added after this format was first written. How much of it
    // is there is the caller's to know — atEnd() cannot answer it, because a
    // caller may have written its own fields after ours.
    const auto readPair = [&](bool& inPlay, bool& pending) {
        bool a = false;
        bool b = false;
        in >> a >> b;
        inPlay = a;
        pending = b;
    };
    if (tail >= 1)
        readPair(e.m_rules.canastaNeededToScore, e.m_pendingRules.canastaNeededToScore);
    if (tail >= 2)
        readPair(e.m_rules.pileFrozenUntilOpened, e.m_pendingRules.pileFrozenUntilOpened);
    if (tail >= 3)
        readPair(e.m_rules.deadHandIfNobodyGoesOut, e.m_pendingRules.deadHandIfNobodyGoesOut);
    if (tail >= 4)
        readPair(e.m_rules.goingOutNeedsADiscard, e.m_pendingRules.goingOutNeedsADiscard);
    if (tail >= 5)
        readPair(e.m_rules.bothReachingTargetIsADraw,
                 e.m_pendingRules.bothReachingTargetIsADraw);
    if (tail >= 6) {
        qint32 count = 0;
        in >> count;
        if (in.status() != QDataStream::Ok || count < 0
            || count != qint32(e.m_pile.size()))
            return false;
        e.m_pileFrom.clear();
        e.m_pileFrom.reserve(std::size_t(count));
        for (qint32 i = 0; i < count; ++i) {
            qint8 seat = 0;
            in >> seat;
            // -1 is "nobody threw it"; anything outside the table is a blob
            // saying something the rules could not have produced.
            if (seat < -1 || seat >= kSeats)
                return false;
            e.m_pileFrom.push_back(seat);
        }
    }
    if (tail >= 7)
        readPair(e.m_rules.freezeCardMakesATee, e.m_pendingRules.freezeCardMakesATee);
    if (tail >= 8)
        readPair(e.m_rules.canastasStackOnRedThrees,
                 e.m_pendingRules.canastasStackOnRedThrees);
    if (in.status() != QDataStream::Ok)
        return false;

    // A stream from before the provenance existed comes back with none. The
    // pile is real and the game plays on; the seats are simply unknown, which
    // reads as "nobody threw it" and turns the fishing defence off for the
    // rest of the hand.
    if (e.m_pileFrom.size() != e.m_pile.size())
        e.m_pileFrom.assign(e.m_pile.size(), qint8(-1));

    // The pack has to still be whole, which is the one check that catches a
    // file that parsed cleanly but says something impossible.
    if (e.cardsInPlay() != e.m_rules.decks * 52 + e.m_rules.jokers)
        return false;

    const std::mt19937 rng = m_rng; // keep this engine's own shuffle sequence
    *this = e;
    m_rng = rng;
    return true;
}

int Engine::pileThrownBy(int index) const
{
    if (index < 0 || index >= int(m_pileFrom.size()))
        return -1;
    return int(m_pileFrom[std::size_t(index)]);
}

int Engine::pileRankSources(int rank, int exceptSeat) const
{
    std::array<bool, kSeats> seats {};
    bool unthrown = false;
    for (int i = 0; i < int(m_pile.size()); ++i) {
        if (m_pile[std::size_t(i)].rank != rank)
            continue;
        const int seat = pileThrownBy(i);
        // Guarded on exceptSeat >= 0 so the default (-1, count everybody) does
        // not collide with the -1 that means "nobody threw it".
        if (exceptSeat >= 0 && seat == exceptSeat)
            continue;
        if (seat < 0 || seat >= kSeats) {
            // The deal's up-card and whatever covered it. Nobody chose to let
            // it go, so it is not a preference -- but it IS a card that came
            // out of the stock rather than a hand, so it counts once. A pile
            // restored from a save with no provenance is entirely this, which
            // is what makes such a hand read cautiously rather than wrongly.
            unthrown = true;
            continue;
        }
        seats[std::size_t(seat)] = true;
    }
    return int(std::count(seats.begin(), seats.end(), true)) + (unthrown ? 1 : 0);
}

void Engine::sortHand(int seat)
{
    if (seat < 0 || seat >= kSeats)
        return;
    std::vector<Card>& h = m_hands[std::size_t(seat)];
    std::stable_sort(h.begin(), h.end(), sortsBefore);
}

int Engine::winner() const
{
    if (m_phase != Phase::GameOver)
        return -1;
    // Under the house rule the game is won by REACHING the target, so a hand
    // that carried both sides past it was won by neither. Recomputed from the
    // scores rather than remembered, so a loaded game answers the same way a
    // played one does.
    if (m_rules.bothReachingTargetIsADraw && m_teams[0].score >= m_rules.targetScore
        && m_teams[1].score >= m_rules.targetScore)
        return kDraw;
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
    m_pileFrom.clear();
    m_error.clear();
    m_hasMelded.fill(false);
    m_turnsTaken = 0;
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
    // plausiblePack bounds handSize on the way in, so this cannot fire from a
    // save; it is here because the pop below has no bound of its own and a
    // caller supplying its own stock (newGameFromStock) can be short.
    if (int(m_stock.size()) < kSeats * m_rules.handSize)
        return;

    const int first = (m_dealer + 1) % kSeats;
    for (int i = 0; i < m_rules.handSize; ++i) {
        for (int s = 0; s < kSeats; ++s) {
            m_hands[std::size_t((first + s) % kSeats)].push_back(m_stock.back());
            m_stock.pop_back();
        }
    }

    // Red threes dealt into a hand go straight down, each replaced from stock.
    // A false answer means the stock ran dry while replacing, which is what
    // drawFromStock reads as the hand being over -- so the deal stops here, the
    // same way the short-stock guard above stops it. The bound above covers the
    // hands and not the replacements, and a caller supplying its own stock
    // (newGameFromStock, which the tests use to build exact positions) can hand
    // over exactly enough to deal and nothing spare. A real 108-card pack has
    // four red threes and cannot reach this.
    for (int s = 0; s < kSeats; ++s) {
        if (!placeRedThrees((first + s) % kSeats, true))
            return;
    }

    // The up-card starts the pile. A wild or a red three under it freezes the
    // pile, and another card is turned to cover it.
    while (!m_stock.empty()) {
        const Card c = m_stock.back();
        m_stock.pop_back();
        m_pile.push_back(c);
        m_pileFrom.push_back(-1);   // turned by the deal, not thrown by anyone
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
    ++m_turnsTaken;
    advanceSeat();
    startTurn();
}

bool Engine::meldingAllowed() const
{
    return !m_rules.noMeldingFirstRound || m_turnsTaken >= kSeats;
}

bool Engine::discardCannotBeTaken() const
{
    // m_turnsTaken counts turns already finished, so the seat playing now is
    // turn m_turnsTaken and the seat after it is m_turnsTaken + 1. That next
    // seat is barred exactly while its own turn number is still inside the
    // round, which is why this is +1 and meldingAllowed() is not.
    return m_rules.noMeldingFirstRound && m_turnsTaken + 1 < kSeats;
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
                   int priorityRank,
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

    // Cheapest wild card first. It usually makes no difference — the same cards
    // go down either way and they are all the team's — but it decides a legal
    // move from an illegal one under the house rule where the meld that
    // captures the pile counts nothing toward opening: the expensive wild has
    // to end up in a meld that DOES count, and the cheap one goes on the pile
    // take. Spreading fills the pile's rank first, so ordering the list here is
    // what puts the joker where it earns its 50.
    std::stable_sort(wilds.begin(), wilds.end(), [this](const Card& a, const Card& b) {
        return cardValue(a, m_rules) < cardValue(b, m_rules);
    });

    // Where the wild cards go. Named explicitly if the caller said so;
    // otherwise the one natural rank on the table, if there is exactly one.
    int wildRank = targetRank;
    if (!wilds.empty() && wildRank < 0) {
        if (naturals.size() == 1) {
            wildRank = naturals.front().first;
        } else if (naturals.empty()) {
            if (!m_rules.wildCardMeldsAllowed) {
                error = QStringLiteral("Say which meld the joker is joining.");
                return false;
            }
            out.push_back(Meld { kJoker, wilds });
            return true;
        } else if (!spreadWilds(naturals, wilds, priorityRank, error)) {
            return false;
        } else {
            // Spread, and so already in the groups below.
            wilds.clear();
        }
    }

    if (naturals.empty() && wilds.empty()) {
        error = QStringLiteral("Nothing selected.");
        return false;
    }
    if (!wilds.empty() && wildRank == 3) {
        error = QStringLiteral("Black threes never take a joker.");
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

// Several ranks going down together with wild cards among them. This used to be
// refused as ambiguous, which was wrong in the one place it mattered most: the
// opening minimum has to be met in a single lay-down, so a player whose 90 was
// spread across two ranks each needing a wild had no legal way to open at all.
//
// Every legal spread comes to the same score — the cards are the same cards and
// they are all the team's — so the engine places them rather than asking. Each
// wild goes where it is most needed: the meld furthest from being a meld, and
// then the one holding the fewest wilds, so a pair of ranks gets one each
// rather than both landing on the first.
bool Engine::spreadWilds(std::vector<std::pair<int, std::vector<Card>>>& naturals,
                         const std::vector<Card>& wilds, int priorityRank, QString& error) const
{
    const Team& t = m_teams[std::size_t(teamOf(m_current))];

    for (const Card& w : wilds) {
        int best = -1;
        bool bestPriority = false;
        int bestDeficit = -1;
        int bestWilds = 0;

        for (std::size_t i = 0; i < naturals.size(); ++i) {
            if (naturals[i].first == 3)
                continue; // black threes never take a wild card

            Meld merged { naturals[i].first, {} };
            if (const Meld* existing = t.meldOfRank(naturals[i].first))
                merged.cards = existing->cards;
            merged.cards.insert(merged.cards.end(), naturals[i].second.begin(),
                                naturals[i].second.end());
            const int deficit = std::max(0, m_rules.minMeldSize - merged.size());
            const int had = merged.wilds();

            merged.cards.push_back(w);
            if (merged.wilds() > m_rules.maxWildsPerMeld)
                continue;
            if (m_rules.wildsFewerThanNaturals && merged.wilds() >= merged.naturals())
                continue;

            // Taking the pile depends on the top card's meld standing up, so
            // that rank gets first refusal on a wild whenever it is still
            // short. Everything else is decided by need, then by spread.
            const bool priority = naturals[i].first == priorityRank && deficit > 0;
            const bool better = best < 0
                ? true
                : priority != bestPriority ? priority
                : deficit != bestDeficit   ? deficit > bestDeficit
                                           : had < bestWilds;
            if (better) {
                best = int(i);
                bestPriority = priority;
                bestDeficit = deficit;
                bestWilds = had;
            }
        }

        if (best < 0) {
            error = QStringLiteral("Nothing in that lay-down can take another joker.");
            return false;
        }
        naturals[std::size_t(best)].second.push_back(w);
    }
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
                error = QStringLiteral("Black threes never take a joker.");
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
            error = QStringLiteral("At most %1 jokers in one meld.").arg(m_rules.maxWildsPerMeld);
            return false;
        }
        // A wild-only meld has no naturals by definition, so the floor below
        // applies only to ordinary melds.
        if (g.rank != kJoker && merged.naturals() < m_rules.minNaturalsPerMeld) {
            error = QStringLiteral("A meld needs at least %1 real cards.")
                        .arg(m_rules.minNaturalsPerMeld);
            return false;
        }
        if (m_rules.wildsFewerThanNaturals && g.rank != kJoker
            && merged.wilds() >= merged.naturals()) {
            error = QStringLiteral("A meld keeps more real cards than jokers: that would "
                                   "leave %1 %2s against %3 jokers.")
                        .arg(merged.naturals())
                        .arg(rankLabel(g.rank))
                        .arg(merged.wilds());
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

    if (!meldingAllowed()) {
        error = QStringLiteral("Nobody lays anything down in the first round — every seat "
                               "plays once first.");
        return false;
    }
    if (!handContains(seat, cards)) {
        error = QStringLiteral("Those cards are not in your hand.");
        return false;
    }
    const bool goingOut = m_hands[std::size_t(seat)].size() == cards.size();
    if (!group(cards, goingOut, targetRank, -1, groups, error))
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
        error = QStringLiteral("The pack is empty.");
        return false;
    }
    // Taking the pile always melds the top card, so a rule that stops anyone
    // laying down in the first round stops the pile being taken as well.
    if (!meldingAllowed()) {
        error = QStringLiteral("Nobody lays anything down in the first round — every seat "
                               "plays once first.");
        return false;
    }
    const Card top = m_pile.back();
    if (m_rules.canastaMakesRankSafe) {
        const Meld* mine = m_teams[std::size_t(teamOf(m_current))].meldOfRank(top.rank);
        if (mine != nullptr && mine->isCanasta(m_rules)) {
            error = QStringLiteral("Your side has a canasta in %1s, so a %1 on top is a safe "
                                   "discard and cannot take the pile.")
                        .arg(rankLabel(top.rank));
            return false;
        }
    }
    if (isWild(top)) {
        error = QStringLiteral("A joker on top stops the pack being taken.");
        return false;
    }
    if (m_rules.blackThreeBlocksPile && isBlackThree(top)) {
        error = QStringLiteral("A black three on top stops the pack being taken.");
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
    // has to hold two matching cards of its own. A house rule can lift that and
    // let it take the pile on the same terms as anyone else.
    const Team& team = m_teams[std::size_t(t)];
    const bool mustUseTwoNaturals
        = m_frozen || (m_rules.pileFrozenUntilOpened && !team.opened);

    if (mustUseTwoNaturals) {
        if (naturalsOfTop < 2) {
            error = m_frozen
                ? QStringLiteral("The pack is frozen: you need two matching cards from your hand.")
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
    // The top card's rank gets first call on a wild, but not every wild: a
    // take whose own meld already stands up leaves them for the other ranks
    // going down with it. Forcing them all onto the top rank was what left a
    // pair of queens short of a meld in a lay-down that was otherwise legal.
    if (!group(combined, goingOut, -1, top.rank, groups, error))
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
        int have = layDownValue(combined);
        if (!m_rules.pileMeldCountsToOpen) {
            // House rule: the meld that captures the top card is worth nothing
            // toward opening, so the minimum has to come from the other melds
            // going down in the same move. The pile can then never be the thing
            // that opens you.
            have = 0;
            for (const Meld& m : groups)
                if (m.rank != top.rank)
                    have += layDownValue(m.cards);
        }
        if (have < need) {
            error = m_rules.pileMeldCountsToOpen
                ? QStringLiteral("Your side needs %1 to open, and that is only %2.")
                      .arg(need)
                      .arg(have)
                : QStringLiteral("Your side needs %1 to open from your other melds — the %2s "
                                 "taking the pile do not count, and the rest comes to %3.")
                      .arg(need)
                      .arg(rankLabel(top.rank))
                      .arg(have);
            return false;
        }
    }
    // Red threes in the pile go straight down when the take is performed and
    // earn no replacement, so the hand ends that many cards SHORTER than the
    // raw arithmetic says. keepsADiscard is what stops a seat reaching a
    // position with no legal move at all, and it was being handed a size the
    // take would not produce: land on two-by-the-arithmetic and one in fact,
    // with no canasta, and the seat is stranded -- canDiscard refuses and every
    // meld refuses, which is the hang CLAUDE.md calls the expensive symptom.
    // Land on nought and goOut() takes the seat out with no canasta at all.
    //
    // Counted over everything but the top card, because the top card goes into
    // the melds rather than the hand and is already the -1 below. Only the deal
    // ever puts a red three in the pile: a drawn one goes down at once, so it
    // can never be discarded.
    const std::size_t redThreesTaken = std::size_t(
        std::count_if(m_pile.begin(), m_pile.end() - 1, isRedThree));
    const std::size_t after = m_hands[std::size_t(seat)].size() - layDown.size()
        + m_pile.size() - 1 - redThreesTaken;
    if (!keepsADiscard(t, after, groups, error))
        return false;
    return true;
}

// The rules leave one shape of position unplayable: down to a single card with
// no canasta, you may not go out, and discarding that card would be going out.
// Nothing legal remains, and the turn cannot be finished. So the lay-down that
// would get you there is what has to be refused.
// The four black threes laid together, which is the one lay-down allowed to
// empty a hand under goingOutNeedsADiscard. Asked of the GROUPS rather than of
// the hand, because it is the shape of the move that earns the exception: all
// four, in one meld, on the way out.
static bool laysFourBlackThrees(const std::vector<Meld>& groups)
{
    for (const Meld& g : groups) {
        if (g.rank != 3 || g.cards.size() != 4)
            continue;
        if (std::all_of(g.cards.begin(), g.cards.end(), isBlackThree))
            return true;
    }
    return false;
}

bool Engine::keepsADiscard(int team, std::size_t handAfter, const std::vector<Meld>& groups,
                           QString& error) const
{
    // The house way out: the last action is a thrown card, so a lay-down that
    // empties the hand is refused whatever else is true of it. Checked before
    // the canasta rule below rather than instead of it — a hand that earns the
    // black-three exception still may not go out without a canasta, and falling
    // through to that check is what keeps both rules binding at once.
    if (m_rules.goingOutNeedsADiscard && handAfter == 0 && !laysFourBlackThrees(groups)) {
        error = QStringLiteral("You go out by throwing your last card — keep one back to "
                               "throw, or finish on all four black threes.");
        return false;
    }

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
    m_pileFrom.clear();
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
    m_pileFrom.push_back(qint8(seat));
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

    // Black threes are the exception, and leaving them out entirely meant the
    // one finish goingOutNeedsADiscard exists to permit -- all four laid
    // together -- was never highlighted in the hand. The highlight IS the
    // affordance, so a player who held the finish had nothing telling them so.
    // Offered only on all four, which is the shape laysFourBlackThrees() asks
    // for; three of them are meldable on the way out too, but highlighting
    // those would mark cards as useful on every turn where they are not.
    if (std::count_if(h.begin(), h.end(), isBlackThree) == 4)
        out.push_back(3);

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
    // A house rule voids a hand nobody went out on: the stock ran dry and the
    // position froze where it stood, so neither side scores it and the next
    // hand is dealt from the same totals. Classic scores it as it lies.
    const bool dead = m_rules.deadHandIfNobodyGoesOut && m_outSeat < 0;

    for (int t = 0; t < kTeams; ++t) {
        Team& team = m_teams[std::size_t(t)];
        const bool wentOut = m_outSeat >= 0 && teamOf(m_outSeat) == t;
        team.handScore = dead ? 0
                              : handScoreFor(team, m_hands[std::size_t(t)],
                                             m_hands[std::size_t(t + 2)], wentOut,
                                             m_outConcealed, m_rules);
        team.score += team.handScore;
    }

    const bool weReached = m_teams[0].score >= m_rules.targetScore;
    const bool theyReached = m_teams[1].score >= m_rules.targetScore;
    // Both sides over the target is a draw under the house rule, and the game
    // ends there — including on an exact tie, which is the same position read
    // the same way. Without the rule a tie on or over the target plays another
    // hand rather than declaring a joint winner, and the higher score wins.
    const bool drawn = m_rules.bothReachingTargetIsADraw && weReached && theyReached;
    const bool decided = (weReached || theyReached) && m_teams[0].score != m_teams[1].score;
    m_phase = (drawn || decided) ? Phase::GameOver : Phase::HandOver;
}

int Engine::freezeCardIndex() const
{
    if (!m_frozen)
        return -1;
    // Both places that set the flag turn a wild card or a red three onto the
    // pile -- the opening turn-up, and a wild discard -- and taking the pile
    // empties it and clears the flag. So the last such card still in the pile
    // is the one that froze it.
    for (int i = int(m_pile.size()) - 1; i >= 0; --i) {
        const Card& c = m_pile[std::size_t(i)];
        if (isWild(c) || isRedThree(c))
            return i;
    }
    return -1;
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
