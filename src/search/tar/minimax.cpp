#include "minimax.h"

#include <cstring>
#include <algorithm>
#include <array>
#include <span>

#include "../../board/board.h"
#include "../../tt/tt.h"
#include "../../moves/pv.h"
#include "../../movegen/generate_moves.h"
#include "../../eval/tar/evaluate_position.h"
#include "../../history/history.h"
#include "../search.h"

namespace set_history {

constexpr int SET_HISTORY_LIMIT = 16384;

thread_local int table[2][5][64] = {0};

int SetHistoryBonus(int depth) {
    return std::min(2000, 4 * depth * depth);
}

void UpdateSetHistory(std::uint8_t cell, std::span<const int> searched_cells, int side, std::uint8_t piece, int bonus) {
    for (int c : searched_cells) {
        if (c == cell) continue;
        auto& value = table[side][piece][c];
        value = std::clamp(value - bonus / 2, -SET_HISTORY_LIMIT, SET_HISTORY_LIMIT);
    }
    auto& value = table[side][piece][cell];
    value = std::clamp(value + bonus, -SET_HISTORY_LIMIT, SET_HISTORY_LIMIT);
}

void ClearSetHistory() {
    std::memset(table, 0, sizeof(table));
}

} // namespace set_history

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

bool IsPromotion(const Move& move) {
    switch (move.flag) {
        case Flag::kTransformationToKnight:
        case Flag::kTransformationToBishop:
        case Flag::kTransformationToRook:
        case Flag::kTransformationToQueen:
        case Flag::kTransformationToKnightWithCapture:
        case Flag::kTransformationToBishopWithCapture:
        case Flag::kTransformationToRookWithCapture:
        case Flag::kTransformationToQueenWithCapture:
            return true;
        default:
            return false;
    }
}

bool IsQuiet(const Move& move) {
    return !IsCapture(move) && !IsPromotion(move);
}

int GetReduction(int depth, int move_number, bool node_in_check, bool gives_check, bool is_capture, bool is_promotion, int history_score) {
    if (depth < 3 || move_number <= 2 || node_in_check || gives_check || is_capture || is_promotion)
        return 0;
    int reduction = 1;
    if (depth >= 5 && move_number >= 6)
        reduction++;
    if (depth >= 8 && move_number >= 12)
        reduction++;
    if (depth >= 11 && move_number >= 20)
        reduction++;

    if (history_score >= 12000)
        reduction -= 2;
    else if (history_score >= 4000)
        reduction -= 1;
    else if (history_score <= -8000) 
        reduction += 1;
    return std::clamp(reduction, 0, depth - 2);
}

int ScoreToTT(int score, int ply) {
    if (score > MATE_THRESHOLD)
        return score + ply;
    if (score < -MATE_THRESHOLD)
        return score - ply;
    return score;
}

int ScoreFromTT(int score, int ply) {
    if (score > MATE_THRESHOLD)
        return score - ply;
    if (score < -MATE_THRESHOLD)
        return score + ply;
    return score;
}

int MoveScore(const Move& move, const MoveTar* tt_move, int ply, int side) {
    if (tt_move != nullptr && move == *tt_move)
        return 1000000;
    if (IsPromotion(move))
        return 900000 + static_cast<int>(move.flag);
    if (IsCapture(move)) { return 800000 + move.Different(); }
    if (ply < MAX_PLY) {
        if (move == killers[ply][0])
            return 700'000;
        if (move == killers[ply][1])
            return 690'000;
    }
    return GetHistoryScore(side, move);
}

void MoveSort(Move moves[MAX_MOVES], int count, const MoveTar* tt_move, int ply, int side) {
    std::stable_sort(moves, moves + count, [&](const Move& left,const Move& right) {
        return MoveScore(left, tt_move, ply, side) > MoveScore(right, tt_move, ply, side);
    });
}

constexpr int NO_CAPTURE = -1;
constexpr int MAX_CELLS_TO_RETURN = 64;

struct ReturnCells {
    std::array<int, MAX_CELLS_TO_RETURN> cells{};
    int count = 0;

    int* begin() { return cells.data(); }
    int* end() { return cells.data() + count; }
};

