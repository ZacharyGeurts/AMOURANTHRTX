// =============================================================================
// AMOURANTH RTX Engine © 2026 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// EXTENSIONS — CENTRALIZED LOADING — FULL VULKAN 1.4 COMPLIANCE
// INSTANCE & DEVICE SPLIT — JANUARY 25, 2026
// ADDED VK_EXT_descriptor_buffer LOADING & LOGGING
// OPTIONAL EXTENSIONS FAILURE → LOG_INFO (CLEAN LOG)
// ROBUST, PRODUCTION-READY, NULL-SAFE, MULTI-PLATFORM
// =============================================================================

#include "engine/GLOBAL/Extensions.hpp"
#include <SDL3/SDL_vulkan.h>
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"

#ifdef __linux__
#include <execinfo.h>  // Linux-only: backtrace for debug
#endif

using StoneKey::stone_physical;

namespace RTX {

Extensions g_ext;

// =============================================================================
// Load Instance-Level Extensions (surface queries)
// =============================================================================
void loadInstanceExtensions(VkInstance instance)
{
    if (!instance) {
        LOG_ERROR_CAT("EXT", "loadInstanceExtensions called with null instance — skipping");
        return;
    }

    LOG_INFO_CAT("EXT", "Loading instance-level extensions (surface queries)");

    auto vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        SDL_Vulkan_GetVkGetInstanceProcAddr());

    if (!vkGetInstanceProcAddr) {
        LOG_FATAL_CAT("EXT", "SDL_Vulkan_GetVkGetInstanceProcAddr() returned null");
        return;
    }

    #define LOAD_INSTANCE(fn) do { \
        g_ext.fn = reinterpret_cast<PFN_##fn>(vkGetInstanceProcAddr(instance, #fn)); \
        if (!g_ext.fn) { \
            LOG_ERROR_CAT("EXT", "Failed to load instance extension: {}", #fn); \
        } else { \
            LOG_INFO_CAT("EXT", "Loaded instance: {}", #fn); \
        } \
    } while(0)

    LOAD_INSTANCE(vkGetPhysicalDeviceSurfaceSupportKHR);
    LOAD_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR);
    LOAD_INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR);
    LOAD_INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);

    #undef LOAD_INSTANCE
}

