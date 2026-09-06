#include "engine.h"

#include <memory>

#include "../moves/to_string.h"
#include "../search/search.h"
#include "../time/time_manager.h"
#include "../tt/tt.h"

void Engine::SetPosition(const std::string& fen) {
    if (searching) {
        stop_signal = true;
        pthread_join(search_thread_, nullptr);
        searching = false;
    }
    board_ptr_.reset();
    board_ptr_ = std::make_unique<Board>(fen);
}

void Engine::MakeMove(const std::string& move) {
    board_ptr_->MakeMove(move);
}

void Engine::SetThreads(int threads) {
    threads_ = threads;
}

void Engine::SetMultiPv(int multi_pv) {
    multi_pv_ = multi_pv;
}

void Engine::Go(int mt, int depth) {
    movetime = mt;
    stop_signal = false;
    searching = true;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    SearchArgs* sa = new SearchArgs{*board_ptr_, depth, threads_, multi_pv_, info_callback_, best_move_callback_};
    #if KEPLER_TAR
        pthread_create(&search_thread_, nullptr, Search<MoveTar>, sa);
    #else
        pthread_create(&search_thread_, nullptr, Search<Move>, sa);
    #endif
}

void Engine::Stop() {
    if (searching) {
        stop_signal.store(true, std::memory_order_relaxed);
        pthread_join(search_thread_, nullptr);
        searching = false;
    }
}

void Engine::NewGame() {
    ClearTT();
}

bool Engine::GetTurn() {
    return board_ptr_->turn;
}

int Engine::GetPly() {
    return board_ptr_->ply;
}

void Engine::SetInfoCallback(InfoCallback callback) {
    info_callback_ = std::move(callback);
}

void Engine::SetBestMoveCallback(BestMoveCallback callback) {
    best_move_callback_ = std::move(callback);
}