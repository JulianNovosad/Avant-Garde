// Android native entry for Blacknode (Raylib + libjpeg-turbo)
#include <android_native_app_glue.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include "network/UdpMjpegReceiver.h"
#include "render/Renderer.h"

using namespace std::chrono_literals;

static std::atomic<bool> appRunning{true};

// Simple double-tap detector
struct TapTracker {
    std::chrono::steady_clock::time_point lastTap = std::chrono::steady_clock::now() - 1s;
    bool checkDoubleTap(){
        auto now = std::chrono::steady_clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTap).count();
        lastTap = now;
        return dt < 400; // double-tap if within 400ms
    }
} tapTracker;

void android_main(struct android_app* app){
    // Attach binder/looper (required by android_native_app_glue)
    app_dummy();

    // Initialize Raylib - on Android this will use the native activity window
    const int screenW = 1280;
    const int screenH = 720;

    // Initialize subsystems
    Renderer renderer;
    renderer.init(screenW, screenH);

    UdpMjpegReceiver receiver;
    receiver.start(50000); // VIDEO_PORT per spec

    enum AppMode { MODE_COCKPIT=0, MODE_CONSTRUCT=1 } mode = MODE_COCKPIT;
    auto lastFrame = std::chrono::steady_clock::now();

    // Simple main loop
    while (appRunning){
        // Process platform events
        int ident;
        int events;
        struct android_poll_source* source;
        while ((ident = ALooper_pollAll(0, nullptr, &events, (void**)&source)) >= 0){
            if (source) source->process(app, source);
            if (app->destroyRequested) appRunning = false;
        }

        // Poll for new MJPEG frame
        std::vector<uint8_t> jpeg = receiver.getLatestFrame();
        if (!jpeg.empty()){
            renderer.uploadJpegFrame(jpeg.data(), (int)jpeg.size());
        }

        // Frame timing
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastFrame).count();
        lastFrame = now;

        renderer.update(dt);

        // Input: detect a double-tap using raylib touch (if available)
        if (IsGestureDetected(GESTURE_TAP)){
            if (tapTracker.checkDoubleTap()){
                if (mode==MODE_COCKPIT) mode = MODE_CONSTRUCT; else mode = MODE_COCKPIT;
                if (mode==MODE_CONSTRUCT) renderer.startConstructSequence();
            }
        }

        renderer.render(mode==MODE_COCKPIT);

        // Sleep briefly to yield
        std::this_thread::sleep_for(8ms);
    }

    receiver.stop();
    renderer.shutdown();
}
