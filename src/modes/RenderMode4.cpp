// src/modes/RenderMode4.cpp
// AMOURANTH RTX — MODE 4: VOLUMETRIC FOG + GOD RAYS
// FINAL: Screen-space + ray-marched volumetric fog with animated light shaft
// LAZY CAMERA = OPTIONAL, MODE 4 = ALWAYS WORKS
// Keyboard key: 4 → Render full-screen volumetric fog with god rays
// FEATURES:
//   • No geometry — pure post-process ray marching
//   • Animated directional light (sun)
//   • Density noise (procedural)
//   • Phase function (Henyey-Greenstein)
//   • High sample count for smooth fog
//   • Camera depth integration
//   • [[assume]] + C++23

#include "modes/RenderMode4.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <format>
#include <vector>
#include <expected>
#include <bit>
#include <cmath>
#include <GLFW/glfw3.h>  // <-- Added for glfwGetTime()

namespace VulkanRTX {

using namespace Logging::Color;
#define LOG_MODE4(...) LOG_INFO_CAT("RenderMode4", __VA_ARGS__)

// ---------------------------------------------------------------------
// Render Mode 4 Entry Point
// ---------------------------------------------------------------------
void renderMode4(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
) {
    int width  = context.swapchainExtent.width;
    int height = context.swapchainExtent.height;

    auto* rtx = context.getRTX();
    if (!rtx || !context.enableRayTracing || !context.vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("RenderMode4", "RTX not available");
        return;
    }

    // === CAMERA (LAZY: fallback if no camera) ===
    glm::vec3 camPos(0.0f, 1.0f, 5.0f);
    glm::vec3 camDir(0.0f, 0.0f, -1.0f);
    float fov = 60.0f;
    if (auto* cam = context.getCamera(); cam) {
        camPos = cam->getPosition();
        camDir = cam->getViewMatrix()[2];  // Forward = -z
        fov = cam->getFOV();
    }

    // === ANIMATED SUN ===
    static float sunAngle = 0.0f;
    sunAngle += deltaTime * 0.3f;
    if (sunAngle > glm::two_pi<float>()) sunAngle -= glm::two_pi<float>();

    glm::vec3 sunDir = glm::normalize(glm::vec3(
        std::cos(sunAngle) * 0.8f,
        std::sin(sunAngle) * 0.6f + 0.4f,
        std::sin(sunAngle) * 0.8f
    ));

    // === BIND ===
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // === PUSH CONSTANTS: Volumetric Fog + God Rays ===
    RTConstants push{};
    push.clearColor       = glm::vec4(0.05f, 0.07f, 0.12f, 1.0f);
    push.cameraPosition   = camPos;
    push.lightDirection   = glm::vec4(sunDir, 0.0f);  // w=0 → directional
    push.lightIntensity   = 20.0f;
    push.resolution       = glm::vec2(width, height);

    // Fog parameters
    push.fogDensity       = 0.08f;
    push.fogHeightFalloff = 0.15f;
    push.fogScattering    = 0.9f;
    push.phaseG           = 0.76f;  // Forward scattering
    push.samplesPerPixel  = 6;
    push.maxDepth         = 1;
    push.maxBounces       = 0;
    push.showEnvMapOnly   = 0;
    push.volumetricMode   = 1;  // Enable fog

    // Time for noise animation
    push.time             = static_cast<float>(glfwGetTime());

    vkCmdPushConstants(commandBuffer, pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
        0, sizeof(RTConstants), &push);

    // === SBT (no hit shaders needed) ===
    VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
    VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
    VkStridedDeviceAddressRegionKHR hit    = {};
    VkStridedDeviceAddressRegionKHR callable = {};

    // === TRACE ===
    context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable,
                              static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1);

    LOG_MODE4("{}DISPATCHED | 6 spp | Volumetric fog | Sun @ ({:.2f}, {:.2f}, {:.2f}){}",
              EMERALD_GREEN, sunDir.x, sunDir.y, sunDir.z, RESET);
}

} // namespace VulkanRTX