#include "history.h"

#include <algorithm>
#include <cstring>

thread_local int history_table[2][64][64]{};

int GetHistoryScore(int side, const Move& move) {
    return history_table[side][move.from][move.to];
}

void UpdateHistory(int side, const Move& move, int bonus) {
    bonus = std::clamp(bonus, -HISTORY_LIMIT, HISTORY_LIMIT);
    int& value = history_table[side][move.from][move.to];
    const int absolute_bonus = bonus >= 0 ? bonus : -bonus;
    value += bonus - value * absolute_bonus / HISTORY_LIMIT;
    value = std::clamp(value, -HISTORY_LIMIT, HISTORY_LIMIT);
}

int HistoryBonus(int depth) {
    return std::min(4000, 16 * depth * depth);
}

void ClearHistory() {
    std::memset(history_table, 0, sizeof(history_table));
}