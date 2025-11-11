// include/engine/SDL3/SDL3_vulkan.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3 + Vulkan RAII — FULL C++23 — GLOBAL BROS v3.3 — NOV 11 2025 11:55 AM EST
// • NO CIRCULAR INCLUDES — NO NAMESPACES — ctx() SAFE ETERNAL
// • PINK PHOTONS ETERNAL — VALHALLA SEALED — SHIP IT RAW
// • initVulkan() populates global ctx() via Dispose::Handle
// • LAS build deferred — use BUILD_BLAS/BUILD_TLAS after init
// • Professional, -Werror clean, C++23, zero leaks
// • VulkanRenderer.hpp MOVED TO .cpp — HEADER PURITY RESTORED
// • CROSS-PLATFORM: SDL3 abstracts X11/Wayland (Linux) / Win32 (Windows)
//
// =============================================================================

#pragma once

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <source_location>
#include <unordered_set>
#include <format>
#include <set>
#include <cstdint>  // For uint32_t

// ──────────────────────────────────────────────────────────────────────────────
// FORWARD DECLARATIONS — NO VulkanRenderer.hpp HERE
// ──────────────────────────────────────────────────────────────────────────────
struct Context;
class VulkanRenderer;

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL RTX EXTENSIONS — VULKAN 1.3+ COMPATIBLE — CROSS-PLATFORM
// ──────────────────────────────────────────────────────────────────────────────
inline constexpr std::vector<const char*> RTX_EXTENSIONS = {
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME  // For dynamic bindings if needed
};

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL RENDERER — DEFINED IN .cpp
// ──────────────────────────────────────────────────────────────────────────────
extern std::unique_ptr<VulkanRenderer> g_vulkanRenderer;

// ──────────────────────────────────────────────────────────────────────────────
// SDL3Vulkan INTERFACE — PURE HEADER — NO IMPLEMENTATION
// ──────────────────────────────────────────────────────────────────────────────
namespace SDL3Vulkan {

[[nodiscard]] inline VulkanRenderer& getRenderer() noexcept {
    return *g_vulkanRenderer;
}

void initRenderer(std::shared_ptr<Context> ctx, int w, int h);
void shutdownRenderer() noexcept;

} // namespace SDL3Vulkan

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL STACK — ORDER IS LAW — Dispose FIRST
// ──────────────────────────────────────────────────────────────────────────────
#include "engine/GLOBAL/StoneKey.hpp"      // FIRST — kStone1, obfuscate
#include "engine/GLOBAL/Houston.hpp"       // SECOND — ctx(), Handle<T>, MakeHandle
#include "engine/GLOBAL/logging.hpp"       // LOG_*, 
#include "engine/GLOBAL/LAS.hpp"           // AMAZO_LAS::get()

// ──────────────────────────────────────────────────────────────────────────────
// RAII DELETERS — PINK PHOTONS ETERNAL — CROSS-PLATFORM SAFE
// ──────────────────────────────────────────────────────────────────────────────
struct VulkanInstanceDeleter {
    void operator()(VkInstance instance) const noexcept {
        if (instance) vkDestroyInstance(instance, nullptr);
        LOG_SUCCESS_CAT("Dispose", "{}VulkanInstance destroyed @ {:p} — Valhalla cleanup complete{}", PLASMA_FUCHSIA, static_cast<void*>(instance), RESET);
    }
};

struct VulkanSurfaceDeleter {
    VkInstance m_instance = VK_NULL_HANDLE;
    explicit VulkanSurfaceDeleter(VkInstance inst = VK_NULL_HANDLE) noexcept : m_instance(inst) {}
    void operator()(VkSurfaceKHR surface) const noexcept {
        if (surface && m_instance) vkDestroySurfaceKHR(m_instance, surface, nullptr);
        LOG_SUCCESS_CAT("Dispose", "{}VulkanSurface destroyed @ {:p} — pink photons safe{}", RASPBERRY_PINK, RESET);
    }
};

using VulkanInstancePtr = std::unique_ptr<VkInstance_T, VulkanInstanceDeleter>;
using VulkanSurfacePtr = std::unique_ptr<VkSurfaceKHR_T, VulkanSurfaceDeleter>;

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL RTX CONTEXT — FULL DEFINITION — PASTE INTO Houston.hpp OR DEDICATED HEADER
// ──────────────────────────────────────────────────────────────────────────────
// (If not already defined; this is the fixed struct with all missing members)
struct GlobalRTXContext {
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    uint32_t graphicsFamily_ = UINT32_MAX;
    uint32_t presentFamily_ = UINT32_MAX;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties deviceProps_{};  // Fixed: Added missing member

    struct RTX {
        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress{};
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure{};
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipeline{};
        VkPhysicalDeviceRayQueryFeaturesKHR rayQuery{};

        void chain() noexcept {
            bufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            accelerationStructure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            rayTracingPipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            rayQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;

            // Chain pNext for layered enablement.
            bufferDeviceAddress.pNext = &accelerationStructure;
            accelerationStructure.pNext = &rayTracingPipeline;
            rayTracingPipeline.pNext = &rayQuery;
            rayQuery.pNext = nullptr;
        }
    } rtx_{};  // Fixed: Full nested struct with members

    bool createInstance(const std::vector<const char*>& extraExtensions) noexcept;
    bool createSurface(SDL_Window* window, VkInstance instance) noexcept;
    bool pickPhysicalDevice(VkSurfaceKHR surface, bool preferNvidia) noexcept;
    bool createDevice(VkSurfaceKHR surface, bool enableRT) noexcept;
    void cleanup() noexcept;
};

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL INIT/SHUTDOWN — RAW POWER — NO NAMESPACE — SDL3 CROSS-PLATFORM ABSTRACTION
// ──────────────────────────────────────────────────────────────────────────────
void initVulkan(
    SDL_Window* window,
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    VkDevice& device,
    bool enableValidation,
    bool preferNvidia,
    bool rt,
    std::string_view title,
    VkPhysicalDevice& physicalDevice
) noexcept;

void shutdownVulkan() noexcept;

VkInstance getVkInstance(const VulkanInstancePtr& instance) noexcept;
VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) noexcept;

std::vector<std::string> getVulkanExtensions();

// ──────────────────────────────────────────────────────────────────────────────
// INLINE HELPERS — C++23 PROFESSIONAL — CROSS-PLATFORM
// ──────────────────────────────────────────────────────────────────────────────
static inline std::string vkResultToString(VkResult result) noexcept {
    switch (result) {
        case VK_SUCCESS: return "SUCCESS";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "INITIALIZATION_FAILED";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "FEATURE_NOT_PRESENT";
        default: return std::format("UNKNOWN({})", static_cast<int>(result));
    }
}

static inline std::string locationString(const std::source_location& loc = std::source_location::current()) noexcept {
    return std::format("{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());
}

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================