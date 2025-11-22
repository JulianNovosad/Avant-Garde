#include "BackpressureDetector.h"
#include <algorithm>

BackpressureDetector::BackpressureDetector(){ lastDecay = std::chrono::steady_clock::now(); }

void BackpressureDetector::notifyPacket(){ count.fetch_add(1); }

void BackpressureDetector::update(){
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto dt = duration_cast<milliseconds>(now - lastDecay).count();
    if (dt < 100) return;
    int c = count.exchange(0);
    // map packet count to level: assume target ~1000pps baseline
    float desired = std::min(1.0f, c / 500.0f);
    // smooth
    float cur = level.load();
    cur = cur*0.8f + desired*0.2f;
    level.store(cur);
    lastDecay = now;
}
