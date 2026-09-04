#include "draughtsboard.h"

#include <algorithm>
#include <climits>
#include <random>

namespace {

// Men move towards the far side; Red starts at the bottom and moves up.
constexpr int kForward[2] = { -1, +1 };

int pieceValue(Piece p)
{
    switch (p) {
    case Piece::RedMan:    return 100;
    case Piece::RedKing:   return 175;
    case Piece::WhiteMan:  return -100;
    case Piece::WhiteKing: return -175;
    default:               return 0;
    }
}

} // namespace

bool belongsTo(Piece p, Side s)
{
    if (s == Side::Red)
        return p == Piece::RedMan || p == Piece::RedKing;
    return p == Piece::WhiteMan || p == Piece::WhiteKing;
}

bool isKing(Piece p)
{
    return p == Piece::RedKing || p == Piece::WhiteKing;
}

void DraughtsBoard::reset()
{
    m_cells.assign(kBoardCells, Piece::Empty);
    // Three rows each, on the dark squares only.
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < kBoardSize; ++col)
            if (isPlayable(row, col))
                set(row, col, Piece::WhiteMan);
    for (int row = kBoardSize - 3; row < kBoardSize; ++row)
        for (int col = 0; col < kBoardSize; ++col)
            if (isPlayable(row, col))
                set(row, col, Piece::RedMan);
}

bool DraughtsBoard::restore(const std::vector<Piece>& cells)
{
    if (cells.size() != std::size_t(kBoardCells))
        return false;

    int reds = 0;
    int whites = 0;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const Piece p = cells[std::size_t(row) * kBoardSize + std::size_t(col)];
            if (p < Piece::Empty || p > Piece::WhiteKing)
                return false;
            if (p == Piece::Empty)
                continue;
            if (!isPlayable(row, col)) // nothing ever stands on a light square
                return false;
            if (belongsTo(p, Side::Red))
                ++reds;
            else
                ++whites;
        }
    }
    // Twelve a side is the whole set, and a side with none has already lost.
    if (reds < 1 || reds > 12 || whites < 1 || whites > 12)
        return false;

    m_cells = cells;
    return true;
}

int DraughtsBoard::count(Side side) const
{
    int n = 0;
    for (Piece p : m_cells)
        if (belongsTo(p, side))
            ++n;
    return n;
}

void DraughtsBoard::collectJumps(Side side, Square from, Piece piece,
                                 const std::vector<Square>& steps,
                                 const std::vector<Square>& captured,
                                 const DraughtsBoard& working,
                                 std::vector<DraughtsMove>& out) const
{
    bool extended = false;

    for (int dc : { -1, 1 }) {
        for (int dr : { -1, 1 }) {
            // A man may only jump forwards; a king either way.
            if (!isKing(piece) && dr != kForward[int(side)])
                continue;

            const int overRow = from.row + dr;
            const int overCol = from.col + dc;
            const int landRow = from.row + 2 * dr;
            const int landCol = from.col + 2 * dc;

            if (!inBounds(landRow, landCol))
                continue;
            if (working.at(landRow, landCol) != Piece::Empty)
                continue;
            const Piece victim = working.at(overRow, overCol);
            if (victim == Piece::Empty || !belongsTo(victim, other(side)))
                continue;

            DraughtsBoard next = working;
            next.set(from.row, from.col, Piece::Empty);
            next.set(overRow, overCol, Piece::Empty);

            // Crowning ends the sequence: a man promoted mid-jump stops there.
            Piece moved = piece;
            const bool crowns = !isKing(piece)
                && landRow == (side == Side::Red ? 0 : kBoardSize - 1);
            if (crowns)
                moved = (side == Side::Red) ? Piece::RedKing : Piece::WhiteKing;
            next.set(landRow, landCol, moved);

            std::vector<Square> nextSteps = steps;
            nextSteps.push_back({ landRow, landCol });
            std::vector<Square> nextCaptured = captured;
            nextCaptured.push_back({ overRow, overCol });

            extended = true;
            if (crowns) {
                // Crowning ends the turn, so the chain stops here. The caller
                // rewrites `from` to the square the whole sequence started on.
                out.push_back({ from, nextSteps, nextCaptured });
                continue;
            }
            collectJumps(side, { landRow, landCol }, moved, nextSteps, nextCaptured, next, out);
        }
    }

    // No further jump from here: if we captured anything, this is a complete move.
    if (!extended && !captured.empty())
        out.push_back({ from, steps, captured });
}

