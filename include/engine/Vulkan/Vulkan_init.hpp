// src/engine/Vulkan/Vulkan_init.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// Licensed under CC BY-NC 4.0

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/utils.hpp"
#include <SDL3/SDL_vulkan.h>
#include <vector>

namespace VulkanInitializer {

// ===================================================================
// CORE INITIALIZATION
// ===================================================================
uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                        uint32_t typeFilter,
                        VkMemoryPropertyFlags properties);

void initInstance(const std::vector<std::string>& extensions,
                  Vulkan::Context& context);

void initSurface(Vulkan::Context& context,
                 void* window,
                 VkSurfaceKHR* rawSurface = nullptr);

VkPhysicalDevice findPhysicalDevice(VkInstance instance,
                                   VkSurfaceKHR surface,
                                   bool preferNvidia = true);

void initDevice(Vulkan::Context& context);

void initializeVulkan(Vulkan::Context& context);  // Full init: instance → surface → device → swapchain

// ===================================================================
// COMMAND BUFFER HELPERS
// ===================================================================
VkCommandBuffer beginSingleTimeCommands(Vulkan::Context& context);
void endSingleTimeCommands(Vulkan::Context& context, VkCommandBuffer cmd);

// ===================================================================
// BUFFER CREATION
// ===================================================================
void createBuffer(VkDevice device,
                  VkPhysicalDevice physicalDevice,
                  VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties,
                  VkBuffer& buffer,
                  VkDeviceMemory& memory,
                  const VkMemoryAllocateFlagsInfo* allocFlags,
                  Vulkan::Context& context);

// ===================================================================
// IMAGE HELPERS
// ===================================================================
void transitionImageLayout(Vulkan::Context& context,
                           VkImage image,
                           VkFormat format,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout);

void copyBuffer(VkDevice device,
                VkCommandPool commandPool,
                VkQueue queue,
                VkBuffer srcBuffer,
                VkBuffer dstBuffer,
                VkDeviceSize size);

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

// ===================================================================
// DESCRIPTOR LAYOUTS
// ===================================================================
void createDescriptorSetLayout(VkDevice device,
                               VkDescriptorSetLayout& rayTracingLayout,
                               VkDescriptorSetLayout& graphicsLayout);

// ===================================================================
// DESCRIPTOR POOL + SET
// ===================================================================
void createDescriptorPoolAndSet(
    VkDevice device,
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

// ===================================================================
// DEVICE ADDRESS HELPERS
// ===================================================================
VkDeviceAddress getBufferDeviceAddress(const Vulkan::Context& context,
                                       VkBuffer buffer);

VkDeviceAddress getAccelerationStructureDeviceAddress(const Vulkan::Context& context,
                                                      VkAccelerationStructureKHR as);

// ===================================================================
// UTILITY
// ===================================================================
bool hasStencilComponent(VkFormat format);

} // namespace VulkanInitializer