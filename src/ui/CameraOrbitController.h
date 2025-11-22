#pragma once
#include <glm/glm.hpp>

class CameraOrbitController {
public:
    CameraOrbitController();
    void orbit(float dx, float dy);
    void pan(float dx, float dy);
    void zoom(float dz);
    glm::mat4 getViewMatrix() const;
    void update(float dt);
private:
    float yaw; // radians
    float pitch;
    float distance;
    glm::vec3 target;
    glm::vec3 offset; // pan offset
    float yawVel, pitchVel, distVel;
};
