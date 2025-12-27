// include/engine/GLOBAL/SwapchainManager.hpp
// =============================================================================
// PLASTIC BEACH v∞ — DECEMBER 19, 2025
// SIMPLIFIED SWAPCHAIN MANAGER HEADER
// ONE LARGE FUNCTION STYLE — MONOLITHIC PERFECTION
// MAX FPS + MINIMAL TEARING — FIFO_RELAXED → IMMEDIATE → FIFO
// 3-IMAGE MAILBOX EMULATION — NO BLACK SCREENS
// MONSTER WATCHES IN PERFECT SILENCE
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include <vulkan/vulkan.h>
#include <vector>

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

	void renderDirectEnvMap(VkCommandBuffer cmd, uint32_t swapImageIndex) noexcept;

    // Core lifecycle — simplified to match new monolithic implementation
    static void create(SDL_Window* window, uint32_t width, uint32_t height) noexcept;
    static void recreate(uint32_t width, uint32_t height) noexcept;
    static void cleanup() noexcept;

    // Image acquisition & presentation
    [[nodiscard]] static VkResult acquireNextImage(uint32_t* pImageIndex,
                                                   VkSemaphore semaphore = VK_NULL_HANDLE,
                                                   VkFence fence = VK_NULL_HANDLE) noexcept;

    static void presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE) noexcept;

    // State queries
    [[nodiscard]] static bool isMinimized() noexcept { return minimized_; }
    [[nodiscard]] static bool isValid()     noexcept { return swapchain_.valid(); }

    // Core getters
    [[nodiscard]] static VkSwapchainKHR           swapchain()       noexcept { return swapchain_.get(); }
    [[nodiscard]] static VkExtent2D                extent()         noexcept { return swapchainExtent_; }
    [[nodiscard]] static uint32_t                  width()          noexcept { return swapchainExtent_.width; }
    [[nodiscard]] static uint32_t                  height()         noexcept { return swapchainExtent_.height; }
    [[nodiscard]] static uint32_t                  imageCount()     noexcept { return static_cast<uint32_t>(swapchainImages_.size()); }

    [[nodiscard]] static const std::vector<VkImage>&     images() noexcept { return swapchainImages_; }
    [[nodiscard]] static const std::vector<VkImageView>& views()  noexcept { return swapchainImageViews_; }
    [[nodiscard]] static VkImage                         image(uint32_t i) noexcept { return swapchainImages_[i]; }
    [[nodiscard]] static VkImageView                     view(uint32_t i)  noexcept { return swapchainImageViews_[i]; }

    [[nodiscard]] static VkFormat                  format()         noexcept { return swapchainFormat_; }
    [[nodiscard]] static VkPresentModeKHR           presentMode()    noexcept { return currentPresentMode_; }

    // Public static state — the island's canvas
    inline static Handle<VkSwapchainKHR>           swapchain_;
    inline static VkExtent2D                       swapchainExtent_    = {0, 0};
    inline static VkFormat                         swapchainFormat_    = VK_FORMAT_UNDEFINED;
    inline static VkPresentModeKHR                 currentPresentMode_ = VK_PRESENT_MODE_FIFO_KHR;

    inline static std::vector<VkImage>             swapchainImages_;
    inline static std::vector<VkImageView>         swapchainImageViews_;

    inline static bool minimized_ = false;

private:
    // Private helpers — kept minimal to match simplified implementation
    static void cleanupImageViews() noexcept;
    static void cleanupSwapchain() noexcept;

    // Core monolithic function — handles both create and recreate
    static void createOrRecreateSwapchain(uint32_t w, uint32_t h, bool isRecreate) noexcept;
};

// Global convenience aliases — one voice for the empire
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
inline bool                     swapchainIsValid()    noexcept { return SwapchainManager::isValid(); }

} // namespace RTX

// =============================================================================
// PLASTIC BEACH v∞ — DECEMBER 19, 2025
// HEADER UPDATED TO MATCH SIMPLIFIED MONOLITHIC CPP
// ALL UNNEEDED FUNCTIONS REMOVED — CLEAN AND MINIMAL
// NO MODULARITY — ONE PIECE — PERFECTION
// THE MONSTER APPROVES — SILENCE IS ETERNAL
// =============================================================================