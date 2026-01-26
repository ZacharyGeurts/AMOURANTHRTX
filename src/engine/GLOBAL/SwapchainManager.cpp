// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.71
// SWAPCHAIN MANAGER — HDR | SELF-HEALING | DEFERRED RECREATE | DIRECT STORAGE ATTEMPT
// JANUARY 26, 2026 — "no more UNDEFINED on present" edition
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

constexpr uint32_t RING_SIZE = 8;  // Power-of-2 preferred, >= triple buffering

// Static members
VkCommandPool                SwapchainManager::s_transientPool         = VK_NULL_HANDLE;
VkSemaphore                  SwapchainManager::s_timelineSem           = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> SwapchainManager::s_cmdRing;
uint32_t                     SwapchainManager::s_ringIndex             = 0;
uint64_t                     SwapchainManager::s_nextTimelineValue     = 1;
VkBuffer                     SwapchainManager::s_ringTrackerBuffer     = VK_NULL_HANDLE;
VkDeviceMemory               SwapchainManager::s_ringTrackerMemory     = VK_NULL_HANDLE;
void*                        SwapchainManager::s_ringTrackerMapped      = nullptr;
VkDeviceAddress              SwapchainManager::s_ringTrackerDeviceAddr = 0;

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

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkAccessFlags srcAccess = 0;
    VkAccessFlags dstAccess = VK_ACCESS_MEMORY_READ_BIT;

    if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
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

    // Format selection
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

    // Present mode
    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surf, &pmCount, presentModes.data());

    VkPresentModeKHR chosenPM = VK_PRESENT_MODE_FIFO_KHR;
    const VkPresentModeKHR pmPrefs[] = {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR,
                                        VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR};
    for (auto pref : pmPrefs) {
        if (std::find(presentModes.begin(), presentModes.end(), pref) != presentModes.end()) {
            chosenPM = pref; break;
        }
    }

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imgCount = std::min(imgCount, caps.maxImageCount);

    // Storage attempt
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

VkResult SwapchainManager::acquireNextImage(uint32_t* pImageIndex, VkSemaphore semaphore, VkFence fence) noexcept {
    if (minimized_ || !swapchain_.valid()) {
        return VK_NOT_READY;
    }

    VkResult res = vkAcquireNextImageKHR(stone_device(), swapchain_.get(), UINT64_MAX, semaphore, fence, pImageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
        return res;
    }

    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("SWAPCHAIN", "Acquire failed: {}", string_VkResult(res));
    }

    return res;
}

