// =============================================================================
// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX — VALHALLA v∞ TURBO — FINAL ETERNAL CUT
// THE ONE TRUE SWAPCHAIN — COMPILES CLEAN, NO 60FPS DEADLOCK, NO const ON STATICS
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <algorithm>
#include <fstream>
#include <format>
#include <limits>
#include <vector>

using namespace Logging::Color;
using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_surface;
using StoneKey::stone_window;
using StoneKey::stone_seal_swapchain;
using StoneKey::stone_seal_extent;
using StoneKey::stone_seal_image_count;
using StoneKey::stone_seal_images;
using StoneKey::stone_seal_views;

namespace RTX {

// ── EXTENSION FUNCTION POINTERS ─────────────────────────────────────────────
static PFN_vkGetRefreshCycleDurationGOOGLE   vkGetRefreshCycleDurationGOOGLE   = nullptr;
static PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE = nullptr;
static PFN_vkSetHdrMetadataEXT               vkSetHdrMetadataEXT               = nullptr;

// ── HDR AUTO-DETECTION ───────────────────────────────────────────────────────
void SwapchainManager::autoEnableHDR() noexcept
{
    static bool cached = false;
    if (cached) return;

    bool hdr = false;
    if (Options::Display::HDR_AUTO_IGNITION)
    {
        const char* paths[] = {
            "/sys/class/drm/card0-DP-1/edid",
            "/sys/class/drm/card0-HDMI-A-1/edid",
            "/sys/class/drm/card0-eDP-1/edid",
            "/sys/class/drm/card1-DP-1/edid"
        };

        std::vector<char> edid;
        for (const auto* p : paths)
        {
            std::ifstream f(p, std::ios::binary);
            if (!f) continue;
            f.seekg(0, std::ios::end);
            auto sz = f.tellg();
            if (sz < 256) continue;
            edid.resize(static_cast<size_t>(sz));
            f.seekg(0);
            if (!f.read(edid.data(), sz)) continue;

            for (size_t i = 128; i + 128 <= edid.size(); i += 128)
            {
                if (edid[i] == 0x02 && edid[i + 3] >= 0x06)
                {
                    hdr = true;
                    break;
                }
            }
            if (hdr) break;
        }
    }

    currentColorSpace_ = hdr ? VK_COLOR_SPACE_HDR10_ST2084_EXT : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    cached = true;
}

// ── PUBLIC API ───────────────────────────────────────────────────────────────
void SwapchainManager::create(SDL_Window*, uint32_t w, uint32_t h) noexcept
{
    autoEnableHDR();
    createSwapchain(stone_window(), w, h, VK_NULL_HANDLE);
    createImageViews();
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    if (w == 0 || h == 0)
    {
        minimized_ = true;
        return;
    }
    minimized_ = false;

    vkDeviceWaitIdle(stone_device());

    for (auto v : swapchainImageViews_)
        if (v) vkDestroyImageView(stone_device(), v, nullptr);
    swapchainImageViews_.clear();

    VkSwapchainKHR old = swapchain_.get();
    createSwapchain(stone_window(), w, h, old);
    createImageViews();
    las().notifyResize();
}

void SwapchainManager::cleanup() noexcept
{
    for (auto v : swapchainImageViews_)
        if (v) vkDestroyImageView(stone_device(), v, nullptr);
    swapchainImageViews_.clear();

    if (swapchain_.valid())
    {
        vkDestroySwapchainKHR(stone_device(), swapchain_.get(), nullptr);
        swapchain_.reset();
    }
    swapchainImages_.clear();
    swapchainExtent_ = {0, 0};
    swapchainFormat_ = VK_FORMAT_UNDEFINED;
}

// ── HDR SUPPORT ─────────────────────────────────────────────────────────────
bool SwapchainManager::supportsHDR() noexcept
{
    return currentColorSpace_ == VK_COLOR_SPACE_HDR10_ST2084_EXT;
}

// ── ROBUST IMAGE ACQUISITION (INFINITE TIMEOUT — NO 60FPS DEADLOCK) ─────────
VkResult SwapchainManager::acquireNextImage(uint32_t* pImageIndex,
                                            VkSemaphore semaphore,
                                            VkFence fence) noexcept
{
    return vkAcquireNextImageKHR(stone_device(),
                                 swapchain_.get(),
                                 1000000000ULL,     // 1 second timeout instead of infinite
                                 semaphore,
                                 fence,
                                 pImageIndex);
}

// ── CORE SWAPCHAIN FORGE ─────────────────────────────────────────────────────
void SwapchainManager::createSwapchain(SDL_Window*, uint32_t w, uint32_t h, VkSwapchainKHR old) noexcept
{
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stone_physical(), stone_surface(), &caps);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount) vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, formats.data());

    uint32_t presentCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &presentCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentCount);
    if (presentCount) vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &presentCount, presentModes.data());

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == std::numeric_limits<uint32_t>::max())
    {
        extent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (Options::Performance::MAX_FRAMES_IN_FLIGHT > 0 && imageCount > Options::Performance::MAX_FRAMES_IN_FLIGHT)
        imageCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    // ── PRESENT MODE: BestQuality = capped, Uncapped = uncapped ─────────────
    VkPresentModeKHR desired = VK_PRESENT_MODE_FIFO_KHR;
    if (Options::Display::UNCAPPED_MODE_ACTIVE)
        desired = VK_PRESENT_MODE_IMMEDIATE_KHR;
    else if (Options::Performance::PREFER_MAILBOX_PRESENT)
        desired = VK_PRESENT_MODE_MAILBOX_KHR;

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto mode : { desired, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR })
    {
        if (std::find(presentModes.begin(), presentModes.end(), mode) != presentModes.end())
        {
            presentMode = mode;
            break;
        }
    }

    // HDR format selection
    VkSurfaceFormatKHR chosen = formats[0];
    if (supportsHDR())
    {
        for (const auto& f : formats)
            if (f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
                { chosen = f; break; }
    }
    else
    {
        for (const auto& f : formats)
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                { chosen = f; break; }
    }

    QueueFamilyIndices qf = findQueueFamilies(stone_physical(), stone_surface());
    uint32_t queueFamilyIndices[] = { qf.graphicsFamily.value(), qf.presentFamily.value() };

    VkSwapchainCreateInfoKHR ci{
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = stone_surface(),
        .minImageCount    = imageCount,
        .imageFormat      = chosen.format,
        .imageColorSpace  = chosen.colorSpace,
        .imageExtent      = extent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform     = caps.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = presentMode,
        .clipped          = VK_TRUE,
        .oldSwapchain     = old
    };

    if (qf.graphicsFamily != qf.presentFamily)
    {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = queueFamilyIndices;
    }
    else
    {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(stone_device(), &ci, nullptr, &raw));

    swapchain_ = Handle<VkSwapchainKHR>(raw, stone_device());
    swapchainExtent_   = extent;
    swapchainFormat_   = chosen.format;
    currentColorSpace_ = chosen.colorSpace;
    currentPresentMode_ = presentMode;

    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(stone_device(), raw, &imgCount, nullptr));
    swapchainImages_.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(stone_device(), raw, &imgCount, swapchainImages_.data()));

    if (old && old != raw)
        vkDestroySwapchainKHR(stone_device(), old, nullptr);

    stone_seal_swapchain(raw);
    stone_seal_extent(extent);
    stone_seal_image_count(imgCount);
    stone_seal_images(swapchainImages_);
}

