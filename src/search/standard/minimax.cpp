#include <cstring>
#include <algorithm>

#include "../../board/board.h"
#include "../../tt/tt.h"
#include "../../moves/pv.h"
#include "../../movegen/generate_moves.h"
#include "../../eval/standard/evaluate_position.h"
#include "../search.h"

#define MAX_MOVES 218

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

void MoveSort(Move moves[218], int count, const Move* prev_best_move, int ply) {
    if (prev_best_move != nullptr) {
        for (int i = 0; i < count; i++) {
            if (moves[i] == *prev_best_move) {
                std::swap(moves[0], moves[i]);
                break;
            }
        }
    }
    Move* start = (prev_best_move == nullptr) ? moves : moves + 1;
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

int Minimax(Board& board, std::uint8_t depth, int alpha, int beta, PrincipalVariation<Move>& pv, std::uint64_t& nodes, int ply) {
    if ((nodes & (1024 - 1)) && IsInterrupted()) return 0;
    if (board.GameAbort()) return 0;

    TTEntry entry = tt[board.zobrist_hash & (TT_SIZE - 1)].load(std::memory_order_relaxed);

    if (entry.key == board.zobrist_hash && entry.depth >= depth) {
        if (abs(entry.score) > MATE_THRESHOLD) {
            if (entry.depth == depth) {
                if (entry.flag == EXACT) return entry.score;
            }
        }

        if (entry.flag == EXACT)
            return entry.score;

        if (entry.flag == LOWERBOUND && entry.score >= beta)
            return entry.score;

        if (entry.flag == UPPERBOUND && entry.score <= alpha)
            return entry.score;
    }

    nodes++;

    if (depth <= 0) {
        int eval = MinimaxCap(board, alpha, beta, nodes);
        pv.Clear();
        return eval;
    }

    if (depth >= 3 && !board.LegalTest(board.turn) && !board.IsEndgame()) {
        int R = 2 + depth / 4;
        board.MakeNullMove();
        PrincipalVariation<Move> dummy;
        int score = Minimax(board, depth - R - 1, beta - 1, beta, dummy, nodes, ply + 1);
        board.UnMakeNullMove();
        if (score >= beta)
            return beta;
    }

    bool possibility = false;
    Move possible_moves[MAX_MOVES];
    int move_count = movegen::GenerateMoves(board, possible_moves);

    if (entry.key == board.zobrist_hash && !(entry.best_move == NO_MOVE))
        MoveSort(possible_moves, move_count, &entry.best_move, ply);
    else
        MoveSort(possible_moves, move_count, nullptr, ply);

    PrincipalVariation<Move> best_pv;
    Move best_move = NO_MOVE;
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
            int reduction = (depth >= 3 && i > 5 && !IsCapture(possible_moves[i]) && board.LegalTest(true)) ? (i / 6) : 0;
            int evaluation;
            if (reduction) {
                evaluation = Minimax(board, ((depth - 1 - reduction) > 0) ? depth - 1 - reduction : 1, best_eval, beta, child_pv, nodes, ply + 1);
                if (evaluation > best_eval)
                    evaluation = Minimax(board, depth - 1, best_eval, beta, child_pv, nodes, ply + 1);
            } else 
                evaluation = Minimax(board, depth - 1, best_eval, beta, child_pv, nodes, ply + 1);
            board.UnMakeMove(possible_moves[i]);
            if (evaluation >= beta) {
                if (!IsCapture(possible_moves[i])) {
                    killers[ply][1] = killers[ply][0];
                    killers[ply][0] = possible_moves[i];
                }
                pv.Set(possible_moves[i], child_pv);
                TTEntry new_entry;
                new_entry.key = board.zobrist_hash;
                new_entry.depth = depth;
                new_entry.score = beta;
                new_entry.flag = LOWERBOUND;
                new_entry.best_move = possible_moves[i];
                tt[board.zobrist_hash & (TT_SIZE - 1)].store(new_entry, std::memory_order_relaxed);
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
            new_entry.score = eval;          
            new_entry.flag = EXACT;          
            new_entry.best_move = NO_MOVE;
            tt[board.zobrist_hash & (TT_SIZE - 1)].store(new_entry, std::memory_order_relaxed);
            return eval;
        } else if (!(best_move == NO_MOVE))
            pv.Set(best_move, best_pv);
        else 
            pv.Clear();

        TTEntry new_entry;
        new_entry.key = board.zobrist_hash;
        new_entry.depth = depth;
        new_entry.score = best_eval;
        new_entry.best_move = best_move;

        if (best_eval <= alpha)
            new_entry.flag = UPPERBOUND;
        else if (best_eval >= beta)
            new_entry.flag = LOWERBOUND;
        else
            new_entry.flag = EXACT;
        tt[board.zobrist_hash & (TT_SIZE - 1)].store(new_entry, std::memory_order_relaxed);
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
            int reduction = (depth >= 3 && i > 5 && !IsCapture(possible_moves[i]) && board.LegalTest(false)) ? (i / 6) : 0;
            int evaluation;
            if (reduction) {
                evaluation = Minimax(board, ((depth - 1 - reduction) > 0) ? depth - 1 - reduction : 1, alpha, best_eval, child_pv, nodes, ply + 1);
                if (evaluation < best_eval)
                    evaluation = Minimax(board, depth - 1, alpha, best_eval, child_pv, nodes, ply + 1);
            } else
                evaluation = Minimax(board, depth - 1, alpha, best_eval, child_pv, nodes, ply + 1);
            board.UnMakeMove(possible_moves[i]);
            if (evaluation <= alpha) {
                if (!IsCapture(possible_moves[i])) {
                    killers[ply][1] = killers[ply][0];
                    killers[ply][0] = possible_moves[i];
                }
                pv.Set(possible_moves[i], child_pv);
                TTEntry new_entry;
                new_entry.key = board.zobrist_hash;
                new_entry.depth = depth;
                new_entry.score = alpha;
                new_entry.flag = UPPERBOUND;
                new_entry.best_move = possible_moves[i];
                tt[board.zobrist_hash & (TT_SIZE - 1)].store(new_entry, std::memory_order_relaxed);
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
            new_entry.score = eval;          
            new_entry.flag = EXACT;          
            new_entry.best_move = NO_MOVE;
            tt[board.zobrist_hash & (TT_SIZE - 1)].store(new_entry, std::memory_order_relaxed);
            return eval;
        } else if (!(best_move == NO_MOVE)) 
            pv.Set(best_move, best_pv);
        else 
            pv.Clear();

        TTEntry new_entry;
        new_entry.key = board.zobrist_hash;
        new_entry.depth = depth;
        new_entry.score = best_eval;
        new_entry.best_move = best_move;

        if (best_eval >= beta)
            new_entry.flag = LOWERBOUND;
        else if (best_eval <= alpha)
            new_entry.flag = UPPERBOUND;
        else
            new_entry.flag = EXACT;
        tt[board.zobrist_hash & (TT_SIZE - 1)].store(new_entry, std::memory_order_relaxed);
        return best_eval;
    }
}