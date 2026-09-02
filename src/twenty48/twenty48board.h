#pragma once

#include "dealseed.h"

#include <array>
#include <random>

// The sliding-tile number game: push everything one way, equal neighbours
// merge, a new tile appears, keep going until nothing can move.
//
// The rules only — no widget, no painting, no undo ACTION, just the position
// and what a push does to it. Extracted so gameshub_selftest can reach them
// (GHUB-0066); before that, 2048's rules were named nowhere in that file.
class Twenty48Board
{
public:
    static constexpr int kSize = 4;
    static constexpr int kCells = kSize * kSize;
    static constexpr int kTarget = 2048;
    // A tile cannot exceed this, and a save claiming one did is refused. 2^20
    // is far beyond any reachable board and still leaves the check cheap.
    // 2^17 is the largest tile a four-by-four board can reach, and it takes
    // perfect play to get there. 1 << 20 admitted four values beyond anything
    // reachable, which a hand-edited save could then put on the board.
    static constexpr int kMaxTile = 1 << 17;

    enum class Direction { Left, Right, Up, Down };

    Twenty48Board() { newGame(); }

    // Clears the board and lays the opening two tiles.
    void newGame();

    // Slides and merges. Returns false when NOTHING moved, in which case the
    // score is left alone and no undo is banked — that is what stops a dead key
    // from wasting a turn, and it is why the caller must not spawn on a false.
    bool slide(Direction direction);

    // A new tile on a free square: nine times out of ten a 2, once a 4, as in
    // the original. Does nothing on a full board.
    void spawn();

    // False when the board is full AND no two neighbours match, which is the
    // only way this game ends.
    bool canMove() const;

    bool canUndo() const { return m_canUndo; }
    // Steps back to the position before the last slide that moved something.
    void undo();

    int at(int row, int col) const { return m_cells[std::size_t(row * kSize + col)]; }
    const std::array<int, kCells>& cells() const { return m_cells; }
    int score() const { return m_score; }
    bool reachedTarget() const { return m_reachedTarget; }

    // Adopts a position from a save. 2048 has no pack to check against, so what
    // stands in is the arithmetic: every tile must be a power of two in range,
    // because nothing else can come out of a merge, and an empty board is not a
    // game in progress. False leaves this object untouched.
    bool restore(const std::array<int, kCells>& cells, int score, bool reachedTarget);

    static bool isTile(int value)
    {
        return value == 0 || (value >= 2 && value <= kMaxTile && (value & (value - 1)) == 0);
    }

private:
    // Named apart from the const at() above on purpose: as an overload it wins
    // on a non-const board and makes the public reader unreachable from a test.
    int& cellAt(int row, int col) { return m_cells[std::size_t(row * kSize + col)]; }

    std::array<int, kCells> m_cells {};
    std::array<int, kCells> m_previous {};
    int m_score = 0;
    int m_previousScore = 0;
    bool m_canUndo = false;
    bool m_reachedTarget = false;

    std::mt19937 m_rng { dealSeed() };
};
