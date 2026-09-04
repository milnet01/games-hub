#include "chessai.h"

#include <algorithm>
#include <cstdlib>
#include <random>
#include <utility>

namespace chess {

namespace {

constexpr int kMate = 100000;
constexpr int kMaxPly = 48;

constexpr int kPieceValue[] = { 0, 100, 320, 330, 500, 900, 0 };

// Piece-square tables, written from White's side of the board: row 0 is rank
// 8, so the last row of each table is White's back rank. Black reads the same
// table with the rows mirrored. The numbers are positional taste in
// centipawns — centre good, rim poor, pawns worth more the further they get.
constexpr int kPawnTable[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     60,  60,  60,  60,  60,  60,  60,  60,
     20,  20,  30,  40,  40,  30,  20,  20,
      8,  10,  18,  28,  28,  18,  10,   8,
      4,   6,  10,  22,  22,  10,   6,   4,
      4,   0,   0,   8,   8,   0,   0,   4,
      4,   8,   8, -16, -16,   8,   8,   4,
      0,   0,   0,   0,   0,   0,   0,   0,
};
constexpr int kKnightTable[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -30,   0,  12,  16,  16,  12,   0, -30,
    -30,   4,  16,  20,  20,  16,   4, -30,
    -30,   0,  16,  20,  20,  16,   0, -30,
    -30,   4,  12,  16,  16,  12,   4, -30,
    -40, -20,   0,   4,   4,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50,
};
constexpr int kBishopTable[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   6,  12,  12,   6,   0, -10,
    -10,   6,   6,  12,  12,   6,   6, -10,
    -10,   0,  12,  12,  12,  12,   0, -10,
    -10,  12,  12,  12,  12,  12,  12, -10,
    -10,   6,   0,   0,   0,   0,   6, -10,
    -20, -10, -10, -10, -10, -10, -10, -20,
};
constexpr int kRookTable[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     10,  20,  20,  20,  20,  20,  20,  10,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
      0,   0,   4,  10,  10,   4,   0,   0,
};
constexpr int kQueenTable[64] = {
    -20, -10, -10,  -5,  -5, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
     -5,   0,   5,   5,   5,   5,   0,  -5,
      0,   0,   5,   5,   5,   5,   0,  -5,
    -10,   5,   5,   5,   5,   5,   0, -10,
    -10,   0,   5,   0,   0,   0,   0, -10,
    -20, -10, -10,  -5,  -5, -10, -10, -20,
};
constexpr int kKingMiddleTable[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
     20,  20,   0,   0,   0,   0,  20,  20,
     20,  30,  10,   0,   0,  10,  30,  20,
};
constexpr int kKingEndTable[64] = {
    -50, -30, -30, -30, -30, -30, -30, -50,
    -30, -30,   0,   0,   0,   0, -30, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -20, -10,   0,   0, -10, -20, -30,
    -50, -40, -30, -20, -20, -30, -40, -50,
};

const int* tableFor(PieceType type, bool endgame)
{
    switch (type) {
    case PieceType::Pawn:   return kPawnTable;
    case PieceType::Knight: return kKnightTable;
    case PieceType::Bishop: return kBishopTable;
    case PieceType::Rook:   return kRookTable;
    case PieceType::Queen:  return kQueenTable;
    case PieceType::King:   return endgame ? kKingEndTable : kKingMiddleTable;
    case PieceType::None:   return nullptr;
    }
    return nullptr;
}

// How far a square is from the middle of the board, 2 in the centre and 8 in a
// corner. Used only to hunt down a lone king.
int edgeDistance(Square s)
{
    return std::max(std::abs(s.row - 3), std::abs(s.row - 4))
        + std::max(std::abs(s.col - 3), std::abs(s.col - 4));
}

int value(PieceType t) { return kPieceValue[int(t)]; }

// Captures first, biggest victim by cheapest attacker, then promotions.
int moveOrderScore(const Board& b, const Move& m)
{
    int score = 0;
    const Piece victim = b.at(m.to);
    if (!victim.empty())
        score += 100 * value(victim.type) - value(b.at(m.from).type);
    if (m.enPassant)
        score += 100 * value(PieceType::Pawn);
    if (m.promotion != PieceType::None)
        score += 90000 + value(m.promotion);
    return score;
}

void orderMoves(const Board& b, std::vector<Move>& moves)
{
    std::stable_sort(moves.begin(), moves.end(), [&b](const Move& x, const Move& y) {
        return moveOrderScore(b, x) > moveOrderScore(b, y);
    });
}

bool isNoisy(const Board& b, const Move& m)
{
    return !b.at(m.to).empty() || m.enPassant || m.promotion != PieceType::None;
}

class Search
{
public:
    explicit Search(long long budget) : m_budget(budget) { }

    bool exhausted() const { return m_budget > 0 && m_nodes >= m_budget; }
    long long nodes() const { return m_nodes; }

