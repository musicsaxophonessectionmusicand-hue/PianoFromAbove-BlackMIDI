package com.kivapfa.player

import java.nio.ByteBuffer

object NativeEngine {
    init { System.loadLibrary("kivapfa") }
    external fun create(midiPath: String, pagePath: String): Long
    external fun destroy(handle: Long)
    external fun durationUs(handle: Long): Long
    external fun noteCount(handle: Long): Long
    external fun seek(handle: Long, timeUs: Long)
    external fun collectInto(handle: Long, timeUs: Long, lookAheadUs: Long, maxNotes: Int, target: ByteBuffer): Int
}
