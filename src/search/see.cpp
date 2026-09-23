#include "see.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include "../eval/standard/evaluate_position.h"
#include "../movegen/generate_moves.h"
#include "../utils/bit_fun.h"

namespace {

constexpr std::uint8_t kPawn = 0;
constexpr std::uint8_t kKnight = 1;
constexpr std::uint8_t kBishop = 2;
constexpr std::uint8_t kRook = 3;
constexpr std::uint8_t kQueen = 4;
constexpr std::uint8_t kKing = 5;
constexpr std::uint8_t kPieceCount = 6;

int PieceValue(std::uint8_t piece) {
    constexpr int kKingValue = 20'000;
    return piece == kKing ? kKingValue : eval::price[piece];
}

bool IsCapture(const Move& move) {
    switch (move.flag) {
        case Flag::kCapture:
        case Flag::kTransformationToKnightWithCapture:
        case Flag::kTransformationToBishopWithCapture:
        case Flag::kTransformationToRookWithCapture:
        case Flag::kTransformationToQueenWithCapture:
        case Flag::kEnPassant:
            return true;
        default:
            return false;
    }
}

std::uint8_t ResultingPiece(const Move& move) {
    switch (move.flag) {
        case Flag::kTransformationToKnight:
        case Flag::kTransformationToBishop:
        case Flag::kTransformationToRook:
        case Flag::kTransformationToQueen:
            return static_cast<std::uint8_t>(static_cast<int>(move.flag) - 1);
        case Flag::kTransformationToKnightWithCapture:
        case Flag::kTransformationToBishopWithCapture:
        case Flag::kTransformationToRookWithCapture:
        case Flag::kTransformationToQueenWithCapture:
            return static_cast<std::uint8_t>(static_cast<int>(move.flag) - 5);
        default:
            return move.piece;
    }
}

void RemoveBit(RotatedBoard& rotated, std::uint8_t square) {
    rotated.occupied &= ~(1ULL << square);
    rotated.rotate45 &= ~movegen::map_to_rotate45[square];
    rotated.rotate90 &= ~movegen::map_to_rotate90[square];
    rotated.rotate315 &= ~movegen::map_to_rotate315[square];
}

void AddBit(RotatedBoard& rotated, std::uint8_t square) {
    rotated.occupied |= 1ULL << square;
    rotated.rotate45 |= movegen::map_to_rotate45[square];
    rotated.rotate90 |= movegen::map_to_rotate90[square];
    rotated.rotate315 |= movegen::map_to_rotate315[square];
}

struct SeeMove {
    std::uint8_t from = 0;
    std::uint8_t piece = 0;
};

class SeeContext {
public:
    SeeContext(const Board& board, const Move& first_move) : square_(first_move.to), rotated_(board.rotated) {
        for (std::uint8_t color = 0; color < 2; color++)
            for (std::uint8_t piece = 0; piece < kPieceCount; piece++)
                bitboards_[color][piece] = board.bitboards[color][piece];
        bitboards_[!board.turn][first_move.piece] &= ~(1ULL << first_move.from);
        RemoveBit(rotated_, first_move.from);

        if (IsCapture(first_move)) {
            std::uint8_t captured_square = first_move.to;
            if (first_move.flag == Flag::kEnPassant)
                captured_square = static_cast<std::uint8_t>(first_move.to + (board.turn ? -8 : 8));
            bitboards_[board.turn][first_move.taken_piece] &= ~(1ULL << captured_square);
            RemoveBit(rotated_, captured_square);
        }

        bitboards_[!board.turn][ResultingPiece(first_move)] |= 1ULL << first_move.to;
        AddBit(rotated_, first_move.to);
    }

    bool GetCheapestAttackerMove(bool turn, SeeMove& move) const {
        Bitboard mask = PawnAttackers(turn);
        if (mask != 0) {
            move = {bsf(mask), kPawn};
            return true;
        }

        mask = movegen::GetKnightAttack(square_) & bitboards_[turn][kKnight];
        if (mask != 0) {
            move = {bsf(mask), kKnight};
            return true;
        }

        const Bitboard bishop_attack = movegen::GetBishopAttack(square_, rotated_);
        mask = bishop_attack & bitboards_[turn][kBishop];
        if (mask != 0) {
            move = {bsf(mask), kBishop};
            return true;
        }

        const Bitboard rook_attack = movegen::GetRookAttack(square_, rotated_);
        mask = rook_attack & bitboards_[turn][kRook];
        if (mask != 0) {
            move = {bsf(mask), kRook};
            return true;
        }

        mask = (bishop_attack | rook_attack) & bitboards_[turn][kQueen];
        if (mask != 0) {
            move = {bsf(mask), kQueen};
            return true;
        }

        mask = movegen::GetKingAttack(square_) & bitboards_[turn][kKing];
        if (mask != 0) {
            move = {bsf(mask), kKing};
            return true;
        }

        return false;
    }

    void MakeCapture(bool turn, std::uint8_t captured_piece, const SeeMove& move, std::uint8_t resulting_piece) {
        bitboards_[!turn][captured_piece] &= ~(1ULL << square_);
        bitboards_[turn][move.piece] &= ~(1ULL << move.from);
        bitboards_[turn][resulting_piece] |= 1ULL << square_;
        RemoveBit(rotated_, move.from);
    }

private:
    Bitboard PawnAttackers(bool turn) const {
        const Bitboard target = 1ULL << square_;
        if (!turn)
            return (((target & 0xFEFEFEFEFEFEFEFEULL) >> 9) | ((target & 0x7F7F7F7F7F7F7F7FULL) >> 7)) & bitboards_[0][kPawn];
        return (((target & 0xFEFEFEFEFEFEFEFEULL) << 7) | ((target & 0x7F7F7F7F7F7F7F7FULL) << 9)) & bitboards_[1][kPawn];
    }

    const std::uint8_t square_;
    Bitboard bitboards_[2][kPieceCount]{};
    RotatedBoard rotated_{};
};

std::uint8_t ResultingPiece(const SeeMove& move, std::uint8_t color, std::uint8_t target_square) {
    if (move.piece != kPawn)
        return move.piece;
    const std::uint8_t target_rank = target_square / 8;
    if ((color == 0 && target_rank == 7) || (color == 1 && target_rank == 0))
        return kQueen;
    return kPawn;
}

} // namespace

int See(const Board& board, const Move& move) {
    const std::uint8_t first_piece = ResultingPiece(move);
    const int captured_value = IsCapture(move) ? PieceValue(move.taken_piece) : 0;
    std::array<int, 32> gains{};
    gains[0] = captured_value + PieceValue(first_piece) - PieceValue(move.piece);
    SeeContext context(board, move);
    bool side = board.turn;
    std::uint8_t piece_on_square = first_piece;
    SeeMove reply;

    std::size_t depth = 0;
    while (depth + 1 < gains.size() && context.GetCheapestAttackerMove(side, reply)) {
        const std::uint8_t resulting_piece = ResultingPiece(reply, side, move.to);
        const int promotion_gain = PieceValue(resulting_piece) - PieceValue(reply.piece);
        depth++;
        gains[depth] = PieceValue(piece_on_square) + promotion_gain - gains[depth - 1];
        context.MakeCapture(side, piece_on_square, reply, resulting_piece);
        piece_on_square = resulting_piece;
        side = !side;
        if (reply.piece == kKing) {
            SeeMove defender;
            if (context.GetCheapestAttackerMove(side, defender))
                depth--;
            break;
        }
    }

    while (depth > 0) {
        gains[depth - 1] = -std::max(-gains[depth - 1], gains[depth]);
        depth--;
    }

    return gains[0];
}
