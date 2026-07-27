#include "minimax.h"

#include <cstring>
#include <algorithm>

#include "../../board/board.h"
#include "../../tt/tt.h"
#include "../../moves/pv.h"
#include "../../movegen/generate_moves.h"
#include "../../eval/standard/evaluate_position.h"
#include "../../history/history.h"
#include "../search.h"

namespace {

constexpr int MAX_SEARCH_PLY = 128;

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

int MoveScore(const Move& move, const Move* tt_move, int ply, int side) {
    if (tt_move != nullptr && move == *tt_move)
        return 1000000;
    if (IsCapture(move)) { return 800000 + move.Different(); }
    if (ply < MAX_SEARCH_PLY) {
        if (move == killers[ply][0])
            return 700'000;
        if (move == killers[ply][1])
            return 690'000;
    }
    return GetHistoryScore(side, move);
}

void MoveSort(Move moves[MAX_MOVES], int count, const Move* tt_move, int ply, int side) {
    std::stable_sort(moves, moves + count, [&](const Move& left,const Move& right) {
        return MoveScore(left, tt_move, ply, side) > MoveScore(right, tt_move, ply, side);
    });
}

int MinimaxCap(Board& board, int alpha, int beta, std::uint64_t& nodes, int ply) {
    if ((nodes & 1023ULL) == 0 && IsInterrupted())
        return 0;
    if (board.GameAbort())
        return 0;
    const bool in_check = !board.LegalTest(!board.turn);
    if (ply >= MAX_SEARCH_PLY)
        return in_check ? 0 : board.EvaluatePosition();
    if (!in_check) {
        int eval = board.EvaluatePosition();
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
    int move_count = movegen::GenerateMoves(board, possible_moves, !in_check);
    if (in_check) {
        Move* good_end = std::partition(possible_moves, possible_moves + move_count, IsCapture);
        std::sort(possible_moves, good_end, [](const Move& a, const Move& b) {
            return a.Different() > b.Different();
        });
    } else std::sort(possible_moves, possible_moves + move_count, [](const Move& a, const Move& b) {
        return a.Different() > b.Different();
    });
    bool possibility = false;
    for (int i = 0; i < move_count; i++) {
        board.MakeMove(possible_moves[i]);
        if (!board.LegalTest(board.turn)) {
            board.UnMakeMove(possible_moves[i]);
            continue;
        }
        possibility = true;
        int evaluation = MinimaxCap(board, alpha, beta, nodes, ply + 1);
        board.UnMakeMove(possible_moves[i]);
        if (IsInterrupted())
            return 0;
        if (board.turn) {
            if (evaluation >= beta)
                return beta;
            if (evaluation > alpha)
                alpha = evaluation;
        } else {
            if (evaluation <= alpha)
                return alpha;
            if (evaluation < beta)
                beta = evaluation;
        }
    }
    if (in_check && !possibility)
        return board.turn ? -MATE_VALUE + ply : MATE_VALUE - ply;
    return board.turn ? alpha : beta;
}

}

int Minimax(Board& board, int depth, int alpha, int beta, PrincipalVariation<Move>& pv, std::uint64_t& nodes, int ply, bool allow_null) {
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
        int eval = MinimaxCap(board, alpha, beta, nodes, ply);
        pv.Clear();
        return eval;
    }

