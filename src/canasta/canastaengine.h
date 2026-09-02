#pragma once

#include "dealseed.h"

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
    // How the hand ENDS. At the family table the last action is always a
    // thrown card, so a lay-down that empties the hand is refused however
    // legal its melds are. The one exception is the hand that finishes on all
    // four black threes: everything else goes onto melds, the four threes go
    // down together, and nothing is thrown — which is also the only way they
    // are ever worth their 20 rather than being caught in hand. The classic
    // game lets you meld out with anything, which is why this is off by
    // default.
    bool goingOutNeedsADiscard = false;
    // A black three on top stops anyone taking the pile.
    bool blackThreeBlocksPile = true;
    // Melds made purely of wild cards. Not part of the classic game.
    bool wildCardMeldsAllowed = false;
    // On an unfrozen pile, take it with one matching card and one wild.
    bool unfrozenPileTakeableWithWild = true;
    // On an unfrozen pile, take it by adding the top card to a meld you own.
    bool unfrozenPileTakeableByExtending = true;
    // Classic Canasta freezes the pile against a side that has not opened, so
    // that side needs two matching cards from hand exactly as if it were frozen
    // for everyone. Turned off, an unopened side takes the pile on the same
    // terms as anyone else — one matching card and a wild will do it.
    bool pileFrozenUntilOpened = true;
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
    // A hand that runs the stock out with nobody going out is void: neither
    // side scores it at all, and the next hand is dealt from the same totals.
    // The classic game scores such a hand where it stands, which can hand a
    // large total to a side that did nothing but sit on its cards while the
    // pile stayed frozen.
    bool deadHandIfNobodyGoesOut = false;
    // The game is won by REACHING the target rather than by being ahead when
    // somebody does. So a hand that carries both sides past it is a draw,
    // however far apart the two totals are, and nobody wins. The classic game
    // hands it to the higher score, which is why this is off by default.
    //
    // It also settles the exact tie, which without it plays another hand: both
    // sides reached the target, so under this rule that is a draw like any
    // other.
    bool bothReachingTargetIsADraw = false;

    // --- Table conventions ---------------------------------------------
    // How the table is laid OUT, rather than what is legal on it. The engine
    // reads neither of these and not one move changes with them. They live
    // here because they are house rules in every sense that matters to the
    // people playing -- "we don't do it like that" -- and because Rules is
    // already the one thing the House dialog edits, stores and restores. A
    // second struct for two booleans would need its own dialog rows, its own
    // settings keys and its own Classic/House switch, all to keep a layering
    // line tidy.

    // The wild card that freezes the pile lies with one end against the pile
    // rather than squarely across its middle, making a T instead of a cross.
    bool freezeCardMakesATee = false;
    // A finished canasta is squared up and laid on the team's red threes, each
    // one turned ninety degrees from the one below so the stack can be counted
    // by its edges. Off, canastas stay fanned in the meld row.
    bool canastasStackOnRedThrees = false;

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

