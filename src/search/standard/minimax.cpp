#include "minimax.h"

#include <cstring>
#include <algorithm>

#include "../../board/board.h"
#include "../../tt/tt.h"
#include "../../moves/pv.h"
#include "../../movegen/generate_moves.h"
#include "../../eval/standard/evaluate_position.h"
#include "../search.h"

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

void MoveSort(Move moves[218], int count, const Move* prev_best_move, int ply) {
    bool tt_move_found = false;
    if (prev_best_move != nullptr) {
        for (int i = 0; i < count; i++) {
            if (moves[i] == *prev_best_move) {
                std::swap(moves[0], moves[i]);
                tt_move_found = true;
                break;
            }
        }
    }
    Move* start = tt_move_found ? moves + 1 : moves;
    Move* end = moves + count;
    Move* good_end = std::partition(start, end, IsCapture);
    std::sort(start, good_end, [](const Move& a, const Move& b) {
        return a.Different() > b.Different();
    });
    for (Move* find = good_end; find < end; find++) {
        if (*find == killers[ply][0] || *find == killers[ply][1]) {
            std::swap(*find, *good_end);
            good_end++;
        }
    }
}

int MinimaxCap(Board& board, int alpha, int beta, std::uint64_t& nodes) {
    if ((nodes & 1023ULL) == 0 && IsInterrupted()) return 0;
    int eval = board.EvaluatePosition();
    if (board.turn) {
        if (eval >= beta) return beta;
        if (eval > alpha) alpha = eval;
    } else {
        if (eval <= alpha) return alpha;
        if (eval < beta) beta = eval;
    }
    nodes++;
    Move possible_moves[MAX_MOVES];
    int move_count = movegen::GenerateMoves(board, possible_moves, true);
    std::sort(possible_moves, possible_moves + move_count, [](const Move& a, const Move& b) {return a.Different() > b.Different();});
    for (int i = 0; i < move_count; i++) {
        board.MakeMove(possible_moves[i]);
        if (!board.LegalTest(board.turn)) {
                board.UnMakeMove(possible_moves[i]);
                continue;
            }
        int evaluation = MinimaxCap(board, alpha, beta, nodes);
        board.UnMakeMove(possible_moves[i]);
        if (board.turn) {
            if (evaluation >= beta) return beta;
            if (evaluation > alpha) alpha = evaluation;
        } else {
            if (evaluation <= alpha) return alpha;
            if (evaluation < beta) beta = evaluation;
        }
    }
    return board.turn ? alpha : beta;
}

}