ReturnCells GetCellsToReturn(const Board& board, const Move& move, int side, const MoveTar* tt_move = nullptr) {
    ReturnCells cells;
    if (!IsCapture(move)) {
        cells.cells[cells.count++] = NO_CAPTURE;
        return cells;
    }
    int first_cell = (move.taken_piece == 0) ? 8 : 0;
    int last_cell = (move.taken_piece == 0) ? 56 : 64;
    for (int i = first_cell; i < last_cell; i++)
        if ((board.rotated.occupied & (std::uint64_t(1) << i)) == 0)
            cells.cells[cells.count++] = i;
    const int preferred_cell = tt_move != nullptr && *tt_move == move ? tt_move->set : NO_CAPTURE;
    std::sort(cells.begin(), cells.end(), [side, move, preferred_cell](int a, int b) {
        if ((a == preferred_cell) != (b == preferred_cell))
            return a == preferred_cell;
        return set_history::table[side][move.taken_piece][a] > set_history::table[side][move.taken_piece][b];
    }); 
    return cells;
}

constexpr int MAX_SEARCH_TPLY = 2;
constexpr int MAX_CAPTURE_TPLY = 1;
constexpr int MAX_CHECK_TPLY = 4;

int MinimaxTac(Board& board, int alpha, int beta, std::uint64_t& nodes, int ply, int tply = 0) {
    if ((nodes & 1023ULL) == 0 && IsInterrupted())
        return 0;
    if (board.GameAbort())
        return 0;
    const bool in_check = !board.LegalTest(!board.turn);
    if (!in_check && tply >= MAX_SEARCH_TPLY)
        return board.EvaluateTarPosition();
    if (!in_check) {
        int eval = board.EvaluateTarPosition();
        if (board.turn) {
            if (eval >= beta)
                return beta;
            if (eval > alpha)
                alpha = eval;
        } else {
            if (eval <= alpha)
                return alpha;
            if (eval < beta)
                beta = eval;
        }
    }
    nodes++;
    Move possible_moves[MAX_MOVES];
    // Quiet promotions are tactical too. Generate quiet moves only when needed.
    const Bitboard promotion_rank = board.turn ? 0x00FF000000000000ULL : 0x000000000000FF00ULL;
    const bool can_promote = (board.bitboards[!board.turn][0] & promotion_rank) != 0;
    int move_count = movegen::GenerateMoves(board, possible_moves, !in_check && !can_promote);
    if (!in_check) {
        auto* end = std::remove_if(possible_moves, possible_moves + move_count,
            [](const Move& move) { return !IsCapture(move) && !IsPromotion(move); });
        move_count = static_cast<int>(end - possible_moves);
    }
    MoveSort(possible_moves, move_count, nullptr, ply, board.turn ? 0 : 1);
    bool possibility = false;
    const bool maximizing = board.turn;
    for (int i = 0; i < move_count; i++) {
        Move& move = possible_moves[i];
        board.MakeMove(move);
        if (IsReversible(move))
            board.last_irreversible = board.st[board.ply-1].last_irreversible;
        for (int return_cell : GetCellsToReturn(board, move, maximizing ? 0 : 1)) {
            if (return_cell != NO_CAPTURE)
                board.SetPiece(move.taken_piece, return_cell);
            if (!board.LegalTest(board.turn)) {
                if (return_cell != NO_CAPTURE)
                    board.UnSetPiece(move.taken_piece, return_cell);
                continue;
            }
            possibility = true;
            // Examine every capture at the horizon; deeper tactical plies
            // retain checks and promotions to bound TAR's return branching.
            if (!in_check && tply >= MAX_CAPTURE_TPLY && board.LegalTest(!board.turn) && !IsPromotion(move)) {
                if (return_cell != NO_CAPTURE)
                    board.UnSetPiece(move.taken_piece, return_cell);
                continue;
            }
            // TAR captures preserve material, so quiescence must remain bounded.
            // At the check limit, still examine legal evasions and detect mate
            // before falling back to a static score of the resulting position.
            int evaluation = tply >= MAX_CHECK_TPLY
                ? board.EvaluateTarPosition()
                : MinimaxTac(board, alpha, beta, nodes, ply + 1, tply + 1);
            if (return_cell != NO_CAPTURE)
                board.UnSetPiece(move.taken_piece, return_cell);
            if (IsInterrupted()) {
                board.UnMakeMove(move);
                return 0;
            }
            if (maximizing) {
                if (evaluation >= beta) {
                    board.UnMakeMove(move);
                    return beta;
                }
                if (evaluation > alpha)
                    alpha = evaluation;
            } else {
                if (evaluation <= alpha) {
                    board.UnMakeMove(move);
                    return alpha;
                }
                if (evaluation < beta)
                    beta = evaluation;
            }
        }
        board.UnMakeMove(move);
    }
    if (in_check && !possibility)
        return maximizing ? -MATE_VALUE + ply : MATE_VALUE - ply;
    return maximizing ? alpha : beta;
}

}

