#pragma once
#include <glm/glm.hpp>
#include <string>

class Edge3D {
public:
    Edge3D(const std::string &a, const std::string &b);
    void setPositions(const glm::vec3 &pa, const glm::vec3 &pb);
    void setPulse(float v);
    void update(float dt);
    glm::vec3 aPos() const { return pA; }
    glm::vec3 bPos() const { return pB; }
    glm::vec4 color() const { return glm::vec4(r,g,b,alpha); }
    const std::string& aId() const { return idA; }
    const std::string& bId() const { return idB; }
private:
    std::string idA, idB;
    glm::vec3 pA, pB;
    float r=0.6f, g=0.8f, b=1.0f, alpha=0.6f;
    float pulse = 0.0f;
};
