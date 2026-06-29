#include "evaluate_position.h"

#include <algorithm>

#include "../..//movegen/generate_moves.h"
#include "../../utils/bit_fun.h"

namespace {

inline bool Check2Bits(const Bitboard& x) {
    return x & (x - 1);
}

inline bool Check3Bits(const Bitboard& x) {
    return x & (x - 1) & (x - 2);
}

bool IsOpening(const Board& board) {
    int count = popcount(board.rotated.occupied);
    return (count <= 28) ? false : true;
}

}

int Board::EvaluatePosition() const {
    using namespace eval;
    
    int eval = 0;
    int mobility = 0;
    Bitboard mask;
    Bitboard pieces;
    Bitboard white;
    Bitboard black;
    std::uint8_t square;
    const Bitboard& white_pawns = bitboards[0][0];
    const Bitboard& black_pawns = bitboards[1][0];

    for (int i = 0; i < 5; i++) {
        eval += (popcount(bitboards[0][i]) - popcount(bitboards[1][i])) * price[i]; // Материал
        white |= bitboards[0][i];
        black |= bitboards[1][i];
    }

    Bitboard not_white = ~white;
    Bitboard not_black = ~black;

    eval += (popcount((white_pawns & (white_pawns << 9) & 0xFEFEFEFEFEFEFEFE) | (white_pawns & (white_pawns << 7) & 0x7F7F7F7F7F7F7F7F)) 
    - popcount((black_pawns&(black_pawns >> 9) & 0x7F7F7F7F7F7F7F7F) | (black_pawns & (black_pawns >> 7) & 0xFEFEFEFEFEFEFEFE))) * 7; // Пешечная цепь

    Bitboard w_pawns = bitboards[0][0];
    while (w_pawns != 0) { // Пешечная структура белых
        square = pop_lsb(w_pawns);
        mask = line_behind_white_pawn[square] & white_pawns;
        eval -= (mask == 0) ? 0 : Check2Bits(mask) ? 40 : 30;
        if ((masks_for_white_passing_pawn[square] & black_pawns) == 0) {
            eval += kEvalPassingWhitePawns[square];
            eval += ((line_behind_white_pawn[square] & bitboards[0][3]) != 0) ? 20 : 0;
        } else
            eval += kEvalWhitePawns[square];
        eval -= ((masks_for_isolated_pawn[square] & white_pawns) == 0) ? 25 : 0;
    }

    Bitboard b_pawns = bitboards[1][0];
    while (b_pawns != 0) { // Пешечная структура черных
        square = pop_lsb(b_pawns);
        mask = line_behind_black_pawn[square] & black_pawns;
        eval += (mask == 0) ? 0 : Check2Bits(mask) ? 40 : 30;
        if ((masks_for_black_passing_pawn[square] & white_pawns) == 0) {
            eval -= kEvalPassingBlackPawns[square];
            eval -= ((line_behind_black_pawn[square] & bitboards[1][3]) != 0) ? 20 : 0;
        } else
            eval -= kEvalBlackPawns[square];
        eval += ((masks_for_isolated_pawn[square] & black_pawns) == 0) ? 25 : 0;
    }

    if (IsOpening(*this)) {
        mask = white_pawns & 0x1818000000; // Центральные пешки в дебюте
        eval += (mask != 0) ? (Check2Bits(mask) ? 60 : 30) : 0;
        mask = black_pawns & 0x1818000000;
        eval -= (mask != 0) ? (Check2Bits(mask) ? 60 : 30) : 0;
        eval += (castling[0]) ? 7 : 0;
        eval += (castling[1]) ? 7 : 0;
        eval -= (castling[2]) ? 7 : 0;
        eval -= (castling[3]) ? 7 : 0;

        pieces = bitboards[0][1]; // Оценка белого коня в дебюте
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval += kEvalKnightPosition[square];
            mobility += popcount(movegen::GetKnightAttack(square) & not_white) * KNIGHT_OPENING_MOBILITY;
        }
        
        pieces = bitboards[0][2]; // Оценка белого слона в дебюте
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval += kEvalBishopPosition[square];
            mobility += popcount(movegen::GetBishopAttack(square, rotated) & not_white) * BISHOP_OPENING_MOBILITY;
        }

        pieces = bitboards[0][3]; // Оценка белой ладьи в дебюте
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval += kWhiteRookMasks[square];
        }

        pieces = bitboards[0][4]; // Оценка белого ферзя в дебюте
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval += kEvalQueenPositionOpening[square];
        }

        pieces = bitboards[1][1]; // Оценка черного коня в дебюте
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval -= kEvalKnightPosition[square];
            mobility -= popcount(movegen::GetKnightAttack(square) & not_black) * KNIGHT_OPENING_MOBILITY;
        }
        
        pieces = bitboards[1][2]; // Оценка черного слона в дебюте
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval -= kEvalBishopPosition[square];
            mobility -= popcount(movegen::GetBishopAttack(square, rotated) & not_black) * BISHOP_OPENING_MOBILITY;
        }

        pieces = bitboards[1][3]; // Оценка черной ладьи в дебюте
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval -= kBlackRookMasks[square];
        }

        pieces = bitboards[1][4]; // Оценка черного ферзя в дебюте
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval -= kEvalQueenPositionOpening[square];
        }

        square = bsf(bitboards[0][5]);
        eval += kEvalWhiteKingPosition[square];
        eval += popcount(white_pawns_shield[square] & white_pawns) * PAWN_SHIELD; // Пешечный щит в дебюте

        square = bsf(bitboards[1][5]);
        eval -= kEvalBlackKingPosition[square];
        eval -= popcount(black_pawns_shield[square] & black_pawns) * PAWN_SHIELD;
    } else if (IsEndgame()) {

        pieces = bitboards[0][1]; // Оценка белого коня в эндшпиле
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval += kEvalKnightPosition[square];
        }
        
        pieces = bitboards[0][2]; // Оценка белого слона в эндшпиле
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval += kEvalBishopPosition[square];
            if (kSquareWhiteColor[square])
                eval += popcount(bitboards[0][0] & kBlackSquares) * 3; // Пешки не цвета слона
            else
                eval += popcount(bitboards[0][0] & kWhiteSquares) * 3;
        }

        pieces = bitboards[0][3]; // Оценка белой ладьи в миттельшпиле
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval += kWhiteRookMasks[square];
            eval += ((columns[square] & white_pawns) != 0) ? 0 : ((columns[square] & black_pawns) == 0) ? 20 : 10;
            mobility += popcount(movegen::GetRookAttack(square, rotated) & not_white) * ROOK_MOBILITY;
        }

        pieces = bitboards[1][1]; // Оценка черного коня в эндшпиле
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval -= kEvalKnightPosition[square];
        }
        
        pieces = bitboards[1][2]; // Оценка черного слона в эндшпиле
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval -= kEvalBishopPosition[square];
            if (kSquareWhiteColor[square])
                eval -= popcount(bitboards[1][0] & kBlackSquares) * 3; // Пешки не цвета слона
            else
                eval -= popcount(bitboards[1][0] & kWhiteSquares) * 3;
        }

        pieces = bitboards[1][3]; // Оценка черной ладьи в миттельшпиле
        while (pieces != 0) {
            square = pop_lsb(pieces);
            eval -= kBlackRookMasks[square];
            eval -= ((columns[square] & black_pawns) != 0) ? 0 : ((columns[square] & white_pawns) == 0) ? 20 : 10;
            mobility -= popcount(movegen::GetRookAttack(square, rotated) & not_black) * ROOK_MOBILITY;
        }

        square = bsf(bitboards[0][5]);
        eval += kEvalKingPositionEndgame[square];

        square = bsf(bitboards[1][5]);
        eval -= kEvalKingPositionEndgame[square];

        eval += mobility;
        mobility = 0;

        if (popcount(bitboards[0][2]) == 1 && popcount(bitboards[1][2]) == 1) { // Разноцветные слоны
            if ((((bitboards[0][2] | bitboards[1][2]) & kWhiteSquares) == 0) || ((bitboards[0][2] | bitboards[1][2]) & kBlackSquares) == 0){
                return (eval > 100) ? (eval - 100) : ((eval < 100) ? (eval + 100) : 0);
            }
        }

    } else {
        Bitboard white_attack = 0;
        Bitboard black_attack = 0;
        Bitboard attack;
        mask = white_pawns & 0x1818000000; // Центральные пешки в миттельшпиле
        eval += (mask != 0) ? (Check2Bits(mask) ? 50 : 25) : 0;
        mask = black_pawns & 0x1818000000;
        eval -= (mask != 0) ? (Check2Bits(mask) ? 50 : 25) : 0;
        eval += (castling[0]) ? 7 : 0;
        eval += (castling[1]) ? 7 : 0;
        eval -= (castling[2]) ? 7 : 0;
        eval -= (castling[3]) ? 7 : 0;

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
    }

    eval += Check2Bits(bitboards[0][2]) ? 40 : 0; // Преимущество двух слонов
    eval -= Check2Bits(bitboards[1][2]) ? 40 : 0;

    eval += (turn) ? 5 : -5;
    return eval + mobility;
}