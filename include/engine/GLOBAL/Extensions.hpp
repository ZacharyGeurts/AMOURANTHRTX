// =============================================================================
// Extensions.hpp — PURE, UNIVERSAL, ETERNAL — NO DEPENDENCIES ON YOUR CODEBASE
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <atomic>

// Forward declare the global GPU crash state — defined in exactly ONE .cpp file
namespace RTX {
    struct GPUCrashInfo {
        std::atomic<bool> happened{false};
        VkDeviceAddress   addr{0};
        VkDeviceFaultAddressTypeEXT type{VK_DEVICE_FAULT_ADDRESS_TYPE_NONE_EXT};
        VkDeviceSize      precision{0};
        char              desc[VK_MAX_DESCRIPTION_SIZE]{};
    };
    extern GPUCrashInfo g_gpu_crash;
}

namespace RTX {

// ─────────────────────────────────────────────────────────────────────────────
// EXTENSION FUNCTION POINTERS — THE EMPIRE'S ARSENAL
// ─────────────────────────────────────────────────────────────────────────────
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

    // VK_EXT_device_fault — THE GPU WILL CONFESS ITS SINS
    PFN_vkGetDeviceFaultInfoEXT                 vkGetDeviceFaultInfoEXT                 = nullptr;
};

// Global — forged once
inline Extensions g_ext;

// =============================================================================
// THE ONE TRUE EXTENSION LOADER — CALL ONCE AFTER VkDevice CREATION
// =============================================================================
inline void loadExtensions(VkInstance instance, VkDevice device)
{
    if (!instance || !device) {
        std::fprintf(stderr, "[FATAL RTX] loadExtensions(): null instance or device\n");
        std::abort();
    }

    auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!vkGetInstanceProcAddr) {
        std::fprintf(stderr, "[FATAL RTX] SDL_Vulkan_GetVkGetInstanceProcAddr() failed\n");
        std::abort();
    }

    // Load everything
    g_ext.vkCmdBeginRendering    = (PFN_vkCmdBeginRendering)vkGetInstanceProcAddr(instance, "vkCmdBeginRendering");
    g_ext.vkCmdEndRendering      = (PFN_vkCmdEndRendering)vkGetInstanceProcAddr(instance, "vkCmdEndRendering");
    g_ext.vkGetDescriptorEXT     = (PFN_vkGetDescriptorEXT)vkGetInstanceProcAddr(instance, "vkGetDescriptorEXT");

    g_ext.vkCmdPipelineBarrier2  = (PFN_vkCmdPipelineBarrier2)vkGetInstanceProcAddr(instance, "vkCmdPipelineBarrier2");
    g_ext.vkCmdWriteTimestamp2   = (PFN_vkCmdWriteTimestamp2)vkGetInstanceProcAddr(instance, "vkCmdWriteTimestamp2");
    g_ext.vkQueueSubmit2         = (PFN_vkQueueSubmit2)vkGetInstanceProcAddr(instance, "vkQueueSubmit2");

    g_ext.vkCreateRayTracingPipelinesKHR        = (PFN_vkCreateRayTracingPipelinesKHR)vkGetInstanceProcAddr(instance, "vkCreateRayTracingPipelinesKHR");
    g_ext.vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetInstanceProcAddr(instance, "vkGetRayTracingShaderGroupHandlesKHR");
    g_ext.vkCmdTraceRaysKHR                     = (PFN_vkCmdTraceRaysKHR)vkGetInstanceProcAddr(instance, "vkCmdTraceRaysKHR");

    g_ext.vkGetAccelerationStructureBuildSizesKHR    = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetInstanceProcAddr(instance, "vkGetAccelerationStructureBuildSizesKHR");
    g_ext.vkCmdBuildAccelerationStructuresKHR        = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetInstanceProcAddr(instance, "vkCmdBuildAccelerationStructuresKHR");
    g_ext.vkCreateAccelerationStructureKHR           = (PFN_vkCreateAccelerationStructureKHR)vkGetInstanceProcAddr(instance, "vkCreateAccelerationStructureKHR");
    g_ext.vkDestroyAccelerationStructureKHR          = (PFN_vkDestroyAccelerationStructureKHR)vkGetInstanceProcAddr(instance, "vkDestroyAccelerationStructureKHR");
    g_ext.vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetInstanceProcAddr(instance, "vkGetAccelerationStructureDeviceAddressKHR");

    g_ext.vkCmdSetEvent2   = (PFN_vkCmdSetEvent2)vkGetInstanceProcAddr(instance, "vkCmdSetEvent2");
    g_ext.vkCmdResetEvent2 = (PFN_vkCmdResetEvent2)vkGetInstanceProcAddr(instance, "vkCmdResetEvent2");
    g_ext.vkCmdWaitEvents2 = (PFN_vkCmdWaitEvents2)vkGetInstanceProcAddr(instance, "vkCmdWaitEvents2");

    g_ext.vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
    g_ext.vkGetDeviceFaultInfoEXT      = (PFN_vkGetDeviceFaultInfoEXT)vkGetInstanceProcAddr(instance, "vkGetDeviceFaultInfoEXT");

    // Critical check
    const std::array<void*, 9> critical = {
        reinterpret_cast<void*>(g_ext.vkCmdTraceRaysKHR),
        reinterpret_cast<void*>(g_ext.vkCreateRayTracingPipelinesKHR),
        reinterpret_cast<void*>(g_ext.vkGetRayTracingShaderGroupHandlesKHR),
        reinterpret_cast<void*>(g_ext.vkCmdBuildAccelerationStructuresKHR),
        reinterpret_cast<void*>(g_ext.vkCreateAccelerationStructureKHR),
        reinterpret_cast<void*>(g_ext.vkGetAccelerationStructureDeviceAddressKHR),
        reinterpret_cast<void*>(g_ext.vkCmdPipelineBarrier2),
        reinterpret_cast<void*>(g_ext.vkQueueSubmit2),
        reinterpret_cast<void*>(g_ext.vkGetDeviceFaultInfoEXT)
    };

    for (auto* ptr : critical) {
        if (!ptr) {
            std::fprintf(stderr, "[FATAL RTX] Missing critical extension — driver unworthy (NVIDIA 560+ required)\n");
            std::abort();
        }
    }

    std::fprintf(stderr, "[RTX] All extensions loaded successfully — RTX + VK_EXT_device_fault ACTIVE\n");
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

