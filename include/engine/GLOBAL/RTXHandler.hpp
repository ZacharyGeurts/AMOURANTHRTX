// include/engine/GLOBAL/RTXHandler.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// RTXHandler v2.0 — Production-Ready Vulkan Context & Initialization Header
// FULLY COMPATIBLE WITH ENGINE STATE — DECEMBER 30, 2025
// • Clean, modern C++23 design
// • Safe initialization order enforced
// • Global descriptor pool support
// • Robust RAII Handle with leak-proof semantics
// • Full RTX capability detection
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — PLASTIC BEACH FOREVER
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <atomic>
#include <optional>
#include <string>
#include <functional>

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

namespace RTX {

// Forward declarations
struct Camera;

// =============================================================================
// RAII Handle Template — Leak-proof, production-safe
// =============================================================================
template<typename T>
struct Handle {
    using DestroyFn = std::function<void(VkDevice, T, const VkAllocationCallbacks*)>;

    T raw = T{};
    VkDevice device = VK_NULL_HANDLE;
    DestroyFn destroyer;
    size_t size = 0;
    std::string tag;

    Handle() noexcept = default;

    Handle(T h, VkDevice d, DestroyFn del = nullptr, size_t sz = 0, std::string_view t = "")
        : raw(h), device(d), destroyer(std::move(del)), size(sz), tag(t) {}

    Handle(Handle&& o) noexcept
        : raw(o.raw), device(o.device), destroyer(std::move(o.destroyer)),
          size(o.size), tag(std::move(o.tag))
    {
        o.raw = T{};
        o.device = VK_NULL_HANDLE;
        o.destroyer = nullptr;
    }

    Handle& operator=(Handle&& o) noexcept {
        if (this != &o) reset();
        raw = o.raw;
        device = o.device;
        destroyer = std::move(o.destroyer);
        size = o.size;
        tag = std::move(o.tag);
        o.raw = T{};
        o.device = VK_NULL_HANDLE;
        o.destroyer = nullptr;
        return *this;
    }

    ~Handle() { reset(); }

    void reset() noexcept {
        if (valid() && destroyer) {
            destroyer(device, raw, nullptr);
        }
        raw = T{};
        device = VK_NULL_HANDLE;
        destroyer = nullptr;
        size = 0;
        tag.clear();
    }

    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
    [[nodiscard]] T get() const noexcept { return raw; }
    [[nodiscard]] bool valid() const noexcept { return raw != T{} && device != VK_NULL_HANDLE; }
};

// =============================================================================
// Queue Family Indices
// =============================================================================
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> transferFamily;