int Minimax(Board& board, std::uint8_t depth, int alpha, int beta, PrincipalVariation<Move>& pv, std::uint64_t& nodes, int ply, bool allow_null) {
    if ((nodes & 1023ULL) == 0 && IsInterrupted()) {
        pv.Clear();
        return 0;
    }
    if (board.GameAbort()) return 0;

    TTEntry entry{};
    const bool tt_hit = ProbeTT(board.zobrist_hash, entry);
    if (tt_hit && ply > 0 && entry.depth >= depth) {
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
        int eval = MinimaxCap(board, alpha, beta, nodes);
        pv.Clear();
        return eval;
    }

    if (allow_null && depth >= 3 && board.LegalTest(!board.turn) && !board.IsEndgame()) {
        const int reduction = 2 + depth / 4;
        const bool maximizing = board.turn;
        board.MakeNullMove();
        PrincipalVariation<Move> dummy;
        int score;
        if (maximizing)
            score = Minimax(board, depth - reduction - 1, beta - 1, beta, dummy, nodes, ply + 1, false);
        else
            score = Minimax(board, depth - reduction - 1, alpha, alpha + 1, dummy, nodes, ply + 1, false);
        board.UnMakeNullMove();
        if (IsInterrupted()) {
            pv.Clear();
            return 0;
        }
        if (maximizing && score >= beta)
            return beta;
        if (!maximizing && score <= alpha)
            return alpha;
    }

    bool possibility = false;
    Move possible_moves[MAX_MOVES];
    int move_count = movegen::GenerateMoves(board, possible_moves);

    if (tt_hit && !(entry.best_move == NO_MOVE))
        MoveSort(possible_moves, move_count, &entry.best_move, ply);
    else
        MoveSort(possible_moves, move_count, nullptr, ply);

    PrincipalVariation<Move> best_pv;
    Move best_move = NO_MOVE;
    const bool node_in_check = !board.LegalTest(!board.turn);
    if (board.turn) {
        int best_eval = alpha;
        for (int i = 0; i < move_count; i++) {
            board.MakeMove(possible_moves[i]);
            if (!board.LegalTest(false)) {
                board.UnMakeMove(possible_moves[i]);
                continue;
            }
            possibility = true;
            PrincipalVariation<Move> child_pv;
            int reduction = (depth >= 3 && i > 5 && !IsCapture(possible_moves[i]) && board.LegalTest(!board.turn) && !node_in_check) ? (i / 6) : 0;
            int evaluation;
            if (reduction) {
                evaluation = Minimax(board, ((depth - 1 - reduction) > 0) ? depth - 1 - reduction : 1, best_eval, beta, child_pv, nodes, ply + 1);
                if (evaluation > best_eval)
                    evaluation = Minimax(board, depth - 1, best_eval, beta, child_pv, nodes, ply + 1);
            } else 
                evaluation = Minimax(board, depth - 1, best_eval, beta, child_pv, nodes, ply + 1);
            board.UnMakeMove(possible_moves[i]);
            if (IsInterrupted()) {
                pv.Clear();
                return 0;
            }
            if (evaluation >= beta) {
                if (!IsCapture(possible_moves[i])) {
                    killers[ply][1] = killers[ply][0];
                    killers[ply][0] = possible_moves[i];
                }
                pv.Set(possible_moves[i], child_pv);
                TTEntry new_entry;
                new_entry.key = board.zobrist_hash;
                new_entry.depth = depth;
                new_entry.score = ScoreToTT(beta, ply);
                new_entry.flag = LOWERBOUND;
                new_entry.best_move = possible_moves[i];
                StoreTT(new_entry);
                return beta;
            } else if (evaluation > best_eval) {
                best_eval = evaluation;
                best_move = possible_moves[i];
                best_pv = child_pv;
            }
        }
        if (!possibility) {
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
            board.MakeMove(possible_moves[i]);
            if (!board.LegalTest(true)) {
                board.UnMakeMove(possible_moves[i]);
                continue;
            }
            possibility = true;
            PrincipalVariation<Move> child_pv;
            int reduction = (depth >= 3 && i > 5 && !IsCapture(possible_moves[i]) && board.LegalTest(!board.turn) && !node_in_check) ? (i / 6) : 0;
            int evaluation;
            if (reduction) {
                evaluation = Minimax(board, ((depth - 1 - reduction) > 0) ? depth - 1 - reduction : 1, alpha, best_eval, child_pv, nodes, ply + 1);
                if (evaluation < best_eval)
                    evaluation = Minimax(board, depth - 1, alpha, best_eval, child_pv, nodes, ply + 1);
            } else
                evaluation = Minimax(board, depth - 1, alpha, best_eval, child_pv, nodes, ply + 1);
            board.UnMakeMove(possible_moves[i]);
            if (IsInterrupted()) {
                pv.Clear();
                return 0;
            }
            if (evaluation <= alpha) {
                if (!IsCapture(possible_moves[i])) {
                    killers[ply][1] = killers[ply][0];
                    killers[ply][0] = possible_moves[i];
                }
                pv.Set(possible_moves[i], child_pv);
                TTEntry new_entry;
                new_entry.key = board.zobrist_hash;
                new_entry.depth = depth;
                new_entry.score = ScoreToTT(alpha, ply);
                new_entry.flag = UPPERBOUND;
                new_entry.best_move = possible_moves[i];
                StoreTT(new_entry);
                return alpha;
            } else if (evaluation < best_eval) {
                best_eval = evaluation;
                best_move = possible_moves[i];
                best_pv = child_pv;
            }
        }
        if (!possibility) {
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