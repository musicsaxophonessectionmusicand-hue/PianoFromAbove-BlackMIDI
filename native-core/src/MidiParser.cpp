#include "kivapfa/MidiParser.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace kivapfa {
namespace {

struct Reader {
    const std::uint8_t* p = nullptr;
    const std::uint8_t* end = nullptr;
    std::uint8_t running = 0;
    std::uint8_t u8() { if (p >= end) throw std::runtime_error("unexpected EOF"); return *p++; }
    std::uint16_t be16() { return (static_cast<std::uint16_t>(u8()) << 8) | u8(); }
    std::uint32_t be32() { return (static_cast<std::uint32_t>(u8()) << 24) | (static_cast<std::uint32_t>(u8()) << 16) | (static_cast<std::uint32_t>(u8()) << 8) | u8(); }
    std::uint32_t var() {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) { auto c = u8(); v = (v << 7) | (c & 0x7f); if (!(c & 0x80)) return v; }
        return v;
    }
    void skip(std::size_t n) { if (static_cast<std::size_t>(end - p) < n) throw std::runtime_error("skip past EOF"); p += n; }
};

struct Mapping {
    int fd = -1;
    std::size_t size = 0;
    const std::uint8_t* data = nullptr;
    explicit Mapping(const std::string& path) {
        fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0) throw std::runtime_error("open MIDI failed: " + std::string(std::strerror(errno)));
        struct stat st{};
        if (::fstat(fd, &st) != 0 || st.st_size < 14) throw std::runtime_error("bad MIDI file");
        size = static_cast<std::size_t>(st.st_size);
        data = static_cast<const std::uint8_t*>(::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (data == MAP_FAILED) { data = nullptr; throw std::runtime_error("mmap MIDI failed"); }
    }
    ~Mapping() { if (data) ::munmap(const_cast<std::uint8_t*>(data), size); if (fd >= 0) ::close(fd); }
};

struct TrackView { const std::uint8_t* begin; const std::uint8_t* end; std::uint16_t id; };

struct HeaderData {
    std::uint16_t format, tracks, division;
    std::vector<TrackView> views;
};

HeaderData parseHeader(const Mapping& m) {
    Reader r{m.data, m.data + m.size};
    if (r.be32() != 0x4d546864u) throw std::runtime_error("not SMF MThd");
    auto hlen = r.be32();
    if (hlen < 6) throw std::runtime_error("bad MIDI header length");
    HeaderData h{r.be16(), r.be16(), r.be16(), {}};
    if (h.division & 0x8000) throw std::runtime_error("SMPTE MIDI timing not supported yet");
    if (hlen > 6) r.skip(hlen - 6);
    h.views.reserve(h.tracks);
    for (std::uint16_t t = 0; t < h.tracks; ++t) {
        if (r.be32() != 0x4d54726bu) throw std::runtime_error("missing MTrk");
        auto len = r.be32();
        if (static_cast<std::size_t>(r.end - r.p) < len) throw std::runtime_error("truncated track");
        h.views.push_back({r.p, r.p + len, t});
        r.skip(len);
    }
    return h;
}

struct Pass1State {
    std::array<std::uint64_t, 128> counts{};
    std::vector<std::pair<std::uint64_t, std::uint32_t>> tempos;
    std::uint64_t maxTick = 0;
};

void scanTrackPass1(const TrackView& tv, Pass1State& out, std::uint8_t threshold) {
    Reader r{tv.begin, tv.end};
    std::uint64_t tick = 0;
    while (r.p < r.end) {
        tick += r.var();
        out.maxTick = std::max(out.maxTick, tick);
        std::uint8_t status = r.u8();
        if (status < 0x80) { --r.p; status = r.running; } else if (status < 0xf0) r.running = status;
        const auto hi = status & 0xf0;
        if (hi == 0x80) { r.skip(2); }
        else if (hi == 0x90) { auto key = r.u8(); auto vel = r.u8(); if (key < 128 && vel >= threshold) out.counts[key]++; }
        else if (hi == 0xa0 || hi == 0xb0 || hi == 0xe0) r.skip(2);
        else if (hi == 0xc0 || hi == 0xd0) r.skip(1);
        else if (status == 0xff) {
            auto type = r.u8(); auto len = r.var();
            if (type == 0x51 && len == 3) {
                std::uint32_t tempo = (static_cast<std::uint32_t>(r.u8()) << 16) | (static_cast<std::uint32_t>(r.u8()) << 8) | r.u8();
                out.tempos.emplace_back(tick, tempo);
            } else r.skip(len);
            if (type == 0x2f) break;
        } else if (status == 0xf0 || status == 0xf7) { auto len = r.var(); r.skip(len); }
        else throw std::runtime_error("unsupported/system MIDI event");
    }
}

std::vector<TempoPoint> makeTempoMap(std::vector<std::pair<std::uint64_t, std::uint32_t>> raw, std::uint16_t ppq) {
    raw.emplace_back(0, 500000);
    std::sort(raw.begin(), raw.end(), [](auto a, auto b){ return a.first < b.first; });
    std::vector<TempoPoint> out;
    for (auto [tick, tempo] : raw) {
        if (!out.empty() && out.back().tick == tick) { out.back().us_per_quarter = tempo; continue; }
        out.push_back({tick, tempo, 0});
    }
    std::int64_t acc = 0;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (i) {
            const auto dt = out[i].tick - out[i-1].tick;
            acc += static_cast<std::int64_t>((static_cast<__int128>(dt) * out[i-1].us_per_quarter) / ppq);
        }
        out[i].accumulated_us = acc;
    }
    return out;
}

