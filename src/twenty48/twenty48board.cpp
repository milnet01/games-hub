#include "twenty48board.h"

#include <vector>

void Twenty48Board::newGame()
{
    m_cells.fill(0);
    m_previous.fill(0);
    m_score = 0;
    m_previousScore = 0;
    m_canUndo = false;
    m_reachedTarget = false;
    spawn();
    spawn();
}

bool Twenty48Board::slide(Direction direction)
{
    const std::array<int, kCells> before = m_cells;
    const int beforeScore = m_score;
    const bool beforeReachedTarget = m_reachedTarget;

    // Every direction is the same operation over a line; only the traversal
    // order changes, so the merge logic is written once.
    const bool horizontal = direction == Direction::Left || direction == Direction::Right;
    const int step = (direction == Direction::Left || direction == Direction::Up) ? 1 : -1;

    auto lineOf = [&](int index) {
        std::vector<int*> line;
        line.reserve(kSize);
        for (int i = 0; i < kSize; ++i) {
            const int along = step > 0 ? i : kSize - 1 - i;
            const int row = horizontal ? index : along;
            const int col = horizontal ? along : index;
            line.push_back(&cellAt(row, col));
        }
        return line;
    };

    for (int index = 0; index < kSize; ++index) {
        std::vector<int*> line = lineOf(index);

        // Compact towards the front.
        std::vector<int> values;
        values.reserve(kSize);
        for (int* cell : line) {
            if (*cell != 0)
                values.push_back(*cell);
        }

        // Merge equal neighbours ONCE each, front to back: 2 2 2 2 becomes
        // 4 4, never 8.
        std::vector<int> merged;
        merged.reserve(kSize);
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i + 1 < values.size() && values[i] == values[i + 1]) {
                const int value = values[i] * 2;
                merged.push_back(value);
                m_score += value;
                if (value >= kTarget)
                    m_reachedTarget = true;
                ++i;
            } else {
                merged.push_back(values[i]);
            }
        }

        for (std::size_t i = 0; i < line.size(); ++i)
            *line[i] = (i < merged.size()) ? merged[i] : 0;
    }

    if (m_cells == before) {
        m_score = beforeScore;
        m_reachedTarget = beforeReachedTarget;
        return false;
    }

    m_previous = before;
    m_previousScore = beforeScore;
    m_previousReachedTarget = beforeReachedTarget;
    m_canUndo = true;
    return true;
}

void Twenty48Board::spawn()
{
    std::vector<int> free;
    free.reserve(kCells);
    for (int i = 0; i < kCells; ++i) {
        if (m_cells[std::size_t(i)] == 0)
            free.push_back(i);
    }
    if (free.empty())
        return;

    std::uniform_int_distribution<std::size_t> pick(0, free.size() - 1);
    // One in ten new tiles is a 4, as in the original.
    std::uniform_int_distribution<int> four(0, 9);
    m_cells[std::size_t(free[pick(m_rng)])] = (four(m_rng) == 0) ? 4 : 2;
}

bool Twenty48Board::canMove() const
{
    for (int value : m_cells) {
        if (value == 0)
            return true;
    }
    for (int row = 0; row < kSize; ++row) {
        for (int col = 0; col < kSize; ++col) {
            const int v = at(row, col);
            if (col + 1 < kSize && at(row, col + 1) == v)
                return true;
            if (row + 1 < kSize && at(row + 1, col) == v)
                return true;
        }
    }
    return false;
}

void Twenty48Board::undo()
{
    if (!m_canUndo)
        return;
    m_cells = m_previous;
    m_score = m_previousScore;
    m_reachedTarget = m_previousReachedTarget;
    m_canUndo = false;
}

bool Twenty48Board::restore(const std::array<int, kCells>& cells, int score, bool reachedTarget)
{
    if (score < 0)
        return false;

    int tiles = 0;
    // The largest score these tiles could have earned. A tile of value v is
    // built by merges worth v * (log2(v) - 1) altogether, so summing that over
    // the board bounds the score from above -- and it over-counts a 4 that was
    // spawned rather than merged, which keeps it an upper bound rather than an
    // exact one. Derived from the position being restored rather than picked as
    // a round number, and it is what stops a save claiming a score near INT_MAX
    // that the very next merge would overflow.
    long long earned = 0;
    for (int value : cells) {
        if (!isTile(value))
            return false;
        if (value == 0)
            continue;
        ++tiles;
        int steps = 0;
        for (int v = value; v > 1; v /= 2)
            ++steps;
        if (steps >= 2)
            earned += 1LL * value * (steps - 1);
    }
    if (score > earned)
        return false;
    // An empty board is not a game in progress, and restoring one would leave
    // the player staring at nothing with no way to move.
    if (tiles == 0)
        return false;

    m_cells = cells;
    m_previous = cells;
    m_score = score;
    m_previousScore = score;
    m_reachedTarget = reachedTarget;
    m_previousReachedTarget = reachedTarget;
    m_canUndo = false;
    return true;
}
