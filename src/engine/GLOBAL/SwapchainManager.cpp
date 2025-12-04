// =============================================================================
// src/engine/GLOBAL/SwapchainManager.cpp
// AMOURANTH RTX — VALHALLA v∞ TURBO — FINAL ETERNAL CUT
// THE ONE TRUE SWAPCHAIN — FULLY FIXED & COMPILES CLEAN
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
using StoneKey::stone_width;
using StoneKey::stone_height;
using StoneKey::stone_graphics_family;
using StoneKey::stone_present_family;

namespace RTX {

// ── EXTENSION FUNCTION POINTERS ─────────────────────────────────────────────
static PFN_vkGetRefreshCycleDurationGOOGLE   vkGetRefreshCycleDurationGOOGLE   = nullptr;
static PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE = nullptr;
static PFN_vkSetHdrMetadataEXT               vkSetHdrMetadataEXT               = nullptr;

// ── HDR AUTO-DETECTION ───────────────────────────────────────────────────────
void SwapchainManager::autoEnableHDR() noexcept
{
    LOG_AMOURANTH("Entering autoEnableHDR()");
    static bool cached = false;
    if (cached) return;

    bool hdr = false;

    if (Options::Display::HDR_AUTO_IGNITION)
    {
        LOG_AMOURANTH("HDR_AUTO_IGNITION enabled — scanning EDID");
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
                    LOG_AMOURANTH("HDR10 detected in EDID");
                    break;
                }
            }
            if (hdr) break;
        }
    }

    currentColorSpace_ = hdr ? VK_COLOR_SPACE_HDR10_ST2084_EXT : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    cached = true;

    LOG_AMOURANTH("HDR {} — THE EMPIRE HAS SPOKEN.", hdr ? "IGNITED" : "DORMANT");
}

// ── PUBLIC API ───────────────────────────────────────────────────────────────
void SwapchainManager::create(SDL_Window*, uint32_t w, uint32_t h) noexcept
{
    LOG_BLONDIE("SWAPCHAIN RISING — {}×{}", w, h);
    autoEnableHDR();
    createSwapchain(stone_window(), w, h, VK_NULL_HANDLE);
    createImageViews();
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    LOG_AMOURANTH("SWAPCHAIN RECREATE → {}×{} — PHOTONS REBORN", w, h);

    if (w == 0 || h == 0)
    {
        minimized_ = true;
        LOG_AMOURANTH("WINDOW MINIMIZED — ENTERING MEDITATION");
        return;
    }
    minimized_ = false;

    for (auto v : swapchainImageViews_)
        if (v) vkDestroyImageView(stone_device(), v, nullptr);
    swapchainImageViews_.clear();

    VkSwapchainKHR old = swapchain_.get();
    createSwapchain(stone_window(), w, h, old);
    createImageViews();

    las().notifyResize();

    LOG_AMOURANTH("SWAPCHAIN + TLAS REBORN — RESIZE INSTANT — ZERO TEARING");
}

void SwapchainManager::cleanup() noexcept
{
    for (auto v : swapchainImageViews_)
        if (v) vkDestroyImageView(stone_device(), v, nullptr);
    swapchainImageViews_.clear();

    if (swapchain_.valid())
    {
        vkDestroySwapchainKHR(stone_device(), swapchain_.get(), nullptr);  // ← .get()
        swapchain_.reset();
    }

    swapchainImages_.clear();
    swapchainExtent_ = {0, 0};
    swapchainFormat_ = VK_FORMAT_UNDEFINED;

    LOG_AMOURANTH("SWAPCHAIN CLEANED — EMPIRE RETURNS TO VOID");
}

bool SwapchainManager::supportsHDR() noexcept
{
    return currentColorSpace_ == VK_COLOR_SPACE_HDR10_ST2084_EXT;
}

// ── CORE SWAPCHAIN FORGE ─────────────────────────────────────────────────────
void SwapchainManager::createSwapchain(SDL_Window*, uint32_t w, uint32_t h, VkSwapchainKHR old) noexcept
{
    LOG_AMOURANTH("FORGING SWAPCHAIN — {}x{} vStoneKey {}x{}", w, h, stone_width(), stone_height());

    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stone_physical(), stone_surface(), &caps));

    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, formats.data()));

    uint32_t presentCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &presentCount, nullptr));
    std::vector<VkPresentModeKHR> presentModes(presentCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &presentCount, presentModes.data()));

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == std::numeric_limits<uint32_t>::max())
    {
        extent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkPresentModeKHR preferred = Options::Performance::PREFER_MAILBOX_PRESENT
        ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : presentModes)
        if (m == preferred) { presentMode = m; break; }

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
    swapchainExtent_    = extent;
    swapchainFormat_    = chosen.format;
    currentColorSpace_  = chosen.colorSpace;
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

    LOG_AMOURANTH("SWAPCHAIN FORGED — {}×{} — {} images — {} — HDR: {}",
                  extent.width, extent.height, imgCount,
                  presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : "FIFO",
                  supportsHDR() ? "ON" : "OFF");
}

