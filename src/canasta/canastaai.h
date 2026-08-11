#pragma once

#include "canastaengine.h"

#include <random>
#include <utility>
#include <vector>

namespace canasta {

enum class Level { Easy, Medium, Hard, Expert };

// A computer seat.
//
// Judgement rather than search: it scores the handful of moves in front of it
// instead of looking ahead. A whole turn costs microseconds, so unlike the chess
// engine this needs no work budget to keep the window responsive.
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

private:
    bool wantsPile(const Engine& e) const;
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
    // Fixed default so an unseeded game still plays the same way twice, which
    // is what makes a failing self-test reproducible. Mutable because choosing
    // a discard reads it without otherwise changing the seat.
    mutable std::mt19937 m_rng { 0x51ee7 };
};

} // namespace canasta