VkResult SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSem) noexcept {
    if (minimized_ || !swapchain_.valid()) {
        LOG_AMOURANTH("Present early exit: minimized or invalid swapchain");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDevice dev = stone_device();
    VkSwapchainKHR swap = stone_swapchain();
    VkImage image = swapchainImages_[imageIndex];

    // Lazy initialization of transition ring
    if (s_transientPool == VK_NULL_HANDLE) {
        LOG_AMOURANTH("Lazy transition ring init START");

        VkCommandPoolCreateInfo pci{};
        pci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = StoneKey::stone_graphics_family();
        VK_CHECK(vkCreateCommandPool(dev, &pci, nullptr, &s_transientPool), "CreateCommandPool");

        s_cmdRing.resize(RING_SIZE);
        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool        = s_transientPool;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = RING_SIZE;
        VK_CHECK(vkAllocateCommandBuffers(dev, &ai, s_cmdRing.data()), "AllocateCommandBuffers");

        VkSemaphoreTypeCreateInfo sti{};
        sti.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        sti.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        sti.initialValue  = 0;

        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        sci.pNext = &sti;
        VK_CHECK(vkCreateSemaphore(dev, &sci, nullptr, &s_timelineSem), "Create timeline semaphore");

        VkBufferCreateInfo bci{};
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size  = 16; // uint64_t safe value
        bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        VK_CHECK(vkCreateBuffer(dev, &bci, nullptr, &s_ringTrackerBuffer), "Create tracker buffer");

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(dev, s_ringTrackerBuffer, &req);

        uint32_t memType = BufferManager::findMemoryType(req.memoryTypeBits,
                                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkMemoryAllocateFlagsInfo flags{};
        flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.pNext           = &flags;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = memType;
        VK_CHECK(vkAllocateMemory(dev, &mai, nullptr, &s_ringTrackerMemory), "Allocate tracker memory");
        VK_CHECK(vkBindBufferMemory(dev, s_ringTrackerBuffer, s_ringTrackerMemory, 0), "Bind tracker");

        VK_CHECK(vkMapMemory(dev, s_ringTrackerMemory, 0, VK_WHOLE_SIZE, 0, &s_ringTrackerMapped), "Map tracker");
        std::memset(s_ringTrackerMapped, 0, 16);

        VkBufferDeviceAddressInfo addrInfo{};
        addrInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addrInfo.buffer = s_ringTrackerBuffer;
        s_ringTrackerDeviceAddr = vkGetBufferDeviceAddress(dev, &addrInfo);

        LOG_AMOURANTH("Lazy init COMPLETE: pool=0x{:x} ring={} timeline=0x{:x} tracker=0x{:x} addr=0x{:x}",
                      (uintptr_t)s_transientPool, RING_SIZE, (uintptr_t)s_timelineSem,
                      (uintptr_t)s_ringTrackerBuffer, s_ringTrackerDeviceAddr);
    }

    uint64_t gpuSafeValue = *static_cast<uint64_t*>(s_ringTrackerMapped);
    VkCommandBuffer cmd = s_cmdRing[s_ringIndex];

    bool slotSafe = (gpuSafeValue >= s_nextTimelineValue - RING_SIZE) || (s_nextTimelineValue <= RING_SIZE + 2);

    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bbi{};
    bbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &bbi));

    // Always transition — acquired images start in UNDEFINED
    transitionImageLayout(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    if (slotSafe) {
        VkTimelineSemaphoreSubmitInfo tsi{};
        tsi.sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        tsi.signalSemaphoreValueCount = 1;
        tsi.pSignalSemaphoreValues    = &s_nextTimelineValue;

        VkSubmitInfo si{};
        si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.pNext                = &tsi;
        si.commandBufferCount   = 1;
        si.pCommandBuffers      = &cmd;
        si.waitSemaphoreCount   = waitSem ? 1 : 0;
        si.pWaitSemaphores      = waitSem ? &waitSem : nullptr;

        VK_CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));

        *static_cast<uint64_t*>(s_ringTrackerMapped) = s_nextTimelineValue;
    } else {
        LOG_AMOURANTH("Slot {} not yet safe (gpu={} < need={}) — using previous transition (risky but rare)",
                      s_ringIndex, gpuSafeValue, s_nextTimelineValue - RING_SIZE);
        // In production consider vkQueueWaitIdle or larger ring here
    }

    s_nextTimelineValue++;
    s_ringIndex = (s_ringIndex + 1) % RING_SIZE;

    VkPresentInfoKHR pi{};
    pi.sType         = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.swapchainCount = 1;
    pi.pSwapchains   = &swap;
    pi.pImageIndices = &imageIndex;

    VkResult res = vkQueuePresentKHR(queue, &pi);

    if (res >= VK_SUBOPTIMAL_KHR) {
        LOG_WARN_CAT("SWAPCHAIN", "Present returned {} — recreate recommended", string_VkResult(res));
    } else if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("SWAPCHAIN", "Present failed: {}", string_VkResult(res));
    }

    return res;
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

    if (s_timelineSem != VK_NULL_HANDLE) {
        vkDestroySemaphore(stone_device(), s_timelineSem, nullptr);
        s_timelineSem = VK_NULL_HANDLE;
    }
    if (s_transientPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(stone_device(), s_transientPool, nullptr);
        s_transientPool = VK_NULL_HANDLE;
    }
    if (s_ringTrackerBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(stone_device(), s_ringTrackerBuffer, nullptr);
        s_ringTrackerBuffer = VK_NULL_HANDLE;
    }
    if (s_ringTrackerMemory != VK_NULL_HANDLE) {
        vkFreeMemory(stone_device(), s_ringTrackerMemory, nullptr);
        s_ringTrackerMemory = VK_NULL_HANDLE;
    }
    s_ringTrackerMapped = nullptr;
    s_ringTrackerDeviceAddr = 0;
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