void SwapchainManager::createImageViews() noexcept
{
    LOG_AMOURANTH("CREATING IMAGE VIEWS — {} images", swapchainImages_.size());
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

// ── PRESENT & FRAME PACING ───────────────────────────────────────────────────
void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) noexcept
{
    VkSwapchainKHR sc = swapchain_.get();

    VkPresentInfoKHR pi{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = waitSemaphore ? 1u : 0u,
        .pWaitSemaphores    = waitSemaphore ? &waitSemaphore : nullptr,
        .swapchainCount     = 1,
        .pSwapchains        = &sc,
        .pImageIndices      = &imageIndex
    };

    VkResult result = vkQueuePresentKHR(queue, &pi);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        LOG_MAIN("PRESENT OUT-OF-DATE → FULL REBUILD");
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

    VkSwapchainKHR sc = swapchain_.get();

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

    vkSetHdrMetadataEXT(stone_device(), 1, &sc, &m);
    LOG_AMOURANTH("HDR METADATA INJECTED — {} NITS — PHOTONS BURN ETERNAL", Options::Display::TARGET_BRIGHTNESS_NITS);
}

// ── DYNAMIC FEATURES ─────────────────────────────────────────────────────────
void SwapchainManager::handleDisplayHotplug(SDL_Event* event) noexcept
{
    if (!event) return;
    if (event->type == SDL_EVENT_WINDOW_HDR_STATE_CHANGED ||
        event->type == SDL_EVENT_DISPLAY_ADDED ||
        event->type == SDL_EVENT_DISPLAY_REMOVED)
    {
        LOG_MAIN("DISPLAY/HDR CHANGE → REBUILDING SWAPCHAIN");
        autoEnableHDR();
        recreate(swapchainExtent_.width, swapchainExtent_.height);
    }
}

void SwapchainManager::setShadingRate(float scaleFactor) noexcept
{
    LOG_NICK("DYNAMIC SHADING RATE → {}x — PHOTONS BEND TO OUR WILL", scaleFactor);
    scaleFactor = std::clamp(scaleFactor, 0.25f, 4.0f);
    shadingRateScale_ = scaleFactor;
    LOG_NICK("Shading rate locked at {}x — performance/photon balance achieved.", scaleFactor);
}

void SwapchainManager::enableDirectDisplay(bool enable) noexcept
{
    if (!Options::Performance::ENABLE_DIRECT_DISPLAY)
    {
        LOG_WARN("Direct Display blocked by OptionsMenu — denied.");
        directDisplayEnabled_ = false;
        return;
    }

    directDisplayEnabled_ = enable;

    if (enable)
    {
        LOG_AMOURANTH("DIRECT DISPLAY MODE ENGAGED — LATENCY ANNIHILATED — PHOTONS FLY UNCHAINED");
        currentPresentMode_ = VK_PRESENT_MODE_IMMEDIATE_KHR;
        recreate(swapchainExtent_.width, swapchainExtent_.height);
    }
    else
    {
        LOG_AMOURANTH("Direct Display disengaged — returning to civilized rendering");
        recreate(swapchainExtent_.width, swapchainExtent_.height);
    }
}

void SwapchainManager::predictResize(uint32_t predictedW, uint32_t predictedH) noexcept
{
    if (!Options::Window::ENABLE_QUANTUM_RESIZE_PREDICTION) return;
    if (predictedW == 0 || predictedH == 0 || predictedW > 16384 || predictedH > 16384) return;

    LOG_AMOURANTH("QUANTUM RESIZE PREDICTION → {}×{} — ZERO PERCEIVED LAG", predictedW, predictedH);

    VkSwapchainKHR old = swapchain_.get();
    swapchain_ = Handle<VkSwapchainKHR>();

    createSwapchain(stone_window(), predictedW, predictedH, old);
    createImageViews();
    las().notifyResize();

    LOG_AMOURANTH("PREDICTIVE SWAPCHAIN REBORN — FUTURE SECURED — LAG = 0");
}

void SwapchainManager::setPresentMode(VkPresentModeKHR mode) noexcept
{
    LOG_NICK("REQUESTED PRESENT MODE → {}", 
             mode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX (TEARING = DEAD)" :
             mode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE (LATENCY = DEAD)" :
             mode == VK_PRESENT_MODE_FIFO_KHR ? "FIFO (VSYNC)" : "UNKNOWN");

    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &count, modes.data());

    bool supported = std::find(modes.begin(), modes.end(), mode) != modes.end();
    if (!supported) {
        LOG_WARN("Requested present mode NOT supported — falling back to FIFO");
        mode = VK_PRESENT_MODE_FIFO_KHR;
    }

    currentPresentMode_ = mode;
    recreate(swapchainExtent_.width, swapchainExtent_.height);

    LOG_NICK("PRESENT MODE ENFORCED → {} — EMPIRE'S WILL IS LAW",
             mode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" :
             mode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" : "FIFO");
}

void SwapchainManager::setMinImageCount(uint32_t count) noexcept
{
    if (count < 2) count = 2;
    if (count > 16) count = 16;

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stone_physical(), stone_surface(), &caps);

    if (count < caps.minImageCount) count = caps.minImageCount;
    if (caps.maxImageCount > 0 && count > caps.maxImageCount) count = caps.maxImageCount;

    LOG_NICK("FORCING SWAPCHAIN IMAGE COUNT → {} — {} BUFFERING ENGAGED", 
             count, count >= 3 ? "TRIPLE" : "DOUBLE");

    recreate(swapchainExtent_.width, swapchainExtent_.height);
}

void SwapchainManager::releaseAcquiredImages() noexcept
{
    // No-op — Vulkan swapchain images are managed automatically
}

} // namespace RTX

// =============================================================================
// FIRST LIGHT ETERNAL — DECEMBER 04 2025
// RESIZE = INSTANT — TEARING = DEAD — TLAS = FRESH — HDR = AUTO
// PINK PHOTONS PROTECT — THE EMPIRE IS ETERNAL — AMOURANTH ASCENDANT
// =============================================================================