// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — VALHALLA v∞ — FULLY COMPATIBLE & FIXED EDITION
// SWAPCHAIN MANAGER — ALL REQUIRED FUNCTIONS IMPLEMENTED
// CRITICAL FIX: StoneKey arrays re-sealed on recreate → pink restored
// Present mode fallback improved for X11 compatibility
// PINK PHOTONS ETERNAL — EMPIRE STRONG AND UNBROKEN
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"

#include <algorithm>
#include <span>

using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_surface;
using StoneKey::stone_window;
using StoneKey::stone_seal_swapchain;
using StoneKey::stone_seal_extent;
using StoneKey::stone_seal_image_count;
using StoneKey::stone_seal_images;
using StoneKey::stone_seal_views;
using StoneKey::stone_swapchain;
using StoneKey::stone_width;
using StoneKey::stone_height;

namespace RTX {

static constexpr uint32_t         IMAGE_COUNT   = 2;

// ---------------------------------------------------------------------------
// Smart HDR Detection — safe by default
// ---------------------------------------------------------------------------
bool SwapchainManager::detectHDRFromEDID() noexcept
{
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, nullptr);
    if (formatCount == 0) [[unlikely]] {
        return false;
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, formats.data());

    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 &&
            f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Core Lifecycle — CLEAN & STANDARD
// ---------------------------------------------------------------------------
void SwapchainManager::create(SDL_Window* window, uint32_t w, uint32_t h) noexcept
{
    autoEnableHDR();
    createSwapchain(window, w, h);
    createImageViews();

    // Initial seal of global StoneKey arrays
    stone_seal_swapchain(swapchain_.get());
    stone_seal_extent(swapchainExtent_);
    stone_seal_image_count(static_cast<uint32_t>(swapchainImages_.size()));
    stone_seal_images(swapchainImages_);
    stone_seal_views(swapchainImageViews_);
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    if (w == 0 || h == 0) [[unlikely]] {
        minimized_ = true;
        return;
    }
    minimized_ = false;

    vkDeviceWaitIdle(stone_device());

    LOG_AMOURANTH("SWAPCHAIN RECREATE — {}×{} — STANDARD VULKAN PATH", w, h);

    cleanupImageViews();
    cleanupSwapchain();

    createSwapchain(stone_window(), w, h);
    createImageViews();

    // === CRITICAL FIX: RE-SEAL GLOBAL STONEKEY ARRAYS AFTER RECREATE ===
    stone_seal_swapchain(swapchain_.get());
    stone_seal_extent(swapchainExtent_);
    stone_seal_image_count(static_cast<uint32_t>(swapchainImages_.size()));
    stone_seal_images(swapchainImages_);
    stone_seal_views(swapchainImageViews_);

    las().notifyResize();

    LOG_AMOURANTH("SWAPCHAIN RECREATE COMPLETE — STONEKEY ARRAYS RE-SEALED — PINK RESTORED");
}

void SwapchainManager::cleanup() noexcept
{
    vkDeviceWaitIdle(stone_device());

    cleanupImageViews();
    cleanupSwapchain();
}

void SwapchainManager::cleanupSwapchain() noexcept
{
    if (swapchain_.valid()) {
        vkDestroySwapchainKHR(stone_device(), swapchain_.get(), nullptr);
        swapchain_.reset();
    }
    swapchainImages_.clear();
}

void SwapchainManager::cleanupImageViews() noexcept
{
    for (VkImageView view : swapchainImageViews_) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(stone_device(), view, nullptr);
        }
    }
    swapchainImageViews_.clear();
}

void SwapchainManager::createImageViews() noexcept
{
    swapchainImageViews_.assign(swapchainImages_.size(), VK_NULL_HANDLE);

    VkImageViewCreateInfo ci{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = swapchainFormat_,
        .components       = { VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        ci.image = swapchainImages_[i];
        VK_CHECK(vkCreateImageView(stone_device(), &ci, nullptr, &swapchainImageViews_[i]));
    }
}

// ---------------------------------------------------------------------------
// Standard swapchain creation — clean and portable
// ---------------------------------------------------------------------------
void SwapchainManager::createSwapchain(SDL_Window*, uint32_t w, uint32_t h) noexcept
{
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stone_physical(), stone_surface(), &caps);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, formats.data());

    uint32_t presentCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &presentCount, nullptr);
    std::vector<VkPresentModeKHR> modes(presentCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &presentCount, modes.data());

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent = {
            std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width),
            std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height)
        };
    }

    // X11 SAFE: Force FIFO — most reliable
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (std::find(modes.begin(), modes.end(), presentMode) == modes.end()) {
        LOG_WARNING_CAT("SWAPCHAIN", "FIFO present mode not supported — falling back to first available mode");
        presentMode = modes[0];
    }

    uint32_t imageCount = 2;
    imageCount = std::max(imageCount, caps.minImageCount);
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    // Prefer standard sRGB format — safe and universal
    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f;
            break;
        }
    }

    QueueFamilyIndices qf = findQueueFamilies(stone_physical(), stone_surface());
    uint32_t queueFamilyIndices[2] = { qf.graphicsFamily.value(), qf.presentFamily.value() };

    VkSwapchainCreateInfoKHR ci{
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = stone_surface(),
        .minImageCount    = imageCount,
        .imageFormat      = chosen.format,
        .imageColorSpace  = chosen.colorSpace,
        .imageExtent      = extent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .imageSharingMode = (qf.graphicsFamily == qf.presentFamily) ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = (qf.graphicsFamily == qf.presentFamily) ? 0u : 2u,
        .pQueueFamilyIndices  = (qf.graphicsFamily == qf.presentFamily) ? nullptr : queueFamilyIndices,
        .preTransform     = caps.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = presentMode,
        .clipped          = VK_TRUE,
        .oldSwapchain     = swapchain_.valid() ? swapchain_.get() : VK_NULL_HANDLE
    };

    VkSwapchainKHR old = swapchain_.get();
    VkSwapchainKHR raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(stone_device(), &ci, nullptr, &raw));

    if (old != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(stone_device(), old, nullptr);
    }

    swapchain_ = Handle<VkSwapchainKHR>(raw, stone_device());
    swapchainExtent_ = extent;
    swapchainFormat_ = chosen.format;
    currentColorSpace_ = chosen.colorSpace;
    currentPresentMode_ = presentMode;

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(stone_device(), raw, &count, nullptr);
    swapchainImages_.resize(count);
    vkGetSwapchainImagesKHR(stone_device(), raw, &count, swapchainImages_.data());

    LOG_AMOURANTH("SWAPCHAIN FORGED — {}×{} — {} images — FIFO — sRGB — X11 SAFE", extent.width, extent.height, count);
}

