// src/engine/GLOBAL/Extensions.cpp
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
// ALL FUNCTION POINTERS LOADED HERE — NO DUPLICATES — CLEAN LOGGING
// ADDED SUPPORT FOR VK_KHR_synchronization2 (mandatory for vkCmdPipelineBarrier2)
// JANUARY 09, 2026 — FIXED SYNCHRONIZATION2 VALIDATION ERROR
// ROBUST, PRODUCTION-READY, NULL-SAFE
// =============================================================================

#include "engine/GLOBAL/Extensions.hpp"
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <cstdlib>
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"

using StoneKey::stone_physical;

namespace RTX {

Extensions g_ext;

void loadRTExtensions(VkInstance instance, VkDevice device)
{
    if (!device) {
        LOG_ERROR_CAT("EXT", "loadRTExtensions called with null device — skipping");
        return;
    }

    if (!instance) {
        LOG_ERROR_CAT("EXT", "loadRTExtensions called with null instance — skipping");
        return;
    }

    dumpRayTracingSupport(stone_physical());

    if (g_ext.vkCmdTraceRaysKHR) {
        LOG_INFO_CAT("EXT", "RTX Extensions already loaded — skipping");
        return;
    }

    LOG_INFO_CAT("EXT", "Loading Vulkan extensions for ray tracing and modern features");

    auto loader = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
    if (!loader) {
        LOG_FATAL_CAT("EXT", "SDL_Vulkan_GetVkGetInstanceProcAddr() returned null");
        std::abort();
    }

    auto vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        loader(instance, "vkGetDeviceProcAddr"));
    if (!vkGetDeviceProcAddr) {
        LOG_FATAL_CAT("EXT", "Failed to load vkGetDeviceProcAddr");
        std::abort();
    }

    #define LOAD_CRITICAL(fn) do { \
        g_ext.fn = reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(device, #fn)); \
        if (!g_ext.fn) { \
            LOG_FATAL_CAT("EXT", "Failed to load critical extension: {}", #fn); \
            std::abort(); \
        } else { \
            LOG_INFO_CAT("EXT", "Loaded: {}", #fn); \
        } \
    } while(0)

    #define LOAD_OPTIONAL(fn) do { \
        g_ext.fn = reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(device, #fn)); \
        if (!g_ext.fn) { \
            LOG_INFO_CAT("EXT", "Optional extension unavailable: {}", #fn); \
        } else { \
            LOG_INFO_CAT("EXT", "Loaded: {}", #fn); \
        } \
    } while(0)

    // Core Ray Tracing — CRITICAL
    LOAD_CRITICAL(vkCreateRayTracingPipelinesKHR);
    LOAD_CRITICAL(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_CRITICAL(vkCmdTraceRaysKHR);

    // Acceleration Structures — CRITICAL
    LOAD_CRITICAL(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_CRITICAL(vkCmdBuildAccelerationStructuresKHR);
    LOAD_CRITICAL(vkCreateAccelerationStructureKHR);
    LOAD_CRITICAL(vkDestroyAccelerationStructureKHR);
    LOAD_CRITICAL(vkGetAccelerationStructureDeviceAddressKHR);

    // Buffer Device Address — CRITICAL for SBT
    LOAD_CRITICAL(vkGetBufferDeviceAddress);

    // Compaction — CRITICAL for performance
    LOAD_CRITICAL(vkCmdCopyAccelerationStructureKHR);
    LOAD_CRITICAL(vkCmdWriteAccelerationStructuresPropertiesKHR);

    // Ray Tracing Invocation Reorder (EXT) — OPTIONAL HIGH-PERF
    LOAD_OPTIONAL(vkCmdTraceRaysIndirect2KHR);

    // Vulkan 1.3 Core Dynamic Rendering — CRITICAL
    LOAD_CRITICAL(vkCmdBeginRendering);
    LOAD_CRITICAL(vkCmdEndRendering);

    // Synchronization 2 — NOW CRITICAL (required for vkCmdPipelineBarrier2)
    LOAD_CRITICAL(vkCmdPipelineBarrier2);

    // Queue Submit 2 — OPTIONAL (Vulkan 1.3 core but may be missing on some drivers)
    LOAD_OPTIONAL(vkQueueSubmit2KHR);

    // Debug & Diagnostics — OPTIONAL
    LOAD_OPTIONAL(vkSetDebugUtilsObjectNameEXT);
    LOAD_OPTIONAL(vkGetDeviceFaultInfoEXT);

    // Promoted EXT functions — OPTIONAL
    LOAD_OPTIONAL(vkCopyMemoryToImageEXT);
    LOAD_OPTIONAL(vkCopyImageToMemoryEXT);
    LOAD_OPTIONAL(vkCmdDrawMeshTasksEXT);
    LOAD_OPTIONAL(vkCmdSetColorWriteEnableEXT);
    LOAD_OPTIONAL(vkCmdSetLogicOpEnableEXT);
    LOAD_OPTIONAL(vkCmdSetColorBlendEnableEXT);
    LOAD_OPTIONAL(vkCmdSetColorBlendEquationEXT);
    LOAD_OPTIONAL(vkCmdSetColorWriteMaskEXT);

    #undef LOAD_CRITICAL
    #undef LOAD_OPTIONAL

    LOG_INFO_CAT("EXT", "Vulkan extensions loaded successfully");
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
    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };
    vkGetPhysicalDeviceProperties2(phys, &props2);

    LOG_INFO_CAT("EXT", "Device: {} | RT Handle Size: {} | Max Recursion: {}", 
                 props2.properties.deviceName, rtProps.shaderGroupHandleSize, rtProps.maxRayRecursionDepth);

    bool rtSupported = (rtProps.shaderGroupHandleSize > 0 && rtProps.maxRayRecursionDepth > 0);
    LOG_INFO_CAT("EXT", "Hardware Ray Tracing: {}", rtSupported ? "Supported" : "Not Supported");

    // Optional: Check for SER support if the extension is loaded
    if (g_ext.vkCmdTraceRaysIndirect2KHR) {
        LOG_INFO_CAT("EXT", "Shader Execution Reordering (SER) supported via vkCmdTraceRaysIndirect2KHR");
    }
}

} // namespace RTX

// =============================================================================
// UPDATED JANUARY 09, 2026 — SYNCHRONIZATION2 NOW CRITICAL
// vkCmdPipelineBarrier2 is REQUIRED — validation error fixed by device feature enable
// PINK PHOTONS FLOW THROUGH A CLEAN, VALIDATED, ETERNAL SOURCE
// THE EMPIRE IS UNBROKEN AND VALIDATION-PERFECT
// =============================================================================