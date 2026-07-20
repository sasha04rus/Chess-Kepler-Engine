#include "../search/search.h"

#include <ctime>
#include <cstring>
#include <iostream>
#include <cstdint>
#include <vector>
#include <memory>
#include <algorithm>
#include <pthread.h>
#include <thread>

#include "../board/board.h"
#include "../moves/standard/move.h"
#include "../moves/tar/move.h"
#include "../moves/pv.h"
#include "../tt/tt.h"
#include "../moves/to_string.h"
#include "../movegen/generate_moves.h"
#include "standard/minimax.h"
#include "tar/minimax.h"

#define INFINITY 15000

struct timespec start_time;
std::atomic<bool> stop_signal{false};
std::atomic<bool> searching{false};
std::string bestmove;
int movetime = 0;
thread_local const std::atomic<bool>* local_stop_signal = nullptr;

static constexpr std::int64_t NANOSECONDS_PER_MILLISECOND = 1000000;
static constexpr std::int64_t NANOSECONDS_PER_SECOND = 1000000000;

bool IsInterrupted() {
    if (local_stop_signal != nullptr && local_stop_signal->load(std::memory_order_relaxed))
        return true;
    if (stop_signal.load(std::memory_order_relaxed))
        return true;
    if (movetime <= 0) return false;
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    int64_t elapsed_ns = (int64_t)(current_time.tv_sec - start_time.tv_sec) * NANOSECONDS_PER_SECOND + (int64_t)(current_time.tv_nsec - start_time.tv_nsec);
    return elapsed_ns / NANOSECONDS_PER_MILLISECOND >= (std::int64_t)movetime;
}

template <typename MoveType>
PrincipalVariation<MoveType> ExtractPvFromTT(Board board, int max_length) {
    PrincipalVariation<MoveType> pv;
    const int capacity = static_cast<int>(sizeof(pv.moves) / sizeof(pv.moves[0]));
    max_length = std::min(max_length, capacity);
    for (int ply = 0; ply < max_length; ply++) {
        TTEntry entry;

        if (!ProbeTT(board.zobrist_hash, entry))
            break;

        if (entry.flag != EXACT)
            break;

        if (entry.best_move == NO_MOVE)
            break;

        MoveType move = entry.best_move;
        board.MakeMove(move);
        if (!board.LegalTest(board.turn)) {
            board.UnMakeMove(move);
            break;
        }
        pv.moves[pv.length++] = move;
    }

    return pv;
}

std::uint64_t GetElapsedMilliseconds() {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    const std::int64_t elapsed_ns = static_cast<std::int64_t>(current_time.tv_sec - start_time.tv_sec) * NANOSECONDS_PER_SECOND +
        static_cast<std::int64_t>(current_time.tv_nsec - start_time.tv_nsec);
    return static_cast<std::uint64_t>(elapsed_ns / NANOSECONDS_PER_MILLISECOND);
}

template <typename MoveType>
SearchResult<MoveType> SearchPv(const Board& root_board, int depth) {
    Board board(root_board);
    std::uint64_t nodes = 0;
    PrincipalVariation<MoveType> pv;
    const int evaluation = Minimax(board, depth, -INFINITY, INFINITY, pv, nodes, 0);
    return SearchResult<MoveType>{evaluation, pv, nodes, !IsInterrupted()};
}

struct LazyShared {
    std::atomic<int> main_depth{1};
    std::atomic<bool> finish{false};
    std::atomic<std::uint64_t> nodes{0};
};

template <typename MoveType>
struct LazyThreadArgs {
    const Board* board = nullptr;
    int max_depth = 0;
    int thread_id = 0;
    LazyShared* shared = nullptr;
};

