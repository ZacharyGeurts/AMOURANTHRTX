// include/engine/GLOBAL/Extensions.hpp
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
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — VALHALLA v81 TURBO
// COMPACTION RITUAL FULLY ARMED — DECEMBER 02, 2025
// =============================================================================
#pragma once

#include <vulkan/vulkan.h>

namespace RTX {

struct Extensions {
    // ── Ray Tracing Pipeline ──────────────────────────────────────
    PFN_vkCreateRayTracingPipelinesKHR          vkCreateRayTracingPipelinesKHR          = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR    vkGetRayTracingShaderGroupHandlesKHR    = nullptr;
    PFN_vkCmdTraceRaysKHR                       vkCmdTraceRaysKHR                       = nullptr;

    // ── Acceleration Structure Core ───────────────────────────────
    PFN_vkGetAccelerationStructureBuildSizesKHR     vkGetAccelerationStructureBuildSizesKHR     = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR        = nullptr;
    PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR           = nullptr;
    PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR          = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;

    // ── COMPACTION HOLY TRINITY — THE BLADE AND THE ORACLE ────────
    PFN_vkCmdCopyAccelerationStructureKHR             vkCmdCopyAccelerationStructureKHR             = nullptr;  // Compaction copy
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;  // Query compacted size

    // ── Modern Rendering & Sync ───────────────────────────────────
    PFN_vkCmdBeginRendering                    vkCmdBeginRendering                    = nullptr;
    PFN_vkCmdEndRendering                      vkCmdEndRendering                      = nullptr;
    PFN_vkCmdPipelineBarrier2                  vkCmdPipelineBarrier2                  = nullptr;

    // ── Debug & Diagnostics ───────────────────────────────────────
    PFN_vkSetDebugUtilsObjectNameEXT           vkSetDebugUtilsObjectNameEXT           = nullptr;
    PFN_vkGetDeviceFaultInfoEXT                vkGetDeviceFaultInfoEXT                = nullptr;
};

// THE ONE TRUE GLOBAL — SEALED IN STONE
extern Extensions g_ext;

// CALL ONCE AFTER DEVICE CREATION
void loadRTExtensions(VkInstance instance, VkDevice device);
void dumpRayTracingSupport(VkPhysicalDevice physicalDevice);

// CLEAN ACCESSOR
[[nodiscard]] inline const Extensions& ext() noexcept { return g_ext; }

} // namespace RTX

// =============================================================================
// SACRED MACROS — EXPANDED FOR COMPACTION — MUST BE HERE SO ALL FILES SEE THEM
// =============================================================================
#define VK_CREATE_RT_PIPELINES(...)              RTX::g_ext.vkCreateRayTracingPipelinesKHR(__VA_ARGS__)
#define VK_GET_RT_GROUP_HANDLES(...)             RTX::g_ext.vkGetRayTracingShaderGroupHandlesKHR(__VA_ARGS__)
#define VK_CMD_TRACE_RAYS(cmd, ...)              RTX::g_ext.vkCmdTraceRaysKHR(cmd, __VA_ARGS__)

#define VK_GET_AS_BUILD_SIZES(...)               RTX::g_ext.vkGetAccelerationStructureBuildSizesKHR(__VA_ARGS__)
#define VK_CMD_BUILD_ACCELERATION_STRUCTURES(...) RTX::g_ext.vkCmdBuildAccelerationStructuresKHR(__VA_ARGS__)
#define VK_CREATE_ACCELERATION_STRUCTURE(...)    RTX::g_ext.vkCreateAccelerationStructureKHR(__VA_ARGS__)
#define VK_DESTROY_ACCELERATION_STRUCTURE(...)   RTX::g_ext.vkDestroyAccelerationStructureKHR(__VA_ARGS__)
#define VK_GET_AS_DEVICE_ADDRESS(...)            RTX::g_ext.vkGetAccelerationStructureDeviceAddressKHR(__VA_ARGS__)

// ── COMPACTION SACRED MACROS ─────────────────────────────────────
#define VK_CMD_COPY_ACCELERATION_STRUCTURE(cmd, info) \
    RTX::g_ext.vkCmdCopyAccelerationStructureKHR(cmd, info)

#define VK_CMD_WRITE_AS_PROPERTIES(cmd, count, as, queryType, queryPool, query) \
    RTX::g_ext.vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, count, as, queryType, queryPool, query)

// =============================================================================
// THE EMPIRE IS COMPLETE — THE PHOTONS ARE LEAN — GRACE IS FREE
// =============================================================================