#pragma once

#include <cstdint>

#include "../flag.h"

struct Move {
    std::uint8_t from;
    std::uint8_t to;
    std::uint8_t piece = 0;
    std::uint8_t taken_piece = 0;
    Flag flag = Flag::kDefault;
    Move() = default;
    Move(std::uint8_t from_, std::uint8_t to_, std::uint8_t piece_) : from(from_), to(to_), piece(piece_) {}
    Move(std::uint8_t from_, std::uint8_t to_, std::uint8_t piece_, std::uint8_t taken_piece_, Flag flag_) : from(from_), to(to_), piece(piece_), taken_piece(taken_piece_), flag(flag_) {}
    bool operator==(const Move& other_move) {
        return from == other_move.from && to == other_move.to && piece == other_move.piece; // && takenpiece_ == other_move.takenpiece_ && flag == other_move.flag;
    }
    int Different() const { return taken_piece - piece; }
};

#define NO_MOVE Move(0, 0, 0, 0, Flag::kDefault) 