std::int64_t tickToUs(const std::vector<TempoPoint>& tempos, std::uint64_t tick, std::uint16_t ppq) {
    auto it = std::upper_bound(tempos.begin(), tempos.end(), tick,
        [](std::uint64_t t, const TempoPoint& p){ return t < p.tick; });
    if (it != tempos.begin()) --it;
    const auto dt = tick - it->tick;
    return it->accumulated_us + static_cast<std::int64_t>((static_cast<__int128>(dt) * it->us_per_quarter) / ppq);
}

struct Pending { std::uint64_t tick; std::uint8_t vel; };

void scanTrackPass2(const TrackView& tv, const std::vector<TempoPoint>& tempos, std::uint16_t ppq,
                    std::uint8_t threshold, NoteRecord* base,
                    const std::array<std::uint64_t,128>& keyBase,
                    std::array<std::uint64_t,128>& writeCursor) {
    Reader r{tv.begin, tv.end};
    std::array<std::vector<Pending>, 128*16> pending;
    std::uint64_t tick = 0;
    auto finish = [&](std::uint8_t key, std::uint8_t ch, std::uint64_t endTick) {
        auto& q = pending[key*16 + ch];
        if (q.empty()) return;
        const auto p = q.back(); q.pop_back();
        if (p.vel < threshold) return;
        const auto index = keyBase[key] + writeCursor[key]++;
        base[index] = NoteRecord{tickToUs(tempos, p.tick, ppq), tickToUs(tempos, endTick, ppq), packMeta(p.vel, ch, tv.id)};
    };

    while (r.p < r.end) {
        tick += r.var();
        std::uint8_t status = r.u8();
        if (status < 0x80) { --r.p; status = r.running; } else if (status < 0xf0) r.running = status;
        const auto hi = status & 0xf0, ch = status & 0x0f;
        if (hi == 0x80) { auto key = r.u8(); r.u8(); if (key < 128) finish(key, ch, tick); }
        else if (hi == 0x90) {
            auto key = r.u8(); auto vel = r.u8();
            if (key < 128) { if (vel == 0) finish(key, ch, tick); else pending[key*16+ch].push_back({tick, vel}); }
        }
        else if (hi == 0xa0 || hi == 0xb0 || hi == 0xe0) r.skip(2);
        else if (hi == 0xc0 || hi == 0xd0) r.skip(1);
        else if (status == 0xff) { auto type = r.u8(); auto len = r.var(); r.skip(len); if (type == 0x2f) break; }
        else if (status == 0xf0 || status == 0xf7) { auto len = r.var(); r.skip(len); }
        else throw std::runtime_error("unsupported/system MIDI event");
    }
    // Close hanging notes at track end.
    for (std::uint16_t key=0; key<128; ++key) for (std::uint8_t ch=0; ch<16; ++ch)
        while (!pending[key*16+ch].empty()) finish(static_cast<std::uint8_t>(key), ch, tick);
}

} // namespace

ParsedIndex MidiParser::buildPagedIndex(const std::string& midiPath, const std::string& pagePath,
                                        std::uint8_t velocityThreshold) {
    Mapping midi(midiPath);
    auto hdr = parseHeader(midi);
    Pass1State p1;
    for (const auto& tv : hdr.views) scanTrackPass1(tv, p1, velocityThreshold);

    ParsedIndex idx;
    idx.format = hdr.format; idx.tracks = hdr.tracks; idx.ppq = hdr.division;
    idx.tempos = makeTempoMap(std::move(p1.tempos), idx.ppq);
    idx.duration_us = tickToUs(idx.tempos, p1.maxTick, idx.ppq);

    std::array<std::uint64_t,128> keyBase{};
    std::uint64_t records = 0;
    for (std::uint16_t k=0; k<128; ++k) {
        keyBase[k] = records;
        idx.keys[k].byte_offset = records * sizeof(NoteRecord);
        idx.keys[k].note_count = p1.counts[k];
        records += p1.counts[k];
    }
    idx.total_notes = records;

    int fd = ::open(pagePath.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) throw std::runtime_error("create pagefile failed");
    const std::uint64_t bytes = records * sizeof(NoteRecord);
    if (::ftruncate(fd, static_cast<off_t>(bytes)) != 0) { ::close(fd); throw std::runtime_error("ftruncate pagefile failed"); }
    NoteRecord* out = nullptr;
    if (bytes) {
        out = static_cast<NoteRecord*>(::mmap(nullptr, bytes, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0));
        if (out == MAP_FAILED) { ::close(fd); throw std::runtime_error("mmap pagefile failed"); }
    }

    std::array<std::uint64_t,128> cursor{};
    for (const auto& tv : hdr.views) scanTrackPass2(tv, idx.tempos, idx.ppq, velocityThreshold, out, keyBase, cursor);

    // Each key is independently sorted in-place; OS paging handles data larger than RAM.
    if (records) {
        for (std::uint16_t k=0; k<128; ++k) {
            auto* begin = out + keyBase[k];
            auto* end = begin + idx.keys[k].note_count;
            std::sort(begin, end, [](const NoteRecord& a, const NoteRecord& b) {
                if (a.start_us != b.start_us) return a.start_us < b.start_us;
                return a.end_us < b.end_us;
            });
            auto& prefix = idx.prefix_max_end_by_block[k];
            std::int64_t maxEnd = std::numeric_limits<std::int64_t>::min();
            for (std::uint64_t i = 0; i < idx.keys[k].note_count; ++i) {
                maxEnd = std::max(maxEnd, begin[i].end_us);
                if ((i + 1) % kIndexBlockNotes == 0 || i + 1 == idx.keys[k].note_count)
                    prefix.push_back(maxEnd);
            }
        }
    }
    if (bytes) { ::msync(out, bytes, MS_SYNC); ::munmap(out, bytes); }
    ::close(fd);
    return idx;
}

} // namespace kivapfa
