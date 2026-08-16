#pragma once
#include "PagedNoteStore.h"
#include <array>
#include <cstdint>
#include <vector>

namespace kivapfa {

// Kiva-style persistent per-key cursors. Normal playback never rescans from note 0.
class PlaybackCursor {
public:
    explicit PlaybackCursor(PagedNoteStore& store);
    void seek(std::int64_t timeUs);
    std::size_t collectInto(std::int64_t timeUs, std::int64_t lookAheadUs,
                            VisibleNote* dst, std::size_t hardCap);
    // Writes startNorm,endNorm,key,velocityNorm directly for the GLES instance VBO.
    std::size_t collectPacked4(std::int64_t timeUs, std::int64_t lookAheadUs,
                               float* dst4, std::size_t hardCap);
    std::vector<VisibleNote> collect(std::int64_t timeUs, std::int64_t lookAheadUs,
                                     std::size_t hardCap = 500000);
    std::uint64_t notesPassed() const;

private:
    PagedNoteStore& store_;
    std::array<std::uint64_t, 128> firstRender_{};
    std::array<std::uint64_t, 128> firstUnhit_{};
    std::int64_t lastTimeUs_ = -1;
};

} // namespace kivapfa
