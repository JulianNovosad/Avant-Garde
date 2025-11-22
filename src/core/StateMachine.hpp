#pragma once
#include <chrono>
#include <atomic>
#include <functional>
#include <glm/glm.hpp>

namespace core {

enum class NodeState : uint8_t { OFFLINE=0, ACTIVE=1, DEGRADED=2, FALLBACK=3 };

struct Color { float r,g,b,a; };

class StateMachine {
public:
    StateMachine();
    void setState(NodeState s);
    NodeState getState() const;
    // call each frame to progress animations; dt in seconds
    void update(float dt);
    Color getColor() const; // current animated color
    // instant set (no animation)
    void forceState(NodeState s);

private:
    std::atomic<NodeState> state;
    NodeState targetState;
    Color currentColor;
    Color startColor;
    Color targetColor;
    float animTime; // elapsed
    float animDuration; // total
    static Color colorForState(NodeState s);
};

}
