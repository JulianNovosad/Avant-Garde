#pragma once
#include <functional>
#include <chrono>
#include <glm/glm.hpp>

class TouchController {
public:
    using TouchCallback = std::function<void()>;
    TouchController();
    // desktop-friendly mouse handlers
    void onMouseDown(int x, int y, int button);
    void onMouseMove(int x, int y);
    void onMouseUp(int x, int y, int button);
    void onScroll(float dy);
    // hold detection: call pollHold each frame, returns true once when hold threshold reached
    bool pollHold(int &outX, int &outY);
    // pinch (two-finger) support: call onTouchBegin/Move/End on multi-touch platforms
    void onTouchBegin(int id, int x, int y);
    void onTouchMove(int id, int x, int y);
    void onTouchEnd(int id);
    float getPinchDelta();
    // gestures (pinch/pan) can be added for mobile
    float getDeltaX() const { return deltaX; }
    float getDeltaY() const { return deltaY; }
    float getZoomDelta() const { return zoomDelta; }
    void resetDeltas(){ deltaX=deltaY=zoomDelta=0.0f; }
private:
    int lastX=0, lastY=0; bool dragging=false; int dragButton=0;
    float deltaX=0.0f, deltaY=0.0f, zoomDelta=0.0f;
    std::chrono::steady_clock::time_point pressTime;
    bool pressed = false;
    bool holdFired = false;
    const int holdMs = 500;
    // multi-touch state
    int touchA = -1, touchB = -1;
    int ax=0, ay=0, bx=0, by=0;
    float pinchDelta = 0.0f;
};
