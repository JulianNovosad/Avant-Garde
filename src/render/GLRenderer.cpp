#include "GLRenderer.h"
#include <GLES3/gl32.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

GLRenderer::GLRenderer(){}
GLRenderer::~GLRenderer(){ shutdown(); }

unsigned int GLRenderer::compileShader(unsigned int type, const char* src){
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    int ok=0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok){ char log[1024]; glGetShaderInfoLog(s, 1024, NULL, log); std::cerr<<"Shader compile error: "<<log<<"\n"; }
    return s;
}

unsigned int GLRenderer::linkProgram(unsigned int vs, unsigned int fs){
    unsigned int p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    int ok=0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok){ char log[1024]; glGetProgramInfoLog(p, 1024, NULL, log); std::cerr<<"Program link error: "<<log<<"\n"; }
    glDetachShader(p, vs); glDetachShader(p, fs);
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

void GLRenderer::buildCube(){
    if (cubeVAO) return;
    float verts[] = {
        // positions        // normals
        -0.5f,-0.5f,-0.5f,  0,0,-1,
         0.5f,-0.5f,-0.5f,  0,0,-1,
         0.5f, 0.5f,-0.5f,  0,0,-1,
        -0.5f, 0.5f,-0.5f,  0,0,-1,
        -0.5f,-0.5f, 0.5f,  0,0,1,
         0.5f,-0.5f, 0.5f,  0,0,1,
         0.5f, 0.5f, 0.5f,  0,0,1,
        -0.5f, 0.5f, 0.5f,  0,0,1,
    };
    unsigned int idx[] = {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        0,4,7, 7,3,0,
        1,5,6, 6,2,1,
        3,2,6, 6,7,3,
        0,1,5, 5,4,0
    };
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glBindVertexArray(0);
}

bool GLRenderer::init(int w, int h){
    width=w; height=h;
    // compile node shader
    unsigned int vs = compileShader(GL_VERTEX_SHADER, Node3D::vertexShaderSrc());
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, Node3D::fragmentShaderSrc());
    shaderProg = linkProgram(vs, fs);
    uMVP_loc = glGetUniformLocation(shaderProg, "uMVP");
    uColor_loc = glGetUniformLocation(shaderProg, "uColor");
    buildLineResources();
    buildOverlayResources();
    buildCube();
    proj = glm::perspective(glm::radians(60.0f), (float)w/(float)h, 0.1f, 100.0f);
    view = camera.getViewMatrix();
    return true;
}

void GLRenderer::shutdown(){
    if (cubeVAO) glDeleteVertexArrays(1, &cubeVAO);
    if (cubeVBO) glDeleteBuffers(1, &cubeVBO);
    if (cubeEBO) glDeleteBuffers(1, &cubeEBO);
    if (shaderProg) glDeleteProgram(shaderProg);
    cubeVAO = cubeVBO = cubeEBO = shaderProg = 0;
}

void GLRenderer::update(float dt){
    if (scene) scene->update(dt);
    else for (auto &n: nodes) n->update(dt);
    camera.update(dt);
    backpressureLevel = std::min(1.0f, backpressureLevel); // clamp
}

void GLRenderer::render(){
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02f,0.02f,0.04f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProg);
    const std::vector<std::shared_ptr<Node3D>> *sourceNodes = nullptr;
    std::vector<std::shared_ptr<Node3D>> tempNodes;
    if (scene) { sourceNodes = &scene->nodes(); }
    else { tempNodes = nodes; sourceNodes = &tempNodes; }
    for (auto &n: *sourceNodes){
        glm::mat4 model = glm::translate(glm::mat4(1.0f), n->getPosition());
        model = glm::scale(model, glm::vec3(0.8f,0.8f,0.8f));
        view = camera.getViewMatrix();
        glm::mat4 mvp = proj * view * model;
        glUniformMatrix4fv(uMVP_loc, 1, GL_FALSE, &mvp[0][0]);
        glm::vec4 c = n->getColorVec4();
        glUniform4f(uColor_loc, c.r, c.g, c.b, c.a);
        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    // draw edges from scenegraph if present
    if (scene){
        glUseProgram(lineProg);
        int lineMVP = glGetUniformLocation(lineProg, "uMVP");
        int lineColor = glGetUniformLocation(lineProg, "uColor");
        for (auto &e: scene->edges()){
            glm::vec3 va = e->aPos(); glm::vec3 vb = e->bPos();
            float verts[6] = { va.x, va.y, va.z, vb.x, vb.y, vb.z };
            glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
            glBindVertexArray(lineVAO);
            glm::mat4 mvp = proj * camera.getViewMatrix() * glm::mat4(1.0f);
            glUniformMatrix4fv(lineMVP, 1, GL_FALSE, &mvp[0][0]);
            glm::vec4 col = e->color();
            glUniform4f(lineColor, col.r, col.g, col.b, col.a);
            glDrawArrays(GL_LINES, 0, 2);
            glBindVertexArray(0);
        }
        glUseProgram(0);
    }

    // backpressure overlay: draw a translucent border based on backpressureLevel
    if (backpressureLevel > 0.01f){
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // simple full-screen overlay influence by backpressure (no quad shader used for brevity)
        float alpha = backpressureLevel * 0.35f;
        glClearColor(0.04f * backpressureLevel, 0.02f, 0.02f, alpha);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_BLEND);
    }
    // render property card on top
    renderPropertyCard();
}

