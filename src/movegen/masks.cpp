#include "generate_moves.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace movegen {

constexpr std::uint8_t MinU8(std::uint8_t a, std::uint8_t b) {
    return a < b ? a : b;
}

constexpr int MinInt(int a, int b) {
    return a < b ? a : b;
}

constexpr int MaxInt(int a, int b) {
    return a > b ? a : b;
}

constexpr Arr<std::uint8_t, 256> MakeCompress() {
    Arr<std::uint8_t, 256> out{};
    for (std::uint16_t mask = 0; mask < 256; ++mask) {
        out[mask] = static_cast<std::uint8_t>((mask >> 1) & 0x3F);
    }
    return out;
}

constexpr Arr2<std::uint8_t, 64, 64> MakeRankAttacks() {
    Arr2<std::uint8_t, 64, 64> out{};

    for (int piece_pos = 0; piece_pos < 8; ++piece_pos) {
        for (std::uint8_t rank_mask = 0; rank_mask < 64; ++rank_mask) {
            std::uint8_t mask = 255;
            mask &= static_cast<std::uint8_t>(~((std::uint8_t)1 << piece_pos));

            bool barrier = false;
            for (int i = piece_pos - 1; i >= 0; --i) {
                if (barrier) {
                    mask &= static_cast<std::uint8_t>(~(std::uint8_t{1} << i));
                }
                if (((rank_mask << 1) & ((std::uint8_t)1 << i)) != 0) {
                    barrier = true;
                }
            }

            barrier = false;
            for (int i = piece_pos + 1; i < 8; ++i) {
                if (barrier) {
                    mask &= static_cast<std::uint8_t>(~((std::uint8_t)1 << i));
                }
                if (((rank_mask << 1) & ((std::uint8_t)1 << i)) != 0) {
                    barrier = true;
                }
            }

            for (int rank = 0; rank < 8; ++rank) {
                out[piece_pos + 8 * rank][rank_mask] = mask;
            }
        }
    }

    return out;
}

constexpr Arr2<Bitboard, 64, 64> MakeFileAttacks(const Arr2<std::uint8_t, 64, 64>& rank) {
    Arr2<Bitboard, 64, 64> out{};

    for (int piece_pos = 0; piece_pos < 8; ++piece_pos) {
        for (std::uint8_t rank_mask = 0; rank_mask < 64; ++rank_mask) {
            std::uint8_t mask = rank[piece_pos][rank_mask];
            for (int file = 0; file < 8; ++file) {
                Bitboard vertical_attacks = 0;
                for (int j = 0; j < 8; ++j) {
                    if ((mask & (1u << j)) != 0) {
                        vertical_attacks |= ((Bitboard)1 << (j * 8 + file));
                    }
                }
                out[piece_pos * 8 + file][rank_mask] = vertical_attacks;
            }
        }
    }

    return out;
}

constexpr Arr<Bitboard, 64> MakeRotate90Map() {
    Arr<Bitboard, 64> out{};
    for (int sq = 0; sq < 64; ++sq) {
        int rank = sq / 8;
        int file = sq % 8;
        int new_sq = file * 8 + rank;
        out[sq] = (Bitboard)1 << new_sq;
    }
    return out;
}

constexpr Arr<Bitboard, 64> MakeRotate45Map() {
    Arr<Bitboard, 64> out{};
    for (int sq = 0; sq < 64; ++sq) {
        int rank = sq / 8;
        int file = sq % 8;
        int diag_sum = rank + file;
        int pos_sum = MinInt(rank, 7 - file);
        int packed_sum = kDiagLengthPrefSum[diag_sum] + pos_sum;
        out[sq] = (Bitboard)1 << packed_sum;
    }
    return out;
}

constexpr Arr<Bitboard, 64> MakeRotate315Map() {
    Arr<Bitboard, 64> out{};
    for (int sq = 0; sq < 64; ++sq) {
        int rank = sq / 8;
        int file = sq % 8;
        int diag_diff = rank - file + 7;
        int pos_diff = MinInt(rank, file);
        int packed_diff = kDiagLengthPrefSum[diag_diff] + pos_diff;
        out[sq] = (Bitboard)1 << packed_diff;
    }
    return out;
}

constexpr Arr2<Bitboard, 64, 64> MakeDiagA1H8Attacks(const Arr2<std::uint8_t, 64, 64>& rank) {
    Arr2<Bitboard, 64, 64> out{};

    for (int sq = 0; sq < 64; ++sq) {
        int rank_idx = sq / 8;
        int file = sq % 8;
        int sum = rank_idx + file;
        int start_rank = MaxInt(0, sum - 7);
        int start_file = sum - start_rank;

        std::array<int, 8> diag_squares{};
        int len = 0;
        int piece_pos = -1;
        int r = start_rank;
        int f = start_file;

        while (r < 8 && f >= 0) {
            int real_sq = r * 8 + f;
            diag_squares[len] = real_sq;
            if (real_sq == sq) {
                piece_pos = len;
            }
            ++r;
            --f;
            ++len;
        }

        for (int state = 0; state < 64; ++state) {
            std::uint8_t mask = rank[piece_pos][state];
            Bitboard attacks = 0;
            for (int i = 0; i < len; ++i) {
                if ((mask & (1u << i)) != 0) {
                    attacks |= ((Bitboard)1 << diag_squares[i]);
                }
            }
            out[sq][state] = attacks;
        }
    }

    return out;
}

constexpr Arr2<Bitboard, 64, 64> MakeDiagH1A8Attacks(const Arr2<std::uint8_t, 64, 64>& rank) {
    Arr2<Bitboard, 64, 64> out{};

    for (int sq = 0; sq < 64; ++sq) {
        int rank_idx = sq / 8;
        int file = sq % 8;
        int diff = rank_idx - file;

        int start_rank = MaxInt(0, diff);
        int start_file = start_rank - diff;

        std::array<int, 8> diag_squares{};
        int len = 0;
        int piece_pos = -1;
        int r = start_rank;
        int f = start_file;

        while (r < 8 && f < 8) {
            int real_sq = r * 8 + f;
            diag_squares[len] = real_sq;
            if (real_sq == sq) {
                piece_pos = len;
            }
            ++r;
            ++f;
            ++len;
        }

        for (int state = 0; state < 64; ++state) {
            std::uint8_t mask = rank[piece_pos][state];
            Bitboard attacks = 0;
            for (int i = 0; i < len; ++i) {
                if ((mask & (1u << i)) != 0) {
                    attacks |= ((Bitboard)1 << diag_squares[i]);
                }
            }
            out[sq][state] = attacks;
        }
    }

    return out;
}

} // namespace movegen