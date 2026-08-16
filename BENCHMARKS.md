# Native core benchmark (reference only)

These numbers were measured in the build/test container, not on an Android phone. They validate algorithmic behavior; they are **not** a promise of identical device FPS.

Release build (`-O3`), synthetic Standard MIDI Files:

- 1,000,000 notes: index build ~105 ms in the measured run.
- 600 sequential render-window queries over the 1,000,000-note file: ~35.75 ms total, ~0.060 ms/query in the measured run.
- 200,000 simultaneously visible notes: native visible-note selection ~3.67 ms in the measured run.

The Android GPU upload/draw cost is device-dependent. The renderer therefore caps visible instances at 200,000 per frame as a safety valve.
