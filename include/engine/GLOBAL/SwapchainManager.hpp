// include/engine/GLOBAL/SwapchainManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — VALHALLA v∞ TURBO — FINAL ETERNAL CUT
// SwapchainManager — MAILBOX + 2 FRAMES — HDR AUTO — INSTANT RESIZE — NO LOCKUP
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
    SwapchainManager() noexcept = default;

public:
    // Delete copy/move — true singleton
    SwapchainManager(const SwapchainManager&) = delete;
    SwapchainManager& operator=(const SwapchainManager&) = delete;
    SwapchainManager(SwapchainManager&&) = delete;
    SwapchainManager& operator=(SwapchainManager&&) = delete;

    // Singleton access
    [[nodiscard]] static SwapchainManager& get() noexcept {
        static SwapchainManager instance;
        return instance;
    }

    // Core lifecycle
    static void create(SDL_Window* window, uint32_t width, uint32_t height) noexcept;
    static void recreate(uint32_t width, uint32_t height) noexcept;
    static void cleanup() noexcept;
    static void cleanupImageViews() noexcept;

    // Image acquisition — infinite timeout, no deadlock
    [[nodiscard]] static VkResult acquireNextImage(uint32_t* pImageIndex,
                                                   VkSemaphore semaphore = VK_NULL_HANDLE,
                                                   VkFence fence = VK_NULL_HANDLE) noexcept;

    // Presentation
    static void presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE) noexcept;

    // HDR & elite features
    static void autoEnableHDR() noexcept;
    [[nodiscard]] static bool detectHDRFromEDID() noexcept;
    static void injectHdrMetadata(VkCommandBuffer cmd = VK_NULL_HANDLE, uint32_t imageIndex = 0) noexcept;

    // Display events
    static void handleDisplayHotplug(SDL_Event* event) noexcept;

    // Dynamic controls
    static void setShadingRate(float scaleFactor) noexcept { shadingRateScale_ = scaleFactor; }
    static void enableDirectDisplay(bool enable) noexcept { directDisplayEnabled_ = enable; }
    static void predictResize(uint32_t predictedW, uint32_t predictedH) noexcept;

    // Desired overrides
    static void setPresentMode(VkPresentModeKHR mode) noexcept { desiredPresentMode_ = mode; }
    static void setMinImageCount(uint32_t count) noexcept { desiredImageCount_ = count; }

    // State queries
    [[nodiscard]] static bool isTripleBuffered() noexcept { return imageCount() >= 3; }
    [[nodiscard]] static bool isValid()          noexcept { return swapchain_.valid(); }
    [[nodiscard]] static bool isMinimized()      noexcept { return minimized_; }
    [[nodiscard]] static bool supportsHDR()      noexcept { return currentColorSpace_ == VK_COLOR_SPACE_HDR10_ST2084_EXT; }

    // Core getters
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

    // Public static state — the empire's canvas
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
    inline static float                            shadingRateScale_    = 1.0f;

    // Desired overrides
    inline static VkPresentModeKHR                 desiredPresentMode_  = VK_PRESENT_MODE_MAILBOX_KHR;
    inline static uint32_t                         desiredImageCount_   = 2;
    static void initializeFramePacing() noexcept;

private:
    static void createSwapchain(SDL_Window* window, uint32_t w, uint32_t h, VkSwapchainKHR old = VK_NULL_HANDLE) noexcept;
    static void createImageViews() noexcept;
    static void releaseAcquiredImages() noexcept;
    [[nodiscard]] static uint64_t getNextPresentTime() noexcept;
};

// Global convenience aliases — the empire speaks with one voice
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
// FIRST LIGHT ETERNAL — DECEMBER 16 2025 — 2026 HARDCODE MASTERMIND
// MAILBOX + 2 FRAMES ENFORCED — HDR AUTO VIA EDID — NO DEADLOCK — INSTANT RESIZE
// PINK PHOTONS PROTECT — THE EMPIRE IS ETERNAL — AMOURANTH ASCENDANT
// =============================================================================