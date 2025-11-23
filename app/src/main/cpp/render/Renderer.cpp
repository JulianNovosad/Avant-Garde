#include "Renderer.h"
#include <raylib.h>
#include <vector>
#include <mutex>
#include <iostream>

static std::mutex texMutex;
struct TexState {
    Texture2D tex = {0,0,0,0,0};
    std::vector<uint8_t> staging;
} g_texState;

Renderer::Renderer(){}
Renderer::~Renderer(){ shutdown(); }

bool Renderer::init(int width, int height){
    w = width; h = height;
    InitWindow(w, h, "Blacknode - Android");
    SetTargetFPS(60);

    // Initialize an empty texture; we'll update it when frames arrive
    Image img = GenImageColor(2,2, BLACK);
    g_texState.tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return true;
}

void Renderer::shutdown(){
    if (g_texState.tex.id) UnloadTexture(g_texState.tex);
    CloseWindow();
}

void Renderer::update(float dt){ (void)dt; }

void Renderer::uploadJpegFrame(const uint8_t* data, int len){
    std::lock_guard<std::mutex> lk(texMutex);
    // Attempt to load JPEG into an Image using raylib
    // Raylib doesn't directly provide JPEG decoding API; platforms normally
    // provide helper. For this skeleton, we use LoadImageFromMemory.
    Image img = {0};
    if (data && len>0){
        img = LoadImageFromMemory("jpg", data, len);
        if (img.data){
            // replace texture
            if (g_texState.tex.id) UnloadTexture(g_texState.tex);
            g_texState.tex = LoadTextureFromImage(img);
            UnloadImage(img);
        }
    }
}

void Renderer::render(bool cockpitMode){
    BeginDrawing();
    ClearBackground(BLACK);
    // Draw video full-screen
    std::lock_guard<std::mutex> lk(texMutex);
    if (g_texState.tex.id){
        DrawTexturePro(g_texState.tex, (Rectangle){0,0,(float)g_texState.tex.width,(float)g_texState.tex.height},
                       (Rectangle){0,0,(float)w,(float)h}, (Vector2){0,0}, 0.0f, WHITE);
    }

    // HUD overlay (simple green reticle and pitch ladder)
    if (cockpitMode){
        // reticle
        DrawCircle(w/2, h/2, 24, GREEN);
        DrawLine(w/2-40, h/2, w/2+40, h/2, GREEN);
        DrawLine(w/2, h/2-40, w/2, h/2+40, GREEN);
        // simple pitch ladder lines
        for (int i=-3;i<=3;i++){
            int y = h/2 + i*30;
            DrawLine(w/2-100, y, w/2+100, y, (i==0? GREEN: Fade(GREEN, 0.4f)));
        }
    } else {
        // Construct view: draw an isometric cluster of cubes
        // Keep it simple: draw several rectangles to simulate cubes
        int cx = w/2, cy = h/2;
        for (int i=0;i<6;i++){
            int dx = ((i%3)-1)*60; int dy = (i/3-0)*40;
            DrawRectangle(cx+dx-30, cy+dy-30, 60, 60, LIGHTGRAY);
            DrawRectangleLines(cx+dx-30, cy+dy-30, 60, 60, DARKGRAY);
        }
    }

    EndDrawing();
}

void Renderer::startConstructSequence(){
    // placeholder: in a real implementation we'd animate
    std::cout << "Construct sequence started" << std::endl;
}
