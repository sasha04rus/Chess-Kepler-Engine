#pragma once

#include <unordered_map>
#include <string>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <functional>

#include "engine_types.h"
#include "../board/board.h"

#define MAX_DEPTH 30
#define DEFAULT_NUMBER_OF_THREADS 1
#define DEFAULT_MULTI_PV 1

class Engine {
public:
    Engine() = default;
    void SetPosition(const std::string& fen);
    void MakeMove(const std::string& move);
    void SetThreads(int threads);
    void SetMultiPv(int multi_pv);
    void Go(int mt, int depth);
    void Stop();
    void NewGame();
    bool GetTurn();
    int GetPly();
    void SetInfoCallback(InfoCallback callback);
    void SetBestMoveCallback(BestMoveCallback callback);
private:
    std::unique_ptr<Board> board_ptr_;
    int threads_ = DEFAULT_NUMBER_OF_THREADS;
    int multi_pv_ = DEFAULT_MULTI_PV;
    pthread_t search_thread_;
    InfoCallback info_callback_;
    BestMoveCallback best_move_callback_;
};