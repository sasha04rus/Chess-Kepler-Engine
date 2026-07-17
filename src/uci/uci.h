#pragma once

#include <unordered_map>
#include <functional>
#include <string>
#include <iostream>
#include <memory>
#include <thread>

#include "../board/board.h"

#define MAX_DEPTH 30
#define DEFAULT_NUMBER_OF_THREADS 1
#define DEFAULT_MULTI_PV 1

class Uci {
public:
    using Handler = std::function<void(const std::vector<std::string>&, std::ostream&)>;
    Uci();

    void Execute(const std::vector<std::string>& parsed_command, std::ostream& out);
private:
    std::unordered_map<std::string, Handler> handlers_;
    std::unique_ptr<Board> board_ptr_;
    int depth_ = MAX_DEPTH;
    int threads_ = DEFAULT_NUMBER_OF_THREADS;
    int multi_pv_ = DEFAULT_MULTI_PV;
    Variant variant_ = Variant::kStandard;
    pthread_t search_thread_;
};