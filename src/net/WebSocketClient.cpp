#include "WebSocketClient.h"
#include <sys/socket.h>
#include <netdb.h>
#include "Network/NetworkPorts.h"
using namespace blacknode;
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <array>
#include <random>
#include <openssl/sha.h>
#include "../core/NativeBridge.h"

// Simple base64 encoder
static std::string base64_encode(const unsigned char* data, size_t len){
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; out.reserve(((len+2)/3)*4);
    size_t i=0;
    while (i<len){
        uint32_t a = i<len?data[i++]:0;
        uint32_t b = i<len?data[i++]:0;
        uint32_t c = i<len?data[i++]:0;
        uint32_t triple = (a<<16) | (b<<8) | c;
        out.push_back(table[(triple >> 18) & 0x3F]);
        out.push_back(table[(triple >> 12) & 0x3F]);
        out.push_back(i-1>len? '=' : table[(triple >> 6) & 0x3F]);
        out.push_back(i>len? '=' : table[triple & 0x3F]);
    }
    return out;
}

static std::string sha1_base64(const std::string &input){
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char*)input.data(), input.size(), hash);
    return base64_encode(hash, SHA_DIGEST_LENGTH);
}

static bool parse_ws_url(const std::string &url, std::string &host, uint16_t &port, std::string &path){
    // expected ws://host:port/path
    if (url.rfind("ws://",0)!=0) return false;
    std::string rest = url.substr(5);
    size_t p1 = rest.find('/');
    std::string hostport = p1==std::string::npos? rest : rest.substr(0,p1);
    path = p1==std::string::npos? "/" : rest.substr(p1);
    size_t colon = hostport.find(':');
    if (colon==std::string::npos){ host = hostport; port = DEFAULT_WS_PORT; }
    else { host = hostport.substr(0,colon); port = (uint16_t)atoi(hostport.c_str()+colon+1); }
    return true;
}

static int make_nonblocking_socket(){
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s<0) return -1;
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
    return s;
}

WebSocketClient::WebSocketClient(){}
WebSocketClient::~WebSocketClient(){ stop(); }

void WebSocketClient::start(const std::string &url){
    if (running.load()) return;
    std::string host; uint16_t port; std::string path;
    if (!parse_ws_url(url, host, port, path)) return;
    running.store(true);
    thr = std::thread(&WebSocketClient::run, this, host, port, path);
}

void WebSocketClient::stop(){
    if (!running.load()) return;
    running.store(false);
    if (thr.joinable()) thr.join();
}

void WebSocketClient::setMessageCallback(MessageCallback cb){ msgCb = cb; }

void WebSocketClient::run(const std::string &host, uint16_t port, const std::string &path){
    while (running.load()){
        struct addrinfo hints{}, *res=nullptr;
        hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        char portstr[16]; snprintf(portstr, sizeof(portstr), "%u", port);
        if (getaddrinfo(host.c_str(), portstr, &hints, &res)!=0){ std::this_thread::sleep_for(std::chrono::seconds(2)); continue; }
        int s = make_nonblocking_socket();
        if (s<0){ freeaddrinfo(res); std::this_thread::sleep_for(std::chrono::seconds(2)); continue; }
        connect(s, res->ai_addr, res->ai_addrlen);
        // wait for connection
        fd_set wf; FD_ZERO(&wf); FD_SET(s,&wf);
        struct timeval tv; tv.tv_sec = 5; tv.tv_usec = 0;
        int sel = select(s+1, NULL, &wf, NULL, &tv);
        if (sel<=0){ close(s); freeaddrinfo(res); std::this_thread::sleep_for(std::chrono::seconds(2)); continue; }

        // build handshake
        std::array<unsigned char,16> keybin;
        std::random_device rd; for (int i=0;i<16;++i) keybin[i]=rd()%256;
        std::string secKey = base64_encode(keybin.data(), keybin.size());
        std::ostringstream req;
        req<<"GET "<<path<<" HTTP/1.1\r\n";
        req<<"Host: "<<host<<":"<<port<<"\r\n";
        req<<"Upgrade: websocket\r\n";
        req<<"Connection: Upgrade\r\n";
        req<<"Sec-WebSocket-Key: "<<secKey<<"\r\n";
        req<<"Sec-WebSocket-Version: 13\r\n";
        req<<"\r\n";
        std::string reqs = req.str();
        send(s, reqs.c_str(), reqs.size(), 0);

        // read response
        std::string resp;
        char buf[1024];
        int rcv; bool headerDone=false;
        for (int attempts=0; attempts<50 && !headerDone; ++attempts){
            ssize_t r = recv(s, buf, sizeof(buf), 0);
            if (r>0){ resp.append(buf, buf+r); if (resp.find("\r\n\r\n")!=std::string::npos) headerDone=true; }
            else std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (!headerDone){ close(s); freeaddrinfo(res); std::this_thread::sleep_for(std::chrono::seconds(2)); continue; }
        // TODO: basic verification
        if (resp.find("101")==std::string::npos){ close(s); freeaddrinfo(res); std::this_thread::sleep_for(std::chrono::seconds(2)); continue; }

        // Simple frame reader for text frames (no fragmentation support for brevity)
        std::string payloadBuf;
        while (running.load()){
            // read header 2 bytes
            unsigned char hdr[2]; ssize_t r = recv(s, hdr, 2, MSG_WAITALL);
            if (r<=0) break;
            bool fin = hdr[0] & 0x80;
            unsigned char opcode = hdr[0] & 0x0F;
            bool mask = hdr[1] & 0x80;
            uint64_t plen = hdr[1] & 0x7F;
            if (plen==126){ unsigned char ext[2]; recv(s, ext, 2, MSG_WAITALL); plen = (ext[0]<<8) | ext[1]; }
            else if (plen==127){ unsigned char ext[8]; recv(s, ext, 8, MSG_WAITALL); plen = 0; for (int i=0;i<8;++i) plen = (plen<<8) | ext[i]; }
            unsigned char maskkey[4]={0,0,0,0};
            if (mask) recv(s, maskkey, 4, MSG_WAITALL);
            std::vector<char> payload(plen);
            size_t got = 0;
            while (got < plen){ ssize_t rc = recv(s, payload.data()+got, plen-got, 0); if (rc<=0) break; got += rc; }
            if (mask){ for (size_t i=0;i<got;++i) payload[i] ^= maskkey[i%4]; }
            if (opcode==0x1){ // text
                std::string text(payload.data(), payload.data()+got);
                if (msgCb) msgCb(text);
                core::NativeBridge::publish("ws_message", text);
            } else if (opcode==0x8){ // close
                break;
            }
        }
        close(s);
        freeaddrinfo(res);
        if (running.load()) std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
