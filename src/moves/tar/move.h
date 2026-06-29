#pragma once

#include <cstdint>

#include "../flag.h"
#include "../standard/move.h"

bool IsReversible(const Move& move) {
    switch (move.flag) {
    case Flag::kDefault:
        return (move.piece == 0) ? true : false;
    case Flag::kLongMove:
    case Flag::kTransformationToKnight:
    case Flag::kTransformationToBishop:
    case Flag::kTransformationToRook:
    case Flag::kTransformationToQueen:
    case Flag::kTransformationToKnightWithCapture:
    case Flag::kTransformationToBishopWithCapture:
    case Flag::kTransformationToRookWithCapture:
    case Flag::kTransformationToQueenWithCapture:
    case Flag::kCapture:
    case Flag::kEnPassant:
        return true;
    default:
        return false;
    }
}

struct MoveTar {
    std::uint8_t from;
    std::uint8_t to;
    std::uint8_t piece = 0;
    std::uint8_t taken_piece = 0;
    std::uint8_t set = 255;
    Flag flag = Flag::kDefault;
    MoveTar() = default;
    MoveTar(std::uint8_t _from, std::uint8_t _to, std::uint8_t _piece) : from(_from), to(_to), piece(_piece) {}
    MoveTar(std::uint8_t _from, std::uint8_t _to, std::uint8_t _piece, std::uint8_t _taken_piece, Flag _flag) : from(_from), to(_to), piece(_piece), taken_piece(_taken_piece), flag(_flag) {}
    MoveTar(const Move& move, std::uint8_t square) {
        from = move.from;
        to = move.to;
        piece = move.piece;
        taken_piece = move.taken_piece;
        flag = move.flag;
        set = square;
    }
    bool operator==(const MoveTar& other_move) {
        return from == other_move.from && to == other_move.to && piece == other_move.piece && taken_piece == other_move.taken_piece && flag == other_move.flag && set == other_move.set;
    }
    int Different() const { return taken_piece - piece; }

};