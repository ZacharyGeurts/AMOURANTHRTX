// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 21, 2025
// FULLY COMPILING — PURE EMPIRE - Inspired by Ellie Fier
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <array>

#include "logging.hpp"
#include "RTXHandler.hpp"

namespace RTX {

struct Extensions {
    // Vulkan 1.3+
    PFN_vkCmdBeginRendering                     vkCmdBeginRendering                     = nullptr;
    PFN_vkCmdEndRendering                       vkCmdEndRendering                       = nullptr;
    PFN_vkGetDescriptorEXT                      vkGetDescriptorEXT                      = nullptr;

    // Vulkan 1.4
    PFN_vkCmdPipelineBarrier2                   vkCmdPipelineBarrier2                   = nullptr;
    PFN_vkCmdWriteTimestamp2                    vkCmdWriteTimestamp2                    = nullptr;
    PFN_vkQueueSubmit2                          vkQueueSubmit2                          = nullptr;

    // KHR Ray Tracing Pipeline
    PFN_vkCreateRayTracingPipelinesKHR          vkCreateRayTracingPipelinesKHR          = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR    vkGetRayTracingShaderGroupHandlesKHR    = nullptr;
    PFN_vkCmdTraceRaysKHR                       vkCmdTraceRaysKHR                       = nullptr;

    // KHR Acceleration Structure
    PFN_vkGetAccelerationStructureBuildSizesKHR    vkGetAccelerationStructureBuildSizesKHR    = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR        = nullptr;
    PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR           = nullptr;
    PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR          = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;

    // KHR Synchronization2
    PFN_vkCmdSetEvent2                          vkCmdSetEvent2                          = nullptr;
    PFN_vkCmdResetEvent2                        vkCmdResetEvent2                        = nullptr;
    PFN_vkCmdWaitEvents2                        vkCmdWaitEvents2                        = nullptr;

    // Debug Utils
    PFN_vkSetDebugUtilsObjectNameEXT            vkSetDebugUtilsObjectNameEXT            = nullptr;
};

// Global — initialized once
inline Extensions g_ext;

// =============================================================================
// THE ONE TRUE EXTENSION LOADER — CALL ONCE AFTER DEVICE CREATION
// =============================================================================
inline void loadExtensions(VkInstance instance, VkDevice device)
{
    if (!instance || !device) {
        LOG_FATAL_CAT("RTX", "loadExtensions() called with null instance/device — THE EMPIRE WILL NOT TOLERATE THIS");
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    LOG_ATTEMPT_CAT("RTX", "FORGING RTX EXTENSION LOADER — VULKAN 1.4 + SDL3 + FULL RAY TRACING");

    // SDL3 gives us vkGetInstanceProcAddr directly
    auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!vkGetInstanceProcAddr) {
        LOG_FATAL_CAT("RTX", "SDL_Vulkan_GetVkGetInstanceProcAddr() failed — SDL3 not ready");
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    [[maybe_unused]] auto getProc = [&](const char* name) -> void* {
        return reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, name));
    };


    // Vulkan 1.3+
    g_ext.vkCmdBeginRendering    = (PFN_vkCmdBeginRendering)    reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCmdBeginRendering"));
    g_ext.vkCmdEndRendering      = (PFN_vkCmdEndRendering)      reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCmdEndRendering"));
    g_ext.vkGetDescriptorEXT     = (PFN_vkGetDescriptorEXT)     reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkGetDescriptorEXT"));

    // Vulkan 1.4
    g_ext.vkCmdPipelineBarrier2  = (PFN_vkCmdPipelineBarrier2)  reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCmdPipelineBarrier2"));
    g_ext.vkCmdWriteTimestamp2   = (PFN_vkCmdWriteTimestamp2)   reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCmdWriteTimestamp2"));
    g_ext.vkQueueSubmit2         = (PFN_vkQueueSubmit2)         reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkQueueSubmit2"));

