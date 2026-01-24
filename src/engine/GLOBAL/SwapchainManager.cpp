// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.60
// SWAPCHAIN MANAGER — HDR | SELF-HEALING | DEFERRED RECREATE | DYNAMIC PRESENT MODE
// JANUARY 24, 2026
// - Dynamic present mode selection — prefers MAILBOX, graceful fallback
// - Optional STORAGE_BIT for direct swapchain write (Options::Rendering::DIRECT_SWAPCHAIN_WRITE)
// - Logs chosen mode and direct write status
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <algorithm>
#include <format>

using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_surface;
using StoneKey::stone_swapchain;
using StoneKey::stone_seal_swapchain;
using StoneKey::stone_seal_extent;
using StoneKey::stone_seal_image_count;
using StoneKey::stone_seal_images;
using StoneKey::stone_seal_views;

namespace RTX {

static void ensureSwapchainExtension() noexcept {
    if (!g_ext.vkCreateSwapchainKHR) {
        g_ext.vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
            vkGetDeviceProcAddr(stone_device(), "vkCreateSwapchainKHR"));
    }
    if (!g_ext.vkDestroySwapchainKHR) {
        g_ext.vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
            vkGetDeviceProcAddr(stone_device(), "vkDestroySwapchainKHR"));
    }
    if (!g_ext.vkGetSwapchainImagesKHR) {
        g_ext.vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
            vkGetDeviceProcAddr(stone_device(), "vkGetSwapchainImagesKHR"));
    }
    if (!g_ext.vkQueuePresentKHR) {
        g_ext.vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(
            vkGetDeviceProcAddr(stone_device(), "vkQueuePresentKHR"));
    }
}

// =============================================================================
// isReady — checks if swapchain is fully usable
// =============================================================================
bool SwapchainManager::isReady() noexcept {
    return swapchain_.valid() &&
           !swapchainImages_.empty() &&
           !swapchainImageViews_.empty() &&
           swapchainExtent_.width > 0 &&
           swapchainExtent_.height > 0 &&
           !minimized_;
}

// =============================================================================
// ensureReady — safe to call every frame — only recreates if needed
// =============================================================================
void SwapchainManager::ensureReady(uint32_t w, uint32_t h) noexcept {
    if (isReady() && swapchainExtent_.width == w && swapchainExtent_.height == h) {
        return;
    }

    if (minimized_ || w == 0 || h == 0) {
        minimized_ = true;
        LOG_WARN_CAT("SWAPCHAIN", "Cannot ensure ready — minimized or zero extent");
        return;
    }

    LOG_INFO_CAT("SWAPCHAIN", "Ensuring swapchain ready — {}×{}", w, h);

    vkDeviceWaitIdle(stone_device());

    createOrRecreateSwapchain(w, h, true, "ensureReady");

    if (!isReady()) {
        minimized_ = true;
        LOG_ERROR_CAT("SWAPCHAIN", "Failed to ensure ready after recreation");
    } else {
        LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain ensured ready — {} images | {}×{}", 
                        swapchainImages_.size(), w, h);
    }
}

// =============================================================================
// transitionImageLayout — static helper
// =============================================================================
void SwapchainManager::transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                             VkImageLayout oldLayout, VkImageLayout newLayout) noexcept {
    if (cmd == VK_NULL_HANDLE || image == VK_NULL_HANDLE || oldLayout == newLayout) return;

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
    } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
        dstAccess = VK_ACCESS_MEMORY_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        srcAccess = VK_ACCESS_MEMORY_READ_BIT;
        dstAccess = VK_ACCESS_SHADER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        srcAccess = 0;
        dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstAccess = VK_ACCESS_MEMORY_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        srcAccess = VK_ACCESS_MEMORY_READ_BIT;
        dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        return;
    }

    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// =============================================================================
