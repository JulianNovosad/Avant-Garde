#pragma once

#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <string>
#include <condition_variable>
#include <unordered_map>
#include <cstdint>
#include <chrono>
#include <functional>
#include "PacketDefs.h"

struct FrameBuffer {
    uint32_t frame_seq = 0;
    uint16_t total_packets = 0;
    uint16_t received = 0;
    std::vector<uint8_t> data; // reassembled jpeg
    std::vector<uint16_t> seen; // per-packet seen flags
    std::chrono::steady_clock::time_point first_recv;
    uint32_t crc32 = 0;
    bool complete = false;
};

struct TelemetryState {
    // Double-buffered simple structure
    std::atomic<uint64_t> seq{0};
    std::string last_json;
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    void start();
    void stop();

    // Callback when a full JPEG frame is ready (frame_seq, jpeg_data)
    void setFrameCallback(std::function<void(uint32_t,const std::vector<uint8_t>&)> cb);

    // Callback when telemetry JSON arrives (port, json)
    void setTelemetryCallback(std::function<void(int,const std::string&)> cb);

private:
    void videoThreadMain();
    void telemetryThreadMain();

    std::thread videoThread;
    std::thread telemetryThread;
    std::atomic<bool> running{false};

    std::mutex framesMutex;
    std::vector<FrameBuffer> framesPool; // max 4 in-flight

    std::function<void(uint32_t,const std::vector<uint8_t>&)> frameCb;
    std::function<void(int,const std::string&)> telemetryCb;
};
