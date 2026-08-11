#pragma once

#include "cards/card.h"

#include <QDataStream>

#include <array>
#include <random>
#include <vector>

// Canasta for four, in two partnerships: seats 0 and 2 against seats 1 and 3,
// with the human at seat 0. No Qt beyond QString, so the whole rule set is
// testable headlessly.
//
// Its own namespace because the self-test compiles every game's rules into one
// binary and `Meld`, `Team` and `Phase` are far too common to leave at global
// scope — the same reason chess/ has one.
namespace canasta {

constexpr int kSeats = 4;
constexpr int kTeams = 2;

// Partners sit opposite each other, so the seat's parity is its team.
inline int teamOf(int seat) { return seat % 2; }
inline int partnerOf(int seat) { return (seat + 2) % kSeats; }

// Every number and toggle the game plays by, in one struct.
//
// This exists because house rules are the norm rather than the exception with
// Canasta — families keep the shape and move the numbers. Nothing in the engine
// hard-codes a score, a threshold or a permission; it all comes from here, so a
// second rule set is a different Rules value rather than a second engine.
struct Rules {
    QString name = QStringLiteral("Classic");

    int targetScore = 5000;
    int handSize = 11;
    int decks = 2;
    int jokers = 4;
    int canastaSize = 7;
    int minMeldSize = 3;
    int maxWildsPerMeld = 3;
    // A meld must keep at least this many real cards, which is what stops
    // three wilds and one natural counting as a meld.
    int minNaturalsPerMeld = 2;

    // The opening meld minimum, chosen by the team's score when the hand is
    // dealt. Rises as you get closer to winning.
    int openMinBelowZero = 15;
    int openMinUnder1500 = 50;
    int openMinUnder3000 = 90;
    int openMinAbove3000 = 120;

    int redThreeValue = 100;
    // What all four are worth to one team, which is more than four separate
    // threes rather than the same.
    int allRedThreesValue = 800;

    int naturalCanastaBonus = 500;
    int mixedCanastaBonus = 300;
    int goingOutBonus = 100;
    // Melding your whole hand in one turn, having laid down nothing before.
    // Replaces the ordinary going-out bonus rather than adding to it.
    int concealedGoingOutBonus = 200;

    // Card values. Held here rather than in a switch so a house rule can move
    // them; the defaults are the standard table.
    int jokerValue = 50;
    int wildTwoValue = 20;
    int aceValue = 20;
    int highCardValue = 10;  // 8 through King
    int lowCardValue = 5;    // 4 through 7
    int blackThreeValue = 5;

    bool requireCanastaToGoOut = true;
    // A black three on top stops anyone taking the pile.
    bool blackThreeBlocksPile = true;
    // Melds made purely of wild cards. Not part of the classic game.
    bool wildCardMeldsAllowed = false;
    // On an unfrozen pile, take it with one matching card and one wild.
    bool unfrozenPileTakeableWithWild = true;
    // On an unfrozen pile, take it by adding the top card to a meld you own.
    bool unfrozenPileTakeableByExtending = true;
    // A meld must always hold more real cards than wild ones — three sixes
    // carry two wilds, and the third wild waits for a fourth six. Stricter
    // than maxWildsPerMeld, which is a flat ceiling rather than a ratio.
    bool wildsFewerThanNaturals = false;
    // A side that never made a canasta counts nothing in its favour: its melds
    // are taken off its score exactly like the cards left in its hands, and its
    // red threes count against it too. Classic Canasta asks only that a side
    // has opened, which is why a side with melds and no canasta still scores
    // positively there.
    bool canastaNeededToScore = false;
    // Once a side has a canasta in a rank, that rank is a safe discard against
    // it: the pile can no longer be taken with it. The canasta itself stays
    // open — its owners go on adding to it — so this is a rule about the pile
    // and not about the meld. The classic game has no such rule.
    bool canastaMakesRankSafe = false;
    // Nobody lays anything down until every seat has had a turn, so the first
    // round is pure draw and discard and the pile builds before anyone opens.
    bool noMeldingFirstRound = false;
    // Whether the meld that captures the top card counts toward the opening
    // minimum. When it does not, the minimum has to be made up entirely of the
    // other melds laid down in the same move — so the pile can never be what
    // opens you.
    bool pileMeldCountsToOpen = true;

    static Rules classic() { return {}; }
};

bool isWild(const Card& c);
bool isRedThree(const Card& c);
bool isBlackThree(const Card& c);
int cardValue(const Card& c, const Rules& rules);

// What a team must lay down to open, given the score it starts the hand on.
int openRequirementFor(int score, const Rules& rules);

// One rank laid on the table. Black threes make a meld only on the turn a
// player goes out, and never take a wild card.
struct Meld {
    int rank = 0;
    std::vector<Card> cards;