std::vector<DraughtsMove> DraughtsBoard::legalMoves(Side side) const
{
    std::vector<DraughtsMove> captures;
    std::vector<DraughtsMove> quiet;

    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const Piece p = at(row, col);
            if (!belongsTo(p, side))
                continue;

            // Captures, walked to the end of every chain.
            std::vector<DraughtsMove> found;
            collectJumps(side, { row, col }, p, {}, {}, *this, found);
            for (DraughtsMove& m : found) {
                m.from = { row, col };
                captures.push_back(m);
            }

            // Simple steps.
            for (int dc : { -1, 1 }) {
                for (int dr : { -1, 1 }) {
                    if (!isKing(p) && dr != kForward[int(side)])
                        continue;
                    const int r = row + dr;
                    const int c = col + dc;
                    if (inBounds(r, c) && at(r, c) == Piece::Empty)
                        quiet.push_back({ { row, col }, { { r, c } }, {} });
                }
            }
        }
    }

    // Taking is compulsory.
    return captures.empty() ? quiet : captures;
}

void DraughtsBoard::apply(const DraughtsMove& move)
{
    Piece p = at(move.from.row, move.from.col);
    set(move.from.row, move.from.col, Piece::Empty);

    for (const Square& s : move.captured)
        set(s.row, s.col, Piece::Empty);

    const Square dest = move.destination();
    // Crown on reaching the far row.
    if (!isKing(p)) {
        if (p == Piece::RedMan && dest.row == 0)
            p = Piece::RedKing;
        else if (p == Piece::WhiteMan && dest.row == kBoardSize - 1)
            p = Piece::WhiteKing;
    }
    set(dest.row, dest.col, p);
}

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------

namespace {

int depthFor(DraughtsLevel level)
{
    switch (level) {
    case DraughtsLevel::Easy:   return 2;
    case DraughtsLevel::Medium: return 5;
    case DraughtsLevel::Hard:   return 8;
    }
    return 5;
}

// Positive favours Red.
int evaluate(const DraughtsBoard& b)
{
    int score = 0;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const Piece p = b.at(row, col);
            if (p == Piece::Empty)
                continue;
            score += pieceValue(p);

            // Advancement: a man is worth more the closer it is to crowning.
            if (p == Piece::RedMan)
                score += (kBoardSize - 1 - row) * 4;
            else if (p == Piece::WhiteMan)
                score -= row * 4;

            // The back row is worth holding, and the edges are safe squares.
            if (col == 0 || col == kBoardSize - 1)
                score += belongsTo(p, Side::Red) ? 6 : -6;
        }
    }
    return score;
}

int negamax(const DraughtsBoard& board, Side side, int depth, int alpha, int beta)
{
    const std::vector<DraughtsMove> moves = board.legalMoves(side);
    if (moves.empty()) {
        // No move at all is a loss for the side to play.
        return -100000 - depth;
    }
    if (depth <= 0) {
        const int score = evaluate(board);
        return side == Side::Red ? score : -score;
    }

    int best = INT_MIN + 1;
    for (const DraughtsMove& m : moves) {
        DraughtsBoard next = board;
        next.apply(m);
        best = std::max(best, -negamax(next, other(side), depth - 1, -beta, -alpha));
        alpha = std::max(alpha, best);
        if (alpha >= beta)
            break;
    }
    return best;
}

} // namespace

bool chooseDraughtsMove(const DraughtsBoard& board, Side side, DraughtsLevel level,
                        DraughtsMove& out)
{
    const std::vector<DraughtsMove> moves = board.legalMoves(side);
    if (moves.empty())
        return false;

    const int depth = depthFor(level);
    int best = INT_MIN + 1;
    std::vector<DraughtsMove> bestMoves;

    for (const DraughtsMove& m : moves) {
        DraughtsBoard next = board;
        next.apply(m);
        const int score = -negamax(next, other(side), depth - 1, INT_MIN + 1, INT_MAX - 1);
        if (score > best) {
            best = score;
            bestMoves = { m };
        } else if (score == best) {
            bestMoves.push_back(m);
        }
    }

    // thread_local, not merely static (GHUB-0047). The search now runs on a
    // worker thread, and a search that has been abandoned keeps running until
    // it finishes -- so two threads can be inside this function at once. C++
    // guarantees thread-safe INITIALISATION of a function-local static and
    // nothing about using one, so a plain static here is a data race and
    // undefined behaviour. Per-thread seeding is no loss: this only breaks ties
    // between equally good moves.
    static thread_local std::mt19937 rng { std::random_device {}() };
    std::uniform_int_distribution<std::size_t> pick(0, bestMoves.size() - 1);
    out = bestMoves[pick(rng)];
    return true;
}
