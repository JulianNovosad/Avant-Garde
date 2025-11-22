#include "EventBus.h"
using namespace core;

int EventBus::subscribe(const std::string &topic, Handler h){
    std::lock_guard<std::mutex> lk(mtx);
    int id = nextId++;
    subs[topic].push_back({id, h});
    return id;
}

void EventBus::unsubscribe(const std::string &topic, int id){
    std::lock_guard<std::mutex> lk(mtx);
    auto it = subs.find(topic);
    if (it==subs.end()) return;
    auto &vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(), [&](auto &p){ return p.first==id; }), vec.end());
}

void EventBus::publish(const std::string &topic, const std::string &payload){
    std::vector<Handler> handlers;
    {
        std::lock_guard<std::mutex> lk(mtx);
        auto it = subs.find(topic);
        if (it==subs.end()) return;
        for (auto &p: it->second) handlers.push_back(p.second);
    }
    for (auto &h: handlers) h(topic, payload);
}
