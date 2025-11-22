#include "TCPClient.h"
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <iostream>
#include "../core/NativeBridge.h"

TCPClient::TCPClient(){}
TCPClient::~TCPClient(){ stop(); }

void TCPClient::start(const std::string &host, uint16_t port){
    if (running.load()) return;
    running.store(true);
    thr = std::thread(&TCPClient::run, this, host, port);
}

void TCPClient::stop(){
    if (!running.load()) return;
    running.store(false);
    if (thr.joinable()) thr.join();
}

void TCPClient::setMessageCallback(MessageCallback cb){ msgCb = cb; }

static int make_nonblocking_socket(){
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s<0) return -1;
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
    return s;
}

void TCPClient::run(const std::string &host, uint16_t port){
    while (running.load()){
        struct addrinfo hints{}, *res=nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        char portstr[16]; snprintf(portstr, sizeof(portstr), "%u", port);
        if (getaddrinfo(host.c_str(), portstr, &hints, &res)!=0){
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        int s = make_nonblocking_socket();
        if (s<0){ freeaddrinfo(res); std::this_thread::sleep_for(std::chrono::seconds(2)); continue; }
        int conn = connect(s, res->ai_addr, res->ai_addrlen);
        if (conn<0){
            if (errno!=EINPROGRESS){ close(s); freeaddrinfo(res); std::this_thread::sleep_for(std::chrono::seconds(2)); continue; }
        }
        freeaddrinfo(res);
        // wait for connect completion or timeout
        fd_set wf; FD_ZERO(&wf); FD_SET(s,&wf);
        struct timeval tv; tv.tv_sec = 5; tv.tv_usec = 0;
        int sel = select(s+1, NULL, &wf, NULL, &tv);
        if (sel<=0){ close(s); std::this_thread::sleep_for(std::chrono::seconds(2)); continue; }

        // connected
        std::string buffer;
        buffer.reserve(4096);
        char tmp[1024];
        while (running.load()){
            ssize_t r = recv(s, tmp, sizeof(tmp), 0);
            if (r>0){ buffer.append(tmp, tmp+r);
                // split by newline
                size_t pos;
                while ((pos = buffer.find('\n')) != std::string::npos){
                    std::string line = buffer.substr(0,pos);
                    buffer.erase(0,pos+1);
                    if (!line.empty()){
                        if (msgCb) msgCb(line);
                        // publish to native EventBus
                        core::NativeBridge::publish("tcp_message", line);
                    }
                }
            } else if (r==0){ // closed
                break;
            } else {
                if (errno==EAGAIN || errno==EWOULDBLOCK){ std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
                else break;
            }
        }
        close(s);
        if (running.load()) std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
