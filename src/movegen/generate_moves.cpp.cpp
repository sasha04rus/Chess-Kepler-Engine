#include "generate_moves.h"

#include <cstdint>

#include "../board/board.h"
#include "../moves/standard/move.h"
#include "../moves/flag.h"
#include "../utils/bit_fun.h"

namespace {

bool CheckUnderAttackE1F1(const Board& board) {
    if (((0x1F00 & board.bitboards[1][0]) != 0) || ((0x1F3B00 & board.bitboards[1][1]) != 0) || ((0x1E10 & board.bitboards[1][5]) != 0)) return false;
    Bitboard mask = movegen::GetBishopAttack(2, board.rotated);
    mask |= movegen::GetBishopAttack(3, board.rotated);
    if((mask & board.bitboards[1][2]) != 0 || (mask & board.bitboards[1][4]) != 0) return false;
    mask = movegen::GetRookAttack(2, board.rotated);
    mask |= movegen::GetRookAttack(3, board.rotated);
    if((mask & board.bitboards[1][3]) != 0 || (mask & board.bitboards[1][4]) != 0) return false;
    return true;
}

bool CheckUnderAttackD1E1(const Board& board) {
    if (((0x7C00&board.bitboards[1][0]) != 0) || ((0x7CEE00&board.bitboards[1][1]) != 0) || ((0x7C04&board.bitboards[1][5]) != 0)) return false;
    Bitboard mask = movegen::GetBishopAttack(4, board.rotated);
    mask |= movegen::GetBishopAttack(3, board.rotated);
    if((mask & board.bitboards[1][2]) != 0 || (mask & board.bitboards[1][4]) != 0) return false;
    mask = movegen::GetRookAttack(4, board.rotated);
    mask |= movegen::GetRookAttack(3, board.rotated);
    if((mask & board.bitboards[1][3]) != 0 || (mask & board.bitboards[1][4]) != 0) return false;
    return true;
}

bool CheckUnderAttackE8F8(const Board& board) {
    if (((0x1F000000000000 & board.bitboards[0][0]) != 0) || ((0x3B1F0000000000 & board.bitboards[0][1]) != 0) || ((0x101E000000000000 & board.bitboards[0][5]) != 0)) return false;
    Bitboard mask = movegen::GetBishopAttack(58, board.rotated);
    mask |= movegen::GetBishopAttack(59, board.rotated);
    if((mask & board.bitboards[0][2]) != 0 || (mask & board.bitboards[0][4]) != 0) return false;
    mask = movegen::GetRookAttack(58, board.rotated);
    mask |= movegen::GetRookAttack(59, board.rotated);
    if((mask & board.bitboards[0][3]) != 0 || (mask & board.bitboards[0][4]) != 0) return false;
    return true;
}

bool CheckUnderAttackD8E8(const Board& board) {
    if (((0x7C000000000000 & board.bitboards[0][0]) != 0) || ((0xE7CE0000000000 & board.bitboards[0][1]) !=0 ) || ((0x047C000000000000 & board.bitboards[0][5]) != 0)) return false;
    Bitboard mask = movegen::GetBishopAttack(60, board.rotated);
    mask |= movegen::GetBishopAttack(59, board.rotated);
    if((mask & board.bitboards[0][2]) != 0 || (mask & board.bitboards[0][4]) != 0) return false;
    mask = movegen::GetRookAttack(60, board.rotated);
    mask |= movegen::GetRookAttack(59, board.rotated);
    if((mask & board.bitboards[0][3]) != 0 || (mask & board.bitboards[0][4]) != 0) return false;
    return true;
}

}

