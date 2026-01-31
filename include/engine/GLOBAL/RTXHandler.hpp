// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL
// RTX HANDLER — GLOBAL VULKAN CONTEXT | MODERN C++23 | RTX-FIRST
// Version 30.76 — January 30, 2026
// - No timeline semaphores — totalTime monolith drives everything
// - Descriptor pool created externally (after first AS build)
// - VK_EXT_descriptor_buffer explicitly supported & preferred
// - All sealing centralized via StoneKey
// - Minimal global state, fire-and-forget submits
// - Handle<T> is fully header-only (inline) — no linker issues
// PINK PHOTONS ETERNAL — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <atomic>
#include <optional>
#include <string_view>

namespace RTX {

// Forward declarations (minimal — avoid heavy includes here)
namespace Options { namespace Debug { extern bool ENABLE_VALIDATION_LAYERS; } }
namespace StoneKey {
    void stone_seal_device_resources(VkInstance, VkDevice, VkPhysicalDevice, VkSurfaceKHR, VkSwapchainKHR);
    void stone_seal_queues(VkQueue g, VkQueue p, VkQueue c, VkQueue t);
    void stone_seal_families(uint32_t g, uint32_t p, uint32_t t, uint32_t c);
    VkDevice stone_device();  // usually returns g_ctx().device
}

// =============================================================================
// Minimal RAII handle for Vulkan objects (device-owned) — FULLY INLINE
// =============================================================================
template<typename T>
class Handle {
public:
    Handle() = default;

    Handle(T h, VkDevice dev, void (*del)(VkDevice, T, const VkAllocationCallbacks*) = nullptr) noexcept
        : handle_(h), device_(dev), deleter_(del) {}

    ~Handle() { reset(); }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&& other) noexcept
        : handle_(other.handle_), device_(other.device_), deleter_(other.deleter_) {
        other.handle_  = VK_NULL_HANDLE;
        other.device_  = VK_NULL_HANDLE;
        other.deleter_ = nullptr;
    }

    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_  = other.handle_;
            device_  = other.device_;
            deleter_ = other.deleter_;
            other.handle_  = VK_NULL_HANDLE;
            other.device_  = VK_NULL_HANDLE;
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
        handle_  = VK_NULL_HANDLE;
        device_  = VK_NULL_HANDLE;
        deleter_ = nullptr;
    }

private:
    T handle_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    void (*deleter_)(VkDevice, T, const VkAllocationCallbacks*) = nullptr;
};

// =============================================================================
// Required device extensions (must all be present)
// =============================================================================
inline constexpr std::array<const char*, 7> requiredDeviceExtensions = {{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME
}};

// =============================================================================
// Global minimal context (true singleton)
// =============================================================================
struct Context {
    VkDevice         device         = VK_NULL_HANDLE;
    VkPhysicalDevice physical       = VK_NULL_HANDLE;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue  = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
    VkQueue computeQueue  = VK_NULL_HANDLE;

    uint32_t graphicsFamily = ~0u;
    uint32_t presentFamily  = ~0u;
    uint32_t transferFamily = ~0u;
    uint32_t computeFamily  = ~0u;

    bool rtxCapable = false;
    bool valid      = false;
    std::atomic<bool> ready{false};

    void setPhysicalDevice(VkPhysicalDevice pd) noexcept { physical = pd; }

    [[nodiscard]] static Context& get() noexcept;
    void init() noexcept;
};

[[nodiscard]] inline Context& g_ctx() noexcept { return Context::get(); }

// =============================================================================
// Public API
// =============================================================================

[[nodiscard]] VkInstance createVulkanInstance() noexcept;

[[nodiscard]] VkDevice createLogicalDeviceAndSelectGPU(
    VkInstance instance,
    VkSurfaceKHR surface) noexcept;

// Helper for writing acceleration structure descriptors
void writeAccelerationStructureDescriptor(
    VkDescriptorSet set,
    uint32_t binding,
    uint32_t arrayElement,
    VkAccelerationStructureKHR accelStruct) noexcept;

} // namespace RTX