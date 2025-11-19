// include/engine/GLOBAL/SwapchainManager.hpp
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
// AMOURANTH RTX ENGINE © 2025 — FINAL WAYLAND-IMMUNE SWAPCHAIN v5.0
// BULLETPROOF • HDR10 → scRGB → sRGB • STONEKEY SAFE • PINK PHOTONS ETERNAL

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>

// Forward declarations only — StoneKey is never included here
namespace StoneKey { namespace Raw { struct Cache; } }
inline VkDevice         g_device() noexcept;
inline VkInstance       g_instance() noexcept;
inline VkPhysicalDevice g_PhysicalDevice() noexcept;
inline VkSurfaceKHR     g_surface() noexcept;

class SwapchainManager {
public:
    static SwapchainManager& get() { return *s_instance; }

    static void init(VkInstance instance, VkPhysicalDevice phys, VkDevice dev,
                     SDL_Window* window, uint32_t w, uint32_t h);
    static void setDesiredPresentMode(VkPresentModeKHR mode) { get().desiredMode_ = mode; }

    void recreate(uint32_t w, uint32_t h);
    void cleanup();

    // Public accessors — used everywhere in the engine
    [[nodiscard]] VkSwapchainKHR    swapchain() const   { return swapchain_ ? *swapchain_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkFormat          format() const      { return surfaceFormat_.format; }
    [[nodiscard]] VkColorSpaceKHR   colorSpace() const  { return surfaceFormat_.colorSpace; }
    [[nodiscard]] VkExtent2D        extent() const      { return extent_; }
    [[nodiscard]] VkRenderPass      renderPass() const  { return renderPass_ ? *renderPass_ : VK_NULL_HANDLE; }
    [[nodiscard]] uint32_t          imageCount() const  { return static_cast<uint32_t>(images_.size()); }
    [[nodiscard]] VkImage           image(uint32_t i) const      { return images_[i]; }
    [[nodiscard]] VkImageView       imageView(uint32_t i) const  { return imageViews_[i] ? *imageViews_[i] : VK_NULL_HANDLE; }

    [[nodiscard]] bool            isHDR() const;
    [[nodiscard]] bool            is10Bit() const;
    [[nodiscard]] bool            isFP16() const;
    [[nodiscard]] const char*     formatName() const;
    [[nodiscard]] const char*     presentModeName() const;
    void                          updateWindowTitle(SDL_Window* window, float fps);

private:
    SwapchainManager() = default;
    ~SwapchainManager() { cleanup(); }

    void createSwapchain(uint32_t w, uint32_t h);
    void createImageViews();
    void createRenderPass();
    bool recreateSurfaceIfLost();

    static inline SwapchainManager* s_instance = nullptr;

    // Raw Vulkan handles — StoneKey protects the real ones via g_*() accessors
    VkInstance       vkInstance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDev_    = VK_NULL_HANDLE;
    VkDevice         device_     = VK_NULL_HANDLE;
    SDL_Window*      window_     = nullptr;
    VkSurfaceKHR     surface_    = VK_NULL_HANDLE;

    VkPresentModeKHR desiredMode_ = VK_PRESENT_MODE_MAX_ENUM_KHR;

    RTX::Handle<VkSwapchainKHR>    swapchain_;
    VkSurfaceFormatKHR             surfaceFormat_{};
    VkPresentModeKHR               presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D                     extent_{};

    std::vector<VkImage>                          images_;
    std::vector<RTX::Handle<VkImageView>>         imageViews_;
    RTX::Handle<VkRenderPass>                     renderPass_;
};

#define SWAPCHAIN SwapchainManager::get()

// PINK PHOTONS ETERNAL — VALHALLA UNBREACHABLE — FIRST LIGHT ACHIEVED