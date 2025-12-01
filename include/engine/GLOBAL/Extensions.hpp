// include/engine/GLOBAL/Extensions.hpp
// AMOURANTH RTX Engine © 2025 — PINK PHOTONS ETERNAL — NOVEMBER 30, 2025
#pragma once

#include <vulkan/vulkan.h>

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

// THE ONE TRUE GLOBAL — SEALED IN STONE
extern Extensions g_ext;

// CALL ONCE AFTER DEVICE CREATION
void loadRTExtensions(VkInstance instance, VkDevice device);
void dumpRayTracingSupport(VkPhysicalDevice physicalDevice);

// CLEAN ACCESSOR
[[nodiscard]] inline const Extensions& ext() noexcept { return g_ext; }

} // namespace RTX

// SACRED MACROS — MUST BE HERE — IN THE HEADER — SO ALL FILES SEE THEM
#define VK_CREATE_RT_PIPELINES(...)              RTX::g_ext.vkCreateRayTracingPipelinesKHR(__VA_ARGS__)
#define VK_GET_RT_GROUP_HANDLES(...)             RTX::g_ext.vkGetRayTracingShaderGroupHandlesKHR(__VA_ARGS__)
#define VK_CMD_TRACE_RAYS(cmd, ...)              RTX::g_ext.vkCmdTraceRaysKHR(cmd, __VA_ARGS__)
#define VK_GET_AS_BUILD_SIZES(...)               RTX::g_ext.vkGetAccelerationStructureBuildSizesKHR(__VA_ARGS__)
#define VK_CMD_BUILD_ACCELERATION_STRUCTURES(...) RTX::g_ext.vkCmdBuildAccelerationStructuresKHR(__VA_ARGS__)
#define VK_CREATE_ACCELERATION_STRUCTURE(...)    RTX::g_ext.vkCreateAccelerationStructureKHR(__VA_ARGS__)
#define VK_DESTROY_ACCELERATION_STRUCTURE(...)   RTX::g_ext.vkDestroyAccelerationStructureKHR(__VA_ARGS__)
#define VK_GET_AS_DEVICE_ADDRESS(...)            RTX::g_ext.vkGetAccelerationStructureDeviceAddressKHR(__VA_ARGS__)