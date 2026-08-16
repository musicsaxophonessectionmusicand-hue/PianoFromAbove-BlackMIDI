package com.kivapfa.player

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import java.io.File

class MainActivity : Activity() {
    private lateinit var surface: PfaSurfaceView
    private lateinit var status: TextView
    private var handle = 0L

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        surface = PfaSurfaceView(this)
        val root = FrameLayout(this)
        root.addView(surface, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        val bar = LinearLayout(this).apply { orientation=LinearLayout.HORIZONTAL; gravity=Gravity.CENTER_VERTICAL; setPadding(12,6,12,6); setBackgroundColor(0x66000000) }
        fun button(text:String, click:()->Unit)=Button(this).apply{ this.text=text; setOnClickListener{click()} }
        bar.addView(button("OPEN"){ openMidi() })
        bar.addView(button("PLAY/PAUSE"){ surface.playing=!surface.playing })
        bar.addView(button("-5s"){ surface.seekTo((surface.currentUs()-5_000_000L).coerceAtLeast(0)) })
        bar.addView(button("+5s"){ surface.seekTo(surface.currentUs()+5_000_000L) })
        status = TextView(this).apply { setTextColor(0xffffffff.toInt()); text="No MIDI"; setPadding(16,0,0,0) }
        bar.addView(status)
        root.addView(bar, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP))
        setContentView(root)
    }

    private fun openMidi(){ startActivityForResult(Intent(Intent.ACTION_OPEN_DOCUMENT).apply{ addCategory(Intent.CATEGORY_OPENABLE); type="audio/midi"; putExtra(Intent.EXTRA_MIME_TYPES,arrayOf("audio/midi","audio/x-midi","application/octet-stream")) }, 42) }

    override fun onActivityResult(requestCode:Int,resultCode:Int,data:Intent?){
        super.onActivityResult(requestCode,resultCode,data)
        if(requestCode!=42||resultCode!=RESULT_OK) return
        val uri=data?.data?:return
        status.text="Indexing…"
        Thread{
            try{
                val midi=copyToCache(uri)
                val page=File(cacheDir,"${midi.name}.kpfa.page").absolutePath
                if(handle!=0L) NativeEngine.destroy(handle)
                handle=NativeEngine.create(midi.absolutePath,page)
                surface.engine=handle; surface.seekTo(0)
                runOnUiThread{ status.text="${midi.name} • ${NativeEngine.noteCount(handle)} notes" }
            }catch(t:Throwable){ runOnUiThread{ status.text="Load error: ${t.message}" } }
        }.start()
    }

    private fun copyToCache(uri:Uri):File{
        var name="input.mid"
        contentResolver.query(uri,null,null,null,null)?.use{ c-> val i=c.getColumnIndex(OpenableColumns.DISPLAY_NAME); if(c.moveToFirst()&&i>=0) name=c.getString(i) }
        val out=File(cacheDir,name.replace(Regex("[^A-Za-z0-9._-]"),"_"))
        contentResolver.openInputStream(uri)!!.use{ input-> out.outputStream().use{ input.copyTo(it,1024*1024) } }
        return out
    }

    override fun onDestroy(){ if(handle!=0L) NativeEngine.destroy(handle); super.onDestroy() }
}
