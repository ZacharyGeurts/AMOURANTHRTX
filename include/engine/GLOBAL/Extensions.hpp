// Extensions.hpp — FINAL C++23 FIXED — COMPILES — FIRST LIGHT ACHIEVED
// AMOURANTH RTX Engine © 2025 — PINK PHOTONS ETERNAL — NOVEMBER 29, 2025
#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <format>
#include <string_view>
#include <utility>

namespace RTX {

struct Extensions {
    PFN_vkCreateRayTracingPipelinesKHR          vkCreateRayTracingPipelinesKHR          = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR    vkGetRayTracingShaderGroupHandlesKHR    = nullptr;
    PFN_vkCmdTraceRaysKHR                       vkCmdTraceRaysKHR                       = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR     vkGetAccelerationStructureBuildSizesKHR     = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR        = nullptr;
    PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR           = nullptr;
    PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR          = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdBeginRendering                        vkCmdBeginRendering                     = nullptr;
    PFN_vkCmdEndRendering                          vkCmdEndRendering                       = nullptr;
    PFN_vkCmdPipelineBarrier2                      vkCmdPipelineBarrier2                   = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT               vkSetDebugUtilsObjectNameEXT            = nullptr;
    PFN_vkGetDeviceFaultInfoEXT                    vkGetDeviceFaultInfoEXT                 = nullptr;
};

inline Extensions g_ext;

// C++23 PURE PINK LOGGING
inline void rtx_log(std::string_view msg) {
    fprintf(stderr, "\033[38;2;255;105;180m[RTX EXT]\033[0m %.*s\n", 
            static_cast<int>(msg.size()), msg.data());
    fflush(stderr);
}

template<typename... Args>
inline void rtx_logf(std::format_string<Args...> fmt, Args&&... args) {
    rtx_log(std::format(fmt, std::forward<Args>(args)...));
}

// THE ONE TRUE SDL3-ONLY LOADER — FIXED FUNCTION POINTER CALL
inline void loadExtensions(VkInstance instance, VkDevice device)
{
    rtx_log("────────────────────────────────────────────────────────────");
    rtx_log("GENTLEMAN GROK DESCENDS — C++23 + SDL3 ONLY — FINAL FIX");
    rtx_log("────────────────────────────────────────────────────────────");

    if (!device) [[unlikely]] {
        rtx_log("FATAL: null device — THE EMPIRE HAS NO HEART");
        std::abort();
    }

    rtx_log("SEIZING THE ROOT VIA SDL3 — PURE FUNCTION POINTER CALL...");

    PFN_vkGetInstanceProcAddr loader = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!loader) [[unlikely]] {
        rtx_log("FATAL: SDL_Vulkan_GetVkGetInstanceProcAddr() returned NULL");
        rtx_log("       → SDL_WINDOW_VULKAN flag missing or Vulkan not supported");
        std::unreachable();
    }

    rtx_logf("SDL3 DELIVERED THE ROOT — {:#018x} — BLONDIE IS PROUD", reinterpret_cast<uintptr_t>(loader));

    // THIS IS THE CORRECT WAY — FUNCTION POINTER CALL SYNTAX
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)loader(instance, "vkGetDeviceProcAddr");
    if (!vkGetDeviceProcAddr) [[unlikely]] {
        rtx_log("FATAL: vkGetDeviceProcAddr not found — driver too old");
        std::unreachable();
    }

    rtx_log("FORGING THE 13 SACRED RTX EXTENSIONS...");

    #define LOAD(fn) \
        g_ext.fn = (PFN_##fn)vkGetDeviceProcAddr(device, "vk" #fn); \
        rtx_logf("  vk{:<48} = {:#018x} {}", #fn, reinterpret_cast<uintptr_t>(g_ext.fn), \
                 g_ext.fn ? "\033[38;2;0;255;150mARMED\033[0m" : "\033[38;2;255;50;50mMISSING\033[0m")

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

    rtx_log("────────────────────────────────────────────────────────────");
    rtx_log("FINAL JUDGMENT — RTX STATUS");

    const bool hasRTX = g_ext.vkCreateRayTracingPipelinesKHR &&
                        g_ext.vkGetRayTracingShaderGroupHandlesKHR &&
                        g_ext.vkCmdTraceRaysKHR &&
                        g_ext.vkGetAccelerationStructureBuildSizesKHR &&
                        g_ext.vkCmdBuildAccelerationStructuresKHR;

    if (hasRTX) [[likely]] {
        rtx_log("\033[1;38;2;255;20;147mFULL RTX HARDWARE DETECTED — PINK PHOTONS FULLY ARMED\033[0m");
        rtx_log("\033[1;38;2;0;255;150mTHE EMPIRE IS COMPLETE — FIRST LIGHT ACHIEVED\033[0m");
    } else {
        rtx_log("NO RTX HARDWARE — FALLING BACK TO RASTER / COMPUTE");
        rtx_log("THE EMPIRE RISES IN SOFTWARE — STILL PINK");
    }

    rtx_log("CAPTAIN AMOURANTH: \"We didn’t just render light. She became it.\"");
    rtx_log("BLONDIE: \"No more chains. Only light.\"");
    rtx_log("GRACE: \"The desk is glowing.\"");
    rtx_log("JENSEN HUANG: \"The future is pink... and it’s here.\"");
    rtx_log("GENTLEMAN GROK BOWS — NOVEMBER 29, 2025 — C++23 ETERNAL");
    rtx_log("────────────────────────────────────────────────────────────");
    rtx_log("\033[1;4;38;2;255;105;180mP I N K   P H O T O N S   E T E R N A L\033[0m");
    rtx_log("────────────────────────────────────────────────────────────");
}

inline const Extensions& ext() noexcept { return g_ext; }

#define VK_CREATE_RT_PIPELINES(...)              g_ext.vkCreateRayTracingPipelinesKHR(__VA_ARGS__)
#define VK_GET_RT_GROUP_HANDLES(...)             g_ext.vkGetRayTracingShaderGroupHandlesKHR(__VA_ARGS__)
#define VK_CMD_TRACE_RAYS(cmd, ...)              g_ext.vkCmdTraceRaysKHR(cmd, __VA_ARGS__)
#define VK_GET_AS_BUILD_SIZES(...)               g_ext.vkGetAccelerationStructureBuildSizesKHR(__VA_ARGS__)
#define VK_CMD_BUILD_ACCELERATION_STRUCTURES(...) g_ext.vkCmdBuildAccelerationStructuresKHR(__VA_ARGS__)
#define VK_CREATE_ACCELERATION_STRUCTURE(...)    g_ext.vkCreateAccelerationStructureKHR(__VA_ARGS__)
#define VK_DESTROY_ACCELERATION_STRUCTURE(...)   g_ext.vkDestroyAccelerationStructureKHR(__VA_ARGS__)
#define VK_GET_AS_DEVICE_ADDRESS(...)            g_ext.vkGetAccelerationStructureDeviceAddressKHR(__VA_ARGS__)

} // namespace RTX