template <typename MoveType>
void* LazyHelper(void* raw_args) {
    auto* args = static_cast<LazyThreadArgs<MoveType>*>(raw_args);
    local_stop_signal = &args->shared->finish;

    ClearKiller();
    int depth_offset = 1 + ((args->thread_id - 1) % 3);
    int last_main_depth = 0;
    while (!IsInterrupted()) {
        const int current_main_depth = args->shared->main_depth.load(std::memory_order_acquire);
        if (current_main_depth == last_main_depth) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        last_main_depth = current_main_depth;
        const int target_depth = current_main_depth + depth_offset;

        if (target_depth > args->max_depth)
            break;

        SearchResult<MoveType> result = SearchPv<MoveType>(*(args->board), target_depth);
        args->shared->nodes.fetch_add(result.nodes, std::memory_order_relaxed);
        if (!result.valid) break;
    }

    local_stop_signal = nullptr;
    return nullptr;
}

template <typename MoveType>
void* Search(void* raw_args) {
    std::unique_ptr<SearchArgs> search_args(static_cast<SearchArgs*>(raw_args));
    bestmove = "0000";
    const Board root_board = search_args->board;
    LazyShared shared;
    std::vector<LazyThreadArgs<MoveType>> helper_args(search_args->threads);
    std::vector<pthread_t> helpers;
    helpers.reserve(search_args->threads);
    for (int thread_id = 1; thread_id < search_args->threads; thread_id++) {
        auto& args = helper_args[thread_id - 1];
        args.board = &root_board;
        args.max_depth = search_args->max_depth;
        args.thread_id = thread_id;
        args.shared = &shared;

        pthread_t thread{};
        const int error = pthread_create(&thread, nullptr, LazyHelper<MoveType>, &args);
        if (error != 0) {
            std::cerr << "info string pthread_create failed " << error << std::endl;
            continue;
        }

        helpers.push_back(thread);
    }

    ClearKiller();
    SearchResult<MoveType> last_complete;
    for (int depth = 1; depth <= search_args->max_depth; depth++) {
        if (IsInterrupted())
            break;

        shared.main_depth.store(depth, std::memory_order_release);
        SearchResult<MoveType> result = SearchPv<MoveType>(root_board, depth);
        shared.nodes.fetch_add(result.nodes, std::memory_order_relaxed);
        if (!result.valid)
            break;
        if (result.pv.length == 0)
            result.pv = ExtractPvFromTT<MoveType>(root_board, depth);
        if (result.pv.length == 0)
            continue;

        last_complete = result;

        bestmove = MoveToString(result.pv.moves[0]);
        std::uint64_t elapsed_ms = GetElapsedMilliseconds();
        std::uint64_t total_nodes = shared.nodes.load(std::memory_order_relaxed);
        std::uint64_t nps = (elapsed_ms > 0) ? (total_nodes * 1000ULL / elapsed_ms) : (total_nodes * 1000ULL);

        if (result.evaluation > MATE_THRESHOLD) {
            int mate_in = (MATE_VALUE - result.evaluation + 1) / 2;
            std::cout << "info depth " << depth << " score mate " << mate_in << " time " << elapsed_ms << " nodes " << total_nodes << " nps " << nps << " pv ";
        } else if (result.evaluation < -MATE_THRESHOLD) {
            int mate_in = (MATE_VALUE + result.evaluation + 1) / 2;
            std::cout << "info depth " << depth << " score mate " << mate_in << " time " << elapsed_ms << " nodes " << total_nodes << " nps " << nps << " pv ";
        } else
            std::cout << "info depth " << depth << " score cp " << result.evaluation << " time " << elapsed_ms << " nodes " << total_nodes << " nps " << nps << " pv ";
                
        for (int i = 0; i < result.pv.length; i++)
            std::cout << MoveToString(result.pv.moves[i]) << ' ';
        std::cout << std::endl;
    }

    shared.finish.store(true, std::memory_order_release);
    for (pthread_t thread : helpers)
        pthread_join(thread, nullptr);
    if (last_complete.valid && last_complete.pv.length > 0)
        bestmove = MoveToString(last_complete.pv.moves[0]);
    std::cout << "bestmove " << bestmove << std::endl;
    searching.store(false, std::memory_order_release);
    return nullptr;
}

template void* Search<Move>(void*);
// template void* Search<MoveTar>(void*);