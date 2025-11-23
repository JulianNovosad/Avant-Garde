#include "NetworkManager.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <rapidjson/document.h>
#include <iostream>

#include "Network/NetworkPorts.h"
using namespace blacknode;

NetworkManager::NetworkManager() {
    framesPool.resize(4);
}

NetworkManager::~NetworkManager(){ stop(); }

void NetworkManager::setFrameCallback(std::function<void(uint32_t,const std::vector<uint8_t>&)> cb){ frameCb = cb; }
void NetworkManager::setTelemetryCallback(std::function<void(int,const std::string&)> cb){ telemetryCb = cb; }

static int makeUdpSocket(uint16_t port, int rcvbuf=4*1024*1024) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return -1;
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(s, (sockaddr*)&addr, sizeof(addr))<0){ close(s); return -1; }
    return s;
}

void NetworkManager::start(){
    if (running.load()) return;
    running.store(true);
    videoThread = std::thread(&NetworkManager::videoThreadMain, this);
    telemetryThread = std::thread(&NetworkManager::telemetryThreadMain, this);
}

void NetworkManager::stop(){
    if (!running.load()) return;
    running.store(false);
    if (videoThread.joinable()) videoThread.join();
    if (telemetryThread.joinable()) telemetryThread.join();
}

void NetworkManager::videoThreadMain(){
    int sock = makeUdpSocket(VIDEO_PORT);
    if (sock < 0){ std::cerr<<"Video socket failed"<<"\n"; return; }

    constexpr size_t MAX_PKT = 1500;
    std::vector<uint8_t> buf(MAX_PKT);

    while (running.load()){
        sockaddr_in src{}; socklen_t sl = sizeof(src);
        ssize_t r = recvfrom(sock, buf.data(), buf.size(), 0, (sockaddr*)&src, &sl);
        if (r <= 0){ std::this_thread::sleep_for(std::chrono::milliseconds(2)); continue; }
        if ((size_t)r < sizeof(PacketHeader)) continue;

        PacketHeader hdr{};
        memcpy(&hdr, buf.data(), sizeof(hdr));
        PacketHeader_ntoh(hdr);
        if (hdr.magic != 0x4D4A5047u) continue; // "MJPG"

        size_t payload_len = hdr.payload_len;
        size_t header_sz = sizeof(PacketHeader);
        if (header_sz + payload_len > (size_t)r) continue;

        std::lock_guard<std::mutex> lk(framesMutex);
        // find or allocate frame buffer
        FrameBuffer* fb = nullptr;
        for (auto &f: framesPool){ if (f.frame_seq == hdr.frame_seq) { fb = &f; break; } }
        if (!fb){
            for (auto &f: framesPool){ if (f.received==0){ fb = &f; break; } }
        }
        if (!fb) {
            // evict oldest
            auto oldest = std::min_element(framesPool.begin(), framesPool.end(), [](const FrameBuffer&a,const FrameBuffer&b){ return a.first_recv < b.first_recv; });
            fb = &*oldest;
            fb->data.clear(); fb->seen.clear(); fb->received=0; fb->complete=false;
        }

        if (fb->received==0){
            fb->frame_seq = hdr.frame_seq;
            fb->total_packets = hdr.total_packets;
            fb->data.resize((size_t)hdr.total_packets * 1300);
            fb->seen.assign(hdr.total_packets, 0);
            fb->first_recv = std::chrono::steady_clock::now();
            fb->crc32 = hdr.crc32;
        }

        if (hdr.packet_seq < fb->total_packets){
            size_t offset = hdr.packet_seq * 1300;
            memcpy(fb->data.data()+offset, buf.data()+sizeof(PacketHeader), payload_len);
            if (!fb->seen[hdr.packet_seq]){ fb->seen[hdr.packet_seq]=1; fb->received++; }
        }

        if (fb->received == fb->total_packets){
            // assemble exact size
            size_t exact_size = (fb->total_packets-1)*1300 + (size_t)hdr.payload_len;
            std::vector<uint8_t> jpeg(exact_size);
            memcpy(jpeg.data(), fb->data.data(), exact_size);
            uint32_t crc = crc32_compute(jpeg.data(), jpeg.size());
            if (crc != fb->crc32) {
                // CRC mismatch -> drop
                fb->received = 0; fb->seen.assign(fb->total_packets,0);
            } else {
                if (frameCb) frameCb(fb->frame_seq, jpeg);
                fb->received = 0; fb->seen.assign(fb->total_packets,0);
                fb->complete = true;
            }
        }

        // timeout cleanup
        for (auto &f: framesPool){ if (f.received>0){
            auto dt = std::chrono::steady_clock::now() - f.first_recv;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() > 200){ f.received=0; f.seen.assign(f.total_packets,0); }
        }}
    }
    close(sock);
}

void NetworkManager::telemetryThreadMain(){
    int sock = makeUdpSocket(TELEMETRY_BASE);
    if (sock < 0){ std::cerr<<"Telemetry socket failed"<<"\n"; return; }
    constexpr size_t MAX_PKT=4096;
    std::vector<uint8_t> buf(MAX_PKT);
    while (running.load()){
        sockaddr_in src{}; socklen_t sl = sizeof(src);
        ssize_t r = recvfrom(sock, buf.data(), buf.size(), 0, (sockaddr*)&src, &sl);
        if (r <= 0){ std::this_thread::sleep_for(std::chrono::milliseconds(2)); continue; }
        int port = ntohs(src.sin_port);
        std::string json((char*)buf.data(), r);
        // Try to parse quickly
        rapidjson::Document d;
        if (d.Parse(json.c_str()).HasParseError()) continue;
        if (telemetryCb) telemetryCb(port, json);
    }
    close(sock);
}
