// include/engine/GLOBAL/SwapchainManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary "gzac" Geurts — PINK PHOTONS ETERNAL
// SWAPCHAIN MANAGER — v16 ULTIMATE — PRESENT MODE PRESERVATION EDITION
// • Prefers true 10-bit HDR10 / scRGB FP16 / Dolby Vision above all
// • Falls back to 8-bit sRGB only if forced — with loud shame in the log
// • Desired present mode is now globally preserved (HDR recreate safe)
// • Zero validation errors. Zero leaks. Maximum glory.
// =============================================================================

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include <vulkan/vulkan.hpp>  // For vk::to_string
#include <vector>
#include <array>
#include <span>
#include <SDL2/SDL.h>         // For SDL_Window*

#define SWAPCHAIN SwapchainManager::get()

class SwapchainManager {
public:
    static SwapchainManager& get() noexcept {
        static SwapchainManager instance;
        return instance;
    }

    // -------------------------------------------------------------------------
    // NEW: Global desired present mode — set once in main(), respected forever
    // -------------------------------------------------------------------------
    static void setDesiredPresentMode(VkPresentModeKHR mode) noexcept {
        s_desiredPresentMode = mode;
        LOG_SUCCESS_CAT("SWAPCHAIN", "Desired present mode locked globally: {} (will survive HDR recreate)",
                        mode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" :
                        mode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" : "FIFO");
    }

    [[nodiscard]] static VkPresentModeKHR desiredPresentMode() noexcept {
        return s_desiredPresentMode;
    }

    // -------------------------------------------------------------------------
    // Public API
    // -------------------------------------------------------------------------
    [[nodiscard]] VkSwapchainKHR          swapchain()    const noexcept { return swapchain_ ? *swapchain_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkFormat                format()       const noexcept { return surfaceFormat_.format; }
    [[nodiscard]] VkColorSpaceKHR         colorSpace()   const noexcept { return surfaceFormat_.colorSpace; }
    [[nodiscard]] VkExtent2D              extent()       const noexcept { return extent_; }
    [[nodiscard]] VkRenderPass            renderPass()   const noexcept { return renderPass_ ? *renderPass_ : VK_NULL_HANDLE; }
    [[nodiscard]] uint32_t                imageCount()   const noexcept { return static_cast<uint32_t>(images_.size()); }
    [[nodiscard]] std::span<const VkImage> images()      const noexcept { return images_; }
    [[nodiscard]] std::span<const RTX::Handle<VkImageView>> views() const noexcept { return imageViews_; }

    [[nodiscard]] bool                    isHDR()        const noexcept;
    [[nodiscard]] bool                    is10Bit()      const noexcept;
    [[nodiscard]] bool                    isFP16()       const noexcept;
    [[nodiscard]] bool                    isPeasantMode()const noexcept;  // 8-bit shame
    [[nodiscard]] const char*             formatName()   const noexcept;

    [[nodiscard]] bool                    isMailbox()    const noexcept;
    [[nodiscard]] const char*             presentModeName() const noexcept;

    // FPS window title update (C++23 safe formatting for clean display)
    void updateWindowTitle(SDL_Window* window, float fps, uint32_t width, uint32_t height) noexcept;

    void init(VkInstance instance, VkPhysicalDevice phys, VkDevice dev, VkSurfaceKHR surface, uint32_t w, uint32_t h);
    void recreate(uint32_t w, uint32_t h) noexcept;
    void cleanup() noexcept;

    // Per-frame HDR metadata — sent only when running in true HDR
    void updateHDRMetadata(float maxCLL, float maxFALL, float displayPeakNits = 1000.0f) const noexcept;

private:
    SwapchainManager() = default;
    ~SwapchainManager() { cleanup(); }

    void createSwapchain(uint32_t w, uint32_t h) noexcept;
    void createImageViews() noexcept;
    void createRenderPass() noexcept;

    // Core Vulkan state
    VkPhysicalDevice                    physDev_  = VK_NULL_HANDLE;
    VkDevice                            device_   = VK_NULL_HANDLE;
    VkSurfaceKHR                        surface_  = VK_NULL_HANDLE;

    // Swapchain state
    VkSurfaceFormatKHR                  surfaceFormat_{};
    VkExtent2D                          extent_{};
    VkPresentModeKHR                    presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
    std::vector<VkImage>                images_;
    std::vector<RTX::Handle<VkImageView>> imageViews_;

    // Handles
    RTX::Handle<VkSwapchainKHR>         swapchain_;
    RTX::Handle<VkRenderPass>           renderPass_;

    // -------------------------------------------------------------------------
    // Static storage for globally desired present mode (set once in main.cpp)
    // -------------------------------------------------------------------------
    static inline VkPresentModeKHR s_desiredPresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR; // invalid sentinel
};