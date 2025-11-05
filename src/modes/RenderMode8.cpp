// src/modes/RenderMode8.cpp
// AMOURANTH RTX — MODE 8: EMISSION + NEON
// Keyboard key: 8

#include "modes/RenderMode8.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/RTConstants.hpp"
#include "engine/camera.hpp"

namespace VulkanRTX {

void renderMode8(
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
    if (!context.camera || !context.enableRayTracing || !context.vkCmdTraceRaysKHR) return;

    const glm::vec3 camPos = context.camera->getPosition();

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    RTConstants push{};
    push.clearColor        = glm::vec4(0.0f);
    push.cameraPosition    = camPos;
    push.lightDirection    = glm::vec3(0.0f);
    push.lightIntensity    = 0.0f;  // Emission only
    push.samplesPerPixel   = 1;
    push.maxDepth          = 2;
    push.maxBounces        = 1;
    push.russianRoulette   = 0.0f;
    push.resolution        = glm::vec2(w, h);
    push.showEnvMapOnly    = 0;
    push.frame             = imageIndex;
    push.fireflyClamp      = 100.0f;

    vkCmdPushConstants(commandBuffer, pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0, sizeof(RTConstants), &push);

    const VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
    const VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize * 1 };
    const VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize * 1 };
    const VkStridedDeviceAddressRegionKHR callable = {};

    context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, w, h, 1);
}

} // namespace VulkanRTX