#pragma once
#include <atomic>
#include <chrono>

class BackpressureDetector {
public:
    BackpressureDetector();
    // call when a telemetry packet arrives
    void notifyPacket();
    // call per-frame to decay counters
    void update();
    // 0.0..1.0 intensity
    float intensity() const { return level.load(); }
private:
    std::atomic<int> count{0};
    std::atomic<float> level{0.0f};
    std::chrono::steady_clock::time_point lastDecay;
};
