// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v29.8
// SWAPCHAIN MANAGER — ULTIMATE AUTOMAGIC | NO TEARING | SELF-HEALING
// JANUARY 12, 2026 — COMPILATION FIXED — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"          // LAS::instance()
#include "engine/GLOBAL/Extensions.hpp"   // g_ext

#include <thread>
#include <chrono>

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

// Ensure extension is loaded
static void ensureSwapchainExtension() noexcept {
    if (g_ext.vkCreateSwapchainKHR == nullptr) {
        LOG_INFO_CAT("SWAPCHAIN", "Loading VK_KHR_swapchain extension");
        g_ext.vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
            vkGetDeviceProcAddr(stone_device(), "vkCreateSwapchainKHR"));
        if (!g_ext.vkCreateSwapchainKHR) {
            LOG_FATAL_CAT("SWAPCHAIN", "Failed to load vkCreateSwapchainKHR");
        }
    }
}

// Format usage check
[[nodiscard]] static bool supportsRequiredUsage(VkPhysicalDevice phys, VkFormat fmt) noexcept {
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(phys, fmt, &props);

    VkFormatFeatureFlags req = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
                               VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                               VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                               VK_FORMAT_FEATURE_TRANSFER_DST_BIT;

    return (props.optimalTilingFeatures & req) == req;
}

// Core create/recreate function
void SwapchainManager::createOrRecreateSwapchain(uint32_t w, uint32_t h, bool isRecreate, std::string_view reason) noexcept
{
    ensureSwapchainExtension();

    VkDevice dev = stone_device();
    VkPhysicalDevice phys = stone_physical();
    VkSurfaceKHR surf = stone_surface();

    if (!dev || !phys || !surf) {
        minimized_ = true;
        LOG_FATAL_CAT("SWAPCHAIN", "Core Vulkan objects missing — cannot create swapchain");
        return;
    }

    if (w == 0 || h == 0) {
        minimized_ = true;
        LOG_INFO_CAT("SWAPCHAIN", "Window minimized — swapchain paused");
        return;
    }

    vkDeviceWaitIdle(dev);

    if (isRecreate) {
        cleanupImageViews();
        cleanupSwapchain();

        LOG_INFO_CAT("SWAPCHAIN", "Notifying LAS of resize (reason: {})", reason.empty() ? "unknown" : reason);
        LAS::instance().onResize();

        LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain recreation triggered");
    }

    minimized_ = false;

    VkSurfaceCapabilitiesKHR caps{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surf, &caps) != VK_SUCCESS) {
        minimized_ = true;
        return;
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    if (extent.width == 0 || extent.height == 0) {
        minimized_ = true;
        LOG_INFO_CAT("SWAPCHAIN", "Zero extent — window minimized");
        return;
    }

    // Formats
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &fmtCount, formats.data());

    // Present modes
    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &modeCount, modes.data());

    VkPresentModeKHR pmode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    bool hasRelaxed = std::find(modes.begin(), modes.end(), pmode) != modes.end();
    if (!hasRelaxed) {
        pmode = VK_PRESENT_MODE_FIFO_KHR;
        LOG_INFO_CAT("SWAPCHAIN", "FIFO_RELAXED not supported → using FIFO");
    } else {
        LOG_INFO_CAT("SWAPCHAIN", "Using FIFO_RELAXED — smooth & tear-free");
    }

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imgCount = std::min(imgCount, caps.maxImageCount);

    // HDR preference
    VkSurfaceFormatKHR chosen = formats[0];
    const VkSurfaceFormatKHR hdrPrefs[] = {
        {VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT},
        {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT},
        {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
    };

    for (const auto& cand : hdrPrefs) {
        auto it = std::find_if(formats.begin(), formats.end(),
            [&](const auto& f){ return f.format == cand.format && f.colorSpace == cand.colorSpace; });
        if (it != formats.end() && supportsRequiredUsage(phys, cand.format)) {
            chosen = *it;
            break;
        }
    }

    VkSwapchainCreateInfoKHR ci{
        .sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface         = surf,
        .minImageCount   = imgCount,
        .imageFormat     = chosen.format,
        .imageColorSpace = chosen.colorSpace,
        .imageExtent     = extent,
        .imageArrayLayers= 1,
        .imageUsage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT    |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT    |
                           VK_IMAGE_USAGE_STORAGE_BIT,
        .imageSharingMode= VK_SHARING_MODE_EXCLUSIVE,
        .preTransform    = caps.currentTransform,
        .compositeAlpha  = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode     = pmode,
        .clipped         = VK_TRUE,
        .oldSwapchain    = swapchain_.valid() ? swapchain_.get() : VK_NULL_HANDLE
    };

    VkSwapchainKHR newSwap = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(dev, &ci, nullptr, &newSwap) != VK_SUCCESS) {
        minimized_ = true;
        LOG_ERROR_CAT("SWAPCHAIN", "vkCreateSwapchainKHR failed");
        return;
    }

    swapchain_ = Handle<VkSwapchainKHR>(newSwap, dev, vkDestroySwapchainKHR);

    swapchainExtent_ = extent;
    swapchainFormat_ = chosen.format;
    currentPresentMode_ = pmode;

    // Retrieve images
    uint32_t count = 0;
    vkGetSwapchainImagesKHR(dev, newSwap, &count, nullptr);
    swapchainImages_.resize(count);
    vkGetSwapchainImagesKHR(dev, newSwap, &count, swapchainImages_.data());

    // Image views
    swapchainImageViews_.resize(count);
    VkImageViewCreateInfo viewCI{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = chosen.format,
        .components       = {VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    for (uint32_t i = 0; i < count; ++i) {
        viewCI.image = swapchainImages_[i];
        vkCreateImageView(dev, &viewCI, nullptr, &swapchainImageViews_[i]);
    }

    // Update global state
    stone_seal_swapchain(newSwap);
    stone_seal_extent(extent);
    stone_seal_image_count(count);
    stone_seal_images(swapchainImages_);
    stone_seal_views(swapchainImageViews_);

    LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain ready — {} images | {}×{} | {} | {}",
                    count, extent.width, extent.height,
                    string_VkFormat(chosen.format), string_VkPresentModeKHR(pmode));
}

