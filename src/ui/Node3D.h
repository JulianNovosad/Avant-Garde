#pragma once
#include <glm/glm.hpp>
#include <string>
#include "../core/StateMachine.hpp"

class Node3D {
public:
    Node3D(const std::string &id, const glm::vec3 &pos);
    void update(float dt);
    void setState(core::NodeState s);
    core::NodeState getState() const;
    const glm::vec3& getPosition() const { return position; }
    const std::string& getId() const { return id; }
    glm::vec4 getColorVec4() const;
    void setPinned(bool v){ pinned = v; }
    bool isPinned() const { return pinned; }

    // Shaders (inline)
    static const char* vertexShaderSrc();
    static const char* fragmentShaderSrc();

private:
    std::string id;
    glm::vec3 position;
    core::StateMachine sm;
    bool pinned = false;
};
