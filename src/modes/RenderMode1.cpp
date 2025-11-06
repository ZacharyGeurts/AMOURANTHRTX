// src/modes/RenderMode1.cpp
// AMOURANTH RTX — MODE 1: ENVIRONMENT MAP ONLY
// FINAL FIX: context.camera null → FALLBACK DISPATCH + NO SKIP
// LAZY CAMERA = OPTIONAL, MODE 1 = ALWAYS WORKS
// Keyboard key: 1 → Full env map, even without camera

#include "modes/RenderMode1.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/RTConstants.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <format>

namespace VulkanRTX {

using namespace Logging::Color;
#define LOG_MODE1(...) LOG_INFO_CAT("RenderMode1", __VA_ARGS__)

void renderMode1(
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

    // === RAY TRACING VALIDATION ===
    if (!context.enableRayTracing || !context.vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("RenderMode1", "Ray tracing not enabled or vkCmdTraceRaysKHR missing");
        return;
    }

    // === CAMERA STATE (OPTIONAL) ===
    glm::vec3 camPos = glm::vec3(0.0f, 0.0f, 5.0f);
    float fov = 60.0f;
    float zoomLevel = 1.0f;

    if (context.camera) {
        auto* cam = static_cast<PerspectiveCamera*>(context.camera);
        camPos = cam->getPosition();
        fov = cam->getFOV();
        zoomLevel = 60.0f / fov;

        LOG_MODE1("{}ENV MAP ONLY | {}x{} | pos: ({:.2f}, {:.2f}, {:.2f}) | FOV: {:.1f}° | zoom: {:.2f}x{}", 
                  ARCTIC_CYAN, width, height, 
                  camPos.x, camPos.y, camPos.z,
                  fov, zoomLevel, RESET);
    } else {
        LOG_MODE1("{}ENV MAP ONLY | {}x{} | fallback pos (0,0,5) | FOV: 60.0°{}", 
                  ARCTIC_CYAN, width, height, RESET);
    }

    // === BIND PIPELINE & DESCRIPTORS ===
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // === PUSH CONSTANTS (WITH ZOOM OFFSET) ===
    RTConstants push{};
    push.clearColor      = glm::vec4(0.02f, 0.02f, 0.05f, 1.0f);
    push.cameraPosition  = camPos + glm::vec3(0.0f, 0.0f, 5.0f * (zoomLevel - 1.0f));
    push._pad0           = 0.0f;
    push.lightDirection  = glm::vec3(0.0f, -1.0f, 0.0f);
    push.lightIntensity  = 8.0f;
    push.samplesPerPixel = 1;
    push.maxDepth        = 1;
    push.maxBounces      = 0;
    push.russianRoulette = 0.0f;
    push.resolution      = glm::vec2(width, height);
    push.showEnvMapOnly  = 1;

    vkCmdPushConstants(commandBuffer, pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
        0, sizeof(RTConstants), &push);

    // === SBT REGIONS ===
    VkStridedDeviceAddressRegionKHR raygen = {
        .deviceAddress = context.raygenSbtAddress,
        .stride        = context.sbtRecordSize,
        .size          = context.sbtRecordSize
    };
    VkStridedDeviceAddressRegionKHR miss = {
        .deviceAddress = context.missSbtAddress,
        .stride        = context.sbtRecordSize,
        .size          = context.sbtRecordSize
    };
    VkStridedDeviceAddressRegionKHR hit = {};
    VkStridedDeviceAddressRegionKHR callable = {};

    // === DISPATCH RAYS ===
    context.vkCmdTraceRaysKHR(
        commandBuffer,
        &raygen,
        &miss,
        &hit,
        &callable,
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        1
    );

    LOG_MODE1("{}DISPATCHED | 1 ray/pixel | env-only | {}WASD + Mouse + Scroll{}", 
              EMERALD_GREEN,
              context.camera ? "" : "fallback | ",
              RESET);
}

} // namespace VulkanRTX