#include "StateMachine.hpp"
using namespace core;

StateMachine::StateMachine(){
    state.store(NodeState::OFFLINE);
    targetState = NodeState::OFFLINE;
    currentColor = colorForState(NodeState::OFFLINE);
    startColor = currentColor;
    targetColor = currentColor;
    animTime = 0.0f; animDuration = 0.2f; // default
}

void StateMachine::setState(NodeState s){
    NodeState prev = state.load();
    if (prev == s) return;
    targetState = s;
    startColor = currentColor;
    targetColor = colorForState(s);
    animTime = 0.0f;
    // choose duration 0.15-0.25 depending on transition severity
    animDuration = (prev==NodeState::OFFLINE || s==NodeState::OFFLINE)?0.25f:0.18f;
    state.store(s);
}

void StateMachine::forceState(NodeState s){ state.store(s); targetState=s; currentColor=colorForState(s); startColor=currentColor; targetColor=currentColor; animTime=0.0f; }

NodeState StateMachine::getState() const{ return state.load(); }

void StateMachine::update(float dt){
    if (animTime < animDuration){
        animTime += dt;
        float t = animTime / animDuration;
        if (t>1.0f) t=1.0f;
        // lerp color
        currentColor.r = startColor.r + (targetColor.r - startColor.r)*t;
        currentColor.g = startColor.g + (targetColor.g - startColor.g)*t;
        currentColor.b = startColor.b + (targetColor.b - startColor.b)*t;
        currentColor.a = startColor.a + (targetColor.a - startColor.a)*t;
    }
}

Color StateMachine::getColor() const{ return currentColor; }

Color StateMachine::colorForState(NodeState s){
    switch(s){
        case NodeState::ACTIVE: return {0.0f, 1.0f, 0.0f, 1.0f};
        case NodeState::OFFLINE: return {1.0f, 0.0f, 0.0f, 1.0f};
        case NodeState::DEGRADED: return {1.0f, 0.8f, 0.0f, 1.0f};
        case NodeState::FALLBACK: default: return {0.0f, 0.4f, 1.0f, 1.0f};
    }
}
