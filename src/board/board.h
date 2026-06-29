#pragma once

#include <cstdint>
#include <string>
#include <cstdint>

#include "../moves/standard/move.h"
#include "../moves/tar/move.h"  

using Bitboard = std::uint64_t;
enum class Variant { kStandard, kTakeAndReturn };

struct State {
    bool castling[4];
    std::uint8_t en_passant;
    std::uint64_t zobrist_hash;
    std::uint8_t last_irreversible;
};

struct RotatedBoard {
    Bitboard occupied;
    Bitboard rotate90;
    Bitboard rotate45;
    Bitboard rotate315;
};

struct Board {
    Bitboard bitboards[2][6] = {0};  
    bool turn = true;
    bool castling[4] = {false};
    std::uint8_t en_passant = 0;
    std::uint64_t zobrist_hash = 0;
    std::uint8_t last_irreversible = 0;

    RotatedBoard rotated;

    State st[256];
    std::uint8_t ply = 0;
    std::uint8_t pieces[64] = {255};

    Board(const std::string fen);
    Board(const Board& other);
    bool GameAbort() const;
    bool IsEndgame() const;
    bool LegalTest(bool turn) const;

    int EvaluatePosition() const;
    int EvaluateTarPosition() const;

    void MakeMove(const std::string& str, Variant variant);

    void MakeMove(const Move& move);
    void SetPiece(std::uint8_t piece, std::uint8_t square);
    void UnSetPiece(std::uint8_t piece, std::uint8_t square);

    void UnMakeMove(const Move& move);

    void MakeNullMove();
    void UnMakeNullMove();
};