#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <string>

namespace core {

class EventBus {
public:
    using Handler = std::function<void(const std::string& topic, const std::string& payload)>;
    int subscribe(const std::string &topic, Handler h);
    void unsubscribe(const std::string &topic, int id);
    void publish(const std::string &topic, const std::string &payload);
private:
    std::mutex mtx;
    std::unordered_map<std::string, std::vector<std::pair<int,Handler>>> subs;
    int nextId = 1;
};

}
