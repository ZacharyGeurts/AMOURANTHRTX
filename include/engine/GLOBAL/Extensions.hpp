// =============================================================================
// Extensions.hpp — FINAL APOCAL postwar EDITION — COMPILES EVERYWHERE — PINK PHOTONS ETERNAL
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <cstdlib>

namespace RTX {

struct Extensions {
    // ── RAY TRACING PIPELINE (MUST NEVER BE NULL) ─────────────────────────────
    PFN_vkCreateRayTracingPipelinesKHR          vkCreateRayTracingPipelinesKHR          = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR    vkGetRayTracingShaderGroupHandlesKHR    = nullptr;
    PFN_vkCmdTraceRaysKHR                       vkCmdTraceRaysKHR                       = nullptr;

    // ── ACCELERATION STRUCTURES (CRITICAL) ───────────────────────────────────
    PFN_vkGetAccelerationStructureBuildSizesKHR     vkGetAccelerationStructureBuildSizesKHR     = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR        = nullptr;
    PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR           = nullptr;
    PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR          = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;

    // ── MODERN VULKAN GOODIES ────────────────────────────────────────────────
    PFN_vkCmdBeginRendering                        vkCmdBeginRendering                     = nullptr;
    PFN_vkCmdEndRendering                          vkCmdEndRendering                       = nullptr;
    PFN_vkCmdPipelineBarrier2                      vkCmdPipelineBarrier2                   = nullptr;

    // ── DEBUG + FAULT (LUXURY BUT WE DESERVE IT) ─────────────────────────────
    PFN_vkSetDebugUtilsObjectNameEXT               vkSetDebugUtilsObjectNameEXT            = nullptr;
    PFN_vkGetDeviceFaultInfoEXT                    vkGetDeviceFaultInfoEXT                 = nullptr;
};

inline Extensions g_ext;

// ─────────────────────────────────────────────────────────────────────────────
// THE ONE TRUE LOADER — WORKS WITH SDL3, GLFW, RAW VULKAN, ANYTHING
// ─────────────────────────────────────────────────────────────────────────────
inline void loadExtensions(VkInstance instance, VkDevice device)
{
    if (!device) {
        fprintf(stderr, "[FATAL RTX] loadExtensions: null device — THE EMPIRE HAS NO HEART\n");
        std::abort();
    }

    // 1. Get vkGetInstanceProcAddr — multiple bulletproof paths
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;

    if (instance) {
        vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)vkGetInstanceProcAddr(instance, "vkGetInstanceProcAddr");
    }
    if (!vkGetInstanceProcAddr) {
        vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkGetInstanceProcAddr");
    }
#ifdef SDL_Vulkan_GetVkGetInstanceProcAddr
    if (!vkGetInstanceProcAddr) {
        vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    }
#endif
    if (!vkGetInstanceProcAddr) {
        fprintf(stderr, "[FATAL RTX] Failed to obtain vkGetInstanceProcAddr — loader broken\n");
        std::abort();
    }

    // 2. Get the REAL device function loader
    auto vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr");
    if (!vkGetDeviceProcAddr) {
        fprintf(stderr, "[FATAL RTX] vkGetDeviceProcAddr not found — ancient Vulkan?\n");
        std::abort();
    }

    // 3. LOAD EVERYTHING USING vkGetDeviceProcAddr — THIS IS THE TRUTH
    g_ext.vkCreateRayTracingPipelinesKHR       = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
    g_ext.vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
    g_ext.vkCmdTraceRaysKHR                    = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");

    g_ext.vkGetAccelerationStructureBuildSizesKHR    = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
    g_ext.vkCmdBuildAccelerationStructuresKHR       = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
    g_ext.vkCreateAccelerationStructureKHR          = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
    g_ext.vkDestroyAccelerationStructureKHR         = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
    g_ext.vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");

    g_ext.vkCmdBeginRendering    = (PFN_vkCmdBeginRendering)vkGetDeviceProcAddr(device, "vkCmdBeginRendering");
    g_ext.vkCmdEndRendering      = (PFN_vkCmdEndRendering)vkGetDeviceProcAddr(device, "vkCmdEndRendering");
    g_ext.vkCmdPipelineBarrier2  = (PFN_vkCmdPipelineBarrier2)vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier2");

    g_ext.vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
    g_ext.vkGetDeviceFaultInfoEXT      = (PFN_vkGetDeviceFaultInfoEXT)vkGetDeviceProcAddr(device, "vkGetDeviceFaultInfoEXT");

    // 4. FINAL JUDGMENT — NO MERCY
    if (!g_ext.vkCreateRayTracingPipelinesKHR ||
        !g_ext.vkGetRayTracingShaderGroupHandlesKHR ||
        !g_ext.vkCmdTraceRaysKHR ||
        !g_ext.vkGetAccelerationStructureBuildSizesKHR ||
        !g_ext.vkCmdBuildAccelerationStructuresKHR) {
        fprintf(stderr, "[FATAL RTX] CRITICAL RAY TRACING EXTENSIONS MISSING:\n");
        fprintf(stderr, "  CreateRT:     %p\n", (void*)g_ext.vkCreateRayTracingPipelinesKHR);
        fprintf(stderr, "  GroupHandles: %p\n", (void*)g_ext.vkGetRayTracingShaderGroupHandlesKHR);
        fprintf(stderr, "  TraceRays:    %p\n", (void*)g_ext.vkCmdTraceRaysKHR);
        fprintf(stderr, "  BuildSizes:   %p\n", (void*)g_ext.vkGetAccelerationStructureBuildSizesKHR);
        fprintf(stderr, "  BuildAS:      %p\n", (void*)g_ext.vkCmdBuildAccelerationStructuresKHR);
        std::abort();
    }

    fprintf(stderr, "[RTX] ALL RAY TRACING + AS EXTENSIONS LOADED — g_ext IS LAW — PHOTONS ARMED\n");
}

// Public accessor
inline const Extensions& ext() noexcept { return g_ext; }

// Empire-approved macros
#define VK_CREATE_RT_PIPELINES(...)              g_ext.vkCreateRayTracingPipelinesKHR(__VA_ARGS__)
#define VK_GET_RT_GROUP_HANDLES(...)             g_ext.vkGetRayTracingShaderGroupHandlesKHR(__VA_ARGS__)
#define VK_CMD_TRACE_RAYS(cmd, ...)              g_ext.vkCmdTraceRaysKHR(cmd, __VA_ARGS__)
#define VK_GET_AS_BUILD_SIZES(...)               g_ext.vkGetAccelerationStructureBuildSizesKHR(__VA_ARGS__)
#define VK_CMD_BUILD_ACCELERATION_STRUCTURES(...) g_ext.vkCmdBuildAccelerationStructuresKHR(__VA_ARGS__)
#define VK_CREATE_ACCELERATION_STRUCTURE(...)    g_ext.vkCreateAccelerationStructureKHR(__VA_ARGS__)
#define VK_GET_AS_DEVICE_ADDRESS(...)            g_ext.vkGetAccelerationStructureDeviceAddressKHR(__VA_ARGS__)

} // namespace RTX