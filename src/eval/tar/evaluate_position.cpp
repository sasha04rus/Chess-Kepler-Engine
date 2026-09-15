#include "evaluate_position.h"

#include <algorithm>

#include "../../movegen/generate_moves.h"
#include "../../utils/bit_fun.h"

namespace {

inline bool Check2Bits(const Bitboard& x) {
    return x & (x - 1);
}

constexpr unsigned int PAWN_HASH_SIZE = 1 << 14;

struct PawnHashEntry {
    Bitboard white_pawns = 0;
    Bitboard black_pawns = 0;
    int evaluation = 0;
    bool valid = false;
};

thread_local std::array<PawnHashEntry, PAWN_HASH_SIZE> pawn_hash;

int EvaluatePawnStructure(Bitboard white_pawns, Bitboard black_pawns) {
    using namespace eval;
    std::uint64_t hash = white_pawns ^ ((black_pawns << 32) | (black_pawns >> 32));
    auto& entry = pawn_hash[hash & (PAWN_HASH_SIZE - 1)];
    if (entry.valid && entry.white_pawns == white_pawns && entry.black_pawns == black_pawns)
        return entry.evaluation;
    int eval = (popcount((white_pawns & (white_pawns << 9) & 0xFEFEFEFEFEFEFEFE) | (white_pawns & (white_pawns << 7) & 0x7F7F7F7F7F7F7F7F)) 
    - popcount((black_pawns & (black_pawns >> 9) & 0x7F7F7F7F7F7F7F7F) | (black_pawns & (black_pawns >> 7) & 0xFEFEFEFEFEFEFEFE))) * 7;

    std::uint8_t square;
    Bitboard mask;
    Bitboard pawns = white_pawns;
    int result;
    while (pawns != 0) {
        square = pop_lsb(pawns);
        mask = line_behind_white_pawn[square] & white_pawns;
        if (mask != 0) eval -= 50;
        if ((masks_for_white_passing_pawn[square] & black_pawns) == 0)
            eval += kEvalPassingWhitePawns[square];
        else
            eval += kEvalWhitePawns[square];
        eval -= ((masks_for_isolated_pawn[square] & white_pawns) == 0) ? 20 : 0;
    }

    pawns = black_pawns;
    while (pawns != 0) {
        square = pop_lsb(pawns);
        mask = line_behind_black_pawn[square] & black_pawns;
        if (mask != 0) eval += 50;
        if ((masks_for_black_passing_pawn[square] & white_pawns) == 0)
            eval -= kEvalPassingBlackPawns[square];
        else
            eval -= kEvalBlackPawns[square];
        eval += ((masks_for_isolated_pawn[square] & black_pawns) == 0) ? 20 : 0;
    }

    entry.white_pawns = white_pawns;
    entry.black_pawns = black_pawns;
    entry.evaluation = eval;
    entry.valid = true;
    return eval;
}

}

