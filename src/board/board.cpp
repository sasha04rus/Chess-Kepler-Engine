#include "board.h"

#include <string>
#include <cstring>
#include <iostream>

#include "../movegen/generate_moves.h"
#include "../hashing/zobrist.h"
#include "../eval/standard/evaluate_position.h"
#include "../utils/bit_fun.h"
#include "../moves/to_string.h"

RotatedBoard MakeRotatedBoards(Bitboard occupied) {
    RotatedBoard rotated;
    rotated.occupied = occupied;
    
    rotated.rotate90 = 0;
    for (int sq = 0; sq < 64; sq++) {
        if (occupied & (1ULL << sq)) {
            int rank = sq / 8;
            int file = sq % 8;
            int new_sq = file * 8 + rank;  
            rotated.rotate90 |= (1ULL << new_sq);
        }
    }
    
    rotated.rotate45 = 0;
    for (int sq = 0; sq < 64; ++sq) {
        if (!(occupied & (1ULL << sq))) continue;
        int rank = sq / 8;
        int file = sq % 8;   
        int diag_sum = rank + file;               
        int pos_sum = std::min(rank, 7 - file);   
        int packed_index = movegen::kDiagLengthPrefSum[diag_sum] + pos_sum;
        rotated.rotate45 |= (1ULL << packed_index);
    }
    
    rotated.rotate315 = 0;
    for (int sq = 0; sq < 64; ++sq) {
        if (!(occupied & (1ULL << sq))) continue;
        int rank = sq / 8;
        int file = sq % 8;
        int diag_diff = rank - file + 7;          
        int pos_diff = std::min(rank, file);      
        int packed_index = movegen::kDiagLengthPrefSum[diag_diff] + pos_diff;
        rotated.rotate315 |= (1ULL << packed_index);
    }
    
    return rotated;
}

Board::Board(const std::string fen) : turn(true), en_passant(0) {
    const char* fen_cstr = fen.c_str();
    int x = 7;  
    int y = 7;  
    while (*fen_cstr != '\0' && *fen_cstr != ' ' && y >= 0) {
        if (*fen_cstr == '/') { 
            x = 7;
            y--;
        } else if (*fen_cstr >= '1' && *fen_cstr <= '8') {
            x -= (*fen_cstr - '0');
        } else {
            int side = (std::isupper(*fen_cstr)) ? 0 : 1;  
            char piece_char = std::tolower(*fen_cstr);
            int piece_index;  
            switch (piece_char) {
                case 'p': piece_index = 0; break;  
                case 'n': piece_index = 1; break;  
                case 'b': piece_index = 2; break;  
                case 'r': piece_index = 3; break;  
                case 'q': piece_index = 4; break;  
                case 'k': piece_index = 5; break;  
                default: continue;  
            }
            int square = y * 8 + x;
            bitboards[side][piece_index] |= (1ULL << square);
            x--;
        }
        fen_cstr++;
    }
    
    while (*fen_cstr == ' ') fen_cstr++;
    if (*fen_cstr != '\0') {
        turn = (*fen_cstr == 'w');  
        fen_cstr++;
    }
    
    while (*fen_cstr == ' ') fen_cstr++;
    if (*fen_cstr != '\0' && *fen_cstr != '-') {
        const char* temp = fen_cstr;
        while (*temp != ' ' && *temp != '\0') {
            switch (*temp) {
                case 'K': castling[0] = true; break;  
                case 'Q': castling[1] = true; break;  
                case 'k': castling[2] = true; break;  
                case 'q': castling[3] = true; break;  
            }
            temp++;
        }
        fen_cstr = temp;
    } else if (*fen_cstr == '-') {
        fen_cstr++;
    }
    
    while (*fen_cstr == ' ') fen_cstr++;
    if (*fen_cstr != '\0' && *fen_cstr != '-') {
        if (fen_cstr[0] >= 'a' && fen_cstr[0] <= 'h' &&
            fen_cstr[1] >= '1' && fen_cstr[1] <= '8') {
            
            int file = fen_cstr[0] - 'a';      
            int rank = fen_cstr[1] - '1';        
            en_passant = rank * 8 + file;
        }
        
        fen_cstr += 2;
    }  

    for (int i = 0; i < 64; i++)
        pieces[i] = 255;

    for (int sq = 0; sq < 64; sq++)
        for (std::uint8_t piece = 0; piece < 5; piece++)
            if ((((uint64_t)1 << sq) & (bitboards[0][piece] | bitboards[1][piece])) != 0)
                pieces[sq] = piece;
    
    for (int color = 0; color < 2; color++)
        for (int piece = 0; piece < 6; piece++)
            for (int square = 0; square < 64; square++)
                if ((bitboards[color][piece] & ((uint64_t)1 << square)) != 0) 
                    zobrist_hash ^= zobrist::piece[color][piece][square];

    for (int i = 0; i < 4; i++)
        if (castling[i])
            zobrist_hash ^= zobrist::castling[i];

    if (en_passant != 0)
        zobrist_hash ^= zobrist::en_passant[zobrist::ep_square_to_index[en_passant]];

    if (turn) zobrist_hash ^= zobrist::turn;

    Bitboard mask = 0;
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 6; j++)
            mask |= bitboards[i][j];

    rotated = MakeRotatedBoards(mask);
}
  
