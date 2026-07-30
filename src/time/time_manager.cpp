#include <algorithm>

#include "time_manager.h"

constexpr int MAX_TIME = 30000;
constexpr int MIN_TIME = 200;
constexpr int AVERAGE_MOVE_OVERHEAD = 50;

int CalculateMoveTime(int my_time, int opponent_time, int increment, int moves) {
    if (my_time < 1000)
        return MIN_TIME - AVERAGE_MOVE_OVERHEAD;

    if (my_time < 60000)
        return std::max(std::min(my_time / 30 + increment / 2, my_time / 2) - AVERAGE_MOVE_OVERHEAD, MIN_TIME);

    int remaining_moves = 30; 
    if (moves > 0)
        remaining_moves = std::max(10, 50 - moves);
    int base_time = my_time / remaining_moves + increment;
    double ratio = static_cast<double>(my_time) / opponent_time;
    if (ratio < 0.5)
        base_time = static_cast<int>(base_time * 0.8);
    else if (ratio > 2.0)
        base_time = static_cast<int>(base_time * 1.2);
    return std::clamp(base_time, MIN_TIME, MAX_TIME);
}