    int naturals() const;
    int wilds() const;
    int size() const { return int(cards.size()); }
    // Black threes never make a canasta however many of them go down, which is
    // why the rank is part of the test and not just the count.
    bool isCanasta(const Rules& r) const { return rank != 3 && size() >= r.canastaSize; }
    bool isNatural(const Rules& r) const { return isCanasta(r) && wilds() == 0; }
    int value(const Rules& r) const;
};

struct Team {
    std::vector<Meld> melds;
    std::vector<Card> redThrees;
    int score = 0;      // running total for the game
    int handScore = 0;  // what the last completed hand was worth
    bool opened = false;

    const Meld* meldOfRank(int rank) const;
    bool hasCanasta(const Rules& r) const;
};

// The whole scoring table in one place: melded card values, canasta bonuses,
// red threes either way, the going-out bonus, less whatever the two partners
// were still holding. A free function so the self-test can score a position it
// has built by hand rather than one it had to play into existence.
int handScoreFor(const Team& t, const std::vector<Card>& handOne, const std::vector<Card>& handTwo,
                 bool wentOut, bool concealed, const Rules& rules);

// The order a hand is fanned in: wild cards first, then aces down to threes,
// suit breaking ties so pairs sit together. Nothing in Canasta depends on the
// order a hand is held in — this is how it is arranged at a table, and the
// engine owns it only because the engine owns the hand.
bool sortsBefore(const Card& a, const Card& b);

class Engine
{
public:
    enum class Phase {
        Draw,      // the seat must draw from stock or take the pile
        Play,      // it may meld, and must discard to end its turn
        HandOver,  // the hand is scored; call nextHand()
        GameOver,
    };

    explicit Engine(Rules rules = Rules::classic());

    const Rules& rules() const { return m_rules; }
    // Applied at the next newGame(), never mid-hand — changing the target score
    // or an opening minimum partway through a hand would rewrite the position
    // under the players.
    void setRules(Rules r) { m_pendingRules = std::move(r); }
    const Rules& pendingRules() const { return m_pendingRules; }

    void newGame();
    void newGame(unsigned seed);
    // Starts a game from a stock you supply rather than a shuffled one. Cards
    // are dealt off the back, so the tail of `stock` is the deal. Lets the
    // self-test build an exact position, and lets a reported deal be replayed.
    void newGameFromStock(std::vector<Card> stock, int dealer = 0);
    // Deals the next hand after HandOver.
    void nextHand();

    Phase phase() const { return m_phase; }
    int handNumber() const { return m_hand; }
    int dealer() const { return m_dealer; }
    int currentSeat() const { return m_current; }

    const std::vector<Card>& hand(int seat) const { return m_hands[std::size_t(seat)]; }
    // Rearranges a hand into sortsBefore() order. Cosmetic, so unlike every
    // action below it validates nothing and may be called at any point.
    void sortHand(int seat);
    const Team& team(int t) const { return m_teams[std::size_t(t)]; }
    const std::vector<Card>& pile() const { return m_pile; }
    bool pileFrozen() const { return m_frozen; }
    int stockCount() const { return int(m_stock.size()); }

    // The minimum a team must lay down to open, fixed when the hand is dealt.
    int openRequirement(int t) const { return m_openReq[std::size_t(t)]; }

    int wentOutSeat() const { return m_outSeat; }
    bool wasConcealed() const { return m_outConcealed; }
    // Team that has won, or -1 while the game is still running.
    int winner() const;

    // --- Actions. Each validates first and changes nothing when it refuses,
    // leaving the reason in lastError(). ---

    bool drawFromStock();
    // Takes the whole pile. `layDown` is every card from hand being laid down
    // as part of the take; the top card joins whichever group matches its rank.
    bool takePile(const std::vector<Card>& layDown);
    // Lays cards down, grouped by rank. Opening must be done in one call, since
    // that is what the minimum is measured against.
    //
    // `targetRank` says which meld the wild cards join. It is needed whenever
    // the selection is wild cards alone — adding a joker to a meld is an
    // ordinary move and the wild carries no rank of its own — and it also
    // settles a mixed selection that would otherwise be ambiguous.
    bool meldCards(const std::vector<Card>& cards, int targetRank = -1);
    bool discard(const Card& c);

    // --- Queries, for the board's highlighting. None of them change state. ---

    // False while Rules::noMeldingFirstRound is still holding the first round
    // open. The board asks so it can say why, since a greyed-out Meld with no
    // reason given reads as a bug rather than as a rule.
    bool meldingAllowed() const;

