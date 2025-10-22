// core.hpp
// Core rendering functions for AMOURANTH RTX Engine.
// Copyright Zachary Geurts 2025

#pragma once
#ifndef CORE_HPP
#define CORE_HPP

#include "ue_init.hpp"
#include <vulkan/vulkan.h>
#include <span>

// Forward declarations for renderModeX functions
void renderMode1(const UE::AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);

void renderMode2(const UE::AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);

void renderMode3(const UE::AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);

void renderMode4(const UE::AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);

void renderMode5(const UE::AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);

void renderMode6(const UE::AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);

void renderMode7(const UE::AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);

void renderMode8(const UE::AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);

void renderMode9(const UE::AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer);

#endif // CORE_HPP