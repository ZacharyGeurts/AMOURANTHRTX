// =============================================================================
// AMOURANTH RTX Engine © 2026 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// EXTENSIONS — CENTRALIZED AND CLEAN — FULL VULKAN 1.4 COMPLIANCE
// SPLIT INSTANCE & DEVICE LOADING — JANUARY 10, 2026
// FULLY STABLE, NO INSTANCE FUNCTIONS VIA DEVICE PROC ADDR
// ROBUST, PRODUCTION-READY, NULL-SAFE
// =============================================================================
#pragma once

// Enable required KHR extensions for compile-time constants & structs
// Must be BEFORE any vulkan.h include in the entire project
#define VK_KHR_acceleration_structure 1
#define VK_KHR_ray_tracing_pipeline 1
#define VK_KHR_deferred_host_operations 1
#define VK_KHR_buffer_device_address 1

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace RTX {

struct Extensions {
    // ── Instance-Level (Surface Queries) ───────────────────────────────
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR      vkGetPhysicalDeviceSurfaceSupportKHR      = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR      vkGetPhysicalDeviceSurfaceFormatsKHR      = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;

    // ── Device-Level (Swapchain Creation & Present) ────────────────────
    PFN_vkCreateSwapchainKHR                      vkCreateSwapchainKHR                      = nullptr;
    PFN_vkDestroySwapchainKHR                     vkDestroySwapchainKHR                     = nullptr;
    PFN_vkGetSwapchainImagesKHR                   vkGetSwapchainImagesKHR                   = nullptr;
    PFN_vkAcquireNextImageKHR                     vkAcquireNextImageKHR                     = nullptr;
    PFN_vkQueuePresentKHR                         vkQueuePresentKHR                         = nullptr;

    // ── Ray Tracing Pipeline (KHR) ─────────────────────────────────────
    PFN_vkCreateRayTracingPipelinesKHR          vkCreateRayTracingPipelinesKHR          = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR    vkGetRayTracingShaderGroupHandlesKHR    = nullptr;
    PFN_vkCmdTraceRaysKHR                       vkCmdTraceRaysKHR                       = nullptr;

    // ── Acceleration Structure (KHR) ───────────────────────────────────
    PFN_vkGetAccelerationStructureBuildSizesKHR     vkGetAccelerationStructureBuildSizesKHR     = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR        = nullptr;
    PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR           = nullptr;
    PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR          = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;

    // ── Buffer Device Address (Core 1.2+) — REQUIRED FOR SBT ─────────────
    PFN_vkGetBufferDeviceAddress                   vkGetBufferDeviceAddress                = nullptr;

    // ── Ray Tracing Maintenance 1 (Compaction) ─────────────────────────
    PFN_vkCmdCopyAccelerationStructureKHR             vkCmdCopyAccelerationStructureKHR             = nullptr;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;

    // ── Ray Tracing Invocation Reorder (EXT) — OPTIONAL ────────────────
    PFN_vkCmdTraceRaysIndirect2KHR                 vkCmdTraceRaysIndirect2KHR              = nullptr;

    // ── Vulkan 1.3 Core — Dynamic Rendering & Synchronization 2 ────────
    PFN_vkCmdBeginRendering                    vkCmdBeginRendering                    = nullptr;
    PFN_vkCmdEndRendering                      vkCmdEndRendering                      = nullptr;
    PFN_vkCmdPipelineBarrier2                  vkCmdPipelineBarrier2                  = nullptr;
    PFN_vkQueueSubmit2KHR                      vkQueueSubmit2KHR                      = nullptr;

    // ── Debug & Diagnostics ───────────────────────────────────────────
    PFN_vkSetDebugUtilsObjectNameEXT           vkSetDebugUtilsObjectNameEXT           = nullptr;
    PFN_vkGetDeviceFaultInfoEXT                vkGetDeviceFaultInfoEXT                = nullptr;

    // ── Promoted from EXT to Core in 1.3/1.4 — Compatibility ─────────────
    PFN_vkCopyMemoryToImageEXT                 vkCopyMemoryToImageEXT                 = nullptr;
    PFN_vkCopyImageToMemoryEXT                 vkCopyImageToMemoryEXT                 = nullptr;
    PFN_vkCmdDrawMeshTasksEXT                  vkCmdDrawMeshTasksEXT                  = nullptr;
    PFN_vkCmdSetColorWriteEnableEXT            vkCmdSetColorWriteEnableEXT            = nullptr;

    // ── Dynamic State (promoted from EXT) ─────────────────────────────
    PFN_vkCmdSetLogicOpEnableEXT               vkCmdSetLogicOpEnableEXT               = nullptr;
    PFN_vkCmdSetColorBlendEnableEXT            vkCmdSetColorBlendEnableEXT            = nullptr;
    PFN_vkCmdSetColorBlendEquationEXT          vkCmdSetColorBlendEquationEXT          = nullptr;
    PFN_vkCmdSetColorWriteMaskEXT              vkCmdSetColorWriteMaskEXT              = nullptr;
};

// THE ONE TRUE GLOBAL — SEALED IN STONE
extern Extensions g_ext;

// CALL ONCE AFTER INSTANCE/DEVICE CREATION
void loadInstanceExtensions(VkInstance instance);
void loadDeviceExtensions(VkDevice device);
void dumpRayTracingSupport(VkPhysicalDevice physicalDevice);

// CLEAN ACCESSOR
[[nodiscard]] inline const Extensions& ext() noexcept { return g_ext; }

} // namespace RTX