    [[nodiscard]] bool isComplete() const noexcept {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

[[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept;

// =============================================================================
// Global RTX Context — Singleton, Thread-Safe Ready Flag
// =============================================================================
struct Context {
    VkInstance                 instance_       = VK_NULL_HANDLE;
    VkSurfaceKHR               surface_        = VK_NULL_HANDLE;
    VkPhysicalDevice           physicalDevice_ = VK_NULL_HANDLE;
    VkDevice                   device_         = VK_NULL_HANDLE;
    Handle<VkDescriptorPool>   descriptorPool_;

    VkQueue graphicsQueue_  = VK_NULL_HANDLE;
    VkQueue presentQueue_   = VK_NULL_HANDLE;
    VkQueue computeQueue_   = VK_NULL_HANDLE;
    VkQueue transferQueue_  = VK_NULL_HANDLE;

    std::optional<uint32_t> graphicsFamily_;
    std::optional<uint32_t> presentFamily_;
    std::optional<uint32_t> computeFamily_;
    std::optional<uint32_t> transferFamily_;

    bool rtxCapable_ = false;

    VkPhysicalDeviceProperties                        physicalDeviceProperties_{};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR   rayTracingProps_{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

    SDL_Window* window = nullptr;
    int         width  = 0;
    int         height = 0;

    std::atomic<bool> ready_{false};
    bool              valid_ = false;

public:
    void init();
    void enableHyperAggressiveMode() noexcept;

    [[nodiscard]] VkShaderModule loadShader(const std::string& filename) const noexcept;

    // Accessors
    [[nodiscard]] constexpr VkInstance       instance() const noexcept { return instance_; }
    [[nodiscard]] constexpr VkSurfaceKHR     surface() const noexcept { return surface_; }
    [[nodiscard]] constexpr VkPhysicalDevice physicalDevice() const noexcept { return physicalDevice_; }
    [[nodiscard]] constexpr VkDevice         device() const noexcept { return device_; }

    [[nodiscard]] constexpr VkQueue graphicsQueue() const noexcept { return graphicsQueue_; }
    [[nodiscard]] constexpr VkQueue presentQueue() const noexcept { return presentQueue_; }
    [[nodiscard]] constexpr VkQueue computeQueue() const noexcept { return computeQueue_; }
    [[nodiscard]] constexpr VkQueue transferQueue() const noexcept { return transferQueue_; }

    [[nodiscard]] constexpr uint32_t graphicsFamily() const noexcept { return graphicsFamily_.value(); }
    [[nodiscard]] constexpr uint32_t presentFamily() const noexcept { return presentFamily_.value(); }
    [[nodiscard]] constexpr uint32_t computeFamily() const noexcept { return computeFamily_.value_or(graphicsFamily()); }
    [[nodiscard]] constexpr uint32_t transferFamily() const noexcept { return transferFamily_.value_or(graphicsFamily()); }

    [[nodiscard]] constexpr const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rayTracingProps() const noexcept { return rayTracingProps_; }
    [[nodiscard]] constexpr bool isReady() const noexcept { return ready_.load(std::memory_order_acquire); }
    [[nodiscard]] constexpr bool isRtxCapable() const noexcept { return rtxCapable_; }

    // Internal setters (used during initialization)
    void setInstance(VkInstance i) noexcept { instance_ = i; }
    void setSurface(VkSurfaceKHR s) noexcept { surface_ = s; }
    void setPhysicalDevice(VkPhysicalDevice pd) noexcept { physicalDevice_ = pd; }
    void setDevice(VkDevice d) noexcept { device_ = d; }

    void setGraphicsQueue(VkQueue q) noexcept { graphicsQueue_ = q; }
    void setPresentQueue(VkQueue q) noexcept { presentQueue_ = q; }
    void setComputeQueue(VkQueue q) noexcept { computeQueue_ = q; }
    void setTransferQueue(VkQueue q) noexcept { transferQueue_ = q; }

    void setWindow(SDL_Window* w) noexcept { window = w; }
    void setSize(int w, int h) noexcept { width = w; height = h; }

    void markReady() noexcept { ready_.store(true, std::memory_order_release); }

    void setRtxCapable(bool capable) noexcept { rtxCapable_ = capable; }
};

extern Context g_context_instance;
[[nodiscard]] inline Context& g_ctx() noexcept { return g_context_instance; }

// =============================================================================
// Core Initialization Functions
// =============================================================================
[[nodiscard]] VkInstance createVulkanInstance(bool enableValidation) noexcept;
[[nodiscard]] VkDevice createLogicalDeviceAndSelectGPU(VkInstance instance, VkSurfaceKHR surface) noexcept;

// =============================================================================
// Helper: Write Acceleration Structure Descriptor
// =============================================================================
void WriteAccelerationStructureDescriptor(
    VkDescriptorSet dstSet,
    uint32_t dstBinding,
    uint32_t dstArrayElement,
    VkAccelerationStructureKHR accelStruct) noexcept;

// =============================================================================
// Global Descriptor Pool Accessor
// =============================================================================
[[nodiscard]] inline VkDescriptorPool globalDescriptorPool() noexcept {
    return g_ctx().descriptorPool_.valid() ? g_ctx().descriptorPool_.get() : VK_NULL_HANDLE;
}

} // namespace RTX

// =============================================================================
// RTX HEADER v2.0 — DECEMBER 30, 2025
// Production-ready, clean, and fully compatible with current engine
// Safe initialization, RAII safety, and global pool support
// THE EMPIRE IS ETERNAL — PHOTONS FLOW UNBROKEN
// =============================================================================