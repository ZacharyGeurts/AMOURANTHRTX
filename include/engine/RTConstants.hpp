// include/engine/RTConstants.hpp
// AMOURANTH RTX — Push Constants for Ray Tracing
// FULLY ALIGNED. FULLY PORTABLE. FULLY GLOWING.

#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace VulkanRTX {

#pragma pack(push, 1)

/**
 * @brief RTConstants — Push Constants for All Render Modes
 *
 * Size: 80 bytes (aligned for Vulkan push constant limits)
 * Used in raygen, miss, and compute shaders
 */
struct RTConstants {
    alignas(16) glm::vec4  clearColor      = glm::vec4(0.0f);
    alignas(16) glm::vec3  cameraPosition  = glm::vec3(0.0f);
    alignas(4)  float      _pad0           = 0.0f;

    alignas(16) glm::vec3  lightDirection  = glm::vec3(0.0f, -1.0f, 0.0f);
    alignas(4)  float      lightIntensity  = 1.0f;

    alignas(4)  uint32_t   samplesPerPixel = 1;
    alignas(4)  uint32_t   maxDepth        = 5;
    alignas(4)  uint32_t   maxBounces      = 3;
    alignas(4)  float      russianRoulette = 0.8f;

    alignas(8)  glm::vec2  resolution      = glm::vec2(1920, 1080);
    alignas(4)  uint32_t   showEnvMapOnly  = 0;
    alignas(4)  uint32_t   _pad1           = 0;
};

#pragma pack(pop)

static_assert(sizeof(RTConstants) == 80, "RTConstants must be 80 bytes");
static_assert(offsetof(RTConstants, clearColor)      == 0);
static_assert(offsetof(RTConstants, cameraPosition)  == 16);
static_assert(offsetof(RTConstants, lightDirection)  == 32);
static_assert(offsetof(RTConstants, samplesPerPixel) == 48);
static_assert(offsetof(RTConstants, resolution)      == 64);
static_assert(offsetof(RTConstants, showEnvMapOnly)  == 72);

} // namespace VulkanRTX