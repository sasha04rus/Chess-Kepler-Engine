#pragma once

enum class Flag {
    kDefault = 0,
    kLongMove = 1,
    kTransformationToKnight = 2,
    kTransformationToBishop = 3,
    kTransformationToRook = 4,
    kTransformationToQueen = 5,
    kTransformationToKnightWithCapture = 6,
    kTransformationToBishopWithCapture = 7,
    kTransformationToRookWithCapture = 8,
    kTransformationToQueenWithCapture = 9,
    kEnPassant = 10,
    kShortWhiteCastling = 11,
    kLongWhiteCastling = 12,
    kShortBlackCastling = 13,
    kLongBlackCastling = 14,
    kCapture = 255 
};