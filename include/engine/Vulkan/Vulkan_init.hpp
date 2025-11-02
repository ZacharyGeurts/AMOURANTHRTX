// src/engine/Vulkan/VulkanInitializer.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0

#pragma once

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/utils.hpp"
#include <SDL3/SDL_vulkan.h>  // ← Correct: SDL3-only Vulkan integration
#include <vector>

namespace VulkanInitializer {

// Core
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
void initInstance(const std::vector<std::string>& extensions, Vulkan::Context& context);
void initSurface(Vulkan::Context& context, void* window, VkSurfaceKHR* rawSurface);
VkPhysicalDevice findPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, bool preferNvidia);
void initDevice(Vulkan::Context& context);
void initializeVulkan(Vulkan::Context& context);  // ← NOW CREATES SWAPCHAIN VIA MANAGER

// Command buffers
VkCommandBuffer beginSingleTimeCommands(Vulkan::Context& context);
void endSingleTimeCommands(Vulkan::Context& context, VkCommandBuffer cmd);

// Buffer creation
void createBuffer(VkDevice device,
                  VkPhysicalDevice physicalDevice,
                  VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties,
                  VkBuffer& buffer,
                  VkDeviceMemory& memory,
                  const VkMemoryAllocateFlagsInfo* allocFlags,
                  Vulkan::Context& context);

// Image helpers
void transitionImageLayout(Vulkan::Context& context,
                           VkImage image,
                           VkFormat format,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout);

void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

void copyBufferToImage(Vulkan::Context& context,
                       VkBuffer srcBuffer,
                       VkImage dstImage,
                       uint32_t width,
                       uint32_t height);

void createStorageImage(VkDevice device,
                        VkPhysicalDevice physicalDevice,
                        VkImage& image,
                        VkDeviceMemory& memory,
                        VkImageView& view,
                        uint32_t width,
                        uint32_t height,
                        Vulkan::Context& context);

// Descriptor layouts
void createDescriptorSetLayout(VkDevice device,
                               VkPhysicalDevice physicalDevice,
                               VkDescriptorSetLayout& rayTracingLayout,
                               VkDescriptorSetLayout& graphicsLayout);

// Descriptor pool + set
void createDescriptorPoolAndSet(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkDescriptorSetLayout descriptorSetLayout,
    VkDescriptorPool& descriptorPool,
    std::vector<VkDescriptorSet>& descriptorSets,
    VkSampler& sampler,
    VkBuffer uniformBuffer,
    VkImageView storageImageView,
    VkAccelerationStructureKHR topLevelAS,
    bool forRayTracing,
    const std::vector<VkBuffer>& materialBuffers,
    const std::vector<VkBuffer>& dimensionBuffers,
    VkImageView denoiseImageView,
    VkImageView envMapView,
    VkImageView densityVolumeView,
    VkImageView gDepthView,
    VkImageView gNormalView,
    Vulkan::Context& context);

// Device address helpers
VkDeviceAddress getBufferDeviceAddress(const Vulkan::Context& context, VkBuffer buffer);
VkDeviceAddress getAccelerationStructureDeviceAddress(const Vulkan::Context& context, VkAccelerationStructureKHR as);

// Utility
bool hasStencilComponent(VkFormat format);

} // namespace VulkanInitializer