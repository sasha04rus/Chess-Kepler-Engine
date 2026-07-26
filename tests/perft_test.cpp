#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "board/board.h"
#include "hashing/zobrist.h"
#include "movegen/generate_moves.h"
#include "moves/to_string.h"

namespace {

constexpr int MAX_TEST_MOVES = 218;

struct BoardSnapshot {
    Bitboard bitboards[2][6]{};
    bool turn = true;
    bool castling[4]{};
    std::uint8_t en_passant = 0;
    std::uint64_t zobrist_hash = 0;
    std::uint8_t last_irreversible = 0;
    RotatedBoard rotated{};
    std::uint8_t ply = 0;
};

BoardSnapshot TakeSnapshot(const Board& board) {
    BoardSnapshot snapshot;

    std::memcpy(
        snapshot.bitboards,
        board.bitboards,
        sizeof(snapshot.bitboards)
    );

    snapshot.turn = board.turn;

    std::memcpy(
        snapshot.castling,
        board.castling,
        sizeof(snapshot.castling)
    );

    snapshot.en_passant = board.en_passant;
    snapshot.zobrist_hash = board.zobrist_hash;
    snapshot.last_irreversible = board.last_irreversible;
    snapshot.rotated = board.rotated;
    snapshot.ply = board.ply;

    return snapshot;
}

int ActualPieceAt(
    const Board& board,
    int square
) {
    const Bitboard square_mask =
        1ULL << square;

    for (int color = 0; color < 2; color++) {
        for (int piece = 0; piece < 6; piece++) {
            if (board.bitboards[color][piece] &
                square_mask) {
                return piece;
            }
        }
    }

    return -1;
}

bool ValidateLazyPieces(
    const Board& board
) {
    for (int square = 0; square < 64; square++) {
        const int actual_piece =
            ActualPieceAt(board, square);

        if (actual_piece < 0 ||
            actual_piece == 5) {
            continue;
        }

        if (board.pieces[square] !=
            actual_piece) {
            std::cerr
                << "Lazy pieces mismatch\n"
                << "square: " << kBoard[square]
                << "\nactual: " << actual_piece
                << "\ncached: "
                << static_cast<int>(
                       board.pieces[square]
                   )
                << '\n';

            return false;
        }
    }

    return true;
}

bool IsRegularCapture(const Move& move) {
    switch (move.flag) {
        case Flag::kCapture:

        case Flag::kTransformationToKnightWithCapture:
        case Flag::kTransformationToBishopWithCapture:
        case Flag::kTransformationToRookWithCapture:
        case Flag::kTransformationToQueenWithCapture:
            return true;

        default:
            return false;
    }
}

bool SamePosition(
    const BoardSnapshot& expected,
    const Board& actual
) {
    if (std::memcmp(
            expected.bitboards,
            actual.bitboards,
            sizeof(expected.bitboards)
        ) != 0) {
        return false;
    }

    if (expected.turn != actual.turn)
        return false;

    if (std::memcmp(
            expected.castling,
            actual.castling,
            sizeof(expected.castling)
        ) != 0) {
        return false;
    }

    if (expected.en_passant != actual.en_passant)
        return false;

    if (expected.zobrist_hash != actual.zobrist_hash)
        return false;

    if (expected.last_irreversible !=
        actual.last_irreversible) {
        return false;
    }

    if (expected.rotated.occupied !=
        actual.rotated.occupied) {
        return false;
    }

    if (expected.rotated.rotate90 !=
        actual.rotated.rotate90) {
        return false;
    }

    if (expected.rotated.rotate45 !=
        actual.rotated.rotate45) {
        return false;
    }

    if (expected.rotated.rotate315 !=
        actual.rotated.rotate315) {
        return false;
    }

    if (expected.ply != actual.ply)
        return false;

    return true;
}

[[noreturn]] void ReportBoardCorruption(
    const Move& move,
    int depth
) {
    std::cerr
        << "\nBOARD CORRUPTION\n"
        << "move: " << MoveToString(move) << '\n'
        << "remaining depth: " << depth << '\n'
        << "MakeMove/UnMakeMove did not restore the board\n";

    std::exit(2);
}

std::uint64_t Perft(Board& board, int depth) {
    if (depth == 0)
        return 1;

    Move moves[MAX_TEST_MOVES];
    const int move_count =
        movegen::GenerateMoves(board, moves);

    std::uint64_t nodes = 0;

    for (int i = 0; i < move_count; i++) {
        const Move move = moves[i];
        const BoardSnapshot before =
            TakeSnapshot(board);

        if (IsRegularCapture(move)) {
            const int actual_piece =
                ActualPieceAt(board, move.to);

            if (actual_piece < 0) {
                std::cerr
                    << "Capture targets empty square\n"
                    << "move: "
                    << MoveToString(move)
                    << '\n';

                std::exit(4);
            }

            if (move.taken_piece != actual_piece) {
                std::cerr
                    << "Wrong taken_piece\n"
                    << "move: "
                    << MoveToString(move)
                    << "\nexpected: "
                    << actual_piece
                    << "\ngenerated: "
                    << static_cast<int>(
                        move.taken_piece
                    )
                    << '\n';

                std::exit(4);
            }
        }

        if (move.flag == Flag::kEnPassant) {
            if (move.taken_piece != 0) {
                std::cerr
                    << "En passant must capture pawn\n";

                std::exit(4);
            }
        }

        board.MakeMove(move);

        if (!ValidateLazyPieces(board)) {
            std::cerr
                << "after move: "
                << MoveToString(move)
                << '\n';

            std::exit(3);
        }

        const bool legal =
            board.LegalTest(board.turn);

        if (legal)
            nodes += Perft(board, depth - 1);

        board.UnMakeMove(move);

        if (!ValidateLazyPieces(board)) {
            std::cerr
                << "after unmake: "
                << MoveToString(move)
                << '\n';

            std::exit(3);
        }

        if (!SamePosition(before, board))
            ReportBoardCorruption(move, depth);
    }

    return nodes;
}

void Divide(Board& board, int depth) {
    Move moves[MAX_TEST_MOVES];
    const int move_count =
        movegen::GenerateMoves(board, moves);

    std::uint64_t total = 0;

    std::cerr << "\nPerft divide, depth "
              << depth << ":\n";

    for (int i = 0; i < move_count; i++) {
        const Move move = moves[i];
        const BoardSnapshot before =
            TakeSnapshot(board);

        board.MakeMove(move);

        if (!board.LegalTest(board.turn)) {
            board.UnMakeMove(move);

            if (!SamePosition(before, board))
                ReportBoardCorruption(move, depth);

            continue;
        }

        const std::uint64_t nodes =
            Perft(board, depth - 1);

        board.UnMakeMove(move);

        if (!SamePosition(before, board))
            ReportBoardCorruption(move, depth);

        std::cerr
            << std::left << std::setw(8)
            << MoveToString(move)
            << nodes << '\n';

        total += nodes;
    }

    std::cerr << "total: " << total << "\n\n";
}

struct PerftCase {
    const char* name;
    const char* fen;
    std::vector<std::uint64_t> expected;
};

bool RunCase(const PerftCase& test) {
    std::cout << "\n" << test.name << '\n';

    Board board(test.fen);
    const BoardSnapshot initial =
        TakeSnapshot(board);

    for (std::size_t i = 0;
         i < test.expected.size();
         i++) {

        const int depth =
            static_cast<int>(i) + 1;

        const auto start =
            std::chrono::steady_clock::now();

        const std::uint64_t actual =
            Perft(board, depth);

        const auto finish =
            std::chrono::steady_clock::now();

        const double seconds =
            std::chrono::duration<double>(
                finish - start
            ).count();

        const std::uint64_t expected =
            test.expected[i];

        const bool passed =
            actual == expected;

        std::cout
            << "depth " << depth
            << ": " << actual
            << " / expected " << expected
            << "  "
            << (passed ? "OK" : "FAILED");

        if (seconds > 0.0) {
            const double nps =
                static_cast<double>(actual) /
                seconds;

            std::cout
                << "  time "
                << std::fixed
                << std::setprecision(3)
                << seconds
                << " s"
                << "  nps "
                << static_cast<std::uint64_t>(nps);
        }

        std::cout << '\n';

        if (!SamePosition(initial, board)) {
            std::cerr
                << "Root board changed after perft\n";

            return false;
        }

        if (!passed) {
            Divide(board, depth);
            return false;
        }
    }

    return true;
}

} // namespace