    bool canDrawFromStock() const;
    bool canTakePile(const std::vector<Card>& layDown) const;
    // True when some legal way to take the pile exists for the current seat.
    // Unlike canTakePile() this ignores the phase, because the engine asks it
    // at the top of a turn to decide whether a hand can continue at all once
    // the stock has gone — before there is a phase to check.
    bool canTakePileAtAll() const;
    // The cards from the current seat's hand that would take the pile, or empty
    // when it cannot be taken (or needs nothing but an existing meld).
    bool findPileTake(std::vector<Card>& out) const;
    bool canMeldCards(const std::vector<Card>& cards, int targetRank = -1) const;
    bool canDiscard(const Card& c) const;
    // Ranks in this seat's hand it could legally lay down right now.
    std::vector<int> meldableRanks(int seat) const;

    const QString& lastError() const { return m_error; }

    // Total cards in play. Every one of the 108 is somewhere; the self-test
    // leans on this to prove no action loses or duplicates a card.
    int cardsInPlay() const;

    // --- Saving a game in progress ---
    //
    // The whole position, so a game can be picked up later: every hand, the
    // melds, the pile, the stock and whose turn it is. Rules are saved with it,
    // because a hand dealt under one set cannot be finished under another.
    // Written through QDataStream, so the format is the member order below and
    // a version number guards it.
    void save(QDataStream& out) const;
    // Leaves the engine untouched and returns false if the stream is from a
    // different version or runs out part way. `hasTail` says whether the stream
    // carries the fields added after the format was first written: only the
    // caller knows, since it may have written fields of its own after ours.
    bool load(QDataStream& in, bool hasTail = true);

private:
    void deal();
    void dealFrom(std::vector<Card> stock);
    void startTurn();
    void endTurn();
    void advanceSeat();
    void scoreHand();
    void goOut(int seat);
    // Moves red threes out of a seat's hand onto the table, drawing a
    // replacement for each. Returns false if the stock ran dry doing it.
    bool placeRedThrees(int seat, bool replace);

    // Splits a lay-down into per-rank groups, refusing anything ambiguous.
    // `goingOut` relaxes the ban on melding black threes; `targetRank` is where
    // wild cards go when their rank is not obvious.
    bool group(const std::vector<Card>& cards, bool goingOut, int targetRank,
               std::vector<Meld>& out, QString& error) const;
    // Places wild cards across several ranks going down together, since every
    // legal spread scores the same. Fails only when no meld can take one.
    bool spreadWilds(std::vector<std::pair<int, std::vector<Card>>>& naturals,
                     const std::vector<Card>& wilds, QString& error) const;
    // Checks each group against the meld of that rank the team already holds.
    bool validateGroups(int team, const std::vector<Meld>& groups, bool goingOut,
                        QString& error) const;
    // Everything meldCards() and takePile() must agree on, so the "can I?"
    // query and the action itself can never diverge.
    bool validateMeld(const std::vector<Card>& cards, int targetRank,
                      std::vector<Meld>& groups, QString& error) const;
    bool validateTake(const std::vector<Card>& layDown, std::vector<Meld>& groups,
                      QString& error) const;
    bool teamWouldHaveCanasta(int team, const std::vector<Meld>& groups) const;
    // Refuses a lay-down that would strand the seat with nothing legal to do.
    bool keepsADiscard(int team, std::size_t handAfter, const std::vector<Meld>& groups,
                       QString& error) const;

    bool handContains(int seat, const std::vector<Card>& cards) const;
    void removeFromHand(int seat, const std::vector<Card>& cards);
    void commit(int team, const std::vector<Meld>& groups);
    int layDownValue(const std::vector<Card>& cards) const;

    bool fail(const QString& why) const;

    Rules m_rules;
    Rules m_pendingRules;

    std::array<std::vector<Card>, kSeats> m_hands;
    std::array<Team, kTeams> m_teams;
    std::array<int, kTeams> m_openReq {};
    // Whether each seat has laid anything down this hand, which is what makes
    // a going-out concealed or not.
    std::array<bool, kSeats> m_hasMelded {};
    bool m_meldedBeforeTurn = false;

    std::vector<Card> m_stock;
    std::vector<Card> m_pile;

    Phase m_phase = Phase::HandOver;
    int m_hand = 0;
    int m_dealer = 0;
    int m_current = 0;
    // Turns finished this hand. The first round is over once every seat has
    // had one, which is what Rules::noMeldingFirstRound waits for.
    int m_turnsTaken = 0;
    int m_outSeat = -1;
    bool m_outConcealed = false;
    bool m_frozen = false;

    mutable QString m_error;
    std::mt19937 m_rng { std::random_device {}() };
};

} // namespace canasta