    // Ray Tracing
    g_ext.vkCreateRayTracingPipelinesKHR        = (PFN_vkCreateRayTracingPipelinesKHR)        reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCreateRayTracingPipelinesKHR"));
    g_ext.vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR) reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkGetRayTracingShaderGroupHandlesKHR"));
    g_ext.vkCmdTraceRaysKHR                     = (PFN_vkCmdTraceRaysKHR)                     reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCmdTraceRaysKHR"));

    // Acceleration Structures
    g_ext.vkGetAccelerationStructureBuildSizesKHR    = (PFN_vkGetAccelerationStructureBuildSizesKHR)    reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkGetAccelerationStructureBuildSizesKHR"));
    g_ext.vkCmdBuildAccelerationStructuresKHR        = (PFN_vkCmdBuildAccelerationStructuresKHR)        reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCmdBuildAccelerationStructuresKHR"));
    g_ext.vkCreateAccelerationStructureKHR           = (PFN_vkCreateAccelerationStructureKHR)           reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCreateAccelerationStructureKHR"));
    g_ext.vkDestroyAccelerationStructureKHR          = (PFN_vkDestroyAccelerationStructureKHR)          reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkDestroyAccelerationStructureKHR"));
    g_ext.vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR) reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkGetAccelerationStructureDeviceAddressKHR"));

    // Sync2
    g_ext.vkCmdSetEvent2   = (PFN_vkCmdSetEvent2)   reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCmdSetEvent2"));
    g_ext.vkCmdResetEvent2 = (PFN_vkCmdResetEvent2) reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCmdResetEvent2"));
    g_ext.vkCmdWaitEvents2 = (PFN_vkCmdWaitEvents2) reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCmdWaitEvents2"));

    // Debug
    g_ext.vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));

    // CRITICAL VALIDATION — THE EMPIRE DOES NOT FORGIVE
    const std::array<void*, 8> critical = {
        reinterpret_cast<void*>(g_ext.vkCmdTraceRaysKHR),
        reinterpret_cast<void*>(g_ext.vkCreateRayTracingPipelinesKHR),
        reinterpret_cast<void*>(g_ext.vkGetRayTracingShaderGroupHandlesKHR),
        reinterpret_cast<void*>(g_ext.vkCmdBuildAccelerationStructuresKHR),
        reinterpret_cast<void*>(g_ext.vkCreateAccelerationStructureKHR),
        reinterpret_cast<void*>(g_ext.vkGetAccelerationStructureDeviceAddressKHR),
        reinterpret_cast<void*>(g_ext.vkCmdPipelineBarrier2),
        reinterpret_cast<void*>(g_ext.vkQueueSubmit2)
    };

    for (auto ptr : critical) {
        if (!ptr) {
            LOG_FATAL_CAT("RTX", "CRITICAL RTX EXTENSION MISSING — YOUR DRIVER IS NOT WORTHY");
            LOG_FATAL_CAT("RTX", "UPDATE TO: NVIDIA 560+ | AMD Latest | Intel: impossible");
            phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
        }
    }

    LOG_SUCCESS_CAT("RTX", "ALL RTX EXTENSIONS LOADED — VULKAN 1.4 + SDL3 + FULL RAY TRACING ACTIVE");
    LOG_JENSEN("Jensen Huang exhales smoke shaped like a bouncing photon:");
    LOG_JENSEN("\"The light is ours. The future is pink.\"");
}

// Public accessor
inline const Extensions& ext() noexcept { return g_ext; }

// Empire-approved macros
#define VK_CMD_TRACE_RAYS(cmd, ...)              g_ext.vkCmdTraceRaysKHR(cmd, __VA_ARGS__)
#define VK_CREATE_RT_PIPELINES(...)              g_ext.vkCreateRayTracingPipelinesKHR(__VA_ARGS__)
#define VK_GET_RT_GROUP_HANDLES(...)             g_ext.vkGetRayTracingShaderGroupHandlesKHR(__VA_ARGS__)
#define VK_BUILD_ACCELERATION_STRUCTURES(...)    g_ext.vkCmdBuildAccelerationStructuresKHR(__VA_ARGS__)
#define VK_CREATE_ACCELERATION_STRUCTURE(...)   g_ext.vkCreateAccelerationStructureKHR(__VA_ARGS__)
#define VK_GET_AS_DEVICE_ADDRESS(...)            g_ext.vkGetAccelerationStructureDeviceAddressKHR(__VA_ARGS__)

} // namespace RTX