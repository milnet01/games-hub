#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

// Noughts and Crosses: the rules and the computer player. No Qt at all, so the
// self-test can play every line of the game without a display.
namespace noughts {

enum class Mark : std::int8_t { Empty = 0, Cross = 1, Nought = 2 };

constexpr Mark other(Mark m)
{
    return m == Mark::Cross ? Mark::Nought : Mark::Cross;
}

constexpr int kSize = 3;
constexpr int kCells = kSize * kSize;

// Three cells in a row, each as row * kSize + column.
using Line = std::array<int, 3>;

// The eight ways to make three in a row: three rows, three columns, two slants.
constexpr std::array<Line, 8> kLines { {
    { 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 },
    { 0, 3, 6 }, { 1, 4, 7 }, { 2, 5, 8 },
    { 0, 4, 8 }, { 2, 4, 6 },
} };

class Board
{
public:
    Mark at(int cell) const { return m_cells[cell]; }
    const std::array<Mark, kCells>& cells() const { return m_cells; }
    static bool inBounds(int cell) { return cell >= 0 && cell < kCells; }

    // Puts `mark` on `cell`. Refuses a cell out of range or already taken, and
    // any move once the game is over.
    bool play(int cell, Mark mark);

    // The mark with three in a row, or Empty while nobody has one.
    Mark winner() const;
    // The row that won, for drawing the line through it.
    std::optional<Line> winningLine() const;
    bool full() const { return marksPlaced() == kCells; }
    bool over() const { return winner() != Mark::Empty || full(); }
    int marksPlaced() const { return kCells - count(Mark::Empty); }
    int count(Mark mark) const;
    std::vector<int> emptyCells() const;

    // Whose turn it is in a game `starter` opened: the two sides alternate, so
    // the counts say it.
    Mark toMove(Mark starter) const;

    // Takes a whole board, for a save. Refuses one no game opened by `starter`
    // could reach: a mark that is not one, the wrong number of each, both
    // sides in a row, or a win followed by another move. The board is left as
    // it was on a refusal.
    bool restore(const std::array<Mark, kCells>& cells, Mark starter);

private:
    std::array<Mark, kCells> m_cells {};
};

enum class Level { Easy, Hard };

// Every move that does best for `mover` against perfect play: a win soonest,
// else a draw, else the longest loss. Empty only once the game is over.
// Public so the self-test can follow every one of them, rather than whichever
// one the random pick happened to choose.
std::vector<int> bestMoves(const Board& board, Mark mover);

// The computer's move. Hard plays one of bestMoves() and never loses. Easy
// plays a random empty cell half the time and a best move otherwise, so it
// misses wins and blocks often enough to be beaten. -1 once the game is over.
// The pick uses rng() directly, never a std distribution, so a seed plays the
// same game on every compiler (see shuffleCards).
int chooseMove(const Board& board, Mark mover, Level level, std::mt19937& rng);

} // namespace noughts
