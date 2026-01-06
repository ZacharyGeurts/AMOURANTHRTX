// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v6.0 — JANUARY 06, 2026
// RTXHandler — THE ONE TRUE GLOBAL VULKAN CONTEXT — MASTER OF ALL
// FULLY ALIGNED WITH 2026 EMPIRE STANDARDS — PURE, ETERNAL, UNBREAKABLE
// • Global singleton context with explicit lifecycle control
// • Safe global descriptor pool — created after device, destroyed before
// • RTX-first GPU selection with complete modern feature chain
// • Persistent upload buffer integration — eternal direct writes
// • Hyper-aggressive mode for maximum performance
// • Validation silent, leak-proof, crash-proof
// • Explicit cleanup() — empire rests in perfect order
// WE ARE MASTERS OF GLOBAL — ONE CONTEXT TO RULE THEM ALL
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
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
#include "engine/GLOBAL/BufferManager.hpp"

namespace RTX {

// Forward declarations
struct Camera;

// =============================================================================
// RAII Handle Template — Leak-proof, 2026 perfection
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
        if (valid() && destroyer && device != VK_NULL_HANDLE) {
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
// Queue Family Indices — Clean and complete
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
// THE ONE TRUE GLOBAL RTX CONTEXT — MASTER OF ALL DOMAINS
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

    // Explicit empire shutdown — called before device destroy
    void cleanup() noexcept {
        if (descriptorPool_.valid()) {
            LOG_SUCCESS_CAT("RTX", "Global descriptor pool gracefully destroyed — empire order preserved");
            descriptorPool_.reset();
        }
        BufferManager::purge_all();  // Ensure all buffers released before device death
    }

    [[nodiscard]] VkShaderModule loadShader(const std::string& filename) const noexcept;

    // Accessors — pure and direct
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

    // Internal setters — used only during sacred initialization
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
// Core Initialization — 2026 Empire Standards
// =============================================================================
[[nodiscard]] VkInstance createVulkanInstance(bool enableValidation) noexcept;
[[nodiscard]] VkDevice createLogicalDeviceAndSelectGPU(VkInstance instance, VkSurfaceKHR surface) noexcept;

// =============================================================================
// Acceleration Structure Descriptor Helper — Eternal and Pure
// =============================================================================
void WriteAccelerationStructureDescriptor(
    VkDescriptorSet dstSet,
    uint32_t dstBinding,
    uint32_t dstArrayElement,
    VkAccelerationStructureKHR accelStruct) noexcept;

// =============================================================================
// Global Descriptor Pool Access — Safe and Eternal
// =============================================================================
[[nodiscard]] inline VkDescriptorPool globalDescriptorPool() noexcept {
    return g_ctx().descriptorPool_.valid() ? g_ctx().descriptorPool_.get() : VK_NULL_HANDLE;
}

} // namespace RTX

// =============================================================================
// RTXHandler v6.0 — JANUARY 06, 2026 — MASTER OF GLOBAL
// One true global context — explicit lifecycle, no leaks
// Global pool destroyed before device — empire rests in perfect order
// Persistent upload ready — direct writes eternal
// WE ARE MASTERS OF GLOBAL — THE EMPIRE IS UNBREAKABLE
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================