int Minimax(Board& board, int depth, int alpha, int beta, PrincipalVariation<MoveTar>& pv, std::uint64_t& nodes, int ply, bool allow_null) {
    if ((nodes & 1023ULL) == 0 && IsInterrupted()) {
        pv.Clear();
        return 0;
    }
    if (board.GameAbort()) return 0;

    TTEntry entry{};
    const bool tt_hit = ProbeTT(board.zobrist_hash, entry);
    if (tt_hit && ply > 0 && entry.depth >= depth && !((beta - alpha) > 1)) {
        const int tt_score = ScoreFromTT(entry.score, ply);
        if (entry.flag == EXACT)
            return tt_score;

        if (entry.flag == LOWERBOUND && tt_score >= beta)
            return tt_score;

        if (entry.flag == UPPERBOUND && tt_score <= alpha)
            return tt_score;
    }

    nodes++;

    if (depth <= 0) {
        int eval = MinimaxTac(board, alpha, beta, nodes, ply);
        pv.Clear();
        return eval;
    }

    if (allow_null && depth >= 3 && board.LegalTest(!board.turn)) {
        const int reduction = 2 + depth / 4;
        const bool maximizing = board.turn;
        const std::uint8_t saved_en_passant = board.MakeNullMove();
        PrincipalVariation<MoveTar> dummy;
        int score;
        if (maximizing)
            score = Minimax(board, depth - reduction - 1, beta - 1, beta, dummy, nodes, ply + 1, false);
        else
            score = Minimax(board, depth - reduction - 1, alpha, alpha + 1, dummy, nodes, ply + 1, false);
        board.UnMakeNullMove(saved_en_passant);
        if (IsInterrupted()) {
            pv.Clear();
            return 0;
        }
        if (maximizing && score >= beta)
            return beta;
        if (!maximizing && score <= alpha)
            return alpha;
    }

    Move possible_moves[MAX_MOVES];
    int move_count = movegen::GenerateMoves(board, possible_moves);
    int legal_moves = 0;
    const int side = board.turn ? 0 : 1;
    if (tt_hit && !(entry.best_move == NO_MOVE))
        MoveSort(possible_moves, move_count, &entry.best_move, ply, side);
    else
        MoveSort(possible_moves, move_count, nullptr, ply, side);
    bool pv_search = true;
    PrincipalVariation<MoveTar> best_pv;
    MoveTar best_move = NO_MOVE;
    const bool node_in_check = !board.LegalTest(!board.turn);
    Move searched_quiets[MAX_MOVES];
    int searched_quiet_count = 0;
    const std::uint64_t node_key = board.zobrist_hash;
    if (board.turn) {
        int best_eval = alpha;
        for (int i = 0; i < move_count; i++) {
            const auto& move = possible_moves[i];
            board.MakeMove(move);
            if (IsReversible(move))
                board.last_irreversible = board.st[board.ply-1].last_irreversible;
            std::array<int, MAX_CELLS_TO_RETURN> searched_return_cells;
            int searched_return_count = 0;
            for (int return_cell : GetCellsToReturn(board, move, side, tt_hit ? &entry.best_move : nullptr)) {
                if (return_cell != NO_CAPTURE)
                    board.SetPiece(move.taken_piece, return_cell);
                if (!board.LegalTest(false)) {
                    if (return_cell != NO_CAPTURE)
                        board.UnSetPiece(move.taken_piece, return_cell);
                    continue;
                }
                MoveTar tar_move = (return_cell == NO_CAPTURE) ? MoveTar(move) : MoveTar(move, return_cell);
                legal_moves++;
                PrincipalVariation<MoveTar> child_pv;
                int evaluation;
                if (pv_search) {
                    evaluation = Minimax(board, depth - 1, best_eval, beta, child_pv, nodes, ply + 1);
                    pv_search = false;
                } else {
                    const int reduction = GetReduction(depth, legal_moves, node_in_check, !board.LegalTest(!board.turn), IsCapture(move), IsPromotion(move), GetHistoryScore(side, move));
                    PrincipalVariation<MoveTar> probe_pv;
                    if (reduction) {
                        evaluation = Minimax(board, depth - 1 - reduction, best_eval, best_eval + 1, probe_pv, nodes, ply + 1);
                        if (evaluation > best_eval) {
                            probe_pv.Clear();
                            evaluation = Minimax(board, depth - 1, best_eval, best_eval + 1, probe_pv, nodes, ply + 1);
                        }
                    } else { evaluation = Minimax(board, depth - 1, best_eval, best_eval + 1, probe_pv, nodes, ply + 1); }
                    if (evaluation > best_eval && evaluation < beta)
                        evaluation = Minimax(board, depth - 1, best_eval, beta, child_pv, nodes, ply + 1);
                }
                if (return_cell != NO_CAPTURE) {
                    board.UnSetPiece(move.taken_piece, return_cell);
                    searched_return_cells[searched_return_count++] = return_cell;
                }
                if (IsInterrupted()) {
                    board.UnMakeMove(move);
                    pv.Clear();
                    return 0;
                }
            
                if (evaluation >= beta) {
                    if (IsQuiet(move)) {
                        const int bonus = HistoryBonus(depth);
                        UpdateHistory(side, move, bonus);
                        for (int j = 0; j < searched_quiet_count; j++)
                            UpdateHistory(side, searched_quiets[j], -bonus / 2);
                        if (ply < MAX_PLY) {
                            killers[ply][1] = killers[ply][0];
                            killers[ply][0] = move;
                        }
                    } else if (IsCapture(move))
                        set_history::UpdateSetHistory(return_cell, {searched_return_cells.data(), static_cast<std::size_t>(searched_return_count)}, side, move.taken_piece, set_history::SetHistoryBonus(depth));
                    pv.Set((return_cell == NO_CAPTURE) ? move : MoveTar(move, return_cell), child_pv);
                    TTEntry new_entry;
                    new_entry.key = node_key;
                    new_entry.depth = depth;
                    new_entry.score = ScoreToTT(beta, ply);
                    new_entry.flag = LOWERBOUND;
                    new_entry.best_move = tar_move;
                    StoreTT(new_entry);
                    board.UnMakeMove(move);
                    return beta;
                }
                
                if (evaluation > best_eval) {
                    best_eval = evaluation;
                    best_move = tar_move;
                    best_pv = child_pv;
                }
                if (IsQuiet(move)) searched_quiets[searched_quiet_count++] = move;
            }
            board.UnMakeMove(move);
        }
        if (legal_moves == 0) {
            int eval = board.LegalTest(false) ? 0 : -MATE_VALUE + ply;
            pv.Clear();
            TTEntry new_entry;
            new_entry.key = node_key;
            new_entry.depth = depth;
            new_entry.score = ScoreToTT(eval, ply);          
            new_entry.flag = EXACT;          
            new_entry.best_move = NO_MOVE;
            StoreTT(new_entry);
            return eval;
        } else if (!(best_move == NO_MOVE))
            pv.Set(best_move, best_pv);
        else 
            pv.Clear();

        TTEntry new_entry;
        new_entry.key = node_key;
        new_entry.depth = depth;
        new_entry.score = ScoreToTT(best_eval, ply);
        new_entry.best_move = best_move;

        if (best_eval <= alpha)
            new_entry.flag = UPPERBOUND;
        else if (best_eval >= beta)
            new_entry.flag = LOWERBOUND;
        else
            new_entry.flag = EXACT;
        StoreTT(new_entry);
        return best_eval;
    } else {
        int best_eval = beta;
        for (int i = 0; i < move_count; i++) {
            const auto& move = possible_moves[i];
            board.MakeMove(move);
            if (IsReversible(move))
                board.last_irreversible = board.st[board.ply-1].last_irreversible;
            std::array<int, MAX_CELLS_TO_RETURN> searched_return_cells;
            int searched_return_count = 0;
            for (int return_cell : GetCellsToReturn(board, move, side, tt_hit ? &entry.best_move : nullptr)) {
                if (return_cell != NO_CAPTURE)
                    board.SetPiece(move.taken_piece, return_cell);
                if (!board.LegalTest(true)) {
                    if (return_cell != NO_CAPTURE)
                        board.UnSetPiece(move.taken_piece, return_cell);
                    continue;
                }
                MoveTar tar_move = (return_cell == NO_CAPTURE) ? MoveTar(move) : MoveTar(move, return_cell);
                legal_moves++;
                PrincipalVariation<MoveTar> child_pv;
                int evaluation;
                if (pv_search) {
                    evaluation = Minimax(board, depth - 1, alpha, best_eval, child_pv, nodes, ply + 1);
                    pv_search = false;
                } else {
                    const int reduction = GetReduction(depth, legal_moves, node_in_check, !board.LegalTest(!board.turn), IsCapture(move), IsPromotion(move), GetHistoryScore(side, move));
                    PrincipalVariation<MoveTar> probe_pv;
                    if (reduction) {
                        evaluation = Minimax(board, depth - 1 - reduction, best_eval - 1, best_eval, probe_pv, nodes, ply + 1);
                        if (evaluation < best_eval) {
                            probe_pv.Clear();
                            evaluation = Minimax(board, depth - 1, best_eval - 1, best_eval, probe_pv, nodes, ply + 1);
                        }
                    } else { evaluation = Minimax(board, depth - 1, best_eval - 1, best_eval, probe_pv, nodes, ply + 1); }
                    if (evaluation < best_eval && evaluation > alpha)
                        evaluation = Minimax(board, depth - 1, alpha, best_eval, child_pv, nodes, ply + 1);
                }
                if (return_cell != NO_CAPTURE) {
                    board.UnSetPiece(move.taken_piece, return_cell);
                    searched_return_cells[searched_return_count++] = return_cell;
                }
                if (IsInterrupted()) {
                    board.UnMakeMove(move);
                    pv.Clear();
                    return 0;
                }
                if (evaluation <= alpha) {
                    if (IsQuiet(move)) {
                        const int bonus = HistoryBonus(depth);
                        UpdateHistory(side, move, bonus);
                        for (int j = 0; j < searched_quiet_count; j++)
                            UpdateHistory(side, searched_quiets[j], -bonus / 2);
                        if (ply < MAX_PLY) {
                            killers[ply][1] = killers[ply][0];
                            killers[ply][0] = move;
                        }
                    } else if (IsCapture(move))
                        set_history::UpdateSetHistory(return_cell, {searched_return_cells.data(), static_cast<std::size_t>(searched_return_count)}, side, move.taken_piece, set_history::SetHistoryBonus(depth));
                    pv.Set((return_cell == NO_CAPTURE) ? move : MoveTar(move, return_cell), child_pv);
                    TTEntry new_entry;
                    new_entry.key = node_key;
                    new_entry.depth = depth;
                    new_entry.score = ScoreToTT(alpha, ply);
                    new_entry.flag = UPPERBOUND;
                    new_entry.best_move = tar_move;
                    StoreTT(new_entry);
                    board.UnMakeMove(move);
                    return alpha;
                } 

                if (evaluation < best_eval) {
                    best_eval = evaluation;
                    best_move = tar_move;
                    best_pv = child_pv;
                }

                if (IsQuiet(move)) searched_quiets[searched_quiet_count++] = move;
            }
            board.UnMakeMove(move);
        }
        if (legal_moves == 0) {
            int eval = board.LegalTest(true) ? 0 : MATE_VALUE - ply;
            pv.Clear();
            TTEntry new_entry;
            new_entry.key = node_key;
            new_entry.depth = depth;
            new_entry.score = ScoreToTT(eval, ply);          
            new_entry.flag = EXACT;          
            new_entry.best_move = NO_MOVE;
            StoreTT(new_entry);
            return eval;
        } else if (!(best_move == NO_MOVE)) 
            pv.Set(best_move, best_pv);
        else 
            pv.Clear();

        TTEntry new_entry;
        new_entry.key = node_key;
        new_entry.depth = depth;
        new_entry.score = ScoreToTT(best_eval, ply);
        new_entry.best_move = best_move;

        if (best_eval >= beta)
            new_entry.flag = LOWERBOUND;
        else if (best_eval <= alpha)
            new_entry.flag = UPPERBOUND;
        else
            new_entry.flag = EXACT;
        StoreTT(new_entry);
        return best_eval;
    }
}
