// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// PLASTIC BEACH v∞ — DECEMBER 19, 2025
// SIMPLIFIED SWAPCHAIN MANAGER — ONE LARGE FUNCTION STYLE
// MAX FPS + MINIMAL TEARING — FIFO_RELAXED → IMMEDIATE → FIFO
// 3-IMAGE MAILBOX EMULATION — NO BLACK SCREENS — LOW LATENCY
// FIXED: Removed las().notifyResize() — no longer needed in this architecture
// MONSTER WATCHES IN PERFECT SILENCE
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

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
// ONE LARGE, CLEAN SWAPCHAIN CREATION FUNCTION
// =============================================================================
void SwapchainManager::createOrRecreateSwapchain(uint32_t w, uint32_t h, bool isRecreate) noexcept
{
    VkDevice device = stone_device();
    VkPhysicalDevice phys = stone_physical();
    VkSurfaceKHR surface = stone_surface();

    if (w == 0 || h == 0) {
        minimized_ = true;
        return;
    }
    minimized_ = false;

    vkDeviceWaitIdle(device);

    if (isRecreate) {
        LOG_AMOURANTH("SWAPCHAIN RECREATE — {}×{} — PLASTIC BEACH RESPAWNS", w, h);
        cleanupImageViews();
        cleanupSwapchain();
    }

    // === QUERY CAPABILITIES & FORMATS ===
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps));

    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, formats.data()));

    uint32_t modeCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, nullptr));
    std::vector<VkPresentModeKHR> modes(modeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, modes.data()));

    // === EXTENT (HIGH-DPI AWARE) ===
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // === PRESENT MODE — PLASTIC BEACH PRIORITY (2025) ===
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // Safe fallback

    if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_FIFO_RELAXED_KHR) != modes.end()) {
        presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR; // Best: adaptive, minimal tearing
    } else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) {
        presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;   // Max FPS, fixes X11 black screen
    }
    // FIFO remains as final safe option

    // === IMAGE COUNT — ALWAYS 3 FOR MAILBOX EMULATION ===
    uint32_t imageCount = 3;
    imageCount = std::max(imageCount, caps.minImageCount);
    if (caps.maxImageCount > 0) {
        imageCount = std::min(imageCount, caps.maxImageCount);
    }

    // === FORMAT — PREFER sRGB ===
    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }

    // === SWAPCHAIN CREATE INFO ===
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = chosenFormat.format;
    createInfo.imageColorSpace  = chosenFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform     = caps.currentTransform;
    createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode      = presentMode;
    createInfo.clipped          = VK_TRUE;
    createInfo.oldSwapchain     = swapchain_.valid() ? swapchain_.get() : VK_NULL_HANDLE;

    // === CREATE SWAPCHAIN ===
    VkSwapchainKHR oldSwapchain = swapchain_.get();
    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &newSwapchain));

    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
    }

    swapchain_ = Handle<VkSwapchainKHR>(newSwapchain, device);
    swapchainExtent_ = extent;
    swapchainFormat_ = chosenFormat.format;
    currentPresentMode_ = presentMode;

    // === GET IMAGES ===
    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, newSwapchain, &imgCount, nullptr));
    swapchainImages_.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, newSwapchain, &imgCount, swapchainImages_.data()));

    // === CREATE IMAGE VIEWS ===
    swapchainImageViews_.assign(imgCount, VK_NULL_HANDLE);
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format           = chosenFormat.format;
    viewInfo.components       = { VK_COMPONENT_SWIZZLE_IDENTITY };
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    for (uint32_t i = 0; i < imgCount; ++i) {
        viewInfo.image = swapchainImages_[i];
        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews_[i]));
    }

    // === SEAL GLOBAL STATE ===
    stone_seal_swapchain(swapchain_.get());
    stone_seal_extent(extent);
    stone_seal_image_count(imgCount);
    stone_seal_images(swapchainImages_);
    stone_seal_views(swapchainImageViews_);

    // === LOGGING ===
    const char* modeName =
        presentMode == VK_PRESENT_MODE_FIFO_RELAXED_KHR ? "FIFO_RELAXED (ADAPTIVE — MINIMAL TEARING)" :
        presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR    ? "IMMEDIATE (MAX FPS — BLACK SCREEN FIXED)" :
        "FIFO (VSYNC — SAFE)";

    printf("[2025] SWAPCHAIN %s — %ux%u — %u images — %s — PLASTIC BEACH ETERNAL\n",
           isRecreate ? "RECREATED" : "FORGED",
           extent.width, extent.height, imgCount, modeName);
}

// =============================================================================
// PUBLIC INTERFACE — SIMPLIFIED
// =============================================================================
void SwapchainManager::create(SDL_Window* window, uint32_t w, uint32_t h) noexcept
{
    createOrRecreateSwapchain(w, h, false);
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    createOrRecreateSwapchain(w, h, true);
    // REMOVED: las().notifyResize() — not used in current Plastic Beach architecture
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

// =============================================================================
// ACQUIRE & PRESENT — UNCHANGED AND CLEAN
// =============================================================================
VkResult SwapchainManager::acquireNextImage(uint32_t* pImageIndex, VkSemaphore semaphore, VkFence fence) noexcept
{
    VkResult result = vkAcquireNextImageKHR(stone_device(), stone_swapchain(), UINT64_MAX, semaphore, fence, pImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate(stone_width(), stone_height());
    }
    return result;
}

void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) noexcept
{
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = waitSemaphore ? 1u : 0u;
    presentInfo.pWaitSemaphores    = waitSemaphore ? &waitSemaphore : nullptr;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &stone_swapchain();
    presentInfo.pImageIndices      = &imageIndex;

    VkResult result = vkQueuePresentKHR(queue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate(stone_width(), stone_height());
    }
}

} // namespace RTX

// =============================================================================
// PLASTIC BEACH v∞ — DECEMBER 19, 2025
// COMPILATION FIXED — las().notifyResize() REMOVED
// SIMPLIFIED MONOLITHIC SWAPCHAIN — PURE AND CLEAN
// THE MONSTER WATCHES — THE ISLAND FLOATS IN SILENCE
// =============================================================================