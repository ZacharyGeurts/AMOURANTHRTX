// include/engine/Vulkan/VulkanContext.hpp
// AMOURANTH RTX Engine © 2025 Zachary Geurts
// Version: Valhalla Elite - November 09, 2025
// FULL VULKAN CONTEXT DEFINITION — NOVEMBER 09, 2025
// SINGLE SOURCE OF TRUTH — NO COMMON.HPP DUPLICATES — CTX RETURNS shared_ptr — DAD FINAL

#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <functional>

namespace Vulkan {

struct Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    // KHR function pointers — RTX ETERNAL
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR = nullptr;
    PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR = nullptr;
    PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
};

// Global singleton — shared_ptr version (consistent everywhere)
[[nodiscard]] inline std::shared_ptr<Context>& ctx() noexcept {
    static std::shared_ptr<Context> instance = std::make_shared<Context>();
    return instance;
}

// ===================================================================
// VulkanHandle RAII Template — LAMBDA DESTROYERS — NO CAST ERRORS
// ===================================================================
template<typename T>
class VulkanHandle {
public:
    using DestroyFn = void(*)(VkDevice, T, const VkAllocationCallbacks*);

    VulkanHandle() noexcept = default;
    VulkanHandle(T handle, VkDevice dev, DestroyFn destroyer = nullptr) noexcept
        : handle_(handle), device_(dev), destroyer_(destroyer) {}

    ~VulkanHandle() { reset(); }

    VulkanHandle(const VulkanHandle&) = delete;
    VulkanHandle& operator=(const VulkanHandle&) = delete;

    VulkanHandle(VulkanHandle&& other) noexcept { *this = std::move(other); }
    VulkanHandle& operator=(VulkanHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            device_ = other.device_;
            destroyer_ = other.destroyer_;
            other.handle_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    [[nodiscard]] T raw() const noexcept { return handle_; }
    [[nodiscard]] T raw_deob() const noexcept { 
        return reinterpret_cast<T>(deobfuscate(reinterpret_cast<uint64_t>(handle_))); 
    }

    void reset() noexcept {
        if (handle_ != VK_NULL_HANDLE && destroyer_) {
            destroyer_(device_, handle_, nullptr);
        }
        handle_ = VK_NULL_HANDLE;
    }

    explicit operator bool() const noexcept { return handle_ != VK_NULL_HANDLE; }

private:
    T handle_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    DestroyFn destroyer_ = nullptr;
};

} // namespace Vulkan

// END OF FILE — SINGLE SOURCE — NO REDEFINITIONS — BUILD CLEAN — PINK PHOTONS ETERNAL