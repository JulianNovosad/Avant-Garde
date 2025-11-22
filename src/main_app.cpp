#include "net/UdpReceiver.h"
#include "render/GLRenderer.h"
#include <glm/vec3.hpp>
#include <memory>
#include <chrono>
#include <thread>
#include <iostream>
#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "net/BackpressureDetector.h"
#include "net/TCPClient.h"
#include "net/WebSocketClient.h"
#include <rapidjson/document.h>

static TouchController g_touch;

static void cursor_pos_cb(GLFWwindow* w, double xpos, double ypos){
    g_touch.onMouseMove((int)xpos, (int)ypos);
}
static void mouse_button_cb(GLFWwindow* w, int button, int action, int mods){
    double x,y; glfwGetCursorPos(w,&x,&y);
    if (action==GLFW_PRESS) g_touch.onMouseDown((int)x,(int)y, button==GLFW_MOUSE_BUTTON_LEFT?1:2);
    else g_touch.onMouseUp((int)x,(int)y, button==GLFW_MOUSE_BUTTON_LEFT?1:2);
}
static void scroll_cb(GLFWwindow* w, double xoff, double yoff){ g_touch.onScroll((float)yoff); }

int main(int argc, char** argv){
    if (!glfwInit()){ std::cerr<<"GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(1280,720, "Blacknode - Desktop", NULL, NULL);
    if (!win){ glfwTerminate(); std::cerr<<"Window creation failed\n"; return -1; }
    glfwMakeContextCurrent(win);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){ std::cerr<<"GLAD init failed\n"; return -1; }
    glfwSetCursorPosCallback(win, cursor_pos_cb);
    glfwSetMouseButtonCallback(win, mouse_button_cb);
    glfwSetScrollCallback(win, scroll_cb);

    // Desktop test harness: create renderer + nodes + UDP receiver
    GLRenderer rend;
    rend.init(1280,720);
    // create SceneGraph and named cluster nodes
    auto scene = std::make_shared<SceneGraph>();
    std::vector<std::string> names = {"raspberrypi","camera","preprocess","tpu","postprocess","tracking","telemetry","stream","renderer","cache","network","ui"};
    float x=0.0f, z=0.0f;
    for (size_t i=0;i<names.size();++i){
        if (i%4==0){ x = -3.0f; z += 1.6f; } else x += 1.6f;
        scene->addNode(names[i], glm::vec3(x, 0.0f, z));
    }
    // connect edges from raspberrypi to modules
    for (size_t i=1;i<names.size();++i) scene->addEdge("raspberrypi", names[i]);
    rend.setSceneGraph(scene);

    UdpReceiver udp;
    BackpressureDetector bp;
    udp.setCallback([&](const TelemetryPacket &p){
        glm::vec3 pos(p.position[0], p.position[1], p.position[2]);
        rend.applyTelemetry(pos, p.state);
        bp.notifyPacket();
    });
    if (!udp.start(40321)) std::cerr<<"UDP start failed\n";

    // TCP client for JSON node configs (port 40322)
    TCPClient tcp;
    tcp.setMessageCallback([&](const std::string &msg){
        rapidjson::Document d;
        d.Parse(msg.c_str());
        if (d.HasParseError()) return;
        if (!d.HasMember("node") || !d.HasMember("state")) return;
        std::string nodeName = d["node"].GetString();
        std::string stateStr = d["state"].GetString();
        core::NodeState ns = core::NodeState::OFFLINE;
        if (stateStr == "active") ns = core::NodeState::ACTIVE;
        else if (stateStr == "degraded") ns = core::NodeState::DEGRADED;
        else if (stateStr == "fallback") ns = core::NodeState::FALLBACK;
        rend.setNodeStateById(nodeName, ns);
    });
    tcp.start("raspberrypi.local", 40322);

    // WebSocket client for cluster updates
    WebSocketClient ws;
    ws.setMessageCallback([&](const std::string &msg){
        rapidjson::Document d;
        d.Parse(msg.c_str());
        if (d.HasParseError()) return;
        if (!d.HasMember("event")) return;
        std::string ev = d["event"].GetString();
        if (ev == "cluster_update" && d.HasMember("nodes") && d["nodes"].IsArray()){
            for (auto &n : d["nodes"].GetArray()){
                if (!n.HasMember("id") || !n.HasMember("state")) continue;
                std::string id = n["id"].GetString();
                std::string st = n["state"].GetString();
                core::NodeState ns = core::NodeState::OFFLINE;
                if (st=="active") ns = core::NodeState::ACTIVE;
                else if (st=="degraded") ns = core::NodeState::DEGRADED;
                else if (st=="fallback") ns = core::NodeState::FALLBACK;
                rend.setNodeStateById(id, ns);
            }
        }
    });
    ws.start("ws://raspberrypi.local:8087");

    auto last = std::chrono::steady_clock::now();
    while (!glfwWindowShouldClose(win)){
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now-last).count();
        last = now;
        bp.update();
        rend.update(dt);
        rend.setBackpressureLevel(bp.intensity());
        if (scene) scene->setEdgePulse(bp.intensity());
        // feed touch deltas into camera
        rend.cameraRef().orbit(g_touch.getDeltaX(), g_touch.getDeltaY());
        rend.cameraRef().pan(g_touch.getDeltaX()*0.01f, g_touch.getDeltaY()*0.01f);
        // apply pinch delta if present
        float pdelta = g_touch.getPinchDelta();
        if (fabs(pdelta) > 1e-6f) rend.cameraRef().zoom(pdelta*10.0f);
        else rend.cameraRef().zoom(g_touch.getZoomDelta());
        // detect hold -> pin node
        int hx=0, hy=0;
        if (g_touch.pollHold(hx, hy)){
            std::string picked = rend.pickNodeAtScreen(hx, hy, 1280, 720);
            if (!picked.empty()){ rend.pinNodeById(picked); std::cout<<"Pinned node: "<<picked<<"\n"; }
        }
        g_touch.resetDeltas();
        glViewport(0,0,1280,720);
        rend.render();
        glfwSwapBuffers(win);
        glfwPollEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    ws.stop(); tcp.stop(); udp.stop();
    glfwDestroyWindow(win); glfwTerminate();
    return 0;
}
