#include <algorithm>

#include "time_manager.h"

#define MAX_TIME 30000
#define MIN_TIME 500

int CalculateMoveTime(int my_time, int opponent_time, int increment, int moves) {

    if (my_time < 500) {
        return increment / 2 + 50;
    }

    if (my_time < 5000) {
        return increment + 100;
    }

    if (my_time < 10000) {
        return increment + 300;
    }

    if (my_time < 30000) {
        return increment + 500;
    }

    int remaining_moves = 30; 
    if (moves > 0) {
        remaining_moves = std::max(10, 50 - moves);
    }

    int base_time = my_time / remaining_moves + increment;

    double ratio = static_cast<double>(my_time) / opponent_time;
    if (ratio < 0.5) {
        
        base_time = static_cast<int>(base_time * 0.8);
    } else if (ratio > 2.0) {
        
        base_time = static_cast<int>(base_time * 1.2);
    }

    base_time = std::clamp(base_time, MIN_TIME, MAX_TIME);

    if (my_time > 120000 && base_time < 10000) {
        base_time = std::min(base_time, 10000);
    }

    return base_time;
}
