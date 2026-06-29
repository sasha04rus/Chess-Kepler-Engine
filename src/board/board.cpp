#include "board.h"
#include "../utils/bit_fun.h"

bool Board::IsEndgame() const {
    int count = popcount(rotated.occupied) - (popcount(bitboards[0][0]) + popcount(bitboards[1][0])) - 2;
    return (count > 5) ? false : true;
}