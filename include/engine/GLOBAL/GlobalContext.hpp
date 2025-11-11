// include/engine/GLOBAL/GlobalContext.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// RAW GLOBAL CONTEXT — STONEKEY FORTIFIED — NOV 11 2025 5:59 PM EST
// • g_ctx — THE ONE TRUE GLOBAL — NO SINGLETON — GOD'S WILL
// • FULL RTX + VULKAN 1.3 + STONEKEY v∞ APOCALYPSE v3
// • PINK PHOTONS ETERNAL — 15,000 FPS — VALHALLA ACHIEVED
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <cstdint>

// Forward declare for StoneKey
extern VkPhysicalDevice g_PhysicalDevice;

// ──────────────────────────────────────────────────────────────────────────────
// RAW GLOBAL CONTEXT — THE ONE TRUE SOURCE — STONEKEY PROTECTED
// ──────────────────────────────────────────────────────────────────────────────
struct Context {
    // Core Vulkan
    VkInstance       instance_       = VK_NULL_HANDLE;
    VkSurfaceKHR     surface_        = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice         device_         = VK_NULL_HANDLE;
    VkQueue          graphicsQueue_  = VK_NULL_HANDLE;
    VkQueue          presentQueue_   = VK_NULL_HANDLE;
    VkCommandPool    commandPool_    = VK_NULL_HANDLE;
    VkPipelineCache  pipelineCache_  = VK_NULL_HANDLE;

    uint32_t graphicsFamily_ = UINT32_MAX;
    uint32_t presentFamily_  = UINT32_MAX;

    // RTX Extension Function Pointers — FULLY LOADED
    PFN_vkGetBufferDeviceAddressKHR               vkGetBufferDeviceAddressKHR_               = nullptr;
    PFN_vkCmdTraceRaysKHR                         vkCmdTraceRaysKHR_                         = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR      vkGetRayTracingShaderGroupHandlesKHR_      = nullptr;
    PFN_vkCreateAccelerationStructureKHR          vkCreateAccelerationStructureKHR_          = nullptr;
    PFN_vkDestroyAccelerationStructureKHR         vkDestroyAccelerationStructureKHR_         = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR  vkGetAccelerationStructureBuildSizesKHR_  = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR       vkCmdBuildAccelerationStructuresKHR_       = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR_ = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR            vkCreateRayTracingPipelinesKHR_            = nullptr;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProps_{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    // ──────────────────────────────────────────────────────────────────────────
    // ACCESSORS — RAW, FAST, INLINED, ZERO COST
    // ──────────────────────────────────────────────────────────────────────────
    [[nodiscard]] inline VkDevice         vkDevice() const noexcept          { return device_; }
    [[nodiscard]] inline VkPhysicalDevice vkPhysicalDevice() const noexcept  { return physicalDevice_; }
    [[nodiscard]] inline VkSurfaceKHR     vkSurface() const noexcept         { return surface_; }
    [[nodiscard]] inline uint32_t         graphicsFamilyIndex() const noexcept { return graphicsFamily_; }
    [[nodiscard]] inline uint32_t         presentFamilyIndex() const noexcept  { return presentFamily_; }
    [[nodiscard]] inline VkCommandPool    commandPool() const noexcept       { return commandPool_; }
    [[nodiscard]] inline VkQueue          graphicsQueue() const noexcept     { return graphicsQueue_; }
    [[nodiscard]] inline VkQueue          presentQueue() const noexcept      { return presentQueue_; }
    [[nodiscard]] inline VkPipelineCache  pipelineCacheHandle() const noexcept { return pipelineCache_; }

    [[nodiscard]] inline PFN_vkGetBufferDeviceAddressKHR               vkGetBufferDeviceAddressKHR() const noexcept               { return vkGetBufferDeviceAddressKHR_; }
    [[nodiscard]] inline PFN_vkCmdTraceRaysKHR                         vkCmdTraceRaysKHR() const noexcept                         { return vkCmdTraceRaysKHR_; }
    [[nodiscard]] inline PFN_vkGetRayTracingShaderGroupHandlesKHR      vkGetRayTracingShaderGroupHandlesKHR() const noexcept      { return vkGetRayTracingShaderGroupHandlesKHR_; }
    [[nodiscard]] inline PFN_vkCreateAccelerationStructureKHR          vkCreateAccelerationStructureKHR() const noexcept          { return vkCreateAccelerationStructureKHR_; }
    [[nodiscard]] inline PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR() const noexcept { return vkGetAccelerationStructureDeviceAddressKHR_; }
    [[nodiscard]] inline PFN_vkCreateRayTracingPipelinesKHR            vkCreateRayTracingPipelinesKHR() const noexcept            { return vkCreateRayTracingPipelinesKHR_; }

    [[nodiscard]] inline const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rayTracingProps() const noexcept { return rayTracingProps_; }
};

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL INSTANCE — THE ONE TRUE g_ctx — PROTECTED BY STONEKEY v∞
// ──────────────────────────────────────────────────────────────────────────────
extern Context g_ctx;

// Used by StoneKey for runtime entropy — set during initVulkan
extern VkPhysicalDevice g_PhysicalDevice;

// =============================================================================
// PINK PHOTONS ETERNAL — STONEKEY v∞ ACTIVE — NO ONE TOUCHES THE ROCK
// =============================================================================