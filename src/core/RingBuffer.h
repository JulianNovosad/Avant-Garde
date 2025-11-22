#pragma once
#include <vector>
#include <mutex>

template<typename T>
class RingBuffer {
public:
    RingBuffer(size_t capacity=1024){ buf.resize(capacity); head=0; tail=0; full=false; }
    bool push(const T &v){ std::lock_guard<std::mutex> lk(mtx); if (full) return false; buf[head]=v; head=(head+1)%buf.size(); if (head==tail) full=true; return true; }
    bool pop(T &out){ std::lock_guard<std::mutex> lk(mtx); if (empty()) return false; out = buf[tail]; tail=(tail+1)%buf.size(); full=false; return true; }
    bool empty() const { return (!full && (head==tail)); }
    bool isFull() const { return full; }
    size_t capacity() const { return buf.size(); }
private:
    std::vector<T> buf; size_t head, tail; bool full; mutable std::mutex mtx;
};
