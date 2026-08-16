#include <jni.h>
#include <memory>
#include <string>
#include "kivapfa/MidiParser.h"
#include "kivapfa/PagedNoteStore.h"
#include "kivapfa/PlaybackCursor.h"

using namespace kivapfa;
struct Engine { ParsedIndex idx; std::unique_ptr<PagedNoteStore> store; std::unique_ptr<PlaybackCursor> cursor; };
static std::string jstr(JNIEnv* e,jstring s){ const char* p=e->GetStringUTFChars(s,nullptr); std::string v(p); e->ReleaseStringUTFChars(s,p); return v; }
static Engine* E(jlong h){ return reinterpret_cast<Engine*>(h); }

extern "C" JNIEXPORT jlong JNICALL Java_com_kivapfa_player_NativeEngine_create(JNIEnv* env,jobject,jstring jm,jstring jp){
    try{
        auto e=std::make_unique<Engine>(); const auto midi=jstr(env,jm), page=jstr(env,jp);
        e->idx=MidiParser::buildPagedIndex(midi,page);
        e->store=std::make_unique<PagedNoteStore>(page,e->idx,4096,48u*1024u*1024u);
        e->cursor=std::make_unique<PlaybackCursor>(*e->store);
        return reinterpret_cast<jlong>(e.release());
    } catch(const std::exception& ex){ jclass c=env->FindClass("java/lang/RuntimeException"); env->ThrowNew(c,ex.what()); return 0; }
}
extern "C" JNIEXPORT void JNICALL Java_com_kivapfa_player_NativeEngine_destroy(JNIEnv*,jobject,jlong h){ delete E(h); }
extern "C" JNIEXPORT jlong JNICALL Java_com_kivapfa_player_NativeEngine_durationUs(JNIEnv*,jobject,jlong h){ return E(h)->idx.duration_us; }
extern "C" JNIEXPORT jlong JNICALL Java_com_kivapfa_player_NativeEngine_noteCount(JNIEnv*,jobject,jlong h){ return E(h)->idx.total_notes; }
extern "C" JNIEXPORT void JNICALL Java_com_kivapfa_player_NativeEngine_seek(JNIEnv*,jobject,jlong h,jlong t){ E(h)->cursor->seek(t); }
extern "C" JNIEXPORT jint JNICALL Java_com_kivapfa_player_NativeEngine_collectInto(JNIEnv* env,jobject,jlong h,jlong t,jlong ahead,jint cap,jobject target){
    if(cap<=0) return 0;
    auto* dst=static_cast<float*>(env->GetDirectBufferAddress(target));
    const auto bytes=env->GetDirectBufferCapacity(target);
    if(!dst || bytes < static_cast<jlong>(cap)*16) { jclass c=env->FindClass("java/lang/IllegalArgumentException"); env->ThrowNew(c,"target must be a direct ByteBuffer sized maxNotes*16"); return 0; }
    return static_cast<jint>(E(h)->cursor->collectPacked4(t,ahead,dst,static_cast<std::size_t>(cap)));
}
