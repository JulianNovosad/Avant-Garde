#include <jni.h>
#include <string>
#include "../core/NativeBridge.h"

static JavaVM* g_vm = nullptr;

jint JNI_OnLoad(JavaVM* vm, void* reserved){
    g_vm = vm;
    core::NativeBridge::init();
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL Java_com_blacknode_BlacknodeBridge_nativeInit(JNIEnv* env, jclass cls){
    // No-op for now (bridge init already in JNI_OnLoad)
}

extern "C" JNIEXPORT void JNICALL Java_com_blacknode_BlacknodeBridge_nativeOnPermissionResult(JNIEnv* env, jclass, jboolean granted){
    core::NativeBridge::onPermissionResult(granted == JNI_TRUE);
}
