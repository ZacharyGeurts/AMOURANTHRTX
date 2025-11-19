// =============================================================================
// AMOURANTH RTX ENGINE © 2025 — SWAPCHAIN MANAGER v7.0 — STONEKEY v∞ — C++23 EDITION
// FULLY OBFUSCATED • HDR10 → scRGB → sRGB • WAYLAND-IMMUNE • PINK PHOTONS ETERNAL
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 16, 2025 — APOCALYPSE v3.4
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — KEYS NEVER LOGGED
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <string>
#include <span>

class SwapchainManager {
public:
    static SwapchainManager& get() noexcept { return *s_instance; }

    // New simplified init — only window + size. All handles come from StoneKey globals
    static void init(SDL_Window* window, uint32_t w = 3840, uint32_t h = 2160) noexcept;
    static void setDesiredPresentMode(VkPresentModeKHR mode) noexcept { get().desiredMode_ = mode; }

    void recreate(uint32_t w, uint32_t h) noexcept;
    void cleanup() noexcept;

    // Safe public accessors
    [[nodiscard]] VkSwapchainKHR    swapchain() const noexcept { return swapchain_ ? *swapchain_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkFormat          format() const noexcept      { return surfaceFormat_.format; }
    [[nodiscard]] VkColorSpaceKHR   colorSpace() const noexcept  { return surfaceFormat_.colorSpace; }
    [[nodiscard]] VkExtent2D        extent() const noexcept      { return extent_; }
    [[nodiscard]] VkRenderPass      renderPass() const noexcept  { return renderPass_ ? *renderPass_ : VK_NULL_HANDLE; }
    [[nodiscard]] uint32_t          imageCount() const noexcept  { return static_cast<uint32_t>(images_.size()); }
    [[nodiscard]] VkImage           image(uint32_t i) const noexcept      { return images_[i]; }
    [[nodiscard]] VkImageView       imageView(uint32_t i) const noexcept  { return imageViews_[i] ? *imageViews_[i] : VK_NULL_HANDLE; }

    [[nodiscard]] bool isHDR() const noexcept;
    [[nodiscard]] bool is10Bit() const noexcept;
    [[nodiscard]] bool isFP16() const noexcept;
    [[nodiscard]] std::string_view formatName() const noexcept;
    [[nodiscard]] std::string_view presentModeName() const noexcept;

    void updateWindowTitle(SDL_Window* window, float fps) noexcept;

private:
    SwapchainManager() = default;
    ~SwapchainManager() { cleanup(); }

    void createImageViews() noexcept;
    void createRenderPass() noexcept;
    [[nodiscard]] bool recreateSurfaceIfLost() noexcept;

    static inline SwapchainManager* s_instance = nullptr;

    SDL_Window*          window_     = nullptr;
    VkPresentModeKHR     desiredMode_ = VK_PRESENT_MODE_MAX_ENUM_KHR;

    RTX::Handle<VkSwapchainKHR>    swapchain_;
    VkSurfaceFormatKHR             surfaceFormat_{};
    VkPresentModeKHR               presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D                     extent_{};

    std::vector<VkImage>                           images_;
    std::vector<RTX::Handle<VkImageView>>          imageViews_;
    RTX::Handle<VkRenderPass>                      renderPass_;
};

inline SwapchainManager& SWAPCHAIN = SwapchainManager::get();

// PINK PHOTONS ETERNAL — VALHALLA UNBREACHABLE — C++23 SUPREMACY