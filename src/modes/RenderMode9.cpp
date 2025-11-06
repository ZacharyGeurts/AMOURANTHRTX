// src/modes/RenderMode9.cpp
// AMOURANTH RTX — MODE 9: FULL PATH TRACER + ACCUMULATION + DENOISE
// Keyboard key: 9 → Reference quality, progressive, TAA, firefly clamp

#include "modes/RenderMode9.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/RTConstants.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"

namespace VulkanRTX {

using namespace Logging::Color;
#define LOG_MODE9(...) LOG_INFO_CAT("RenderMode9", __VA_ARGS__)

void renderMode9(
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

    glm::vec3 camPos = glm::vec3(0.0f, 0.0f, 5.0f);
    float fov = 60.0f;
    float zoomLevel = 1.0f;

    if (context.camera) {
        auto* cam = static_cast<PerspectiveCamera*>(context.camera);
        camPos = cam->getPosition();
        fov = cam->getFOV();
        zoomLevel = 60.0f / fov;

        LOG_MODE9("{}REFERENCE PT | {}x{} | pos: ({:.2f}, {:.2f}, {:.2f}) | FOV: {:.1f}° | zoom: {:.2f}x{}", 
                  BRIGHT_PINKISH_PURPLE, w, h, camPos.x, camPos.y, camPos.z, fov, zoomLevel, RESET);
    } else {
        LOG_MODE9("{}REFERENCE PT | {}x{} | fallback pos (0,0,5){}", BRIGHT_PINKISH_PURPLE, w, h, RESET);
    }

    if (!context.enableRayTracing || !context.vkCmdTraceRaysKHR) return;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    RTConstants push{};
    push.clearColor        = glm::vec4(0.0f);
    push.cameraPosition    = camPos + glm::vec3(0.0f, 0.0f, 5.0f * (zoomLevel - 1.0f));
    push._pad0             = 0.0f;
    push.lightDirection    = glm::normalize(glm::vec3(0.6f, -1.0f, 0.4f));
    push.lightIntensity    = 20.0f;
    push.samplesPerPixel   = 1;
    push.maxDepth          = 8;
    push.maxBounces        = 6;
    push.russianRoulette   = 0.98f;
    push.resolution        = glm::vec2(w, h);
    push.showEnvMapOnly    = 0;
    push.frame             = imageIndex;
    push.fireflyClamp      = 30.0f;

    vkCmdPushConstants(commandBuffer, pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0, sizeof(RTConstants), &push);

    const VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
    const VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize * 3 };
    const VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize * 4 };
    const VkStridedDeviceAddressRegionKHR callable = {};

    context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, w, h, 1);

    LOG_MODE9("{}REFERENCE DISPATCH | 1 SPP | 6 bounces | accumulation + TAA{}", EMERALD_GREEN, RESET);
}

} // namespace VulkanRTX