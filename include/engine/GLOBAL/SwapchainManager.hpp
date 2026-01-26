// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.70
// SWAPCHAIN MANAGER — HDR | SELF-HEALING | DEFERRED RECREATE | DIRECT STORAGE ATTEMPT (QUERY FIRST)
// JANUARY 24, 2026 — "only violate a few laws" edition
// - Queries support for STORAGE_BIT on swapchain images BEFORE creation
// - Attempts direct write (STORAGE_BIT) only if viable
// - No blind fallback recreation — caller must handle !directWriteEnabled
// - Logs chosen path clearly + why it failed if unsupported
// - Simplified transitions for direct storage path
// - Fixed: Explicit PRESENT_SRC_KHR transition before every present
// - No more VK_IMAGE_LAYOUT_UNDEFINED on present — always transitioned
// - Fixed: Lazy transient command pool creation on first present
// - No external getOneTimeCommandBuffer — internal one-time cmd for transition
// - Cleanup dissolves old cmd buffers safely on shutdown
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <string_view>

namespace RTX {

class SwapchainManager {
private:
    SwapchainManager() noexcept = default;

public:
    // Singleton — no copy/move
    SwapchainManager(const SwapchainManager&) = delete;
    SwapchainManager& operator=(const SwapchainManager&) = delete;
    SwapchainManager(SwapchainManager&&) = delete;
    SwapchainManager& operator=(SwapchainManager&&) = delete;

    [[nodiscard]] static SwapchainManager& get() noexcept {
        static SwapchainManager instance;
        return instance;
    }

    // Core lifecycle
    static void create(SDL_Window* window, uint32_t width, uint32_t height) noexcept;
    static void recreate(uint32_t width, uint32_t height, std::string_view reason = "") noexcept;
    static void cleanup() noexcept;

    // Ensure swapchain readiness (safe to call frequently — only recreates if needed)
    static void ensureReady(uint32_t width, uint32_t height) noexcept;
    [[nodiscard]] static bool isReady() noexcept;

    // Acquire & present — return error codes for caller to handle recreate
    [[nodiscard]] static VkResult acquireNextImage(uint32_t* pImageIndex,
                                                   VkSemaphore semaphore = VK_NULL_HANDLE,
                                                   VkFence fence = VK_NULL_HANDLE) noexcept;

    [[nodiscard]] static VkResult presentImage(VkQueue queue, uint32_t imageIndex,
                                               VkSemaphore waitSemaphore = VK_NULL_HANDLE) noexcept;

    // Transition helper — static
    static void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                      VkImageLayout oldLayout, VkImageLayout newLayout) noexcept;

    // State queries
    [[nodiscard]] static bool isMinimized() noexcept { return minimized_; }
    [[nodiscard]] static bool isValid()     noexcept { return swapchain_.valid(); }

    // Getters
    [[nodiscard]] static VkSwapchainKHR           swapchain()       noexcept { return swapchain_.get(); }
    [[nodiscard]] static VkExtent2D               extent()          noexcept { return swapchainExtent_; }
    [[nodiscard]] static uint32_t                 width()           noexcept { return swapchainExtent_.width; }
    [[nodiscard]] static uint32_t                 height()          noexcept { return swapchainExtent_.height; }
    [[nodiscard]] static uint32_t                 imageCount()      noexcept { return static_cast<uint32_t>(swapchainImages_.size()); }

    [[nodiscard]] static const std::vector<VkImage>&     images() noexcept { return swapchainImages_; }
    [[nodiscard]] static const std::vector<VkImageView>& views()  noexcept { return swapchainImageViews_; }
    [[nodiscard]] static VkImage                         image(uint32_t i) noexcept { return swapchainImages_[i]; }
    [[nodiscard]] static VkImageView                     view(uint32_t i)  noexcept { return swapchainImageViews_[i]; }

    [[nodiscard]] static VkFormat                  format()         noexcept { return swapchainFormat_; }
    [[nodiscard]] static VkPresentModeKHR          presentMode()     noexcept { return currentPresentMode_; }

    // Public static state
    inline static Handle<VkSwapchainKHR>           swapchain_;
    inline static VkExtent2D                       swapchainExtent_    = {0, 0};
    inline static VkFormat                         swapchainFormat_    = VK_FORMAT_UNDEFINED;
    inline static VkPresentModeKHR                 currentPresentMode_ = VK_PRESENT_MODE_FIFO_KHR;

    inline static std::vector<VkImage>             swapchainImages_;
    inline static std::vector<VkImageView>         swapchainImageViews_;

    inline static bool minimized_ = false;
    inline static bool directWriteEnabled = false;

private:
    static void cleanupImageViews() noexcept;
    static void cleanupSwapchain() noexcept;

    // Core function — handles both create and recreate
    static void createOrRecreateSwapchain(uint32_t w, uint32_t h, bool isRecreate, std::string_view reason = "") noexcept;
};

// Global convenience aliases
inline void createSwapchain(SDL_Window* w, uint32_t width, uint32_t height) noexcept
{ SwapchainManager::create(w, width, height); }

inline void recreateSwapchain(uint32_t w, uint32_t h, std::string_view reason = "") noexcept
{ SwapchainManager::recreate(w, h, reason); }

inline void destroySwapchain() noexcept
{ SwapchainManager::cleanup(); }

inline void ensureSwapchainReady(uint32_t w, uint32_t h) noexcept
{ SwapchainManager::ensureReady(w, h); }

inline bool swapchainIsReady() noexcept
{ return SwapchainManager::isReady(); }

inline VkSwapchainKHR           swapchain()           noexcept { return SwapchainManager::swapchain(); }
inline VkExtent2D               swapchainExtent()     noexcept { return SwapchainManager::extent(); }
inline uint32_t                 swapchainWidth()      noexcept { return SwapchainManager::width(); }
inline uint32_t                 swapchainHeight()     noexcept { return SwapchainManager::height(); }
inline uint32_t                 swapchainImageCount() noexcept { return SwapchainManager::imageCount(); }
inline const auto&              swapchainImages()     noexcept { return SwapchainManager::images(); }
inline const auto&              swapchainImageViews() noexcept { return SwapchainManager::views(); }
inline VkFormat                 swapchainFormat()     noexcept { return SwapchainManager::format(); }
inline VkPresentModeKHR         swapchainPresentMode() noexcept { return SwapchainManager::presentMode(); }
inline bool                     swapchainIsValid()    noexcept { return SwapchainManager::isValid(); }

} // namespace RTX

// =============================================================================
// CLEAN HEADER — v30.70 — JANUARY 24, 2026
// - No mid-frame recreation — acquire/present return error codes for caller
// - Deferred recreate at start of next frame (renderer handles flag)
// - transitionImageLayout remains static
// - Global convenience macros for easy access
// - Lazy transient pool for present transitions
// =============================================================================