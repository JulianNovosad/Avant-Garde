#pragma once
#include <vector>

class Renderer {
public:
    Renderer();
    ~Renderer();
    bool init(int width, int height);
    void shutdown();
    void update(float dt);
    void render(bool cockpitMode);
    void uploadJpegFrame(const uint8_t* data, int len);
    void startConstructSequence();
private:
    int w=0, h=0;
    // internal texture/state handles are hidden in implementation
};
