#include "noughtsboard.h"

#include <algorithm>

namespace noughts {

bool Board::play(int cell, Mark mark)
{
    if (!inBounds(cell) || mark == Mark::Empty || m_cells[cell] != Mark::Empty || over())
        return false;
    m_cells[cell] = mark;
    return true;
}

std::optional<Line> Board::winningLine() const
{
    for (const Line& line : kLines) {
        const Mark m = m_cells[line[0]];
        if (m != Mark::Empty && m_cells[line[1]] == m && m_cells[line[2]] == m)
            return line;
    }
    return std::nullopt;
}

Mark Board::winner() const
{
    const std::optional<Line> line = winningLine();
    return line ? m_cells[(*line)[0]] : Mark::Empty;
}

int Board::count(Mark mark) const
{
    return int(std::count(m_cells.begin(), m_cells.end(), mark));
}

std::vector<int> Board::emptyCells() const
{
    std::vector<int> cells;
    for (int i = 0; i < kCells; ++i)
        if (m_cells[i] == Mark::Empty)
            cells.push_back(i);
    return cells;
}

Mark Board::toMove(Mark starter) const
{
    return count(starter) == count(other(starter)) ? starter : other(starter);
}

bool Board::restore(const std::array<Mark, kCells>& cells, Mark starter)
{
    if (starter != Mark::Cross && starter != Mark::Nought)
        return false;
    Board b;
    for (int i = 0; i < kCells; ++i) {
        if (cells[i] != Mark::Empty && cells[i] != Mark::Cross && cells[i] != Mark::Nought)
            return false;
        b.m_cells[i] = cells[i];
    }
    const int lead = b.count(starter) - b.count(other(starter));
    if (lead != 0 && lead != 1)
        return false;

    bool crossLine = false;
    bool noughtLine = false;
    for (const Line& line : kLines) {
        const Mark m = cells[line[0]];
        if (m != Mark::Empty && cells[line[1]] == m && cells[line[2]] == m)
            (m == Mark::Cross ? crossLine : noughtLine) = true;
    }
    // The game stops at the first three in a row, so both sides cannot have
    // one, and the winner made the last move: the starter leads by one after
    // its own win and is level after the other side's.
    if (crossLine && noughtLine)
        return false;
    const Mark won = crossLine ? Mark::Cross : (noughtLine ? Mark::Nought : Mark::Empty);
    if (won != Mark::Empty && (lead == 1) != (won == starter))
        return false;

    m_cells = b.m_cells;
    return true;
}

namespace {

// The position's worth to `mover`, who is about to play. A win is worth more
// the sooner it comes and a loss costs less the later it comes, so perfect
// play takes a win at once and makes the other side work for one. Board is
// nine bytes, so each move is tried on a copy; alpha-beta over a tree this
// small needs no table.
int value(const Board& board, Mark mover, int alpha, int beta)
{
    if (board.winner() != Mark::Empty)
        return -(20 - board.marksPlaced()); // the side that just moved has won
    if (board.full())
        return 0;
    int best = -100;
    for (int cell : board.emptyCells()) {
        Board next = board;
        next.play(cell, mover);
        const int v = -value(next, other(mover), -beta, -alpha);
        best = std::max(best, v);
        alpha = std::max(alpha, v);
        if (alpha >= beta)
            break;
    }
    return best;
}

} // namespace

std::vector<int> bestMoves(const Board& board, Mark mover)
{
    std::vector<int> moves;
    if (board.over())
        return moves;
    int best = -100;
    for (int cell : board.emptyCells()) {
        Board next = board;
        next.play(cell, mover);
        // A full window for every move, so each one's worth is exact and a
        // tie is a real tie rather than a cut-off.
        const int v = -value(next, other(mover), -100, 100);
        if (v > best) {
            best = v;
            moves.clear();
        }
        if (v == best)
            moves.push_back(cell);
    }
    return moves;
}

int chooseMove(const Board& board, Mark mover, Level level, std::mt19937& rng)
{
    if (board.over())
        return -1;
    if (level == Level::Easy && rng() % 2 == 0) {
        const std::vector<int> any = board.emptyCells();
        return any[rng() % any.size()];
    }
    const std::vector<int> best = bestMoves(board, mover);
    return best[rng() % best.size()];
}

} // namespace noughts
