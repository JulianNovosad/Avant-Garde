#pragma once
#include <string>
#include <cstdint>
#include <netinet/in.h>

class ControlSender {
public:
    ControlSender();
    ~ControlSender();
    bool start(const std::string &host, uint16_t port);
    void stop();
    // normalized coords 0..1
    void sendReticle(float nx, float ny, int redundancy = 4);
private:
    int sockfd = -1;
    std::string host;
    uint16_t port=0;
    struct sockaddr_in dest{};
};