Board::Board(const Board& other) {
    for (int i = 0; i < 6; i++) {
        bitboards[0][i] = other.bitboards[0][i];
        bitboards[1][i] = other.bitboards[1][i];
    }
    turn = other.turn;
    for (int i = 0; i < 4; i++)
        castling[i] = other.castling[i];
    en_passant = other.en_passant;
    zobrist_hash = other.zobrist_hash;
    ply = other.ply;
    last_irreversible = other.last_irreversible;
    rotated = other.rotated;
    for (int i = 0; i < 64; i++)
        pieces[i] = other.pieces[i];
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < 4; j++)
            st[i].castling[j] = other.st[i].castling[j];
        st[i].en_passant = other.st[i].en_passant;
        st[i].zobrist_hash = other.st[i].zobrist_hash;
        st[i].last_irreversible = other.st[i].last_irreversible;
    }
}

bool Board::GameAbort() const {
    if ((ply - last_irreversible) >= 50)
        return true;

    std::uint8_t repeat_count = 0;
    for (int i = last_irreversible; i < ply; i++) 
        if (zobrist_hash == st[i].zobrist_hash)
            repeat_count++;
    
    if (repeat_count >= 2)
        return true;

    return false;
}

bool Board::IsEndgame() const {
    int count = popcount(rotated.occupied) - (popcount(bitboards[0][0]) + popcount(bitboards[1][0])) - 2;
    return (count > 5) ? false : true;
}

