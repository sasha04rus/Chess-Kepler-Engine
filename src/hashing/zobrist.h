#include <cstdint>

namespace zobrist {

extern const std::uint8_t ep_square_to_index[64];

extern std::uint64_t piece[2][6][64];   
extern std::uint64_t castling[4];       
extern std::uint64_t en_passant[16];      
extern std::uint64_t turn;

void Init();

} // namespace zobrist