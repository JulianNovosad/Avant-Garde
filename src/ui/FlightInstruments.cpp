#include "FlightInstruments.h"
#include "../core/NativeBridge.h"
#include <GLES3/gl32.h>
#include <rapidjson/document.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <cmath>

FlightInstruments::FlightInstruments(){}
FlightInstruments::~FlightInstruments(){ shutdown(); }

unsigned int FlightInstruments::compileShader(unsigned int type, const char* src){ unsigned int s = glCreateShader(type); glShaderSource(s,1,&src,NULL); glCompileShader(s); int ok=0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok); if(!ok){ char log[1024]; glGetShaderInfoLog(s,1024,NULL,log); std::cerr<<"FI shader compile: "<<log<<"\n"; } return s; }
unsigned int FlightInstruments::linkProgram(unsigned int vs, unsigned int fs){ unsigned int p = glCreateProgram(); glAttachShader(p,vs); glAttachShader(p,fs); glLinkProgram(p); int ok=0; glGetProgramiv(p, GL_LINK_STATUS, &ok); if(!ok){ char log[1024]; glGetProgramInfoLog(p,1024,NULL,log); std::cerr<<"FI prog link: "<<log<<"\n"; } glDeleteShader(vs); glDeleteShader(fs); return p; }

void FlightInstruments::buildShader(){ if (prog) return; const char* vs = "#version 330 core\nlayout(location=0) in vec2 aPos; uniform mat4 uOrtho; void main(){ gl_Position = uOrtho * vec4(aPos,0.0,1.0); }"; const char* fs = "#version 330 core\nuniform vec4 uColor; out vec4 frag; void main(){ frag = uColor; }"; unsigned int v = compileShader(GL_VERTEX_SHADER, vs); unsigned int f = compileShader(GL_FRAGMENT_SHADER, fs); prog = linkProgram(v,f); uOrtho = glGetUniformLocation(prog, "uOrtho"); uColor = glGetUniformLocation(prog, "uColor"); glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo); }

bool FlightInstruments::init(int w, int h){ width=w; height=h; buildShader();
    // subscribe to udp_telemetry
    subId = core::NativeBridge::subscribe("udp_telemetry", [&](const std::string &topic, const std::string &payload){
        rapidjson::Document d; d.Parse(payload.c_str()); if (d.HasParseError()) return;
        std::lock_guard<std::mutex> lk(mtx);
        if (d.HasMember("pitch") && d["pitch"].IsNumber()) target.pitch = d["pitch"].GetFloat();
        if (d.HasMember("roll") && d["roll"].IsNumber()) target.roll = d["roll"].GetFloat();
        if (d.HasMember("alt") && d["alt"].IsNumber()) target.altitude = d["alt"].GetFloat();
        if (d.HasMember("speed") && d["speed"].IsNumber()) target.speed = d["speed"].GetFloat();
    });
    return true;
}

void FlightInstruments::shutdown(){ if (subId!=-1) core::NativeBridge::unsubscribe("udp_telemetry", subId); if (vao) glDeleteVertexArrays(1,&vao); if (vbo) glDeleteBuffers(1,&vbo); if (prog) glDeleteProgram(prog); vao=vbo=prog=0; }

void FlightInstruments::update(float dt){ // smooth towards target over ~0.2s
    const float tau = 0.2f; float a = dt / (tau + 1e-6f); if (a>1.0f) a=1.0f;
    std::lock_guard<std::mutex> lk(mtx);
    disp.pitch += (target.pitch - disp.pitch) * a;
    disp.roll += (target.roll - disp.roll) * a;
    disp.altitude += (target.altitude - disp.altitude) * a;
    disp.speed += (target.speed - disp.speed) * a;
}

void FlightInstruments::render(){ if (!prog) return; glm::mat4 ortho = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    glUseProgram(prog); glUniformMatrix4fv(uOrtho,1,GL_FALSE,&ortho[0][0]);
    // Draw pitch ladder centered vertically: a few horizontal lines offset by pitch
    float cx = width * 0.5f; float midY = height * 0.5f;
    std::vector<float> verts;
    { std::lock_guard<std::mutex> lk(mtx); float p = disp.pitch; // degrees
        for (int i=-4;i<=4;i++){
            float y = midY + (i * 20.0f) - p * 2.0f; // scale pitch to pixels
            float x0 = cx - 200.0f; float x1 = cx + 200.0f;
            verts.push_back(x0); verts.push_back(y); verts.push_back(x1); verts.push_back(y);
        }
    }
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_DYNAMIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glUniform4f(uColor, 0.0f, 1.0f, 0.0f, 1.0f);
    glDrawArrays(GL_LINES, 0, (GLsizei)(verts.size()/2));
    glBindVertexArray(0);

    // Left data tape: altitude
    float bx = 12.0f, by = 120.0f, bw = 80.0f, bh = 200.0f;
    float rect[12] = { bx,by, bx+bw,by, bx+bw,by+bh, bx,by, bx+bw,by+bh, bx,by+bh };
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_DYNAMIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glUniform4f(uColor, 0.0f, 1.0f, 0.0f, 0.25f); glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Right data tape: speed
    float rx = width - 12.0f - bw, ry = by;
    float rect2[12] = { rx,ry, rx+bw,ry, rx+bw,ry+bh, rx,ry, rx+bw,ry+bh, rx,ry+bh };
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); glBufferData(GL_ARRAY_BUFFER, sizeof(rect2), rect2, GL_DYNAMIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glUniform4f(uColor, 0.0f, 1.0f, 0.0f, 0.25f); glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glUseProgram(0);
}
