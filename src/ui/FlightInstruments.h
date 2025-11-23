#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <glm/vec2.hpp>

class FlightInstruments {
public:
    FlightInstruments();
    ~FlightInstruments();
    bool init(int w, int h);
    void shutdown();
    void update(float dt);
    void render();

    // subscription id for eventbus
    int subId{-1};
private:
    int width=0, height=0;
    std::mutex mtx;
    struct Data { float pitch=0.0f; float roll=0.0f; float altitude=0.0f; float speed=0.0f; } target, disp;
    unsigned int prog=0, vao=0, vbo=0;
    int uOrtho=-1, uColor=-1;
    unsigned int compileShader(unsigned int type, const char* src);
    unsigned int linkProgram(unsigned int vs, unsigned int fs);
    void buildShader();
};
