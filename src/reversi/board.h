#pragma once

#include <array>
#include <cstdint>
#include <vector>

// Reversi rules engine. Deliberately free of any Qt dependency so
// the AI can search thousands of positions per move without touching the UI.

enum class Player : std::int8_t { Black = 1, White = -1 };
enum class Cell : std::int8_t { Empty = 0, Black = 1, White = -1 };

constexpr int kSize = 8;
constexpr int kCells = kSize * kSize;

constexpr Player opponent(Player p)
{
    return p == Player::Black ? Player::White : Player::Black;
}

struct Move {
    int row = 0;
    int col = 0;

    friend bool operator==(Move a, Move b) { return a.row == b.row && a.col == b.col; }
};

class Board
{
public:
    Board() { reset(); }

    // Standard opening: the four centre squares, Black to move.
    void reset();

    Cell at(int row, int col) const { return m_cells[row * kSize + col]; }
    static bool inBounds(int row, int col)
    {
        return row >= 0 && row < kSize && col >= 0 && col < kSize;
    }

    std::vector<Move> legalMoves(Player p) const;
    bool isLegal(Player p, Move m) const { return flipCount(p, m) > 0; }

    // Discs that would flip if p played m. 0 means the move is illegal.
    int flipCount(Player p, Move m) const;

    // Plays m for p and flips the captured discs. Returns the number flipped,
    // or 0 (leaving the board untouched) if the move was illegal.
    int play(Player p, Move m);

    int count(Player p) const;
    int emptyCount() const;

    // True once neither side has a legal move — the only way Reversi ends.
    bool gameOver() const { return legalMoves(Player::Black).empty() && legalMoves(Player::White).empty(); }

private:
    // Number of opponent discs captured from m in direction (dr, dc), or 0 if
    // that ray is not bracketed by one of p's own discs.
    int ray(Player p, Move m, int dr, int dc) const;

    void set(int row, int col, Cell c) { m_cells[row * kSize + col] = c; }

    std::array<Cell, kCells> m_cells{};
};
