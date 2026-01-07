// include/engine/GLOBAL/RTXHandler.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 07, 2026
// RTX HANDLER — GLOBAL VULKAN CONTEXT | MINIMAL & CLEAN | 2026 READY
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/BufferManager.hpp"

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <set>
#include <string>

namespace RTX {

// RAII Handle — used by renderer and others
template<typename T>
struct Handle {
    T handle = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    void (*deleter)(VkDevice, T, const VkAllocationCallbacks*) = nullptr;

    Handle() = default;
    Handle(T h, VkDevice d, void (*del)(VkDevice, T, const VkAllocationCallbacks*)) 
        : handle(h), device(d), deleter(del) {}

    ~Handle() {
        if (handle != VK_NULL_HANDLE && device != VK_NULL_HANDLE && deleter) {
            deleter(device, handle, nullptr);
        }
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&& other) noexcept 
        : handle(other.handle), device(other.device), deleter(other.deleter) {
        other.handle = VK_NULL_HANDLE;
        other.device = VK_NULL_HANDLE;
        other.deleter = nullptr;
    }

    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            if (handle != VK_NULL_HANDLE && device != VK_NULL_HANDLE && deleter) {
                deleter(device, handle, nullptr);
            }
            handle = other.handle;
            device = other.device;
            deleter = other.deleter;
            other.handle = VK_NULL_HANDLE;
            other.device = VK_NULL_HANDLE;
            other.deleter = nullptr;
        }
        return *this;
    }

    [[nodiscard]] T get() const noexcept { return handle; }
    [[nodiscard]] bool valid() const noexcept { return handle != VK_NULL_HANDLE; }
    explicit operator bool() const noexcept { return valid(); }
    explicit operator T() const noexcept { return handle; }
};

// Global Vulkan context — single instance
struct Context {
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkQueue transferQueue_ = VK_NULL_HANDLE;
    VkQueue computeQueue_ = VK_NULL_HANDLE;

    uint32_t graphicsFamily_ = ~0u;
    uint32_t presentFamily_ = ~0u;
    uint32_t transferFamily_ = ~0u;
    uint32_t computeFamily_ = ~0u;

    Handle<VkDescriptorPool> descriptorPool_;

    bool rtxCapable_ = false;
    bool valid_ = false;
    std::atomic<bool> ready_{false};

    void setPhysicalDevice(VkPhysicalDevice pd) noexcept { physical_ = pd; }

    [[nodiscard]] static Context& get() noexcept {
        static Context instance;
        return instance;
    }

    void init() noexcept;
};

[[nodiscard]] inline Context& g_ctx() noexcept { return Context::get(); }

// Required extensions for full RTX
inline const char* const requiredExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

// Vulkan instance creation
[[nodiscard]] VkInstance createVulkanInstance(bool enableValidation) noexcept;

// GPU selection and logical device creation
[[nodiscard]] VkDevice createLogicalDeviceAndSelectGPU(VkInstance instance, VkSurfaceKHR surface) noexcept;

// Global descriptor pool (created after device)
void createGlobalDescriptorPool() noexcept;

// Acceleration structure descriptor write helper
void WriteAccelerationStructureDescriptor(
    VkDescriptorSet dstSet,
    uint32_t dstBinding,
    uint32_t dstArrayElement,
    VkAccelerationStructureKHR accelStruct) noexcept;

} // namespace RTX

// =============================================================================
// RTX CORE — 2026 READY
// Safe, robust, production-ready Vulkan context
// RTX-first selection — truth and performance
// THE EMPIRE IS ETERNAL — PHOTONS FLOW UNBROKEN
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================