#pragma once
#include <glm/glm.hpp>

namespace MaterialData {
struct PushConstants {
    alignas(16) glm::vec4 clearColor;
    alignas(16) glm::vec3 cameraPos;
    alignas(16) glm::vec3 lightPos;
    alignas(16) glm::vec3 lightColor;
    alignas(4)  float    lightIntensity;
    alignas(4)  uint32_t samplesPerPixel;
    alignas(4)  uint32_t maxDepth;
    alignas(4)  uint32_t maxBounces;
    alignas(4)  float    russianRoulette;
    alignas(8)  glm::vec2 resolution;   // used by ray-tracing
};
} // namespace MaterialData