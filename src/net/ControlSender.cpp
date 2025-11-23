#include "ControlSender.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <iostream>

ControlSender::ControlSender(){}
ControlSender::~ControlSender(){ stop(); }

bool ControlSender::start(const std::string &h, uint16_t p){
    stop();
    host = h; port = p;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); return false; }
    struct addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *res = nullptr;
    int r = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (r != 0 || !res){ std::cerr<<"getaddrinfo failed: "<<gai_strerror(r)<<"\n"; return false; }
    struct sockaddr_in *sin = (struct sockaddr_in*)res->ai_addr;
    dest.sin_family = AF_INET; dest.sin_addr = sin->sin_addr; dest.sin_port = htons(port);
    freeaddrinfo(res);
    return true;
}

void ControlSender::stop(){ if (sockfd>=0) { close(sockfd); sockfd=-1; } }

void ControlSender::sendReticle(float nx, float ny, int redundancy){
    if (sockfd<0) return;
    if (nx < 0.0f) nx = 0.5f; if (ny < 0.0f) ny = 0.5f;
    char buf[128]; int n = snprintf(buf, sizeof(buf), "{\"type\":\"reticle\",\"x\":%f,\"y\":%f}", nx, ny);
    for (int i=0;i<redundancy;i++){
        ssize_t s = sendto(sockfd, buf, n, 0, (struct sockaddr*)&dest, sizeof(dest));
        (void)s;
    }
}
