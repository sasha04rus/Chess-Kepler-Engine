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