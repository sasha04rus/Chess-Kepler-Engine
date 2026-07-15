#pragma once
#include <array>
#include <cstdint>

#include "../board/board.h"

namespace movegen {

using Byte = std::uint8_t;

template <typename T, std::size_t N>
using Arr = std::array<T, N>;

template <typename T, std::size_t N, std::size_t M>
using Matrix = std::array<std::array<T, M>, N>;

inline constexpr Arr<Byte, 15> kDiagLengthPrefSum = {0, 1, 3, 6, 10, 15, 21, 28, 36, 43, 49, 54, 58, 61, 63};

inline constexpr Arr<int, 64> diag_h1a8 = {
    0,1,2,3,4,5,6,7,
    1,2,3,4,5,6,7,8,
    2,3,4,5,6,7,8,9,
    3,4,5,6,7,8,9,10,
    4,5,6,7,8,9,10,11,
    5,6,7,8,9,10,11,12,
    6,7,8,9,10,11,12,13,
    7,8,9,10,11,12,13,14
};

inline constexpr Arr<int, 64> diag_a1h8 = {
    7,6,5,4,3,2,1,0,
    8,7,6,5,4,3,2,1,
    9,8,7,6,5,4,3,2,
    10,9,8,7,6,5,4,3,
    11,10,9,8,7,6,5,4,
    12,11,10,9,8,7,6,5,
    13,12,11,10,9,8,7,6,
    14,13,12,11,10,9,8,7
};

constexpr Bitboard kMasksForKing[64] = {
    770ULL, 1797ULL, 3594ULL, 7188ULL, 14376ULL, 28752ULL, 57504ULL, 49216ULL,
    197123ULL, 460039ULL, 920078ULL, 1840156ULL, 3680312ULL, 7360624ULL, 14721248ULL, 12599488ULL,
    50463488ULL, 117769984ULL, 235539968ULL, 471079936ULL, 942159872ULL, 1884319744ULL, 3768639488ULL, 3225468928ULL,
    12918652928ULL, 30149115904ULL, 60298231808ULL, 120596463616ULL, 241192927232ULL, 482385854464ULL, 964771708928ULL, 825720045568ULL,
    3307175149568ULL, 7718173671424ULL, 15436347342848ULL, 30872694685696ULL, 61745389371392ULL, 123490778742784ULL, 246981557485568ULL, 211384331665408ULL,
    846636838289408ULL, 1975852459884544ULL, 3951704919769088ULL, 7903409839538176ULL, 15806819679076352ULL, 31613639358152704ULL, 63227278716305408ULL, 54114388906344448ULL,
    216739030602088448ULL, 505818229730443264ULL, 1011636459460886528ULL, 2023272918921773056ULL, 4046545837843546112ULL, 8093091675687092224ULL, 16186183351374184448ULL, 13853283560024178688ULL,
    144959613005987840ULL, 362258295026614272ULL, 724516590053228544ULL, 1449033180106457088ULL, 2898066360212914176ULL, 5796132720425828352ULL, 11592265440851656704ULL, 4665729213955833856ULL
};

constexpr Bitboard kMasksForKnight[64] = {
    132096ULL, 329728ULL, 659712ULL, 1319424ULL, 2638848ULL, 5277696ULL, 10489856ULL, 4202496ULL,
    33816580ULL, 84410376ULL, 168886289ULL, 337772578ULL, 675545156ULL, 1351090312ULL, 2685403152ULL,
    1075839008ULL, 8657044482ULL, 21609056261ULL, 43234889994ULL, 86469779988ULL, 172939559976ULL,
    345879119952ULL, 687463207072ULL, 275414786112ULL, 2216203387392ULL, 5531918402816ULL, 11068131838464ULL,
    22136263676928ULL, 44272527353856ULL, 88545054707712ULL, 175990581010432ULL, 70506185244672ULL,
    567348067172352ULL, 1416171111120896ULL, 2833441750646784ULL, 5666883501293568ULL, 11333767002587136ULL,
    22667534005174272ULL, 45053588738670592ULL, 18049583422636032ULL, 145241105196122112ULL,
    362539804446949376ULL, 725361088165576704ULL, 1450722176331153408ULL, 2901444352662306816ULL,
    5802888705324613632ULL, 11533718717099671552ULL, 4620693356194824192ULL, 288234782788157440ULL,
    576469569871282176ULL, 1224997833292120064ULL, 2449995666584240128ULL, 4899991333168480256ULL,
    9799982666336960512ULL, 1152939783987658752ULL, 2305878468463689728ULL, 1128098930098176ULL,
    2257297371824128ULL, 4796069720358912ULL, 9592139440717824ULL, 19184278881435648ULL, 38368557762871296ULL,
    4679521487814656ULL, 9077567998918656ULL
};

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
    for (std::uint16_t mask = 0; mask < 256; mask++)
        out[mask] = (0x3F & ((uint8_t)mask >> 1));
    return out;
}

constexpr Matrix<std::uint8_t, 64, 64> MakeRankAttacks() {
    Matrix<std::uint8_t, 64, 64> out{};
    for (int piece_pos = 0; piece_pos < 8; piece_pos++) {
        for (std::uint8_t rank_mask = 0; rank_mask < 64; rank_mask++) {
            std::uint8_t mask = 255;
            mask &= ~((std::uint8_t)1 << piece_pos);
            bool barrier = false;
            int i = piece_pos - 1;
            while (i >= 0) {
                if (barrier)
                    mask &= ~((std::uint8_t)1 << i);
                if (((rank_mask << 1) & (std::uint8_t)1 << i))
                    barrier = true;
                i--;
            }
            barrier = false;
            i = piece_pos + 1;
            while (i < 8) {
                if (barrier)
                    mask &= ~((std::uint8_t)1 << i);
                if (((rank_mask << 1) & (std::uint8_t)1 << i))
                    barrier = true;
                i++;
            }
            for (int i = 0; i < 8; i++)
                out[piece_pos + 8 * i][rank_mask] = mask;
        }
    }

    return out;
}

