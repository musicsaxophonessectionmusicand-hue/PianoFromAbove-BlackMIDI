#include "kivapfa/MidiParser.h"
#include "kivapfa/PagedNoteStore.h"
#include "kivapfa/PlaybackCursor.h"
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

static void writeTestMidi(const char* path) {
    // Format 0, PPQ 480. C4 0..480, E4 240..720, tempo 500000 us/qn.
    const std::uint8_t bytes[] = {
        'M','T','h','d',0,0,0,6,0,0,0,1,1,0xE0,
        'M','T','r','k',0,0,0,23,
        0x00,0x90,60,100,
        0x81,0x70,0x90,64,90,
        0x81,0x70,0x80,60,0,
        0x81,0x70,0x80,64,0,
        0x00,0xFF,0x2F,0x00
    };
    std::ofstream f(path, std::ios::binary); f.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

int main() {
    const char* midi = "/tmp/kivapfa_test.mid";
    const char* page = "/tmp/kivapfa_test.page";
    writeTestMidi(midi);
    auto idx = kivapfa::MidiParser::buildPagedIndex(midi, page);
    assert(idx.total_notes == 2);
    assert(idx.keys[60].note_count == 1);
    assert(idx.keys[64].note_count == 1);
    kivapfa::PagedNoteStore store(page, idx, 4096, 4096);
    kivapfa::PlaybackCursor cursor(store);
    auto v0 = cursor.collect(0, 1000000);
    assert(v0.size() == 2);
    auto v1 = cursor.collect(600000, 1000000);
    assert(v1.size() == 1);
    cursor.seek(200000);
    auto v2 = cursor.collect(200000, 1000000);
    assert(v2.size() == 2);
    std::cout << "PASS notes=" << idx.total_notes << " duration_us=" << idx.duration_us << "\n";
}