    int negamax(const Board& board, int depth, int alpha, int beta, int ply)
    {
        ++m_nodes;
        const bool checked = board.inCheck();
        // A check is cheap to extend and is where nearly all the tactics are.
        if (checked && ply < kMaxPly)
            ++depth;

        if (depth <= 0)
            return quiesce(board, alpha, beta, ply);

        std::vector<Move> moves = board.legalMoves();
        if (moves.empty())
            return checked ? -(kMate - ply) : 0;
        if (ply >= kMaxPly || exhausted())
            return sideToMoveScore(board);

        orderMoves(board, moves);

        int best = -kMate * 2;
        for (const Move& m : moves) {
            Board next = board;
            next.apply(m);
            best = std::max(best, -negamax(next, depth - 1, -beta, -alpha, ply + 1));
            alpha = std::max(alpha, best);
            if (alpha >= beta)
                break;
        }
        return best;
    }

private:
    static int sideToMoveScore(const Board& board)
    {
        const int score = evaluate(board);
        return board.toMove() == Colour::White ? score : -score;
    }

    // Plays out the captures so the search never stops in the middle of an
    // exchange and calls the half-finished trade a win.
    int quiesce(const Board& board, int alpha, int beta, int ply)
    {
        ++m_nodes;
        if (ply >= kMaxPly || exhausted())
            return sideToMoveScore(board);

        const bool checked = board.inCheck();

        // In check, every legal move is a candidate -- and the full list is also
        // what tells mate from a way out of it, so it is worth generating here.
        std::vector<Move> moves;
        int best = 0;
        if (checked) {
            moves = board.legalMoves();
            if (moves.empty())
                return -(kMate - ply);
            best = -kMate * 2;
        } else {
            const int stand = sideToMoveScore(board);
            if (stand >= beta)
                return stand;
            alpha = std::max(alpha, stand);

            // Out of check, only the moves that change material are worth
            // looking at -- and they are GENERATED as such rather than filtered
            // out of the full list afterwards. legalMoves() copies the board and
            // runs inCheck() for every pseudo-legal move, so the old shape paid
            // for about thirty-five moves to keep about five, at every quiet
            // node, inside a fixed node budget.
            //
            // The one thing this gives up is stalemate detection at a quiet
            // leaf: with no captures we return the evaluation rather than nought.
            // Deliberate, and the standard trade -- proving stalemate needs the
            // full generation this exists to avoid, and the main search still
            // detects mate and stalemate at every node above a leaf.
            moves = board.legalCaptures();
            if (moves.empty())
                return stand;
            best = stand;
        }

        orderMoves(board, moves);
        for (const Move& m : moves) {
            Board next = board;
            next.apply(m);
            best = std::max(best, -quiesce(next, -beta, -alpha, ply + 1));
            alpha = std::max(alpha, best);
            if (alpha >= beta)
                break;
        }
        return best;
    }

