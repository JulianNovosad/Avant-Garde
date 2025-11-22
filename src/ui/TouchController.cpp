#include "TouchController.h"

TouchController::TouchController(){}

void TouchController::onMouseDown(int x, int y, int button){ lastX=x; lastY=y; dragging=true; dragButton=button; }
void TouchController::onMouseMove(int x, int y){ if (!dragging) return; int dx = x - lastX; int dy = y - lastY; lastX = x; lastY = y; if (dragButton==1){ // left -> orbit
    deltaX += (float)dx; deltaY += (float)dy; } else if (dragButton==2){ // right -> pan
    deltaX += (float)dx*0.5f; deltaY += (float)dy*0.5f; } }
void TouchController::onMouseUp(int x, int y, int button){ dragging=false; dragButton=0; }
void TouchController::onScroll(float dy){ zoomDelta += dy; }

bool TouchController::pollHold(int &outX, int &outY){
    using namespace std::chrono;
    if (dragging){
        if (!pressed){ pressed=true; pressTime = steady_clock::now(); holdFired=false; }
        else if (!holdFired){
            auto now = steady_clock::now();
            auto ms = duration_cast<milliseconds>(now-pressTime).count();
            if (ms >= holdMs){ holdFired = true; outX = lastX; outY = lastY; return true; }
        }
    } else { pressed=false; holdFired=false; }
    return false;
}

void TouchController::onTouchBegin(int id, int x, int y){
    if (touchA==-1){ touchA=id; ax=x; ay=y; }
    else if (touchB==-1){ touchB=id; bx=x; by=y; }
}

void TouchController::onTouchMove(int id, int x, int y){
    if (id==touchA){ ax=x; ay=y; }
    else if (id==touchB){ bx=x; by=y; }
    if (touchA!=-1 && touchB!=-1){
        float dx = (float)(ax-bx);
        float dy = (float)(ay-by);
        float dist = sqrtf(dx*dx + dy*dy);
        static float lastDist = dist;
        pinchDelta = (dist - lastDist) * 0.01f;
        lastDist = dist;
    }
}

void TouchController::onTouchEnd(int id){
    if (id==touchA) touchA=-1;
    else if (id==touchB) touchB=-1;
    pinchDelta = 0.0f;
}

float TouchController::getPinchDelta(){ return pinchDelta; }
