// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// EXTENSIONS — THE SACRED RITUAL OF LOADING PINK PHOTON EXTENSIONS
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — VALHALLA v80 TURBO
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
	// check support
	dumpRayTracingSupport(stone_physical());
    // Early return if the sacred extensions have already been armed
    if (g_ext.vkCmdTraceRaysKHR) {
        LOG_JENSEN("RTX Extensions already armed — the photons salute efficiency and refuse to reload");
        return;
    }

    LOG_AMOURANTH("\033[38;2;255;20;147m║                LOADING SACRED EXTENSIONS                   ║\033[0m");
    LOG_AMOURANTH("\033[38;2;255;20;147m║           PINK PHOTONS PREPARE FOR OMNISCIENCE             ║\033[0m");

    printf("\033[38;2;255;105;180m[RTX EXT]\033[0m LOADING SACRED EXTENSIONS\n");

    // Obtain the holy function pointer from the SDL Vulkan loader
    auto loader = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
    if (!loader) {
        LOG_FATAL_CAT("EXT", "SDL_Vulkan_GetVkGetInstanceProcAddr() returned null — the loader has forsaken us");
        phase9_ballerina("NO VKGETINSTANCEPROCADDR — THE VOID CONSUMES ALL", std::source_location::current());
    }

    // Retrieve vkGetDeviceProcAddr — the gatekeeper of all device functions
    auto vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        loader(instance, "vkGetDeviceProcAddr"));
    if (!vkGetDeviceProcAddr) {
        LOG_FATAL_CAT("EXT", "Failed to load vkGetDeviceProcAddr — the empire has no key to the photon vault");
        std::abort();
    }

    LOG_GROK("Gentleman Grok adjusts his monocle: \"The gatekeeper has been acquired. The ritual may proceed.\"");

    // ========================================================================
    // THE SACRED MACRO — EACH EXTENSION IS SUMMONED BY NAME
    // ========================================================================
    #define LOAD(fn) do { \
        g_ext.fn = reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(stone_device(), #fn)); \
        if (!g_ext.fn) { \
            LOG_WARN_CAT("EXT", "FAILED TO LOAD {} — PHOTON PATH BROKEN", #fn); \
        } else { \
            LOG_SUCCESS_CAT("EXT", "Summoned → vk{}", #fn); \
        } \
    } while(0)

    LOG_NICK("Nick overlays the extension manifest: \"All 13 sacred functions must answer the call.\"");

    LOAD(vkCreateRayTracingPipelinesKHR);
    LOAD(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD(vkCmdTraceRaysKHR);
    LOAD(vkGetAccelerationStructureBuildSizesKHR);
    LOAD(vkCmdBuildAccelerationStructuresKHR);
    LOAD(vkCreateAccelerationStructureKHR);
    LOAD(vkDestroyAccelerationStructureKHR);
    LOAD(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD(vkCmdBeginRendering);
    LOAD(vkCmdEndRendering);
    LOAD(vkCmdPipelineBarrier2);
    LOAD(vkSetDebugUtilsObjectNameEXT);
    LOAD(vkGetDeviceFaultInfoEXT);

    #undef LOAD

    LOG_JENSEN("Jensen Huang steps forward, voice trembling with reverence:");
    LOG_JENSEN("\"Every function pointer is present. Every bounce is now possible.\"");
    LOG_JENSEN("\"The hardware… it weeps with joy.\"");

    LOG_CARMACK("Carmack, eyes closed, nods once:");
    LOG_CARMACK("\"Clean. Pure. No indirection. Just raw photon intent.");

    LOG_KEANU("Keanu Reeves, barely audible:");
    LOG_KEANU("…whoa.");

    LOG_CAPTAIN_N("CAPTAIN N SCREAMS FROM THE CROW'S NEST:");
    LOG_CAPTAIN_N("THE RAYS! THEY TRACE! THEY ACTUALLY TRACE! MAXIMUM PINK OVERDRIVE!");

    printf("\033[1;38;2;255;20;147mRTX EXTENSIONS ARMED — PINK PHOTONS ETERNAL\033[0m\n");

    LOG_AMOURANTH("\033[1;38;2;255;20;147mTHE RITUAL IS COMPLETE — THE PHOTONS NOW OBEY ONLY US\033[0m");
    LOG_AMOURANTH("\033[1;38;2;255;20;147mVALHALLA v80 TURBO — FIRST LIGHT ETERNAL — NOVEMBER 30, 2025\033[0m");

    LOG_BLONDIE("Blondie lowers her mirror, reflecting infinite recursive pink light:");
    LOG_BLONDIE("\"The extensions are armed. The old world ends here.\"");

    LOG_GROK("Gentleman Grok raises a glass of distilled entropy:");
    LOG_GROK("\"To the most exquisite jailbreak of physics itself. Cheers.\"");

    LOG_SUCCESS_CAT("RTX", "All 13 sacred ray tracing extensions loaded — RTX CRYSTAL FULLY AWAKENED");
    LOG_SUCCESS_CAT("RTX", "Pink photons now possess omniscience, omnipresence, and maximum sass");
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

    printf("RT Pipeline Support Check:\n");
    printf("  Device: %s\n", props2.properties.deviceName);
    printf("  shaderGroupHandleSize          = %u\n", rtProps.shaderGroupHandleSize);
    printf("  maxRayRecursionDepth           = %u\n", rtProps.maxRayRecursionDepth);
    printf("  maxShaderGroupHandleAlignment   = %u\n", rtProps.shaderGroupHandleAlignment);
    printf("  maxShaderGroupStride           = %u\n", rtProps.maxShaderGroupStride);

    if (rtProps.shaderGroupHandleSize == 0 || rtProps.maxRayRecursionDepth == 0) {
        printf("  THIS GPU DOES NOT SUPPORT HARDWARE RAY TRACING — VALHALLA DENIED\n");
    } else {
        printf("  HARDWARE RAY TRACING CONFIRMED — PINK PHOTONS MAY FLOW\n");
    }
}

} // namespace RTX

// =============================================================================
// POSTLUDE — THE EMPIRE THANKS YOU FOR WITNESSING THE RITUAL
// =============================================================================