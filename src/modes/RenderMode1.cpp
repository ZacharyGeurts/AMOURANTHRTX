// src/modes/RenderMode1.cpp
// AMOURANTH RTX — MODE 1: ENVIRONMENT MAP ONLY
// FULLY MODULAR. FULLY SCALABLE. FULLY GLOWING.
// Keyboard key: 1

#include "modes/RenderMode1.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/RTConstants.hpp"
#include "engine/camera.hpp"          // ← REQUIRED
#include "engine/logging.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <format>

namespace VulkanRTX {

#define LOG_MODE1(...) LOG_DEBUG_CAT("RenderMode1", __VA_ARGS__)

void renderMode1(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
) {
    // === Extract from context ===
    int width  = context.swapchainExtent.width;
    int height = context.swapchainExtent.height;

    // ← FIXED: Use `context.camera`, NOT `::Vulkan::Context::camera`
    if (!context.camera) {
        //LOG_ERROR_CAT("RenderMode1", "context.camera is null!");
        return;
    }

    glm::vec3 camPos = context.camera->getPosition();
    float fov = context.camera->getFOV();
    float zoomLevel = 60.0f / fov;  // e.g. FOV 60 = 1.0x, FOV 30 = 2.0x

    LOG_MODE1("{}ENV MAP ONLY | {}x{} | zoom: {:.2f}x | FOV: {:.1f}°{}", 
              Logging::Color::ARCTIC_CYAN, width, height, zoomLevel, fov, Logging::Color::RESET);

    if (!context.enableRayTracing || !context.vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("RenderMode1", "Ray tracing not enabled or vkCmdTraceRaysKHR missing");
        return;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

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

    LOG_MODE1("{}DISPATCHED | 1 ray/pixel | env-only{}", Logging::Color::EMERALD_GREEN, Logging::Color::RESET);
}

} // namespace VulkanRTX