#include "move.h"

bool IsReversible(const Move& move) {
    switch (move.flag) {
    case Flag::kDefault:
        return (move.piece == 0) ? true : false;
    case Flag::kLongMove:
    case Flag::kTransformationToKnight:
    case Flag::kTransformationToBishop:
    case Flag::kTransformationToRook:
    case Flag::kTransformationToQueen:
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