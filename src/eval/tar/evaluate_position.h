#pragma once
#include <array>
#include <cstdint>

using Bitboard = std::uint64_t;

namespace eval {

inline constexpr int KNIGHT_MOBILITY = 17;
inline constexpr int BISHOP_MOBILITY = 9;
inline constexpr int ROOK_MOBILITY = 10;
inline constexpr int QUEEN_MOBILITY = 8;

inline constexpr int ATTACK_PAWN = 17;
inline constexpr int ATTACK_KHIGHT = 43;
inline constexpr int ATTACK_BISHOP = 25;
inline constexpr int ATTACK_ROOK = 26;
inline constexpr int ATTACK_QUEEN = 42;

inline constexpr int CONNECTION_ROOK = 25;
inline constexpr int ATTACKED_SQUARE = 20;
inline constexpr int PAWN_SHIELD = 12;

inline constexpr std::array<std::int8_t, 64> kEvalWhiteKingPosition = {
     10,  20,  15,  10,  10,  20,  15,  10,
      5,   5,   5,   5,   5,   5,   5,   5,
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,  
      0,   0,   0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
};

inline constexpr std::array<std::int8_t, 64> kEvalBlackKingPosition = {
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,  
      0,   0,   0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
      5,   5,   5,   5,   5,   5,   5,   5,
     10,  20,  15,  10,  10,  20,  15,  10,    
};


inline constexpr std::array<std::int8_t, 64> kEvalQueenPosition = {
    -5, -5, -5, -5, -5, -5, -5, -5,   
    -5,  0,  0,  0,  0,  0,  0, -5,   
    -5,  0,  5,  5,  5,  5,  0, -5,   
    -5,  0,  5,  5,  5,  5,  0, -5,   
    -5,  0,  5,  5,  5,  5,  0, -5,   
    -5,  0,  5,  5,  5,  5,  0, -5,   
    -5,  0,  0,  0,  0,  0,  0, -5,   
    -5, -5, -5, -5, -5, -5, -5, -5 
};

inline constexpr std::array<std::int8_t, 64> kEvalBishopPosition = {
    -20, -10, -10, -10, -10, -10, -10, -20,  
    -10,   0,   0,   0,   0,   0,   0, -10,  
    -10,   0,   5,   5,   5,   5,   0, -10,  
    -10,   0,   5,  10,  10,   5,   0, -10,  
    -10,   0,   5,  10,  10,   5,   0, -10,  
    -10,   0,   5,   5,   5,   5,   0, -10,  
    -10,   0,   0,   0,   0,   0,   0, -10,  
    -20, -10, -10, -10, -10, -10, -10, -20, 
};

inline constexpr std::array<std::int8_t, 64> kEvalKnightPosition = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50,
};

inline constexpr std::array<std::int8_t, 64> kWhiteRookMasks = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    10, 10, 10, 10, 10, 10, 10, 10,
    5, 5, 5, 5, 5, 5, 5, 5,
};

