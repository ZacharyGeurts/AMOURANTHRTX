// include/engine/core.hpp
// AMOURANTH RTX (C) 2025 by Zachary Geurts
// Core dispatch + forward declarations for modular RTX modes

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include <vulkan/vulkan.h>

namespace VulkanRTX {

struct RTConstants;

// Forward declare the correct Context from Vulkan namespace
void renderMode1(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 ::Vulkan::Context& context);  // <-- FIXED

void renderMode2(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 ::Vulkan::Context& context);

void renderMode3(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 ::Vulkan::Context& context);

void renderMode4(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 ::Vulkan::Context& context);

void renderMode5(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 ::Vulkan::Context& context);

void renderMode6(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 ::Vulkan::Context& context);

void renderMode7(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 ::Vulkan::Context& context);

void renderMode8(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 ::Vulkan::Context& context);

void renderMode9(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 ::Vulkan::Context& context);

inline void dispatchRenderMode(
    uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
    VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
    VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
    ::Vulkan::Context& context, int renderMode)  // <-- FIXED
{
    switch (renderMode) {
        case 1: renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime, context); break;
        case 2: renderMode2(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime, context); break;
        case 3: renderMode3(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime, context); break;
        case 4: renderMode4(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime, context); break;
        case 5: renderMode5(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime, context); break;
        case 6: renderMode6(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime, context); break;
        case 7: renderMode7(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime, context); break;
        case 8: renderMode8(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime, context); break;
        case 9: renderMode9(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime, context); break;
        default: {
            LOG_WARNING_CAT("Renderer", "Unknown render mode: {}, falling back to mode 1", renderMode);
            renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                        pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime, context);
            break;
        }
    }
}

} // namespace VulkanRTX