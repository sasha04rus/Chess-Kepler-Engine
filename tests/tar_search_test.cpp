#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include "hashing/zobrist.h"
#include "moves/to_string.h"

// Include the implementation so these regressions can exercise the bounded
// tactical search and return-square ordering without exposing a production API.
// The test target must not compile minimax.cpp a second time.
#include "../src/search/tar/minimax.cpp"

namespace {

int interruption_calls = 0;
int interrupt_after = 0;
int deepest_observed_ply = 0;
const Board* observed_board = nullptr;

void Require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

std::uint8_t Square(const std::string& name) {
    for (int square = 0; square < 64; ++square)
        if (name == kBoard[square])
            return static_cast<std::uint8_t>(square);
    throw std::runtime_error("Unknown square: " + name);
}

void ResetSearchState() {
    ClearTT();
    ClearKiller();
    ClearHistory();
    set_history::ClearSetHistory();
    interruption_calls = 0;
    interrupt_after = 0;
    deepest_observed_ply = 0;
    observed_board = nullptr;
}

void RequireRestored(const Board& expected, const Board& actual) {
    Require(std::memcmp(expected.bitboards, actual.bitboards, sizeof(expected.bitboards)) == 0,
            "Search did not restore piece bitboards");
    Require(expected.zobrist_hash == actual.zobrist_hash, "Search did not restore Zobrist hash");
    Require(expected.ply == actual.ply, "Search did not restore ply");
    Require(expected.turn == actual.turn, "Search did not restore side to move");
    Require(expected.en_passant == actual.en_passant, "Search did not restore en passant");
    Require(expected.last_irreversible == actual.last_irreversible,
            "Search did not restore irreversible-move state");
    Require(std::memcmp(expected.castling, actual.castling, sizeof(expected.castling)) == 0,
            "Search did not restore castling rights");
    Require(expected.rotated.occupied == actual.rotated.occupied &&
            expected.rotated.rotate90 == actual.rotated.rotate90 &&
            expected.rotated.rotate45 == actual.rotated.rotate45 &&
            expected.rotated.rotate315 == actual.rotated.rotate315,
            "Search did not restore rotated occupancy");
}

void TestReturnSquares() {
    for (bool pawn : {false, true}) {
        ResetSearchState();
        Board board(pawn ? "7k/p7/8/8/8/8/8/R6K w - - 0 1"
                         : "n6k/8/8/8/8/8/8/R6K w - - 0 1");
        const Board original(board);
        const Move capture(Square("a1"), Square(pawn ? "a7" : "a8"), 3,
                           pawn ? 0 : 1, Flag::kCapture);
        board.MakeMove(capture);
        const int preferred = Square("d4");
        const int distracting = Square("e4");
        set_history::table[0][capture.taken_piece][distracting] = 10000;
        const MoveTar tt_move(capture, preferred);
        const auto returns = GetCellsToReturn(board, capture, 0, &tt_move);
        Require(returns.count > 0 && returns.cells[0] == preferred,
                "TT return square must outrank history");

        std::array<bool, 64> actual{};
        for (int i = 0; i < returns.count; ++i) {
            const int square = returns.cells[i];
            Require(square >= 0 && square < 64, "Invalid return-square index");
            Require(!actual[square], "Duplicate return square");
            actual[square] = true;
        }
        for (int square = 0; square < 64; ++square) {
            const bool empty = (board.rotated.occupied & (1ULL << square)) == 0;
            const bool permitted_rank = !pawn || (square >= 8 && square < 56);
            Require(actual[square] == (empty && permitted_rank),
                    "Return-square ordering lost a legal square or added an illegal one");
        }

        // A hash move for a different capture must not influence this capture.
        MoveTar unrelated = tt_move;
        unrelated.from = Square("b1");
        const auto unrelated_returns = GetCellsToReturn(board, capture, 0, &unrelated);
        Require(unrelated_returns.cells[0] == distracting,
                "Unrelated TT move affected return-square ordering");
        board.UnMakeMove(capture);
        RequireRestored(original, board);
    }
}

void TestCutoffPreservesReturn(bool white) {
    ResetSearchState();
    Board board(white ? "n6k/8/8/8/8/8/8/R6K w - - 0 1"
                      : "r6k/8/8/8/8/8/8/N6K b - - 0 1");
    const Board original(board);
    const Move capture(Square(white ? "a1" : "a8"),
                       Square(white ? "a8" : "a1"), 3, 1, Flag::kCapture);
    const MoveTar expected(capture, Square("d4"));
    TTEntry seed{};
    seed.key = board.zobrist_hash;
    seed.depth = 0;
    seed.best_move = expected;
    StoreTT(seed);

    PrincipalVariation<MoveTar> pv;
    std::uint64_t nodes = 0;
    const int alpha = white ? -15000 : 14000;
    const int beta = white ? -14000 : 15000;
    const int score = Minimax(board, 1, alpha, beta, pv, nodes, 0, false);
    Require(score == (white ? beta : alpha), "Fixture did not produce the expected cutoff");
    TTEntry saved{};
    Require(ProbeTT(original.zobrist_hash, saved), "Cutoff omitted TT entry");
    Require(saved.flag == (white ? LOWERBOUND : UPPERBOUND), "Wrong cutoff bound type");
    Require(saved.best_move == expected, "Cutoff TT move lost its TAR return square");
    Require(pv.length > 0 && pv.moves[0] == expected, "Cutoff PV lost its TAR return square");
    RequireRestored(original, board);
}

void TestInterruptedTacticalSearch() {
    for (int threshold : {2, 3, 4, 8, 16}) {
        ResetSearchState();
        Board board("n6k/8/8/8/8/8/8/R6K w - - 0 1");
        const Board original(board);
        observed_board = &board;
        interrupt_after = threshold;
        std::uint64_t nodes = 0;
        const int score = MinimaxTac(board, -15000, 15000, nodes, 0);
        Require(interruption_calls >= threshold, "Cancellation fixture did not reach interrupt");
        Require(deepest_observed_ply > 0, "Cancellation never occurred during a child search");
        Require(score == 0, "Interrupted tactical search did not propagate interruption");
        RequireRestored(original, board);
        observed_board = nullptr;
    }
}

void TestQuietPromotion(bool white) {
    ResetSearchState();
    Board board(white ? "8/P7/7k/8/8/8/8/7K w - - 0 1"
                      : "7k/8/8/8/8/7K/p7/8 b - - 0 1");
    const Board original(board);
    const int static_score = board.EvaluateTarPosition();
    std::uint64_t nodes = 0;
    const int tactical_score = MinimaxTac(board, -15000, 15000, nodes, 0);
    Require(white ? tactical_score > static_score + 300 : tactical_score < static_score - 300,
            "Tactical search overlooked an uncontested quiet promotion");
    RequireRestored(original, board);
}

void TestNonCheckingCaptureAtHorizon() {
    ResetSearchState();
    // Bxd5 can relocate the active knight to a corner. It does not give
    // check, but the positional gain must still be visible at the horizon.
    Board board("7k/8/8/3n4/8/8/6B1/K7 w - - 0 1");
    const Board original(board);
    const int static_score = board.EvaluateTarPosition();
    std::uint64_t nodes = 0;
    const int tactical_score = MinimaxTac(board, -15000, 15000, nodes, 0);
    Require(tactical_score > static_score + 20,
            "Tactical search overlooked a nonchecking capture and relocation");
    RequireRestored(original, board);
}

void TestMateAtTacticalLimit(bool white) {
    ResetSearchState();
    Board board(white ? "8/8/8/8/8/5k2/6q1/7K w - - 0 1"
                      : "7k/6Q1/5K2/8/8/8/8/8 b - - 0 1");
    const Board original(board);
    Require(!board.LegalTest(!board.turn), "Mate fixture must begin in check");
    std::uint64_t nodes = 0;
    constexpr int search_ply = 7;
    const int score = MinimaxTac(board, -15000, 15000, nodes, search_ply, 4);
    Require(score == (white ? -MATE_VALUE + search_ply : MATE_VALUE - search_ply),
            "Tactical depth cap hid checkmate behind a static evaluation");
    RequireRestored(original, board);
}

} // namespace

bool IsInterrupted() {
    ++interruption_calls;
    if (observed_board != nullptr)
        deepest_observed_ply = std::max(deepest_observed_ply, static_cast<int>(observed_board->ply));
    return interrupt_after > 0 && interruption_calls >= interrupt_after;
}

int main() {
    zobrist::Init();
    int failed = 0;
    const auto run = [&failed](const char* name, auto test) {
        try {
            test();
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    };
    run("TAR return-square completeness and TT priority", TestReturnSquares);
    run("White cutoff retains TAR placement", [] { TestCutoffPreservesReturn(true); });
    run("Black cutoff retains TAR placement", [] { TestCutoffPreservesReturn(false); });
    run("Interrupted tactical search restores position", TestInterruptedTacticalSearch);
    run("White quiet promotion at horizon", [] { TestQuietPromotion(true); });
    run("Black quiet promotion at horizon", [] { TestQuietPromotion(false); });
    run("Nonchecking capture and relocation at horizon", TestNonCheckingCaptureAtHorizon);
    run("White checkmate at tactical limit", [] { TestMateAtTacticalLimit(true); });
    run("Black checkmate at tactical limit", [] { TestMateAtTacticalLimit(false); });
    return failed == 0 ? 0 : 1;
}
