// AMOURANTH RTX Engine, October 2025 - Vulkan initialization utilities.
// Dependencies: Vulkan 1.3+, VulkanCore.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#pragma once
#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include "VulkanCore.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h> // Add for ray tracing and buffer device address extensions
#include <span>
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace VulkanInitializer {
    // Ray tracing and buffer device address function pointers
    extern PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    extern PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    extern PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    extern PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    extern PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    extern PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    extern PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    extern PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;

    void createBuffer(
        VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
        VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& bufferMemory,
        const VkMemoryAllocateFlagsInfo* allocFlagsInfo,
        VulkanResourceManager& resourceManager
    );
    VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer);
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
    void createShaderBindingTable(Vulkan::Context& context);
    void createAccelerationStructures(Vulkan::Context& context, VulkanBufferManager& bufferManager,
                                     std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                    VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createDescriptorPoolAndSet(VkDevice device, VkPhysicalDevice physicalDevice, VkDescriptorSetLayout descriptorSetLayout,
                                   VkDescriptorPool& descriptorPool, std::vector<VkDescriptorSet>& descriptorSets,
                                   VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
                                   VkAccelerationStructureKHR topLevelAS, bool forRayTracing,
                                   std::vector<VkBuffer> materialBuffers, std::vector<VkBuffer> dimensionBuffers,
                                   VkImageView denoiseImageView);
}

#endif // VULKAN_INIT_HPP