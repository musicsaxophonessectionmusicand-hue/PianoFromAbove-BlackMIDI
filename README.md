# KivaPFA Android

Clean-room Android MIDI visualizer/player engine inspired by the **behavior** of aPFA/Piano From Above and by Kiva's large-MIDI optimization strategy. It does not copy Kiva or aPFA source code.

## What is optimized

- **Two-pass SMF parser**: pass 1 counts notes and builds the tempo map; pass 2 writes exactly-sized note storage.
- **Disk-backed note pagefile**: notes are written directly into 128 key slices instead of becoming millions of Kotlin/Java objects.
- **Compact 20-byte native note records**.
- **Per-key sorted note streams**.
- **Kiva-style persistent cursors** (`firstRender` / `firstUnhit`) so normal playback advances through notes instead of rescanning from the beginning every frame.
- **Seek uses binary search** into each key stream and resets the persistent cursors.
- **LRU page cache** around currently-visible notes; resident note memory is capped (48 MiB in the Android JNI layer).
- **OpenGL ES 3 instanced renderer**: one static rectangle mesh + one per-note instance buffer; no View objects and no per-note draw calls.
- **Native C++ hot path**: MIDI parsing, paging, seeking and visible-note selection never touch the Android GC.

## Architecture

`SMF -> mmap parser -> pass 1 counts/tempo -> pre-sized pagefile -> pass 2 compact notes -> per-key sort -> LRU note pages -> 128 persistent cursors -> visible instance buffer -> GLES3 instanced draw`

This specifically removes the worst Black-MIDI failure mode: an `O(total notes)` scan on every frame. During normal forward playback, work is approximately proportional to notes that become visible / expire plus the notes actually drawn.

## Current scope

This first version is a high-performance visualization core and basic Android player shell. It supports standard PPQ SMF files, note-on/note-off, running status, tempo meta events and arbitrary track counts. SMPTE time division and MIDI audio synthesis are not implemented yet.

## Native core test

```sh
cmake -S native-core -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j
./build/native/kivapfa_core_test
```

## Android build

Open the repository in Android Studio with Android SDK 35 + NDK installed, then build the `app` module.

## Performance knobs

- page size: 4096 notes
- Android resident page cache: 48 MiB
- visible hard cap per frame: 200,000 notes
- look-ahead: 4 seconds
- ABI: arm64-v8a and x86_64

The hard cap is a frame-safety valve, not a parser limit. Raise it only when the GPU can sustain the density.

### Frame path

The renderer reuses one 3.2 MiB direct `ByteBuffer` for up to 200,000 visible notes. JNI writes into that buffer in place and GLES uploads from the same reusable buffer, so there is **no per-frame `FloatArray`, per-note object allocation, or per-note draw call** on the Java/Kotlin side.
