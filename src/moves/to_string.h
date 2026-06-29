#pragma once
#include <string>

#include "../moves/standard/move.h"
#include "../moves/tar/move.h"

extern const char* kBoard[64];

const std::string MoveToString(const Move& move);
const std::string MoveToString(const MoveTar& move);