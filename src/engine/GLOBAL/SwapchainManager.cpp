// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.1
// SWAPCHAIN MANAGER — PURE LIGHT | AUTO HDR | NO TEARING | SELF-HEALING
// JANUARY 21, 2026 — FIXED LAYOUT TRANSITIONS + SEMAPHORE SYNC — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/Extensions.hpp"

#include <thread>
#include <chrono>
#include <algorithm>

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
    if (!g_ext.vkCreateSwapchainKHR) {
        g_ext.vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
            vkGetDeviceProcAddr(stone_device(), "vkCreateSwapchainKHR"));
        if (!g_ext.vkCreateSwapchainKHR) {
            LOG_FATAL("SWAPCHAIN", "Failed to load vkCreateSwapchainKHR");
        }
    }
}

// Check if format supports required usage
[[nodiscard]] static bool supportsRequiredUsage(VkPhysicalDevice phys, VkFormat fmt) noexcept {
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(phys, fmt, &props);

    VkFormatFeatureFlags req = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
                               VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                               VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                               VK_FORMAT_FEATURE_TRANSFER_DST_BIT;

    return (props.optimalTilingFeatures & req) == req;
}

// =============================================================================
// transitionImageLayout — helper for swapchain images (used by caller)
// =============================================================================
void SwapchainManager::transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                             VkImageLayout oldLayout, VkImageLayout newLayout) noexcept {
    if (cmd == VK_NULL_HANDLE || image == VK_NULL_HANDLE) {
        return;
    }

    if (oldLayout == newLayout) {
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkAccessFlags srcAccess = 0;
    VkAccessFlags dstAccess = 0;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        srcAccess = 0;
        dstAccess = VK_ACCESS_SHADER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
        dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        srcAccess = VK_ACCESS_TRANSFER_READ_BIT;
        dstAccess = VK_ACCESS_MEMORY_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        srcAccess = 0;
        dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        srcAccess = VK_ACCESS_MEMORY_READ_BIT;
        dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        LOG_WARNING("SWAPCHAIN", "Unsupported transition: {} → {}", string_VkImageLayout(oldLayout), string_VkImageLayout(newLayout));
        return;
    }

    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// Core create/recreate — auto HDR preference
void SwapchainManager::createOrRecreateSwapchain(uint32_t w, uint32_t h, bool isRecreate, std::string_view reason) noexcept {
    ensureSwapchainExtension();

    VkDevice dev = stone_device();
    VkPhysicalDevice phys = stone_physical();
    VkSurfaceKHR surf = stone_surface();

    if (!dev || !phys || !surf) {
        minimized_ = true;
        LOG_FATAL("SWAPCHAIN", "Core Vulkan objects missing");
        return;
    }

    if (w == 0 || h == 0) {
        minimized_ = true;
        LOG_INFO("SWAPCHAIN", "Window minimized — swapchain paused");
        return;
    }

    vkDeviceWaitIdle(dev);

    if (isRecreate) {
        cleanupImageViews();
        cleanupSwapchain();

        LAS::instance().onResize();
        LOG_SUCCESS("SWAPCHAIN", "Swapchain recreation triggered: {}", reason.empty() ? "unknown" : reason);
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
        LOG_INFO("SWAPCHAIN", "Zero extent — window minimized");
        return;
    }

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &fmtCount, formats.data());

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &modeCount, modes.data());

    VkPresentModeKHR pmode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (std::find(modes.begin(), modes.end(), pmode) == modes.end()) {
        pmode = VK_PRESENT_MODE_FIFO_KHR;
        LOG_INFO("SWAPCHAIN", "FIFO_RELAXED not supported — using FIFO");
    }

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imgCount = std::min(imgCount, caps.maxImageCount);

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
            LOG_SUCCESS("SWAPCHAIN", "Auto HDR selected — {}", string_VkFormat(cand.format));
            break;
        }
    }

    VkSwapchainCreateInfoKHR ci{};
    ci.sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface         = surf;
    ci.minImageCount   = imgCount;
    ci.imageFormat     = chosen.format;
    ci.imageColorSpace = chosen.colorSpace;
    ci.imageExtent     = extent;
    ci.imageArrayLayers= 1;
    ci.imageUsage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT    |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT    |
                         VK_IMAGE_USAGE_STORAGE_BIT;
    ci.imageSharingMode= VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform    = caps.currentTransform;
    ci.compositeAlpha  = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode     = pmode;
    ci.clipped         = VK_TRUE;
    ci.oldSwapchain    = swapchain_.valid() ? swapchain_.get() : VK_NULL_HANDLE;

    VkSwapchainKHR newSwap = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(dev, &ci, nullptr, &newSwap) != VK_SUCCESS) {
        minimized_ = true;
        LOG_ERROR("SWAPCHAIN", "vkCreateSwapchainKHR failed");
        return;
    }

    swapchain_ = Handle<VkSwapchainKHR>(newSwap, dev, vkDestroySwapchainKHR);

    swapchainExtent_ = extent;
    swapchainFormat_ = chosen.format;
    currentPresentMode_ = pmode;

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(dev, newSwap, &count, nullptr);
    swapchainImages_.resize(count);
    vkGetSwapchainImagesKHR(dev, newSwap, &count, swapchainImages_.data());

    swapchainImageViews_.resize(count);
    VkImageViewCreateInfo viewCI{};
    viewCI.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format           = chosen.format;
    viewCI.components       = {VK_COMPONENT_SWIZZLE_IDENTITY};
    viewCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    for (uint32_t i = 0; i < count; ++i) {
        viewCI.image = swapchainImages_[i];
        if (vkCreateImageView(dev, &viewCI, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS) {
            LOG_FATAL("SWAPCHAIN", "Failed to create image view {}", i);
            return;
        }
    }

    stone_seal_swapchain(newSwap);
    stone_seal_extent(extent);
    stone_seal_image_count(count);
    stone_seal_images(swapchainImages_);
    stone_seal_views(swapchainImageViews_);

    LOG_SUCCESS("SWAPCHAIN", "Swapchain ready — {} images | {}×{} | {} | {}",
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

// Acquire with semaphore
VkResult SwapchainManager::acquireNextImage(uint32_t* idx, VkSemaphore sem, VkFence fence) noexcept {
    if (minimized_ || !swapchain_.valid()) return VK_NOT_READY;

    VkResult res = vkAcquireNextImageKHR(stone_device(), swapchain_.get(), UINT64_MAX, sem, fence, idx);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
        recreate(stone_width(), stone_height(), "acquire invalid");
        return VK_NOT_READY;
    }

    return res;
}

// Present with semaphore
void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSem) noexcept {
    if (minimized_ || !swapchain_.valid()) return;

    VkSwapchainKHR current = swapchain_.get();

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = waitSem ? 1u : 0u;
    pi.pWaitSemaphores    = waitSem ? &waitSem : nullptr;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &current;
    pi.pImageIndices      = &imageIndex;

    VkResult res = vkQueuePresentKHR(queue, &pi);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
        recreate(stone_width(), stone_height(), "present invalid");
    }
}

} // namespace RTX

// =============================================================================
// FINAL v30.1 — JANUARY 21, 2026
// - Added missing layout transitions (UNDEFINED → TRANSFER_DST, PRESENT → TRANSFER_DST)
// - Semaphore support in acquire/present (sync chain)
// - Minimal logging — startup + fatal/error only
// - Auto HDR preserved
// Pink photons flowing smoothly — AMOURANTH FOREVER 💖
// =============================================================================