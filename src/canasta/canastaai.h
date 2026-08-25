#pragma once

#include "canastaengine.h"

#include <random>
#include <utility>
#include <vector>

namespace canasta {

enum class Level { Easy, Medium, Hard, Expert };

// What it costs to throw a card into a rank the other side has melded. Bigger
// is worse; a negative figure means the card is SAFER than an ordinary one.
//
// A free function, like handScoreFor and openRequirementFor, so the ranking it
// produces can be checked against a hand-built position rather than one played
// into existence.

// A computer seat.
//
// Judgement rather than search: it scores the handful of moves in front of it
// instead of looking ahead. A whole turn costs microseconds, so unlike the chess
// engine this needs no work budget to keep the window responsive.
double discardRisk(const Team& theirs, int rank, int pileSize, bool frozen, const Rules& r);

// What ending the hand RIGHT NOW would take off the other side, over and above
// the going-out bonus, in points. Under canastaNeededToScore a side with no
// canasta has its own melds and red threes subtracted rather than added — see
// handScoreFor — so every point they have showing is worth two to us: one they
// do not get, and the same one taken off them. The owner's family calls it
// catching them a minus.
//
// Zero when the rule is off or they already have their canasta, which is the
// common case and the whole reason this is computed rather than assumed.
int minusOnOffer(const Team& theirs, const Rules& r);

// What staying in is worth, in points — the owner's family calls the position
// being fed. Zero when the pack is not coming back to us, so it can be compared
// against minusOnOffer directly rather than ordered against it.
//
// Three readings, and they are the three the position turns on. How big the
// pack has grown, since a thin one is not worth staying for. Whether it is
// frozen against us, which is the pack not coming back at all unless we hold a
// pair, Engine::canTakePile wanting two naturals out of hand. And how many of
// its cards are in ranks we already have down, which is what makes the NEXT
// throw into it one we can take.
int packWorthStayingFor(const std::vector<Card>& pile, const std::vector<Card>& hand,
                        const Team& mine, bool frozen, const Rules& r);

// What the safety half of a discard is WORTH against this opponent, as a
// multiplier on it. A free function, like the three above, so it can be checked
// on figures rather than on a position played into existence.
//
// A side that has not opened cannot simply take the pack: the meld it takes
// with has to clear its own opening minimum too. So the danger is graded by
// that bar — barely there at 15 or 50, real at 90, heavy at 120 — and caution
// spent above it is spent on something that mostly cannot happen (GHUB-0121).
//
// Returns exactly 1.0 against an opened side, so the common position plays as
// it always did. Pack size is deliberately NOT a factor here: GHUB-0108 asked
// for one weight over the whole accumulator and it measured worse, because
// discardRisk already carries its own. Read that bullet before adding it.
double throwCaution(const Team& theirs, int openRequirement);

// What throwing one of three or more of a rank is worth as BAIT (GHUB-0103).
// The owner's tactic: you throw them out one at a time until two are left,
// hoping the seat that discards to you is watching, reads the rank as safe and
// follows with it -- which hands you the pack.
//
// Zero below three held, since the pair is the key and breaking it buys
// nothing; zero on a pack too thin to be worth advertising for; and zero where
// no card of the rank is unaccounted for, because then no seat is holding one
// to follow with and the bait is an advertisement nobody can answer.
//
// The cost of it lands on the far side of the table -- the seat BELOW sees the
// same discard and may hold the pair that takes the pack with it. That half is
// not priced here: discardRisk and the pack count already answer it, and they
// are subtracted from the same discard's score.
double fishingWorth(int held, int packSize, int unseen);

// Whether this side is better off running the stock out and killing the hand
// than letting it be played to a score (GHUB-0114). The owner's tactic: if the
// opponents have a high score and you can run the pack dead -- no cards left in
// the pick-up pile -- and your own score will be low, it is better to run it
// dead than to play the hand out.
//
// It depends ENTIRELY on the house rule, which is the thing to get right rather
// than assume. deadHandIfNobodyGoesOut makes a hand the stock kills VOID, so
// neither side scores it; classic Canasta scores such a hand where it stands
// (pagat.com: when a player who wishes to draw cannot, "the play ends
// immediately and the hand is scored"), which hands the leader their points
// anyway. So the tactic is worth points under House and worth nothing under
// Classic, and this returns false there.
//
// The two showings are each side's table less the cards the reading seat holds
// -- never the opponents' hands, which this AI does not see.
bool runTheHandDead(int ourShowing, int theirShowing, int stockLeft, const Rules& r);

// Which of two unfinished melds to spend wild cards on first, for a side caught
// a minus (GHUB-0107). True when `a` should be closed before `b`.
//
// Nearest a canasta first, which is the OPPOSITE of the ordinary instinct and
// deliberately so: the meld closest to a canasta is also the one likeliest to
// fill naturally, so closing it with a wild turns a 500-point natural into a
// 300-point mixed one. Under canastaNeededToScore that trade is worth making --
// a side ending the hand with no canasta at all has everything it is showing
// taken OFF its score rather than added, so the first canasta is insurance
// worth far more than the 300 it pays.
//
// Which is why chooseMelds applies it only while caughtAMinus is true. Sorting
// unconditionally was measured and cost a game of medium v easy and 200 points
// of its margin -- the natural canasta being spent for nothing.
bool closeFirstUnderAMinus(const Meld& a, const Meld& b);

// What throwing a black three is worth as a block (GHUB-0109). It stops anyone
// taking the pack for exactly one turn, until it is covered — so it is worth
// what would have been taken in that turn.
//
// Two readings, and they are the two the owner named: how fat the pack is, and
// whether the seat to the left is live. That seat is always an opponent,
// partners sitting opposite, so throwCaution's grading of how live their side
// is answers the second exactly as it answers a dangerous throw. The pack-size
// half is the step this judgement already had, kept at its measured figures.
double blackThreeWorth(int packSize, double caution);

// How many of a rank are already accounted for — melded by either side, lying
// in the pack, or in this seat's own hand (GHUB-0106). Two packs, so eight of
// every rank exist, and a rank with all eight visible is a throw nobody at the
// table can use.
//
// A free function so the count can be checked on a hand-built table rather than
// on one played into existence; Ai::seen is a one-line call to it against the
// seat playing now.
int seenSoFar(const std::vector<Card>& hand, const std::vector<Card>& pile, const Team& one,
              const Team& two, int rank);

// What counting the pack is worth on a throw of one rank, as a safety figure —
// bigger is safer (GHUB-0106). `unseen` is 8 less what seenSoFar found.
//
// There is no separate cliff for a rank with all eight seen, and that is
// deliberate rather than an omission: unseen 0 is already the maximum of this
// term, carrying the bonus in full with no penalty against it, so the cliff is
// the top of the slope. Easy and Medium do not count the pack at all; Hard and
// Expert count it identically, and a fourth difference between those two would
// be a change the ladder cannot measure (GHUB-0110).
double packCountSafety(int unseen, int pileSize);

// How much of this hand is going to end up feeding the other side, as a share
// of the cards in it (GHUB-0101). The owner's first reason to freeze: "when
// your side will feed (continually give the pack away) the opposition".
//
// It has a precise meaning here rather than a vague one. discardRisk prices a
// throw into a rank the other side has melded, and returns zero the moment the
// pack is frozen — so a hand full of feeders is a hand that freezing makes
// throwable, quite apart from what it denies them. Asked through discardRisk
// rather than by counting melds directly, so the two cannot disagree about a
// rank a house rule has already made safe.
//
// Wild cards and threes are not counted: neither is an ordinary throw.
double feedPressure(const std::vector<Card>& hand, const Team& theirs, const Rules& r);

// Whether there is a REASON to spend a wild card freezing the pack, rather than
// merely the opportunity to (GHUB-0101). Three of them: the pack is their asset
// and not ours, we are fishing on a rank we have already melded, or this hand
// will keep feeding them whatever it throws. The first is published strategy;
// the other two are the owner's own, given 2026-08-25.
//
// This is what GHUB-0101 asked for. That bullet proposed a PRICE on the freeze
// rather than the veto it was filed as, and a price on a wild card is a tuned
// threshold the ladder cannot measure. Reasons are checkable instead.
bool freezeIsWorthTheWild(const std::vector<Card>& hand, const Team& mine, const Team& theirs,
                          const Rules& r);

// Whether freezing the pack would cost THIS side its own access to it
// (GHUB-0113). "Your team is positioned to claim the pile — freezing costs you
// access too": a freeze locks everyone out, so freezing a pack that is already
// coming back to us throws away the thing it was meant to protect.
//
// False for a side that has not opened, which pileFrozenUntilOpened already
// holds to two naturals out of hand — a freeze takes nothing from it. That is
// the whole of the difference between this and packWorthStayingFor, which it
// otherwise simply asks.
bool freezeCostsUsThePack(const std::vector<Card>& pile, const std::vector<Card>& hand,
                          const Team& mine, const Rules& r);

// Whether this side may spend another wild card freezing the pack this hand
// (GHUB-0113). Published strategy is explicit and numeric — "do not freeze more
// than twice per hand" — and nothing counted them, so a hand with spare wilds
// could spend three or four at 20 or 50 points each.
//
// A free function, like the four above, because the number is what wants
// checking and a hand that reaches a third freeze cannot be built cheaply: the
// pack has to be taken twice in between to unfreeze it.
bool freezeBudgetLeft(int freezesThisHand);

class Ai
{
public:
    explicit Ai(Level level = Level::Medium) : m_level(level) { }

