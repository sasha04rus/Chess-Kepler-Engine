#include "../search/search.h"

#include <ctime>
#include <cstring>
#include <iostream>
#include <cstdint>

#include "../board/board.h"
#include "../moves/standard/move.h"
#include "../moves/tar/move.h"
#include "../moves/pv.h"
#include "../tt/tt.h"

#define MAX_DEPTH 30
#define INFINITY 15000
#define MATE_VALUE 14000
#define MATE_THRESHOLD 13000  

struct timespec start_time;
std::atomic<bool> stop_signal{false};
std::atomic<bool> searching{false};
int threads = 8;
int movetime = 0;
int move_number = 0;

static constexpr std::int64_t NANOSECONDS_PER_MILLISECOND = 1000000;
static constexpr std::int64_t NANOSECONDS_PER_SECOND = 1000000000;

bool IsInterrupted() {
    if (stop_signal) return true;
    if (movetime <= 0) return false;
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    int64_t elapsed_ns = (int64_t)(current_time.tv_sec - start_time.tv_sec) * NANOSECONDS_PER_SECOND + (int64_t)(current_time.tv_nsec - start_time.tv_nsec);
    return elapsed_ns / NANOSECONDS_PER_MILLISECOND >= (int64_t)movetime;
}

template <typename MoveType>
void* Search(void* args) {
    SearchArgs<MoveType>* search_args = static_cast<SearchArgs<MoveType>*>(args);;
    ThreadResult<MoveType>* result = search_args->result;
    int depth;
    ClearKiller();
    std::uint64_t total_nodes = 0;
    MoveType best_move(0, 0, 0, 0, Flag::kDefault);
    if (search_args->multi_pv == 1) {
        for (depth = 1; !stop_signal && depth <= MAX_DEPTH; depth++) {
            std::uint64_t nodes = 0;
            PrincipalVariation<MoveType> pv;
            Board board = search_args->board;
            int evaluation = Minimax(board, depth, -INFINITY, INFINITY, pv, nodes, 0);
            total_nodes += nodes;
            if (IsInterrupted() || stop_signal) break;
            if (stop_signal) break;
            if (pv.length > 0) {
                best_move = pv.moves[0];
                result->best_move = best_move;
                result->depth = depth;
                result->score = evaluation;
            }
            if (search_args->thread_id == 0) {
                struct timespec current_time;
                clock_gettime(CLOCK_MONOTONIC, &current_time);
                int elapsed_time = (current_time.tv_sec - start_time.tv_sec) * 1000 + (current_time.tv_nsec - start_time.tv_nsec) / 1000000;
                if (evaluation > MATE_THRESHOLD) {
                    int mate_in = (MATE_VALUE - evaluation + 1) / 2;
                    std::cout << "info depth " << depth << " score mate " << mate_in << " time " << elapsed_time << " nodes " << total_nodes << " nps " << (nodes/(elapsed_time + 1) * 1000) << " pv ";
                } else if (evaluation < -MATE_THRESHOLD) {
                    int mate_in = (MATE_VALUE + evaluation + 1) / 2;
                    std::cout << "info depth " << depth << " score mate " << mate_in << " time " << elapsed_time << " nodes " << total_nodes << " nps " << (nodes/(elapsed_time + 1) * 1000) << " pv ";
                } else
                    std::cout << "info depth " << depth << " score cp " << evaluation << " time " << elapsed_time << " nodes " << total_nodes << " nps " << (nodes/(elapsed_time + 1) * 1000) << " pv ";
                for (int i = 0; i < pv.length; i++)
                    std::cout << MoveToString(pv.moves[i]) << ' ';
                std::cout << std::endl;
            }
        }
    }
    
    result->finished = true;

    delete search_args;
    return nullptr;
}

template void* Search<Move>(void*);
template void* Search<MoveTar>(void*);