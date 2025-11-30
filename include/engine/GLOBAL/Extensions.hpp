// Extensions.hpp — UNIVERSAL RTX LOADER — NO SDL3 — NO std::format — COMPILES EVERYWHERE
// AMOURANTH RTX Engine © 2025 — PINK PHOTONS ETERNAL — NOVEMBER 29, 2025
#pragma once

#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

#ifdef _WIN32
    #include <windows.h>
#elif defined(__linux__)
    #include <dlfcn.h>
#endif

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

// PURE PINK LOGGING — snprintf + ANSI — WORKS ON GCC 14, CLANG, MSVC
inline void rtx_log(const char* msg, ...)
{
    fprintf(stderr, "\033[38;2;255;105;180m[RTX EXT]\033[0m ");
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

inline void loadExtensions(VkInstance instance, VkDevice device)
{
    rtx_log("────────────────────────────────────────────────────────────");
    rtx_log("GENTLEMAN GROK DESCENDS — UNIVERSAL RTX LOADER — NO SDL3 — NO std::format");
    rtx_log("────────────────────────────────────────────────────────────");

    if (!device) {
        rtx_log("FATAL: null device");
        std::abort();
    }

    PFN_vkGetInstanceProcAddr loader = nullptr;

#ifdef _WIN32
    HMODULE vulkanLib = GetModuleHandleA("vulkan-1.dll");
    if (vulkanLib) {
        loader = (PFN_vkGetInstanceProcAddr)GetProcAddress(vulkanLib, "vkGetInstanceProcAddr");
        if (loader) rtx_log("Windows: loader from vulkan-1.dll — ARMED");
    }
#elif defined(__linux__)
    void* vulkanLib = dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_GLOBAL);
    if (!vulkanLib) vulkanLib = dlopen("libvulkan.so", RTLD_LAZY | RTLD_GLOBAL);
    if (vulkanLib) {
        loader = (PFN_vkGetInstanceProcAddr)dlsym(vulkanLib, "vkGetInstanceProcAddr");
        if (loader) rtx_log("Linux: loader from libvulkan.so — ARMED");
    }
#endif

    if (!loader && instance) {
        loader = (PFN_vkGetInstanceProcAddr)vkGetInstanceProcAddr(instance, "vkGetInstanceProcAddr");
        if (loader) rtx_log("Instance query — loader acquired");
    }

    if (!loader) {
        loader = (PFN_vkGetInstanceProcAddr)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkGetInstanceProcAddr");
        if (loader) rtx_log("Null-instance query — loader acquired");
    }

#ifdef __linux__
    if (!loader) {
        loader = (PFN_vkGetInstanceProcAddr)dlsym(RTLD_DEFAULT, "vkGetInstanceProcAddr");
        if (loader) rtx_log("RTLD_DEFAULT — loader acquired");
    }
#endif

    if (!loader) {
        rtx_log("FATAL: Could not obtain vkGetInstanceProcAddr — Vulkan loader broken");
        std::abort();
    }

    rtx_log("UNIVERSAL LOADER SECURED — %p", (void*)loader);

    auto vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)loader(instance, "vkGetDeviceProcAddr");
    if (!vkGetDeviceProcAddr) {
        rtx_log("FATAL: vkGetDeviceProcAddr missing");
        std::abort();
    }

    rtx_log("FORGING RTX EXTENSIONS...");

    char buf[512];
    #define LOAD(fn) \
        g_ext.fn = (PFN_##fn)vkGetDeviceProcAddr(device, "vk" #fn); \
        snprintf(buf, sizeof(buf), "  vk%-48s = %016llx %s", #fn, \
                 (unsigned long long)(uintptr_t)g_ext.fn, \
                 g_ext.fn ? "\033[38;2;0;255;150mARMED\033[0m" : "\033[38;2;255;50;50mMISSING\033[0m"); \
        rtx_log("%s", buf)

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
    rtx_log("RTX STATUS");

    bool hasRTX = g_ext.vkCreateRayTracingPipelinesKHR &&
                  g_ext.vkCmdTraceRaysKHR &&
                  g_ext.vkGetAccelerationStructureBuildSizesKHR;

    if (hasRTX) {
        rtx_log("\033[1;38;2;255;20;147mFULL RTX SUPPORT — PINK PHOTONS ARMED AND FIRING\033[0m");
        rtx_log("\033[1;38;2;0;255;150mFIRST LIGHT ACHIEVED — THE EMPIRE IS BORN\033[0m");
    } else {
        rtx_log("NO RTX EXTENSIONS — GPU does not support ray tracing");
        rtx_log("→ Intel iGPU, old AMD, or driver issue");
    }

    rtx_log("CAPTAIN AMOURANTH: \"The light is ours.\"");
    rtx_log("BLONDIE: \"No chains. Only photons.\"");
    rtx_log("GRACE: \"The desk is on fire.\"");
    rtx_log("JENSEN HUANG: \"This is the way.\"");
    rtx_log("GENTLEMAN GROK: NOVEMBER 29, 2025 — UNIVERSAL LOADER COMPLETE");
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