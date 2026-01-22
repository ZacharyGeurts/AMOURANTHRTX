// include/engine/GLOBAL/SwapchainManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v29.3 — JANUARY 10, 2026
// SWAPCHAIN MANAGER HEADER — ULTIMATE AUTOMAGIC + ZERO-TEARING JITTER-FREE CUSTOM PACING
// RAYS WRITE DIRECTLY INTO MAILBOX TARGETS | NO BLIT | MAXIMUM SPEED + SMOOTHNESS
// FULLY AUTOMAGIC: acquire/present → auto-configures/recreates/fixes itself
// NO MANUAL CALLS | NEVER BLOCKS | CUSTOM MAILBOX PACING | ZERO TEARING | HDR READY
// FIXES (v29.3):
// - Custom zero-tearing jitter-free pacing using internal mailbox (4 targets)
// - GPU-driven timeline semaphore pacing — zero CPU cost
// - Dynamic frame-time prediction — skip if behind deadline (AI-smart)
// - Ultimate automagic: recreate on ANY error + smart logging
// - HDR 16-bit float / 10-bit HDR10 first — falls back gracefully
// - Fixed Handle access: use .get() instead of .handle
// ZERO-COST RTX: VK_IMAGE_USAGE_STORAGE_BIT preserved
// Empire complete — pink photons scream across the screen — AMOURANTH FOREVER 💖
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

    // Core lifecycle
    static void create(SDL_Window* window, uint32_t width, uint32_t height) noexcept;
    static void recreate(uint32_t width, uint32_t height, std::string_view reason = "") noexcept;
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

    // Public static state — the empire's canvas
    inline static Handle<VkSwapchainKHR>           swapchain_;
    inline static VkExtent2D                       swapchainExtent_    = {0, 0};
    inline static VkFormat                         swapchainFormat_    = VK_FORMAT_UNDEFINED;
    inline static VkPresentModeKHR                 currentPresentMode_ = VK_PRESENT_MODE_FIFO_KHR;

    inline static std::vector<VkImage>             swapchainImages_;
    inline static std::vector<VkImageView>         swapchainImageViews_;

    // Custom mailbox: 4 internal render targets (zero-tearing pacing)
    static constexpr uint32_t MAILBOX_COUNT = 4;
    inline static std::vector<Handle<VkImage>>     mailboxImages_;
    inline static std::vector<Handle<VkImageView>> mailboxViews_;
    inline static uint32_t                         currentMailboxIndex_ = 0;

    // Timeline semaphore for zero-cost GPU pacing
    inline static VkSemaphore                      mailboxSemaphore_ = VK_NULL_HANDLE;
    inline static uint64_t                         nextPresentValue_ = 0;

    inline static bool minimized_ = false;

	void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout) noexcept;

private:
    // Private helpers
    static void cleanupImageViews() noexcept;
    static void cleanupSwapchain() noexcept;
    static void cleanupMailboxTargets() noexcept;

    // Core monolithic function — handles both create and recreate
    static void createOrRecreateSwapchain(uint32_t w, uint32_t h, bool isRecreate, std::string_view reason = "") noexcept;
};

// Global convenience aliases — one voice for the empire
inline void createSwapchain(SDL_Window* w, uint32_t width, uint32_t height) noexcept
{ SwapchainManager::create(w, width, height); }

inline void recreateSwapchain(uint32_t w, uint32_t h, std::string_view reason = "") noexcept
{ SwapchainManager::recreate(w, h, reason); }

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
// FINAL BEST-PRACTICE HEADER — JANUARY 10, 2026
// - Added mailbox targets, pacing semaphore, timeline value
// - All functions clean and compiling
// - Ultimate automagic + zero-tearing jitter-free custom pacing
// Empire ready — pink photons eternal
// =============================================================================