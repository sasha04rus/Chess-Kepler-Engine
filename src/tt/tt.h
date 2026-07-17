#pragma once

#include <atomic>
#include <cstdint>

#include "../moves/standard/move.h"

#define MAX_PLY 30

constexpr std::size_t TT_SIZE = 1ULL << 23;

enum TTFlag : std::uint8_t {EXACT, LOWERBOUND, UPPERBOUND};

struct TTEntry {
    std::uint64_t key = 0;
    int depth = -1;
    int score = 0;
    TTFlag flag = EXACT;
    Move best_move = NO_MOVE;
};

struct TTSlot {
    std::atomic<std::uint64_t> verification{0};
    std::atomic<std::uint64_t> data{0};
};

static_assert(std::atomic<std::uint64_t>::is_always_lock_free, "64-bit atomics are not lock-free on this platform");

extern TTSlot tt[TT_SIZE];
extern thread_local Move killers[MAX_PLY][2];

bool ProbeTT(std::uint64_t key, TTEntry& entry);
void StoreTT(const TTEntry& entry);

void ClearKiller();
void ClearTT();