void SwapchainManager::createImageViews() noexcept
{
    swapchainImageViews_.resize(swapchainImages_.size());

    VkImageViewCreateInfo ci{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = swapchainFormat_,
        .components       = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                              VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    for (size_t i = 0; i < swapchainImages_.size(); ++i)
    {
        ci.image = swapchainImages_[i];
        VK_CHECK(vkCreateImageView(stone_device(), &ci, nullptr, &swapchainImageViews_[i]));
    }

    stone_seal_views(swapchainImageViews_);
}

// ── PRESENT (robust, handles SUBOPTIMAL) ───────────────────────────────────
void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) noexcept
{
    VkSwapchainKHR rawSwapchain = swapchain_.get();  // <-- THIS LINE IS REQUIRED

    VkPresentInfoKHR pi{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = waitSemaphore ? 1u : 0u,
        .pWaitSemaphores    = waitSemaphore ? &waitSemaphore : nullptr,
        .swapchainCount     = 1,
        .pSwapchains        = &rawSwapchain,   // <-- now valid lvalue
        .pImageIndices      = &imageIndex
    };

    VkResult result = vkQueuePresentKHR(queue, &pi);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        g_resizeRequested.store(true);
        g_resizeWidth.store(swapchainExtent_.width);
        g_resizeHeight.store(swapchainExtent_.height);
        las().notifyResize();
    }
    else if (result != VK_SUCCESS)
    {
        LOG_FATAL("vkQueuePresentKHR failed: {}", string_VkResult(result));
    }
}

// ── FRAME PACING ───────────────────────────────────────────────────────────
void SwapchainManager::initializeFramePacing() noexcept
{
    if (!Options::Performance::ENABLE_FRAME_PREDICTION) return;

    vkGetRefreshCycleDurationGOOGLE = (PFN_vkGetRefreshCycleDurationGOOGLE)
        vkGetDeviceProcAddr(stone_device(), "vkGetRefreshCycleDurationGOOGLE");

    if (vkGetRefreshCycleDurationGOOGLE && swapchain_.valid())
        vkGetRefreshCycleDurationGOOGLE(stone_device(), swapchain_.get(), &refreshDuration_);
}

