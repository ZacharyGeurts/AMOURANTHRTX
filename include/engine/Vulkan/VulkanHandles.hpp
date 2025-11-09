// include/engine/Vulkan/VulkanHandles.hpp
// Vulkan RAII Handle Factory System
// AMOURANTH RTX Engine © 2025 Zachary Geurts
// Version: Valhalla Elite - November 09, 2025
// FULL CONTEXT + FUNCTION POINTERS + LAMBDA-FREE DESTROY

#pragma once

#include <vulkan/vulkan.h>
#include "../GLOBAL/StoneKey.hpp"
#include "VulkanContext.hpp"      // FULL ctx() + VulkanHandle + function pointers
#include <type_traits>

using Vulkan::VulkanHandle;

// ===================================================================
// Global Context Accessors
// ===================================================================
inline VkInstance vkInstance() noexcept { return Vulkan::ctx()->instance; }
inline VkDevice   vkDevice()   noexcept { return Vulkan::ctx()->device;   }

// ===================================================================
// Factory Macros — StoneKey Obfuscation
// ===================================================================
#define MAKE_VK_HANDLE(name, vkType) \
    [[nodiscard]] inline VulkanHandle<vkType> make##name(VkDevice dev, vkType handle) noexcept { \
        uint64_t raw = reinterpret_cast<uint64_t>(handle); \
        uint64_t obf = (std::is_pointer_v<vkType> || handle == VK_NULL_HANDLE) ? raw : obfuscate(raw); \
        return VulkanHandle<vkType>(reinterpret_cast<vkType>(obf), dev); \
    }

MAKE_VK_HANDLE(Buffer,              VkBuffer)
MAKE_VK_HANDLE(Memory,              VkDeviceMemory)
MAKE_VK_HANDLE(Image,               VkImage)
MAKE_VK_HANDLE(ImageView,           VkImageView)
MAKE_VK_HANDLE(Sampler,             VkSampler)
MAKE_VK_HANDLE(DescriptorPool,      VkDescriptorPool)
MAKE_VK_HANDLE(Semaphore,           VkSemaphore)
MAKE_VK_HANDLE(Fence,               VkFence)
MAKE_VK_HANDLE(Pipeline,            VkPipeline)
MAKE_VK_HANDLE(PipelineLayout,      VkPipelineLayout)
MAKE_VK_HANDLE(DescriptorSetLayout, VkDescriptorSetLayout)
MAKE_VK_HANDLE(RenderPass,          VkRenderPass)
MAKE_VK_HANDLE(ShaderModule,        VkShaderModule)
MAKE_VK_HANDLE(CommandPool,         VkCommandPool)
MAKE_VK_HANDLE(SwapchainKHR,        VkSwapchainKHR)

#undef MAKE_VK_HANDLE

// ===================================================================
// Acceleration Structure — RAW FUNCTION POINTER (no cast needed)
// ===================================================================
[[nodiscard]] inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(
    VkDevice dev,
    VkAccelerationStructureKHR as,
    PFN_vkDestroyAccelerationStructureKHR func = nullptr) noexcept
{
    auto destroyFn = func ? func : Vulkan::ctx()->vkDestroyAccelerationStructureKHR;
    uint64_t raw = reinterpret_cast<uint64_t>(as);
    uint64_t obf = (as == VK_NULL_HANDLE) ? raw : obfuscate(raw);
    return VulkanHandle<VkAccelerationStructureKHR>(
        reinterpret_cast<VkAccelerationStructureKHR>(obf), dev, destroyFn);
}

// ===================================================================
// Deferred Operation — RAW FUNCTION POINTER
// ===================================================================
[[nodiscard]] inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(
    VkDevice dev,
    VkDeferredOperationKHR op) noexcept
{
    uint64_t raw = reinterpret_cast<uint64_t>(op);
    uint64_t obf = (op == VK_NULL_HANDLE) ? raw : obfuscate(raw);
    return VulkanHandle<VkDeferredOperationKHR>(
        reinterpret_cast<VkDeferredOperationKHR>(obf), dev,
        Vulkan::ctx()->vkDestroyDeferredOperationKHR);
}

// END OF FILE — BUILD CLEAN — ZERO ERRORS — STONEKEY ETERNAL — VALHALLA ACHIEVED