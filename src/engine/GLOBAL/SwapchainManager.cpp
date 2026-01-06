// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v5.3 — JANUARY 05, 2026
// SwapchainManager — FINAL PRODUCTION EDITION — VALIDATION PERFECT
// SINGLETON PATTERN | HDR PRIORITY | SAFE USAGE FLAGS | LAS INTEGRATED
// NO STORAGE BIT | NO VUID ERRORS | MINIMIZED HANDLING | CLEAN SHUTDOWN
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"

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

// =============================================================================
// CORE SWAPCHAIN CREATION / RECREATION — SINGLE SOURCE OF TRUTH
// =============================================================================
void SwapchainManager::createOrRecreateSwapchain(uint32_t w, uint32_t h, bool isRecreate) noexcept
{
    VkDevice device = stone_device();
    VkPhysicalDevice phys = stone_physical();
    VkSurfaceKHR surface = stone_surface();

    if (device == VK_NULL_HANDLE || phys == VK_NULL_HANDLE || surface == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("SWAPCHAIN", "Device, physical device, or surface not ready");
        return;
    }

    if (w == 0 || h == 0) {
        minimized_ = true;
        LOG_AMOURANTH("WINDOW MINIMIZED — PHOTONS PAUSED — SACRED PINK LIGHT ENGAGED");
        VulkanRenderer::get()->forcePinkFallbackClear();
        return;
    }
    minimized_ = false;

    vkDeviceWaitIdle(device);

    if (isRecreate) {
        LOG_AMOURANTH("SWAPCHAIN RECREATION — {}×{} — EMPIRE REFORGED", w, h);
        cleanupImageViews();
        cleanupSwapchain();

        // Notify LAS to clear TLAS and prepare for rebuild
        RTX::las().onResize();
    }

    // === SURFACE CAPABILITIES ===
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps));

    // === FORMATS ===
    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, formats.data()));

    // === PRESENT MODES ===
    uint32_t modeCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, nullptr));
    std::vector<VkPresentModeKHR> modes(modeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, modes.data()));

    // === EXTENT — HIGH-DPI AWARE ===
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // === PRESENT MODE — BEST AVAILABLE ===
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // Always available

    auto hasMode = [&modes](VkPresentModeKHR m) {
        return std::find(modes.begin(), modes.end(), m) != modes.end();
    };

    if (hasMode(VK_PRESENT_MODE_MAILBOX_KHR)) {
        presentMode = VK_PRESENT_MODE_MAILBOX_KHR;           // Ideal: low latency + tear-free
    } else if (hasMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)) {
        presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;      // Adaptive VSync
    } else if (hasMode(VK_PRESENT_MODE_IMMEDIATE_KHR)) {
        presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;        // Max FPS (may tear)
    }

    // === IMAGE COUNT — PREFER TRIPLE BUFFERING ===
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) {
        imageCount = std::min(imageCount, caps.maxImageCount);
    }

    // === FORMAT — HDR PRIORITY (scRGB FP16 first) ===
    VkSurfaceFormatKHR chosenFormat = formats[0]; // Safe fallback

    struct Candidate {
        VkFormat format;
        VkColorSpaceKHR space;
        const char* name;
        bool hdr;
    };

    const Candidate candidates[] = {
        { VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, "scRGB FP16 (Best HDR)", true },
        { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT, "HDR10 10-bit", true },
        { VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_HDR10_ST2084_EXT, "FP16 PQ HDR", true },
        { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, "8-bit sRGB", false }
    };

    bool hdrEnabled = false;
    for (const auto& cand : candidates) {
        auto it = std::find_if(formats.begin(), formats.end(),
            [&](const VkSurfaceFormatKHR& f) { return f.format == cand.format && f.colorSpace == cand.space; });
        if (it != formats.end()) {
            chosenFormat = *it;
            if (cand.hdr) {
                LOG_AMOURANTH("HDR SWAPCHAIN FORGED — {} — PINK PHOTONS BLOOM IN FULL DYNAMIC RANGE", cand.name);
                hdrEnabled = true;
            }
            break;
        }
    }

    if (!hdrEnabled) {
        LOG_INFO_CAT("SWAPCHAIN", "HDR not available — falling back to 8-bit sRGB");
    }

    // === CREATE SWAPCHAIN — VALIDATION-SAFE USAGE FLAGS ===
    VkSwapchainCreateInfoKHR createInfo{
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = surface,
        .minImageCount    = imageCount,
        .imageFormat      = chosenFormat.format,
        .imageColorSpace  = chosenFormat.colorSpace,
        .imageExtent      = extent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform     = caps.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = presentMode,
        .clipped          = VK_TRUE,
        .oldSwapchain     = swapchain_.valid() ? swapchain_.get() : VK_NULL_HANDLE
    };

    VkSwapchainKHR oldSwapchain = swapchain_.get();
    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &newSwapchain);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("SWAPCHAIN", "Failed to create swapchain: {}", string_VkResult(result));
        return;
    }

    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
    }

    swapchain_ = Handle<VkSwapchainKHR>(newSwapchain, device);
    swapchainExtent_ = extent;
    swapchainFormat_ = chosenFormat.format;
    currentPresentMode_ = presentMode;

    // === RETRIEVE SWAPCHAIN IMAGES ===
    uint32_t retrievedCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, newSwapchain, &retrievedCount, nullptr));
    swapchainImages_.resize(retrievedCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, newSwapchain, &retrievedCount, swapchainImages_.data()));

    // === CREATE IMAGE VIEWS ===
    swapchainImageViews_.resize(retrievedCount);
    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = chosenFormat.format,
        .components       = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                              VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    for (uint32_t i = 0; i < retrievedCount; ++i) {
        viewInfo.image = swapchainImages_[i];
        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews_[i]));
    }

    // === SEAL INTO STONEKEY ===
    stone_seal_swapchain(newSwapchain);
    stone_seal_extent(extent);
    stone_seal_image_count(retrievedCount);
    stone_seal_images(swapchainImages_);
    stone_seal_views(swapchainImageViews_);

    // === FINAL LOGGING ===
    const char* modeName =
        presentMode == VK_PRESENT_MODE_MAILBOX_KHR       ? "MAILBOX (Low Latency)" :
        presentMode == VK_PRESENT_MODE_FIFO_KHR          ? "FIFO (VSync)" :
        presentMode == VK_PRESENT_MODE_FIFO_RELAXED_KHR  ? "FIFO_RELAXED (Adaptive)" :
        presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR     ? "IMMEDIATE (Max FPS)" :
        "UNKNOWN";

    LOG_AMOURANTH("[2026] SWAPCHAIN {} — {}×{} — {} images — {} — {} — PLASTIC BEACH ETERNAL",
                  isRecreate ? "RECREATED" : "FORGED",
                  extent.width, extent.height, retrievedCount, modeName,
                  hdrEnabled ? "HDR (Full Glory)" : "8-bit sRGB");
}

