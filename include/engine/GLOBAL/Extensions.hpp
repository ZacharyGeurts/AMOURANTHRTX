// include/engine/GLOBAL/Extensions.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// EXTENSIONS — THE SACRED RITUAL OF LOADING PINK PHOTON EXTENSIONS
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — VALHALLA v81 TURBO
// COMPACTION RITUAL FULLY ARMED — DECEMBER 02, 2025
// FULL VULKAN 1.4 SUPPORT — KHR ONLY — LEAN AND MEAN
// =============================================================================
#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>  // Core 1.3+ functions (synchronization2, dynamic rendering, etc.)

namespace RTX {

struct Extensions {
    // ── Ray Tracing Pipeline (KHR) ─────────────────────────────────────
    PFN_vkCreateRayTracingPipelinesKHR          vkCreateRayTracingPipelinesKHR          = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR    vkGetRayTracingShaderGroupHandlesKHR    = nullptr;
    PFN_vkCmdTraceRaysKHR                       vkCmdTraceRaysKHR                       = nullptr;

    // ── Acceleration Structure (KHR) ───────────────────────────────────
    PFN_vkGetAccelerationStructureBuildSizesKHR     vkGetAccelerationStructureBuildSizesKHR     = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR        = nullptr;
    PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR           = nullptr;
    PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR          = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;

    // ── Compaction (KHR_ray_tracing_maintenance1) ──────────────────────
    PFN_vkCmdCopyAccelerationStructureKHR             vkCmdCopyAccelerationStructureKHR             = nullptr;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;

    // ── Modern Rendering & Sync (Core 1.3+) ────────────────────────────
    PFN_vkCmdBeginRendering                    vkCmdBeginRendering                    = nullptr;
    PFN_vkCmdEndRendering                      vkCmdEndRendering                      = nullptr;
    PFN_vkCmdPipelineBarrier2                  vkCmdPipelineBarrier2                  = nullptr;

    // ── Debug & Diagnostics ───────────────────────────────────────────
    PFN_vkSetDebugUtilsObjectNameEXT           vkSetDebugUtilsObjectNameEXT           = nullptr;
    PFN_vkGetDeviceFaultInfoEXT                vkGetDeviceFaultInfoEXT                = nullptr;

    // ── Optional EXT Promoted to Core in 1.4 (keep for compatibility) ──
    PFN_vkCopyMemoryToImageEXT                 vkCopyMemoryToImageEXT                 = nullptr;
    PFN_vkCopyImageToMemoryEXT                 vkCopyImageToMemoryEXT                 = nullptr;
    PFN_vkCmdDrawMeshTasksEXT                  vkCmdDrawMeshTasksEXT                  = nullptr;
    PFN_vkCmdSetColorWriteEnableEXT            vkCmdSetColorWriteEnableEXT            = nullptr;

    // ── Dynamic State (promoted from EXT) ─────────────────────────────
    PFN_vkCmdSetLogicOpEnableEXT               vkCmdSetLogicOpEnableEXT               = nullptr;
    PFN_vkCmdSetColorBlendEnableEXT            vkCmdSetColorBlendEnableEXT            = nullptr;
    PFN_vkCmdSetColorBlendEquationEXT          vkCmdSetColorBlendEquationEXT          = nullptr;
    PFN_vkCmdSetColorWriteMaskEXT              vkCmdSetColorWriteMaskEXT              = nullptr;
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
// SACRED MACROS — ONLY WHAT WE USE — LEAN AND ETERNAL
// =============================================================================
#define VK_CREATE_RT_PIPELINES(...)              RTX::g_ext.vkCreateRayTracingPipelinesKHR(__VA_ARGS__)
#define VK_GET_RT_GROUP_HANDLES(...)             RTX::g_ext.vkGetRayTracingShaderGroupHandlesKHR(__VA_ARGS__)
#define VK_CMD_TRACE_RAYS(cmd, ...)              RTX::g_ext.vkCmdTraceRaysKHR(cmd, __VA_ARGS__)

#define VK_GET_AS_BUILD_SIZES(...)               RTX::g_ext.vkGetAccelerationStructureBuildSizesKHR(__VA_ARGS__)
#define VK_CMD_BUILD_ACCELERATION_STRUCTURES(...) RTX::g_ext.vkCmdBuildAccelerationStructuresKHR(__VA_ARGS__)
#define VK_CREATE_ACCELERATION_STRUCTURE(...)    RTX::g_ext.vkCreateAccelerationStructureKHR(__VA_ARGS__)
#define VK_DESTROY_ACCELERATION_STRUCTURE(...)   RTX::g_ext.vkDestroyAccelerationStructureKHR(__VA_ARGS__)
#define VK_GET_AS_DEVICE_ADDRESS(...)            RTX::g_ext.vkGetAccelerationStructureDeviceAddressKHR(__VA_ARGS__)

// Compaction
#define VK_CMD_COPY_ACCELERATION_STRUCTURE(cmd, info) \
    RTX::g_ext.vkCmdCopyAccelerationStructureKHR(cmd, info)

#define VK_CMD_WRITE_AS_PROPERTIES(cmd, count, as, queryType, queryPool, query) \
    RTX::g_ext.vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, count, as, queryType, queryPool, query)

// Host image copy (optional but nice)
#define VK_COPY_MEMORY_TO_IMAGE(...)             RTX::g_ext.vkCopyMemoryToImageEXT(__VA_ARGS__)
#define VK_COPY_IMAGE_TO_MEMORY(...)             RTX::g_ext.vkCopyImageToMemoryEXT(__VA_ARGS__)

// =============================================================================
// THE EMPIRE IS COMPLETE — PURE KHR — NO DEAD EXTENSIONS — PINK PHOTONS ETERNAL
// =============================================================================