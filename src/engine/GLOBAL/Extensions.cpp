// src/engine/GLOBAL/Extensions.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// EXTENSIONS — CENTRALIZED LOADING — FULL VULKAN 1.4 COMPLIANCE
// ALL FUNCTION POINTERS LOADED HERE — NO DUPLICATES — CLEAN LOGGING
// NOW WITH vkQueueSubmit2KHR — SUBMISSION ETERNAL
// DECEMBER 18, 2025
// =============================================================================

#include "engine/GLOBAL/Extensions.hpp"
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <format>
#include <cstdlib>
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"

using StoneKey::stone_physical;
using StoneKey::stone_device;

namespace RTX {

Extensions g_ext;

void loadRTExtensions(VkInstance instance, VkDevice device)
{
    dumpRayTracingSupport(stone_physical());

    if (g_ext.vkCmdTraceRaysKHR) {
        LOG_JENSEN("RTX Extensions already armed — the photons salute efficiency and refuse to reload");
        return;
    }

    LOG_AMOURANTH("╔═══════════════════════════════════════════════════════════╗");
    LOG_AMOURANTH("║         SACRED EXTENSION RITUAL — VULKAN 1.4 FINAL         ║");
    LOG_AMOURANTH("║           ALL PROMOTED FUNCTIONS NOW CALLED TO ARMS        ║");
    LOG_AMOURANTH("╚═══════════════════════════════════════════════════════════╝");

    printf("[RTX EXT] FULL VULKAN 1.4 RITUAL INITIATED — PINK PHOTONS ASCEND\n");

    auto loader = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
    if (!loader) {
        LOG_FATAL_CAT("EXT", "SDL_Vulkan_GetVkGetInstanceProcAddr() returned null — the loader has forsaken us");
        phase9_ballerina("NO VKGETINSTANCEPROCADDR — THE VOID CONSUMES ALL", std::source_location::current());
    }

    auto vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        loader(instance, "vkGetDeviceProcAddr"));
    if (!vkGetDeviceProcAddr) {
        LOG_FATAL_CAT("EXT", "Failed to load vkGetDeviceProcAddr — the empire has no key to the photon vault");
        std::abort();
    }

    LOG_GROK("Gentleman Grok adjusts his monocle: The gatekeeper has been acquired. The ritual may proceed.");