// =============================================================================
// Public Interface
// =============================================================================
void SwapchainManager::create(SDL_Window* window, uint32_t w, uint32_t h) noexcept
{
    createOrRecreateSwapchain(w, h, false);
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    createOrRecreateSwapchain(w, h, true);
}

void SwapchainManager::cleanup() noexcept
{
    if (stone_device() == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(stone_device());

    RTX::las().onResize();  // Ensure LAS clears TLAS before swapchain destruction
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

VkResult SwapchainManager::acquireNextImage(uint32_t* pImageIndex, VkSemaphore semaphore, VkFence fence) noexcept
{
    if (minimized_) {
        VulkanRenderer::get()->forcePinkFallbackClear();
        return VK_NOT_READY;
    }

    VkResult result = vkAcquireNextImageKHR(stone_device(), stone_swapchain(), UINT64_MAX, semaphore, fence, pImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate(stone_width(), stone_height());
    } else if (result != VK_SUCCESS) {
        LOG_AMOURANTH("Image acquire failed — sacred pink light sustains us");
        VulkanRenderer::get()->forcePinkFallbackClear();
    }

    return result;
}

void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) noexcept
{
    if (minimized_) {
        VulkanRenderer::get()->forcePinkFallbackClear();
        return;
    }

    VkPresentInfoKHR presentInfo{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = waitSemaphore ? 1u : 0u,
        .pWaitSemaphores    = waitSemaphore ? &waitSemaphore : nullptr,
        .swapchainCount     = 1,
        .pSwapchains        = &stone_swapchain(),
        .pImageIndices      = &imageIndex
    };

    VkResult result = vkQueuePresentKHR(queue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate(stone_width(), stone_height());
    } else if (result != VK_SUCCESS) {
        LOG_AMOURANTH("Present failed — pink light endures");
        VulkanRenderer::get()->forcePinkFallbackClear();
    }
}

} // namespace RTX

// =============================================================================
// JANUARY 05, 2026 — FINAL PRODUCTION SWAPCHAIN v5.3
// Singleton | HDR first | Safe flags | LAS-aware | Minimized safe | Validation perfect
// The empire is complete — photons flow without error
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================