int main() {
    zobrist::Init();

    const std::vector<PerftCase> tests = {
        {
            "Start position",
            "rnbqkbnr/pppppppp/8/8/8/8/"
            "PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            {
                20,
                400,
                8902,
                197281
            }
        },
        {
            "Kiwipete",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/"
            "1p2P3/2N2Q1p/PPPBBPPP/"
            "R3K2R w KQkq - 0 1",
            {
                48,
                2039,
                97862
            }
        },
        {
            "En passant and rook endgame",
            "8/2p5/3p4/KP5r/1R3p1k/"
            "8/4P1P1/8 w - - 0 1",
            {
                14,
                191,
                2812,
                43238
            }
        },
        {
            "Castling and promotions",
            "r3k2r/Pppp1ppp/1b3nbN/nP6/"
            "BBP1P3/q4N2/Pp1P2PP/"
            "R2Q1RK1 w kq - 0 1",
            {
                6,
                264,
                9467
            }
        },
        {
            "Promotion tactics",
            "rnbq1k1r/pp1Pbppp/2p5/8/"
            "2B5/8/PPP1NnPP/"
            "RNBQK2R w KQ - 1 8",
            {
                44,
                1486,
                62379
            }
        },
        {
            "Middlegame",
            "r4rk1/1pp1qppp/p1np1n2/"
            "2b1p1B1/2B1P1b1/P1NP1N2/"
            "1PP1QPPP/R4RK1 w - - 0 10",
            {
                46,
                2079,
                89890
            }
        }
    };

    bool all_passed = true;

    for (const PerftCase& test : tests) {
        if (!RunCase(test))
            all_passed = false;
    }

    if (!all_passed) {
        std::cerr << "\nPERFT TESTS FAILED\n";
        return 1;
    }

    std::cout << "\nALL PERFT TESTS PASSED\n";
    return 0;
}
