#include <atomic>
#include <cstdint>

#include "../moves//standard/move.h"

#define MAX_PLY 30

constexpr int TT_SIZE = 1 << 23;

enum TTFlag { EXACT, LOWERBOUND, UPPERBOUND };

struct TTEntry {
  std::uint64_t key = 0;
  int depth = -1;
  int score = 0;
  TTFlag flag = EXACT;
  Move best_move = NO_MOVE;
};

extern std::atomic<TTEntry> tt[TT_SIZE];
extern thread_local Move killers[MAX_PLY][2];

void ClearKiller();
void ClearTT();

inline void StoreTT(const TTEntry& new_entry) {
    auto& slot = tt[new_entry.key & (TT_SIZE - 1)];
    TTEntry old_entry = slot.load(std::memory_order_relaxed);
    if (old_entry.key == 0 || old_entry.key != new_entry.key || new_entry.depth >= old_entry.depth || new_entry.flag == EXACT)
        slot.store(new_entry, std::memory_order_relaxed);
}