inline constexpr std::array<std::int8_t, 64> kBlackRookMasks = {
    5, 5, 5, 5, 5, 5, 5, 5,
    10, 10, 10, 10, 10, 10, 10, 10,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

inline constexpr std::array<std::uint8_t, 64> kEvalPassingWhitePawns = {
    0, 0, 0, 0, 0, 0, 0, 0,
    5, 5, 5, 5, 5, 5, 5, 5,
    15, 15, 15, 15, 15, 15, 15, 15,
    25, 25, 25, 25, 25, 25, 25, 25,
    40, 40, 40, 40, 40, 40, 40, 40,
    65, 65, 65, 65, 65, 65, 65, 65,
    105, 105, 105, 105, 105, 105, 105, 105,
    0, 0, 0, 0, 0, 0, 0, 0,
};

inline constexpr std::array<std::uint8_t, 64> kEvalPassingBlackPawns = {
    0, 0, 0, 0, 0, 0, 0, 0,
    105, 105, 105, 105, 105, 105, 105, 105,
    65, 65, 65, 65, 65, 65, 65, 65,
    40, 40, 40, 40, 40, 40, 40, 40,
    25, 25, 25, 25, 25, 25, 25, 25,
    15, 15, 15, 15, 15, 15, 15, 15,
    5, 5, 5, 5, 5, 5, 5, 5,
    0, 0, 0, 0, 0, 0, 0, 0,
};

inline constexpr std::array<std::int8_t, 64> kEvalWhitePawns = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    10, 10, 10, 10, 10, 10, 10, 10,
    15, 15, 15, 15, 15, 15, 15, 15,
    20, 20, 20, 20, 20, 20, 20, 20,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

inline constexpr std::array<std::int8_t, 64> kEvalBlackPawns = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    20, 20, 20, 20, 20, 20, 20, 20,
    15, 15, 15, 15, 15, 15, 15, 15,
    10, 10, 10, 10, 10, 10, 10, 10,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

inline constexpr std::array<bool, 64> kSquareWhiteColor = {
    true, false, true, false, true, false, true, false,
    false, true, false, true, false, true, false, true,
    true, false, true, false, true, false, true, false,
    false, true, false, true, false, true, false, true, 
    true, false, true, false, true, false, true, false,
    false, true, false, true, false, true, false, true, 
    true, false, true, false, true, false, true, false,
    false, true, false, true, false, true, false, true, 
};

constexpr std::array<Bitboard, 64> MakeWhitePassingPawnMasks() {
    std::array<Bitboard, 64> out{};
    for (int sq = 8; sq < 56; sq++) {
        Bitboard mask = 0;
        int file = sq % 8;
        for (int rank = sq / 8 + 1; rank < 8; ++rank) {
            mask |= (Bitboard)1 << (rank * 8 + file);
            if (file < 7) mask |= (Bitboard)1 << (rank * 8 + file + 1);
            if (file > 0) mask |= (Bitboard)1 << (rank * 8 + file - 1);
        }
        out[sq] = mask;
    }
    return out;
}

constexpr std::array<Bitboard, 64> MakeBlackPassingPawnMasks() {
    std::array<Bitboard, 64> out{};
    for (int sq = 8; sq < 56; sq++) {
        Bitboard mask = 0;
        int file = sq % 8;
        for (int rank = sq / 8 - 1; rank >= 0; --rank) {
            mask |= (Bitboard)1 << (rank * 8 + file);
            if (file < 7) mask |= (Bitboard)1 << (rank * 8 + file + 1);
            if (file > 0) mask |= (Bitboard)1 << (rank * 8 + file - 1);
        }
        out[sq] = mask;
    }
    return out;
}

constexpr std::array<Bitboard, 64> MakeIsolatedPawnMasks() {
    std::array<Bitboard, 64> out{};
    for (int sq = 8; sq < 56; sq++) {
        Bitboard mask = 0;
        int file = sq % 8;
        for (int rank = 0; rank < 8; ++rank) {
            if (file < 7) mask |= (Bitboard)1 << (rank * 8 + file + 1);
            if (file > 0) mask |= (Bitboard)1 << (rank * 8 + file - 1);
        }
        out[sq] = mask;
    }
    return out;
}

constexpr std::array<Bitboard, 64> MakeLineBehindWhitePawn() {
    std::array<Bitboard, 64> out{};
    for (int sq = 8; sq < 56; sq++) {
        Bitboard mask = 0;
        int file = sq % 8;
        for (int rank = sq / 8 - 1; rank >= 0; --rank) {
            mask |= (Bitboard)1 << (rank * 8 + file);
        }
        out[sq] = mask;
    }
    return out;
}

constexpr std::array<Bitboard, 64> MakeLineBehindBlackPawn() {
    std::array<Bitboard, 64> out{};
    for (int sq = 8; sq < 56; sq++) {
        Bitboard mask = 0;
        int file = sq % 8;
        for (int rank = sq / 8 + 1; rank < 8; ++rank) {
            mask |= (Bitboard)1 << (rank * 8 + file);
        }
        out[sq] = mask;
    }
    return out;
}

constexpr std::array<Bitboard, 64> MakeWhitePawnShield() {
    std::array<Bitboard, 64> out{};
    for (int sq = 0; sq < 64; sq++) {
        Bitboard mask = 0;
        int file = sq % 8;
        int start_rank = sq / 8 + 1;
        int end_rank = (start_rank + 2 < 8) ? start_rank + 2 : 8;
        for (int rank = start_rank; rank < end_rank; ++rank) {
            mask |= (Bitboard)1 << (rank * 8 + file);
            if (file < 7) mask |= (Bitboard)1 << (rank * 8 + file + 1);
            if (file > 0) mask |= (Bitboard)1 << (rank * 8 + file - 1);
        }
        out[sq] = mask;
    }
    return out;
}

constexpr std::array<Bitboard, 64> MakeBlackPawnShield() {
    std::array<Bitboard, 64> out{};
    for (int sq = 0; sq < 64; sq++) {
        Bitboard mask = 0;
        int file = sq % 8;
        int start_rank = sq / 8 - 1;
        int end_rank = (start_rank - 2 > -1) ? start_rank - 2 : -1;
        for (int rank = start_rank; rank > end_rank; --rank) {
            mask |= (Bitboard)1 << (rank * 8 + file);
            if (file < 7) mask |= (Bitboard)1 << (rank * 8 + file + 1);
            if (file > 0) mask |= (Bitboard)1 << (rank * 8 + file - 1);
        }
        out[sq] = mask;
    }
    return out;
}

constexpr std::array<Bitboard, 64> MakeColumns() {
    std::array<Bitboard, 64> out{};
    for (int sq = 0; sq < 64; sq++) {
        Bitboard mask = 0;
        int file = sq % 8;
        for (int rank = 0; rank < 8; ++rank) {
            mask |= (Bitboard)1 << (rank * 8 + file);
        }
        out[sq] = mask;
    }
    return out;
}

inline constexpr auto masks_for_white_passing_pawn = MakeWhitePassingPawnMasks();
inline constexpr auto masks_for_black_passing_pawn = MakeBlackPassingPawnMasks();
inline constexpr auto masks_for_isolated_pawn = MakeIsolatedPawnMasks();
inline constexpr auto line_behind_white_pawn = MakeLineBehindWhitePawn();
inline constexpr auto line_behind_black_pawn = MakeLineBehindBlackPawn();
inline constexpr auto white_pawns_shield = MakeWhitePawnShield();
inline constexpr auto black_pawns_shield = MakeBlackPawnShield();
inline constexpr auto columns = MakeColumns();

inline constexpr Bitboard kWhiteSquares = 0xAA55AA55AA55AA55ULL;
inline constexpr Bitboard kBlackSquares = 0x55AA55AA55AA55AAULL;
inline constexpr std::array<int, 5> price = {100, 290, 310, 485, 926};

} // namespace eval