    if (allow_null && depth >= 3 && board.LegalTest(!board.turn) && !board.IsEndgame()) {
        const int reduction = 2 + depth / 4;
        const bool maximizing = board.turn;
        const std::uint8_t saved_en_passant = board.MakeNullMove();
        PrincipalVariation<Move> dummy;
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
    PrincipalVariation<Move> best_pv;
    Move best_move = NO_MOVE;
    const bool node_in_check = !board.LegalTest(!board.turn);
    Move searched_quiets[MAX_MOVES];
    int searched_quiet_count = 0;
    if (board.turn) {
        int best_eval = alpha;
        for (int i = 0; i < move_count; i++) {
            const auto& move = possible_moves[i];
            board.MakeMove(move);
            if (!board.LegalTest(false)) {
                board.UnMakeMove(move);
                continue;
            }
            legal_moves++;
            PrincipalVariation<Move> child_pv;
            int evaluation;
            if (pv_search) {
                evaluation = Minimax(board, depth - 1, best_eval, beta, child_pv, nodes, ply + 1);
                pv_search = false;
            } else {
                const int reduction = GetReduction(depth, legal_moves, node_in_check, !board.LegalTest(!board.turn), IsCapture(move), IsPromotion(move), GetHistoryScore(side, move));
                PrincipalVariation<Move> probe_pv;
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
            board.UnMakeMove(move);
            if (IsInterrupted()) {
                pv.Clear();
                return 0;
            }
            if (evaluation >= beta) {
                if (IsQuiet(move)) {
                    const int bonus = HistoryBonus(depth);
                    UpdateHistory(side, move, bonus);
                    for (int j = 0; j < searched_quiet_count; j++)
                        UpdateHistory(side, searched_quiets[j], -bonus / 2);
                    if (ply < MAX_SEARCH_PLY) {
                        killers[ply][1] = killers[ply][0];
                        killers[ply][0] = move;
                    }
                }
                pv.Set(move, child_pv);
                TTEntry new_entry;
                new_entry.key = board.zobrist_hash;
                new_entry.depth = depth;
                new_entry.score = ScoreToTT(beta, ply);
                new_entry.flag = LOWERBOUND;
                new_entry.best_move = move;
                StoreTT(new_entry);
                return beta;
            }
            
            if (evaluation > best_eval) {
                best_eval = evaluation;
                best_move = move;
                best_pv = child_pv;
            }
            if (IsQuiet(move)) searched_quiets[searched_quiet_count++] = move;
        }
        if (legal_moves == 0) {
            int eval = board.LegalTest(false) ? 0 : -MATE_VALUE + ply;
            pv.Clear();
            TTEntry new_entry;
            new_entry.key = board.zobrist_hash;
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
        new_entry.key = board.zobrist_hash;
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
            if (!board.LegalTest(true)) {
                board.UnMakeMove(move);
                continue;
            }
            legal_moves++;
            PrincipalVariation<Move> child_pv;
            int evaluation;
            if (pv_search) {
                evaluation = Minimax(board, depth - 1, alpha, best_eval, child_pv, nodes, ply + 1);
                pv_search = false;
            } else {
                const int reduction = GetReduction(depth, legal_moves, node_in_check, !board.LegalTest(!board.turn), IsCapture(move), IsPromotion(move), GetHistoryScore(side, move));
                PrincipalVariation<Move> probe_pv;
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
            board.UnMakeMove(move);
            if (IsInterrupted()) {
                pv.Clear();
                return 0;
            }
            if (evaluation <= alpha) {
                if (IsQuiet(move)) {
                    const int bonus = HistoryBonus(depth);
                    UpdateHistory(side, move, bonus);
                    for (int j = 0; j < searched_quiet_count; j++)
                        UpdateHistory(side, searched_quiets[j], -bonus / 2);
                    if (ply < MAX_SEARCH_PLY) {
                        killers[ply][1] = killers[ply][0];
                        killers[ply][0] = move;
                    }
                }
                pv.Set(move, child_pv);
                TTEntry new_entry;
                new_entry.key = board.zobrist_hash;
                new_entry.depth = depth;
                new_entry.score = ScoreToTT(alpha, ply);
                new_entry.flag = UPPERBOUND;
                new_entry.best_move = move;
                StoreTT(new_entry);
                return alpha;
            } 

            if (evaluation < best_eval) {
                best_eval = evaluation;
                best_move = move;
                best_pv = child_pv;
            }

            if (IsQuiet(move)) searched_quiets[searched_quiet_count++] = move;
        }
        if (legal_moves == 0) {
            int eval = board.LegalTest(true) ? 0 : MATE_VALUE - ply;
            pv.Clear();
            TTEntry new_entry;
            new_entry.key = board.zobrist_hash;
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
        new_entry.key = board.zobrist_hash;
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