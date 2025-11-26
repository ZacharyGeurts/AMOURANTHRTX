// =============================================================================
// include/engine/GLOBAL/SwapchainManager.hpp
// AMOURANTH RTX Engine © 2025 — DREAM SWAPCHAIN EDITION — FINAL CUT
// "No gripes. No flicker. No mercy. HDR where it counts."
// First light eternal — November 25, 2025
// The empire’s swapchain. Short. Lethal. Perfect.
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include <vulkan/vulkan.h>
#include <vector>

namespace RTX {

class SwapchainManager {
public:
    // ────────────────────── Lifecycle ──────────────────────
    static void create(SDL_Window* window, uint32_t width, uint32_t height) noexcept;
    static void recreate(uint32_t width, uint32_t height) noexcept;
    static void cleanup() noexcept;

    // ────────────────────── Core Getters ──────────────────────
    [[nodiscard]] static VkSwapchainKHR           swapchain()       noexcept { return swapchain_.get(); }
	inline static VkFormat                         swapchainFormat_     = VK_FORMAT_UNDEFINED;
	inline static VkColorSpaceKHR                  currentColorSpace_   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	[[nodiscard]] static VkExtent2D                extent()          noexcept { return swapchainExtent_; }
    [[nodiscard]] static uint32_t                  width()           noexcept { return swapchainExtent_.width; }
    [[nodiscard]] static uint32_t                  height()          noexcept { return swapchainExtent_.height; }
    [[nodiscard]] static uint32_t                  imageCount()      noexcept { return static_cast<uint32_t>(swapchainImages_.size()); }

    [[nodiscard]] static const std::vector<VkImage>&           images() noexcept { return swapchainImages_; }
    [[nodiscard]] static const std::vector<Handle<VkImageView>>& views() noexcept { return swapchainImageViews_; }
    [[nodiscard]] static VkImage                               image(uint32_t i) noexcept { return swapchainImages_[i]; }
    [[nodiscard]] static const Handle<VkImageView>&            view(uint32_t i)  noexcept { return swapchainImageViews_[i]; }

    [[nodiscard]] static VkFormat                  format()          noexcept { return swapchainFormat_; }

    // ────────────────────── THE ONE TRUE FORMAT SELECTOR — PURE LAW ──────────────────────
    [[nodiscard]] static VkFormat swapchainFormat() noexcept;

    // ────────────────────── HDR & Advanced ──────────────────────
    [[nodiscard]] static bool              supportsHDR() noexcept;
    [[nodiscard]] static VkColorSpaceKHR    colorSpace()  noexcept { return currentColorSpace_; }
    static void                             enableHDR(bool enable) noexcept;
    static void                             injectHdrMetadata(VkCommandBuffer cmd, uint32_t imageIndex) noexcept;

    // ────────────────────── Present & Buffering ──────────────────────
    [[nodiscard]] static VkPresentModeKHR           presentMode() noexcept { return currentPresentMode_; }
    [[nodiscard]] static VkSurfaceTransformFlagBitsKHR transform() noexcept { return currentTransform_; }
    [[nodiscard]] static bool                      isTripleBuffered() noexcept { return imageCount() >= 3; }
    [[nodiscard]] static bool                      isValid() noexcept { return swapchain_.valid(); }

    // ────────────────────── Controls ──────────────────────
    static void setPresentMode(VkPresentModeKHR mode) noexcept;
    static void setMinImageCount(uint32_t count) noexcept;

private:
    SwapchainManager() = delete;  // Static-only

    // ────────────────────── State (RAII-protected) ──────────────────────
    inline static Handle<VkSwapchainKHR>           swapchain_;    
    inline static VkExtent2D                       swapchainExtent_     = {0, 0};
    inline static VkPresentModeKHR                 currentPresentMode_  = VK_PRESENT_MODE_FIFO_KHR;
    inline static VkSurfaceTransformFlagBitsKHR    currentTransform_   = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    inline static std::vector<VkImage>             swapchainImages_;
    inline static std::vector<Handle<VkImageView>> swapchainImageViews_;

    // ────────────────────── Internal helpers ──────────────────────
    static void createSwapchain(SDL_Window* window,
                                uint32_t width,
                                uint32_t height,
                                VkSwapchainKHR old = VK_NULL_HANDLE) noexcept;
    static void createImageViews() noexcept;
    static void releaseAcquiredImages() noexcept;
};

// ────────────────────── Global convenience aliases ──────────────────────

    inline void createSwapchain(SDL_Window* w, uint32_t width, uint32_t height) noexcept
    { SwapchainManager::create(w, width, height); }

    inline void recreateSwapchain(uint32_t w, uint32_t h) noexcept
    { SwapchainManager::recreate(w, h); }

    inline void destroySwapchain() noexcept
    { SwapchainManager::cleanup(); }

    inline VkSwapchainKHR           swapchain()           noexcept { return SwapchainManager::swapchain(); }
    inline VkExtent2D               swapchainExtent()     noexcept { return SwapchainManager::extent(); }
    inline uint32_t                 swapchainWidth()      noexcept { return SwapchainManager::width(); }
    inline uint32_t                 swapchainHeight()     noexcept { return SwapchainManager::height(); }
    inline uint32_t                 swapchainImageCount() noexcept { return SwapchainManager::imageCount(); }
    inline const auto&              swapchainImages()     noexcept { return SwapchainManager::images(); }
    inline const auto&              swapchainImageViews() noexcept { return SwapchainManager::views(); }
    inline VkPresentModeKHR         swapchainPresentMode() noexcept { return SwapchainManager::presentMode(); }
    inline bool                     swapchainSupportsHDR() noexcept { return SwapchainManager::supportsHDR(); }
    inline bool                     swapchainIsValid()    noexcept { return SwapchainManager::isValid(); }

} // namespace RTX

// =============================================================================
// Cast & Crew — etched forever
// Amouranth — The Vision
// Nick      — The Iron
// Blondie   — The Silence
// Ballerina — The Judgment
// Grok      — The Truth
//
// 110 lines. Zero bloat. Infinite power.
// First light achieved — November 25, 2025
// PINK PHOTONS ETERNAL
// =============================================================================