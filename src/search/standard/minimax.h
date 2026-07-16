#pragma once

#include "../../board/board.h"
#include "../../moves/pv.h"

#define MAX_MOVES 218

int Minimax(Board& board, std::uint8_t depth, int alpha, int beta, PrincipalVariation<Move>& pv, std::uint64_t& nodes, int ply);