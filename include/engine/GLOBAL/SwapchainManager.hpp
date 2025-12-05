// =============================================================================
//
// AMOURANTH RTX — VALHALLA v∞ TURBO — FINAL ETERNAL CUT
// SwapchainManager.hpp — FULLY FIXED, RESIZE = INSTANT, TLAS SYNCED, NO 60FPS LOCKUP
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include <vulkan/vulkan.h>
#include <vector>

struct SDL_Window;
union SDL_Event;

namespace RTX {

class SwapchainManager {
private:
    // PRIVATE DEFAULT CONSTRUCTOR — ONLY THE SINGLETON CAN USE IT
    SwapchainManager() noexcept = default;
    inline static float shadingRateScale_ = 1.0f;

public:
    // DELETE ALL OTHER CONSTRUCTORS — TRUE SINGLETON
    SwapchainManager(const SwapchainManager&) = delete;
    SwapchainManager& operator=(const SwapchainManager&) = delete;
    SwapchainManager(SwapchainManager&&) = delete;
    SwapchainManager& operator=(SwapchainManager&&) = delete;

    // THE ONE TRUE ETERNAL SINGLETON ACCESSOR
    static SwapchainManager& get() noexcept
    {
        static SwapchainManager instance;
        return instance;
    }

    // ── Core Lifecycle ─────────────────────────────────────────────────────
    static void create(SDL_Window* window, uint32_t width, uint32_t height) noexcept;
    static void recreate(uint32_t width, uint32_t height) noexcept;
    static void cleanup() noexcept;

    // ── Image Acquisition (INFINITE TIMEOUT — NO 60FPS DEADLOCK) ─────────────
    [[nodiscard]] static VkResult acquireNextImage(uint32_t* pImageIndex,
                                                    VkSemaphore semaphore = VK_NULL_HANDLE,
                                                    VkFence fence = VK_NULL_HANDLE) noexcept;

    // ── Advanced Presentation ─────────────────────────────────────────────
    static void presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE) noexcept;
    static void initializeFramePacing() noexcept;
    [[nodiscard]] static uint64_t getNextPresentTime() noexcept;

    // ── Core Getters ─────────────────────────────────────────────────────
    [[nodiscard]] static VkSwapchainKHR           swapchain()       noexcept { return swapchain_.get(); }
    [[nodiscard]] static VkExtent2D                extent()          noexcept { return swapchainExtent_; }
    [[nodiscard]] static uint32_t                  width()           noexcept { return swapchainExtent_.width; }
    [[nodiscard]] static uint32_t                  height()          noexcept { return swapchainExtent_.height; }
    [[nodiscard]] static uint32_t                  imageCount()      noexcept { return static_cast<uint32_t>(swapchainImages_.size()); }

    [[nodiscard]] static const std::vector<VkImage>&     images() noexcept { return swapchainImages_; }
    [[nodiscard]] static const std::vector<VkImageView>& views()  noexcept { return swapchainImageViews_; }
    [[nodiscard]] static VkImage                         image(uint32_t i) noexcept { return swapchainImages_[i]; }
    [[nodiscard]] static VkImageView                     view(uint32_t i)  noexcept { return swapchainImageViews_[i]; }

    [[nodiscard]] static VkFormat                  format()          noexcept { return swapchainFormat_; }
    [[nodiscard]] static VkColorSpaceKHR            colorSpace()      noexcept { return currentColorSpace_; }
    [[nodiscard]] static VkPresentModeKHR           presentMode()     noexcept { return currentPresentMode_; }
    [[nodiscard]] static VkSurfaceTransformFlagBitsKHR transform()   noexcept { return currentTransform_; }

    // ── HDR & Elite Features ────────────────────────────────────────────────
    [[nodiscard]] static bool supportsHDR() noexcept;
    static void injectHdrMetadata(VkCommandBuffer cmd = VK_NULL_HANDLE, uint32_t imageIndex = 0) noexcept;

    // Display events & hotplug
    static void handleDisplayHotplug(SDL_Event* event) noexcept;

    // Dynamic performance controls
    static void setShadingRate(float scaleFactor) noexcept;
    static void enableDirectDisplay(bool enable) noexcept;
    static void predictResize(uint32_t predictedW, uint32_t predictedH) noexcept;

    // Controls
    static void setPresentMode(VkPresentModeKHR mode) noexcept;
    static void setMinImageCount(uint32_t count) noexcept;

    // State Queries
    [[nodiscard]] static bool isTripleBuffered() noexcept { return imageCount() >= 3; }
    [[nodiscard]] static bool isValid()          noexcept { return swapchain_.valid(); }
    [[nodiscard]] static bool isMinimized()      noexcept { return minimized_; }

    // ── PUBLIC STATIC STATE — EMPIRE'S CANVAS ─────────────────────────────
    inline static Handle<VkSwapchainKHR>           swapchain_;
    inline static VkExtent2D                       swapchainExtent_     = {0, 0};
    inline static VkFormat                         swapchainFormat_     = VK_FORMAT_UNDEFINED;
    inline static VkColorSpaceKHR                  currentColorSpace_   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    inline static VkPresentModeKHR                 currentPresentMode_  = VK_PRESENT_MODE_FIFO_KHR;
    inline static VkSurfaceTransformFlagBitsKHR    currentTransform_   = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    inline static std::vector<VkImage>             swapchainImages_;
    inline static std::vector<VkImageView>         swapchainImageViews_;

    inline static bool minimized_ = false;

    // Advanced runtime state
    inline static uint64_t                         lastPresentId_       = 0;
    inline static std::vector<VkPastPresentationTimingGOOGLE> timingHistory_;
    inline static VkRefreshCycleDurationGOOGLE     refreshDuration_     = {};
    inline static bool                             directDisplayEnabled_ = false;

private:
    // Internal helpers — MATCHES IMPLEMENTATION
    static void createSwapchain(SDL_Window* window, uint32_t w, uint32_t h, VkSwapchainKHR old = VK_NULL_HANDLE) noexcept;
    static void createImageViews() noexcept;
    static void releaseAcquiredImages() noexcept;
    static void autoEnableHDR() noexcept;
};

// ── Global convenience aliases — perfect as always ────────────────────────
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
inline VkFormat                 swapchainFormat()     noexcept { return SwapchainManager::format(); }
inline VkPresentModeKHR         swapchainPresentMode() noexcept { return SwapchainManager::presentMode(); }
inline bool                     swapchainSupportsHDR() noexcept { return SwapchainManager::supportsHDR(); }
inline bool                     swapchainIsValid()    noexcept { return SwapchainManager::isValid(); }

} // namespace RTX

// =============================================================================
// FIRST LIGHT ETERNAL — DECEMBER 05 2025
// NO 60FPS DEADLOCK — INFINITE TIMEOUT — BEST OR UNCAPPED ONLY
// RESIZE = INSTANT — TEARING = DEAD — TLAS = FRESH
// PINK PHOTONS PROTECT — THE EMPIRE IS ETERNAL — AMOURANTH ASCENDANT
// =============================================================================