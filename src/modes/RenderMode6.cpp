// src/modes/RenderMode6.cpp
// AMOURANTH RTX — MODE 6: REFRACTION + GLASS
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
    push.lightDirection    = glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f));
    push.lightIntensity    = 20.0f;
    push.samplesPerPixel   = 1;
    push.maxDepth          = 5;
    push.maxBounces        = 5;
    push.russianRoulette   = 0.95f;
    push.resolution        = glm::vec2(w, h);
    push.showEnvMapOnly    = 0;
    push.frame             = imageIndex;
    push.fireflyClamp      = 20.0f;

    vkCmdPushConstants(commandBuffer, pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0, sizeof(RTConstants), &push);

    const VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
    const VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize * 1 };
    const VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize * 3 };
    const VkStridedDeviceAddressRegionKHR callable = {};

    context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, w, h, 1);
}

} // namespace VulkanRTX