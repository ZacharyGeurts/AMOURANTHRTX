// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Core rendering logic for dispatching render modes 1-9.
// Dependencies: Vulkan, core.hpp, logging.hpp
// Supported platforms: Linux, Windows
// Zachary Geurts 2025

#include "engine/core.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>

namespace VulkanRTX {

void dispatchRenderMode(
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
    Vulkan::Context& context,
    int renderMode
) {
    if (renderMode < 1 || renderMode > 9) {
        LOG_ERROR_CAT("Core", "Invalid render mode: {}, falling back to mode 1", renderMode);
        renderMode = 1;
    }

    switch (renderMode) {
        case 1:
            renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                        zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                        device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
            break;
        case 2:
            renderMode2(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                        zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                        device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
            break;
        case 3:
            renderMode3(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                        zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                        device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
            break;
        case 4:
            renderMode4(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                        zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                        device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
            break;
        case 5:
            renderMode5(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                        zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                        device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
            break;
        case 6:
            renderMode6(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                        zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                        device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
            break;
        case 7:
            renderMode7(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                        zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                        device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
            break;
        case 8:
            renderMode8(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                        zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                        device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
            break;
        case 9:
            renderMode9(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                        zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                        device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
            break;
    }
    LOG_DEBUG_CAT("Core", "Dispatched render mode {}", renderMode);
}

void renderMode2(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context) {
    LOG_INFO_CAT("Core", "renderMode2: Placeholder implementation");
    renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
}

void renderMode3(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context) {
    LOG_INFO_CAT("Core", "renderMode3: Placeholder implementation");
    renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
}

void renderMode4(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context) {
    LOG_INFO_CAT("Core", "renderMode4: Placeholder implementation");
    renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
}

void renderMode5(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context) {
    LOG_INFO_CAT("Core", "renderMode5: Placeholder implementation");
    renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
}

void renderMode6(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context) {
    LOG_INFO_CAT("Core", "renderMode6: Placeholder implementation");
    renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
}

void renderMode7(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context) {
    LOG_INFO_CAT("Core", "renderMode7: Placeholder implementation");
    renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
}

void renderMode8(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context) {
    LOG_INFO_CAT("Core", "renderMode8: Placeholder implementation");
    renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
}

void renderMode9(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context) {
    LOG_INFO_CAT("Core", "renderMode9: Placeholder implementation");
    renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                zoomLevel, width, height, wavePhase, pipelineLayout, descriptorSet,
                device, vertexBufferMemory, pipeline, deltaTime, renderPass, framebuffer, context);
}

} // namespace VulkanRTX