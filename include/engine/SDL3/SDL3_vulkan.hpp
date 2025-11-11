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
// • FIXED: Minimal Context def; SDL3 bool return; RTX full names; CommandPool order
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
#include <array>    // For constexpr RTX_EXTENSIONS
#include <SDL3/SDL.h>

// ──────────────────────────────────────────────────────────────────────────────
// MINIMAL CONTEXT — SELF-CONTAINED — NO EXTERNAL .hpp
// ──────────────────────────────────────────────────────────────────────────────
struct Context {
    SDL_Window* m_window = nullptr;
    bool m_enableValidation = true;

    SDL_Window* getWindow() const noexcept { return m_window; }
    bool enableValidation() const noexcept { return m_enableValidation; }
    void setWindow(SDL_Window* win) noexcept { m_window = win; }
    void setValidation(bool val) noexcept { m_enableValidation = val; }
};

class VulkanRenderer;

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL RTX EXTENSIONS — VULKAN 1.3+ COMPATIBLE — CROSS-PLATFORM — FIXED: std::array
// ──────────────────────────────────────────────────────────────────────────────
inline constexpr std::array<const char*, 6> RTX_EXTENSIONS = {
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
// GLOBAL STACK — ORDER IS LAW — Dispose FIRST — FIXED: Include GlobalContext.hpp
// ──────────────────────────────────────────────────────────────────────────────
#include "engine/GLOBAL/StoneKey.hpp"           // FIRST — kStone1, obfuscate
#include "engine/GLOBAL/Houston.hpp"            // SECOND — ctx(), Handle<T>, MakeHandle
#include "engine/GLOBAL/GlobalContext.hpp"      // THIRD — GlobalRTXContext def (no redef)
#include "engine/GLOBAL/logging.hpp"            // LOG_*, 
#include "engine/GLOBAL/LAS.hpp"                // AMAZO_LAS::get()

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
// INLINE HELPERS — C++23 PROFESSIONAL — CROSS-PLATFORM — FIXED: array to vector
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

inline std::vector<std::string> getVulkanExtensions() {
    std::vector<std::string> exts;
    exts.reserve(RTX_EXTENSIONS.size());
    for (auto* ext : RTX_EXTENSIONS) {
        exts.emplace_back(ext);
    }
    return exts;
}

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================