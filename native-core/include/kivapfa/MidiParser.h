#pragma once
#include "Types.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace kivapfa {

inline constexpr std::size_t kIndexBlockNotes = 4096;

struct ParsedIndex {
    std::array<KeySpan, 128> keys{};
    std::array<std::vector<std::int64_t>, 128> prefix_max_end_by_block{};
    std::vector<TempoPoint> tempos;
    std::uint16_t format = 0;
    std::uint16_t tracks = 0;
    std::uint16_t ppq = 480;
    std::uint64_t total_notes = 0;
    std::int64_t duration_us = 0;
};

// Two-pass Standard MIDI File parser optimized for huge files.
// Pass 1 counts notes + builds global tempo map. Pass 2 writes compact notes
// directly into key-specific slices of a disk-backed pagefile.
class MidiParser {
public:
    static ParsedIndex buildPagedIndex(const std::string& midiPath,
                                       const std::string& pagePath,
                                       std::uint8_t velocityThreshold = 1);
};

} // namespace kivapfa