    long long m_nodes = 0;
    long long m_budget = 0;
};

// One root pass: every legal move scored, best first.
//
// `completed` comes back false when the node budget ran out part way, which
// makes the scores unsafe to act on — the caller keeps the last full iteration
// instead.
//
// `exact` searches every move on a full window. Without it, alpha-beta only
// guarantees the winning move's score: everything else comes back as an upper
// bound that can sit exactly level with the best, so a level that picks among
// near-equal moves would happily play one that is nothing of the sort. The
// best move itself is tracked as it is found, so it stays right either way.
std::vector<std::pair<Move, int>> rootScores(const Board& board, int depth, long long budget,
                                             const std::vector<Move>& preferred, bool exact = false,
                                             bool* completed = nullptr)
{
    std::vector<Move> moves = board.legalMoves();
    if (moves.empty())
        return {};

    orderMoves(board, moves);
    // Last iteration's ordering beats a static one, and better ordering is the
    // whole reason iterative deepening pays for itself.
    if (!preferred.empty()) {
        std::stable_sort(moves.begin(), moves.end(), [&preferred](const Move& x, const Move& y) {
            const auto rank = [&preferred](const Move& m) {
                const auto it = std::find(preferred.begin(), preferred.end(), m);
                return it == preferred.end() ? preferred.size() : std::size_t(it - preferred.begin());
            };
            return rank(x) < rank(y);
        });
    }

    Search search(budget);
    std::vector<std::pair<Move, int>> scored;
    scored.reserve(moves.size());

    int alpha = -kMate * 2;
    Move bestMove = moves.front();
    for (const Move& m : moves) {
        Board next = board;
        next.apply(m);
        const int childBeta = exact ? kMate * 2 : -alpha;
        const int score = -search.negamax(next, depth - 1, -kMate * 2, childBeta, 1);
        scored.emplace_back(m, score);
        if (score > alpha) {
            alpha = score;
            bestMove = m;
        }
    }

    if (completed)
        *completed = !search.exhausted();

    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
    // Ties are only meaningful when every score is exact; otherwise the move
    // that actually raised alpha is the one to play.
    const auto it = std::find_if(scored.begin(), scored.end(),
                                 [&bestMove](const auto& e) { return e.first == bestMove; });
    if (it != scored.end())
        std::rotate(scored.begin(), it, it + 1);
    return scored;
}

struct LevelPlan {
    int depth;
    long long budget;
    int slack;   // how far below the best move the engine will still play
};

LevelPlan planFor(Level level)
{
    switch (level) {
    // The budgets are node counts, tuned so the worst middlegame position
    // still answers inside about a second. The search runs on a worker since
    // GHUB-0047, so overrunning no longer stops the window repainting -- what
    // it costs is the player sitting waiting for a reply, which is the thing
    // these numbers are protecting.
    case Level::Easy:   return { 2, 30000, 70 };
    case Level::Medium: return { 3, 120000, 20 };
    case Level::Hard:   return { 5, 200000, 0 };
    }
    return { 3, 120000, 20 };
}

} // namespace

int evaluate(const Board& board)
{
    int score = 0;
    const int nonPawn = board.material(Colour::White) + board.material(Colour::Black)
        - (board.pieceCount(Colour::White) + board.pieceCount(Colour::Black));
    // Once roughly a queen and a rook have come off, kings belong in the
    // middle rather than behind a wall of pawns that no longer exists.
    const bool endgame = nonPawn <= 26;

    for (int row = 0; row < kRanks; ++row) {
        for (int col = 0; col < kFiles; ++col) {
            const Piece p = board.at(row, col);
            if (p.empty())
                continue;
            const int* table = tableFor(p.type, endgame);
            const int index = p.colour == Colour::White ? row * kFiles + col
                                                        : (kRanks - 1 - row) * kFiles + col;
            const int worth = value(p.type) + table[index];
            score += p.colour == Colour::White ? worth : -worth;
        }
    }

    if (endgame) {
        const int lead = board.material(Colour::White) * 100 - board.material(Colour::Black) * 100;
        if (std::abs(lead) >= 300) {
            const Colour strong = lead > 0 ? Colour::White : Colour::Black;
            const Square hunter = board.kingSquare(strong);
            const Square prey = board.kingSquare(other(strong));
            if (hunter.valid() && prey.valid()) {
                // Push the bare king to the rim and keep our own king near it,
                // otherwise a won endgame shuffles until the fifty-move rule.
                const int closeness =
                    14 - (std::abs(hunter.row - prey.row) + std::abs(hunter.col - prey.col));
                const int bonus = edgeDistance(prey) * 10 + closeness * 4;
                score += strong == Colour::White ? bonus : -bonus;
            }
        }
    }

    return score;
}

bool searchBestMove(const Board& board, int depth, Move& out, int* score)
{
    // The narrow window is enough here: alpha-beta always resolves the winning
    // move and its score exactly, and this entry point exposes nothing else.
    const std::vector<std::pair<Move, int>> scored = rootScores(board, depth, 0, {});
    if (scored.empty())
        return false;
    out = scored.front().first;
    if (score)
        *score = scored.front().second;
    return true;
}

bool chooseMove(const Board& board, Level level, Move& out)
{
    const LevelPlan plan = planFor(level);
    // Only a level that plays something other than the top move needs every
    // score to be trustworthy, and those levels are the shallow ones, so the
    // wider window costs nothing that matters.
    const bool exact = plan.slack > 0;

    std::vector<std::pair<Move, int>> best;
    std::vector<Move> preferred;
    for (int depth = 1; depth <= plan.depth; ++depth) {
        bool completed = false;
        std::vector<std::pair<Move, int>> scored =
            rootScores(board, depth, plan.budget, preferred, exact, &completed);
        if (scored.empty())
            return false;
        if (!completed && !best.empty())
            break;   // Out of budget: the shallower answer is the trustworthy one.

        preferred.clear();
        for (const auto& entry : scored)
            preferred.push_back(entry.first);
        best = std::move(scored);

        if (!completed)
            break;
        // A forced mate is not going to improve by looking further.
        if (best.front().second >= kMate - kMaxPly)
            break;
    }
    if (best.empty())
        return false;

    // Everything within the level's slack of the best move is playable, and
    // choosing among them keeps openings from repeating game after game.
    std::vector<Move> candidates { best.front().first };
    const int cutoff = best.front().second - plan.slack;
    for (std::size_t i = 1; plan.slack > 0 && i < best.size(); ++i)
        if (best[i].second >= cutoff)
            candidates.push_back(best[i].first);

    // thread_local, not merely static (GHUB-0047). The search now runs on a
    // worker thread, and a search that has been abandoned keeps running until
    // it finishes -- so two threads can be inside this function at once. C++
    // guarantees thread-safe INITIALISATION of a function-local static and
    // nothing about using one, so a plain static here is a data race and
    // undefined behaviour. Per-thread seeding is no loss: this only breaks ties
    // between equally good moves.
    static thread_local std::mt19937 rng { std::random_device {}() };
    std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
    out = candidates[pick(rng)];
    return true;
}

} // namespace chess
