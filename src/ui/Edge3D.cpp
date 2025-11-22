#include "Edge3D.h"
#include <cmath>

Edge3D::Edge3D(const std::string &a, const std::string &b)
    : idA(a), idB(b), pA(0.0f), pB(0.0f), pulse(0.0f) {}

void Edge3D::setPositions(const glm::vec3 &pa, const glm::vec3 &pb){ pA = pa; pB = pb; }

void Edge3D::setPulse(float v){ pulse = v; alpha = 0.3f + 0.7f * glm::clamp(v, 0.0f, 1.0f); }

void Edge3D::update(float dt){
    static float phase = 0.0f;
    phase += dt * 2.0f;
    if (phase > 6.2831853f) phase -= 6.2831853f;
    if (pulse > 0.001f){ alpha = 0.3f + 0.7f * pulse * (0.8f + 0.2f * sinf(phase)); }
}