constexpr Matrix<Bitboard, 64, 64> MakeFileAttacks(const Matrix<std::uint8_t, 64, 64>& rank) {
    Matrix<Bitboard, 64, 64> out{};
    for (int piece_pos = 0; piece_pos < 8; piece_pos++) {
        for (std::uint8_t rank_mask = 0; rank_mask < 64; rank_mask++) {
            std::uint8_t mask = rank_attacks[piece_pos][rank_mask];
            for (int file = 0; file < 8; file++) {
                Bitboard vertical_attacks = 0;
                for (int j = 0; j < 8; j++)
                    if (mask & (1 << j))
                        vertical_attacks |= (1ULL << (j * 8 + file));
                out[piece_pos * 8 + file][rank_mask] = vertical_attacks;
            }
        }
    }
    return out;
}

constexpr Arr<Bitboard, 64> MakeRotate90Map() {
    Arr<Bitboard, 64> out{};
    for (int sq = 0; sq < 64; sq++) {
        int rank = sq / 8;
        int file = sq % 8;
        int new_sq = file * 8 + rank;
        out[sq] = (Bitboard)1 << new_sq;
    }
    return out;
}

constexpr Arr<Bitboard, 64> MakeRotate45Map() {
    Arr<Bitboard, 64> out{};
    for (int sq = 0; sq < 64; sq++) {
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
    for (int sq = 0; sq < 64; sq++) {
        int rank = sq / 8;
        int file = sq % 8;
        int diag_diff = rank - file + 7;
        int pos_diff = MinInt(rank, file);
        int packed_diff = kDiagLengthPrefSum[diag_diff] + pos_diff;
        out[sq] = (Bitboard)1 << packed_diff;
    }
    return out;
}

constexpr Matrix<Bitboard, 64, 64> MakeDiagA1H8Attacks(const Matrix<std::uint8_t, 64, 64>& rank) {
    Matrix<Bitboard, 64, 64> out{};
    for (int sq = 0; sq < 64; sq++) {
        int rank = sq / 8;
        int file = sq % 8;
        int sum = rank + file;
        int start_rank = std::max(0, sum - 7);
        int start_file = sum - start_rank; 
        int diag_squares[8] = {0};
        int len = 0;
        int piece_pos = -1;
        int r = start_rank;
        int f = start_file;
        while (r < 8 && f >= 0) {
            int real_sq = r * 8 + f;
            diag_squares[len] = real_sq;
            if (real_sq == sq) piece_pos = len;
            r++;
            f--;
            len++;
        }
        for (int state = 0; state < 64; state++) {
            uint8_t mask = rank_attacks[piece_pos][state];
            Bitboard attacks = 0;
            for (int i = 0; i < len; i++)
                if (mask & (1 << i))
                    attacks |= (1ULL << diag_squares[i]);
            out[sq][state] = attacks;
        }
    }
    return out;
}

constexpr Matrix<Bitboard, 64, 64> MakeDiagH1A8Attacks(const Matrix<std::uint8_t, 64, 64>& rank) {
    Matrix<Bitboard, 64, 64> out{};
    for (int sq = 0; sq < 64; sq++) {
        int rank = sq / 8;
        int file = sq % 8;
        int diff = rank - file; 
        int start_rank = std::max(0, diff);
        int start_file = start_rank - diff; 
        int diag_squares[8] = {0};
        int len = 0;
        int piece_pos = -1;
        int r = start_rank;
        int f = start_file;
        while (r < 8 && f < 8) {
            int real_sq = r * 8 + f;
            diag_squares[len] = real_sq;
            if (real_sq == sq) piece_pos = len;
            r++;
            f++;
            len++;
        }
        for (int state = 0; state < 64; state++) {
            uint8_t mask = rank_attacks[piece_pos][state];
            Bitboard attacks = 0;
            for (int i = 0; i < len; i++)
                if (mask & (1 << i))
                    attacks |= (1ULL << diag_squares[i]);
            out[sq][state] = attacks;
        }
    }
    return out;
}

inline constexpr auto compress = MakeCompress();
inline constexpr auto rank_attacks = MakeRankAttacks();
inline constexpr auto file_attacks = MakeFileAttacks(rank_attacks);
inline constexpr auto diaga1h8_attacks = MakeDiagA1H8Attacks(rank_attacks);
inline constexpr auto diagh1a8_attacks = MakeDiagH1A8Attacks(rank_attacks);
inline constexpr auto map_to_rotate90 = MakeRotate90Map();
inline constexpr auto map_to_rotate45 = MakeRotate45Map();
inline constexpr auto map_to_rotate315 = MakeRotate315Map();

Bitboard GetPawnWhiteLeftCapture(const Bitboard& pawns, const Bitboard& enemy);
Bitboard GetPawnWhiteRightCapture(const Bitboard& pawns, const Bitboard& enemy);
Bitboard GetPawnBlackLeftCapture(const Bitboard& pawns, const Bitboard& enemy);
Bitboard GetPawnBlackRightCapture(const Bitboard& pawns, const Bitboard& enemy);
Bitboard GetKnightAttack(std::uint8_t sq);
Bitboard GetKingAttack(std::uint8_t sq);
Bitboard GetRookAttack(std::uint8_t sq, const RotatedBoard& rotate);
Bitboard GetBishopAttack(std::uint8_t sq, const RotatedBoard& rotate);

int GenerateMoves(const Board& board, Move possible_moves[218], bool only_captures = false);

} // namespace movegen