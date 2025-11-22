#pragma once
#include <functional>
#include <thread>
#include <atomic>
#include <cstdint>
#include <vector>

struct TelemetryPacket {
    uint32_t magic;        // 0xC0A1FACE
    uint64_t timestamp;    // nanos
    float position[3];     // x, y, z
    float confidence;      // 0.0-1.0
    uint8_t state;         // 0=offline,1=active,2=degraded,3=fallback
    uint8_t reserved[3];
};

class UdpReceiver {
public:
    using Callback = std::function<void(const TelemetryPacket&)>;
    UdpReceiver();
    ~UdpReceiver();
    bool start(uint16_t port = 40321);
    void stop();
    void setCallback(Callback cb);

private:
    void run();
    std::thread thr;
    std::atomic<bool> running{false};
    Callback cb;
    int sockfd{-1};
};
