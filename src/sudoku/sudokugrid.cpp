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
    // Through clearMarks() rather than writing the same zero a second time:
    // entering a digit is the one thing that clears a cell's marks, and two
    // copies of that are two places to change.
    if (value != 0)
        clearMarks(row, col);
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

// ---------------------------------------------------------------------------
// Saving the puzzle
// ---------------------------------------------------------------------------

namespace {

// True when `grid` is a completed Sudoku: every row, column and box holds each
// digit 1-9 exactly once. This is the check that makes a restored puzzle a
// puzzle rather than an arbitrary blob of digits -- everything else about the
// save is measured against the solution, so if the solution is real the rest
// cannot be nonsense.
bool isCompleteSolution(const std::array<int, SudokuGrid::kCells>& grid)
{
    constexpr int kSize = SudokuGrid::kSize;
    for (int i = 0; i < kSize; ++i) {
        std::uint16_t row = 0;
        std::uint16_t col = 0;
        std::uint16_t box = 0;
        for (int j = 0; j < kSize; ++j) {
            const int r = grid[SudokuGrid::index(i, j)];
            const int c = grid[SudokuGrid::index(j, i)];
            const int b = grid[SudokuGrid::index((i / 3) * 3 + j / 3, (i % 3) * 3 + j % 3)];
            if (r < 1 || r > 9 || c < 1 || c > 9 || b < 1 || b > 9)
                return false;
            row |= std::uint16_t(1u << r);
            col |= std::uint16_t(1u << c);
            box |= std::uint16_t(1u << b);
        }
        constexpr std::uint16_t kAllNine = 0x03fe;   // bits 1..9
        if (row != kAllNine || col != kAllNine || box != kAllNine)
            return false;
    }
    return true;
}

} // namespace

void SudokuGrid::save(QDataStream& out) const
{
    for (int v : m_solution)
        out << qint8(v);
    for (int v : m_puzzle)
        out << qint8(v);
    for (int v : m_working)
        out << qint8(v);
    for (std::uint16_t m : m_marks)
        out << quint16(m);
}

bool SudokuGrid::load(QDataStream& in)
{
    std::array<int, kCells> solution {};
    std::array<int, kCells> puzzle {};
    std::array<int, kCells> working {};
    std::array<std::uint16_t, kCells> marks {};

    auto readDigits = [&in](std::array<int, kCells>& into, int low) {
        for (int& v : into) {
            qint8 value = 0;
            in >> value;
            if (value < low || value > 9)
                return false;
            v = value;
        }
        return in.status() == QDataStream::Ok;
    };

    if (!readDigits(solution, 1) || !readDigits(puzzle, 0) || !readDigits(working, 0))
        return false;
    for (std::uint16_t& m : marks) {
        quint16 value = 0;
        in >> value;
        // One bit per digit 1-9; bit 0 and anything above bit 9 is not a mark
        // this game can make.
        if ((value & ~0x03feu) != 0)
            return false;
        m = value;
    }
    if (in.status() != QDataStream::Ok)
        return false;

    if (!isCompleteSolution(solution))
        return false;

    for (int i = 0; i < kCells; ++i) {
        // A clue is a cell of the solution shown from the start, so it must
        // agree with it -- and the working grid must still show it, because
        // set() refuses to write over a clue.
        if (puzzle[i] != 0) {
            if (puzzle[i] != solution[i] || working[i] != puzzle[i] || marks[i] != 0)
                return false;
        }
    }

    m_solution = solution;
    m_puzzle = puzzle;
    m_working = working;
    m_marks = marks;
    return true;
}