void Board::MakeMove(const Move& move) {
    for (int i = 0; i < 4; i++)
        st[ply].castling[i] = castling[i];
    st[ply].en_passant = en_passant;
    st[ply].zobrist_hash = zobrist_hash;
    st[ply].last_irreversible = last_irreversible;
    ply++;
    rotated.occupied ^= ((uint64_t)1 << move.from);
    rotated.rotate90 ^= movegen::map_to_rotate90[move.from];
    rotated.rotate45 ^= movegen::map_to_rotate45[move.from];
    rotated.rotate315 ^= movegen::map_to_rotate315[move.from];
    switch (move.flag) {
        case Flag::kDefault: 
            bitboards[!turn][move.piece] &= ~((uint64_t)1 << move.from);
            bitboards[!turn][move.piece] |= ((uint64_t)1 << move.to);
            zobrist_hash ^= zobrist::piece[!turn][move.piece][move.from];
            zobrist_hash ^= zobrist::piece[!turn][move.piece][move.to];
            rotated.occupied ^= ((uint64_t)1 << move.to);
            rotated.rotate90 ^= movegen::map_to_rotate90[move.to];
            rotated.rotate45 ^= movegen::map_to_rotate45[move.to];
            rotated.rotate315 ^= movegen::map_to_rotate315[move.to];
            pieces[move.to] = move.piece;
            if (castling[0])
                if ((move.from == 0) || (move.from == 3)) {
                    castling[0] = false;
                    zobrist_hash ^= zobrist::castling[0];
            } if (castling[1]) 
                if ((move.from == 7) || (move.from == 3)) {
                    castling[1] = false;
                    zobrist_hash ^= zobrist::castling[1];
            } if (castling[2]) 
                if ((move.from == 56) || (move.from == 59)) {
                    castling[2] = false;
                    zobrist_hash ^= zobrist::castling[2];
            } if (castling[3]) 
                if ((move.from == 63) || (move.from == 59)) {
                    castling[3] = false;
                    zobrist_hash ^= zobrist::castling[3];
            }
            if (move.piece == 0)
                last_irreversible = ply;
            break;
        case Flag::kLongMove: 
            bitboards[!turn][0] &= ~((uint64_t)1 << move.from);
            bitboards[!turn][0] |= ((uint64_t)1 << move.to);
            zobrist_hash ^= zobrist::piece[!turn][0][move.from];
            zobrist_hash ^= zobrist::piece[!turn][0][move.to];
            rotated.occupied ^= ((uint64_t)1 << move.to);
            rotated.rotate90 ^= movegen::map_to_rotate90[move.to];
            rotated.rotate45 ^= movegen::map_to_rotate45[move.to];
            rotated.rotate315 ^= movegen::map_to_rotate315[move.to];
            pieces[move.to] = 0;
            if (turn) 
                en_passant = move.from + 8;    
            else
                en_passant = move.from - 8;
            zobrist_hash ^= zobrist::en_passant[zobrist::ep_square_to_index[en_passant]];
            turn = !turn;
            zobrist_hash ^= zobrist::turn;
            last_irreversible = ply;
            return;
            break;
        case Flag::kTransformationToKnight: 
        case Flag::kTransformationToBishop:
        case Flag::kTransformationToRook:
        case Flag::kTransformationToQueen:
            bitboards[!turn][0] &= ~((uint64_t)1 << move.from);
            bitboards[!turn][(int)move.flag - 1] |= ((uint64_t)1 << move.to);
            zobrist_hash ^= zobrist::piece[!turn][0][move.from];
            zobrist_hash ^= zobrist::piece[!turn][(int)move.flag - 1][move.to];
            last_irreversible = ply;
            rotated.occupied ^= ((uint64_t)1 << move.to);
            rotated.rotate90 ^= movegen::map_to_rotate90[move.to];
            rotated.rotate45 ^= movegen::map_to_rotate45[move.to];
            rotated.rotate315 ^= movegen::map_to_rotate315[move.to];
            pieces[move.to] = (int)move.flag - 1;
            break;
        case Flag::kTransformationToKnightWithCapture: 
        case Flag::kTransformationToBishopWithCapture:
        case Flag::kTransformationToRookWithCapture:
        case Flag::kTransformationToQueenWithCapture:
            bitboards[!turn][0] &= ~((uint64_t)1 << move.from);
            bitboards[!turn][(int)move.flag - 5] |= ((uint64_t)1 << move.to);
            bitboards[turn][move.taken_piece] &= ~((uint64_t)1 << move.to);
            zobrist_hash ^= zobrist::piece[!turn][0][move.from];
            zobrist_hash ^= zobrist::piece[!turn][(int)move.flag - 5][move.to];
            zobrist_hash ^= zobrist::piece[turn][move.taken_piece][move.to];
            pieces[move.to] = (int)move.flag - 5;
            if (castling[0] && move.to == 0) {
                    castling[0] = false;
                    zobrist_hash ^= zobrist::castling[0];
            } if (castling[1] && move.to == 7) {
                    castling[1] = false;
                    zobrist_hash ^= zobrist::castling[1];
            } if (castling[2] && move.to == 56) {
                    castling[2] = false;
                    zobrist_hash ^= zobrist::castling[2];
            } if (castling[3] && move.to == 63) {
                    castling[3] = false;
                    zobrist_hash ^= zobrist::castling[3];
            } 
            last_irreversible = ply;
            break;
        case Flag::kEnPassant: 
            bitboards[!turn][0] &= ~((uint64_t)1 << move.from);
            bitboards[!turn][0] |= ((uint64_t)1 << en_passant);
            zobrist_hash ^= zobrist::piece[!turn][0][move.from];
            zobrist_hash ^= zobrist::piece[!turn][0][en_passant];
            rotated.occupied ^= ((uint64_t)1 << en_passant);
            rotated.rotate90 ^= movegen::map_to_rotate90[en_passant];
            rotated.rotate45 ^= movegen::map_to_rotate45[en_passant];
            rotated.rotate315 ^= movegen::map_to_rotate315[en_passant];
            pieces[en_passant] = 0;
            if (turn) {
                bitboards[1][0] &= ~((uint64_t)1 << (en_passant - 8));
                zobrist_hash ^= zobrist::piece[1][0][en_passant - 8];
                rotated.occupied ^= ((uint64_t)1 << (en_passant - 8));
                rotated.rotate90 ^= movegen::map_to_rotate90[en_passant - 8];
                rotated.rotate45 ^= movegen::map_to_rotate45[en_passant - 8];
                rotated.rotate315 ^= movegen::map_to_rotate315[en_passant - 8];
            } else {
                bitboards[0][0] &= ~((uint64_t)1 << (en_passant + 8));
                zobrist_hash ^= zobrist::piece[0][0][en_passant + 8];
                rotated.occupied ^= ((uint64_t)1 << (en_passant + 8));
                rotated.rotate90 ^= movegen::map_to_rotate90[en_passant + 8];
                rotated.rotate45 ^= movegen::map_to_rotate45[en_passant + 8];
                rotated.rotate315 ^= movegen::map_to_rotate315[en_passant + 8];
            } 
            last_irreversible = ply;
            break;
        case Flag::kShortWhiteCastling: 
            bitboards[0][3] &= ~(uint64_t)1;
            bitboards[0][3] |= ((uint64_t)1 << 2);
            bitboards[0][5] = ((uint64_t)1 << 1);
            zobrist_hash ^= zobrist::piece[0][3][0];
            zobrist_hash ^= zobrist::piece[0][3][2];
            zobrist_hash ^= zobrist::piece[0][5][3];
            zobrist_hash ^= zobrist::piece[0][5][1];
            rotated.occupied ^= ((uint64_t)1);
            rotated.rotate90 ^= movegen::map_to_rotate90[0];
            rotated.rotate45 ^= movegen::map_to_rotate45[0];
            rotated.rotate315 ^= movegen::map_to_rotate315[0];
            rotated.occupied ^= ((uint64_t)1 << 1);
            rotated.rotate90 ^= movegen::map_to_rotate90[1];
            rotated.rotate45 ^= movegen::map_to_rotate45[1];
            rotated.rotate315 ^= movegen::map_to_rotate315[1];
            rotated.occupied ^= ((uint64_t)1 << 2);
            rotated.rotate90 ^= movegen::map_to_rotate90[2];
            rotated.rotate45 ^= movegen::map_to_rotate45[2];
            rotated.rotate315 ^= movegen::map_to_rotate315[2];
            pieces[2] = 3;
            castling[0] = false;
            castling[1] = false;
            zobrist_hash ^= zobrist::castling[0];
            zobrist_hash ^= zobrist::castling[1];
            last_irreversible = ply;
            break;
        case Flag::kLongWhiteCastling: 
            bitboards[0][3] &= ~((uint64_t)1 << 7);
            bitboards[0][3] |= ((uint64_t)1 << 4);
            bitboards[0][5] = ((uint64_t)1 << 5);
            zobrist_hash ^= zobrist::piece[0][3][7];
            zobrist_hash ^= zobrist::piece[0][3][4];
            zobrist_hash ^= zobrist::piece[0][5][3];
            zobrist_hash ^= zobrist::piece[0][5][5];
            rotated.occupied ^= ((uint64_t)1 << 7);
            rotated.rotate90 ^= movegen::map_to_rotate90[7];
            rotated.rotate45 ^= movegen::map_to_rotate45[7];
            rotated.rotate315 ^= movegen::map_to_rotate315[7];
            rotated.occupied ^= ((uint64_t)1 << 4);
            rotated.rotate90 ^= movegen::map_to_rotate90[4];
            rotated.rotate45 ^= movegen::map_to_rotate45[4];
            rotated.rotate315 ^= movegen::map_to_rotate315[4];
            rotated.occupied ^= ((uint64_t)1 << 5);
            rotated.rotate90 ^= movegen::map_to_rotate90[5];
            rotated.rotate45 ^= movegen::map_to_rotate45[5];
            rotated.rotate315 ^= movegen::map_to_rotate315[5];
            pieces[4] = 3;
            castling[0] = false;
            castling[1] = false;
            zobrist_hash ^= zobrist::castling[0];
            zobrist_hash ^= zobrist::castling[1];
            last_irreversible = ply;
            break;
        case Flag::kShortBlackCastling: 
            bitboards[1][3] &= ~((uint64_t)1 << 56);
            bitboards[1][3] |= ((uint64_t)1 << 58);
            bitboards[1][5] = ((uint64_t)1 << 57);
            zobrist_hash ^= zobrist::piece[1][3][56];
            zobrist_hash ^= zobrist::piece[1][3][58];
            zobrist_hash ^= zobrist::piece[1][5][59];
            zobrist_hash ^= zobrist::piece[1][5][57];
            rotated.occupied ^= ((uint64_t)1 << 56);
            rotated.rotate90 ^= movegen::map_to_rotate90[56];
            rotated.rotate45 ^= movegen::map_to_rotate45[56];
            rotated.rotate315 ^= movegen::map_to_rotate315[56];
            rotated.occupied ^= ((uint64_t)1 << 57);
            rotated.rotate90 ^= movegen::map_to_rotate90[57];
            rotated.rotate45 ^= movegen::map_to_rotate45[57];
            rotated.rotate315 ^= movegen::map_to_rotate315[57];
            rotated.occupied ^= ((uint64_t)1 << 58);
            rotated.rotate90 ^= movegen::map_to_rotate90[58];
            rotated.rotate45 ^= movegen::map_to_rotate45[58];
            rotated.rotate315 ^= movegen::map_to_rotate315[58];
            pieces[58] = 3;
            castling[2] = false;
            castling[3] = false;
            zobrist_hash ^= zobrist::castling[2];
            zobrist_hash ^= zobrist::castling[3];
            last_irreversible = ply;
            break;
        case Flag::kLongBlackCastling: 
            bitboards[1][3] &= ~((uint64_t)1 << 63);
            bitboards[1][3] |= ((uint64_t)1 << 60);
            bitboards[1][5] = ((uint64_t)1 << 61);
            zobrist_hash ^= zobrist::piece[1][3][63];
            zobrist_hash ^= zobrist::piece[1][3][60];
            zobrist_hash ^= zobrist::piece[1][5][59];
            zobrist_hash ^= zobrist::piece[1][5][61];
            rotated.occupied ^= ((uint64_t)1 << 60);
            rotated.rotate90 ^= movegen::map_to_rotate90[60];
            rotated.rotate45 ^= movegen::map_to_rotate45[60];
            rotated.rotate315 ^= movegen::map_to_rotate315[60];
            rotated.occupied ^= ((uint64_t)1 << 61);
            rotated.rotate90 ^= movegen::map_to_rotate90[61];
            rotated.rotate45 ^= movegen::map_to_rotate45[61];
            rotated.rotate315 ^= movegen::map_to_rotate315[61];
            rotated.occupied ^= ((uint64_t)1 << 63);
            rotated.rotate90 ^= movegen::map_to_rotate90[63];
            rotated.rotate45 ^= movegen::map_to_rotate45[63];
            rotated.rotate315 ^= movegen::map_to_rotate315[63];
            pieces[60] = 3;
            castling[2] = false;
            castling[3] = false;
            zobrist_hash ^= zobrist::castling[2];
            zobrist_hash ^= zobrist::castling[3];
            last_irreversible = ply;
            break;
        case Flag::kCapture: 
            bitboards[!turn][move.piece] &= ~((uint64_t)1 << move.from);
            bitboards[!turn][move.piece] |= ((uint64_t)1 << move.to);
            bitboards[turn][move.taken_piece] &= ~((uint64_t)1 << move.to);
            zobrist_hash ^= zobrist::piece[!turn][move.piece][move.from];
            zobrist_hash ^= zobrist::piece[!turn][move.piece][move.to];
            zobrist_hash ^= zobrist::piece[turn][move.taken_piece][move.to];
            pieces[move.to] = move.piece;
            if (castling[0])
                if ((move.from == 0) || (move.from == 3) || (move.to == 0)) {
                    castling[0] = false;
                    zobrist_hash ^= zobrist::castling[0];
            } if (castling[1]) 
                if ((move.from == 7) || (move.from == 3) || (move.to == 7)) {
                    castling[1] = false;
                    zobrist_hash ^= zobrist::castling[1];
            } if (castling[2]) 
                if ((move.from == 56) || (move.from == 59) || (move.to == 56)) {
                    castling[2] = false;
                    zobrist_hash ^= zobrist::castling[2];
            } if (castling[3]) 
                if ((move.from == 63) || (move.from == 59) || (move.to == 63)) {
                    castling[3] = false;
                    zobrist_hash ^= zobrist::castling[3];
            }
            last_irreversible = ply;
            break;
    }
    if (en_passant != 0)
        zobrist_hash ^= zobrist::en_passant[zobrist::ep_square_to_index[en_passant]];
    en_passant = 0;
    turn = !turn;
    zobrist_hash ^= zobrist::turn;
}

