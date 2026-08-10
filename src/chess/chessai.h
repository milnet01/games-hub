#pragma once

#include "chessboard.h"

// The opponent. Negamax with alpha-beta over copied boards, the same shape as
// Reversi and Draughts use — a chess board is 128 bytes, so copying one per
// node stays cheaper than writing an unmake.
namespace chess {

enum class Level { Easy, Medium, Hard };

// Material plus piece-square tables, in centipawns and always from White's
// point of view.
int evaluate(const Board& board);

// Fixed-depth search for the side to move. Deterministic, so the self-test can
// assert on what it finds. `score` is from the searching side's point of view.
bool searchBestMove(const Board& board, int depth, Move& out, int* score = nullptr);

// Picks the move actually played in the game. Deepens until it runs out of its
// node budget, then chooses among the moves the level treats as good enough —
// Easy tolerates a real mistake, Hard takes the best it found.
bool chooseMove(const Board& board, Level level, Move& out);

} // namespace chess
