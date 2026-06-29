#include <cstring>

template <typename MoveType>
struct PrincipalVariation {
    MoveType moves[30];
    int length = 0;

    PrincipalVariation() = default;
    PrincipalVariation(const PrincipalVariation& other) {
        length = other.length;
        memcpy(moves, other.moves, length * sizeof(MoveType));
    }

    void Set(const MoveType& move, const PrincipalVariation& rest) {
        moves[0] = move;
        memcpy(moves + 1, rest.moves, rest.length * sizeof(MoveType));
        length = rest.length + 1;
    }

    void Clear() { length = 0; }
};