#include "minefield.h"

#include <algorithm>
#include <random>

Minefield::Minefield(int width, int height, int mines)
    : m_width(width)
    , m_height(height)
    , m_mines(std::min(mines, width * height - 9)) // leave room for a safe opening
    , m_seed(std::random_device {}())
{
    m_squares.resize(std::size_t(width) * height);
}

bool Minefield::restore(std::vector<Square> squares)
{
    if (squares.size() != m_squares.size())
        return false;

    int mines = 0;
    for (const Square& s : squares) {
        if (s.revealed && s.flagged)
            return false;
        if (s.mine) {
            if (s.revealed) // a dug mine is a game already lost, not one to resume
                return false;
            ++mines;
        }
    }
    if (mines != m_mines)
        return false;

    m_squares = std::move(squares);
    m_minesPlaced = true;
    m_state = State::Playing;
    countNeighbours();
    // A board with nothing left to dig is finished rather than in progress, and
    // the caller checks state() to find out.
    checkWin();
    return true;
}

int Minefield::flagCount() const
{
    return int(std::count_if(m_squares.begin(), m_squares.end(),
                             [](const Square& s) { return s.flagged; }));
}

void Minefield::placeMines(int safeRow, int safeCol)
{
    // Every square except the opening click and its neighbours is a candidate,
    // which guarantees the first reveal opens a blank area rather than a 1.
    std::vector<int> candidates;
    candidates.reserve(m_squares.size());
    for (int r = 0; r < m_height; ++r)
        for (int c = 0; c < m_width; ++c)
            if (std::abs(r - safeRow) > 1 || std::abs(c - safeCol) > 1)
                candidates.push_back(r * m_width + c);

    std::mt19937 rng { m_seed };
    std::shuffle(candidates.begin(), candidates.end(), rng);

    const int count = std::min<int>(m_mines, int(candidates.size()));
    for (int i = 0; i < count; ++i)
        m_squares[candidates[i]].mine = true;

    m_minesPlaced = true;
    countNeighbours();
}

void Minefield::countNeighbours()
{
    for (int r = 0; r < m_height; ++r) {
        for (int c = 0; c < m_width; ++c) {
            int n = 0;
            for (int dr = -1; dr <= 1; ++dr)
                for (int dc = -1; dc <= 1; ++dc)
                    if ((dr || dc) && inBounds(r + dr, c + dc) && at(r + dr, c + dc).mine)
                        ++n;
            square(r, c).neighbours = n;
        }
    }
}

void Minefield::reveal(int row, int col)
{
    if (m_state != State::Playing || !inBounds(row, col))
        return;

    const Square& s = at(row, col);
    if (s.revealed || s.flagged)
        return;

    if (!m_minesPlaced)
        placeMines(row, col);

    if (at(row, col).mine) {
        square(row, col).revealed = true;
        m_state = State::Lost;
        // Show the rest of the field, which is what makes a loss readable.
        for (Square& sq : m_squares)
            if (sq.mine)
                sq.revealed = true;
        return;
    }

    revealFrom(row, col);
    checkWin();
}

// Iterative flood fill — a recursive one overflows the stack on a large blank
// area at expert size.
void Minefield::revealFrom(int row, int col)
{
    std::vector<std::pair<int, int>> stack { { row, col } };

    while (!stack.empty()) {
        const auto [r, c] = stack.back();
        stack.pop_back();

        if (!inBounds(r, c))
            continue;
        Square& s = square(r, c);
        if (s.revealed || s.flagged || s.mine)
            continue;

        s.revealed = true;
        if (s.neighbours != 0)
            continue;

        for (int dr = -1; dr <= 1; ++dr)
            for (int dc = -1; dc <= 1; ++dc)
                if (dr || dc)
                    stack.emplace_back(r + dr, c + dc);
    }
}

void Minefield::toggleFlag(int row, int col)
{
    if (m_state != State::Playing || !inBounds(row, col) || at(row, col).revealed)
        return;
    square(row, col).flagged = !at(row, col).flagged;
}

void Minefield::chord(int row, int col)
{
    if (m_state != State::Playing || !inBounds(row, col))
        return;
    const Square& s = at(row, col);
    if (!s.revealed || s.neighbours == 0)
        return;

    int flags = 0;
    for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc)
            if ((dr || dc) && inBounds(row + dr, col + dc) && at(row + dr, col + dc).flagged)
                ++flags;

    if (flags != s.neighbours)
        return;

    for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc)
            if (dr || dc)
                reveal(row + dr, col + dc);
}

void Minefield::checkWin()
{
    for (const Square& s : m_squares)
        if (!s.mine && !s.revealed)
            return;
    m_state = State::Won;
}