namespace movegen {

static const std::uint8_t kKnightShifts[] = {10, 17, 15, 6};
static const std::uint8_t kKingShifts[] = {1, 7, 8, 9};

Bitboard GetPawnWhiteLeftCapture(const Bitboard& pawns, const Bitboard& enemy) {
    return (pawns << 9) & 0xFEFEFEFEFEFEFEFE & enemy;
}

Bitboard GetPawnWhiteRightCapture(const Bitboard& pawns, const Bitboard& enemy) {
    return (pawns << 7) & 0x7F7F7F7F7F7F7F7F & enemy;
}

Bitboard GetPawnBlackLeftCapture(const Bitboard& pawns, const Bitboard& enemy) {
    return (pawns >> 7) & 0xFEFEFEFEFEFEFEFE & enemy;
}

Bitboard GetPawnBlackRightCapture(const Bitboard& pawns, const Bitboard& enemy) {
    return (pawns >> 9) & 0x7F7F7F7F7F7F7F7F & enemy;
}

Bitboard GetKnightAttack(std::uint8_t sq) {
    return kMasksForKnight[sq];
}

Bitboard GetKingAttack(std::uint8_t sq) {
    return kMasksForKing[sq];
}

Bitboard GetRookAttack(std::uint8_t sq, const RotatedBoard& rotate) {
    std::uint8_t rank = sq / 8;
    std::uint8_t file = sq % 8;
    std::uint8_t horizontal_attack = rank_attacks[sq][compress[(uint8_t)(rotate.occupied >> (8 * rank))]];
    Bitboard vertical_attack = file_attacks[sq][compress[(uint8_t)(rotate.rotate90 >> (8 * file))]];
    return vertical_attack | ((Bitboard)horizontal_attack << (8 * rank));
}

Bitboard GetBishopAttack(std::uint8_t sq, const RotatedBoard& rotate) {
    int shift_1 = kDiagLengthPrefSum[diag_h1a8[sq]];
    uint8_t state_1 = (rotate.rotate45 >> shift_1);
    int shift_2 = kDiagLengthPrefSum[diag_a1h8[sq]];
    uint8_t state_2 = (rotate.rotate315 >> shift_2);
    return diaga1h8_attacks[sq][compress[state_1]] | diagh1a8_attacks[sq][compress[state_2]];
}

int GenerateMoves(const Board& board, Move possible_moves[218], bool only_captures) {
    int index = 0;

    Bitboard ally = 0;
    Bitboard ioccupied = (board.rotated.occupied ^ UINT64_MAX);
    std::uint8_t square;
    for (int i = 0; i < 6; i++)
        ally |= board.bitboards[!board.turn][i];
    Bitboard ially = (ally ^ UINT64_MAX);
    Bitboard enemy = ally ^ board.rotated.occupied;
    Bitboard mask;
    if (board.turn) {
        mask = GetPawnWhiteLeftCapture(board.bitboards[0][0], enemy);
        while (mask != 0) {
            std::uint8_t captured_piece;
            square = pop_lsb(mask);
            if (square >= 56) {
                captured_piece = board.pieces[square];
                possible_moves[index++] = Move(square - 9, square, 0, captured_piece, Flag::kTransformationToQueenWithCapture);
                possible_moves[index++] = Move(square - 9, square, 0, captured_piece, Flag::kTransformationToKnightWithCapture);
                possible_moves[index++] = Move(square - 9, square, 0, captured_piece, Flag::kTransformationToRookWithCapture);
                possible_moves[index++] = Move(square - 9, square, 0, captured_piece, Flag::kTransformationToBishopWithCapture);
            } else {
                captured_piece = board.pieces[square];
                possible_moves[index++] = Move(square - 9, square, 0, captured_piece, Flag::kCapture);
            }
        }
        mask = GetPawnWhiteRightCapture(board.bitboards[0][0], enemy);
        while (mask != 0) {
            std::uint8_t captured_piece;
            square = pop_lsb(mask);
            if (square >= 56) {
                captured_piece = board.pieces[square];
                possible_moves[index++] = Move(square - 7, square, 0, captured_piece, Flag::kTransformationToQueenWithCapture);
                possible_moves[index++] = Move(square - 7, square, 0, captured_piece, Flag::kTransformationToKnightWithCapture);
                possible_moves[index++] = Move(square - 7, square, 0, captured_piece, Flag::kTransformationToRookWithCapture);
                possible_moves[index++] = Move(square - 7, square, 0, captured_piece, Flag::kTransformationToBishopWithCapture);
            } else {
                captured_piece = board.pieces[square];
                possible_moves[index++] = Move(square - 7, square, 0, captured_piece, Flag::kCapture);
            }
        }  
    } else {
        mask = GetPawnBlackLeftCapture(board.bitboards[1][0], enemy);
        while (mask != 0) {
            std::uint8_t captured_piece;
            square = pop_lsb(mask);
            if (square <= 7) {
                captured_piece = board.pieces[square];
                possible_moves[index++] = Move(square + 7, square, 0, captured_piece, Flag::kTransformationToQueenWithCapture);
                possible_moves[index++] = Move(square + 7, square, 0, captured_piece, Flag::kTransformationToKnightWithCapture);
                possible_moves[index++] = Move(square + 7, square, 0, captured_piece, Flag::kTransformationToRookWithCapture);
                possible_moves[index++] = Move(square + 7, square, 0, captured_piece, Flag::kTransformationToBishopWithCapture);
            } else {
                captured_piece = board.pieces[square];
                possible_moves[index++] = Move(square + 7, square, 0, captured_piece, Flag::kCapture);
            }
        }
        mask = GetPawnBlackRightCapture(board.bitboards[1][0], enemy);
        while (mask != 0) {
            std::uint8_t captured_piece;
            square = pop_lsb(mask);
            if (square <= 7) {
                captured_piece = board.pieces[square];
                possible_moves[index++] = Move(square + 9, square, 0, captured_piece, Flag::kTransformationToQueenWithCapture);
                possible_moves[index++] = Move(square + 9, square, 0, captured_piece, Flag::kTransformationToKnightWithCapture);
                possible_moves[index++] = Move(square + 9, square, 0, captured_piece, Flag::kTransformationToRookWithCapture);
                possible_moves[index++] = Move(square + 9, square, 0, captured_piece, Flag::kTransformationToBishopWithCapture);
            } else {
                captured_piece = board.pieces[square];
                possible_moves[index++] = Move(square + 9, square, 0, captured_piece, Flag::kCapture);
            }
        }
    }

    if (board.en_passant != 0) {
        if (board.en_passant > 32) {
            mask = ((uint64_t)1 << board.en_passant);
            if (((mask >> 7) & board.bitboards[0][0] & 0xFEFEFEFEFEFEFEFE) != 0)
                possible_moves[index++] = Move(board.en_passant - 7, board.en_passant, 0, 0, Flag::kEnPassant);
            if (((mask >> 9) & board.bitboards[0][0] & 0x7F7F7F7F7F7F7F7F) != 0)
                possible_moves[index++] = Move(board.en_passant - 9, board.en_passant, 0, 0, Flag::kEnPassant);
        } else {
            mask = ((uint64_t)1 << board.en_passant);
            if (((mask << 9) & board.bitboards[1][0] & 0xFEFEFEFEFEFEFEFE) != 0)
                possible_moves[index++] = Move(board.en_passant + 9, board.en_passant, 0, 0, Flag::kEnPassant);
            if (((mask << 7) & board.bitboards[1][0] & 0x7F7F7F7F7F7F7F7F) != 0)
                possible_moves[index++] = Move(board.en_passant + 7, board.en_passant, 0, 0, Flag::kEnPassant);
        }
    }

    if (!only_captures) {
        if (board.turn) {
            if (board.castling[0] && ((board.rotated.occupied & 0x6) == 0) && CheckUnderAttackE1F1(board))
                possible_moves[index++] = Move(3, 1, 5, 255, Flag::kShortWhiteCastling);
            if (board.castling[1] && ((board.rotated.occupied & 0x70) == 0) && CheckUnderAttackD1E1(board))
                possible_moves[index++] = Move(3, 5, 5, 255, Flag::kLongWhiteCastling);
        } else {
            if (board.castling[2] && ((board.rotated.occupied & 0x600000000000000) == 0) && CheckUnderAttackE8F8(board))
                possible_moves[index++] = Move(59, 57, 5, 255, Flag::kShortBlackCastling);
            if (board.castling[3] && ((board.rotated.occupied & 0x7000000000000000) == 0) && CheckUnderAttackD8E8(board))
                possible_moves[index++] = Move(59, 61, 5, 255, Flag::kLongBlackCastling);
        }
    }

    if (!only_captures) {
        if (board.turn) {
            mask = (board.bitboards[0][0] << 8) & ioccupied;
            Bitboard mask_for_long_move = ((0xFF0000 & mask) << 8) & ioccupied;
            while (mask != 0) {
                square = pop_lsb(mask);
                if (square >= 56) {
                    possible_moves[index++] = Move(square - 8, square, 0, 0, Flag::kTransformationToQueen);
                    possible_moves[index++] = Move(square - 8, square, 0, 0, Flag::kTransformationToKnight);
                    possible_moves[index++] = Move(square - 8, square, 0, 0, Flag::kTransformationToRook);
                    possible_moves[index++] = Move(square - 8, square, 0, 0, Flag::kTransformationToBishop);
                    continue;
                }
                possible_moves[index++] = Move(square - 8, square, 0);
            }
            while (mask_for_long_move != 0) {
                square = pop_lsb(mask_for_long_move);
                possible_moves[index++] = Move(square - 16, square, 0, 0, Flag::kLongMove);
            }
        } else {
            mask = (board.bitboards[1][0] >> 8) & ioccupied;
            Bitboard mask_for_long_move = ((0xFF0000000000 & mask) >> 8) & ioccupied;
            while (mask != 0) {
                square = pop_lsb(mask);
                if (square <= 7) {
                    possible_moves[index++] = Move(square + 8, square, 0, 0, Flag::kTransformationToQueen);
                    possible_moves[index++] = Move(square + 8, square, 0, 0, Flag::kTransformationToKnight);
                    possible_moves[index++] = Move(square + 8, square, 0, 0, Flag::kTransformationToRook);
                    possible_moves[index++] = Move(square + 8, square, 0, 0, Flag::kTransformationToBishop);
                    continue;
                }
                possible_moves[index++] = Move(square + 8, square, 0);
            }
            while (mask_for_long_move != 0) {
                square = pop_lsb(mask_for_long_move);
                possible_moves[index++] = Move(square + 16, square, 0, 0, Flag::kLongMove);
            }
        }
    }

    Bitboard knights = board.bitboards[!board.turn][1];
    if (only_captures) {
        while (knights != 0) {
            square = pop_lsb(knights);
            mask = GetKnightAttack(square) & enemy;
            while (mask != 0) {
                std::uint8_t square_to = pop_lsb(mask);
                possible_moves[index++] = Move(square, square_to, 1, board.pieces[square_to], Flag::kCapture);
            }
        }
    } else {
        while (knights != 0) {
            square = pop_lsb(knights);
            mask = GetKnightAttack(square) & ially;
            Bitboard mask_captures = mask & enemy;
            mask ^= mask_captures;
            while (mask_captures != 0) {
                std::uint8_t square_to = pop_lsb(mask_captures);
                possible_moves[index++] = Move(square, square_to, 1, board.pieces[square_to], Flag::kCapture);
            }
            while (mask != 0) {
                std::uint8_t square_to = pop_lsb(mask);
                possible_moves[index++] = Move(square, square_to, 1);
            }
        }
    }

    Bitboard bishops = board.bitboards[!board.turn][2];
    if (only_captures) {
        while (bishops != 0) {
            square = pop_lsb(bishops);
            mask = GetBishopAttack(square, board.rotated) & enemy;
            while (mask != 0) {
                std::uint8_t square_to = pop_lsb(mask);
                possible_moves[index++] = Move(square, square_to, 2, board.pieces[square_to], Flag::kCapture);
            }
        }
    } else {
        while (bishops != 0) {
            square = pop_lsb(bishops);
            mask = GetBishopAttack(square, board.rotated) & ially;
            Bitboard mask_captures = mask & enemy;
            mask ^= mask_captures;
            while (mask_captures != 0) {
                std::uint8_t square_to = pop_lsb(mask_captures);
                possible_moves[index++] = Move(square, square_to, 2, board.pieces[square_to], Flag::kCapture);
            }
            while (mask != 0) {
                std::uint8_t square_to = pop_lsb(mask);
                possible_moves[index++] = Move(square, square_to, 2);
            }
        }
    }

    Bitboard rooks = board.bitboards[!board.turn][3];
    if (only_captures) {
        while (rooks != 0) {
            square = pop_lsb(rooks);
            mask = GetRookAttack(square, board.rotated) & enemy;
            while (mask != 0) {
                std::uint8_t square_to = pop_lsb(mask);
                possible_moves[index++] = Move(square, square_to, 3, board.pieces[square_to], Flag::kCapture);
            }
        }
    } else {
        while (rooks != 0) {
            square = pop_lsb(rooks);
            mask = GetRookAttack(square, board.rotated) & ially;
            Bitboard mask_captures = mask & enemy;
            mask ^= mask_captures;
            while (mask_captures != 0) {
                std::uint8_t square_to = pop_lsb(mask_captures);
                possible_moves[index++] = Move(square, square_to, 3, board.pieces[square_to], Flag::kCapture);
            }
            while (mask != 0) {
                std::uint8_t square_to = pop_lsb(mask);
                possible_moves[index++] = Move(square, square_to, 3);
            }
        }
    }

    Bitboard queens = board.bitboards[!board.turn][4];
    if (only_captures) {
        while (queens != 0) {
            square = pop_lsb(queens);
            mask = (GetRookAttack(square, board.rotated) | GetBishopAttack(square, board.rotated)) & enemy;
            while (mask != 0) {
                std::uint8_t square_to = pop_lsb(mask);
                possible_moves[index++] = Move(square, square_to, 4, board.pieces[square_to], Flag::kCapture);
            }
        }
    } else {
        while (queens != 0) {
            square = pop_lsb(queens);
            mask = (GetRookAttack(square, board.rotated) | GetBishopAttack(square, board.rotated)) & ially;
            Bitboard mask_captures = mask & enemy;
            mask ^= mask_captures;
            while (mask_captures != 0) {
                std::uint8_t square_to = pop_lsb(mask_captures);
                possible_moves[index++] = Move(square, square_to, 4, board.pieces[square_to], Flag::kCapture);
            }
            while (mask != 0) {
                std::uint8_t square_to = pop_lsb(mask);
                possible_moves[index++] = Move(square, square_to, 4);
            }
        }
    }

    square = bsf(board.bitboards[!board.turn][5]);
    mask = GetKingAttack(square) & ially;
    Bitboard mask_captures = mask & enemy;
    if (only_captures)
        mask = 0;
    else
        mask ^= mask_captures;

    while (mask_captures != 0) {
        std::uint8_t square_to = pop_lsb(mask_captures);
        possible_moves[index++] = Move(square, square_to, 5, board.pieces[square_to], Flag::kCapture);
    }

    while (mask != 0) {
        std::uint8_t square_to = pop_lsb(mask);
        possible_moves[index++] = Move(square, square_to, 5);
    }

    return index;
}

}