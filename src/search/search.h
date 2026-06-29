#pragma once

#include <atomic>

#include "../board/board.h"

#define INFINITY 15000
#define MATE_VALUE 14000
#define MATE_THRESHOLD 13000  

template <typename MoveType>
struct ThreadResult {
    MoveType best_move;
    int depth = 0;
    int score = 0;
    bool finished = false;
};

template <typename MoveType>
struct SearchArgs {
    Board board;
    std::uint8_t multi_pv = 1;
    int thread_id = 0;
    ThreadResult<MoveType>* result = nullptr;

    SearchArgs(const Board& _board) : board(_board) {}
};

template <typename MoveType>
void* Search(void* args);

extern struct timespec start_time;
extern std::atomic<bool> stop_signal;
extern std::atomic<bool> searching;
extern int threads;
extern int movetime;
extern int move_number;

bool IsInterrupted();