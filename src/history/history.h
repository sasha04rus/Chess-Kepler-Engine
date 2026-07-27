#pragma once

#include "../moves/standard/move.h"

constexpr int HISTORY_LIMIT = 16384;

extern thread_local int history_table[2][64][64];

int GetHistoryScore(int side, const Move& move);
void UpdateHistory(int side, const Move& move, int bonus);
int HistoryBonus(int depth);
void ClearHistory();