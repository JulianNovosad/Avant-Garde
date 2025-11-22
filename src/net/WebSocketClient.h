#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>

class WebSocketClient {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    WebSocketClient();
    ~WebSocketClient();
    void start(const std::string &url); // e.g. ws://host:port/path
    void stop();
    void setMessageCallback(MessageCallback cb);

private:
    void run(const std::string &host, uint16_t port, const std::string &path);
    std::thread thr;
    std::atomic<bool> running{false};
    MessageCallback msgCb;
};
