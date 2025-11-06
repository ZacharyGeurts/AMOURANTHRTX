// src/modes/RenderMode7.cpp
// AMOURANTH RTX — MODE 7: ANISOTROPIC SPECULAR + BRUSHED METAL
// Keyboard key: 7 → Directional highlights, metal grain, realism

#include "modes/RenderMode7.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/RTConstants.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"

namespace VulkanRTX {

using namespace Logging::Color;
#define LOG_MODE7(...) LOG_INFO_CAT("RenderMode7", __VA_ARGS__)

void renderMode7(
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

        LOG_MODE7("{}ANISOTROPIC | {}x{} | pos: ({:.2f}, {:.2f}, {:.2f}) | FOV: {:.1f}° | zoom: {:.2f}x{}", 
                  CRIMSON_MAGENTA, w, h, camPos.x, camPos.y, camPos.z, fov, zoomLevel, RESET);
    } else {
        LOG_MODE7("{}ANISOTROPIC | {}x{} | fallback pos (0,0,5){}", CRIMSON_MAGENTA, w, h, RESET);
    }

    if (!context.enableRayTracing || !context.vkCmdTraceRaysKHR) return;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    RTConstants push{};
    push.clearColor        = glm::vec4(0.03f, 0.02f, 0.03f, 1.0f);
    push.cameraPosition    = camPos + glm::vec3(0.0f, 0.0f, 5.0f * (zoomLevel - 1.0f));
    push._pad0             = 0.0f;
    push.lightDirection    = glm::normalize(glm::vec3(1.0f, -0.7f, 0.8f));
    push.lightIntensity    = 16.0f;
    push.samplesPerPixel   = 1;
    push.maxDepth          = 3;
    push.maxBounces        = 3;
    push.russianRoulette   = 0.85f;
    push.resolution        = glm::vec2(w, h);
    push.showEnvMapOnly    = 0;
    push.frame             = imageIndex;
    push.fireflyClamp      = 12.0f;

    vkCmdPushConstants(commandBuffer, pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0, sizeof(RTConstants), &push);

    const VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
    const VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
    const VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize * 2 };
    const VkStridedDeviceAddressRegionKHR callable = {};

    context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, w, h, 1);

    LOG_MODE7("{}BRUSHED METAL | 1 SPP | 3 bounces | anisotropic streak{}", EMERALD_GREEN, RESET);
}

} // namespace VulkanRTX