#include "kivapfa/PlaybackCursor.h"
#include <algorithm>

namespace kivapfa {

PlaybackCursor::PlaybackCursor(PagedNoteStore& store) : store_(store) {}

void PlaybackCursor::seek(std::int64_t timeUs) {
    for (std::uint16_t k = 0; k < 128; ++k) {
        const auto p = store_.lowerBoundEnd(static_cast<std::uint8_t>(k), timeUs);
        firstRender_[k] = p;
        firstUnhit_[k] = p;
    }
    lastTimeUs_ = timeUs;
}

std::size_t PlaybackCursor::collectInto(std::int64_t timeUs, std::int64_t lookAheadUs,
                                       VisibleNote* dst, std::size_t hardCap) {
    if (!dst || hardCap == 0) return 0;
    if (lastTimeUs_ < 0 || timeUs < lastTimeUs_) seek(timeUs);
    const auto cutoff = timeUs + lookAheadUs;
    const double denom = static_cast<double>(std::max<std::int64_t>(1, lookAheadUs));
    std::size_t outCount = 0;

    for (std::uint16_t k = 0; k < 128; ++k) {
        const auto count = store_.index().keys[k].note_count;
        auto& r = firstRender_[k];
        auto& u = firstUnhit_[k];

        while (r < count) {
            const auto n = store_.read(static_cast<std::uint8_t>(k), r);
            if (n.end_us >= timeUs) break;
            ++r;
        }
        if (u < r) u = r;

        std::uint64_t i = r;
        while (i < count && outCount < hardCap) {
            const auto n = store_.read(static_cast<std::uint8_t>(k), i);
            if (n.start_us >= cutoff) break;
            if (n.end_us >= timeUs) {
                dst[outCount++] = VisibleNote{
                    static_cast<float>((n.start_us - timeUs) / denom),
                    static_cast<float>((n.end_us - timeUs) / denom),
                    k,
                    velocityOf(n),
                    channelOf(n),
                    trackOf(n)
                };
            }
            ++i;
        }

        while (u < count) {
            const auto n = store_.read(static_cast<std::uint8_t>(k), u);
            if (n.start_us >= timeUs) break;
            ++u;
        }
        if (outCount >= hardCap) break;
    }
    lastTimeUs_ = timeUs;
    return outCount;
}


std::size_t PlaybackCursor::collectPacked4(std::int64_t timeUs, std::int64_t lookAheadUs,
                                          float* dst4, std::size_t hardCap) {
    if (!dst4 || hardCap == 0) return 0;
    if (lastTimeUs_ < 0 || timeUs < lastTimeUs_) seek(timeUs);
    const auto cutoff = timeUs + lookAheadUs;
    const double denom = static_cast<double>(std::max<std::int64_t>(1, lookAheadUs));
    std::size_t outCount = 0;

    for (std::uint16_t k = 0; k < 128; ++k) {
        const auto count = store_.index().keys[k].note_count;
        auto& r = firstRender_[k];
        auto& u = firstUnhit_[k];
        while (r < count) {
            const auto n = store_.read(static_cast<std::uint8_t>(k), r);
            if (n.end_us >= timeUs) break;
            ++r;
        }
        if (u < r) u = r;

        std::uint64_t i = r;
        while (i < count && outCount < hardCap) {
            const auto n = store_.read(static_cast<std::uint8_t>(k), i);
            if (n.start_us >= cutoff) break;
            if (n.end_us >= timeUs) {
                float* d = dst4 + outCount * 4;
                d[0] = static_cast<float>((n.start_us - timeUs) / denom);
                d[1] = static_cast<float>((n.end_us - timeUs) / denom);
                d[2] = static_cast<float>(k);
                d[3] = static_cast<float>(velocityOf(n)) / 127.0f;
                ++outCount;
            }
            ++i;
        }
        while (u < count) {
            const auto n = store_.read(static_cast<std::uint8_t>(k), u);
            if (n.start_us >= timeUs) break;
            ++u;
        }
        if (outCount >= hardCap) break;
    }
    lastTimeUs_ = timeUs;
    return outCount;
}

std::vector<VisibleNote> PlaybackCursor::collect(std::int64_t timeUs, std::int64_t lookAheadUs,
                                                std::size_t hardCap) {
    std::vector<VisibleNote> out(hardCap);
    const auto n = collectInto(timeUs, lookAheadUs, out.data(), hardCap);
    out.resize(n);
    return out;
}

std::uint64_t PlaybackCursor::notesPassed() const {
    std::uint64_t n = 0;
    for (auto v : firstUnhit_) n += v;
    return n;
}

} // namespace kivapfa