    void setLevel(Level l) { m_level = l; }
    Level level() const { return m_level; }
    void seed(unsigned s) { m_rng.seed(s); }

    // First half of a turn: takes the pile when it is worth taking, otherwise
    // draws. Returns true if it took the pile.
    bool draw(Engine& e);
    // Second half: lays down what it should, then discards, ending the turn.
    void playAndDiscard(Engine& e);

    // How many wild cards this seat has spent freezing the pack in the hand
    // being played. Public so a check can see the budget move without playing
    // a hand as far as the ceiling.
    int freezesThisHand() const { return m_freezes; }

private:
    // Notices a fresh deal and clears the freeze budget. Called at the top of
    // both halves of a turn.
    void noteHand(const Engine& e);
    bool wantsPile(const Engine& e) const;
    // Whether this hand is one to run dead rather than finish (GHUB-0114) --
    // runTheHandDead asked of the position in front of this seat.
    bool killingTheHand(const Engine& e) const;
    // How many of a rank are already accounted for — melded by anyone, sitting
    // in the pile, or in this seat's own hand. A rank with all eight visible is
    // a discard nobody can use, which is the safest card in the game.
    int seen(const Engine& e, int rank) const;
    // Whether laying this rank down now is worth more than keeping it in hand
    // to take the pile with. Expert only: the others lay down everything they
    // legally can, which is the single biggest thing separating a good player
    // from a beginner.
    bool worthHolding(const Engine& e, int rank, int naturals) const;
    // Whether the frozen pile is worth more than laying this rank down. Every
    // card that goes on the table is a rank the opposition will then never
    // throw, and while the pile is frozen the pile is the prize.
    bool holdsWhileFrozen(const Engine& e, int rank, int naturals) const;
    // Whether the hand should be closed out now — a canasta down and the other
    // side without one, which is when going out catches them worst.
    bool closingOut(const Engine& e, std::size_t inHand) const;
    // Lay-downs to attempt in order, each with the rank its wild cards join.
    std::vector<std::pair<std::vector<Card>, int>> chooseMelds(const Engine& e) const;
    Card chooseDiscard(const Engine& e) const;
    // A wild card worth spending to freeze the pile against the opponents.
    bool wantsToFreeze(const Engine& e, Card& wild) const;

    Level m_level;
    // The per-hand freeze budget, and how a fresh deal is spotted. See
    // Ai::noteHand: a new game restarts Engine::handNumber() while these seats
    // live for the whole session, so the hand number is not the key.
    int m_freezes = 0;
    int m_lastStock = -1;
    // Fixed default so an unseeded game still plays the same way twice, which
    // is what makes a failing self-test reproducible. Mutable because choosing
    // a discard reads it without otherwise changing the seat.
    mutable std::mt19937 m_rng { 0x51ee7 };
};

} // namespace canasta