// createOrRecreateSwapchain — core logic with dynamic present mode + optional storage
// =============================================================================
void SwapchainManager::createOrRecreateSwapchain(uint32_t w, uint32_t h, bool isRecreate, std::string_view reason) noexcept {
    ensureSwapchainExtension();

    VkDevice dev = stone_device();
    VkPhysicalDevice phys = stone_physical();
    VkSurfaceKHR surf = stone_surface();

    if (!dev || !phys || !surf || w == 0 || h == 0) {
        minimized_ = true;
        LOG_WARN_CAT("SWAPCHAIN", "Cannot create swapchain — invalid params");
        return;
    }

    vkDeviceWaitIdle(dev);

    if (isRecreate) {
        cleanupImageViews();
        if (swapchain_.valid()) {
            vkDestroySwapchainKHR(dev, swapchain_.get(), nullptr);
            swapchain_ = {};
        }
        LAS::instance().onResize();
    }

    minimized_ = false;

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surf, &caps);

    VkExtent2D extent = caps.currentExtent.width == UINT32_MAX ?
        VkExtent2D{std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width),
                   std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height)} :
        caps.currentExtent;

    if (extent.width == 0 || extent.height == 0) {
        minimized_ = true;
        LOG_WARN_CAT("SWAPCHAIN", "Zero extent — marked minimized");
        return;
    }

    // Query formats
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &fmtCount, formats.data());

    VkSurfaceFormatKHR chosenFormat = formats[0];
    const VkSurfaceFormatKHR formatPrefs[] = {
        {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
    };

    for (const auto& pref : formatPrefs) {
        auto it = std::find_if(formats.begin(), formats.end(),
            [&](const auto& f) { return f.format == pref.format && f.colorSpace == pref.colorSpace; });
        if (it != formats.end()) {
            chosenFormat = *it;
            break;
        }
    }

    // Query present modes and select best
    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &pmCount, presentModes.data());

    VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR; // Guaranteed safe fallback

    const VkPresentModeKHR pmPrefs[] = {
        VK_PRESENT_MODE_MAILBOX_KHR,         // Best: triple buffer, low latency
        VK_PRESENT_MODE_FIFO_RELAXED_KHR,    // Good: vsync with tear on late frames
        VK_PRESENT_MODE_IMMEDIATE_KHR,       // Tearing allowed
        VK_PRESENT_MODE_FIFO_KHR             // Strict vsync
    };

    for (auto pref : pmPrefs) {
        if (std::find(presentModes.begin(), presentModes.end(), pref) != presentModes.end()) {
            chosenPM = pref;
            break;
        }
    }

    LOG_INFO_CAT("SWAPCHAIN", "Chosen present mode: {}", 
                 chosenPM == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX (optimal)" :
                 chosenPM == VK_PRESENT_MODE_FIFO_RELAXED_KHR ? "FIFO_RELAXED" :
                 chosenPM == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" :
                 "FIFO (fallback)");

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imgCount = std::min(imgCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = surf;
    ci.minImageCount    = imgCount;
    ci.imageFormat      = chosenFormat.format;
    ci.imageColorSpace  = chosenFormat.colorSpace;
    ci.imageExtent      = extent;
    ci.imageArrayLayers = 1;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = chosenPM;
    ci.clipped          = VK_TRUE;
    ci.oldSwapchain     = swapchain_.valid() ? swapchain_.get() : VK_NULL_HANDLE;

    // Add STORAGE_BIT if direct write enabled
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (Options::Rendering::DIRECT_SWAPCHAIN_WRITE) {
        ci.imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
        LOG_INFO_CAT("SWAPCHAIN", "Direct swapchain write enabled — STORAGE_BIT added");
    }

    VkSwapchainKHR newSwap = VK_NULL_HANDLE;
    VkResult res = vkCreateSwapchainKHR(dev, &ci, nullptr, &newSwap);
    if (res != VK_SUCCESS) {
        minimized_ = true;
        LOG_FATAL_CAT("SWAPCHAIN", "vkCreateSwapchainKHR failed: {}", string_VkResult(res));
        return;
    }

    swapchain_ = Handle<VkSwapchainKHR>(newSwap, dev, vkDestroySwapchainKHR);
    swapchainExtent_ = extent;
    swapchainFormat_ = chosenFormat.format;

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(dev, newSwap, &count, nullptr);
    swapchainImages_.resize(count);
    vkGetSwapchainImagesKHR(dev, newSwap, &count, swapchainImages_.data());

    swapchainImageViews_.resize(count);
    VkImageViewCreateInfo viewCI{};
    viewCI.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format           = chosenFormat.format;
    viewCI.components       = {VK_COMPONENT_SWIZZLE_IDENTITY};
    viewCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    for (uint32_t i = 0; i < count; ++i) {
        viewCI.image = swapchainImages_[i];
        res = vkCreateImageView(dev, &viewCI, nullptr, &swapchainImageViews_[i]);
        if (res != VK_SUCCESS) {
            LOG_FATAL_CAT("SWAPCHAIN", "Failed to create image view {}: {}", i, string_VkResult(res));
            minimized_ = true;
            return;
        }
    }

    stone_seal_swapchain(newSwap);
    stone_seal_extent(extent);
    stone_seal_image_count(count);
    stone_seal_images(swapchainImages_);
    stone_seal_views(swapchainImageViews_);

    LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain created — {} images | {}×{} | {} | {}", 
                    count, extent.width, extent.height, string_VkFormat(chosenFormat.format), reason);
}

