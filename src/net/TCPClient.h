#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>

class TCPClient {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    TCPClient();
    ~TCPClient();
    void start(const std::string &host, uint16_t port);
    void stop();
    void setMessageCallback(MessageCallback cb);

private:
    void run(const std::string &host, uint16_t port);
    std::thread thr;
    std::atomic<bool> running{false};
    MessageCallback msgCb;
};