// True when a side's own cards are about to be counted AGAINST it for want of
// a canasta. The owner's family calls it catching them a minus, which is both
// shorter and clearer than any sentence about melds being subtracted.
//
// A free function rather than a method, on the same footing as handScoreFor
// and openRequirementFor: it can be checked on a hand-built position instead
// of one played into existence.
bool caughtAMinus(const Team& t, const Rules& rules);

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
    // Changes the rules of the game ALREADY IN PROGRESS, keeping the position
    // and the scores. Everything that is a permission or a value takes effect
    // at once; the three numbers that shaped the deal — pack size, jokers and
    // hand size — cannot change under cards already dealt, so they carry on as
    // dealt and take effect from the next hand. Nobody should have to abandon a
    // game at 2335 to correct a house rule.
    void applyRules(Rules r);

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
    // Which seat discarded pile()[index], or -1 for a card nobody threw -- the
    // up-card and whatever covered it at the deal, and any card in a pile
    // restored from a save written before this was recorded.
    //
    // The pack keeps this because the AI cannot otherwise tell a rank the table
    // has genuinely parted with from BAIT one seat keeps feeding in. Both look
    // identical as a count, and the count is exactly what a fishing seat aims
    // at (GHUB-0124).
    int pileThrownBy(int index) const;
    // How many INDEPENDENT sources have put `rank` into the pile: each seat
    // that threw one counts once however many it threw, and the cards nobody
    // threw -- the deal's up-card and its cover -- count once between them.
    //
    // A seat that has thrown the same rank twice is telling you it holds more
    // of them, not fewer, so its second card is no evidence the rank is safe.
    // A raw count cannot tell that from two seats genuinely letting the rank
    // go, and the raw count is what a fishing seat feeds (GHUB-0124).
    // exceptSeat is not counted. The asking seat passes its own: its earlier
    // discards are still in the pile, and reading them back as evidence that
    // the TABLE is parting with the rank is a seat believing its own bait.
    int pileRankSources(int rank, int exceptSeat = -1) const;
    bool pileFrozen() const { return m_frozen; }
    // Where in pile() the card that froze it sits, or -1 when the pile is not
    // frozen. The table draws that card sideways and needs its DEPTH rather
    // than its identity: drawn one place under the top card instead, it
    // climbed back over every discard thrown after it (GHUB-0094).
    int freezeCardIndex() const;
    int stockCount() const { return int(m_stock.size()); }

    // The minimum a team must lay down to open, fixed when the hand is dealt.
    int openRequirement(int t) const { return m_openReq[std::size_t(t)]; }

    int wentOutSeat() const { return m_outSeat; }
    bool wasConcealed() const { return m_outConcealed; }
    // What winner() answers when the game is over and NOBODY won it: both
    // sides reached the target in the same hand under bothReachingTargetIsADraw.
    // Its own value rather than -1, which already means "still running" — a
    // caller that tested `winner() == 0` and called everything else a loss
    // would otherwise report a draw as a defeat.
    static constexpr int kDraw = -2;
    // Team that has won, kDraw when the game is over and nobody did, or -1
    // while the game is still running.
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

    // True when what the current seat throws cannot be taken by anybody at all,
    // which is what makes the throw free. True only while the first-round rule
    // still bars the seat that plays NEXT — a different question from
    // meldingAllowed(), and the difference is the whole of it. The fourth seat
    // of the first round is itself barred from melding, but the turn after it
    // is the first seat playing a SECOND time, by which point the rule has
    // lifted. So the fourth seat is the one seat in the round whose discard is
    // live, and this returns FALSE for that seat while returning true for the
    // three before it.
    //
    // Read the other way round — as though it were named discardCanBeTaken —
    // it is a loaded gun, in the corner CLAUDE.md already calls the hardest
    // quarter of a bug to notice: an AI rule built on the inverted reading is
    // right three times a round and wrong on the fourth.
    //
    // The AI reads it to know when a throw is free: with nothing takeable, a
    // black three's blocking power is worth nothing and the card it would
    // never dare throw later costs nothing now.
    bool discardCannotBeTaken() const;

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
    // The number of appended-rule pairs the current save() writes. A caller
    // reading an older stream passes how many that stream has.
    //
    // This said 3 while save() wrote 4, from GHUB-0120: the default-argument
    // path — which is what the self-test's round trip uses — then stopped one
    // pair short and silently dropped goingOutNeedsADiscard on the way back
    // in. A stale figure here does not fail, it loses a rule.
    // Tail 6 is not a rule pair: it is the pile's provenance, one seat per
    // card. A stream that stops at 5 loads with the pile marked unknown, which
    // turns the fishing defence off for that hand rather than refusing the
    // save (GHUB-0124).
    // Tails 7 and 8 are freezeCardMakesATee and canastasStackOnRedThrees. They
    // were in neither writeRules() nor a tail, so a resumed House game came back
    // with the freeze card drawn as a cross and canastas fanned in the meld row,
    // while the toolbar still read House -- the same class as GHUB-0120 above.
    static constexpr int kTail = 8;
    // Leaves the engine untouched and returns false if the stream is from a
    // different version or runs out part way. `tail` says how many of the
    // fields added after the format was first written the stream carries: only
    // the caller knows, since it may have written fields of its own after ours.
    bool load(QDataStream& in, int tail = kTail);

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
    // `priorityRank` gets first call on a wild card while it is still short of
    // a meld — the top card's rank during a pile take, since the move depends
    // on it. Everything after that is spread by need.
    bool group(const std::vector<Card>& cards, bool goingOut, int targetRank, int priorityRank,
               std::vector<Meld>& out, QString& error) const;
    // Places wild cards across several ranks going down together, since every
    // legal spread scores the same. Fails only when no meld can take one.
    bool spreadWilds(std::vector<std::pair<int, std::vector<Card>>>& naturals,
                     const std::vector<Card>& wilds, int priorityRank, QString& error) const;
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
    // Parallel to m_pile: who threw each card. Kept in step at every point the
    // pile changes -- the deal, a discard, and a take, which clears both.
    std::vector<qint8> m_pileFrom;

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
    std::mt19937 m_rng { dealSeed() };
};

} // namespace canasta
