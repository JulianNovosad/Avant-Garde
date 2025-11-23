#include "HUD.h"
#include <GLES3/gl32.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <cmath>

HUD::HUD(){}
HUD::~HUD(){ shutdown(); }

static const char* hud_vs = "#version 330 core\nlayout(location=0) in vec2 aPos; uniform mat4 uOrtho; void main(){ gl_Position = uOrtho * vec4(aPos,0.0,1.0); }";
static const char* hud_fs = "#version 330 core\nuniform vec4 uColor; out vec4 frag; void main(){ frag = uColor; }";

unsigned int compileShader(unsigned int type, const char* src){ unsigned int s = glCreateShader(type); glShaderSource(s,1,&src,NULL); glCompileShader(s); int ok=0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok); if(!ok){ char log[1024]; glGetShaderInfoLog(s,1024,NULL,log); std::cerr<<"HUD shader compile: "<<log<<"\n"; } return s; }
unsigned int linkProgram(unsigned int vs, unsigned int fs){ unsigned int p = glCreateProgram(); glAttachShader(p,vs); glAttachShader(p,fs); glLinkProgram(p); int ok=0; glGetProgramiv(p, GL_LINK_STATUS, &ok); if(!ok){ char log[1024]; glGetProgramInfoLog(p,1024,NULL,log); std::cerr<<"HUD prog link: "<<log<<"\n"; } glDeleteShader(vs); glDeleteShader(fs); return p; }

void HUD::buildShader(){ if (prog) return; unsigned int vs = compileShader(GL_VERTEX_SHADER, hud_vs); unsigned int fs = compileShader(GL_FRAGMENT_SHADER, hud_fs); prog = linkProgram(vs, fs); uOrtho_loc = glGetUniformLocation(prog, "uOrtho"); uColor_loc = glGetUniformLocation(prog, "uColor"); glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); }

bool HUD::init(int w, int h){ width=w; height=h; buildShader(); return true; }
void HUD::shutdown(){ if (vao) glDeleteVertexArrays(1,&vao); if (vbo) glDeleteBuffers(1,&vbo); if (prog) glDeleteProgram(prog); vao=vbo=prog=0; }

#include <algorithm>
void HUD::update(float dt){
    // smooth display target towards actual over ~0.2s
    float tau = 0.2f; float a = dt / (tau + 1e-6f); if (a>1.0f) a=1.0f;
    dtx += (tx - dtx) * a;
    dty += (ty - dty) * a;
}

void HUD::setTargetScreen(float sx, float sy, float w, float h){ tx = sx; ty = sy; tw = w; th = h; }
void HUD::setLocked(bool v){ locked = v; }
void HUD::setE2ELatencyMs(int ms){ latencyMs = ms; }
void HUD::setStreamAlive(bool alive){ streamAlive = alive; }

void HUD::render(){ if (!prog) return; // draw simple overlays: reticle and latency
    glm::mat4 ortho = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    glUseProgram(prog);
    glUniformMatrix4fv(uOrtho_loc, 1, GL_FALSE, &ortho[0][0]);

    // DEBUG: draw a large semi-opaque green center rectangle to confirm rendering
    float dbgW = (float)width * 0.5f; float dbgH = (float)height * 0.5f; float dbgX = (width - dbgW) * 0.5f; float dbgY = (height - dbgH) * 0.5f;
    float dbgRect[12] = { dbgX,dbgY, dbgX+dbgW,dbgY, dbgX+dbgW,dbgY+dbgH, dbgX,dbgY, dbgX+dbgW,dbgY+dbgH, dbgX,dbgY+dbgH };
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); glBufferData(GL_ARRAY_BUFFER, sizeof(dbgRect), dbgRect, GL_DYNAMIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0); glUniform4f(uColor_loc, 0.0f, 1.0f, 0.0f, 0.12f); glDrawArrays(GL_TRIANGLES, 0, 6); glBindVertexArray(0);

    // Reticle: center open circle (idle) or red diamond (locked)
    float cx = dtx; float cy = dty;
    if (cx<=0 && cy<=0){ cx = width*0.5f; cy = height*0.5f; }
    if (!locked){
        // draw a green circle approximated by 32-seg fan lines
        const int segs = 24; float r = 32.0f;
        std::vector<float> verts; verts.reserve(segs*2);
        for (int i=0;i<segs;i++){ float a = (float)i / (float)segs * 6.2831853f; verts.push_back(cx + cosf(a)*r); verts.push_back(cy + sinf(a)*r); }
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
        glUniform4f(uColor_loc, 0.0f, 1.0f, 0.0f, 1.0f);
        glDrawArrays(GL_LINE_LOOP, 0, segs);
        glBindVertexArray(0);
    } else {
        // locked: red diamond
        float s = 18.0f; float verts[] = { cx, cy-s, cx+s, cy, cx, cy+s, cx-s, cy };
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
        glUniform4f(uColor_loc, 1.0f, 0.0f, 0.0f, 1.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
        glBindVertexArray(0);
        // thin bounding box if target dims present
        if (tw>0.5f && th>0.5f){ float bx = cx - tw*0.5f; float by = cy - th*0.5f; float bverts[12] = { bx,by, bx+tw,by, bx+tw,by+th, bx,by, bx+tw,by+th, bx,by+th }; glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); glBufferData(GL_ARRAY_BUFFER, sizeof(bverts), bverts, GL_DYNAMIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0); glUniform4f(uColor_loc, 1.0f, 0.0f, 0.0f, 0.6f); glDrawArrays(GL_LINE_LOOP, 0, 4); glBindVertexArray(0); }
    }

    // Latency text: we can't draw text easily without fonts; emit a small green rectangle + debug output
    float px = 8.0f, py = 12.0f; float wbox = 120.0f, hbox = 20.0f;
    float rect[12] = { px,py, px+wbox,py, px+wbox,py+hbox, px,py, px+wbox,py+hbox, px,py+hbox };
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_DYNAMIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0); glUniform4f(uColor_loc, 0.0f, 1.0f, 0.0f, 0.6f); glDrawArrays(GL_TRIANGLES, 0, 6); glBindVertexArray(0);
    // print numeric latency to stdout so we can capture it in logs
    std::cout<<"E2E:"<<latencyMs<<"ms STREAM:"<<(streamAlive?"OK":"NO_STREAM")<<"\n";

    glUseProgram(0);
}
