// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.70
// SWAPCHAIN MANAGER — HDR | SELF-HEALING | DEFERRED RECREATE | DIRECT STORAGE ATTEMPT (QUERY FIRST)
// JANUARY 24, 2026 — "only violate a few laws" edition
// - Queries support for STORAGE_BIT on swapchain images BEFORE creation
// - Attempts direct write (STORAGE_BIT) only if viable
// - No blind fallback recreation — caller must handle !directWriteEnabled
// - Logs chosen path clearly + why it failed if unsupported
// - Simplified transitions for direct storage path
// - Fixed: Explicit PRESENT_SRC_KHR transition before every present
// - No more VK_IMAGE_LAYOUT_UNDEFINED on present — always transitioned
// - Fixed: Use transient command pool for one-time transitions
// - No external getOneTimeCommandBuffer — internal transient cmd creation
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/Extensions.hpp"

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
using StoneKey::stone_transient_pool;

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
        LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain ready — {} images | {}×{} | Direct storage write: {}",
                        swapchainImages_.size(), w, h, directWriteEnabled ? "ENABLED (STORAGE_BIT)" : "DISABLED — use offscreen target");
    }
}

// =============================================================================
// transitionImageLayout — focused on present path
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

    // Critical: always transition to PRESENT_SRC_KHR before present
    if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED || oldLayout == VK_IMAGE_LAYOUT_GENERAL ||
            oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            srcAccess = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            dstAccess = VK_ACCESS_MEMORY_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
    } else if (newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        srcAccess = VK_ACCESS_MEMORY_READ_BIT;
        dstAccess = VK_ACCESS_SHADER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    } else {
        return;  // unsupported transition
    }

    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// =============================================================================
// createOrRecreateSwapchain — query-first direct storage attempt
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

    // Query present modes
    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &pmCount, presentModes.data());

    VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR;
    const VkPresentModeKHR pmPrefs[] = {
        VK_PRESENT_MODE_MAILBOX_KHR,
        VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_KHR
    };

    for (auto pref : pmPrefs) {
        if (std::find(presentModes.begin(), presentModes.end(), pref) != presentModes.end()) {
            chosenPM = pref;
            break;
        }
    }

    LOG_INFO_CAT("SWAPCHAIN", "Chosen present mode: {}", 
                 chosenPM == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" :
                 chosenPM == VK_PRESENT_MODE_FIFO_RELAXED_KHR ? "FIFO_RELAXED" :
                 chosenPM == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" :
                 "FIFO");

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imgCount = std::min(imgCount, caps.maxImageCount);

    // Decide usage: try direct storage only if supported
    directWriteEnabled = false;

    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    bool storageSupportedBySurface = (caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) != 0;

    if (storageSupportedBySurface) {
        VkImageFormatProperties fmtProps{};
        VkResult propsRes = vkGetPhysicalDeviceImageFormatProperties(
            phys,
            chosenFormat.format,
            VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            0,
            &fmtProps);

        if (propsRes == VK_SUCCESS) {
            directWriteEnabled = true;
            imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
            LOG_SUCCESS_CAT("SWAPCHAIN", "Direct storage write SUPPORTED — attempting zero-copy path (STORAGE_BIT enabled)");
        } else {
            LOG_WARN_CAT("SWAPCHAIN", "STORAGE_BIT unsupported for format {} in optimal tiling ({}), falling back to standard path",
                         string_VkFormat(chosenFormat.format), string_VkResult(propsRes));
        }
    } else {
        LOG_WARN_CAT("SWAPCHAIN", "Surface does NOT support VK_IMAGE_USAGE_STORAGE_BIT — direct write impossible");
    }

    // Always add TRANSFER_DST for potential future blit/tonemap safety
    imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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
    ci.imageUsage       = imageUsage;

    VkSwapchainKHR newSwap = VK_NULL_HANDLE;
    VkResult res = vkCreateSwapchainKHR(dev, &ci, nullptr, &newSwap);

    if (res != VK_SUCCESS) {
        minimized_ = true;
        LOG_FATAL_CAT("SWAPCHAIN", "vkCreateSwapchainKHR failed: {} (even without forcing extra usage)", string_VkResult(res));
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

    LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain created — {} images | {}×{} | Direct storage write: {}",
                    count, extent.width, extent.height, directWriteEnabled ? "YES (STORAGE_BIT)" : "NO — renderer must use offscreen HDR target");
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
// presentImage — always ensure image in PRESENT_SRC_KHR layout before present
// =============================================================================
VkResult SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSem) noexcept {
    if (minimized_ || !swapchain_.valid()) return VK_ERROR_INITIALIZATION_FAILED;

    VkSwapchainKHR currentSwap = stone_swapchain();

    VkImage currentImage = swapchainImages_[imageIndex];

    // Lazy-create transient command pool if not exists
    VkCommandPool transientPool = stone_transient_pool();
    if (transientPool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo pci{};
        pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = StoneKey::stone_graphics_family();

        VkResult res = vkCreateCommandPool(stone_device(), &pci, nullptr, &transientPool);
        if (res != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "Failed to lazy-create transient pool for present transition: {}", string_VkResult(res));
            // Continue — present may work if image already in correct layout
        } else {
            StoneKey::stone_seal_transient_pool(transientPool);  // Seal if created
            LOG_INFO_CAT("SWAPCHAIN", "Lazy-created transient command pool for present transition");
        }
    }

    // One-time cmd buffer for transition
    VkCommandBuffer cmd = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = transientPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkResult res = vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("SWAPCHAIN", "Failed to allocate transient cmd for present transition: {}", string_VkResult(res));
        // Continue anyway
    } else {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &beginInfo);

        // Transition to PRESENT_SRC_KHR
        transitionImageLayout(cmd, currentImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(stone_device(), transientPool, 1, &cmd);
    }

    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = waitSem ? 1 : 0;
    pi.pWaitSemaphores    = waitSem ? &waitSem : nullptr;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &currentSwap;
    pi.pImageIndices      = &imageIndex;

    VkResult presentRes = vkQueuePresentKHR(queue, &pi);

    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR || presentRes == VK_ERROR_SURFACE_LOST_KHR) {
        LOG_WARN_CAT("SWAPCHAIN", "Present detected invalid swapchain — flag recreate next frame");
    } else if (presentRes != VK_SUCCESS) {
        LOG_ERROR_CAT("SWAPCHAIN", "vkQueuePresentKHR failed: {}", string_VkResult(presentRes));
    }

    return presentRes;
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
// SwapchainManager v30.70 — JANUARY 24, 2026
// - Query-first STORAGE_BIT attempt for direct ray tracing writes
// - No auto-fallback creation — respect driver reality
// - Zero-copy when it works (rare), offscreen required otherwise
// - Tonemap your raw ass later
// - Fixed: Explicit PRESENT_SRC_KHR transition before every present
// - No more VK_IMAGE_LAYOUT_UNDEFINED on present
// - Fixed: Use transient command pool for one-time transitions
// - No external getOneTimeCommandBuffer — internal transient cmd creation
// Celebrated 420 lines. Joe Rogan and Elon Musk interview says I can smoke in public if it is legal. You saw it.
// =============================================================================