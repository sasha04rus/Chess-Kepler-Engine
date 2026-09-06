#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct SearchInfo {
    int depth;
    int evaluation;
    int mate_in = 0;
    std::uint64_t time;
    std::uint64_t nodes;
    std::uint64_t nps;
    std::vector<std::string> pv;
};

using InfoCallback = std::function<void(const SearchInfo&)>;
using BestMoveCallback = std::function<void(const std::string&)>;