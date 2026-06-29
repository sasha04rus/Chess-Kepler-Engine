#include <cstdint>

inline uint8_t bsf(const std::uint64_t& bb) {
    return __builtin_ctzll(bb);
}

inline uint8_t pop_lsb(std::uint64_t& mask) {
    uint8_t idx = __builtin_ctzll(mask);
    mask &= mask - 1;
    return idx;
}

inline int popcount(uint64_t x) {
    return __builtin_popcountll(x);
}