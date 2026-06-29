#include "tt.h"

#include <cstring>

std::atomic<TTEntry> tt[TT_SIZE];
thread_local Move killers[MAX_PLY][2];

void ClearKiller() { 
    for (int i = 0; i < MAX_PLY; i++) {
        killers[i][0] = NO_MOVE;
        killers[i][1] = NO_MOVE;
    }
}

void ClearTT() {
    for (size_t i = 0; i < TT_SIZE; i++) {
        tt[i].store(TTEntry{}, std::memory_order_relaxed);
    }
}