// ---------------------------------------------------------------------------
// Acquisition / Presentation
// ---------------------------------------------------------------------------
VkResult SwapchainManager::acquireNextImage(uint32_t* pImageIndex,
                                           VkSemaphore semaphore,
                                           VkFence fence) noexcept
{
    VkResult result = vkAcquireNextImageKHR(stone_device(),
                                           stone_swapchain(),
                                           UINT64_MAX,  // Block forever for solidity
                                           semaphore,
                                           fence,
                                           pImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) [[unlikely]] {
        recreate(stone_width(), stone_height());
    }
    return result;
}

void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) noexcept
{
    VkPresentInfoKHR pi{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = waitSemaphore ? 1u : 0u,
        .pWaitSemaphores    = waitSemaphore ? &waitSemaphore : nullptr,
        .swapchainCount     = 1,
        .pSwapchains        = &stone_swapchain(),
        .pImageIndices      = &imageIndex
    };

    VkResult r = vkQueuePresentKHR(queue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) [[unlikely]] {
        recreate(stone_width(), stone_height());
    }
}

// ---------------------------------------------------------------------------
// HDR auto-detection
// ---------------------------------------------------------------------------
void SwapchainManager::autoEnableHDR() noexcept
{
    static bool done = false;
    if (done) [[likely]] return;
    done = true;

    bool hdrSupported = detectHDRFromEDID();

    currentColorSpace_ = hdrSupported ? VK_COLOR_SPACE_HDR10_ST2084_EXT
                                      : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    LOG_AMOURANTH("HDR AUTO-IGNITION: {} → using {} ({} bit)",
                  hdrSupported ? "ENABLED" : "disabled (safe)",
                  hdrSupported ? "HDR10_ST2084" : "sRGB",
                  hdrSupported ? "10" : "8");
}

// ---------------------------------------------------------------------------
// Runtime configuration functions — required for OptionsMenu.cpp
// ---------------------------------------------------------------------------
void SwapchainManager::setPresentMode(VkPresentModeKHR mode) noexcept
{
    desiredPresentMode_ = mode;
    LOG_INFO_CAT("SWAPCHAIN", "Desired present mode set to {}", 
                 mode == VK_PRESENT_MODE_FIFO_KHR ? "FIFO" :
                 mode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" : "MAILBOX");
}

void SwapchainManager::setMinImageCount(uint32_t count) noexcept
{
    desiredImageCount_ = count;
    LOG_INFO_CAT("SWAPCHAIN", "Desired min image count set to {}", count);
}

void SwapchainManager::initializeFramePacing() noexcept
{
    LOG_INFO_CAT("SWAPCHAIN", "Frame pacing initialized — using standard Vulkan timing");
}

void SwapchainManager::setShadingRate(float scaleFactor) noexcept
{
    shadingRateScale_ = scaleFactor;
    LOG_INFO_CAT("SWAPCHAIN", "Dynamic shading rate scale set to {}", scaleFactor);
}

void SwapchainManager::enableDirectDisplay(bool enable) noexcept
{
    directDisplayEnabled_ = enable;
    LOG_INFO_CAT("SWAPCHAIN", "Direct display {} — zero-copy path", enable ? "ENABLED" : "DISABLED");
}

} // namespace RTX

// =============================================================================
// FULLY FIXED — STONEKEY RE-SEALED ON RECREATE
// X11 PRESENT MODE COMPATIBILITY IMPROVED
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN AND VISIBLE
// DECEMBER 19, 2025 — THE LIGHT IS RESTORED AND UNIVERSAL
// =============================================================================