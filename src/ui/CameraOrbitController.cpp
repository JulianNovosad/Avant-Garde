#include "CameraOrbitController.h"
#include <glm/gtc/matrix_transform.hpp>

CameraOrbitController::CameraOrbitController()
    : yaw(0.0f), pitch(-0.3f), distance(8.0f), target(0.0f), offset(0.0f), yawVel(0), pitchVel(0), distVel(0)
{}

void CameraOrbitController::orbit(float dx, float dy){
    yaw += dx * 0.01f;
    pitch += dy * 0.01f;
    if (pitch > 1.4f) pitch = 1.4f; if (pitch < -1.4f) pitch = -1.4f;
}

void CameraOrbitController::pan(float dx, float dy){
    // move target in view plane
    glm::vec3 right = glm::vec3(cos(yaw), 0, -sin(yaw));
    glm::vec3 up = glm::vec3(0,1,0);
    offset += (-right * dx * 0.01f) + (up * dy * 0.01f);
}

void CameraOrbitController::zoom(float dz){ distance += dz * -0.02f; if (distance < 1.0f) distance = 1.0f; if (distance > 50.0f) distance = 50.0f; }

glm::mat4 CameraOrbitController::getViewMatrix() const{
    glm::vec3 pos = target + offset + glm::vec3(sin(yaw)*cos(pitch)*distance, sin(pitch)*distance, cos(yaw)*cos(pitch)*distance);
    return glm::lookAt(pos, target + offset, glm::vec3(0,1,0));
}

void CameraOrbitController::update(float dt){ /* inertia can be added */ }
