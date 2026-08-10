#pragma once

#include "board.h"

#include <optional>

enum class Difficulty { Easy, Medium, Hard };

// Best move for p, or nullopt when p has to pass.
std::optional<Move> chooseMove(const Board& b, Player p, Difficulty d);