void GLRenderer::buildLineResources(){
    const char* vs = "#version 320 es\nlayout(location=0) in vec3 aPos; uniform mat4 uMVP; void main(){ gl_Position = uMVP * vec4(aPos,1.0); }";
    const char* fs = "#version 320 es\nprecision mediump float; uniform vec4 uColor; out vec4 frag; void main(){ frag = uColor; }";
    unsigned int v = compileShader(GL_VERTEX_SHADER, vs);
    unsigned int f = compileShader(GL_FRAGMENT_SHADER, fs);
    lineProg = linkProgram(v,f);
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glBindVertexArray(0);
}

void GLRenderer::buildOverlayResources(){
    const char* vs = "#version 320 es\nlayout(location=0) in vec2 aPos; uniform mat4 uOrtho; void main(){ gl_Position = uOrtho * vec4(aPos,0.0,1.0); }";
    const char* fs = "#version 320 es\nprecision mediump float; uniform vec4 uColor; out vec4 frag; void main(){ frag = uColor; }";
    unsigned int v = compileShader(GL_VERTEX_SHADER, vs);
    unsigned int f = compileShader(GL_FRAGMENT_SHADER, fs);
    overlayProg = linkProgram(v,f);
    glGenVertexArrays(1, &overlayVAO);
    glGenBuffers(1, &overlayVBO);
    glBindVertexArray(overlayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glBindVertexArray(0);
}

void GLRenderer::renderPropertyCard(){
    if (!scene) return;
    // find pinned node
    std::shared_ptr<Node3D> pinned;
    for (auto &n: scene->nodes()) if (n->isPinned()) { pinned = n; break; }
    if (!pinned) return;
    // render a top-right overlay rectangle
    float w = 320.0f, h = 160.0f; float margin = 20.0f;
    float x0 = (float)width - w - margin; float y0 = margin;
    float verts[12] = { x0, y0, x0+w, y0, x0+w, y0+h, x0, y0, x0+w, y0+h, x0, y0+h };
    // orthographic matrix
    glm::mat4 ortho = glm::ortho(0.0f, (float)width, 0.0f, (float)height);
    glUseProgram(overlayProg);
    int loc = glGetUniformLocation(overlayProg, "uOrtho");
    int col = glGetUniformLocation(overlayProg, "uColor");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &ortho[0][0]);
    glUniform4f(col, 0.05f, 0.05f, 0.08f, 0.85f);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glBindVertexArray(overlayVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
    // print details to stdout for now
    std::cout<<"PropertyCard: id="<<pinned->getId()<<" state="<<(int)pinned->getState()<<"\n";
}

// Project world position to screen coords and compare to click
std::string GLRenderer::pickNodeAtScreen(int sx, int sy, int winW, int winH){
    auto nodesRef = scene ? scene->nodes() : nodes;
    float bestD = 1e9f; std::string bestId;
    glm::mat4 vp = proj * camera.getViewMatrix();
    for (auto &n: nodesRef){
        glm::vec4 wp = vp * glm::vec4(n->getPosition(), 1.0f);
        if (wp.w==0.0f) continue;
        glm::vec3 ndc = glm::vec3(wp) / wp.w;
        // NDC to window coords
        float wx = (ndc.x * 0.5f + 0.5f) * winW;
        float wy = (1.0f - (ndc.y * 0.5f + 0.5f)) * winH; // flip Y
        float dx = wx - (float)sx; float dy = wy - (float)sy;
        float d2 = dx*dx + dy*dy;
        if (d2 < bestD){ bestD = d2; bestId = n->getId(); }
    }
    // threshold in pixels^2
    if (bestD < 400.0f) return bestId;
    return std::string();
}

void GLRenderer::pinNodeById(const std::string &id){
    if (!scene) return;
    auto n = scene->getNode(id);
    if (!n) return;
    // unpin others
    for (auto &nn : scene->nodes()) nn->setPinned(false);
    n->setPinned(true);
    // move camera target to node
    // CameraOrbitController expose internal target via pan/orbit; approximate by setting pan offset
    camera.pan(0.0f,0.0f);
}

void GLRenderer::setNodeStateById(const std::string &id, core::NodeState s){
    for (auto &n: nodes){ if (n->getId() == id){ n->setState(s); break; } }
}

void GLRenderer::addNode(const std::shared_ptr<Node3D>& node){ nodes.push_back(node); }

void GLRenderer::applyTelemetry(const glm::vec3& pos, uint8_t state){
    // find nearest node
    float bestD = 1e9f; int bestIdx = -1;
    for (int i=0;i<(int)nodes.size();++i){ float d = glm::distance(nodes[i]->getPosition(), pos); if (d < bestD){ bestD=d; bestIdx=i; }}
    if (bestIdx>=0){
        core::NodeState ns = core::NodeState::OFFLINE;
        switch(state){ case 1: ns = core::NodeState::ACTIVE; break; case 2: ns = core::NodeState::DEGRADED; break; case 3: ns = core::NodeState::FALLBACK; break; default: ns = core::NodeState::OFFLINE; }
        nodes[bestIdx]->setState(ns);
    }
}

// (setNodeStateById already defined earlier)
