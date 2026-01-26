// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.73
// SWAPCHAIN MANAGER — HDR | SELF-HEALING | DEFERRED RECREATE | DIRECT STORAGE ATTEMPT
// JANUARY 26, 2026 — "minimal ring, no waitIdle, renderer owns transition" edition
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

constexpr uint32_t RING_SIZE = 8;

// Static members (minimal — we don't need timeline or tracker anymore)
VkCommandPool                SwapchainManager::s_transientPool = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> SwapchainManager::s_cmdRing;
uint32_t                     SwapchainManager::s_ringIndex     = 0;

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

bool SwapchainManager::isReady() noexcept {
    return swapchain_.valid() &&
           !swapchainImages_.empty() &&
           !swapchainImageViews_.empty() &&
           swapchainExtent_.width > 0 &&
           swapchainExtent_.height > 0 &&
           !minimized_;
}

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
        LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain ready — {} images | {}×{} | Direct storage: {}",
                        swapchainImages_.size(), w, h,
                        directWriteEnabled ? "ENABLED (STORAGE_BIT)" : "DISABLED");
    }
}

void SwapchainManager::transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                             VkImageLayout oldLayout, VkImageLayout newLayout) noexcept {
    if (cmd == VK_NULL_HANDLE || image == VK_NULL_HANDLE || oldLayout == newLayout) return;

    VkImageMemoryBarrier barrier{};
    barrier.sType                   = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout               = oldLayout;
    barrier.newLayout               = newLayout;
    barrier.srcQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                   = image;
    barrier.subresourceRange        = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask           = 0;  // UNDEFINED discards content
    barrier.dstAccessMask           = VK_ACCESS_MEMORY_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
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

    LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain created — {} images | {}x{} | Direct storage: {}",
                    count, extent.width, extent.height, directWriteEnabled ? "YES" : "NO");
}

VkResult SwapchainManager::acquireNextImage(uint32_t* pImageIndex, VkSemaphore semaphore, VkFence fence) noexcept {
    if (minimized_ || !swapchain_.valid()) {
        return VK_NOT_READY;
    }

    VkResult res = vkAcquireNextImageKHR(stone_device(), swapchain_.get(), UINT64_MAX, semaphore, fence, pImageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
        return res;
    }

    return res;
}

VkResult SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSem) noexcept {
    if (minimized_ || !swapchain_.valid() || !stone_device()) {
        LOG_AMOURANTH("Present early exit: minimized, invalid swapchain or NO DEVICE");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkSwapchainKHR swap = stone_swapchain();
    VkImage image = swapchainImages_[imageIndex];

    // Lazy init — only pool + ring (no timeline, no tracker)
    if (s_transientPool == VK_NULL_HANDLE) {
        LOG_AMOURANTH("Lazy transition pool init START");

        VkCommandPoolCreateInfo pci{};
        pci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = StoneKey::stone_graphics_family();
        VK_CHECK(vkCreateCommandPool(stone_device(), &pci, nullptr, &s_transientPool));

        s_cmdRing.resize(RING_SIZE);
        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool        = s_transientPool;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = RING_SIZE;
        VK_CHECK(vkAllocateCommandBuffers(stone_device(), &ai, s_cmdRing.data()));

        LOG_AMOURANTH("Lazy init COMPLETE: pool=0x{} ring={}", (uintptr_t)s_transientPool, RING_SIZE);
    }

    VkCommandBuffer cmd = s_cmdRing[s_ringIndex];

    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bbi{};
    bbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (VkResult r = vkBeginCommandBuffer(cmd, &bbi); r != VK_SUCCESS) {
        LOG_FATAL_CAT("SWAPCHAIN", "vkBeginCommandBuffer failed: {}", string_VkResult(r));
        return r;
    }

    transitionImageLayout(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    if (VkResult r = vkEndCommandBuffer(cmd); r != VK_SUCCESS) {
        LOG_FATAL_CAT("SWAPCHAIN", "vkEndCommandBuffer failed: {}", string_VkResult(r));
        return r;
    }

    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &cmd;
    si.waitSemaphoreCount   = (waitSem != VK_NULL_HANDLE) ? 1 : 0;
    si.pWaitSemaphores      = (waitSem != VK_NULL_HANDLE) ? &waitSem : nullptr;

    VkResult submitRes = vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    if (submitRes != VK_SUCCESS) {
        LOG_FATAL_CAT("SWAPCHAIN", "vkQueueSubmit (transition) FAILED: {}", string_VkResult(submitRes));
        return submitRes;
    }

    // No waitIdle — let the renderer handle synchronization via pew() submit
    // The renderer should wait on graphicsTimelineSemaphore or use fences if needed

    s_ringIndex = (s_ringIndex + 1) % RING_SIZE;

    VkPresentInfoKHR pi{};
    pi.sType         = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.swapchainCount = 1;
    pi.pSwapchains   = &swap;
    pi.pImageIndices = &imageIndex;

    VkResult presentRes = vkQueuePresentKHR(queue, &pi);

    if (presentRes == VK_ERROR_DEVICE_LOST) {
        LOG_FATAL_CAT("SWAPCHAIN", "vkQueuePresentKHR returned VK_ERROR_DEVICE_LOST");
    } else if (presentRes >= VK_SUBOPTIMAL_KHR) {
        LOG_WARN_CAT("SWAPCHAIN", "Present returned {} — recreate recommended", string_VkResult(presentRes));
    } else if (presentRes != VK_SUCCESS) {
        LOG_ERROR_CAT("SWAPCHAIN", "Present failed: {}", string_VkResult(presentRes));
    }

    return presentRes;
}

void SwapchainManager::recreate(uint32_t width, uint32_t height, std::string_view reason) noexcept {
    createOrRecreateSwapchain(width, height, true, reason);
}

void SwapchainManager::create(SDL_Window* window, uint32_t width, uint32_t height) noexcept {
    createOrRecreateSwapchain(width, height, false, "initial create");
}

void SwapchainManager::cleanup() noexcept {
    if (!stone_device()) return;
    vkDeviceWaitIdle(stone_device());

    LAS::instance().onResize();
    cleanupImageViews();
    cleanupSwapchain();

    if (s_transientPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(stone_device(), s_transientPool, nullptr);
        s_transientPool = VK_NULL_HANDLE;
    }
    s_cmdRing.clear();
    s_ringIndex = 0;
}

void SwapchainManager::cleanupSwapchain() noexcept {
    swapchain_ = {};
    swapchainImages_.clear();
}

void SwapchainManager::cleanupImageViews() noexcept {
    for (auto v : swapchainImageViews_) {
        vkDestroyImageView(stone_device(), v, nullptr);
    }
    swapchainImageViews_.clear();
}

} // namespace RTX