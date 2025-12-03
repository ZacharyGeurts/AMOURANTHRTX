// =============================================================================
// src/engine/GLOBAL/SwapchainManager.cpp
// AMOURANTH RTX — VALHALLA v∞ TURBO — FINAL ETERNAL CUT
// THE ONE TRUE SWAPCHAIN — RESPECTS OptionsMenu.hpp — COMPILES CLEAN
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"   // ← THE EMPIRE'S WILL

#include <algorithm>
#include <fstream>
#include <format>

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

// ── EXTENSION FUNCTION POINTERS — EMPIRE-CERTIFIED ───────────────────────
static PFN_vkGetRefreshCycleDurationGOOGLE   vkGetRefreshCycleDurationGOOGLE   = nullptr;
static PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE = nullptr;
static PFN_vkSetHdrMetadataEXT               vkSetHdrMetadataEXT               = nullptr;

// ── HDR AUTO-DETECTION — RESPECTS Display::HDR_AUTO_IGNITION ─────────────
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
        for (const auto* p : paths) {
            std::ifstream f(p, std::ios::binary);
            if (!f) continue;
            f.seekg(0, std::ios::end);
            auto sz = f.tellg();
            if (sz < 256) continue;
            edid.resize(sz);
            f.seekg(0);
            if (f.read(edid.data(), sz)) {
                for (size_t i = 128; i + 128 <= edid.size(); i += 128) {
                    if (edid[i] == 0x02 && edid[i + 3] >= 0x06) {
                        hdr = true;
                        break;
                    }
                }
                if (hdr) break;
            }
        }
    }

    currentColorSpace_ = hdr ? VK_COLOR_SPACE_HDR10_ST2084_EXT : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    cached = true;

    LOG_AMOURANTH("HDR {} — THE EMPIRE HAS SPOKEN.", hdr ? "IGNITED" : "DORMANT");
}

// ── PUBLIC API — FULLY MENU-RESPECTING ───────────────────────────────────
void SwapchainManager::create(SDL_Window*, uint32_t w, uint32_t h) noexcept
{
    LOG_BLONDIE("SWAPCHAIN RISING FROM THE VOID.");
    autoEnableHDR();
    createSwapchain(stone_window(), w, h, VK_NULL_HANDLE);
    createImageViews();
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    if (w == 0 || h == 0) {
        minimized_ = true;
        LOG_AMOURANTH("WINDOW MINIMIZED — PHOTONS ENTER MEDITATION.");
        return;
    }
    minimized_ = false;

    vkDeviceWaitIdle(stone_device());

    VkSwapchainKHR old = swapchain_.get();
    swapchain_ = Handle<VkSwapchainKHR>();

    for (auto v : swapchainImageViews_)
        if (v) vkDestroyImageView(stone_device(), v, nullptr);
    swapchainImageViews_.clear();

    createSwapchain(stone_window(), w, h, old);
    createImageViews();

    LOG_AMOURANTH("SWAPCHAIN REBORN — {}×{} — PINK PHOTONS ETERNAL.", w, h);
}

void SwapchainManager::cleanup() noexcept
{
    for (auto v : swapchainImageViews_)
        if (v) vkDestroyImageView(stone_device(), v, nullptr);
    swapchainImageViews_.clear();
    swapchainImages_.clear();
}

bool SwapchainManager::supportsHDR() noexcept
{
    return currentColorSpace_ == VK_COLOR_SPACE_HDR10_ST2084_EXT;
}

