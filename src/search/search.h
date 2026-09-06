#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <iostream>

#include "../board/board.h"
#include "../moves/pv.h"
#include "../engine/engine_types.h"

#define MATE_VALUE 14000
#define MATE_THRESHOLD 13000  

extern struct timespec start_time;
extern std::atomic<bool> stop_signal;
extern std::atomic<bool> searching;
extern std::string bestmove;
extern int movetime;

struct SearchArgs {
    Board board;
    int max_depth;
    int threads;
    int multi_pv;
    InfoCallback info_callback;
    BestMoveCallback best_move_callback;
};

template <typename MoveType>
struct SearchResult {
    int evaluation;
    PrincipalVariation<MoveType> pv;
    std::uint64_t nodes;
    bool valid = false;
};

template <typename MoveType>
struct RootResult {
    MoveType root_move;
    int evaluation;
    PrincipalVariation<MoveType> pv;
    std::uint64_t nodes;
    bool valid = false;
};

template <typename MoveType>
void* Search(void* args);

std::uint64_t GetElapsedMilliseconds();

bool IsInterrupted();