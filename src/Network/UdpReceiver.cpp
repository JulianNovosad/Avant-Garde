#include "../PacketDefs.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <iostream>
#include <chrono>
#include <functional>
#include <zlib.h> // for crc32
#include "rapidjson/document.h"
#include "Network/NetworkPorts.h"

namespace net {

// Pre-allocated pool for frames
struct FrameBuffer {
    uint32_t frame_seq;
    uint16_t total_packets;
    std::vector<std::vector<uint8_t>> chunks; // indexed by packet_seq
    std::vector<uint16_t> chunk_lens;
    std::chrono::steady_clock::time_point first_seen;
    FrameBuffer(): frame_seq(0), total_packets(0){}
};

class UdpReceiver {
public:
    using FrameCallback = std::function<void(const std::vector<uint8_t>&)>;

    UdpReceiver(FrameCallback cb): cb(cb), running(false) {}

    ~UdpReceiver(){ stop(); }

    bool start(uint16_t portVideo=blacknode::VIDEO_PORT, uint16_t portTelemetryStart=blacknode::TELEMETRY_BASE){
        running = true;
        // start video socket
        videoSock = socket(AF_INET, SOCK_DGRAM, 0);
        if(videoSock < 0) return false;
        int flags = fcntl(videoSock, F_GETFL, 0);
        fcntl(videoSock, F_SETFL, flags | O_NONBLOCK);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(portVideo);
        if(bind(videoSock, (sockaddr*)&addr, sizeof(addr))<0){
            perror("bind"); close(videoSock); return false;
        }

        // telemetry socket listens on a range
        telemetrySock = socket(AF_INET, SOCK_DGRAM, 0);
        if(telemetrySock < 0) return false;
        fcntl(telemetrySock, F_SETFL, O_NONBLOCK);
        sockaddr_in taddr{};
        taddr.sin_family = AF_INET; taddr.sin_addr.s_addr = INADDR_ANY; taddr.sin_port = htons(portTelemetryStart);
        if(bind(telemetrySock, (sockaddr*)&taddr, sizeof(taddr))<0){ perror("bind telemetry"); }

        worker = std::thread(&UdpReceiver::runLoop, this);
        return true;
    }

    void stop(){
        running = false;
        if(worker.joinable()) worker.join();
        if(videoSock>0) close(videoSock);
        if(telemetrySock>0) close(telemetrySock);
    }

private:
    int videoSock=-1;
    int telemetrySock=-1;
    std::thread worker;
    std::atomic<bool> running;
    std::mutex lock;
    std::unordered_map<uint32_t, FrameBuffer> ring; // keyed by frame_seq
    FrameCallback cb;

    void runLoop(){
        constexpr size_t MAXPKT = 1400;
        uint8_t buf[MAXPKT];
        while(running){
            // video
            ssize_t r = recv(videoSock, buf, sizeof(buf), 0);
            if(r>0){
                processVideoPacket(buf, (size_t)r);
            }
            // telemetry
            ssize_t t = recv(telemetrySock, buf, sizeof(buf), 0);
            if(t>0){
                processTelemetry((const char*)buf, (size_t)t);
            }
            // cleanup old frames
            cleanupOldFrames();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void processTelemetry(const char* data, size_t len){
        rapidjson::Document d;
        d.Parse(data, len);
        if(d.HasParseError()) return;
        if(d.HasMember("type") && d["type"].IsString()){
            std::string t = d["type"].GetString();
            // handle simple telemetry types here (application-specific)
            // We just print for now
            // e.g., bbox
            if(t=="bbox"){
                // process bounding boxes
            }
        }
    }

    void processVideoPacket(const uint8_t* data, size_t len){
        if(len < sizeof(FramePacketHeader)) return;
        // parse big-endian header safely
        uint32_t magic = be32(data);
        const uint8_t* p = data;
        uint32_t frame_seq = be32(p+4);
        uint16_t packet_seq = be16(p+8);
        uint16_t total_packets = be16(p+10);
        uint16_t payload_len = be16(p+12);
        uint32_t crc = be32(p+14);
        if(payload_len + sizeof(FramePacketHeader) > len) return; // invalid
        const uint8_t* payload = data + sizeof(FramePacketHeader);

        std::lock_guard<std::mutex> g(lock);
        auto &fb = ring[frame_seq];
        if(fb.chunks.empty()){
            fb.frame_seq = frame_seq;
            fb.total_packets = total_packets;
            fb.chunks.resize(total_packets);
            fb.chunk_lens.resize(total_packets);
            fb.first_seen = std::chrono::steady_clock::now();
        }
        if(packet_seq < fb.chunks.size()){
            fb.chunks[packet_seq].assign(payload, payload+payload_len);
            fb.chunk_lens[packet_seq] = payload_len;
        }

        // check if complete
        bool complete = true;
        for(size_t i=0;i<fb.total_packets;++i){ if(fb.chunks[i].empty()){ complete=false; break; } }
        if(complete){
            // assemble
            size_t totalSize=0; for(size_t i=0;i<fb.total_packets;++i) totalSize += fb.chunk_lens[i];
            std::vector<uint8_t> frame; frame.reserve(totalSize);
            for(size_t i=0;i<fb.total_packets;++i){ frame.insert(frame.end(), fb.chunks[i].begin(), fb.chunks[i].end()); }
            // verify crc32
            uint32_t computed = (uint32_t)crc32(0, frame.data(), (uInt)frame.size());
            if(computed == crc){
                // deliver
                if(cb) cb(frame);
            }
            // erase
            ring.erase(frame_seq);
        }
    }

    void cleanupOldFrames(){
        std::vector<uint32_t> toDrop;
        auto now = std::chrono::steady_clock::now();
        for(auto &kv : ring){
            auto &fb = kv.second;
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - fb.first_seen).count();
            if(age > 200){ toDrop.push_back(kv.first); }
        }
        for(auto k: toDrop) ring.erase(k);
    }
};

} // namespace net
