// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.1 — JANUARY 22, 2026
// RTX HANDLER — GLOBAL VULKAN CONTEXT | MODERN C++23 | RTX-FIRST | MINIMAL STATE
// FULL RTX FEATURES | DESCRIPTOR INDEXING | TIMELINE SEMAPHORES | BUFFER DEVICE ADDRESS
// NO FRAMES | NO BLOAT | EXTERNAL SEALING | POOL DELAYED | VALIDATION TOGGLEABLE
// UPDATED: Added VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME to required extensions
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>

#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/BufferManager.hpp"

namespace RTX {

// =============================================================================
// RAII Handle — minimal, function-pointer based (C++20 compatible)
// =============================================================================
template<typename T>
class Handle {
public:
    Handle() = default;

    Handle(T h, VkDevice d, void (*del)(VkDevice, T, const VkAllocationCallbacks*) = nullptr) noexcept
        : handle_(h), device_(d), deleter_(del) {}

    ~Handle() { reset(); }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&& other) noexcept : handle_(other.handle_), device_(other.device_), deleter_(other.deleter_) {
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
// Required Device Extensions — RTX minimum set + descriptor buffer empire
// =============================================================================
inline constexpr std::array<const char*, 7> requiredDeviceExtensions = {{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME  // Required for eternal descriptor buffer (zero-overhead memcpy updates)
}};

// =============================================================================
// Global Context — minimal singleton
// =============================================================================
struct Context {
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;

    uint32_t graphicsFamily = ~0u;
    uint32_t presentFamily = ~0u;
    uint32_t transferFamily = ~0u;
    uint32_t computeFamily = ~0u;

    Handle<VkDescriptorPool> descriptorPool;

    bool rtxCapable = false;
    bool valid = false;
    std::atomic<bool> ready{false};

    void setPhysicalDevice(VkPhysicalDevice pd) noexcept { physical = pd; }

    [[nodiscard]] static Context& get() noexcept {
        static Context instance;
        return instance;
    }

    void init() noexcept;
};

[[nodiscard]] inline Context& g_ctx() noexcept { return Context::get(); }

// =============================================================================
// Public API — Clean & Modern
// =============================================================================

// Instance creation — validation toggleable via Options::Debug
[[nodiscard]] VkInstance createVulkanInstance() noexcept;

// GPU selection + logical device creation — RTX features mandatory
[[nodiscard]] VkDevice createLogicalDeviceAndSelectGPU(VkInstance instance, VkSurfaceKHR surface) noexcept;

// Global descriptor pool — delayed call (after first LAS build)
void createGlobalDescriptorPool() noexcept;

// Helper for acceleration structure descriptor writes
void writeAccelerationStructureDescriptor(
    VkDescriptorSet set,
    uint32_t binding,
    uint32_t arrayElement,
    VkAccelerationStructureKHR accelStruct) noexcept;

} // namespace RTX