// =============================================================================
// GPU AUTOPSY — PURE, NO DEPENDENCIES, PRINTS DIRECTLY TO STDERR
// =============================================================================
inline void record_gpu_fault(VkDevice device) noexcept
{
    if (!device || !g_ext.vkGetDeviceFaultInfoEXT) return;

    VkDeviceFaultCountsEXT counts{};
    counts.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT;

    if (g_ext.vkGetDeviceFaultInfoEXT(device, &counts, nullptr) != VK_SUCCESS)
        return;

    if (counts.addressInfoCount == 0 && counts.vendorInfoCount == 0 && counts.vendorBinarySize == 0)
        return;

    std::vector<VkDeviceFaultAddressInfoEXT> addrInfos(counts.addressInfoCount);
    std::vector<VkDeviceFaultVendorInfoEXT>  vendorInfos(counts.vendorInfoCount);
    std::vector<uint8_t>                     vendorBinary(counts.vendorBinarySize);

    VkDeviceFaultInfoEXT info{};
    info.sType             = VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT;
    info.pAddressInfos     = addrInfos.data();
    info.pVendorInfos      = vendorInfos.data();
    info.pVendorBinaryData = vendorBinary.data();

    if (g_ext.vkGetDeviceFaultInfoEXT(device, &counts, &info) != VK_SUCCESS)
        return;

    if (counts.addressInfoCount > 0 && info.pAddressInfos) {
        const auto& a = info.pAddressInfos[0];
        g_gpu_crash.addr      = a.reportedAddress;
        g_gpu_crash.type      = a.addressType;
        g_gpu_crash.precision = a.addressPrecision;
    }

    if (info.description[0] != '\0') {
        std::strncpy(g_gpu_crash.desc, info.description, sizeof(g_gpu_crash.desc) - 1);
        g_gpu_crash.desc[sizeof(g_gpu_crash.desc) - 1] = '\0';
    }

    g_gpu_crash.happened.store(true, std::memory_order_release);

    std::fprintf(stderr, "\n");
    std::fprintf(stderr, "==================================================\n");
    std::fprintf(stderr, "           GPU PAGE FAULT — AUTOPSY COMPLETE       \n");
    std::fprintf(stderr, "==================================================\n");
    std::fprintf(stderr, "  Address     : 0x%llX\n", (unsigned long long)g_gpu_crash.addr);
    std::fprintf(stderr, "  Type        : %d\n", (int)g_gpu_crash.type);
    std::fprintf(stderr, "  Precision   : %llu bytes\n", (unsigned long long)g_gpu_crash.precision);
    std::fprintf(stderr, "  Message     : %s\n", g_gpu_crash.desc[0] ? g_gpu_crash.desc : "(no message)");
    std::fprintf(stderr, "==================================================\n");
    std::fflush(stderr);
}

} // namespace RTX