void Board::MakeMove(const std::string& str, Variant variant) {
    std::string from_str = str.substr(0, 2);
    std::string to_str = str.substr(2, 2);
    Move move;
    for (int i = 0; i < 64; i++) {
        if (kBoard[i] == from_str)
            move.from = i;
        if (kBoard[i] == to_str)
            move.to = i;
    }
    for (int i = 0; i < 6; i++) 
        if ((bitboards[!turn][i]&((uint64_t)1 << move.from)) != 0) move.piece = i;
    if ((move.piece == 0)&&((move.from + 16 == move.to)||(move.from - 16 == move.to))) move.flag = Flag::kLongMove;
    else if ((move.piece == 0)&&(((move.from == (en_passant - 7))||(move.from == (en_passant + 7))||(move.from == (en_passant - 9))||(move.from == (en_passant + 9)))&&(move.to == en_passant))) move.flag = Flag::kEnPassant;
    else if ((move.from == 3)&&(move.to == 1)&&(move.piece == 5)) move.flag = Flag::kShortWhiteCastling;
    else if ((move.from == 3)&&(move.to == 5)&&(move.piece == 5)) move.flag = Flag::kLongWhiteCastling;
    else if ((move.from == 59)&&(move.to == 57)&&(move.piece == 5)) move.flag = Flag::kShortBlackCastling;
    else if ((move.from == 59)&&(move.to == 61)&&(move.piece == 5)) move.flag = Flag::kLongBlackCastling;
    else if ( str.length() - 1)
        switch (str.back()) {
        case 'n':
            move.flag = Flag::kTransformationToKnight;
            break;
        case 'b':
            move.flag = Flag::kTransformationToBishop;
            break;
        case 'r':
            move.flag = Flag::kTransformationToRook;
            break;
        case 'q':
            move.flag = Flag::kTransformationToQueen;
            break;
        default:
            break;
        }

    for(int i = 0; i < 6; i++) {
        if ((bitboards[turn][i]&((uint64_t)1 << move.to)) != 0) {
            move.taken_piece = i;
            if (move.flag != Flag::kDefault) 
                move.flag = (Flag)((int)move.flag + 4);
            else 
                move.flag = Flag::kCapture;
        }
    }
    
    switch (variant) {
    case Variant::kStandard:
        this->MakeMove(move);
        break;
    case Variant::kTakeAndReturn: 
        this->MakeMove(move);
        if (IsReversible(move))
            last_irreversible = st[ply-1].last_irreversible;
        if (str.length() > 5) {
            std::string set_str = str.substr(4, 2);
            for (int i = 0; i < 64; i++)
                if (kBoard[i] == set_str)
                    this->SetPiece(move.taken_piece, i);
        }
        break;
    default:
        break;
    }
}

