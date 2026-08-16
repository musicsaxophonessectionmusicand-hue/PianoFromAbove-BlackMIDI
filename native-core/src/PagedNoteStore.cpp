#include "kivapfa/PagedNoteStore.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/types.h>
#include <unistd.h>

namespace kivapfa {

PagedNoteStore::PagedNoteStore(std::string path, ParsedIndex index, std::size_t pageNotes,
                               std::size_t maxResidentBytes)
    : path_(std::move(path)), index_(std::move(index)), pageNotes_(pageNotes),
      maxResidentBytes_(maxResidentBytes) {
    if (pageNotes_ != kIndexBlockNotes) throw std::invalid_argument("pageNotes must match index block size");
    fd_ = ::open(path_.c_str(), O_RDONLY);
    if (fd_ < 0) throw std::runtime_error("open pagefile failed: " + std::string(std::strerror(errno)));
}

PagedNoteStore::~PagedNoteStore() { if (fd_ >= 0) ::close(fd_); }

PagedNoteStore::Page& PagedNoteStore::loadPage(std::uint8_t key, std::uint64_t pageNo) {
    PageKey pk{key, pageNo};
    auto it = pages_.find(pk);
    if (it != pages_.end()) {
        lru_.splice(lru_.begin(), lru_, it->second.lruIt);
        return it->second;
    }

    const auto& span = index_.keys[key];
    const std::uint64_t first = pageNo * pageNotes_;
    if (first >= span.note_count) throw std::out_of_range("page beyond key span");
    const std::uint64_t count = std::min<std::uint64_t>(pageNotes_, span.note_count - first);

    Page p;
    p.notes.resize(static_cast<std::size_t>(count));
    const off_t off = static_cast<off_t>(span.byte_offset + first * sizeof(NoteRecord));
    const std::size_t bytes = p.notes.size() * sizeof(NoteRecord);
    std::size_t done = 0;
    while (done < bytes) {
        const auto n = ::pread(fd_, reinterpret_cast<char*>(p.notes.data()) + done, bytes - done, off + done);
        if (n <= 0) throw std::runtime_error("pread pagefile failed");
        done += static_cast<std::size_t>(n);
    }
    lru_.push_front(pk);
    p.lruIt = lru_.begin();
    residentBytes_ += bytes;
    auto [inserted, ok] = pages_.emplace(pk, std::move(p));
    (void)ok;
    trim();
    return inserted->second;
}

void PagedNoteStore::trim() {
    while (residentBytes_ > maxResidentBytes_ && pages_.size() > 1 && !lru_.empty()) {
        const PageKey victim = lru_.back();
        auto it = pages_.find(victim);
        if (it == pages_.end()) { lru_.pop_back(); continue; }
        residentBytes_ -= it->second.notes.size() * sizeof(NoteRecord);
        lru_.pop_back();
        pages_.erase(it);
    }
}

NoteRecord PagedNoteStore::read(std::uint8_t key, std::uint64_t noteIndex) {
    if (key >= 128 || noteIndex >= index_.keys[key].note_count) throw std::out_of_range("note index");
    const std::uint64_t pageNo = noteIndex / pageNotes_;
    auto& p = loadPage(key, pageNo);
    return p.notes[static_cast<std::size_t>(noteIndex % pageNotes_)];
}

std::uint64_t PagedNoteStore::lowerBoundEnd(std::uint8_t key, std::int64_t timeUs) {
    const auto count = index_.keys[key].note_count;
    if (!count) return 0;

    // First note whose start >= time.
    std::uint64_t lo = 0, hi = count;
    while (lo < hi) {
        const std::uint64_t mid = lo + (hi - lo) / 2;
        const auto n = read(key, mid);
        if (n.start_us < timeUs) lo = mid + 1; else hi = mid;
    }

    // A very long note can start far before the seek point. The parser stores a
    // prefix maximum end-time per 4096-note block, so seek remains correct without
    // rescanning the whole key from note zero.
    std::uint64_t scanFrom = lo;
    const auto& prefix = index_.prefix_max_end_by_block[key];
    if (lo > 0 && !prefix.empty()) {
        auto bit = std::lower_bound(prefix.begin(), prefix.end(), timeUs);
        if (bit != prefix.end()) {
            const std::uint64_t block = static_cast<std::uint64_t>(bit - prefix.begin());
            scanFrom = std::min<std::uint64_t>(scanFrom, block * kIndexBlockNotes);
        }
    }
    while (scanFrom < count) {
        const auto n = read(key, scanFrom);
        if (n.end_us >= timeUs || n.start_us >= timeUs) break;
        ++scanFrom;
    }
    return scanFrom;
}

} // namespace kivapfa
