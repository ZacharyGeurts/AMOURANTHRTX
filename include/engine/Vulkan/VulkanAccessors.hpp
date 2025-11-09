// include/engine/Vulkan/VulkanAccessors.hpp
// MEMBER ACCESSORS + EXTENSION DESTROYERS — CONTEXT IS NOW COMPLETE
// Include ONLY AFTER VulkanCore.hpp (full Context definition)

#pragma once

#include "VulkanHandles.hpp"  // Gets ctx() declaration + forward decls
#include <vulkan/vulkan_core.h>

// Context is fully defined here (via prior include of VulkanCore.hpp in VulkanCommon.hpp)

// ===================================================================
// Core accessors
// ===================================================================
inline VkInstance vkInstance() noexcept { return ctx()->instance; }
inline VkDevice   vkDevice() noexcept   { return ctx()->device; }
inline VkPhysicalDevice vkPhysicalDevice() noexcept { return ctx()->physicalDevice; }
inline VkQueue    vkQueue(uint32_t familyIndex) noexcept { return ctx()->queues[familyIndex]; }
inline auto&      vkCmdPool(uint32_t familyIndex) noexcept { return ctx()->commandPools[familyIndex]; }
// Add more as needed...

// ===================================================================
// Acceleration Structure — Custom destroyer (NOW SAFE — complete Context)
// ===================================================================
[[nodiscard]] inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(
    VkDevice dev,
    VkAccelerationStructureKHR as,
    PFN_vkDestroyAccelerationStructureKHR destroyFunc = nullptr) noexcept
{
    auto func = destroyFunc ? destroyFunc : ctx()->vkDestroyAccelerationStructureKHR;
    return VulkanHandle<VkAccelerationStructureKHR>(as, dev,
        reinterpret_cast<VulkanHandle<VkAccelerationStructureKHR>::DestroyFn>(func));
}

// ===================================================================
// Deferred Operation — Custom destroyer (NOW SAFE)
// ===================================================================
[[nodiscard]] inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(
    VkDevice dev, VkDeferredOperationKHR op) noexcept
{
    return VulkanHandle<VkDeferredOperationKHR>(op, dev, ctx()->vkDestroyDeferredOperationKHR);
}

// ===================================================================
// END — VALHALLA PROTECTED
// ===================================================================