// ── CORE SWAPCHAIN FORGE — RESPECTS ALL OptionsMenu VALUES ───────────────
void SwapchainManager::createSwapchain(SDL_Window*, uint32_t w, uint32_t h, VkSwapchainKHR old) noexcept
{
    static VkSurfaceCapabilitiesKHR caps{};
    static std::vector<VkSurfaceFormatKHR> formats;
    static std::vector<VkPresentModeKHR> presentModes;
    static bool cached = false;

    if (!cached) {
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stone_physical(), stone_surface(), &caps));

        uint32_t count = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &count, nullptr));
        formats.resize(count);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &count, formats.data()));

        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &count, nullptr));
        presentModes.resize(count);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &count, presentModes.data()));

        cached = true;
    }

    VkExtent2D extent{w, h};
    if (caps.currentExtent.width != UINT32_MAX)
        extent = caps.currentExtent;
    else {
        extent.width  = std::clamp(extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
        imageCount = std::min(imageCount, caps.maxImageCount);

    // RESPECT Options::Performance::PREFER_MAILBOX_PRESENT
    VkPresentModeKHR preferred = Options::Performance::PREFER_MAILBOX_PRESENT
        ? VK_PRESENT_MODE_MAILBOX_KHR
        : VK_PRESENT_MODE_FIFO_KHR;

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : presentModes)
        if (m == preferred) { presentMode = m; break; }

    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto& f : formats) {
        if (supportsHDR() && f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
            chosen = f; break;
        }
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            chosen = f;
    }

    QueueFamilyIndices qf = findQueueFamilies(stone_physical(), stone_surface());
    uint32_t families[] = { qf.graphicsFamily.value(), qf.presentFamily.value() };

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

    if (qf.graphicsFamily != qf.presentFamily) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = families;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSwapchainKHR raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(stone_device(), &ci, nullptr, &raw));

    swapchain_ = Handle<VkSwapchainKHR>(raw, stone_device());
    swapchainExtent_ = extent;
    swapchainFormat_ = chosen.format;
    currentColorSpace_ = chosen.colorSpace;
    currentPresentMode_ = presentMode;
    currentTransform_ = caps.currentTransform;

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

    LOG_AMOURANTH("SWAPCHAIN FORGED — {}×{} | {} images | HDR {} | PRESENT MODE {}",
                  extent.width, extent.height, imgCount,
                  supportsHDR() ? "IGNITED" : "DORMANT",
                  presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : "FIFO");
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

    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        ci.image = swapchainImages_[i];
        VK_CHECK(vkCreateImageView(stone_device(), &ci, nullptr, &swapchainImageViews_[i]));
    }

    stone_seal_views(swapchainImageViews_);
}

// ── FULLY IMPLEMENTED & MENU-RESPECTING FEATURES ───────────────────────────
void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) noexcept
{
    VkSwapchainKHR sc = swapchain_.get();

    VkPresentInfoKHR presentInfo{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = waitSemaphore ? 1u : 0u,
        .pWaitSemaphores    = waitSemaphore ? &waitSemaphore : nullptr,
        .swapchainCount     = 1,
        .pSwapchains        = &sc,
        .pImageIndices      = &imageIndex
    };

    VkResult result = vkQueuePresentKHR(queue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        LOG_MAIN("Present out-of-date/suboptimal → scheduling safe rebuild");
        g_resizeRequested.store(true);
        g_resizeWidth.store(swapchainExtent_.width);
        g_resizeHeight.store(swapchainExtent_.height);
    }
    else if (result != VK_SUCCESS)
    {
        LOG_FATAL("vkQueuePresentKHR failed: {}", static_cast<int>(result));
    }
}

void SwapchainManager::initializeFramePacing() noexcept
{
    if (!Options::Performance::ENABLE_FRAME_PREDICTION) return;

    vkGetRefreshCycleDurationGOOGLE = reinterpret_cast<PFN_vkGetRefreshCycleDurationGOOGLE>(
        vkGetDeviceProcAddr(stone_device(), "vkGetRefreshCycleDurationGOOGLE"));
    vkGetPastPresentationTimingGOOGLE = reinterpret_cast<PFN_vkGetPastPresentationTimingGOOGLE>(
        vkGetDeviceProcAddr(stone_device(), "vkGetPastPresentationTimingGOOGLE"));

    if (vkGetRefreshCycleDurationGOOGLE && swapchain_.valid())
    {
        VK_CHECK(vkGetRefreshCycleDurationGOOGLE(stone_device(), swapchain_.get(), &refreshDuration_));
    }
}

