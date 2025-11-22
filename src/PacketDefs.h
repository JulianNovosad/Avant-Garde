#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

// Packet header (network / big-endian)
struct FramePacketHeader {
    uint32_t magic; // 4 bytes
    uint32_t frame_seq; // 4 bytes
    uint16_t packet_seq; // 2 bytes
    uint16_t total_packets; // 2 bytes
    uint16_t payload_len; // 2 bytes
    uint32_t crc32; // 4 bytes (crc of full assembled frame)
};

// helper to decode BE fields on any platform
static inline uint16_t be16(const uint8_t *b){ return (uint16_t(b[0])<<8) | uint16_t(b[1]); }
static inline uint32_t be32(const uint8_t *b){ return (uint32_t(b[0])<<24) | (uint32_t(b[1])<<16) | (uint32_t(b[2])<<8) | uint32_t(b[3]); }

// Telemetry JSON schemas are parsed with RapidJSON in the receiver.
// Example payloads are documented in the protocol spec.
#pragma once
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>

// Packet header, big-endian on wire
#pragma pack(push,1)
struct PacketHeader {
    uint32_t magic;        // 0x4D4A5047 ("MJPG")
    uint32_t frame_seq;    // Monotonic frame ID
    uint16_t packet_seq;   // Index of this chunk
    uint16_t total_packets;// Total chunks for this frame
    uint16_t payload_len;  // Data size (Max 1300 bytes per chunk)
    uint32_t crc32;        // CRC of the *entire* reassembled JPEG
};
#pragma pack(pop)

inline void PacketHeader_ntoh(PacketHeader &h) {
    h.magic = ntohl(h.magic);
    h.frame_seq = ntohl(h.frame_seq);
    h.packet_seq = ntohs(h.packet_seq);
    h.total_packets = ntohs(h.total_packets);
    h.payload_len = ntohs(h.payload_len);
    h.crc32 = ntohl(h.crc32);
}

inline void PacketHeader_hton(PacketHeader &h) {
    h.magic = htonl(h.magic);
    h.frame_seq = htonl(h.frame_seq);
    h.packet_seq = htons(h.packet_seq);
    h.total_packets = htons(h.total_packets);
    h.payload_len = htons(h.payload_len);
    h.crc32 = htonl(h.crc32);
}

// CRC32 helper (simple implementation)
static inline uint32_t crc32_compute(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1u)));
    }
    return ~crc;
}
