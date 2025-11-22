#pragma once
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(int width, int height);
    void shutdown();
    void renderFrame();

    // set latest video texture data (GL texture id)
    void setVideoTexture(unsigned int tex) { videoTex = tex; }

    // HUD state
    void setTargetLocked(bool locked) { target_locked = locked; }

private:
    bool inited = false;
    unsigned int videoTex = 0;
    int width=0, height=0;
    bool target_locked = false;

    void renderVideoBackground();
    void renderCluster();
    void renderHUD();
};
