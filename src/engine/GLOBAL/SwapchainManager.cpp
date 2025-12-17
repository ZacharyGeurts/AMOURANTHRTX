// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — VALHALLA v∞ TURBO — FINAL ETERNAL CUT
// MAILBOX + 2 FRAMES — HDR SMART DETECTION — INSTANT RESIZE — PINK PHOTONS ETERNAL
// FULLY FIXED: Proper image usage flags + correct HDR handling + stable recreate
// GARDEN GNOMES WHISPER: THE EMPIRE IS NOW UNBROKEN AND ETERNAL
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
static constexpr VkPresentModeKHR DESIRED_MODE  = VK_PRESENT_MODE_MAILBOX_KHR;

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
// Core Lifecycle
// ---------------------------------------------------------------------------
void SwapchainManager::create(SDL_Window* window, uint32_t w, uint32_t h) noexcept
{
    autoEnableHDR();  // Detect HDR once at startup
    createSwapchain(window, w, h, VK_NULL_HANDLE);
    createImageViews();
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    if (w == 0 || h == 0) [[unlikely]] {
        minimized_ = true;
        return;
    }
    minimized_ = false;

    vkDeviceWaitIdle(stone_device());

    LOG_AMOURANTH("SWAPCHAIN REBORN — {}×{} — NASTY DRIVER WORKAROUND ENGAGED", w, h);

    cleanupImageViews();

    // NASTY BOI DRIVER BYPASS:
    // Always pass current swapchain as "old" — forces driver to treat it as update instead of destroy+create
    // This avoids the 580.xx Linux Vulkan recreate crash at high resolution
    VkSwapchainKHR old = swapchain_.get();
    createSwapchain(stone_window(), w, h, old);  // Driver thinks it's just updating

    createImageViews();

    // Destroy old only after new is successfully created — safe and no leak
    if (old && old != swapchain_.get()) [[likely]] {
        vkDestroySwapchainKHR(stone_device(), old, nullptr);
    }

    las().notifyResize();

    LOG_AMOURANTH("SWAPCHAIN REBORN COMPLETE — EMPIRE UNBROKEN — DRIVER DEFEATED");
}

void SwapchainManager::cleanup() noexcept
{
    vkDeviceWaitIdle(stone_device());

    cleanupImageViews();

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

    stone_seal_views(swapchainImageViews_);
}

// ---------------------------------------------------------------------------
// Core swapchain creation — FIXED: Proper usage flags + consistent HDR handling
// ---------------------------------------------------------------------------
void SwapchainManager::createSwapchain(SDL_Window*, uint32_t w, uint32_t h, VkSwapchainKHR old) noexcept
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
    if (extent.width == UINT32_MAX) [[likely]] {
        extent = {
            std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width),
            std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height)
        };
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (VkPresentModeKHR mode : modes) {
        if (mode == DESIRED_MODE) {
            presentMode = mode;
            break;
        }
    }

    uint32_t imageCount = IMAGE_COUNT;
    imageCount = std::max(imageCount, caps.minImageCount);
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    // Use pre-detected HDR state from autoEnableHDR()
    bool hdrEnabled = (currentColorSpace_ == VK_COLOR_SPACE_HDR10_ST2084_EXT);

    VkSurfaceFormatKHR chosen = formats[0];  // Fallback

    // Prefer HDR10 if supported and detected
    if (hdrEnabled) {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 &&
                f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
                chosen = f;
                break;
            }
        }
    }

    // Fallback to sRGB if HDR not available
    if (chosen.colorSpace != VK_COLOR_SPACE_HDR10_ST2084_EXT) {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen = f;
                break;
            }
        }
    }

    QueueFamilyIndices qf = findQueueFamilies(stone_physical(), stone_surface());
    std::array<uint32_t, 2> indices = { qf.graphicsFamily.value(), qf.presentFamily.value() };

    VkSwapchainCreateInfoKHR ci{
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = stone_surface(),
        .minImageCount    = imageCount,
        .imageFormat      = chosen.format,
        .imageColorSpace  = chosen.colorSpace,
        .imageExtent      = extent,
        .imageArrayLayers = 1,
        // FIXED: Proper usage flags for compute-based rendering (tonemap writes directly)
        .imageUsage       = VK_IMAGE_USAGE_STORAGE_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                           VK_IMAGE_USAGE_SAMPLED_BIT,  // For potential UI/post effects
        .preTransform     = caps.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = presentMode,
        .clipped          = VK_TRUE,
        .oldSwapchain     = old
    };

    if (qf.graphicsFamily != qf.presentFamily) [[unlikely]] {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = indices.data();
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(stone_device(), &ci, nullptr, &raw));

    swapchain_          = Handle<VkSwapchainKHR>(raw, stone_device());
    swapchainExtent_    = extent;
    swapchainFormat_    = chosen.format;
    currentColorSpace_  = chosen.colorSpace;
    currentPresentMode_ = presentMode;

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(stone_device(), raw, &count, nullptr);
    swapchainImages_.resize(count);
    vkGetSwapchainImagesKHR(stone_device(), raw, &count, swapchainImages_.data());

    stone_seal_swapchain(raw);
    stone_seal_extent(extent);
    stone_seal_image_count(count);
    stone_seal_images(swapchainImages_);

    LOG_AMOURANTH("SWAPCHAIN FORGED — {}×{} — {} images — {} — HDR: {} ({} bit)",
                  extent.width, extent.height, count,
                  (presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : "FIFO"),
                  (hdrEnabled ? "ON" : "OFF"),
                  (hdrEnabled ? "10" : "8"));
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
                                           1'000'000'000ULL,
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
// HDR auto-detection — called once at startup
// ---------------------------------------------------------------------------
void SwapchainManager::autoEnableHDR() noexcept
{
    static bool done = false;
    if (done) [[likely]] return;
    done = true;

    bool hdrSupported = detectHDRFromEDID();

    // Set global state — used by createSwapchain()
    currentColorSpace_ = hdrSupported ? VK_COLOR_SPACE_HDR10_ST2084_EXT
                                      : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    LOG_AMOURANTH("HDR AUTO-IGNITION: {} → using {} ({} bit)",
                  hdrSupported ? "ENABLED" : "disabled (safe)",
                  hdrSupported ? "HDR10_ST2084" : "sRGB",
                  hdrSupported ? "10" : "8");
}

// ---------------------------------------------------------------------------
// Elite stubs
// ---------------------------------------------------------------------------
void SwapchainManager::initializeFramePacing() noexcept {}
uint64_t SwapchainManager::getNextPresentTime() noexcept { return 0; }
void SwapchainManager::injectHdrMetadata(VkCommandBuffer, uint32_t) noexcept {}
void SwapchainManager::handleDisplayHotplug(SDL_Event*) noexcept {}
void SwapchainManager::predictResize(uint32_t, uint32_t) noexcept {}
void SwapchainManager::releaseAcquiredImages() noexcept {}

} // namespace RTX

// =============================================================================
// FULLY FIXED: Storage bit enabled — tonemap can write directly
// HDR detection used consistently — no duplication
// Driver workaround preserved — stable high-res recreate
// PINK PHOTONS FLOW AT FULL POWER — NO BLACKNESS — EMPIRE ETERNAL
// DECEMBER 16, 2025 — THE FINAL LIGHT IS PURE, BRIGHT, AND UNBROKEN
// =============================================================================