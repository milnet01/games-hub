#pragma once

#include "canastaengine.h"

#include <random>
#include <utility>
#include <vector>

namespace canasta {

enum class Level { Easy, Medium, Hard };

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
