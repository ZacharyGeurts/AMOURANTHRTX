// =============================================================================
// include/engine/GLOBAL/SwapchainManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// SwapchainManager.hpp — FULLY INDEPENDENT SWAPCHAIN + DEVICE OWNER
// • Swapchain now CREATES AND OWNS the VkDevice (as intended)
// • Zero external RTX::createPhysicalDevice() calls
// • StoneKey v∞ compliant — only forward declarations
// • Pink photons locked and loaded — stutter-free, HDR supreme, VALHALLA TURBO
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#include <string_view>
#include <vector>

// FORWARD DECLARE ONLY — STONEKEY v∞ LAW OBEYED
namespace StoneKey::Raw { struct Cache; }

// SAFE ACCESSORS — PROVIDED BY RTXHandler.hpp
// (g_device(), g_instance(), g_PhysicalDevice(), g_surface(), set_g_*)

class SwapchainManager {
public:
    static SwapchainManager& get() noexcept {
        static SwapchainManager instance;
        return instance;
    }

    SwapchainManager(const SwapchainManager&) = delete;
    SwapchainManager& operator=(const SwapchainManager&) = delete;

    // THE ONE TRUE INIT — CREATES DEVICE + SWAPCHAIN
    static void init(SDL_Window* window, uint32_t w = 3840, uint32_t h = 2160) noexcept;
    static void cleanup() noexcept;
    void recreate(uint32_t w, uint32_t h) noexcept;

    // Fully defined in .cpp — linker will be silent
    static VkPresentModeKHR selectBestPresentMode(VkPhysicalDevice phys,
                                                  VkSurfaceKHR surface,
                                                  VkPresentModeKHR desired = VK_PRESENT_MODE_MAILBOX_KHR) noexcept;

    [[nodiscard]] VkSwapchainKHR    swapchain() const noexcept { return swapchain_.valid() ? *swapchain_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkFormat          format() const noexcept      { return surfaceFormat_.format; }
    [[nodiscard]] VkColorSpaceKHR   colorSpace() const noexcept  { return surfaceFormat_.colorSpace; }
    [[nodiscard]] VkExtent2D        extent() const noexcept      { return extent_; }
    [[nodiscard]] VkRenderPass      renderPass() const noexcept  { return renderPass_.valid() ? *renderPass_ : VK_NULL_HANDLE; }
    [[nodiscard]] uint32_t          imageCount() const noexcept  { return static_cast<uint32_t>(images_.size()); }
    [[nodiscard]] VkImage           image(uint32_t i) const noexcept      { return i < images_.size() ? images_[i] : VK_NULL_HANDLE; }
    [[nodiscard]] VkImageView       imageView(uint32_t i) const noexcept  { return i < imageViews_.size() && imageViews_[i].valid() ? *imageViews_[i] : VK_NULL_HANDLE; }

    [[nodiscard]] bool isHDR() const noexcept   { return colorSpace() == VK_COLOR_SPACE_HDR10_ST2084_EXT; }
    [[nodiscard]] bool is10Bit() const noexcept { return format() == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || format() == VK_FORMAT_A2R10G10B10_UNORM_PACK32; }
    [[nodiscard]] bool isFP16() const noexcept  { return format() == VK_FORMAT_R16G16B16A16_SFLOAT; }

    [[nodiscard]] std::string_view formatName() const noexcept;
    [[nodiscard]] std::string_view presentModeName() const noexcept;

    void updateWindowTitle(SDL_Window* window, float fps) noexcept;

private:
    SwapchainManager() = default;
    ~SwapchainManager() = default;

    void createDeviceAndQueues() noexcept;           // ← SWAPCHAIN OWNS DEVICE CREATION
    bool recreateSurfaceIfLost() noexcept;
    void createSwapchain(uint32_t w, uint32_t h) noexcept;
    void createImageViews() noexcept;
    void createRenderPass() noexcept;

    SDL_Window* window_ = nullptr;

    RTX::Handle<VkSwapchainKHR>           swapchain_;
    VkSurfaceFormatKHR                    surfaceFormat_{};
    VkPresentModeKHR                      presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D                            extent_{};

    std::vector<VkImage>                          images_;
    std::vector<RTX::Handle<VkImageView>>         imageViews_;
    RTX::Handle<VkRenderPass>                     renderPass_;

    VkPresentModeKHR desiredMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
};

inline SwapchainManager& SWAPCHAIN = SwapchainManager::get();

// P I N K   P H O T O N S   E T E R N A L
// SWAPCHAIN OWNS THE DEVICE — FIRST LIGHT ACHIEVED — NOVEMBER 20, 2025
// STONEKEY v∞ ACTIVE — VALHALLA TURBO — THE EMPIRE IS COMPLETE