int Board::EvaluateTarPosition() const {
    using namespace eval;
    
    int eval = 0;
    int mobility = 0;
    Bitboard mask;
    Bitboard pieces;
    Bitboard white = bitboards[0][5];
    Bitboard black = bitboards[1][5];
    std::uint8_t square;
    const Bitboard& white_pawns = bitboards[0][0];
    const Bitboard& black_pawns = bitboards[1][0];

    for (int i = 0; i < 5; i++) {
        white |= bitboards[0][i];
        black |= bitboards[1][i];
    }
    
    if ((popcount(white_pawns) != 8) || (popcount(black_pawns) != 8))
        for (int i = 0; i < 5; i++)
            eval += (popcount(bitboards[0][i]) - popcount(bitboards[1][i])) * price[i]; // Материал

    Bitboard not_white = ~white;
    Bitboard not_black = ~black;

    int pawn_eval = EvaluatePawnStructure(white_pawns, black_pawns);
    eval += pawn_eval;
        
    Bitboard white_attack = ((white_pawns << 9) & 0xFEFEFEFEFEFEFEFEULL) | ((white_pawns << 7) & 0x7F7F7F7F7F7F7F7FULL);
    Bitboard black_attack = ((black_pawns >> 9) & 0x7F7F7F7F7F7F7F7FULL) | ((black_pawns >> 7) & 0xFEFEFEFEFEFEFEFEULL);
    Bitboard attack;
    eval += (castling[0]) ? 8 : 0;
    eval += (castling[1]) ? 8 : 0;
    eval -= (castling[2]) ? 8 : 0;
    eval -= (castling[3]) ? 8 : 0;

    pieces = bitboards[0][1]; // Оценка белого коня в миттельшпиле
    while (pieces != 0) {
        square = pop_lsb(pieces);
        eval += kEvalKnightPosition[square];
        attack = movegen::GetKnightAttack(square) & not_white;
        white_attack |= attack;
        mobility += popcount(attack) * KNIGHT_MOBILITY;
    }
    
    pieces = bitboards[0][2]; // Оценка белого слона в миттельшпиле
    while (pieces != 0) {
        square = pop_lsb(pieces);
        eval += kEvalBishopPosition[square];
        attack = movegen::GetBishopAttack(square, rotated) & not_white;
        white_attack |= attack;
        mobility += popcount(attack) * BISHOP_MOBILITY;
    }

    pieces = bitboards[0][3]; // Оценка белой ладьи в миттельшпиле
    while (pieces != 0) {
        square = pop_lsb(pieces);
        eval += kWhiteRookMasks[square];
        attack = movegen::GetRookAttack(square, rotated);
        if ((attack & bitboards[0][3]) != 0)
            eval += CONNECTION_ROOK; // Связь ладей
        eval += ((columns[square] & white_pawns) != 0) ? 0 : ((columns[square] & black_pawns) == 0) ? 20 : 10;
        white_attack |= attack;
        mobility += popcount(attack & not_white) * ROOK_MOBILITY;
    }

    pieces = bitboards[0][4]; // Оценка белого ферзя в миттельшпиле
    while (pieces != 0) {
        square = pop_lsb(pieces);
        eval += kEvalQueenPosition[square];
        attack = (movegen::GetBishopAttack(square, rotated) | movegen::GetRookAttack(square, rotated)) & not_white;
        white_attack |= attack;
        mobility += popcount(attack) * QUEEN_MOBILITY;
    }

    pieces = bitboards[1][1]; // Оценка черного коня в миттельшпиле
    while (pieces != 0) {
        square = pop_lsb(pieces);
        eval -= kEvalKnightPosition[square];
        attack = movegen::GetKnightAttack(square) & not_black;
        black_attack |= attack;
        mobility -= popcount(attack) * KNIGHT_MOBILITY;
    }
    
    pieces = bitboards[1][2]; // Оценка черного слона в миттельшпиле
    while (pieces != 0) {
        square = pop_lsb(pieces);
        eval -= kEvalBishopPosition[square];
        attack = movegen::GetBishopAttack(square, rotated) & not_black;
        black_attack |= attack;
        mobility -= popcount(attack) * BISHOP_MOBILITY;
    }

    pieces = bitboards[1][3]; // Оценка черной ладьи в миттельшпиле
    while (pieces != 0) {
        square = pop_lsb(pieces);
        eval -= kBlackRookMasks[square];
        attack = movegen::GetRookAttack(square, rotated);
        if ((attack & bitboards[1][3]) != 0)
            eval -= CONNECTION_ROOK; // Связь ладей
        eval -= ((columns[square] & black_pawns) != 0) ? 0 : ((columns[square] & white_pawns) == 0) ? 20 : 10;
        black_attack |= attack;
        mobility -= popcount(attack & not_black) * ROOK_MOBILITY;
    }

    pieces = bitboards[1][4]; // Оценка черного ферзя в миттельшпиле
    while (pieces != 0) {
        square = pop_lsb(pieces);
        eval -= kEvalQueenPosition[square];
        attack = (movegen::GetBishopAttack(square, rotated) | movegen::GetRookAttack(square, rotated)) & not_black;
        black_attack |= attack;
        mobility -= popcount(attack) * QUEEN_MOBILITY;
    }

    square = bsf(bitboards[0][5]);
    eval += kEvalWhiteKingPosition[square];
    eval -= popcount(movegen::kMasksForKing[square] & black_attack) * ATTACKED_SQUARE;
    eval += popcount(white_pawns_shield[square] & white_pawns) * PAWN_SHIELD; // Пешечный щит

    square = bsf(bitboards[1][5]);
    eval -= kEvalBlackKingPosition[square];
    eval += popcount(movegen::kMasksForKing[square] & white_attack) * ATTACKED_SQUARE;
    eval -= popcount(black_pawns_shield[square] & black_pawns) * PAWN_SHIELD;

    eval += popcount(white_attack & bitboards[1][0]) * ATTACK_PAWN; // Угроза перемещения фигуры
    eval += popcount(white_attack & bitboards[1][1]) * ATTACK_KHIGHT;
    eval += popcount(white_attack & bitboards[1][2]) * ATTACK_BISHOP;
    eval += popcount(white_attack & bitboards[1][3]) * ATTACK_ROOK;
    eval += popcount(white_attack & bitboards[1][4]) * ATTACK_QUEEN;
    eval -= popcount(black_attack & bitboards[0][0]) * ATTACK_PAWN;
    eval -= popcount(black_attack & bitboards[0][1]) * ATTACK_KHIGHT;
    eval -= popcount(black_attack & bitboards[0][2]) * ATTACK_BISHOP;
    eval -= popcount(black_attack & bitboards[0][3]) * ATTACK_ROOK;
    eval -= popcount(black_attack & bitboards[0][4]) * ATTACK_QUEEN;

    eval += (turn) ? 5 : -5;
    return eval + mobility;
}