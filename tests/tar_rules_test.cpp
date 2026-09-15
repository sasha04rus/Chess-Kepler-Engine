#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "board/board.h"
#include "hashing/zobrist.h"
#include "movegen/generate_moves.h"
#include "moves/to_string.h"

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

int Square(const std::string& name) {
    for (int square = 0; square < 64; ++square)
        if (name == kBoard[square]) return square;
    throw std::runtime_error("Unknown square: " + name);
}

std::uint64_t Rehash(const Board& board) {
    std::uint64_t hash = board.turn ? zobrist::turn : 0;
    for (int side = 0; side < 2; ++side)
        for (int piece = 0; piece < 6; ++piece)
            for (int square = 0; square < 64; ++square)
                if (board.bitboards[side][piece] & (1ULL << square))
                    hash ^= zobrist::piece[side][piece][square];
    for (int i = 0; i < 4; ++i)
        if (board.castling[i]) hash ^= zobrist::castling[i];
    if (board.en_passant)
        hash ^= zobrist::en_passant[zobrist::ep_square_to_index[board.en_passant]];
    return hash;
}

void CheckBoard(const Board& board, const std::string& label) {
    Require(board.zobrist_hash == Rehash(board), label + ": inconsistent hash");
    Bitboard occupied = 0;
    for (int side = 0; side < 2; ++side)
        for (int piece = 0; piece < 6; ++piece) {
            const Bitboard pieces = board.bitboards[side][piece];
            Require((occupied & pieces) == 0, label + ": overlapping pieces");
            occupied |= pieces;
            if (piece < 5)
                for (int square = 0; square < 64; ++square)
                    if (pieces & (1ULL << square))
                        Require(board.pieces[square] == piece,
                                label + ": inconsistent occupied-square piece");
        }
    Require(occupied == board.rotated.occupied, label + ": inconsistent occupancy");
}

Move FindMove(const Board& board, const std::string& uci) {
    Move moves[256];
    const int count = movegen::GenerateMoves(board, moves);
    for (int i = 0; i < count; ++i)
        if (MoveToString(moves[i]) == uci) return moves[i];
    throw std::runtime_error("Move not generated: " + uci);
}

struct Snapshot {
    std::array<Bitboard, 12> pieces;
    std::array<bool, 4> castling;
    RotatedBoard rotated;
    bool turn;
    std::uint8_t en_passant;
    std::uint8_t ply;
    std::uint8_t last_irreversible;
    std::uint64_t hash;

    explicit Snapshot(const Board& board)
        : rotated(board.rotated), turn(board.turn), en_passant(board.en_passant),
          ply(board.ply), last_irreversible(board.last_irreversible),
          hash(board.zobrist_hash) {
        for (int side = 0; side < 2; ++side)
            for (int piece = 0; piece < 6; ++piece)
                pieces[side * 6 + piece] = board.bitboards[side][piece];
        std::copy_n(board.castling, 4, castling.begin());
    }

    void CheckRestored(const Board& board, const std::string& label) const {
        const Snapshot after(board);
        Require(pieces == after.pieces && castling == after.castling &&
                    turn == after.turn && en_passant == after.en_passant &&
                    ply == after.ply && last_irreversible == after.last_irreversible &&
                    hash == after.hash && rotated.occupied == after.rotated.occupied &&
                    rotated.rotate90 == after.rotated.rotate90 &&
                    rotated.rotate45 == after.rotated.rotate45 &&
                    rotated.rotate315 == after.rotated.rotate315,
                label + ": make/unmake did not restore board");
        CheckBoard(board, label);
    }
};

void CheckDoublePushes() {
    Board board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    const Snapshot before(board);
    const std::array<std::string, 5> sequence = {
        "e2e4", "e7e5", "d2d4", "d7d5", "g1f3"
    };
    std::array<Move, 5> moves;
    for (std::size_t i = 0; i < sequence.size(); ++i) {
        moves[i] = FindMove(board, sequence[i]);
        board.MakeMove(moves[i]);
        CheckBoard(board, sequence[i]);
    }
    for (auto it = moves.rbegin(); it != moves.rend(); ++it) {
        board.UnMakeMove(*it);
        CheckBoard(board, "unmake double push sequence");
    }
    before.CheckRestored(board, "double push sequence");
}