// =============================================================================
// acquireNextImage — return error for caller to flag recreate
// =============================================================================
VkResult SwapchainManager::acquireNextImage(uint32_t* pImageIndex, VkSemaphore semaphore, VkFence fence) noexcept {
    if (minimized_ || !swapchain_.valid()) {
        LOG_WARN_CAT("SWAPCHAIN", "Acquire failed — minimized or invalid swapchain");
        return VK_NOT_READY;
    }

    VkResult res = vkAcquireNextImageKHR(stone_device(), swapchain_.get(), UINT64_MAX, semaphore, fence, pImageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
        LOG_WARN_CAT("SWAPCHAIN", "Acquire detected invalid swapchain — flag recreate next frame");
        return res;
    }

    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("SWAPCHAIN", "vkAcquireNextImageKHR failed: {}", string_VkResult(res));
    }

    return res;
}

// =============================================================================
// presentImage — return result for caller to flag recreate
// =============================================================================
VkResult SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSem) noexcept {
    if (minimized_ || !swapchain_.valid()) return VK_ERROR_INITIALIZATION_FAILED;

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = waitSem ? 1 : 0;
    pi.pWaitSemaphores    = waitSem ? &waitSem : nullptr;
    pi.swapchainCount     = 1;
	VkSwapchainKHR currentSwap = stone_swapchain();
    pi.pSwapchains        = &currentSwap;
    pi.pImageIndices      = &imageIndex;

    VkResult res = vkQueuePresentKHR(queue, &pi);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
        LOG_WARN_CAT("SWAPCHAIN", "Present detected invalid swapchain — flag recreate next frame");
    } else if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("SWAPCHAIN", "vkQueuePresentKHR failed: {}", string_VkResult(res));
    }

    return res;
}

// =============================================================================
// recreate — alias for createOrRecreateSwapchain
// =============================================================================
void SwapchainManager::recreate(uint32_t width, uint32_t height, std::string_view reason) noexcept {
    createOrRecreateSwapchain(width, height, true, reason);
}

// =============================================================================
// create — initial creation
// =============================================================================
void SwapchainManager::create(SDL_Window* window, uint32_t width, uint32_t height) noexcept {
    createOrRecreateSwapchain(width, height, false, "initial");
}

// =============================================================================
// cleanup — full shutdown
// =============================================================================
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
    for (auto v : swapchainImageViews_) vkDestroyImageView(stone_device(), v, nullptr);
    swapchainImageViews_.clear();
}

} // namespace RTX

// =============================================================================
// SwapchainManager v30.60 — JANUARY 24, 2026
// - Dynamic present mode selection — prefers MAILBOX, falls back gracefully
// - Optional STORAGE_BIT for direct swapchain write (Options::Rendering::DIRECT_SWAPCHAIN_WRITE)
// - Logs chosen mode and direct write status
// - Deferred recreate, HDR-ready, self-healing
// =============================================================================