#include "UdpReceiver.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include "../core/NativeBridge.h"

static inline uint64_t ntoh64(uint64_t x){
    return (((uint64_t)ntohl(x & 0xFFFFFFFFULL)) << 32) | ntohl(x >> 32);
}

UdpReceiver::UdpReceiver(){}
UdpReceiver::~UdpReceiver(){ stop(); }

bool UdpReceiver::start(uint16_t port){
    if (running.load()) return false;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return false;
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    int rcvbuf = 4*1024*1024;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr))<0){ close(sockfd); sockfd=-1; return false; }
    running.store(true);
    thr = std::thread(&UdpReceiver::run, this);
    return true;
}

void UdpReceiver::stop(){
    if (!running.load()) return;
    running.store(false);
    if (thr.joinable()) thr.join();
    if (sockfd>=0) { close(sockfd); sockfd=-1; }
}

void UdpReceiver::setCallback(Callback c){ cb = c; }

void UdpReceiver::run(){
    constexpr size_t PKT_SIZE = 64;
    uint8_t buf[PKT_SIZE];
    while (running.load()){
        ssize_t r = recv(sockfd, buf, PKT_SIZE, 0);
        if (r <= 0){ std::this_thread::sleep_for(std::chrono::milliseconds(2)); continue; }
        if ((size_t)r < sizeof(TelemetryPacket)) continue;
        TelemetryPacket p{};
        memcpy(&p, buf, sizeof(TelemetryPacket));
        // convert network to host order
        p.magic = ntohl(p.magic);
        p.timestamp = ntoh64(p.timestamp);
        // floats are sent in IEEE-754; assume same endianness; if not, swap needed
        // state already single byte
        if (p.magic != 0xC0A1FACEu) continue;
            if (cb) cb(p);
            // publish a compact JSON to EventBus
            char tmp[256];
            int n = snprintf(tmp, sizeof(tmp), "{\"ts\":%llu,\"x\":%f,\"y\":%f,\"z\":%f,\"conf\":%f,\"state\":%u}", (unsigned long long)p.timestamp, p.position[0], p.position[1], p.position[2], p.confidence, (unsigned)p.state);
            if (n>0) core::NativeBridge::publish("udp_telemetry", std::string(tmp, tmp+std::min((int)n, (int)sizeof(tmp))));
    }
}
