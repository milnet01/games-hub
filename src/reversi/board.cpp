#include "board.h"

namespace {
constexpr int kDirs[8][2] = {
    { -1, -1 }, { -1, 0 }, { -1, 1 },
    { 0, -1 },             { 0, 1 },
    { 1, -1 },  { 1, 0 },  { 1, 1 },
};
}

void Board::reset()
{
    m_cells.fill(Cell::Empty);
    set(3, 3, Cell::White);
    set(3, 4, Cell::Black);
    set(4, 3, Cell::Black);
    set(4, 4, Cell::White);
}

int Board::ray(Player p, Move m, int dr, int dc) const
{
    const Cell mine = static_cast<Cell>(p);
    int r = m.row + dr;
    int c = m.col + dc;
    int n = 0;

    while (inBounds(r, c) && at(r, c) != Cell::Empty && at(r, c) != mine) {
        ++n;
        r += dr;
        c += dc;
    }

    // The run must be non-empty and closed off by one of our own discs.
    if (n == 0 || !inBounds(r, c) || at(r, c) != mine)
        return 0;
    return n;
}

int Board::flipCount(Player p, Move m) const
{
    if (!inBounds(m.row, m.col) || at(m.row, m.col) != Cell::Empty)
        return 0;

    int total = 0;
    for (const auto& d : kDirs)
        total += ray(p, m, d[0], d[1]);
    return total;
}

int Board::play(Player p, Move m)
{
    if (!inBounds(m.row, m.col) || at(m.row, m.col) != Cell::Empty)
        return 0;

    const Cell mine = static_cast<Cell>(p);
    int total = 0;

    for (const auto& d : kDirs) {
        const int n = ray(p, m, d[0], d[1]);
        for (int i = 1; i <= n; ++i)
            set(m.row + d[0] * i, m.col + d[1] * i, mine);
        total += n;
    }

    if (total > 0)
        set(m.row, m.col, mine);
    return total;
}

std::vector<Move> Board::legalMoves(Player p) const
{
    std::vector<Move> moves;
    moves.reserve(16);
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c)
            if (flipCount(p, { r, c }) > 0)
                moves.push_back({ r, c });
    return moves;
}

int Board::count(Player p) const
{
    const Cell mine = static_cast<Cell>(p);
    int n = 0;
    for (Cell c : m_cells)
        if (c == mine)
            ++n;
    return n;
}

int Board::emptyCount() const
{
    int n = 0;
    for (Cell c : m_cells)
        if (c == Cell::Empty)
            ++n;
    return n;
}
