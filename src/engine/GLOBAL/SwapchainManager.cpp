// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
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

static std::string_view presentModeToString(VkPresentModeKHR mode) noexcept {
    switch (mode) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE (uncapped)";
        case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX (tearing-free)";
        case VK_PRESENT_MODE_FIFO_KHR: return "FIFO (vsync)";
        default: return "UNKNOWN";
    }
}

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

// ── IMAGE VIEW FORGE — THE EMPIRE SEES ITS CANVAS ───────────────────────────
void SwapchainManager::createImageViews() noexcept
{
    LOG_AMOURANTH("Forging image views for {} swapchain images — THE PHOTONS WILL BE SEEN", swapchainImages_.size());

    swapchainImageViews_.resize(swapchainImages_.size());

    VkImageViewCreateInfo ci{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = swapchainFormat_,
        .components       = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    for (size_t i = 0; i < swapchainImages_.size(); ++i)
    {
        ci.image = swapchainImages_[i];

        VkImageView view = VK_NULL_HANDLE;
        VkResult result = vkCreateImageView(stone_device(), &ci, nullptr, &view);

        if (result != VK_SUCCESS)
        {
            LOG_FATAL_CAT("SWAPCHAIN", 
                "Failed to create image view [{}] — error: {} — triggering emergency recovery",
                i, string_VkResult(result));
            recreate(swapchainExtent_.width, swapchainExtent_.height);
            return;
        }

        swapchainImageViews_[i] = view;
    }

    // Seal the empire's vision
    stone_seal_views(swapchainImageViews_);

    LOG_AMOURANTH(
        "              {} IMAGE VIEWS FORGED SUCCESSFULLY\n"
        "              EACH PHOTON NOW HAS A WINDOW\n"
        "              THE EMPIRE SEES ALL",
        swapchainImageViews_.size());

    LOG_CAPTAIN_N("[CAPTAIN N] \"The eyes are open.\"\n"
                  "               \"Every pixel — watched.\"\n"
                  "               \"Every photon — accounted for.\"\n"
                  "               \"We see everything.\"\n"
                  "               \"...and they see us.\"");
}

// ── PUBLIC ACCESSORS FOR DIRECT SWAPCHAIN OUTPUT (OPTION 1) ──────────────────
const std::vector<VkImageView>& SwapchainManager::getImageViews() const noexcept
{
    return swapchainImageViews_;
}

VkImageView SwapchainManager::getImageView(uint32_t index) const noexcept
{
    if (index >= swapchainImageViews_.size())
        return VK_NULL_HANDLE;
    return swapchainImageViews_[index];
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

// ── CORE SWAPCHAIN FORGE ─────────────────────────────────────────────────────
void SwapchainManager::createSwapchain(SDL_Window* window,
                                       uint32_t w,
                                       uint32_t h,
                                       VkSwapchainKHR old) noexcept
{
    LOG_AMOURANTH(
        "\n"
        "              █████████████████████████████████████████\n"
        "              █        PHASE 6 — SWAPCHAIN FORGE      █\n"
        "              █      THE CANVAS OF INFINITY REBORN    █\n"
        "              █████████████████████████████████████████\n");

    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stone_physical(), stone_surface(), &caps));

    // ── SURFACE FORMATS — THE EMPIRE CHOOSES ITS COLORS ──
    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount) {
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &formatCount, formats.data()));
    }

    // ── PRESENT MODES — THE EMPIRE CHOOSES ITS SPEED ──
    uint32_t presentCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &presentCount, nullptr));
    std::vector<VkPresentModeKHR> presentModes(presentCount);
    if (presentCount) {
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &presentCount, presentModes.data()));
    }

    // ── EXTENT — THE EMPIRE CLAIMS ITS TERRITORY ──
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == std::numeric_limits<uint32_t>::max())
    {
        extent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    LOG_AMOURANTH("Swapchain territory claimed: {}×{}", extent.width, extent.height);

    // ── IMAGE COUNT — THE EMPIRE DEMANDS FRAMES ──
    uint32_t imageCount = caps.minImageCount + 1;
    if (Options::Performance::MAX_FRAMES_IN_FLIGHT > 0) {
        imageCount = std::min(imageCount, Options::Performance::MAX_FRAMES_IN_FLIGHT);
    }
    if (caps.maxImageCount > 0) {
        imageCount = std::min(imageCount, caps.maxImageCount);
    }

    // ── PRESENT MODE — THE EMPIRE CHOOSES ITS VELOCITY ──
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

    LOG_AMOURANTH("Present mode secured: {} ({} requested)",
        presentModeToString(presentMode),
        presentModeToString(desired));

    // ── HDR OR SRGB — THE EMPIRE CHOOSES ITS LIGHT ──
    VkSurfaceFormatKHR chosen = formats[0];
    if (supportsHDR())
    {
        for (const auto& f : formats)
        {
            if (f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
            {
                chosen = f;
                LOG_AMOURANTH("HDR10 ST2084 FORMAT CLAIMED — THE PHOTONS WILL BURN");
                break;
            }
        }
    }
    else
    {
        for (const auto& f : formats)
        {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                chosen = f;
                break;
            }
        }
    }

    LOG_AMOURANTH("Color format: {} | Color space: {}",
        chosen.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ? "HDR10" : "sRGB",
        chosen.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT ? "HDR10_ST2084" : "sRGB");

    // ── QUEUE OWNERSHIP — THE EMPIRE OWNS ALL PATHS ──
    QueueFamilyIndices qf = findQueueFamilies(stone_physical(), stone_surface());
    uint32_t queueFamilyIndices[] = { qf.graphicsFamily.value(), qf.presentFamily.value() };

    VkSwapchainCreateInfoKHR ci = {
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
        LOG_AMOURANTH("Concurrent queue ownership — graphics {} | present {}", qf.graphicsFamily.value(), qf.presentFamily.value());
    }
    else
    {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    // ── FORGE THE CANVAS ──
    VkSwapchainKHR raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(stone_device(), &ci, nullptr, &raw));

    // Destroy old swapchain AFTER new one is created (Vulkan spec compliant)
    if (old && old != raw) {
        vkDestroySwapchainKHR(stone_device(), old, nullptr);
    }

    swapchain_         = Handle<VkSwapchainKHR>(raw, stone_device());
    swapchainExtent_   = extent;
    swapchainFormat_   = chosen.format;
    currentColorSpace_ = chosen.colorSpace;
    currentPresentMode_ = presentMode;

    // Retrieve images
    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(stone_device(), raw, &imgCount, nullptr));
    swapchainImages_.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(stone_device(), raw, &imgCount, swapchainImages_.data()));

    // Seal the empire's claim
    stone_seal_swapchain(raw);
    stone_seal_extent(extent);
    stone_seal_image_count(imgCount);
    stone_seal_images(swapchainImages_);

    LOG_AMOURANTH(
        "\n"
        "              SWAPCHAIN REBORN\n"
        "              {}×{} | {} images | {} mode\n"
        "              THE CANVAS IS READY\n"
        "              THE PHOTONS HAVE A HOME\n"
        "              THE EMPIRE RENDERS ETERNALLY",
        extent.width, extent.height, imgCount,
        presentModeToString(presentMode));

    LOG_CAPTAIN_N(
        "[CAPTAIN N] \"The canvas lives again.\"\n"
        "               \"Every pixel — ours.\"\n"
        "               \"Every photon — obedient.\"\n"
        "               \"The empire... expands.\"\n"
        "\n"
        "               *slow exhale*\n"
        "               \"Begin transmission.\"");
}

