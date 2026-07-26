#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>

#include "board/board.h"
#include "hashing/zobrist.h"
#include "moves/pv.h"
#include "moves/to_string.h"
#include "search/search.h"
#include "search/standard/minimax.h"
#include "tt/tt.h"

struct timespec start_time {};
std::atomic<bool> stop_signal {false};
std::atomic<bool> searching {false};
std::string bestmove;
int movetime = 0;

std::uint64_t GetElapsedMilliseconds() {return 0;}

bool IsInterrupted() {return false;}

namespace {

constexpr int BENCH_INFINITY = 15000;

struct BenchPosition {
    const char* name;
    const char* fen;
};

struct BenchResult {
    int score = 0;
    std::uint64_t nodes = 0;
    double milliseconds = 0.0;
    std::string bestmove = "0000";
    PrincipalVariation<Move> pv;
};

const std::array<BenchPosition, 6> POSITIONS = {{
    {
        "Start",
        "rnbqkbnr/pppppppp/8/8/8/8/"
        "PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    },
    {
        "Kiwipete",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/"
        "1p2P3/2N2Q1p/PPPBBPPP/"
        "R3K2R w KQkq - 0 1"
    },
    {
        "Middlegame",
        "r4rk1/1pp1qppp/p1np1n2/"
        "2b1p1B1/2B1P1b1/P1NP1N2/"
        "1PP1QPPP/R4RK1 w - - 0 10"
    },
    {
        "Promotion tactics",
        "rnbq1k1r/pp1Pbppp/2p5/8/"
        "2B5/8/PPP1NnPP/"
        "RNBQK2R w KQ - 1 8"
    },
    {
        "Rook endgame",
        "8/2p5/3p4/KP5r/1R3p1k/"
        "8/4P1P1/8 w - - 0 1"
    },
    {
        "Castling and promotions",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/"
        "BBP1P3/q4N2/Pp1P2PP/"
        "R2Q1RK1 w kq - 0 1"
    }
}};

BenchResult RunPosition(const BenchPosition& test, int depth) {
    ClearTT();
    ClearKiller();

    Board board(test.fen);

    const std::uint64_t initial_hash = board.zobrist_hash;
    PrincipalVariation<Move> pv;
    std::uint64_t nodes = 0;

    const auto started = std::chrono::steady_clock::now();
    const int score = Minimax(board, depth, -BENCH_INFINITY, BENCH_INFINITY, pv, nodes, 0);
    const auto finished = std::chrono::steady_clock::now();

    if (board.zobrist_hash != initial_hash) {
        std::cerr << "Search corrupted root board in " << test.name << '\n';
        std::exit(2);
    }

    const double milliseconds = std::chrono::duration<double, std::milli>(finished - started).count();
    std::string root_move = "0000";

    if (pv.length > 0)
        root_move = MoveToString(pv.moves[0]);

    return BenchResult { score, nodes, milliseconds, root_move, pv };
}

void WarmUp(int depth) {
    ClearTT();
    ClearKiller();

    Board board(POSITIONS[0].fen);

    PrincipalVariation<Move> pv;
    std::uint64_t nodes = 0;

    Minimax(board, std::min(depth, 4), -BENCH_INFINITY, BENCH_INFINITY, pv, nodes, 0);
}

void PrintPv(const PrincipalVariation<Move>& pv) {
    for (int i = 0; i < pv.length; i++)
        std::cout << MoveToString(pv.moves[i]) << ' ';
}

} // namespace

int main(int argc, char** argv) {
    int depth = 6;
    if (argc >= 2) {
        try {
            depth = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Usage: ChessKeplerBench [depth]\n";
            return 1;
        }
    }

    if (depth < 1 || depth > 30) {
        std::cerr << "Depth must be between 1 and 30\n";
        return 1;
    }

    zobrist::Init();
    WarmUp(depth);
    std::uint64_t total_nodes = 0;
    double total_milliseconds = 0.0;

    std::uint64_t fingerprint = 1469598103934665603ULL;

    std::cout << "Chess Kepler bench, depth " << depth << "\n\n";

    for (const BenchPosition& test : POSITIONS) {
        const BenchResult result = RunPosition(test, depth);
        const double seconds = result.milliseconds / 1000.0;
        const std::uint64_t nps = seconds > 0.0 ? static_cast<std::uint64_t>(static_cast<double>(result.nodes) / seconds) : 0;

        total_nodes += result.nodes;
        total_milliseconds += result.milliseconds;

        fingerprint ^= result.nodes;
        fingerprint *= 1099511628211ULL;

        fingerprint ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(result.score));
        fingerprint *= 1099511628211ULL;

        std::cout
            << std::left
            << std::setw(25)
            << test.name
            << " score "
            << std::setw(7)
            << result.score
            << " best "
            << std::setw(7)
            << result.bestmove
            << " nodes "
            << std::setw(12)
            << result.nodes
            << " time "
            << std::fixed
            << std::setprecision(2)
            << std::setw(10)
            << result.milliseconds
            << " nps "
            << nps
            << '\n';

        std::cout << "  pv ";
        PrintPv(result.pv);
        std::cout << '\n';
    }

    const double total_seconds =
        total_milliseconds / 1000.0;

    const std::uint64_t total_nps =
        total_seconds > 0.0
            ? static_cast<std::uint64_t>(
                  static_cast<double>(total_nodes) /
                  total_seconds
              )
            : 0;

    std::cout
        << "\nTOTAL"
        << " nodes " << total_nodes
        << " time "
        << std::fixed
        << std::setprecision(2)
        << total_milliseconds
        << " ms"
        << " nps " << total_nps
        << " fingerprint "
        << std::hex
        << fingerprint
        << std::dec
        << '\n';

    return 0;
}