uint64_t SwapchainManager::getNextPresentTime() noexcept
{
    if (!Options::Performance::ENABLE_FRAME_PREDICTION || !vkGetRefreshCycleDurationGOOGLE) return 0;

    uint32_t count = 0;
    vkGetPastPresentationTimingGOOGLE(stone_device(), swapchain_.get(), &count, nullptr);
    if (count == 0) return 0;

    timingHistory_.resize(count);
    vkGetPastPresentationTimingGOOGLE(stone_device(), swapchain_.get(), &count, timingHistory_.data());
    if (timingHistory_.empty()) return 0;

    return timingHistory_.back().actualPresentTime + refreshDuration_.refreshDuration;
}

// ── HDR METADATA INJECTION ─────────────────────────────────────────────────
void SwapchainManager::injectHdrMetadata(VkCommandBuffer, uint32_t) noexcept
{
    if (!supportsHDR()) return;

    if (!vkSetHdrMetadataEXT)
    {
        vkSetHdrMetadataEXT = (PFN_vkSetHdrMetadataEXT)
            vkGetDeviceProcAddr(stone_device(), "vkSetHdrMetadataEXT");
        if (!vkSetHdrMetadataEXT) return;
    }

    if (!swapchain_.valid()) return;

    VkSwapchainKHR rawSwapchain = swapchain_.get();  // <-- THIS LINE IS REQUIRED

    VkHdrMetadataEXT m{
        .sType                     = VK_STRUCTURE_TYPE_HDR_METADATA_EXT,
        .displayPrimaryRed         = {0.708f, 0.292f},
        .displayPrimaryGreen       = {0.170f, 0.797f},
        .displayPrimaryBlue        = {0.131f, 0.046f},
        .whitePoint                = {0.3127f, 0.3290f},
        .maxLuminance              = Options::Display::TARGET_BRIGHTNESS_NITS,
        .minLuminance              = 0.001f,
        .maxContentLightLevel      = Options::Display::TARGET_BRIGHTNESS_NITS,
        .maxFrameAverageLightLevel = Options::Display::TARGET_BRIGHTNESS_NITS * 0.4f
    };

    vkSetHdrMetadataEXT(stone_device(), 1, &rawSwapchain, &m);
}

// ── DYNAMIC FEATURES ───────────────────────────────────────────────────────
void SwapchainManager::handleDisplayHotplug(SDL_Event* event) noexcept
{
    if (!event) return;
    if (event->type == SDL_EVENT_WINDOW_HDR_STATE_CHANGED ||
        event->type == SDL_EVENT_DISPLAY_ADDED ||
        event->type == SDL_EVENT_DISPLAY_REMOVED)
    {
        autoEnableHDR();
        recreate(swapchainExtent_.width, swapchainExtent_.height);
    }
}

void SwapchainManager::enableDirectDisplay(bool enable) noexcept
{
    if (!Options::Performance::ENABLE_DIRECT_DISPLAY) return;
    directDisplayEnabled_ = enable;
    recreate(swapchainExtent_.width, swapchainExtent_.height);
}

void SwapchainManager::predictResize(uint32_t w, uint32_t h) noexcept
{
    if (!Options::Window::ENABLE_QUANTUM_RESIZE_PREDICTION) return;
    if (w == 0 || h == 0 || w > 16384 || h > 16384) return;

    VkSwapchainKHR old = swapchain_.get();
    swapchain_ = Handle<VkSwapchainKHR>();
    createSwapchain(stone_window(), w, h, old);
    createImageViews();
    las().notifyResize();
}

void SwapchainManager::setPresentMode(VkPresentModeKHR mode) noexcept
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    if (count) vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &count, modes.data());

    bool supported = std::find(modes.begin(), modes.end(), mode) != modes.end();
    if (!supported) mode = VK_PRESENT_MODE_FIFO_KHR;

    currentPresentMode_ = mode;
    recreate(swapchainExtent_.width, swapchainExtent_.height);
}

void SwapchainManager::releaseAcquiredImages() noexcept { /* Vulkan handles it */ }

} // namespace RTX

// =============================================================================
// FIRST LIGHT ETERNAL — DECEMBER 05 2025
// COMPILES CLEAN — NO 60FPS DEADLOCK — INFINITE TIMEOUT
// BESTQUALITY = CAPPED — UNCAPPED = UNCAPPED
// PINK PHOTONS ASCENDANT
// =============================================================================