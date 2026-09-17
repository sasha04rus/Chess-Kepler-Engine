#include "engine.h"

#include <memory>

#include "../moves/to_string.h"
#include "../search/search.h"
#include "../time/time_manager.h"
#include "../tt/tt.h"
#include "../movegen/generate_moves.h"

namespace {

bool IsCapture(const Move& move) {
    switch (move.flag) {
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

} // namespace

void Engine::SetPosition(const std::string& fen) {
    if (searching) {
        stop_signal = true;
        pthread_join(search_thread_, nullptr);
        searching = false;
    }
    board_ptr_.reset();
    board_ptr_ = std::make_unique<Board>(fen);
}

void Engine::MakeMove(const std::string& move) {
    board_ptr_->MakeMove(move);
}

void Engine::SetThreads(int threads) {
    threads_ = threads;
}

void Engine::SetMultiPv(int multi_pv) {
    multi_pv_ = multi_pv;
}

void Engine::Go(int mt, int depth) {
    movetime = mt;
    stop_signal = false;
    searching = true;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    SearchArgs* sa = new SearchArgs{*board_ptr_, depth, threads_, multi_pv_, info_callback_, best_move_callback_};
    #if KEPLER_TAR
        pthread_create(&search_thread_, nullptr, Search<MoveTar>, sa);
    #else
        pthread_create(&search_thread_, nullptr, Search<Move>, sa);
    #endif
}

void Engine::Stop() {
    if (searching) {
        stop_signal.store(true, std::memory_order_relaxed);
        pthread_join(search_thread_, nullptr);
        searching = false;
    }
}

void Engine::NewGame() {
    ClearTT();
}

bool Engine::IsEmptyPosition() const {
    return board_ptr_ == nullptr;
}

bool Engine::GetTurn() const {
    return board_ptr_->turn;
}

int Engine::GetPly() const {
    return board_ptr_->ply;
}

bool Engine::IsCheck() const {
    return !board_ptr_->LegalTest(!board_ptr_->turn);
}

std::vector<std::string> Engine::GetLegalMoves() const {
    std::vector<std::string> moves;
    Move possible_moves[218];
    int move_count = movegen::GenerateMoves(*board_ptr_, possible_moves);
    #if KEPLER_TAR
        for (int i = 0; i < move_count; i++) {
            const auto& move = possible_moves[i];
            board_ptr_->MakeMove(move);
            if (IsCapture(move)) {
                const int first = (move.taken_piece == 0) ? 8 : 0;
                const int end = (move.taken_piece == 0) ? 56 : 64;
                for (int cell = first; cell < end; cell++) {
                    if ((board_ptr_->rotated.occupied & (1ULL << cell)) == 0) {
                        board_ptr_->SetPiece(move.taken_piece, cell);
                        if (board_ptr_->LegalTest(!board_ptr_->turn))
                            moves.push_back(MoveToString(MoveTar(move, cell)));
                        board_ptr_->UnSetPiece(move.taken_piece, cell);
                    }
                }
            } else if (board_ptr_->LegalTest(board_ptr_->turn))
                moves.push_back(MoveToString(move));
            board_ptr_->UnMakeMove(move);
        }
    #else
        for (int i = 0; i < move_count; i++) {
            const auto& move = possible_moves[i];
            board_ptr_->MakeMove(move);
            if (board_ptr_->LegalTest(board_ptr_->turn))
                moves.push_back(MoveToString(move));
            board_ptr_->UnMakeMove(move);
        }
    #endif
    return moves;
}

std::string Engine::GetPosition() const {
    static constexpr char kWhitePieces[] = "PNBRQK";
    static constexpr char kBlackPieces[] = "pnbrqk";

    std::string fen;
    for (int rank = 7; rank >= 0; rank--) {
        int empty_count = 0;
        for (int internal_file = 7; internal_file >= 0; --internal_file) {
            const int square = rank * 8 + internal_file;
            const Bitboard bit = 1ULL << square;
            char piece_character = '\0';

            for (int piece = 0; piece < 6 && piece_character == '\0'; ++piece) {
                if ((board_ptr_->bitboards[0][piece] & bit) != 0)
                    piece_character = kWhitePieces[piece];
                else if ((board_ptr_->bitboards[1][piece] & bit) != 0)
                    piece_character = kBlackPieces[piece];
            }

            if (piece_character == '\0') {
                empty_count++;
                continue;
            }

            if (empty_count != 0) {
                fen += static_cast<char>('0' + empty_count);
                empty_count = 0;
            }
            fen += piece_character;
        }

        if (empty_count != 0)
            fen += static_cast<char>('0' + empty_count);
        if (rank != 0)
            fen += '/';
    }

    fen += board_ptr_->turn ? " w " : " b ";
    bool has_castling_rights = false;
    static constexpr char kCastlingRights[] = "KQkq";
    for (int index = 0; index < 4; ++index) {
        if (board_ptr_->castling[index]) {
            fen += kCastlingRights[index];
            has_castling_rights = true;
        }
    }
    if (!has_castling_rights)
        fen += '-';

    fen += ' ';
    if (board_ptr_->en_passant == 0) {
        fen += '-';
    } else {
        fen += static_cast<char>('h' - (board_ptr_->en_passant % 8));
        fen += static_cast<char>('1' + (board_ptr_->en_passant / 8));
    }

    fen += ' ';
    fen += std::to_string(board_ptr_->ply - board_ptr_->last_irreversible);
    fen += ' ';
    fen += std::to_string(board_ptr_->ply / 2 + 1);
    return fen;
}

std::string Engine::GetResult() const {
    if (GetLegalMoves().empty()) {
        if (!IsCheck())
            return "stalemate";
        if (board_ptr_->turn)
            return "0-1";
        return "1-0";
    }
    if (board_ptr_->GameAbort())
        return "1/2-1/2";
    return "*";
}

void Engine::SetInfoCallback(InfoCallback callback) {
    info_callback_ = std::move(callback);
}

void Engine::SetBestMoveCallback(BestMoveCallback callback) {
    best_move_callback_ = std::move(callback);
}
