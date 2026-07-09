#include "to_string.h"

const char* kBoard[64] = {
    "h1", "g1", "f1", "e1", "d1", "c1", "b1", "a1",
    "h2", "g2", "f2", "e2", "d2", "c2", "b2", "a2",
    "h3", "g3", "f3", "e3", "d3", "c3", "b3", "a3",
    "h4", "g4", "f4", "e4", "d4", "c4", "b4", "a4",
    "h5", "g5", "f5", "e5", "d5", "c5", "b5", "a5",
    "h6", "g6", "f6", "e6", "d6", "c6", "b6", "a6",
    "h7", "g7", "f7", "e7", "d7", "c7", "b7", "a7",
    "h8", "g8", "f8", "e8", "d8", "c8", "b8", "a8"
};

const std::string MoveToString(const Move& move) {
    std::string str;
    str += kBoard[move.from]; 
    str += kBoard[move.to];
    switch ((int)move.flag) {
    case 2:
    case 6:
        str += "n";
        break;
    case 3:
    case 7:
        str += "b";
        break;
    case 4:
    case 8:
        str += "r";
        break;
    case 5:
    case 9:
        str += "q";
        break;
    default:
        break;
    }
    return str;
}

const std::string MoveToString(const MoveTar& move) {
    std::string str;
    str += kBoard[move.from]; 
    str += kBoard[move.to];
    if (move.set != 255)
        str += kBoard[move.set];
    switch ((int)move.flag) {
    case 2:
    case 6:
        str += "n";
        break;
    case 3:
    case 7:
        str += "b";
        break;
    case 4:
    case 8:
        str += "r";
        break;
    case 5:
    case 9:
        str += "q";
        break;
    default:
        break;
    }
    return str;
}