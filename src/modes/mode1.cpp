// Mode 1 rendering for AMOURANTH RTX Engine: Sphere with enhanced RTX ambient lighting and wisp-like point light.
// Copyright Zachary Geurts 2025

#include "engine/core.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

struct PushConstants {
    alignas(16) glm::vec4 clearColor;      // Background color for miss shader
    alignas(16) glm::vec3 cameraPosition;  // Camera position for ray origin
    alignas(16) glm::vec3 lightPosition;   // Point light position
    alignas(16) glm::vec3 lightColor;      // Point light color (wisp effect)
    alignas(4) float lightIntensity;       // Point light intensity
    alignas(4) uint32_t samplesPerPixel;   // Samples for anti-aliasing
    alignas(4) uint32_t maxDepth;          // Max recursion depth for ray tracing
    alignas(4) uint32_t maxBounces;        // Max bounces for path tracing
    alignas(4) float russianRoulette;      // Probability for terminating rays
};

namespace VulkanRTX {

void renderMode1(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    Vulkan::Context& context
) {
    LOG_DEBUG_CAT("RenderMode1", "Rendering mode 1 with zoomLevel: {}, wavePhase: {}", zoomLevel, wavePhase);

    // Begin render pass
    VkClearValue clearValue = {{{0.02f, 0.02f, 0.05f, 1.0f}}}; // Darker misty background
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Check if ray tracing is enabled
    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode1", "Ray tracing disabled, falling back to rasterization");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0); // Hardcoded for triangle fallback
    } else {
        // Bind ray tracing pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);

        // Bind descriptor set
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        // Set up push constants for wisp-like light
        PushConstants pushConstants{
            .clearColor = glm::vec4(0.02f, 0.02f, 0.05f, 1.0f),
            .cameraPosition = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            .lightPosition = glm::vec3(
                std::sin(deltaTime * 0.8f) * 3.0f,
                std::cos(deltaTime * 0.5f) * 2.0f + std::sin(deltaTime * 1.2f) * 1.5f,
                5.0f + std::cos(deltaTime * 0.7f) * 1.0f
            ),
            .lightColor = glm::vec3(0.4f, 0.7f, 1.0f),
            .lightIntensity = 8.0f + std::sin(deltaTime * 2.0f) * 2.0f,
            .samplesPerPixel = 4,
            .maxDepth = 5,
            .maxBounces = 3,
            .russianRoulette = 0.8f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                           0, sizeof(PushConstants), &pushConstants);

        // Set up SBT entries
        VkStridedDeviceAddressRegionKHR raygenEntry{
            .deviceAddress = context.raygenSbtAddress,
            .stride = context.sbtRecordSize,
            .size = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR missEntry{
            .deviceAddress = context.missSbtAddress,
            .stride = context.sbtRecordSize,
            .size = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR hitEntry{
            .deviceAddress = context.hitSbtAddress,
            .stride = context.sbtRecordSize,
            .size = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR callableEntry{};

        LOG_DEBUG_CAT("RenderMode1", "Raygen SBT address: 0x{:x}", raygenEntry.deviceAddress);
        LOG_DEBUG_CAT("RenderMode1", "Miss SBT address: 0x{:x}", missEntry.deviceAddress);
        LOG_DEBUG_CAT("RenderMode1", "Hit SBT address: 0x{:x}", hitEntry.deviceAddress);

        // Trace rays
        if (!VulkanInitializer::vkCmdTraceRaysKHR) {
            LOG_ERROR_CAT("RenderMode1", "vkCmdTraceRaysKHR function pointer is null");
            throw std::runtime_error("vkCmdTraceRaysKHR not initialized");
        }
        VulkanInitializer::vkCmdTraceRaysKHR(commandBuffer, &raygenEntry, &missEntry, &hitEntry, &callableEntry, width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode1", "Completed mode 1 render");
}

} // namespace VulkanRTX