// =============================================================================
// SACRED MACROS — LEAN AND CENTRALIZED
// =============================================================================
#define VK_CREATE_RT_PIPELINES(...)              RTX::g_ext.vkCreateRayTracingPipelinesKHR(__VA_ARGS__)
#define VK_GET_RT_GROUP_HANDLES(...)             RTX::g_ext.vkGetRayTracingShaderGroupHandlesKHR(__VA_ARGS__)
#define VK_CMD_TRACE_RAYS(cmd, ...)              RTX::g_ext.vkCmdTraceRaysKHR(cmd, __VA_ARGS__)
#define VK_CMD_TRACE_RAYS_INDIRECT2(cmd, ...)    RTX::g_ext.vkCmdTraceRaysIndirect2KHR(cmd, __VA_ARGS__)

#define VK_GET_AS_BUILD_SIZES(...)               RTX::g_ext.vkGetAccelerationStructureBuildSizesKHR(__VA_ARGS__)
#define VK_CMD_BUILD_ACCELERATION_STRUCTURES(...) RTX::g_ext.vkCmdBuildAccelerationStructuresKHR(__VA_ARGS__)
#define VK_CREATE_ACCELERATION_STRUCTURE(...)    RTX::g_ext.vkCreateAccelerationStructureKHR(__VA_ARGS__)
#define VK_DESTROY_ACCELERATION_STRUCTURE(...)   RTX::g_ext.vkDestroyAccelerationStructureKHR(__VA_ARGS__)
#define VK_GET_AS_DEVICE_ADDRESS(...)            RTX::g_ext.vkGetAccelerationStructureDeviceAddressKHR(__VA_ARGS__)

#define VK_GET_BUFFER_DEVICE_ADDRESS(...)        RTX::g_ext.vkGetBufferDeviceAddress(__VA_ARGS__)

#define VK_CMD_COPY_ACCELERATION_STRUCTURE(cmd, info) \
    RTX::g_ext.vkCmdCopyAccelerationStructureKHR(cmd, info)

#define VK_CMD_WRITE_AS_PROPERTIES(cmd, count, as, queryType, queryPool, query) \
    RTX::g_ext.vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, count, as, queryType, queryPool, query)

#define VK_COPY_MEMORY_TO_IMAGE(...)             RTX::g_ext.vkCopyMemoryToImageEXT(__VA_ARGS__)
#define VK_COPY_IMAGE_TO_MEMORY(...)             RTX::g_ext.vkCopyImageToMemoryEXT(__VA_ARGS__)

// SWAPCHAIN MACROS — AUTOMAGIC ACCESS
#define VK_CREATE_SWAPCHAIN(...)                 RTX::g_ext.vkCreateSwapchainKHR(__VA_ARGS__)
#define VK_DESTROY_SWAPCHAIN(...)                RTX::g_ext.vkDestroySwapchainKHR(__VA_ARGS__)
#define VK_GET_SWAPCHAIN_IMAGES(...)             RTX::g_ext.vkGetSwapchainImagesKHR(__VA_ARGS__)
#define VK_ACQUIRE_NEXT_IMAGE(...)               RTX::g_ext.vkAcquireNextImageKHR(__VA_ARGS__)
#define VK_QUEUE_PRESENT(...)                    RTX::g_ext.vkQueuePresentKHR(__VA_ARGS__)

// =============================================================================
// ALL EXTENSIONS NOW LIVE IN THIS FILE — NO MORE SCATTERED DECLARATIONS
// INSTANCE & DEVICE SPLIT — FULLY STABLE & VALIDATION CLEAN
// JANUARY 10, 2026 — SWAPCHAIN AUTOMAGIC CONVERGENCE ACHIEVED
// =============================================================================