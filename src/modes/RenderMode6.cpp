// src/modes/RenderMode6.cpp
// AMOURANTH RTX — MODE 6: PATH TRACED GLOBAL ILLUMINATION
// FULLY MODULAR. FULLY SCALABLE. FULLY GLOWING.
// Keyboard key: 6

#include "modes/RenderMode6.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/RTConstants.hpp"
#include "engine/camera.hpp"

namespace VulkanRTX {

void renderMode6(
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

    if (!context.camera) return;

    const glm::vec3 camPos = context.camera->getPosition();

    if (!context.enableRayTracing || !context.vkCmdTraceRaysKHR) return;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    RTConstants push{};
    push.clearColor        = glm::vec4(0.05f, 0.05f, 0.10f, 1.0f);
    push.cameraPosition    = camPos;
    push.lightDirection    = glm::normalize(glm::vec3(1.0f, -0.5f, 0.8f));
    push.lightIntensity    = 20.0f;
    push.samplesPerPixel   = 2;
    push.maxDepth          = 5;
    push.maxBounces        = 4;
    push.russianRoulette   = 0.8f;
    push.resolution        = glm::vec2(width, height);
    push.showEnvMapOnly    = 0;
    push.frame             = imageIndex;

    vkCmdPushConstants(commandBuffer, pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
        0, sizeof(RTConstants), &push);

    const VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
    const VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize * 2 };
    const VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize };
    const VkStridedDeviceAddressRegionKHR callable = {};

    context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, width, height, 1);
}

} // namespace VulkanRTX