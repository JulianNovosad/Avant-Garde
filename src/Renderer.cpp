#include "Renderer.h"
#include "raylib.h"
#include <GL/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

Renderer::Renderer(){}
Renderer::~Renderer(){ shutdown(); }

bool Renderer::init(int w, int h){
    if (inited) return true;
    width = w; height = h;
    InitWindow(w, h, "Blacknode 1 - Cyberpunk Cockpit");
    SetTargetFPS(60);
    // create placeholder video texture
    Image img = GenImageColor(4,4, BLACK);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    videoTex = tex.id;
    inited = true;
    return true;
}

void Renderer::shutdown(){ if (!inited) return; CloseWindow(); inited=false; }

void Renderer::renderFrame(){
    if (!inited) return;
    BeginDrawing();
    ClearBackground(BLACK);
    renderVideoBackground();
    renderCluster();
    renderHUD();
    EndDrawing();
}

void Renderer::renderVideoBackground(){
    // Draw full-screen texture quad
    if (videoTex){
        // Use raylib to draw texture stretched
        Texture2D t; t.id = (unsigned int)videoTex; t.width = width; t.height = height; t.mipmaps = 1; t.format = 0;
        DrawTexturePro(t, {0,0,(float)width,(float)height}, {0,0,(float)width,(float)height}, {0,0}, 0.0f, WHITE);
    }
}

void Renderer::renderCluster(){
    // Simple placeholder for instanced isometric cubes
    int cols = 8, rows = 6;
    for (int y=0;y<rows;++y){
        for (int x=0;x<cols;++x){
            float px = 50 + x*60 + (y%2?30:0);
            float py = 80 + y*40;
            Color col = (Color){100,180,255,80};
            DrawRectangle((int)px, (int)py, 40, 24, col);
            DrawRectangleLines((int)px, (int)py, 40, 24, Fade(WHITE,0.3f));
        }
    }
}

void Renderer::renderHUD(){
    // Pitch ladder (placeholder)
    DrawLine(20, height/2, width-20, height/2, Fade(GRAY,0.5f));
    // Reticle
    Vector2 center{(float)width*0.5f, (float)height*0.5f};
    if (!target_locked) DrawCircleV(center, 24, GREEN);
    else {
        // red diamond
        DrawTriangle((Vector2){center.x, center.y-20}, (Vector2){center.x-20, center.y}, (Vector2){center.x+20, center.y}, RED);
        DrawTriangle((Vector2){center.x, center.y+20}, (Vector2){center.x-20, center.y}, (Vector2){center.x+20, center.y}, RED);
    }
}
