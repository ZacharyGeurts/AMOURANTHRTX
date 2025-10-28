// mode8.cpp
// Implementation of renderMode8 for AMOURANTH RTX Engine to draw eight moving mirror balls.
// Copyright Zachary Geurts 2025

#include "engine/core.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ue_init.hpp"

void renderMode8(const UE::AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, int width, int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer) {
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {(uint32_t)width, (uint32_t)height};

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    struct PushConstants {
        glm::mat4 modelViewProj;
        glm::vec4 color;
    } pushConstants;

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 100.0f);
    glm::mat4 view = amouranth->getViewMatrix();

    static float time = 0.0f;
    time += deltaTime;

    for (int i = 0; i < 8; ++i) {
        float angle = time * (1.0f + i * 0.5f);
        float radius = 1.0f + i * 0.5f;
        glm::vec3 position = glm::vec3(glm::cos(angle) * radius, 0.0f, glm::sin(angle) * radius);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        pushConstants.modelViewProj = projection * view * model;

        pushConstants.color = (i == 0) ? glm::vec4(0.8f, 0.8f, 0.8f, 1.0f) :
                             (i == 1) ? glm::vec4(1.0f, 0.84f, 0.0f, 1.0f) :
                             (i == 2) ? glm::vec4(0.8f, 0.5f, 0.2f, 1.0f) :
                             (i == 3) ? glm::vec4(0.9f, 0.5f, 0.3f, 1.0f) :
                             (i == 4) ? glm::vec4(0.5f, 0.5f, 0.9f, 1.0f) :
                             (i == 5) ? glm::vec4(0.9f, 0.3f, 0.9f, 1.0f) :
                             (i == 6) ? glm::vec4(0.3f, 0.9f, 0.3f, 1.0f) :
                                        glm::vec4(1.0f, 0.3f, 0.3f, 1.0f); // Red

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);

        vkCmdDraw(commandBuffer, 1, 1, i, 0);
    }

    vkCmdEndRenderPass(commandBuffer);
}