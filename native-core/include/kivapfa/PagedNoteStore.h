#pragma once
#include "MidiParser.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

namespace kivapfa {

class PagedNoteStore {
public:
    PagedNoteStore(std::string path, ParsedIndex index, std::size_t pageNotes = 4096,
                   std::size_t maxResidentBytes = 32u * 1024u * 1024u);
    ~PagedNoteStore();
    PagedNoteStore(const PagedNoteStore&) = delete;
    PagedNoteStore& operator=(const PagedNoteStore&) = delete;

    const ParsedIndex& index() const { return index_; }
    NoteRecord read(std::uint8_t key, std::uint64_t noteIndex);
    std::uint64_t lowerBoundEnd(std::uint8_t key, std::int64_t timeUs);

private:
    struct PageKey {
        std::uint8_t key;
        std::uint64_t page;
        bool operator==(const PageKey& o) const { return key == o.key && page == o.page; }
    };
    struct PageKeyHash {
        std::size_t operator()(const PageKey& k) const {
            return (static_cast<std::size_t>(k.key) << 56) ^ std::hash<std::uint64_t>{}(k.page);
        }
    };
    struct Page {
        std::vector<NoteRecord> notes;
        std::list<PageKey>::iterator lruIt;
    };

    Page& loadPage(std::uint8_t key, std::uint64_t page);
    void trim();

    std::string path_;
    int fd_ = -1;
    ParsedIndex index_;
    std::size_t pageNotes_;
    std::size_t maxResidentBytes_;
    std::size_t residentBytes_ = 0;
    std::unordered_map<PageKey, Page, PageKeyHash> pages_;
    std::list<PageKey> lru_;
};

} // namespace kivapfa
