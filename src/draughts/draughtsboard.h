#pragma once

#include <cstdint>
#include <vector>

// Draughts (checkers) on the standard 8x8 board, English rules: men move and
// capture diagonally forwards, kings both ways, captures are compulsory, and
// a capture sequence must be played to its end. No Qt, so it is testable
// headlessly like every other rules core here.

enum class Side : std::int8_t { Red = 0, White = 1 };

enum class Piece : std::int8_t {
    Empty = 0,
    RedMan,
    RedKing,
    WhiteMan,
    WhiteKing,
};

constexpr int kBoardSize = 8;
constexpr int kBoardCells = kBoardSize * kBoardSize;

constexpr Side other(Side s) { return s == Side::Red ? Side::White : Side::Red; }

bool belongsTo(Piece p, Side s);
bool isKing(Piece p);

struct Square {
    int row = 0;
    int col = 0;

    friend bool operator==(Square a, Square b) { return a.row == b.row && a.col == b.col; }
};

// A move is the square it starts on plus the squares it lands on in order: a
// simple move has one step, a multi-jump has several.
struct DraughtsMove {
    Square from;
    std::vector<Square> steps;
    std::vector<Square> captured;

    Square destination() const { return steps.empty() ? from : steps.back(); }
    bool isCapture() const { return !captured.empty(); }
};

class DraughtsBoard
{
public:
    DraughtsBoard() { reset(); }

    void reset();

    Piece at(int row, int col) const { return m_cells[row * kBoardSize + col]; }
    static bool inBounds(int row, int col)
    {
        return row >= 0 && row < kBoardSize && col >= 0 && col < kBoardSize;
    }
    // Play happens only on the dark squares.
    static bool isPlayable(int row, int col) { return ((row + col) % 2) == 1; }

    // Every legal move for `side`. When any capture exists, only captures are
    // returned — taking is compulsory.
    std::vector<DraughtsMove> legalMoves(Side side) const;

    void apply(const DraughtsMove& move);

    int count(Side side) const;
    bool gameOver(Side toMove) const { return legalMoves(toMove).empty(); }

    // The whole board, for saving a game in progress.
    const std::vector<Piece>& cells() const { return m_cells; }
    // Takes a board back. Refuses one that is the wrong size, holds something
    // that is not a piece, stands a piece on a light square, or leaves a side
    // with nothing at all. Leaves this board untouched when it refuses.
    bool restore(const std::vector<Piece>& cells);

private:
    void set(int row, int col, Piece p) { m_cells[row * kBoardSize + col] = p; }
    // Walks every capture continuation from a square, depth first.
    // By const reference: each branch copies what it needs into its own
    // nextSteps / nextCaptured / next before touching it, so taking these by
    // value copied two vectors and a whole board at every step of the search
    // and then copied them again.
    void collectJumps(Side side, Square from, Piece piece, const std::vector<Square>& steps,
                      const std::vector<Square>& captured, const DraughtsBoard& working,
                      std::vector<DraughtsMove>& out) const;

    std::vector<Piece> m_cells;
};

// Search depth by difficulty, mirroring Reversi's three levels.
enum class DraughtsLevel { Easy, Medium, Hard };

// Best move for `side`, or an empty move list when there is none.
bool chooseDraughtsMove(const DraughtsBoard& board, Side side, DraughtsLevel level,
                        DraughtsMove& out);
