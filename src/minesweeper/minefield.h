#pragma once

#include <cstdint>
#include <vector>

// Minesweeper board logic, free of Qt so it can be tested headlessly.
class Minefield
{
public:
    enum class State { Playing, Won, Lost };

    struct Square {
        bool mine = false;
        bool revealed = false;
        bool flagged = false;
        int neighbours = 0; // adjacent mines, valid once mines are placed
    };

    Minefield(int width, int height, int mines);

    int width() const { return m_width; }
    int height() const { return m_height; }
    int mines() const { return m_mines; }
    State state() const { return m_state; }

    const Square& at(int row, int col) const { return m_squares[row * m_width + col]; }
    bool inBounds(int row, int col) const
    {
        return row >= 0 && row < m_height && col >= 0 && col < m_width;
    }

    int flagCount() const;
    // Mines left to find, by the player's own reckoning: total minus flags.
    int minesRemaining() const { return m_mines - flagCount(); }

    // Uncovers a square, cascading through blank areas. The first reveal of a
    // game lays the mines, so the opening click is never fatal.
    void reveal(int row, int col);
    void toggleFlag(int row, int col);

    // Uncovers every unflagged neighbour of an already-revealed square whose
    // flag count matches its number — the usual middle-click shortcut.
    void chord(int row, int col);

    void reset();

    // A game in progress, for saving: the squares in row-major order. The
    // neighbour counts are deliberately not part of that — restore() works them
    // out from the mines again, so a saved board cannot disagree with itself
    // about what a number means.
    const std::vector<Square>& squares() const { return m_squares; }
    // Takes a board back, mines already laid. Refuses one that is the wrong
    // size, carries the wrong number of mines, holds a square that is both dug
    // and flagged, or is not a game still in progress; this field is left
    // untouched when it does.
    bool restore(std::vector<Square> squares);

private:
    void placeMines(int safeRow, int safeCol);
    void countNeighbours();
    void revealFrom(int row, int col);
    void checkWin();

    Square& square(int row, int col) { return m_squares[row * m_width + col]; }

    int m_width;
    int m_height;
    int m_mines;
    bool m_minesPlaced = false;
    State m_state = State::Playing;
    std::vector<Square> m_squares;
    std::uint32_t m_seed;
};
