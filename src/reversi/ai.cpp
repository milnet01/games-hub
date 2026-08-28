#include "ai.h"

#include <algorithm>
#include <climits>
#include <random>

namespace {

// Classic Reversi square values: corners are permanent, the squares next to
// them hand a corner to the opponent, so they are heavily penalised.
constexpr int kWeights[kCells] = {
    120, -20, 20,  5,  5, 20, -20, 120,
    -20, -40, -5, -5, -5, -5, -40, -20,
     20,  -5, 15,  3,  3, 15,  -5,  20,
      5,  -5,  3,  3,  3,  3,  -5,   5,
      5,  -5,  3,  3,  3,  3,  -5,   5,
     20,  -5, 15,  3,  3, 15,  -5,  20,
    -20, -40, -5, -5, -5, -5, -40, -20,
    120, -20, 20,  5,  5, 20, -20, 120,
};

int searchDepth(Difficulty d)
{
    switch (d) {
    case Difficulty::Easy:   return 1;
    case Difficulty::Medium: return 4;
    case Difficulty::Hard:   return 6;
    }
    return 4;
}

// Score of the position from p's point of view.
int evaluate(const Board& b, Player p)
{
    const Player o = opponent(p);

    // Endgame: nothing matters but the final disc count.
    if (b.emptyCount() <= 10)
        return (b.count(p) - b.count(o)) * 100;

    int positional = 0;
    for (int r = 0; r < kSize; ++r) {
        for (int c = 0; c < kSize; ++c) {
            const Cell cell = b.at(r, c);
            if (cell == Cell::Empty)
                continue;
            const int w = kWeights[r * kSize + c];
            positional += (cell == static_cast<Cell>(p)) ? w : -w;
        }
    }

    // Mobility: keeping options open matters more than owning discs early on.
    const int mine = static_cast<int>(b.legalMoves(p).size());
    const int theirs = static_cast<int>(b.legalMoves(o).size());
    const int mobility = (mine + theirs) ? 100 * (mine - theirs) / (mine + theirs) : 0;

    return positional + mobility * 8;
}

int negamax(const Board& b, Player p, int depth, int alpha, int beta)
{
    if (depth <= 0)
        return evaluate(b, p);

    std::vector<Move> moves = b.legalMoves(p);
    if (moves.empty()) {
        // No move available: pass, unless the opponent is stuck too — then the
        // game is over and only the disc count counts. Depth still decreases so
        // a pass chain cannot recurse forever.
        if (b.legalMoves(opponent(p)).empty())
            return (b.count(p) - b.count(opponent(p))) * 1000;
        return -negamax(b, opponent(p), depth - 1, -beta, -alpha);
    }

    // Try promising squares first so alpha-beta prunes more of the tree.
    std::sort(moves.begin(), moves.end(), [](Move a, Move c) {
        return kWeights[a.row * kSize + a.col] > kWeights[c.row * kSize + c.col];
    });

    int best = INT_MIN + 1;
    for (Move m : moves) {
        Board next = b;
        next.play(p, m);
        best = std::max(best, -negamax(next, opponent(p), depth - 1, -beta, -alpha));
        alpha = std::max(alpha, best);
        if (alpha >= beta)
            break;
    }
    return best;
}

} // namespace

std::optional<Move> chooseMove(const Board& b, Player p, Difficulty d)
{
    const std::vector<Move> moves = b.legalMoves(p);
    if (moves.empty())
        return std::nullopt;

    const int depth = searchDepth(d);
    int best = INT_MIN + 1;
    std::vector<Move> bestMoves;

    for (Move m : moves) {
        Board next = b;
        next.play(p, m);
        const int score = -negamax(next, opponent(p), depth - 1, INT_MIN + 1, INT_MAX - 1);
        if (score > best) {
            best = score;
            bestMoves = { m };
        } else if (score == best) {
            bestMoves.push_back(m);
        }
    }

    // Pick randomly between equally-good moves so repeated games differ.
    // thread_local, not merely static (GHUB-0047). The search now runs on a
    // worker thread, and a search that has been abandoned keeps running until
    // it finishes -- so two threads can be inside this function at once. C++
    // guarantees thread-safe INITIALISATION of a function-local static and
    // nothing about using one, so a plain static here is a data race and
    // undefined behaviour. Per-thread seeding is no loss: this only breaks ties
    // between equally good moves.
    static thread_local std::mt19937 rng { std::random_device {}() };
    std::uniform_int_distribution<std::size_t> pick(0, bestMoves.size() - 1);
    return bestMoves[pick(rng)];
}
