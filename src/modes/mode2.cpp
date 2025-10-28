// mode2.cpp
// Implementation of renderMode2 for AMOURANTH RTX Engine to draw two moving mirror balls.
// Copyright Zachary Geurts 2025

#include "engine/core.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ue_init.hpp"

void renderMode2(const UE::AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, int width, int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer) {
    // Begin the render pass
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {(uint32_t)width, (uint32_t)height};

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}}; // Black background
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the graphics pipeline (assumes VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind vertex buffer (assumes two vertices)
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);

    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Push constants for transformation and color
    struct PushConstants {
        glm::mat4 modelViewProj; // Combined model-view-projection matrix
        glm::vec4 color;         // Color for mirror ball
    } pushConstants;

    // Compute projection and view matrices
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 100.0f);
    glm::mat4 view = amouranth->getViewMatrix();

    // Animation parameters
    static float time = 0.0f;
    time += deltaTime; // Accumulate time for animation

    // Draw two mirror balls with orbiting motion
    for (int i = 0; i < 2; ++i) {
        // Define positions for two balls orbiting in opposite directions
        float angle = (i == 0 ? time : -time) * 1.0f; // Speed = 1 rad/s
        glm::vec3 position = glm::vec3(glm::cos(angle) * 1.0f, 0.0f, glm::sin(angle) * 1.0f); // Radius = 1.0

        // Model matrix for each ball
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        pushConstants.modelViewProj = projection * view * model;

        // Colors: Silver for ball 1, Gold for ball 2
        pushConstants.color = (i == 0) ? glm::vec4(0.8f, 0.8f, 0.8f, 1.0f) : // Silver
                                        glm::vec4(1.0f, 0.84f, 0.0f, 1.0f); // Gold

        // Update push constants
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);

        // Draw one point (vertex i)
        vkCmdDraw(commandBuffer, 1, 1, i, 0);
    }

    // End the render pass
    vkCmdEndRenderPass(commandBuffer);
}