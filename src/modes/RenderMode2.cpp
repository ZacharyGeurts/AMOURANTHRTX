// src/modes/RenderMode2.cpp
// AMOURANTH RTX — MODE 2: BASIC PATH TRACING
// FULLY MODULAR. SINGLE SPHERE. 2 BOUNCES. ENV LIT.

#include "modes/RenderMode2.hpp"
#include "engine/Vulkan/VulkanCore.hpp"  // Full Vulkan::Context
#include "engine/RTConstants.hpp"
#include "engine/logging.hpp"

#include <glm/gtc/constants.hpp>
#include <format>

namespace VulkanRTX {

#define LOG_MODE2(...) LOG_DEBUG_CAT("RenderMode2", __VA_ARGS__)

void renderMode2(
    [[maybe_unused]] uint32_t imageIndex,
    [[maybe_unused]] VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    [[maybe_unused]] VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    [[maybe_unused]] float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    [[maybe_unused]] float deltaTime,
    Vulkan::Context& context
) {
    LOG_MODE2("{}BASIC PATH | {}x{} | zoom: {:.2f} | sphere + env{}", 
              Logging::Color::ARCTIC_CYAN, width, height, zoomLevel, Logging::Color::RESET);

    if (!context.enableRayTracing || !context.vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("RenderMode2", "Ray tracing not enabled or vkCmdTraceRaysKHR missing");
        return;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // === PUSH CONSTANTS ===
    RTConstants push{};
    push.clearColor      = glm::vec4(0.02f, 0.02f, 0.05f, 1.0f);
    push.cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel);
    push._pad0           = 0.0f;
    push.lightDirection  = glm::vec3(0.0f, -1.0f, 0.0f);
    push.lightIntensity  = 8.0f;
    push.samplesPerPixel = 4;  // MODE 2: 4 SPP
    push.maxDepth        = 2;  // MODE 2: 2 bounces
    push.maxBounces      = 2;
    push.russianRoulette = 0.8f;
    push.resolution      = glm::vec2(width, height);
    push.showEnvMapOnly  = 0;  // Full tracing

    vkCmdPushConstants(commandBuffer, pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
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
    VkStridedDeviceAddressRegionKHR hit = {
        .deviceAddress = context.hitSbtAddress,
        .stride        = context.sbtRecordSize,
        .size          = context.sbtRecordSize
    };
    VkStridedDeviceAddressRegionKHR callable = {};

    // === DISPATCH ===
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

    LOG_MODE2("{}DISPATCHED | 4 spp | 2 bounces | sphere glow{}", Logging::Color::EMERALD_GREEN, Logging::Color::RESET);
}

} // namespace VulkanRTX