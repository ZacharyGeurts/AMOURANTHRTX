// src/engine/Vulkan/Vulkan_init.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// Licensed under CC BY-NC 4.0

#pragma once

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
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
                  Context& context);

void initSurface(Context& context,
                 void* window,
                 VkSurfaceKHR* rawSurface = nullptr);

VkPhysicalDevice findPhysicalDevice(VkInstance instance,
                                   VkSurfaceKHR surface,
                                   bool preferNvidia = true);

void initDevice(Context& context);

void initializeVulkan(Context& context);  // Full init: instance → surface → device → swapchain

// ===================================================================
// COMMAND BUFFER HELPERS
// ===================================================================
VkCommandBuffer beginSingleTimeCommands(Context& context);
void endSingleTimeCommands(Context& context, VkCommandBuffer cmd);

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
                  Context& context);

// ===================================================================
// IMAGE HELPERS
// ===================================================================
void transitionImageLayout(Context& context,
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

void copyBufferToImage(Context& context,
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
                        Context& context);

// ===================================================================
// DESCRIPTOR LAYOUTS
// ===================================================================
void createDescriptorSetLayout(VkDevice device,
                               VkPhysicalDevice physicalDevice,
                               VkDescriptorSetLayout& rayTracingLayout,
                               VkDescriptorSetLayout& graphicsLayout);

// ===================================================================
// DESCRIPTOR POOL + SET
// ===================================================================
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
    Context& context);

// ===================================================================
// DEVICE ADDRESS HELPERS
// ===================================================================
VkDeviceAddress getBufferDeviceAddress(const Context& context,
                                       VkBuffer buffer);

VkDeviceAddress getAccelerationStructureDeviceAddress(const Context& context,
                                                      VkAccelerationStructureKHR as);

// ===================================================================
// UTILITY
// ===================================================================
bool hasStencilComponent(VkFormat format);

} // namespace VulkanInitializer