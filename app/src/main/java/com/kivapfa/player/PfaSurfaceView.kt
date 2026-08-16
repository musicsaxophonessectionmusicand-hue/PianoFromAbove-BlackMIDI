package com.kivapfa.player

import android.content.Context
import android.opengl.GLES30
import android.opengl.GLSurfaceView
import android.os.SystemClock
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class PfaSurfaceView(context: Context) : GLSurfaceView(context), GLSurfaceView.Renderer {
    companion object { private const val MAX_NOTES = 200_000 }
    @Volatile var engine: Long = 0
    @Volatile var playing = false
    @Volatile var speed = 1.0
    @Volatile var lookAheadUs = 4_000_000L
    private var timeUs = 0L
    private var lastNs = 0L
    private var program = 0
    private var vao = 0
    private var quadVbo = 0
    private var instanceVbo = 0
    private var uKeyRange = -1
    private val instanceBytes: ByteBuffer = ByteBuffer.allocateDirect(MAX_NOTES * 16).order(ByteOrder.nativeOrder())
    private val instanceFloats: FloatBuffer = instanceBytes.asFloatBuffer()

    init { setEGLContextClientVersion(3); setRenderer(this); renderMode = RENDERMODE_CONTINUOUSLY }

    fun seekTo(us: Long) { timeUs = us; val h = engine; if (h != 0L) NativeEngine.seek(h, us); lastNs = 0 }
    fun currentUs() = timeUs

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        GLES30.glClearColor(0f,0f,0f,1f)
        val vs = """#version 300 es
            layout(location=0) in vec2 aQuad;
            layout(location=1) in vec4 iNote;
            out float vVel;
            uniform vec2 uKeyRange;
            void main(){
              float x0=(iNote.z-uKeyRange.x)/(uKeyRange.y-uKeyRange.x);
              float keyW=1.0/(uKeyRange.y-uKeyRange.x);
              float x=(x0+aQuad.x*keyW)*2.0-1.0;
              float y0=-1.0 + iNote.x*1.82;
              float y1=-1.0 + iNote.y*1.82;
              gl_Position=vec4(x,mix(y0,y1,aQuad.y),0,1); vVel=iNote.w;
            }""".trimIndent()
        val fs = """#version 300 es
            precision mediump float; in float vVel; out vec4 o;
            void main(){ float b=0.35+0.65*vVel; o=vec4(b,b,b,1.0); }""".trimIndent()
        program = link(vs, fs); uKeyRange = GLES30.glGetUniformLocation(program, "uKeyRange")
        val ids = IntArray(1); GLES30.glGenVertexArrays(1, ids, 0); vao = ids[0]; GLES30.glBindVertexArray(vao)
        GLES30.glGenBuffers(1, ids, 0); quadVbo = ids[0]; GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, quadVbo)
        val quad = floatBuffer(floatArrayOf(0f,0f, 1f,0f, 0f,1f, 0f,1f, 1f,0f, 1f,1f))
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, quad.capacity()*4, quad, GLES30.GL_STATIC_DRAW)
        GLES30.glEnableVertexAttribArray(0); GLES30.glVertexAttribPointer(0,2,GLES30.GL_FLOAT,false,8,0)
        GLES30.glGenBuffers(1, ids, 0); instanceVbo=ids[0]; GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, instanceVbo)
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, MAX_NOTES*16, null, GLES30.GL_STREAM_DRAW)
        GLES30.glEnableVertexAttribArray(1); GLES30.glVertexAttribPointer(1,4,GLES30.GL_FLOAT,false,16,0); GLES30.glVertexAttribDivisor(1,1)
        GLES30.glBindVertexArray(0)
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) { GLES30.glViewport(0,0,width,height) }

    override fun onDrawFrame(gl: GL10?) {
        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT)
        val h = engine; if (h == 0L) return
        val now = SystemClock.elapsedRealtimeNanos(); if (playing && lastNs != 0L) timeUs += (((now-lastNs)/1000.0)*speed).toLong(); lastNs = now
        val duration = NativeEngine.durationUs(h); if (timeUs > duration) { timeUs = duration; playing=false }
        instanceBytes.position(0)
        val count = NativeEngine.collectInto(h, timeUs, lookAheadUs, MAX_NOTES, instanceBytes)
        if (count <= 0) return
        instanceFloats.position(0); instanceFloats.limit(count*4)
        GLES30.glUseProgram(program); GLES30.glUniform2f(uKeyRange, 21f,109f); GLES30.glBindVertexArray(vao); GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, instanceVbo)
        GLES30.glBufferSubData(GLES30.GL_ARRAY_BUFFER, 0, count*16, instanceFloats)
        GLES30.glDrawArraysInstanced(GLES30.GL_TRIANGLES,0,6,count); GLES30.glBindVertexArray(0)
    }

    private fun floatBuffer(a: FloatArray): FloatBuffer = ByteBuffer.allocateDirect(a.size*4).order(ByteOrder.nativeOrder()).asFloatBuffer().apply{ put(a); position(0) }
    private fun shader(type:Int, src:String):Int { val s=GLES30.glCreateShader(type); GLES30.glShaderSource(s,src); GLES30.glCompileShader(s); return s }
    private fun link(v:String,f:String):Int { val p=GLES30.glCreateProgram(); val vs=shader(GLES30.GL_VERTEX_SHADER,v); val fs=shader(GLES30.GL_FRAGMENT_SHADER,f); GLES30.glAttachShader(p,vs); GLES30.glAttachShader(p,fs); GLES30.glLinkProgram(p); GLES30.glDeleteShader(vs); GLES30.glDeleteShader(fs); return p }
}