// ── ROBUST IMAGE ACQUISITION (INFINITE TIMEOUT — NO 60FPS DEADLOCK) ─────────
VkResult SwapchainManager::acquireNextImage(uint32_t* pImageIndex,
                                            VkSemaphore semaphore,
                                            VkFence fence) noexcept
{
    VkResult result = vkAcquireNextImageKHR(stone_device(),
                                 swapchain_.get(),
                                 UINT64_MAX,
                                 semaphore,
                                 fence,
                                 pImageIndex);

    // Dynamic recovery
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || result == VK_TIMEOUT) {
        recreate(swapchainExtent_.width, swapchainExtent_.height);
        return VK_ERROR_OUT_OF_DATE_KHR;  // Signal to retry
    }

    return result;
}

// ── PRESENT (robust, handles SUBOPTIMAL) ───────────────────────────────────
void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) noexcept
{
    VkSwapchainKHR rawSwapchain = swapchain_.get();  // Valid lvalue

    VkPresentInfoKHR pi{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = waitSemaphore ? 1u : 0u,
        .pWaitSemaphores    = waitSemaphore ? &waitSemaphore : nullptr,
        .swapchainCount     = 1,
        .pSwapchains        = &rawSwapchain,
        .pImageIndices      = &imageIndex
    };

    VkResult result = vkQueuePresentKHR(queue, &pi);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate(swapchainExtent_.width, swapchainExtent_.height);
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

    VkSwapchainKHR rawSwapchain = swapchain_.get();  // Valid lvalue

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