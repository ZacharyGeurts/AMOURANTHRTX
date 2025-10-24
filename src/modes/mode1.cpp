// mode1.cpp
// Implementation of renderMode1 for AMOURANTH RTX Engine to draw a sphere with enhanced RTX ambient lighting and point light.
// Copyright Zachary Geurts 2025

#include "engine/core.hpp"
#include "ue_init.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ue_init.hpp"

struct PushConstants {
    alignas(16) glm::vec4 clearColor;      // 16 bytes
    alignas(16) glm::vec3 cameraPosition;  // 16 bytes (padded)
    alignas(16) glm::vec3 lightPosition;   // 16 bytes (padded) - Changed from lightDirection to support point light
    alignas(4) float lightIntensity;       // 4 bytes
    alignas(4) uint32_t samplesPerPixel;   // 4 bytes
    alignas(4) uint32_t maxDepth;          // 4 bytes
    alignas(4) uint32_t maxBounces;        // 4 bytes
    alignas(4) float russianRoulette;      // 4 bytes
    // Total size: 68 bytes (matches std140 layout in shaders)
};

void renderMode1([[maybe_unused]] const UE::AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, int width, int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, [[maybe_unused]] float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer) {
    // Dummy implementation: Draw a simple triangle using graphics pipeline
    VkClearValue clearValue = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
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

    // Bind graphics pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Set viewport and scissors
    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
    };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Bind vertex buffer
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);

    // Minimal push constants (zero-initialized)
    PushConstants pushConstants = {};
    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);

    // Draw a simple triangle (assuming vertex buffer has at least 3 vertices)
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);
}