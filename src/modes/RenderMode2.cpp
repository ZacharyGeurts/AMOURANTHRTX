// src/modes/RenderMode2.cpp
// AMOURANTH RTX — MODE 2: RTX CORE + PATH TRACED DIFFUSE
// CAMERA = ON | FULL RTX | ZOOM OFFSET | NO NULL CRASH
// Keyboard key: 2 → Full path-traced scene with dynamic camera

#include "modes/RenderMode2.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/RTConstants.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>

namespace VulkanRTX {

using namespace Logging::Color;
#define LOG_MODE2(...) LOG_INFO_CAT("RenderMode2", __VA_ARGS__)

void renderMode2(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
) {
    const int width  = context.swapchainExtent.width;
    const int height = context.swapchainExtent.height;

    // === CAMERA: SAFE ACCESS + FALLBACK ===
    glm::vec3 camPos = glm::vec3(0.0f, 0.0f, 5.0f);
    float fov = 60.0f;
    float zoomLevel = 1.0f;

    if (context.camera) {
        auto* cam = static_cast<PerspectiveCamera*>(context.camera);
        camPos = cam->getPosition();
        fov = cam->getFOV();
        zoomLevel = 60.0f / fov;

        LOG_MODE2("{}RTX CORE | {}x{} | pos: ({:.2f}, {:.2f}, {:.2f}) | FOV: {:.1f}° | zoom: {:.2f}x{}", 
                  OCEAN_TEAL, width, height, 
                  camPos.x, camPos.y, camPos.z,
                  fov, zoomLevel, RESET);
    } else {
        LOG_MODE2("{}RTX CORE | {}x{} | fallback pos (0,0,5) | FOV: 60.0°{}", 
                  OCEAN_TEAL, width, height, RESET);
    }

    // === VALIDATE RTX ===
    if (!context.enableRayTracing || !context.vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("RenderMode2", "Ray tracing not enabled or vkCmdTraceRaysKHR missing");
        return;
    }

    // === BIND ===
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // === PUSH CONSTANTS WITH ZOOM OFFSET ===
    RTConstants push{};
    push.clearColor        = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    push.cameraPosition    = camPos + glm::vec3(0.0f, 0.0f, 5.0f * (zoomLevel - 1.0f));
    push._pad0             = 0.0f;
    push.lightDirection    = glm::normalize(glm::vec3(1.0f, -1.0f, 0.5f));
    push.lightIntensity    = 12.0f;
    push.samplesPerPixel   = 1;
    push.maxDepth          = 4;
    push.maxBounces        = 4;
    push.russianRoulette   = 0.8f;
    push.resolution        = glm::vec2(width, height);
    push.showEnvMapOnly    = 0;
    push.frame             = imageIndex;
    push.fireflyClamp      = 10.0f;

    vkCmdPushConstants(commandBuffer, pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0, sizeof(RTConstants), &push);

    // === SBT REGIONS ===
    const VkStridedDeviceAddressRegionKHR raygen = {
        .deviceAddress = context.raygenSbtAddress,
        .stride        = context.sbtRecordSize,
        .size          = context.sbtRecordSize
    };
    const VkStridedDeviceAddressRegionKHR miss = {
        .deviceAddress = context.missSbtAddress,
        .stride        = context.sbtRecordSize,
        .size          = context.sbtRecordSize * 2
    };
    const VkStridedDeviceAddressRegionKHR hit = {
        .deviceAddress = context.hitSbtAddress,
        .stride        = context.sbtRecordSize,
        .size          = context.sbtRecordSize * 1
    };
    const VkStridedDeviceAddressRegionKHR callable = {};

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

    LOG_MODE2("{}RTX DISPATCHED | 1 SPP | 4 bounces | firefly clamp = 10.0 | WASD + Mouse + Scroll{}", 
              EMERALD_GREEN, RESET);
}

} // namespace VulkanRTX