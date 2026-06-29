#include <cstdint>

using Bitboard = std::uint64_t;

#define KNIGHT_MOBILITY 6
#define KNIGHT_OPENING_MOBILITY 7
#define BISHOP_MOBILITY 3
#define BISHOP_OPENING_MOBILITY 4
#define ROOK_MOBILITY 3
#define QUEEN_MOBILITY 2

#define CONNECTION_ROOK 25
#define ATTACKED_SQUARE 17
#define PAWN_SHIELD 15

const std::int8_t kEvalWhiteKingPosition[64] = {
     35,  50,  25,  20,  20,  45,  40,  30,
     10,  10,  10,  10,  10,  10,  10,  10,
      0,   0,   0,   0,   0,   0,   0,   0,
    -10, -10, -10, -10, -10, -10, -10, -10,  
    -50, -50, -50, -50, -50, -50, -50, -50, 
    -90, -90, -90, -90, -90, -90, -90, -90, 
    -99, -99, -99, -99, -99, -99, -99, -99, 
    -99, -99, -99, -99, -99, -99, -99, -99,
};

const std::int8_t kEvalBlackKingPosition[64] = {
    -99, -99, -99, -99, -99, -99, -99, -99, 
    -99, -99, -99, -99, -99, -99, -99, -99,
    -90, -90, -90, -90, -90, -90, -90, -90,
    -50, -50, -50, -50, -50, -50, -50, -50,  
     10, -10, -10, -10, -10, -10, -10, -10,
      0,   0,   0,   0,   0,   0,   0,   0,
     10,  10,  10,  10,  10,  10,  10,  10,
     35,  50,  25,  20,  20,  45,  40,  30,    
};

const std::uint8_t kEvalKingPositionEngame[64] = {
      0,  10,  20,  30,  30,  20,  10,   0,
     10,  20,  30,  40,  40,  30,  20,  10,
     20,  30,  40,  50,  50,  40,  30,  20,
     30,  40,  50,  60,  60,  50,  40,  30,  
     30,  40,  50,  60,  60,  50,  40,  30,
     20,  30,  40,  50,  50,  40,  30,  20,
     10,  20,  30,  40,  40,  30,  20,  10,
      0,  10,  20,  30,  30,  20,  10,   0    
};

const std::int8_t kEvalQueenPosition[64] = {
    -5, -5, -5, -5, -5, -5, -5, -5,   
    -5,  0,  0,  0,  0,  0,  0, -5,   
    -5,  0,  5,  5,  5,  5,  0, -5,   
    -5,  0,  5,  5,  5,  5,  0, -5,   
    -5,  0,  5,  5,  5,  5,  0, -5,   
    -5,  0,  5,  5,  5,  5,  0, -5,   
    -5,  0,  0,  0,  0,  0,  0, -5,   
    -5, -5, -5, -5, -5, -5, -5, -5 
};

const std::int8_t kEvalQueenPositionOpening[64] = {
    -5, -5, -5, -5, -5, -5, -5, -5,   
    -5,  0,  0,  0,  0,  0,  0, -5,   
    -5,  0, -5, -5, -5, -5,  0, -5,   
    -5,  0, -5, -5, -5, -5,  0, -5,   
    -5,  0, -5, -5, -5, -5,  0, -5,   
    -5,  0, -5, -5, -5, -5,  0, -5,   
    -5,  0,  0,  0,  0,  0,  0, -5,   
    -5, -5, -5, -5, -5, -5, -5, -5 
};

const std::int8_t kEvalBishopPosition[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,  
    -10,   0,   0,   0,   0,   0,   0, -10,  
    -10,   0,   5,   5,   5,   5,   0, -10,  
    -10,   0,   5,  10,  10,   5,   0, -10,  
    -10,   0,   5,  10,  10,   5,   0, -10,  
    -10,   0,   5,   5,   5,   5,   0, -10,  
    -10,   0,   0,   0,   0,   0,   0, -10,  
    -20, -10, -10, -10, -10, -10, -10, -20, 
};

const std::int8_t kEvalKnightPosition[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50,
};

const std::uint8_t kWhiteRookMasks[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    30, 30, 30, 30, 30, 30, 30, 30,
    20, 20, 20, 20, 20, 20, 20, 20,
};

const std::uint8_t kBlackRookMasks[64] = {
    20, 20, 20, 20, 20, 20, 20, 20,
    30, 30, 30, 30, 30, 30, 30, 30,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

const std::uint8_t kEvalPassingWhitePawns[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    50, 50, 50, 50, 50, 50, 50, 50,
    55, 55, 55, 55, 55, 55, 55, 55,
    60, 60, 60, 60, 60, 60, 60, 60,
    70, 70, 70, 70, 70, 70, 70, 70,
    80, 80, 80, 80, 80, 80, 80, 80,
    130, 130, 130, 130, 130, 130, 130, 130,
    0, 0, 0, 0, 0, 0, 0, 0,
};

const std::uint8_t kEvalPassingBlackPawns[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    130, 130, 130, 130, 130, 130, 130, 130,
    80, 80, 80, 80, 80, 80, 80, 80,
    70, 70, 70, 70, 70, 70, 70, 70,
    60, 60, 60, 60, 60, 60, 60, 60,
    55, 55, 55, 55, 55, 55, 55, 55,
    50, 50, 50, 50, 50, 50, 50, 50,
    0, 0, 0, 0, 0, 0, 0, 0,
};

const std::uint8_t kEvalWhitePawns[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    10, 10, 10, 10, 10, 10, 10, 10,
    20, 20, 20, 20, 20, 20, 20, 20,
    30, 30, 30, 30, 30, 30, 30, 30,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

const std::uint8_t kEvalBlackPawns[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    30, 30, 30, 30, 30, 30, 30, 30,
    20, 20, 20, 20, 20, 20, 20, 20,
    10, 10, 10, 10, 10, 10, 10, 10,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

const bool kSquareWhiteColor[64] = {
    true, false, true, false, true, false, true, false,
    false, true, false, true, false, true, false, true,
    true, false, true, false, true, false, true, false,
    false, true, false, true, false, true, false, true, 
    true, false, true, false, true, false, true, false,
    false, true, false, true, false, true, false, true, 
    true, false, true, false, true, false, true, false,
    false, true, false, true, false, true, false, true, 
};

Bitboard masks_for_white_passing_pawn[64];
Bitboard masks_for_black_passing_pawn[64];

Bitboard masks_for_isolated_pawn[64];

Bitboard line_behind_white_pawn[64];
Bitboard line_behind_black_pawn[64];

Bitboard white_pawns_shield[64];
Bitboard black_pawns_shield[64];

Bitboard columns[64];

const Bitboard kWhiteSquares = 0xAA55AA55AA55AA55;
const Bitboard kBlackSquares = 0x55AA55AA55AA55AA;

const int price[5] = {100, 290, 310, 485, 926};