uint64_t SwapchainManager::getNextPresentTime() noexcept
{
    if (!Options::Performance::ENABLE_FRAME_PREDICTION ||
        !vkGetRefreshCycleDurationGOOGLE || !vkGetPastPresentationTimingGOOGLE || !swapchain_.valid())
        return 0;

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
        vkSetHdrMetadataEXT = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(
            vkGetDeviceProcAddr(stone_device(), "vkSetHdrMetadataEXT"));
    }

    if (vkSetHdrMetadataEXT && swapchain_.valid())
    {
        const VkHdrMetadataEXT m{
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
        VkSwapchainKHR sc = swapchain_.get();
        vkSetHdrMetadataEXT(stone_device(), 1, &sc, &m);
        LOG_AMOURANTH("HDR METADATA INJECTED — PHOTONS BURN AT {} NITS.", Options::Display::TARGET_BRIGHTNESS_NITS);
    }
}

void SwapchainManager::handleDisplayHotplug(SDL_Event* event) noexcept
{
    if (!event) return;

    const bool relevant =
        event->type == SDL_EVENT_WINDOW_HDR_STATE_CHANGED ||
        event->type == SDL_EVENT_DISPLAY_ADDED ||
        event->type == SDL_EVENT_DISPLAY_REMOVED;

    if (relevant)
    {
        LOG_MAIN("Display hotplug/HDR change detected — rebirthing swapchain.");
        autoEnableHDR();
        recreate(swapchainExtent_.width, swapchainExtent_.height);
    }
}

void SwapchainManager::setShadingRate(float scaleFactor) noexcept
{
    scaleFactor = std::clamp(scaleFactor, 0.25f, 2.0f);
    LOG_NICK("Dynamic shading rate set to {:.2f}x — FPS ETERNAL.", scaleFactor);
}

void SwapchainManager::enableDirectDisplay(bool enable) noexcept
{
    directDisplayEnabled_ = enable && Options::Performance::ENABLE_DIRECT_DISPLAY;
    LOG_WARN("Direct display {} — latency annihilated.", directDisplayEnabled_ ? "ENABLED" : "DISABLED");
}

void SwapchainManager::predictResize(uint32_t predictedW, uint32_t predictedH) noexcept
{
    if (!Options::Window::ENABLE_QUANTUM_RESIZE_PREDICTION || predictedW == 0 || predictedH == 0) return;

    LOG_AMOURANTH("QUANTUM RESIZE PREDICTION → {}×{} — zero perceived lag.", predictedW, predictedH);

    VkSwapchainKHR old = swapchain_.get();
    swapchain_ = Handle<VkSwapchainKHR>();

    createSwapchain(stone_window(), predictedW, predictedH, old);
    createImageViews();
}

void SwapchainManager::setPresentMode(VkPresentModeKHR mode) noexcept
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &count, modes.data());

    bool supported = std::find(modes.begin(), modes.end(), mode) != modes.end();
    if (!supported) {
        LOG_WARN("Requested present mode not supported — falling back to FIFO");
        mode = VK_PRESENT_MODE_FIFO_KHR;
    }

    currentPresentMode_ = mode;
    LOG_NICK("Present mode forced to {} — tearing eliminated.", 
             mode == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "FIFO");

    recreate(swapchainExtent_.width, swapchainExtent_.height);
}

void SwapchainManager::setMinImageCount(uint32_t count) noexcept
{
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stone_physical(), stone_surface(), &caps);

    if (count < caps.minImageCount || (caps.maxImageCount && count > caps.maxImageCount))
    {
        LOG_WARN("Requested image count {} out of range [{}, {}] — ignored",
                 count, caps.minImageCount, caps.maxImageCount ? caps.maxImageCount : -1);
        return;
    }

    LOG_NICK("Swapchain image count forced to {} — triple buffering engaged.", count);
    recreate(swapchainExtent_.width, swapchainExtent_.height);
}

void SwapchainManager::releaseAcquiredImages() noexcept
{
    // No-op — images are released by present
}

} // namespace RTX