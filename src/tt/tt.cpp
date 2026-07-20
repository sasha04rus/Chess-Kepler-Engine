#include "tt.h"

#include <algorithm>

TTSlot tt[TT_SIZE];

thread_local Move killers[MAX_PLY][2];

namespace {

constexpr int TO_SHIFT = 6;
constexpr int PIECE_SHIFT = 12;
constexpr int TAKEN_PIECE_SHIFT = 20;
constexpr int MOVE_FLAG_SHIFT = 28;
constexpr int SCORE_SHIFT = 36;
constexpr int DEPTH_SHIFT = 52;
constexpr int TT_FLAG_SHIFT = 60;

std::uint64_t PackMove(const Move& move) {
    std::uint64_t packed = 0;
    packed |= static_cast<std::uint64_t>(move.from & 0x3F);
    packed |= static_cast<std::uint64_t>(move.to & 0x3F) << TO_SHIFT;
    packed |= static_cast<std::uint64_t>(move.piece) << PIECE_SHIFT;
    packed |= static_cast<std::uint64_t>(move.taken_piece) << TAKEN_PIECE_SHIFT;
    packed |= static_cast<std::uint64_t>(static_cast<std::uint8_t>(move.flag)) << MOVE_FLAG_SHIFT;
    return packed;
}

Move UnpackMove(std::uint64_t packed) {
    const std::uint8_t from = static_cast<std::uint8_t>(packed & 0x3F);
    const std::uint8_t to = static_cast<std::uint8_t>((packed >> TO_SHIFT) & 0x3F);
    const std::uint8_t piece = static_cast<std::uint8_t>((packed >> PIECE_SHIFT) & 0xFF);
    const std::uint8_t taken_piece = static_cast<std::uint8_t>((packed >> TAKEN_PIECE_SHIFT) & 0xFF);
    const Flag move_flag = static_cast<Flag>(static_cast<std::uint8_t>((packed >> MOVE_FLAG_SHIFT) & 0xFF));
    return Move(from, to, piece, taken_piece, move_flag);
}

std::uint64_t PackEntryData(const TTEntry& entry) {
    const int depth = std::clamp(entry.depth, 0, 255);
    const std::uint16_t packed_score = static_cast<std::uint16_t>(static_cast<std::int16_t>(entry.score));
    std::uint64_t data = PackMove(entry.best_move);
    data |= static_cast<std::uint64_t>(packed_score) << SCORE_SHIFT;
    data |= static_cast<std::uint64_t>(static_cast<std::uint8_t>(depth)) << DEPTH_SHIFT;
    data |= static_cast<std::uint64_t>(static_cast<std::uint8_t>(entry.flag)) << TT_FLAG_SHIFT;
    return data;
}

void UnpackEntryData(std::uint64_t key, std::uint64_t data, TTEntry& entry) {
    entry.key = key;
    entry.best_move = UnpackMove(data);
    const std::uint16_t packed_score = static_cast<std::uint16_t>((data >> SCORE_SHIFT) & 0xFFFF);
    entry.score = packed_score >= 0x8000 ? static_cast<int>(packed_score) - 0x10000 : static_cast<int>(packed_score);
    entry.depth = static_cast<int>((data >> DEPTH_SHIFT) & 0xFF);
    entry.flag = static_cast<TTFlag>((data >> TT_FLAG_SHIFT) & 0x03);
}

}

bool ProbeTT(std::uint64_t key, TTEntry& entry) {
    TTSlot& slot = tt[key & (TT_SIZE - 1)];
    const std::uint64_t verification_before =
        slot.verification.load(std::memory_order_acquire);
    if (verification_before == 0)
        return false;
    const std::uint64_t data = slot.data.load(std::memory_order_relaxed);
    const std::uint64_t verification_after = slot.verification.load(std::memory_order_acquire);
    if (verification_before != verification_after)
        return false;
    if ((verification_before ^ data) != key)
        return false;
    UnpackEntryData(key, data, entry);
    return true;
}

void StoreTT(const TTEntry& entry) {
    TTEntry old_entry{};
    if (ProbeTT(entry.key, old_entry)) {
        if (old_entry.depth > entry.depth)
            return;
        if (old_entry.depth == entry.depth && old_entry.flag == EXACT && entry.flag != EXACT)
            return;
    }

    TTSlot& slot = tt[entry.key & (TT_SIZE - 1)];
    const std::uint64_t data = PackEntryData(entry);
    slot.data.store(data, std::memory_order_relaxed);
    slot.verification.store(entry.key ^ data, std::memory_order_release);
}

void ClearKiller() {
    for (int i = 0; i < MAX_PLY; ++i) {
        killers[i][0] = NO_MOVE;
        killers[i][1] = NO_MOVE;
    }
}

void ClearTT() {
    for (std::size_t i = 0; i < TT_SIZE; ++i) {
        tt[i].verification.store(0, std::memory_order_relaxed);
        tt[i].data.store(0, std::memory_order_relaxed);
    }
}