void Board::UnMakeMove(const Move& move) {
    ply--;
    for (int i = 0; i < 4; i++) 
        castling[i] = st[ply].castling[i];
    en_passant = st[ply].en_passant;
    zobrist_hash = st[ply].zobrist_hash;
    last_irreversible = st[ply].last_irreversible;
    turn = !turn;
    rotated.occupied ^= ((uint64_t)1 << move.from);
    rotated.rotate90 ^= movegen::map_to_rotate90[move.from];
    rotated.rotate45 ^= movegen::map_to_rotate45[move.from];
    rotated.rotate315 ^= movegen::map_to_rotate315[move.from];
    pieces[move.from] = move.piece;
    switch (move.flag) {
        case Flag::kDefault: 
            bitboards[!turn][move.piece] &= ~((uint64_t)1 << move.to);
            bitboards[!turn][move.piece] |= ((uint64_t)1 << move.from);
            rotated.occupied ^= ((uint64_t)1 << move.to);
            rotated.rotate90 ^= movegen::map_to_rotate90[move.to];
            rotated.rotate45 ^= movegen::map_to_rotate45[move.to];
            rotated.rotate315 ^= movegen::map_to_rotate315[move.to];
            break;
        case Flag::kLongMove: 
            bitboards[!turn][0] &= ~((uint64_t)1 << move.to);
            bitboards[!turn][0] |= ((uint64_t)1 << move.from);
            rotated.occupied ^= ((uint64_t)1 << move.to);
            rotated.rotate90 ^= movegen::map_to_rotate90[move.to];
            rotated.rotate45 ^= movegen::map_to_rotate45[move.to];
            rotated.rotate315 ^= movegen::map_to_rotate315[move.to];
            break;
        case Flag::kTransformationToKnight: 
        case Flag::kTransformationToBishop:
        case Flag::kTransformationToRook:
        case Flag::kTransformationToQueen:
            bitboards[!turn][(int)move.flag - 1] &= ~((uint64_t)1 << move.to);
            bitboards[!turn][0] |= ((uint64_t)1 << move.from);
            rotated.occupied ^= ((uint64_t)1 << move.to);
            rotated.rotate90 ^= movegen::map_to_rotate90[move.to];
            rotated.rotate45 ^= movegen::map_to_rotate45[move.to];
            rotated.rotate315 ^= movegen::map_to_rotate315[move.to];
            break;
        case Flag::kTransformationToKnightWithCapture: 
        case Flag::kTransformationToBishopWithCapture:
        case Flag::kTransformationToRookWithCapture:
        case Flag::kTransformationToQueenWithCapture:
            bitboards[!turn][(int)move.flag - 5] &= ~((uint64_t)1 << move.to);
            bitboards[!turn][0] |= ((uint64_t)1 << move.from);
            bitboards[turn][move.taken_piece] |= ((uint64_t)1 << move.to);
            pieces[move.to] = move.taken_piece;
            break;
        case Flag::kEnPassant: 
            bitboards[!turn][0] |= ((uint64_t)1 << move.from);
            bitboards[!turn][0] &= ~((uint64_t)1 << en_passant);
            rotated.occupied ^= ((uint64_t)1 << en_passant);
            rotated.rotate90 ^= movegen::map_to_rotate90[en_passant];
            rotated.rotate45 ^= movegen::map_to_rotate45[en_passant];
            rotated.rotate315 ^= movegen::map_to_rotate315[en_passant];
            if (turn) { 
                bitboards[1][0] |= ((uint64_t)1 << (en_passant - 8)); 
                rotated.occupied ^= ((uint64_t)1 << (en_passant - 8));
                rotated.rotate90 ^= movegen::map_to_rotate90[en_passant - 8];
                rotated.rotate45 ^= movegen::map_to_rotate45[en_passant - 8];
                rotated.rotate315 ^= movegen::map_to_rotate315[en_passant - 8];
                pieces[en_passant - 8] = 0;
            } else { 
                bitboards[0][0] |= ((uint64_t)1 << (en_passant + 8)); 
                rotated.occupied ^= ((uint64_t)1 << (en_passant + 8));
                rotated.rotate90 ^= movegen::map_to_rotate90[en_passant + 8];
                rotated.rotate45 ^= movegen::map_to_rotate45[en_passant + 8];
                rotated.rotate315 ^= movegen::map_to_rotate315[en_passant + 8];
                pieces[en_passant + 8] = 0;
            } break;
        case Flag::kShortWhiteCastling: 
            bitboards[0][3] |= (uint64_t)1;
            bitboards[0][3] &= ~((uint64_t)1 << 2);
            bitboards[0][5] = ((uint64_t)1 << 3);
            rotated.occupied ^= ((uint64_t)1);
            rotated.rotate90 ^= movegen::map_to_rotate90[0];
            rotated.rotate45 ^= movegen::map_to_rotate45[0];
            rotated.rotate315 ^= movegen::map_to_rotate315[0];
            rotated.occupied ^= ((uint64_t)1 << 1);
            rotated.rotate90 ^= movegen::map_to_rotate90[1];
            rotated.rotate45 ^= movegen::map_to_rotate45[1];
            rotated.rotate315 ^= movegen::map_to_rotate315[1];
            rotated.occupied ^= ((uint64_t)1 << 2);
            rotated.rotate90 ^= movegen::map_to_rotate90[2];
            rotated.rotate45 ^= movegen::map_to_rotate45[2];
            rotated.rotate315 ^= movegen::map_to_rotate315[2];
            pieces[0] = 3;
            break;
        case Flag::kLongWhiteCastling: 
            bitboards[0][3] |= ((uint64_t)1 << 7);
            bitboards[0][3] &= ~((uint64_t)1 << 4);
            bitboards[0][5] = ((uint64_t)1 << 3);
            rotated.occupied ^= ((uint64_t)1 << 7);
            rotated.rotate90 ^= movegen::map_to_rotate90[7];
            rotated.rotate45 ^= movegen::map_to_rotate45[7];
            rotated.rotate315 ^= movegen::map_to_rotate315[7];
            rotated.occupied ^= ((uint64_t)1 << 4);
            rotated.rotate90 ^= movegen::map_to_rotate90[4];
            rotated.rotate45 ^= movegen::map_to_rotate45[4];
            rotated.rotate315 ^= movegen::map_to_rotate315[4];
            rotated.occupied ^= ((uint64_t)1 << 5);
            rotated.rotate90 ^= movegen::map_to_rotate90[5];
            rotated.rotate45 ^= movegen::map_to_rotate45[5];
            rotated.rotate315 ^= movegen::map_to_rotate315[5];
            pieces[7] = 3;
            break;
        case Flag::kShortBlackCastling: 
            bitboards[1][3] |= ((uint64_t)1 << 56);
            bitboards[1][3] &= ~((uint64_t)1 << 58);
            bitboards[1][5] = ((uint64_t)1 << 59);
            rotated.occupied ^= ((uint64_t)1 << 56);
            rotated.rotate90 ^= movegen::map_to_rotate90[56];
            rotated.rotate45 ^= movegen::map_to_rotate45[56];
            rotated.rotate315 ^= movegen::map_to_rotate315[56];
            rotated.occupied ^= ((uint64_t)1 << 57);
            rotated.rotate90 ^= movegen::map_to_rotate90[57];
            rotated.rotate45 ^= movegen::map_to_rotate45[57];
            rotated.rotate315 ^= movegen::map_to_rotate315[57];
            rotated.occupied ^= ((uint64_t)1 << 58);
            rotated.rotate90 ^= movegen::map_to_rotate90[58];
            rotated.rotate45 ^= movegen::map_to_rotate45[58];
            rotated.rotate315 ^= movegen::map_to_rotate315[58];
            pieces[56] = 3;
            break;
        case Flag::kLongBlackCastling: 
            bitboards[1][3] |= ((uint64_t)1 << 63);
            bitboards[1][3] &= ~((uint64_t)1 << 60);
            bitboards[1][5] = ((uint64_t)1 << 59);
            rotated.occupied ^= ((uint64_t)1 << 60);
            rotated.rotate90 ^= movegen::map_to_rotate90[60];
            rotated.rotate45 ^= movegen::map_to_rotate45[60];
            rotated.rotate315 ^= movegen::map_to_rotate315[60];
            rotated.occupied ^= ((uint64_t)1 << 61);
            rotated.rotate90 ^= movegen::map_to_rotate90[61];
            rotated.rotate45 ^= movegen::map_to_rotate45[61];
            rotated.rotate315 ^= movegen::map_to_rotate315[61];
            rotated.occupied ^= ((uint64_t)1 << 63);
            rotated.rotate90 ^= movegen::map_to_rotate90[63];
            rotated.rotate45 ^= movegen::map_to_rotate45[63];
            rotated.rotate315 ^= movegen::map_to_rotate315[63];
            pieces[63] = 3;
            break;
        case Flag::kCapture: 
            bitboards[!turn][move.piece] &= ~((uint64_t)1 << move.to);
            bitboards[!turn][move.piece] |= ((uint64_t)1 << move.from);
            bitboards[turn][move.taken_piece] |= ((uint64_t)1 << move.to);
            pieces[move.to] = move.taken_piece;
            break;
    }
}

void Board::MakeNullMove() {
    turn != turn;
    zobrist_hash ^= zobrist::turn;
}

void Board::UnMakeNullMove() {
    turn != turn;
    zobrist_hash ^= zobrist::turn;
}

void Board::SetPiece(std::uint8_t piece, std::uint8_t square) {
    bitboards[!turn][piece] |= ((uint64_t)1 << square);
    zobrist_hash ^= zobrist::piece[!turn][piece][square];
}

void Board::UnSetPiece(std::uint8_t piece, std::uint8_t square) {
    bitboards[!turn][piece] &= ~((uint64_t)1 << square);
    zobrist_hash ^= zobrist::piece[!turn][piece][square];
}