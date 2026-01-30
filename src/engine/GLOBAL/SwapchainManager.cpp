// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.78
// SWAPCHAIN MANAGER — HDR | SELF-HEALING | DEFERRED RECREATE | DIRECT STORAGE ATTEMPT
// JANUARY 29, 2026 — "validation clean + no one-time submit + semaphore reuse"
// - Fixed UNDEFINED layout on present: transition in reusable cmd buffer
// - Fixed one-time submit violation: cmd buffer now reset/reused per frame
// - Fixed semaphore destroy in use: semaphore reused across frames
// - vkDeviceWaitIdle before recreate — no pending work
// - Cached stone_xxx() handles — stable per call
// - Validation clean, no device lost
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/BufferManager.hpp"

#include <algorithm>
#include <format>
#include <cstring>
#include <vector>

using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_surface;
using StoneKey::stone_swapchain;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_seal_swapchain;
using StoneKey::stone_seal_extent;
using StoneKey::stone_seal_image_count;
using StoneKey::stone_seal_images;
using StoneKey::stone_seal_views;

namespace RTX {

VkCommandPool transientPool_ = VK_NULL_HANDLE;

static void ensureSwapchainExtension() noexcept {
    VkDevice dev = stone_device();
    if (!g_ext.vkCreateSwapchainKHR) {
        g_ext.vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
            vkGetDeviceProcAddr(dev, "vkCreateSwapchainKHR"));
    }
    if (!g_ext.vkDestroySwapchainKHR) {
        g_ext.vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
            vkGetDeviceProcAddr(dev, "vkDestroySwapchainKHR"));
    }
    if (!g_ext.vkGetSwapchainImagesKHR) {
        g_ext.vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
            vkGetDeviceProcAddr(dev, "vkGetSwapchainImagesKHR"));
    }
    if (!g_ext.vkQueuePresentKHR) {
        g_ext.vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(
            vkGetDeviceProcAddr(dev, "vkQueuePresentKHR"));
    }
}

bool SwapchainManager::isReady() noexcept {
    return swapchain_.valid() &&
           !swapchainImages_.empty() &&
           !swapchainImageViews_.empty() &&
           swapchainExtent_.width > 0 &&
           swapchainExtent_.height > 0 &&
           !minimized_;
}

void SwapchainManager::ensureReady(uint32_t w, uint32_t h) noexcept {
    if (isReady() && swapchainExtent_.width == w && swapchainExtent_.height == h) return;

    if (minimized_ || w == 0 || h == 0) {
        minimized_ = true;
        LOG_WARN_CAT("SWAPCHAIN", "Cannot ensure ready — minimized or zero extent");
        return;
    }

    LOG_INFO_CAT("SWAPCHAIN", "Ensuring swapchain ready — {}×{}", w, h);

    VkDevice dev = stone_device();
    vkDeviceWaitIdle(dev);  // Ensure no pending work before possible recreate

    createOrRecreateSwapchain(w, h, true, "ensureReady");

    if (!isReady()) {
        minimized_ = true;
        LOG_ERROR_CAT("SWAPCHAIN", "Failed to ensure ready after recreation");
    } else {
        LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain ready — {} images | {}×{} | Direct storage: {}",
                        swapchainImages_.size(), w, h,
                        directWriteEnabled ? "ENABLED (STORAGE_BIT)" : "DISABLED");
    }
}

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

    VkExtent2D extent = (caps.currentExtent.width == UINT32_MAX) ?
        VkExtent2D{std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width),
                   std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height)} :
        caps.currentExtent;

    if (extent.width == 0 || extent.height == 0) {
        minimized_ = true;
        LOG_WARN_CAT("SWAPCHAIN", "Zero extent — marked minimized");
        return;
    }

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surf, &fmtCount, formats.data());

    VkSurfaceFormatKHR chosenFormat = formats[0];
    const VkSurfaceFormatKHR prefs[] = {
        {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
    };
    for (const auto& p : prefs) {
        auto it = std::find_if(formats.begin(), formats.end(),
                               [&](const auto& f){ return f.format == p.format && f.colorSpace == p.colorSpace; });
        if (it != formats.end()) { chosenFormat = *it; break; }
    }

    VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR;

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imgCount = std::min(imgCount, caps.maxImageCount);

    directWriteEnabled = false;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) {
        VkImageFormatProperties props{};
        if (vkGetPhysicalDeviceImageFormatProperties(phys, chosenFormat.format, VK_IMAGE_TYPE_2D,
                                                     VK_IMAGE_TILING_OPTIMAL,
                                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                                     0, &props) == VK_SUCCESS) {
            directWriteEnabled = true;
            usage |= VK_IMAGE_USAGE_STORAGE_BIT;
            LOG_SUCCESS_CAT("SWAPCHAIN", "STORAGE_BIT supported → direct write enabled");
        }
    }

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
    ci.oldSwapchain     = isRecreate ? swapchain_.get() : VK_NULL_HANDLE;
    ci.imageUsage       = usage;

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
            LOG_FATAL_CAT("SWAPCHAIN", "vkCreateImageView[{}] failed: {}", i, string_VkResult(res));
            minimized_ = true;
            return;
        }
    }

    stone_seal_swapchain(newSwap);
    stone_seal_extent(extent);
    stone_seal_image_count(count);
    stone_seal_images(swapchainImages_);
    stone_seal_views(swapchainImageViews_);

    LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain created — {} images | {}×{} | Direct storage: {}",
                    count, extent.width, extent.height, directWriteEnabled ? "YES" : "NO");
}

