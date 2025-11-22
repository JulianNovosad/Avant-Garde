#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "../ui/Node3D.h"
#include "../ui/TouchController.h"
#include "../ui/CameraOrbitController.h"
#include "../ui/SceneGraph.h"

class GLRenderer {
public:
    GLRenderer();
    ~GLRenderer();
    bool init(int w, int h);
    void shutdown();
    void update(float dt);
    void render();
    void addNode(const std::shared_ptr<Node3D>& node);
    // find nearest node to position and set its state
    void applyTelemetry(const glm::vec3& pos, uint8_t state);
    // set node state by id (exact match)
    void setNodeStateById(const std::string &id, core::NodeState s);
    void setBackpressureLevel(float v){ backpressureLevel = v; }
    // expose camera control for the desktop harness
    CameraOrbitController &cameraRef() { return camera; }
    // expose scene graph control and picking/pinning for the harness
    void setSceneGraph(const std::shared_ptr<SceneGraph>& sg){ scene = sg; }
    std::string pickNodeAtScreen(int sx, int sy, int winW, int winH);
    void pinNodeById(const std::string &id);

private:
    int width=0, height=0;
    std::vector<std::shared_ptr<Node3D>> nodes;
    unsigned int cubeVAO=0, cubeVBO=0, cubeEBO=0;
    unsigned int shaderProg=0;
    unsigned int lineProg=0;
    unsigned int lineVAO=0, lineVBO=0;
    std::shared_ptr<SceneGraph> scene;
    unsigned int overlayProg=0;
    unsigned int overlayVAO=0, overlayVBO=0;
    glm::mat4 proj, view;
    int uMVP_loc = -1;
    int uColor_loc = -1;
    TouchController touch;
    CameraOrbitController camera;
    float backpressureLevel = 0.0f;
    void buildCube();
    void buildLineResources();
    void buildOverlayResources();
    void renderPropertyCard();
    // pick node at screen coords (window coords origin top-left)
    unsigned int compileShader(unsigned int type, const char* src);
    unsigned int linkProgram(unsigned int vs, unsigned int fs);
    
};
