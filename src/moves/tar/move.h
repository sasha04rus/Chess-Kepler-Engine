#pragma once

#include <cstdint>

#include "../flag.h"
#include "../standard/move.h"

bool IsReversible(const Move& move);

struct MoveTar {
    std::uint8_t from;
    std::uint8_t to;
    std::uint8_t piece = 0;
    std::uint8_t taken_piece = 0;
    std::uint8_t set = 0;
    Flag flag = Flag::kDefault;
    MoveTar() = default;
    MoveTar(std::uint8_t _from, std::uint8_t _to, std::uint8_t _piece) : from(_from), to(_to), piece(_piece) {}
    MoveTar(std::uint8_t _from, std::uint8_t _to, std::uint8_t _piece, std::uint8_t _taken_piece, Flag _flag) : from(_from), to(_to), piece(_piece), taken_piece(_taken_piece), flag(_flag) {}
    MoveTar(std::uint8_t _from, std::uint8_t _to, std::uint8_t _piece, std::uint8_t _taken_piece, Flag _flag, std::uint8_t _set) : from(_from), to(_to), piece(_piece), taken_piece(_taken_piece), flag(_flag), set(_set) {}
    MoveTar(const Move& move) {
        from = move.from;
        to = move.to;
        piece = move.piece;
        taken_piece = move.taken_piece;
        flag = move.flag;
    }
    MoveTar(const Move& move, std::uint8_t square) {
        from = move.from;
        to = move.to;
        piece = move.piece;
        taken_piece = move.taken_piece;
        flag = move.flag;
        set = square;
    }
    bool operator==(const Move& other_move) const {
        return from == other_move.from && to == other_move.to && piece == other_move.piece && taken_piece == other_move.taken_piece && flag == other_move.flag;
    }
    bool operator==(const MoveTar& other_move) const {
        return from == other_move.from && to == other_move.to && piece == other_move.piece && taken_piece == other_move.taken_piece && flag == other_move.flag && set == other_move.set;
    }
    int Different() const { return taken_piece - piece; }

};