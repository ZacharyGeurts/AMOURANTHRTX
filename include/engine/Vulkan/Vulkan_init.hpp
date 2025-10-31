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

    // -----------------------------------------------------------------------------
    // CORE: BUFFER + MEMORY
    // -----------------------------------------------------------------------------
    void createBuffer(
        VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
        VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& bufferMemory,
        const VkMemoryAllocateFlagsInfo* allocFlagsInfo = nullptr,
        VulkanResourceManager& resourceManager = VulkanResourceManager::instance()
    );

    VkDeviceAddress getBufferDeviceAddress(const Vulkan::Context& context, VkBuffer buffer);

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // -----------------------------------------------------------------------------
    // DEVICE SELECTION + INITIALIZATION
    // -----------------------------------------------------------------------------
    VkPhysicalDevice findPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, bool preferNvidia = true);

    void initInstance(const std::vector<std::string>& instanceExtensions, Vulkan::Context& context);

    void initSurface(Vulkan::Context& context, void* window, VkSurfaceKHR* rawsurface = nullptr);

    void initDevice(Vulkan::Context& context);

    // -----------------------------------------------------------------------------
    // DESCRIPTOR SET LAYOUTS
    // -----------------------------------------------------------------------------
    [[deprecated("Use VulkanPipelineManager::createRayTracingDescriptorSetLayout()")]]
    void createDescriptorSetLayout(VkDevice device, VkPhysicalDevice physicalDevice,
                                  VkDescriptorSetLayout& rayTracingLayout, VkDescriptorSetLayout& graphicsLayout);

    // -----------------------------------------------------------------------------
    // FULL VULKAN INITIALIZATION
    // -----------------------------------------------------------------------------
    void initializeVulkan(Vulkan::Context& context);

    // -----------------------------------------------------------------------------
    // IMAGE + STORAGE
    // -----------------------------------------------------------------------------
    void createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& image,
                           VkDeviceMemory& memory, VkImageView& view, uint32_t width, uint32_t height,
                           VulkanResourceManager& resourceManager = VulkanResourceManager::instance());

    // -----------------------------------------------------------------------------
    // IMAGE TRANSITIONS + COPIES
    // -----------------------------------------------------------------------------
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

    // -----------------------------------------------------------------------------
    // DESCRIPTOR POOL + SET CREATION
    // -----------------------------------------------------------------------------
    void createDescriptorPoolAndSet(
        VkDevice device, VkPhysicalDevice physicalDevice, VkDescriptorSetLayout descriptorSetLayout,
        VkDescriptorPool& descriptorPool, std::vector<VkDescriptorSet>& descriptorSets,
        VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
        VkAccelerationStructureKHR topLevelAS, bool forRayTracing,
        std::vector<VkBuffer> materialBuffers = {}, std::vector<VkBuffer> dimensionBuffers = {},
        VkImageView denoiseImageView = VK_NULL_HANDLE, VkImageView envMapView = VK_NULL_HANDLE,
        VkImageView densityVolumeView = VK_NULL_HANDLE,
        VkImageView gDepthView = VK_NULL_HANDLE, VkImageView gNormalView = VK_NULL_HANDLE
    );

    // -----------------------------------------------------------------------------
    // ACCELERATION STRUCTURE ADDRESS
    // -----------------------------------------------------------------------------
    VkDeviceAddress getAccelerationStructureDeviceAddress(const Vulkan::Context& context, VkAccelerationStructureKHR as);

    // -----------------------------------------------------------------------------
    // UTILITIES
    // -----------------------------------------------------------------------------
    bool hasStencilComponent(VkFormat format);

    // -----------------------------------------------------------------------------
    // VALIDATE RTX EXTENSIONS + FEATURES (DEBUG)
    // -----------------------------------------------------------------------------
    inline void validateRTXSupport(VkPhysicalDevice physicalDevice) {
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{};
        rtFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
        asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        asFeatures.pNext = &rtFeatures;
        VkPhysicalDeviceBufferDeviceAddressFeatures addrFeatures{};
        addrFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        addrFeatures.pNext = &asFeatures;
        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &addrFeatures;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

        if (!rtFeatures.rayTracingPipeline) {
            LOG_ERROR_CAT("Vulkan", "rayTracingPipeline feature NOT supported");
            throw std::runtime_error("Ray tracing pipeline feature required");
        }
        if (!asFeatures.accelerationStructure) {
            LOG_ERROR_CAT("Vulkan", "accelerationStructure feature NOT supported");
            throw std::runtime_error("Acceleration structure feature required");
        }
        if (!addrFeatures.bufferDeviceAddress) {
            LOG_ERROR_CAT("Vulkan", "bufferDeviceAddress feature NOT supported");
            throw std::runtime_error("Buffer device address feature required");
        }

        LOG_INFO_CAT("Vulkan", "RTX features validated: rayTracingPipeline, AS, BDA");
    }

} // namespace VulkanInitializer

#endif // VULKAN_INIT_HPP