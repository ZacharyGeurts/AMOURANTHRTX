// include/engine/Vulkan/Vulkan_init.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VulkanInitializer → RTX namespace | Uses global RTX::ctx()
// =============================================================================

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/utils.hpp"
#include <SDL3/SDL_vulkan.h>
#include <vector>

namespace RTX {

// ===================================================================
// CORE INITIALIZATION
// ===================================================================
uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                        uint32_t typeFilter,
                        VkMemoryPropertyFlags properties);

void initInstance(const std::vector<const char*>& extensions);  // Uses global RTX::ctx()

void initSurface(void* window);  // Uses global RTX::ctx()

VkPhysicalDevice findPhysicalDevice(VkInstance instance,
                                   VkSurfaceKHR surface,
                                   bool preferNvidia = true);

void initDevice();  // Uses global RTX::ctx()

void initializeVulkan(void* window);  // Full init: instance → surface → device → swapchain | Uses global RTX::ctx()

// ===================================================================
// SWAPCHAIN
// ===================================================================
void createSwapchain(void* window);  // Uses global RTX::swapchain() etc.
void destroySwapchain();  // Uses global RTX::swapchain() etc.

// ===================================================================
// COMMAND BUFFER HELPERS
// ===================================================================
VkCommandBuffer beginSingleTimeCommands();
void endSingleTimeCommands(VkCommandBuffer cmd);

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
                  const VkMemoryAllocateFlagsInfo* allocFlags);

// ===================================================================
// IMAGE HELPERS
// ===================================================================
void transitionImageLayout(VkImage image,
                           VkFormat format,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout);

void copyBuffer(VkDevice device,
                VkCommandPool commandPool,
                VkQueue queue,
                VkBuffer srcBuffer,
                VkBuffer dstBuffer,
                VkDeviceSize size);

void copyBufferToImage(VkBuffer srcBuffer,
                       VkImage dstImage,
                       uint32_t width,
                       uint32_t height);

void createStorageImage(VkDevice device,
                        VkPhysicalDevice physicalDevice,
                        VkImage& image,
                        VkDeviceMemory& memory,
                        VkImageView& view,
                        uint32_t width,
                        uint32_t height);

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
    VkImageView gNormalView);

// ===================================================================
// DEVICE ADDRESS HELPERS
// ===================================================================
VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer);

VkDeviceAddress getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as);

// ===================================================================
// UTILITY
// ===================================================================
bool hasStencilComponent(VkFormat format);

} // namespace RTX