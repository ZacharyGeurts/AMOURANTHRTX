// AMOURANTH RTX Engine, October 2025 - Vulkan initialization utilities.
// Dependencies: Vulkan 1.3+, VulkanCore.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#pragma once
#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include "VulkanCore.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <span>
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace VulkanInitializer {
    void createBuffer(
        VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
        VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& bufferMemory,
        const VkMemoryAllocateFlagsInfo* allocFlagsInfo,
        VulkanResourceManager& resourceManager
    );

    VkDeviceAddress getBufferDeviceAddress(const Vulkan::Context& context, VkBuffer buffer);

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkPhysicalDevice findPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, bool preferNvidia);
    void initInstance(const std::vector<std::string>& instanceExtensions, Vulkan::Context& context);
    void initSurface(Vulkan::Context& context, void* window, VkSurfaceKHR* rawsurface = nullptr);
    void initDevice(Vulkan::Context& context);
    void createDescriptorSetLayout(VkDevice device, VkPhysicalDevice physicalDevice,
                                  VkDescriptorSetLayout& rayTracingLayout, VkDescriptorSetLayout& graphicsLayout);
    void initializeVulkan(Vulkan::Context& context);
    void createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& image,
                           VkDeviceMemory& memory, VkImageView& view, uint32_t width, uint32_t height,
                           VulkanResourceManager& resourceManager);

    // ADDED: Required helper functions used in VulkanRenderer_Render.cpp
    void transitionImageLayout(
        Vulkan::Context& context,
        VkImage image, VkFormat format,
        VkImageLayout oldLayout, VkImageLayout newLayout
    );

    void copyBufferToImage(
        Vulkan::Context& context,
        VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height
    );

    void copyBuffer(
        VkDevice device, VkCommandPool commandPool, VkQueue queue,
        VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size
    );

    void createDescriptorPoolAndSet(
        VkDevice device, VkPhysicalDevice physicalDevice, VkDescriptorSetLayout descriptorSetLayout,
        VkDescriptorPool& descriptorPool, std::vector<VkDescriptorSet>& descriptorSets,
        VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
        VkAccelerationStructureKHR topLevelAS, bool forRayTracing,
        std::vector<VkBuffer> materialBuffers, std::vector<VkBuffer> dimensionBuffers,
        VkImageView alphaTexView, VkImageView envMapView, VkImageView densityVolumeView,
        VkImageView gDepthView, VkImageView gNormalView
    );

    // ADDED: Utility function for depth-stencil formats
    bool hasStencilComponent(VkFormat format);
}

#endif // VULKAN_INIT_HPP