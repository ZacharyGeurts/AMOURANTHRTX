// src/core.hpp
// Core rendering functions for AMOURANTH RTX Engine.
// Copyright Zachary Geurts 2025
// FINAL: C++11, NO std::span, NO std::format, NO threading

#pragma once
#ifndef CORE_HPP
#define CORE_HPP

#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <cstdio>

namespace VulkanRTX {

// ---------------------------------------------------------------------
//  FORWARD DECLARATIONS: renderMode1 to renderMode9
// ---------------------------------------------------------------------
void renderMode1(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context);

void renderMode2(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context);

void renderMode3(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context);

void renderMode4(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context);

void renderMode5(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context);

void renderMode6(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context);

void renderMode7(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context);

void renderMode8(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context);

void renderMode9(uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
                 VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
                 VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context);

// ---------------------------------------------------------------------
//  MAIN DISPATCH FUNCTION — C++11 safe, uses snprintf
// ---------------------------------------------------------------------
inline void dispatchRenderMode(
    uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
    VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
    VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
    VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context, int renderMode)
{
    switch (renderMode) {
        case 1: renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime,
                            renderPass, framebuffer, context); break;
        case 2: renderMode2(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime,
                            renderPass, framebuffer, context); break;
        case 3: renderMode3(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime,
                            renderPass, framebuffer, context); break;
        case 4: renderMode4(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime,
                            renderPass, framebuffer, context); break;
        case 5: renderMode5(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime,
                            renderPass, framebuffer, context); break;
        case 6: renderMode6(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime,
                            renderPass, framebuffer, context); break;
        case 7: renderMode7(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime,
                            renderPass, framebuffer, context); break;
        case 8: renderMode8(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime,
                            renderPass, framebuffer, context); break;
        case 9: renderMode9(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime,
                            renderPass, framebuffer, context); break;
        default: {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "Unknown render mode: %d, falling back to mode 1", renderMode);
            LOG_WARNING_CAT("Renderer", "%s", buf);
            renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer, zoomLevel, width, height, wavePhase,
                        pipelineLayout, descriptorSet, device, vertexBufferMemory, pipeline, deltaTime,
                        renderPass, framebuffer, context);
            break;
        }
    }
}

} // namespace VulkanRTX

#endif // CORE_HPP