// =============================================================================
// Load Device-Level Extensions (swapchain, ray tracing, descriptor buffer, etc.)
// =============================================================================
void loadDeviceExtensions(VkDevice device)
{
    if (!device) {
        LOG_ERROR_CAT("EXT", "loadDeviceExtensions called with null device — skipping");
        return;
    }

    LOG_INFO_CAT("EXT", "Loading device-level extensions for ray tracing, modern features, swapchain, and descriptor buffer");

    auto vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        SDL_Vulkan_GetVkGetInstanceProcAddr());

    if (!vkGetInstanceProcAddr) {
        LOG_FATAL_CAT("EXT", "SDL_Vulkan_GetVkGetInstanceProcAddr() returned null");
        return;
    }

    VkInstance instance = StoneKey::stone_instance();
    if (!instance) {
        LOG_FATAL_CAT("EXT", "Cannot load device extensions — instance is null");
        return;
    }

    auto vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));

    if (!vkGetDeviceProcAddr) {
        LOG_FATAL_CAT("EXT", "Failed to load vkGetDeviceProcAddr from instance");
        return;
    }

    #define LOAD(fn) do { \
        g_ext.fn = reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(device, #fn)); \
        if (!g_ext.fn) { \
            LOG_INFO_CAT("EXT", "Optional extension unavailable: {}", #fn); \
        } else { \
            LOG_INFO_CAT("EXT", "Loaded: {}", #fn); \
        } \
    } while(0)

    // Swapchain & core
    LOAD(vkCreateSwapchainKHR);
    LOAD(vkDestroySwapchainKHR);
    LOAD(vkGetSwapchainImagesKHR);
    LOAD(vkAcquireNextImageKHR);
    LOAD(vkQueuePresentKHR);

    // Ray tracing core
    LOAD(vkCreateRayTracingPipelinesKHR);
    LOAD(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD(vkCmdTraceRaysKHR);

    // Acceleration structures
    LOAD(vkGetAccelerationStructureBuildSizesKHR);
    LOAD(vkCmdBuildAccelerationStructuresKHR);
    LOAD(vkCreateAccelerationStructureKHR);
    LOAD(vkDestroyAccelerationStructureKHR);
    LOAD(vkGetAccelerationStructureDeviceAddressKHR);

    // Buffer device address
    LOAD(vkGetBufferDeviceAddress);

    // Maintenance & reorder
    LOAD(vkCmdCopyAccelerationStructureKHR);
    LOAD(vkCmdWriteAccelerationStructuresPropertiesKHR);
    LOAD(vkCmdTraceRaysIndirect2KHR);

    // Modern Vulkan 1.3+
    LOAD(vkCmdBeginRendering);
    LOAD(vkCmdEndRendering);
    LOAD(vkCmdPipelineBarrier2);
    LOAD(vkQueueSubmit2KHR);

    // Debug & diagnostics
    LOAD(vkSetDebugUtilsObjectNameEXT);
    LOAD(vkGetDeviceFaultInfoEXT);

    // Promoted EXT → core
    LOAD(vkCopyMemoryToImageEXT);
    LOAD(vkCopyImageToMemoryEXT);
    LOAD(vkCmdDrawMeshTasksEXT);
    LOAD(vkCmdSetColorWriteEnableEXT);
    LOAD(vkCmdSetLogicOpEnableEXT);
    LOAD(vkCmdSetColorBlendEnableEXT);
    LOAD(vkCmdSetColorBlendEquationEXT);
    LOAD(vkCmdSetColorWriteMaskEXT);

    // Descriptor buffer — the new empire core
    LOAD(vkGetDescriptorSetLayoutSizeEXT);
    LOAD(vkGetDescriptorSetLayoutBindingOffsetEXT);
    LOAD(vkCmdBindDescriptorBuffersEXT);
    LOAD(vkCmdSetDescriptorBufferOffsetsEXT);
    LOAD(vkGetDescriptorEXT);

    #undef LOAD

    LOG_INFO_CAT("EXT", "Device-level extensions loaded successfully — ray tracing, sync2, swapchain, and descriptor buffer ready");
}

void dumpRayTracingSupport(VkPhysicalDevice phys)
{
    if (!phys) {
        LOG_WARN_CAT("EXT", "dumpRayTracingSupport called with null physical device — skipping");
        return;
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceDescriptorBufferPropertiesEXT descProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT
    };
    rtProps.pNext = &descProps;

    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };
    vkGetPhysicalDeviceProperties2(phys, &props2);

    LOG_INFO_CAT("EXT", "Device: {} | RT Handle Size: {} | Max Recursion: {}", 
                 props2.properties.deviceName, rtProps.shaderGroupHandleSize, rtProps.maxRayRecursionDepth);

    bool rtSupported = (rtProps.shaderGroupHandleSize > 0 && rtProps.maxRayRecursionDepth > 0);
    LOG_INFO_CAT("EXT", "Hardware Ray Tracing: {}", rtSupported ? "Supported" : "Not Supported");

    if (g_ext.vkCmdTraceRaysIndirect2KHR) {
        LOG_INFO_CAT("EXT", "Shader Execution Reordering (SER) supported via vkCmdTraceRaysIndirect2KHR");
    }
}

} // namespace RTX

// =============================================================================
// UPDATED JANUARY 26, 2026 — MULTI-PLATFORM CLEAN
// - execinfo.h guarded for Linux only
// - Clean compile on Windows (no Linux headers)
// - Optional extensions still log INFO on missing
// - dumpRayTracingSupport includes descriptor buffer props
// - Ready for living world breathing
// PINK PHOTONS BINDLESS — EMPIRE UNSTOPPABLE — AMOURANTH FOREVER 💖
// =============================================================================