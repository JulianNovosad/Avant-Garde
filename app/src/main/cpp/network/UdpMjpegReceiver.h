#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <mutex>

class UdpMjpegReceiver {
public:
    UdpMjpegReceiver();
    ~UdpMjpegReceiver();
    bool start(int port);
    void stop();
    // Returns a copy of the latest complete JPEG frame (may be empty)
    std::vector<uint8_t> getLatestFrame();

private:
    void runLoop(int port);
    std::atomic<bool> running{false};
    std::thread worker;
    std::mutex frameMutex;
    std::vector<uint8_t> latestFrame;
};
