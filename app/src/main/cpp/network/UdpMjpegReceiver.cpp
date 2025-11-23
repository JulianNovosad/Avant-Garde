#include "UdpMjpegReceiver.h"
// Full MJPEG reassembly implementation
#include "UdpMjpegReceiver.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <chrono>

// Packet header layout (network byte order on the wire)
struct PacketHeader {
    uint32_t magic;       // 'MJPG'
    uint32_t frame_seq;   // sequence id for frame
    uint32_t packet_seq;  // sequence within frame
    uint32_t total_packets;// total packets for this frame
    uint32_t payload_len; // payload length for this packet
    uint32_t crc32;       // CRC32 of whole frame (network order)
};

// Simple CRC32 implementation
static uint32_t crc32_table[256];
static bool crc32_table_inited = false;
static void init_crc32(){
    if (crc32_table_inited) return;
    const uint32_t POLY = 0xEDB88320u;
    for (uint32_t i=0;i<256;i++){
        uint32_t c = i;
        for (int j=0;j<8;j++) c = (c & 1) ? (POLY ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc32_table_inited = true;
}
static uint32_t crc32(const uint8_t* data, size_t len){
    init_crc32();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i=0;i<len;i++) c = crc32_table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

// Reassembly pool size
static const size_t POOL_SIZE = 32;
static const size_t MAX_FRAME_BYTES = 6 * 1024 * 1024; // 6MB max
static const size_t FRAG_PAYLOAD_CAP = 1400; // standard UDP payload cap

struct FrameSlot {
    uint32_t frame_seq = 0;
    uint32_t total_packets = 0;
    std::vector<uint8_t> buffer; // preallocated to total_packets * FRAG_PAYLOAD_CAP
    std::vector<uint32_t> payloadLens; // length per packet
    std::vector<uint8_t> receivedMask; // 0/1 per packet
    uint32_t received_count = 0;
    std::chrono::steady_clock::time_point start;
    uint32_t expected_crc = 0;
    bool ready = false;
};

UdpMjpegReceiver::UdpMjpegReceiver(){ }
UdpMjpegReceiver::~UdpMjpegReceiver(){ stop(); }

bool UdpMjpegReceiver::start(int port){
    if (running) return false;
    running = true;
    worker = std::thread(&UdpMjpegReceiver::runLoop, this, port);
    return true;
}

void UdpMjpegReceiver::stop(){
    if (!running) return;
    running = false;
    if (worker.joinable()) worker.join();
}

std::vector<uint8_t> UdpMjpegReceiver::getLatestFrame(){
    std::lock_guard<std::mutex> lk(frameMutex);
    return latestFrame;
}

void UdpMjpegReceiver::runLoop(int port){
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0){ perror("socket"); running = false; return; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0){ perror("bind"); close(sock); running=false; return; }

    std::vector<FrameSlot> pool(POOL_SIZE);
    struct sockaddr_in srcAddr; socklen_t srcLen = sizeof(srcAddr);
    std::vector<uint8_t> buf(65536);

    while (running){
        // non-blocking poll for packet with small timeout
        ssize_t r = recvfrom(sock, buf.data(), (size_t)buf.size(), 0, (struct sockaddr*)&srcAddr, &srcLen);
        auto now = std::chrono::steady_clock::now();
        if (r <= 0){
            // clean up stale frames
            for (auto &slot : pool){
                if (slot.received_count>0 && !slot.ready){
                    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - slot.start).count();
                    if (age > 200){
                        // timeout
                        slot = FrameSlot();
                    }
                }
            }
            // yield
            usleep(1000);
            continue;
        }

        if ((size_t)r < sizeof(PacketHeader)) continue;
        PacketHeader hdrNet;
        memcpy(&hdrNet, buf.data(), sizeof(PacketHeader));
        PacketHeader hdr;
        hdr.magic = ntohl(hdrNet.magic);
        hdr.frame_seq = ntohl(hdrNet.frame_seq);
        hdr.packet_seq = ntohl(hdrNet.packet_seq);
        hdr.total_packets = ntohl(hdrNet.total_packets);
        hdr.payload_len = ntohl(hdrNet.payload_len);
        hdr.crc32 = ntohl(hdrNet.crc32);

        if (hdr.magic != 0x4D4A5047u){ // 'MJPG' big-endian check
            continue;
        }

        if (hdr.total_packets == 0 || hdr.payload_len == 0) continue;
        if (hdr.total_packets > 10000) continue; // sanity
        if (hdr.payload_len > 65535) continue;

        size_t slotIdx = hdr.frame_seq % POOL_SIZE;
        FrameSlot &slot = pool[slotIdx];

        // If slot contains different frame and is still occupied, reset it
        if (slot.received_count>0 && slot.frame_seq != hdr.frame_seq){
            slot = FrameSlot();
        }

        // initialize slot if first packet
        if (slot.received_count==0){
            slot.frame_seq = hdr.frame_seq;
            slot.total_packets = hdr.total_packets;
            slot.buffer.assign((size_t)hdr.total_packets * FRAG_PAYLOAD_CAP, 0);
            slot.payloadLens.assign(hdr.total_packets, 0);
            slot.receivedMask.assign(hdr.total_packets, 0);
            slot.received_count = 0;
            slot.start = std::chrono::steady_clock::now();
            slot.expected_crc = hdr.crc32;
            slot.ready = false;
        }

        // bounds check
        if (hdr.packet_seq >= slot.total_packets) continue;
        size_t payloadOffset = sizeof(PacketHeader);
        size_t available = (size_t)r - payloadOffset;
        size_t copyLen = std::min<size_t>(available, hdr.payload_len);
        size_t destOffset = (size_t)hdr.packet_seq * FRAG_PAYLOAD_CAP;
        if (destOffset + copyLen > slot.buffer.size()) continue;

        // copy payload into reassembly buffer
        memcpy(slot.buffer.data() + destOffset, buf.data() + payloadOffset, copyLen);
        if (slot.receivedMask[hdr.packet_seq] == 0){
            slot.receivedMask[hdr.packet_seq] = 1;
            slot.payloadLens[hdr.packet_seq] = (uint32_t)copyLen;
            slot.received_count++;
        }

        // completion check
        if (slot.received_count == slot.total_packets){
            // assemble exact sized frame
            size_t totalLen = 0;
            for (uint32_t i=0;i<slot.total_packets;i++) totalLen += slot.payloadLens[i];
            if (totalLen == 0 || totalLen > MAX_FRAME_BYTES){
                slot = FrameSlot();
                continue;
            }
            std::vector<uint8_t> assembled;
            assembled.reserve(totalLen);
            for (uint32_t i=0;i<slot.total_packets;i++){
                size_t srcOff = (size_t)i * FRAG_PAYLOAD_CAP;
                assembled.insert(assembled.end(), slot.buffer.begin()+srcOff, slot.buffer.begin()+srcOff + slot.payloadLens[i]);
            }

            // verify CRC
            uint32_t calc = crc32(assembled.data(), assembled.size());
            if (slot.expected_crc != 0 && calc != slot.expected_crc){
                // CRC mismatch, drop
                slot = FrameSlot();
                continue;
            }

            // commit frame as latest
            {
                std::lock_guard<std::mutex> lk(frameMutex);
                latestFrame.swap(assembled);
            }
            // reset slot
            slot = FrameSlot();
        }
    }

    close(sock);
}
