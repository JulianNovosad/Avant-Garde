// android_main entrypoint using native_app_glue
#include <android_native_app_glue.h>
#include <android/log.h>
#include <jni.h>
#include <unistd.h>
#include <thread>
#include "Network/UdpReceiver.cpp"
#include "Video/Decoder.cpp"
#include "Render/Scene.cpp"

static void handle_cmd(struct android_app* app, int32_t cmd){
    // handle lifecycle events if needed
}

void android_main(struct android_app* state){
    app_dummy();
    __android_log_print(ANDROID_LOG_INFO, "blacknode", "android_main start");
    state->onAppCmd = handle_cmd;

    // permission request via JNI (call Java PermissionsBridge.requestPermissions)
    JNIEnv* env = nullptr;
    JavaVM* vm = state->activity->vm;
    vm->AttachCurrentThread(&env, nullptr);
    jclass cls = env->FindClass("com/avantgarde/blacknode/PermissionsBridge");
    if(cls){
        jmethodID mid = env->GetStaticMethodID(cls, "requestPermissions", "(Landroid/app/Activity;[Ljava/lang/String;I)V");
        if(mid){
            jobject activity = state->activity->clazz;
            jclass stringClass = env->FindClass("java/lang/String");
            jobjectArray perms = env->NewObjectArray(2, stringClass, nullptr);
            env->SetObjectArrayElement(perms, 0, env->NewStringUTF("android.permission.INTERNET"));
            env->SetObjectArrayElement(perms, 1, env->NewStringUTF("android.permission.ACCESS_NETWORK_STATE"));
            env->CallStaticVoidMethod(cls, mid, activity, perms, 1234);
        }
    }

    // Scene
    Scene scene;
    // get display size placeholder
    scene.init(1280,720);

    // Network receiver: pass decoded frames to decoder
    Decoder decoder;
    net::UdpReceiver receiver([&](const std::vector<uint8_t>& frame){
        decoder.decodeToTexture(frame);
        // texture can be sampled in render loop if integrated
    });
    receiver.start();

    // main loop (simple)
    auto last = std::chrono::steady_clock::now();
    while(true){
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        scene.update(dt);
        scene.render();
        // Simple exit condition if app is destroyed
        if(0){ break; }
    }

    receiver.stop();
    scene.shutdown();
    vm->DetachCurrentThread();
}
#include "NetworkManager.h"
#include "VideoDec.h"
#include "Renderer.h"
#include <jni.h>
#include <android/native_activity.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <thread>
#include <atomic>
#include <iostream>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Blacknode", __VA_ARGS__)

static NetworkManager *gNet = nullptr;
static VideoDec *gDec = nullptr;
static Renderer *gRend = nullptr;
static std::atomic<bool> gRunning{false};

extern "C" void android_main(struct android_app* state){
    app_dummy();
    LOGI("Blacknode native start");

    // initialize components
    gNet = new NetworkManager();
    gDec = new VideoDec();
    gRend = new Renderer();

    gNet->setFrameCallback([&](uint32_t seq, const std::vector<uint8_t>& jpeg){
        int w,h; std::vector<uint8_t> rgb;
        if (!gDec->decodeJPEG(jpeg, w, h, rgb)) return;
        // create texture (simple path) and upload
        // For simplicity, use raylib texture create via Image
        // In production use GL textures + PBOs
        // We'll upload via VideoDec's uploadToTexture if GL texture available
    });

    gNet->start();

    // Desktop simulation guard
#ifdef PLATFORM_DESKTOP
    gRend->init(1280,720);
    gRunning.store(true);
    while (!WindowShouldClose()){
        gRend->renderFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    gRunning.store(false);
    gRend->shutdown();
#else
    // Android main loop: wait for lifecycle events and render when window available
    // Minimal loop — detailed lifecycle handling omitted for clarity
    gRend->init(1280,720);
    gRunning.store(true);
    while (gRunning.load()){
        gRend->renderFrame();
        // check for app events
        int ident;
        int events;
        struct android_poll_source* source;
        while ((ident = ALooper_pollAll(0, NULL, &events, (void**)&source)) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) gRunning.store(false);
        }
    }
    gRend->shutdown();
#endif

    gNet->stop();
    delete gNet; gNet=nullptr;
    delete gDec; gDec=nullptr;
    delete gRend; gRend=nullptr;
    LOGI("Blacknode native exit");
}
