#include <raylib.h>
#include <rlgl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <chrono>
#include <cmath>
#include <string>

using namespace std::chrono;

struct ClusterState {
    enum Type { HIVE, BREAKUP, INSPECTION } type = HIVE;
    float t = 0.0f; // interpolation
};

class Scene {
public:
    Scene(): state(){ }
    void init(int w, int h){
        screenW = w; screenH = h;
        // instanced cube setup
        InitWindow(w, h, "Blacknode");
        SetTargetFPS(60);
        // generate instance transforms for 50 cubes in hexagon
        generateHive();
        // additive shader
        const char *vsrc = "#version 300 es\nlayout (location=0) in vec3 aPos; layout(location=1) in mat4 instanceModel; uniform mat4 proj; uniform mat4 view; void main(){ gl_Position = proj * view * instanceModel * vec4(aPos,1.0); }";
        const char *fsrc = "#version 300 es\nprecision mediump float; out vec4 fragColor; void main(){ fragColor = vec4(0.583,0.38,0.678,0.6); }";
        shader = LoadShaderFromMemory(vsrc, fsrc);
        // use rlgl for instancing VAO
    }

    void update(float dt){
        // update state transitions
        state.t += dt;
        // simple pulsing
        for(auto &m: models) m.a += dt*0.5f;
    }

    void render(){
        BeginDrawing();
        ClearBackground(BLACK);
        // 3D
        BeginMode3D(camera);
        rlPushMatrix();
        // draw instanced cubes
        for(auto &mi: models){
            DrawCubeV(mi.pos, mi.size, Color{149,97,173,150});
        }
        rlPopMatrix();
        EndMode3D();

        // HUD
        drawHUD();

        EndDrawing();
    }

    void shutdown(){ CloseWindow(); }

private:
    int screenW=1280, screenH=720;
    ClusterState state;
    Shader shader{0};
    Camera3D camera{0};

    struct ModelInstance{ Vector3 pos; Vector3 size; float a; };
    std::vector<ModelInstance> models;

    void generateHive(){
        // hexagon packing approximate
        int count = 60;
        float R = 3.0f;
        for(int i=0;i<count;++i){
            float ang = (float)i / (float)count * (2.0f * 3.1415926f);
            float r = R * (0.5f + 0.5f * (float)rand()/RAND_MAX);
            ModelInstance mi; mi.pos = { r * cosf(ang), 0.0f, r * sinf(ang) };
            mi.size = {0.3f,0.3f,0.3f}; mi.a = 0.0f;
            models.push_back(mi);
        }
        camera.position = { 6.0f, 6.0f, 6.0f };
        camera.target = { 0.0f, 0.0f, 0.0f };
        camera.up = { 0.0f, 1.0f, 0.0f };
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;
    }

    void drawHUD(){
        // pitch ladder - fake gyro data for placeholder
        float horizon = screenH*0.5f;
        for(int i=-5;i<=5;++i){
            int y = (int)(horizon + i*20);
            DrawLine(20,y, screenW-20, y, Fade(GREEN, 0.3f));
        }
        // reticle
        DrawCircle(screenW/2, screenH/2, 20, GREEN);
    }
};
