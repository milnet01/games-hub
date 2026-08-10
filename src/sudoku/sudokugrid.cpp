#include "sudokugrid.h"

#include <algorithm>
#include <numeric>

bool SudokuGrid::allowed(const std::array<int, kCells>& grid, int row, int col, int value)
{
    for (int i = 0; i < kSize; ++i) {
        if (grid[index(row, i)] == value || grid[index(i, col)] == value)
            return false;
    }
    const int boxRow = (row / 3) * 3;
    const int boxCol = (col / 3) * 3;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            if (grid[index(boxRow + r, boxCol + c)] == value)
                return false;
    return true;
}

bool SudokuGrid::fill(std::array<int, kCells>& grid, int pos, std::mt19937& rng)
{
    if (pos == kCells)
        return true;

    const int row = pos / kSize;
    const int col = pos % kSize;

    std::array<int, kSize> digits {};
    std::iota(digits.begin(), digits.end(), 1);
    std::shuffle(digits.begin(), digits.end(), rng);

    for (int d : digits) {
        if (!allowed(grid, row, col, d))
            continue;
        grid[std::size_t(pos)] = d;
        if (fill(grid, pos + 1, rng))
            return true;
        grid[std::size_t(pos)] = 0;
    }
    return false;
}

int SudokuGrid::countSolutions(std::array<int, kCells> grid, int limit)
{
    // Solve on the most constrained empty cell first; a plain left-to-right
    // scan is fast enough to write but far too slow on a sparse grid.
    int bestPos = -1;
    int bestCount = 10;
    for (int pos = 0; pos < kCells; ++pos) {
        if (grid[std::size_t(pos)] != 0)
            continue;
        const int row = pos / kSize;
        const int col = pos % kSize;
        int options = 0;
        for (int d = 1; d <= 9; ++d)
            if (allowed(grid, row, col, d))
                ++options;
        if (options < bestCount) {
            bestCount = options;
            bestPos = pos;
            if (options <= 1)
                break;
        }
    }

    if (bestPos < 0)
        return 1; // no empty cells: a complete solution
    if (bestCount == 0)
        return 0;

    const int row = bestPos / kSize;
    const int col = bestPos % kSize;
    int found = 0;
    for (int d = 1; d <= 9 && found < limit; ++d) {
        if (!allowed(grid, row, col, d))
            continue;
        grid[std::size_t(bestPos)] = d;
        found += countSolutions(grid, limit - found);
        grid[std::size_t(bestPos)] = 0;
    }
    return found;
}

void SudokuGrid::generate(Level level)
{
    m_solution.fill(0);
    fill(m_solution, 0, m_rng);
    m_puzzle = m_solution;

    // How many clues to leave. Fewer clues means more deduction.
    const int target = (level == Level::Easy) ? 42 : (level == Level::Medium) ? 32 : 26;

    std::array<int, kCells> order {};
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), m_rng);

    int clues = kCells;
    for (int pos : order) {
        if (clues <= target)
            break;
        const int saved = m_puzzle[std::size_t(pos)];
        if (saved == 0)
            continue;
        m_puzzle[std::size_t(pos)] = 0;
        // Keep the removal only if exactly one solution remains, so every
        // puzzle can be reasoned out rather than guessed.
        if (countSolutions(m_puzzle, 2) != 1)
            m_puzzle[std::size_t(pos)] = saved;
        else
            --clues;
    }

    restart();
}

void SudokuGrid::restart()
{
    m_working = m_puzzle;
    m_marks.fill(0);
}

void SudokuGrid::set(int row, int col, int value)
{
    if (isClue(row, col))
        return;
    m_working[index(row, col)] = value;
    if (value != 0)
        m_marks[index(row, col)] = 0;
}

void SudokuGrid::toggleMark(int row, int col, int digit)
{
    if (isClue(row, col) || digit < 1 || digit > 9)
        return;
    m_marks[index(row, col)] ^= std::uint16_t(1u << (digit - 1));
}

bool SudokuGrid::conflicts(int row, int col) const
{
    const int v = value(row, col);
    if (v == 0)
        return false;

    for (int i = 0; i < kSize; ++i) {
        if (i != col && value(row, i) == v)
            return true;
        if (i != row && value(i, col) == v)
            return true;
    }
    const int boxRow = (row / 3) * 3;
    const int boxCol = (col / 3) * 3;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            const int rr = boxRow + r;
            const int cc = boxCol + c;
            if ((rr != row || cc != col) && value(rr, cc) == v)
                return true;
        }
    }
    return false;
}

bool SudokuGrid::solved() const
{
    return m_working == m_solution;
}

int SudokuGrid::emptyCount() const
{
    return int(std::count(m_working.begin(), m_working.end(), 0));
}