void CheckCastling() {
    struct Case { const char* side; const char* rights; const char* move; };
    for (const auto& test : std::array<Case, 8>{{
             {"w", "K", "e1g1"}, {"w", "Q", "e1c1"},
             {"b", "k", "e8g8"}, {"b", "q", "e8c8"},
             {"w", "KQ", "e1g1"}, {"w", "KQ", "e1c1"},
             {"b", "kq", "e8g8"}, {"b", "kq", "e8c8"}
         }}) {
        Board board(std::string("r3k2r/8/8/8/8/8/8/R3K2R ") +
                    test.side + " " + test.rights + " - 0 1");
        const Snapshot before(board);
        const Move move = FindMove(board, test.move);
        board.MakeMove(move);
        Require(board.LegalTest(board.turn), std::string(test.move) + ": illegal castling");
        CheckBoard(board, test.move);
        board.UnMakeMove(move);
        before.CheckRestored(board, test.move);
    }
}

void CheckCaptureReturn(const std::string& fen, const std::string& uci,
                        Flag expected_flag, const std::string& extra_return) {
    Board board(fen);
    const Snapshot before(board);
    const Move move = FindMove(board, uci);
    Require(move.flag == expected_flag, uci + ": wrong move flag");
    if (expected_flag == Flag::kEnPassant)
        Require(board.en_passant == Square(uci.substr(2, 2)), uci + ": wrong FEN target");
    for (const int returned : {static_cast<int>(move.from), Square(extra_return)}) {
        board.MakeMove(move);
        if (IsReversible(move))
            board.last_irreversible = board.st[board.ply - 1].last_irreversible;
        CheckBoard(board, uci + " before return");
        board.SetPiece(move.taken_piece, returned);
        Require(board.LegalTest(board.turn), uci + ": illegal capture and return");
        Require((board.bitboards[!board.turn][move.taken_piece] & (1ULL << returned)) != 0,
                uci + ": returned piece has wrong owner");
        CheckBoard(board, uci + " after return");
        board.UnSetPiece(move.taken_piece, returned);
        CheckBoard(board, uci + " after removing return");
        board.UnMakeMove(move);
        before.CheckRestored(board, uci);
    }
}

void CheckPromotionReturnNotation() {
    Board board("r3k3/1P6/8/8/8/8/8/4K3 w - - 0 1");
    const Move promotion = FindMove(board, "b7a8q");
    const MoveTar tar_move(promotion, Square("h8"));

    Require(MoveToString(tar_move) == "b7a8qh8",
            "promotion must precede the return square");

    board.MakeMove("b7a8qh8");
    Require((board.bitboards[0][4] & (1ULL << Square("a8"))) != 0,
            "promoted queen is missing from a8");
    Require((board.bitboards[1][3] & (1ULL << Square("h8"))) != 0,
            "captured rook was not returned to h8");
    CheckBoard(board, "promotion and return notation");
}

} // namespace

int main() {
    try {
        zobrist::Init();
        CheckDoublePushes();
        CheckCastling();
        CheckCaptureReturn("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
                           "e5d6", Flag::kEnPassant, "a7");
        CheckCaptureReturn("4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1",
                           "e4d3", Flag::kEnPassant, "a2");
        CheckCaptureReturn("4k3/8/8/3n4/3R4/8/8/4K3 w - - 0 1",
                           "d4d5", Flag::kCapture, "a7");
        CheckCaptureReturn("4k3/8/8/3r4/3N4/8/8/4K3 b - - 0 1",
                           "d5d4", Flag::kCapture, "a2");
        CheckCaptureReturn("r3k3/1P6/8/8/8/8/8/4K3 w - - 0 1",
                           "b7a8q", Flag::kTransformationToQueenWithCapture, "h8");
        CheckCaptureReturn("4k3/8/8/8/8/8/1p6/R3K3 b - - 0 1",
                           "b2a1q", Flag::kTransformationToQueenWithCapture, "h1");
        CheckPromotionReturnNotation();
        std::cout << "TAR board rules and hash regressions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
