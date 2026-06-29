#include "generate_moves.h"

#include <iostream>

#include "../utils/bit_fun.h"

bool Board::LegalTest(bool local_turn) const {
    const Bitboard& king = bitboards[local_turn][5];
    std::uint8_t square = bsf(king);
    if (!local_turn) {
        if ((((king << 7) & 0x7F7F7F7F7F7F7F7F & bitboards[1][0]) != 0) || (((king << 9) & 0xFEFEFEFEFEFEFEFE & bitboards[1][0]) != 0))
            return false;
    } else {
        if ((((king >> 7) & 0xFEFEFEFEFEFEFEFE & bitboards[0][0]) != 0) || (((king >> 9) & 0x7F7F7F7F7F7F7F7F & bitboards[0][0]) != 0))
            return false;
    }
    if ((bitboards[!local_turn][1] & movegen::kMasksForKnight[square]) != 0 || (bitboards[!local_turn][5] & movegen::kMasksForKing[square]) != 0)
        return false;
    Bitboard diag_attack = movegen::GetBishopAttack(square, rotated);
    if ((diag_attack & bitboards[!local_turn][2]) != 0 || (diag_attack & bitboards[!local_turn][4]) != 0)
        return false;
    Bitboard rank_attack = movegen::GetRookAttack(square, rotated);
    if ((rank_attack & bitboards[!local_turn][3]) != 0 || (rank_attack & bitboards[!local_turn][4]) != 0) 
        return false;
    return true;
}