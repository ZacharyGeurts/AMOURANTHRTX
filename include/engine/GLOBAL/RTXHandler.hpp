// include/engine/GLOBAL/RTXHandler.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v28.1 — JANUARY 08, 2026
// RTX HANDLER — GLOBAL VULKAN CONTEXT | MODERN C++23 | SAFE & ETERNAL
// FULL RTX SUPPORT | ZERO-COST COMPATIBLE | VALIDATION PERFECT
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>

#include <functional>
#include <optional>

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/BufferManager.hpp"

namespace RTX {

// =============================================================================
// RAII Handle — Classic function pointer version (C++20 compatible)
// =============================================================================
template<typename T>
class Handle {
public:
    Handle() = default;

    Handle(T h, VkDevice d, void (*del)(VkDevice, T, const VkAllocationCallbacks*) = nullptr) noexcept
        : handle_(h), device_(d), deleter_(del) {}

    ~Handle() {
        reset();
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&& other) noexcept
        : handle_(other.handle_), device_(other.device_), deleter_(other.deleter_) {
        other.handle_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
        other.deleter_ = nullptr;
    }

    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            device_ = other.device_;
            deleter_ = other.deleter_;
            other.handle_ = VK_NULL_HANDLE;
            other.device_ = VK_NULL_HANDLE;
            other.deleter_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] T get() const noexcept { return handle_; }
    [[nodiscard]] bool valid() const noexcept { return handle_ != VK_NULL_HANDLE; }
    explicit operator bool() const noexcept { return valid(); }
    explicit operator T() const noexcept { return handle_; }

    void reset() noexcept {
        if (handle_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE && deleter_) {
            deleter_(device_, handle_, nullptr);
        }
        handle_ = VK_NULL_HANDLE;
        device_ = VK_NULL_HANDLE;
        deleter_ = nullptr;
    }

private:
    T handle_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    void (*deleter_)(VkDevice, T, const VkAllocationCallbacks*) = nullptr;
};

// =============================================================================
// Global Vulkan Context — Singleton with modern design
// =============================================================================
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

// =============================================================================
// Required Device Extensions — RTX Complete Set
// =============================================================================
inline constexpr std::array<const char*, 6> requiredExtensions = {{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
}};

// =============================================================================
// Public API — Clean and Modern
// =============================================================================

// Vulkan instance creation
[[nodiscard]] VkInstance createVulkanInstance(bool enableValidation = true) noexcept;

// GPU selection and logical device creation
[[nodiscard]] VkDevice createLogicalDeviceAndSelectGPU(VkInstance instance, VkSurfaceKHR surface) noexcept;

// Global descriptor pool (called internally after device creation)
// Declaration only — definition is in RTXHandler.cpp with static linkage
void createGlobalDescriptorPool() noexcept;

// Helper for writing acceleration structure descriptors
void WriteAccelerationStructureDescriptor(
    VkDescriptorSet dstSet,
    uint32_t dstBinding,
    uint32_t dstArrayElement,
    VkAccelerationStructureKHR accelStruct) noexcept;

} // namespace RTX

// =============================================================================
// RTX CORE v28.1 — JANUARY 08, 2026
// Modern C++23 | Classic function pointer Handle (C++20 compatible)
// constexpr arrays | Clean minimal includes
// Safe, robust, production-ready Vulkan context
// RTX-first selection — truth and performance
// THE EMPIRE IS ETERNAL — PHOTONS FLOW UNBROKEN
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================