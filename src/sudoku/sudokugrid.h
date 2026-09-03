#pragma once

#include "dealseed.h"

#include <QDataStream>

#include <array>
#include <cstdint>
#include <random>

// Sudoku generation and solving, free of Qt. Puzzles are made by filling a
// grid completely and then removing clues while a unique solution survives —
// which is what makes a puzzle solvable by reasoning rather than guessing.
class SudokuGrid
{
public:
    static constexpr int kSize = 9;
    static constexpr int kCells = kSize * kSize;

    enum class Level { Easy, Medium, Hard };

    // Deliberately does NOT generate. Every user either calls generate() or
    // load()s a save immediately after, so generating here was thrown away
    // every time: two puzzles built to open the game and three to restore one
    // save (GHUB-0160). An empty grid is what the callers overwrite.
    SudokuGrid() = default;

    void generate(Level level);

    int given(int row, int col) const { return m_puzzle[index(row, col)]; }
    int solution(int row, int col) const { return m_solution[index(row, col)]; }
    int value(int row, int col) const { return m_working[index(row, col)]; }
    bool isClue(int row, int col) const { return m_puzzle[index(row, col)] != 0; }

    // Setting a cell is refused on a clue; 0 clears it.
    void set(int row, int col, int value);

    // Pencil marks, one bit per digit 1-9.
    std::uint16_t marks(int row, int col) const { return m_marks[index(row, col)]; }
    void toggleMark(int row, int col, int digit);
    void clearMarks(int row, int col) { m_marks[index(row, col)] = 0; }

    // True when the digit clashes with another in its row, column or box.
    bool conflicts(int row, int col) const;
    bool solved() const;
    int emptyCount() const;

    void restart();

    static int index(int row, int col) { return row * kSize + col; }

    // Counts solutions up to `limit`, which is how uniqueness is checked.
    static int countSolutions(std::array<int, kCells> grid, int limit);

    // The puzzle, its solution, what has been written into it and the pencil
    // marks. Written through QDataStream, so the format IS the member order
    // below; a new member goes at the END and bumps the view's version.
    void save(QDataStream& out) const;
    // False on a grid the game could not have produced -- a solution that is
    // not a completed Sudoku, a clue that disagrees with it, an entry sitting
    // on top of a clue. Nothing is written to this object unless every check
    // passes, so a corrupt blob leaves the puzzle on screen alone. This is
    // what a game with no move log has instead of replaying the moves.
    bool load(QDataStream& in);

private:
    bool fill(std::array<int, kCells>& grid, int pos, std::mt19937& rng);
    static bool allowed(const std::array<int, kCells>& grid, int row, int col, int value);

    std::array<int, kCells> m_solution {};
    std::array<int, kCells> m_puzzle {};
    std::array<int, kCells> m_working {};
    std::array<std::uint16_t, kCells> m_marks {};
    std::mt19937 m_rng { dealSeed() };
};