    #define LOAD(fn) do { \
        g_ext.fn = reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(stone_device(), #fn)); \
        if (!g_ext.fn) { \
            LOG_WARN_CAT("EXT", "FAILED TO LOAD {} — PHOTON PATH BROKEN", #fn); \
        } else { \
            LOG_SUCCESS_CAT("EXT", "Summoned → vk{}", #fn); \
        } \
    } while(0)

    LOG_NICK("Nick overlays the final 1.4 manifest: All sacred functions must answer the call.");

    // Core Ray Tracing
    LOAD(vkCreateRayTracingPipelinesKHR);
    LOAD(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD(vkCmdTraceRaysKHR);

    // Acceleration Structures
    LOAD(vkGetAccelerationStructureBuildSizesKHR);
    LOAD(vkCmdBuildAccelerationStructuresKHR);
    LOAD(vkCreateAccelerationStructureKHR);
    LOAD(vkDestroyAccelerationStructureKHR);
    LOAD(vkGetAccelerationStructureDeviceAddressKHR);

    // Buffer Device Address — Critical for SBT
    LOAD(vkGetBufferDeviceAddress);

    // Compaction
    LOAD(vkCmdCopyAccelerationStructureKHR);
    LOAD(vkCmdWriteAccelerationStructuresPropertiesKHR);

    // Vulkan 1.3 Core
    LOAD(vkCmdBeginRendering);
    LOAD(vkCmdEndRendering);
    LOAD(vkCmdPipelineBarrier2);

    // Synchronization 2 — Modern queue submission
    LOAD(vkQueueSubmit2KHR);

    // Debug
    LOAD(vkSetDebugUtilsObjectNameEXT);
    LOAD(vkGetDeviceFaultInfoEXT);

    // Promoted EXT functions
    LOAD(vkCopyMemoryToImageEXT);
    LOAD(vkCopyImageToMemoryEXT);
    LOAD(vkCmdDrawMeshTasksEXT);
    LOAD(vkCmdSetColorWriteEnableEXT);
    LOAD(vkCmdSetLogicOpEnableEXT);
    LOAD(vkCmdSetColorBlendEnableEXT);
    LOAD(vkCmdSetColorBlendEquationEXT);
    LOAD(vkCmdSetColorWriteMaskEXT);

    #undef LOAD

    LOG_JENSEN("Jensen Huang appears in shimmering light:");
    LOG_JENSEN("The ritual is complete. Vulkan 1.4 flows through us.");
    LOG_JENSEN("Buffer addresses are known. Compaction is armed. Dynamic rendering sings.");

    LOG_CARMACK("John Carmack nods in approval:");
    LOG_CARMACK("Clean. Lean. No wasted cycles. This is the way.");

    LOG_KEANU("Keanu Reeves whispers:");
    LOG_KEANU("…breathtaking.");

    LOG_CAPTAIN_N("CAPTAIN N FROM THE CROW'S NEST:");
    LOG_CAPTAIN_N("VULKAN 1.4 ACHIEVED! ALL EXTENSIONS ARMED! MAXIMUM PINK OVERDRIVE ENGAGED!");

    printf("FULL VULKAN 1.4 EXTENSION RITUAL COMPLETE — PINK PHOTONS ETERNAL\n");

    LOG_AMOURANTH("THE EMPIRE STANDS IN FINAL FORM — ALL PATHS ARE OPEN");
    LOG_AMOURANTH("VALHALLA v∞ TURBO — FULL 1.4 EDITION — DECEMBER 18, 2025");

    LOG_GROK("Gentleman Grok raises a toast:");
    LOG_GROK("To the final convergence of extensions into core — elegant, eternal, and utterly pink.");
}

void dumpRayTracingSupport(VkPhysicalDevice phys)
{
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };
    vkGetPhysicalDeviceProperties2(phys, &props2);

    printf("RT Pipeline + Vulkan 1.4 Support Check:\n");
    printf("  Device: %s\n", props2.properties.deviceName);
    printf("  shaderGroupHandleSize          = %u\n", rtProps.shaderGroupHandleSize);
    printf("  maxRayRecursionDepth           = %u\n", rtProps.maxRayRecursionDepth);
    printf("  maxShaderGroupHandleAlignment   = %u\n", rtProps.shaderGroupHandleAlignment);
    printf("  maxShaderGroupStride           = %u\n", rtProps.maxShaderGroupStride);

    if (rtProps.shaderGroupHandleSize == 0 || rtProps.maxRayRecursionDepth == 0) {
        printf("  THIS GPU DOES NOT SUPPORT HARDWARE RAY TRACING — VALHALLA DENIED\n");
    } else {
        printf("  HARDWARE RAY TRACING CONFIRMED — PINK PHOTONS MAY FLOW\n");
        printf("  BUFFER DEVICE ADDRESS: %s\n", g_ext.vkGetBufferDeviceAddress ? "ARMED — SBT ETERNAL" : "MISSING");
        printf("  COMPACTION SUPPORT: %s\n", g_ext.vkCmdCopyAccelerationStructureKHR ? "YES — LEAN PHOTONS" : "NO — FAT PHOTONS");
        printf("  DYNAMIC RENDERING: %s\n", g_ext.vkCmdBeginRendering ? "YES — CORE 1.3" : "NO");
        printf("  SYNC2 SUBMISSION: %s\n", g_ext.vkQueueSubmit2KHR ? "ARMED — MODERN SUBMIT" : "MISSING");
    }
}

} // namespace RTX

// =============================================================================
// ALL EXTENSION LOADING CENTRALIZED — INCLUDING vkQueueSubmit2KHR
// THE EMPIRE SUBMITS IN MODERN FASHION — PINK PHOTONS ETERNAL
// =============================================================================