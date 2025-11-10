// include/engine/Vulkan/VulkanHandles.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// Vulkan RAII Handle Factory System - Professional Production Edition
// Full StoneKey obfuscation, zero-cost, supports custom lambda deleters via std::function

#pragma once

#include <vulkan/vulkan.h>
#include <functional>
#include "../GLOBAL/StoneKey.hpp"
#include "VulkanContext.hpp"

namespace Vulkan {

/**
 * @brief Global context accessors
 */
inline VkInstance vkInstance() noexcept { return ctx()->instance; }
inline VkDevice   vkDevice()   noexcept { return ctx()->device;   }

/**
 * @brief Macro for creating standard Vulkan RAII handles with StoneKey obfuscation
 */
#define MAKE_VK_HANDLE(name, vkType) \
    [[nodiscard]] inline VulkanHandle<vkType> make##name(VkDevice dev, vkType handle) noexcept \
    { \
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

/**
 * @brief Deleter type for acceleration structures - supports full lambda capture
 */
using ASDeleter = std::function<void(VkDevice, VkAccelerationStructureKHR, const VkAllocationCallbacks*)>;

/**
 * @brief Creates RAII acceleration structure with custom deleter and StoneKey obfuscation
 */
[[nodiscard]] inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(
    VkDevice dev,
    VkAccelerationStructureKHR as,
    ASDeleter deleter = nullptr) noexcept
{
    if (!deleter) {
        deleter = [](VkDevice d, VkAccelerationStructureKHR a, const VkAllocationCallbacks* p) {
            ctx()->vkDestroyAccelerationStructureKHR(d, a, p);
        };
    }

    uint64_t raw = reinterpret_cast<uint64_t>(as);
    uint64_t obf = (as == VK_NULL_HANDLE) ? raw : obfuscate(raw);

    return VulkanHandle<VkAccelerationStructureKHR>(
        reinterpret_cast<VkAccelerationStructureKHR>(obf),
        dev,
        std::move(deleter));
}

/**
 * @brief Creates RAII deferred operation with raw function pointer deleter
 */
[[nodiscard]] inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(
    VkDevice dev,
    VkDeferredOperationKHR op) noexcept
{
    uint64_t raw = reinterpret_cast<uint64_t>(op);
    uint64_t obf = (op == VK_NULL_HANDLE) ? raw : obfuscate(raw);

    return VulkanHandle<VkDeferredOperationKHR>(
        reinterpret_cast<VkDeferredOperationKHR>(obf),
        dev,
        ctx()->vkDestroyDeferredOperationKHR);
}

} // namespace Vulkan