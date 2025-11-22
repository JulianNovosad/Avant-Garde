#include "Node3D.h"
#include <glm/gtc/type_ptr.hpp>

Node3D::Node3D(const std::string &id_, const glm::vec3 &pos)
    : id(id_), position(pos)
{
    sm.forceState(core::NodeState::OFFLINE);
}

void Node3D::update(float dt){
    sm.update(dt);
}

void Node3D::setState(core::NodeState s){ sm.setState(s); }
core::NodeState Node3D::getState() const { return sm.getState(); }

glm::vec4 Node3D::getColorVec4() const{
    auto c = sm.getColor();
    if (pinned) {
        // highlight pinned node with cyan tint
        return glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
    }
    return glm::vec4(c.r,c.g,c.b,c.a);
}

const char* Node3D::vertexShaderSrc(){
    return R"GLSL(#version 320 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMVP;
out vec3 vNormal;
void main(){ vNormal = aNormal; gl_Position = uMVP * vec4(aPos,1.0); }
)GLSL";
}

const char* Node3D::fragmentShaderSrc(){
    return R"GLSL(#version 320 es
precision mediump float;
in vec3 vNormal;
uniform vec4 uColor;
out vec4 fragColor;
void main(){
    float ndl = max(dot(normalize(vNormal), vec3(0.0,0.0,1.0)), 0.15);
    fragColor = vec4(uColor.rgb * ndl, uColor.a);
}
)GLSL";
}
