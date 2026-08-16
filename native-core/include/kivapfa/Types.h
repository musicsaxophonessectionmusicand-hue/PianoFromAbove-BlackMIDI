#pragma once
#include <array>
#include <cstdint>

namespace kivapfa {

#pragma pack(push, 1)
struct NoteRecord {
    std::int64_t start_us;
    std::int64_t end_us;
    std::uint32_t meta; // velocity[0..7], channel[8..11], track[12..27]
};
#pragma pack(pop)
static_assert(sizeof(NoteRecord) == 20, "NoteRecord must stay compact");

struct KeySpan {
    std::uint64_t byte_offset = 0;
    std::uint64_t note_count = 0;
};

struct TempoPoint {
    std::uint64_t tick = 0;
    std::uint32_t us_per_quarter = 500000;
    std::int64_t accumulated_us = 0;
};

struct VisibleNote {
    float start_norm;
    float end_norm;
    std::uint16_t key;
    std::uint8_t velocity;
    std::uint8_t channel;
    std::uint16_t track;
};

inline std::uint8_t velocityOf(const NoteRecord& n) { return static_cast<std::uint8_t>(n.meta & 0xffu); }
inline std::uint8_t channelOf(const NoteRecord& n) { return static_cast<std::uint8_t>((n.meta >> 8) & 0x0fu); }
inline std::uint16_t trackOf(const NoteRecord& n) { return static_cast<std::uint16_t>((n.meta >> 12) & 0xffffu); }
inline std::uint32_t packMeta(std::uint8_t velocity, std::uint8_t channel, std::uint16_t track) {
    return static_cast<std::uint32_t>(velocity) |
           (static_cast<std::uint32_t>(channel & 0x0f) << 8) |
           (static_cast<std::uint32_t>(track) << 12);
}

} // namespace kivapfa
