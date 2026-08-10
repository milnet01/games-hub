#pragma once

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

    SudokuGrid() { generate(Level::Easy); }

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

private:
    bool fill(std::array<int, kCells>& grid, int pos, std::mt19937& rng);
    static bool allowed(const std::array<int, kCells>& grid, int row, int col, int value);

    std::array<int, kCells> m_solution {};
    std::array<int, kCells> m_puzzle {};
    std::array<int, kCells> m_working {};
    std::array<std::uint16_t, kCells> m_marks {};
    std::mt19937 m_rng { std::random_device {}() };
};