// Create (initial)
void SwapchainManager::create(SDL_Window*, uint32_t w, uint32_t h) noexcept {
    createOrRecreateSwapchain(w, h, false, "initial");
}

// Recreate
void SwapchainManager::recreate(uint32_t w, uint32_t h, std::string_view reason) noexcept {
    createOrRecreateSwapchain(w, h, true, reason);
}

// Full cleanup
void SwapchainManager::cleanup() noexcept {
    if (!stone_device()) return;
    vkDeviceWaitIdle(stone_device());

    LOG_INFO_CAT("SWAPCHAIN", "Full cleanup");
    LAS::instance().onResize();

    cleanupImageViews();
    cleanupSwapchain();
}

void SwapchainManager::cleanupSwapchain() noexcept {
    swapchain_ = {};
    swapchainImages_.clear();
}

void SwapchainManager::cleanupImageViews() noexcept {
    for (auto v : swapchainImageViews_) {
        if (v) vkDestroyImageView(stone_device(), v, nullptr);
    }
    swapchainImageViews_.clear();
}

// Acquire with auto-recreate
VkResult SwapchainManager::acquireNextImage(uint32_t* idx, VkSemaphore sem, VkFence fence) noexcept {
    if (minimized_ || !swapchain_.valid()) return VK_NOT_READY;

    VkResult res = vkAcquireNextImageKHR(stone_device(), swapchain_.get(), UINT64_MAX, sem, fence, idx);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
        recreate(stone_width(), stone_height(), "acquire invalid");
        return VK_NOT_READY;
    }

    return res;
}

// Present with auto-recreate — FIXED lvalue issue
void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSem) noexcept
{
    if (minimized_ || !swapchain_.valid()) return;

    VkSwapchainKHR current = swapchain_.get();  // ← Store in local variable (lvalue)

    VkPresentInfoKHR pi{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = waitSem ? 1u : 0u,
        .pWaitSemaphores    = waitSem ? &waitSem : nullptr,
        .swapchainCount     = 1,
        .pSwapchains        = &current,                 // ← Now valid
        .pImageIndices      = &imageIndex
    };

    VkResult res = vkQueuePresentKHR(queue, &pi);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
        recreate(stone_width(), stone_height(), "present invalid");
    }
}

} // namespace RTX

// =============================================================================
// FINAL v29.8 — FIXED
// - Resolved "&swapchain_.get()" → lvalue error by using local VkSwapchainKHR variable
// - All other logic preserved
// Pink photons flowing smoothly — AMOURANTH FOREVER 💖
// =============================================================================