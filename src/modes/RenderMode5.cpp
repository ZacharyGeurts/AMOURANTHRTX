// src/modes/RenderMode5.cpp
// AMOURANTH RTX — MODE 5: GLOSSY REFLECTIONS + METALNESS
// CAMERA = ON | ZOOM OFFSET | FALLBACK SAFE | FULL LOGGING
// Keyboard key: 5 → Mirror-like reflections, metallic surfaces, sharp highlights

#include "modes/RenderMode5.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/RTConstants.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>

namespace VulkanRTX {

using namespace Logging::Color;
#define LOG_MODE5(...) LOG_INFO_CAT("RenderMode5", __VA_ARGS__)

void renderMode5(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
) {
    const int w = context.swapchainExtent.width;
    const int h = context.swapchainExtent.height;

    // === CAMERA: SAFE + ZOOM + FALLBACK ===
    glm::vec3 camPos = glm::vec3(0.0f, 0.0f, 5.0f);
    float fov = 60.0f;
    float zoomLevel = 1.0f;

    if (context.camera) {
        auto* cam = static_cast<PerspectiveCamera*>(context.camera);
        camPos = cam->getPosition();
        fov = cam->getFOV();
        zoomLevel = 60.0f / fov;

        LOG_MODE5("{}GLOSSY + METAL | {}x{} | pos: ({:.2f}, {:.2f}, {:.2f}) | FOV: {:.1f} degrees | zoom: {:.2f}x{}", 
                  BRIGHT_PINKISH_PURPLE, w, h, 
                  camPos.x, camPos.y, camPos.z,
                  fov, zoomLevel, RESET);
    } else {
        LOG_MODE5("{}GLOSSY + METAL | {}x{} | fallback pos (0,0,5) | FOV: 60.0 degrees{}", 
                  BRIGHT_PINKISH_PURPLE, w, h, RESET);
    }

    // === RTX VALIDATION ===
    if (!context.enableRayTracing || !context.vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("RenderMode5", "Ray tracing not enabled or vkCmdTraceRaysKHR missing");
        return;
    }

    // === BIND ===
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // === PUSH CONSTANTS WITH ZOOM OFFSET ===
    RTConstants push{};
    push.clearColor        = glm::vec4(0.0f);
    push.cameraPosition    = camPos + glm::vec3(0.0f, 0.0f, 5.0f * (zoomLevel - 1.0f));
    push._pad0             = 0.0f;
    push.lightDirection    = glm::normalize(glm::vec3(-0.5f, -1.0f, 0.6f));
    push.lightIntensity    = 14.0f;
    push.samplesPerPixel   = 1;
    push.maxDepth          = 3;
    push.maxBounces        = 3;
    push.russianRoulette   = 0.9f;
    push.resolution        = glm::vec2(w, h);
    push.showEnvMapOnly    = 0;
    push.frame             = imageIndex;
    push.fireflyClamp      = 15.0f;

    vkCmdPushConstants(commandBuffer, pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
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
        .size          = context.sbtRecordSize * 2
    };
    const VkStridedDeviceAddressRegionKHR callable = {};

    // === DISPATCH ===
    context.vkCmdTraceRaysKHR(
        commandBuffer,
        &raygen,
        &miss,
        &hit,
        &callable,
        static_cast<uint32_t>(w),
        static_cast<uint32_t>(h),
        1
    );

    LOG_MODE5("{}GLOSSY DISPATCHED | 1 SPP | 3 bounces | sharp reflections | WASD + Mouse + Scroll{}", 
              EMERALD_GREEN, RESET);
}

} // namespace VulkanRTX