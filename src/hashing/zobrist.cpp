#include <random>

#include "zobrist.h"

namespace zobrist {

const std::uint8_t ep_square_to_index[64] = {
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,1,2,3,4,5,6,7,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    8,9,10,11,12,13,14,15,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

std::uint64_t piece[2][6][64];
std::uint64_t castling[4];
std::uint64_t en_passant[16];
std::uint64_t turn;

void Init() {
    std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    
    for (auto& color : piece)
        for (auto& piece_type : color)
            for (auto& val : piece_type)
                val = dist(rng);
    
    for (auto& val : castling) val = dist(rng);
    for (auto& val : en_passant) val = dist(rng);
    turn = dist(rng);
}

} // namespace zobrist