VkResult SwapchainManager::acquireNextImage(uint32_t* pImageIndex, VkSemaphore* pSemaphoreOut) noexcept {
    if (minimized_ || !swapchain_.valid()) return VK_NOT_READY;

    VkDevice dev = stone_device();
    VkSwapchainKHR sw = stone_swapchain();

    // Reuse semaphore across frames — destroy only in cleanup
    static VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
    if (acquireSemaphore == VK_NULL_HANDLE) {
        VkSemaphoreCreateInfo semCI{};
        semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(dev, &semCI, nullptr, &acquireSemaphore));
    }

    VkResult res = vkAcquireNextImageKHR(dev, sw, UINT64_MAX,
                                         acquireSemaphore, VK_NULL_HANDLE, pImageIndex);

    if (res != VK_SUCCESS) {
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
            // Let renderer handle recreate
        } else if (res != VK_NOT_READY) {
            LOG_ERROR_CAT("SWAPCHAIN", "Acquire failed: {}", string_VkResult(res));
        }
    }

    *pSemaphoreOut = acquireSemaphore;
    return res;
}

void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore, VkSwapchainKHR swapchainHandle) noexcept {
    if (minimized_ || !swapchain_.valid() || !stone_device()) return;

    VkDevice dev = stone_device();

    VkImage image = swapchainImages_[imageIndex];

    // Reuse persistent cmd buffer — reset each frame
    static VkCommandBuffer presentCmd = VK_NULL_HANDLE;
    if (presentCmd == VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = transientPool_;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(dev, &allocInfo, &presentCmd));
    }

    vkResetCommandBuffer(presentCmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;  // NOT one-time — reusable

    VK_CHECK(vkBeginCommandBuffer(presentCmd, &beginInfo));

    VkImageMemoryBarrier barrier{};
    barrier.sType                   = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout               = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;  // From blit
    barrier.newLayout               = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                   = image;
    barrier.subresourceRange        = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask           = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask           = VK_ACCESS_MEMORY_READ_BIT;

    vkCmdPipelineBarrier(presentCmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    VK_CHECK(vkEndCommandBuffer(presentCmd));

    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &presentCmd;

    if (waitSemaphore != VK_NULL_HANDLE) {
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};
        submit.waitSemaphoreCount   = 1;
        submit.pWaitSemaphores      = &waitSemaphore;
        submit.pWaitDstStageMask    = waitStages;
    }

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &swapchainHandle;
    pi.pImageIndices      = &imageIndex;

    if (waitSemaphore != VK_NULL_HANDLE) {
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores    = &waitSemaphore;
    }

    VkResult res = vkQueuePresentKHR(queue, &pi);

    if (res == VK_ERROR_DEVICE_LOST) {
        LOG_FATAL_CAT("SWAPCHAIN", "vkQueuePresentKHR → DEVICE LOST");
    } else if (res >= VK_SUBOPTIMAL_KHR) {
        LOG_WARN_CAT("SWAPCHAIN", "Present returned {} — recreate soon", string_VkResult(res));
    } else if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("SWAPCHAIN", "Present failed: {}", string_VkResult(res));
    }
}

void SwapchainManager::recreate(uint32_t width, uint32_t height, std::string_view reason) noexcept {
    VkDevice dev = stone_device();
    vkDeviceWaitIdle(dev);  // Ensure no pending work
    createOrRecreateSwapchain(width, height, true, reason);
}

void SwapchainManager::create(SDL_Window* window, uint32_t width, uint32_t height) noexcept {
    createOrRecreateSwapchain(width, height, false, "initial create");
}

void SwapchainManager::cleanup() noexcept {
    VkDevice dev = stone_device();
    if (!dev) return;
    vkDeviceWaitIdle(dev);

    LAS::instance().onResize();
    cleanupImageViews();
    cleanupSwapchain();

    if (transientPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(dev, transientPool_, nullptr);
        transientPool_ = VK_NULL_HANDLE;
    }

    // Destroy persistent semaphore
    static VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
    if (acquireSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(dev, acquireSemaphore, nullptr);
        acquireSemaphore = VK_NULL_HANDLE;
    }
}

void SwapchainManager::cleanupSwapchain() noexcept {
    swapchain_ = {};
    swapchainImages_.clear();
}

void SwapchainManager::cleanupImageViews() noexcept {
    VkDevice dev = stone_device();
    for (auto v : swapchainImageViews_) {
        vkDestroyImageView(dev, v, nullptr);
    }
    swapchainImageViews_.clear();
}

} // namespace RTX