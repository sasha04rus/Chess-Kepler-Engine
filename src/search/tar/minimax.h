#pragma once

#include "../../board/board.h"
#include "../../moves/pv.h"

#define MAX_MOVES 218

namespace set_history {
 
extern thread_local int table[2][5][64];
void ClearSetHistory();

} // set_history

int Minimax(Board& board, int depth, int alpha, int beta, PrincipalVariation<MoveTar>& pv, std::uint64_t& nodes, int ply, bool allow_null = true);