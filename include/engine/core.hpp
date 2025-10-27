// Core rendering functions for AMOURANTH RTX Engine.
// Copyright Zachary Geurts 2025

#pragma once
#ifndef CORE_HPP
#define CORE_HPP

#include "engine/Vulkan/Vulkan_init.hpp"
#include <vulkan/vulkan.h>

namespace VulkanRTX {
// Forward declarations for renderModeX functions
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

void dispatchRenderMode(
    uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
    VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, VkDevice device,
    VkDeviceMemory vertexBufferMemory, VkPipeline pipeline, float deltaTime,
    VkRenderPass renderPass, VkFramebuffer framebuffer, Vulkan::Context& context, int renderMode);

} // namespace VulkanRTX

#endif // CORE_HPP