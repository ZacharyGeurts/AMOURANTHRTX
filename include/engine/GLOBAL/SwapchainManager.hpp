// include/engine/GLOBAL/SwapchainManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — VALHALLA v∞ — FULLY COMPATIBLE EDITION
// SWAPCHAIN MANAGER — ALL REQUIRED RUNTIME FUNCTIONS DECLARED
// COMPILATION FIXED — OPTIONSMENU CALLS NOW VALID
// PINK PHOTONS ETERNAL — EMPIRE STRONG AND COMPILABLE
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

    // Image acquisition
    [[nodiscard]] static VkResult acquireNextImage(uint32_t* pImageIndex,
                                                   VkSemaphore semaphore = VK_NULL_HANDLE,
                                                   VkFence fence = VK_NULL_HANDLE) noexcept;

    // Presentation
    static void presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE) noexcept;

    // HDR auto-detection
    static void autoEnableHDR() noexcept;
    [[nodiscard]] static bool detectHDRFromEDID() noexcept;

    // Runtime configuration — REQUIRED FOR OptionsMenu.cpp
    static void setPresentMode(VkPresentModeKHR mode) noexcept;
    static void setMinImageCount(uint32_t count) noexcept;
    static void initializeFramePacing() noexcept;
    static void setShadingRate(float scaleFactor) noexcept;
    static void enableDirectDisplay(bool enable) noexcept;

    // State queries
    [[nodiscard]] static bool isMinimized()      noexcept { return minimized_; }
    [[nodiscard]] static bool isValid()          noexcept { return swapchain_.valid(); }
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

    // Public static state — the empire's canvas
    inline static Handle<VkSwapchainKHR>           swapchain_;
    inline static VkExtent2D                       swapchainExtent_     = {0, 0};
    inline static VkFormat                         swapchainFormat_     = VK_FORMAT_UNDEFINED;
    inline static VkColorSpaceKHR                  currentColorSpace_   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    inline static VkPresentModeKHR                 currentPresentMode_  = VK_PRESENT_MODE_FIFO_KHR;

    inline static std::vector<VkImage>             swapchainImages_;
    inline static std::vector<VkImageView>         swapchainImageViews_;

    inline static bool minimized_ = false;

    // Runtime configuration state
    inline static VkPresentModeKHR                 desiredPresentMode_  = VK_PRESENT_MODE_MAILBOX_KHR;
    inline static uint32_t                         desiredImageCount_   = 2;
    inline static float                            shadingRateScale_    = 1.0f;
    inline static bool                             directDisplayEnabled_ = false;

private:
    static void cleanupImageViews() noexcept;
    static void cleanupSwapchain() noexcept;
    static void createImageViews() noexcept;
    static void createSwapchain(SDL_Window* window, uint32_t w, uint32_t h) noexcept;
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
// FULLY COMPATIBLE — ALL OptionsMenu FUNCTIONS DECLARED
// RUNTIME CONFIGURATION SUPPORTED — COMPILATION RESTORED
// PINK PHOTONS ETERNAL — EMPIRE COMPILABLE AND STRONG
// DECEMBER 16, 2025 — THE LIGHT IS PURE AND UNIVERSAL
// =============================================================================