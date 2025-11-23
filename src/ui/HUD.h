#pragma once
#include <cstdint>
#include <glm/vec2.hpp>

class HUD {
public:
    HUD();
    ~HUD();
    bool init(int w, int h);
    void shutdown();
    void update(float dt);
    void render();

    void setTargetScreen(float sx, float sy, float w, float h);
    void setLocked(bool v);
    void setE2ELatencyMs(int ms);
    void setStreamAlive(bool alive);

private:
    int width=0, height=0;
    bool locked=false;
    bool streamAlive=true;
    float tx=0.0f, ty=0.0f, tw=0.0f, th=0.0f;
    float dtx=0.0f, dty=0.0f; // display-smoothed target
    int latencyMs=0;
    // GL resources
    unsigned int prog=0;
    unsigned int vao=0, vbo=0;
    unsigned int uOrtho_loc=-1, uColor_loc=-1;
    void buildShader();
};
