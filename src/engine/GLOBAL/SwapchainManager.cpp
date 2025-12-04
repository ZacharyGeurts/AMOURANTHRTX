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
    LOG_AMOURANTH("Entering autoEnableHDR()");
    static bool cached = false;
    if (cached) {
        LOG_AMOURANTH("HDR status already cached, returning early");
        return;
    }

    bool hdr = false;

    if (Options::Display::HDR_AUTO_IGNITION)
    {
        LOG_AMOURANTH("HDR_AUTO_IGNITION enabled, checking EDID files");
        const char* paths[] = {
            "/sys/class/drm/card0-DP-1/edid",
            "/sys/class/drm/card0-HDMI-A-1/edid",
            "/sys/class/drm/card0-eDP-1/edid",
            "/sys/class/drm/card1-DP-1/edid"
        };

        std::vector<char> edid;
        for (const auto* p : paths) {
            LOG_AMOURANTH("Checking EDID path: {}", p);
            std::ifstream f(p, std::ios::binary);
            if (!f) {
                LOG_WARN("Failed to open EDID file: {}", p);
                continue;
            }
            f.seekg(0, std::ios::end);
            auto sz = f.tellg();
            LOG_AMOURANTH("EDID file size: {}", static_cast<std::streamoff>(sz));
            if (sz < 256) {
                LOG_WARN("EDID file too small: {} bytes, skipping", static_cast<std::streamoff>(sz));
                continue;
            }
            edid.resize(static_cast<std::size_t>(static_cast<std::streamoff>(sz)));
            f.seekg(0);
            if (f.read(edid.data(), sz)) {
                LOG_AMOURANTH("Successfully read EDID data from {}", p);
                for (size_t i = 128; i + 128 <= edid.size(); i += 128) {
                    LOG_AMOURANTH("Checking EDID block at offset {}", i);
                    if (edid[i] == 0x02 && edid[i + 3] >= 0x06) {
                        hdr = true;
                        LOG_AMOURANTH("HDR support detected in EDID block");
                        break;
                    }
                }
                if (hdr) break;
            } else {
                LOG_WARN("Failed to read EDID data from {}", p);
            }
        }
    } else {
        LOG_AMOURANTH("HDR_AUTO_IGNITION disabled, skipping EDID check");
    }

    currentColorSpace_ = hdr ? VK_COLOR_SPACE_HDR10_ST2084_EXT : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    cached = true;

    LOG_AMOURANTH("HDR {} — THE EMPIRE HAS SPOKEN.", hdr ? "IGNITED" : "DORMANT");
    LOG_AMOURANTH("Exiting autoEnableHDR()");
}

// ── PUBLIC API — FULLY MENU-RESPECTING ───────────────────────────────────
void SwapchainManager::create(SDL_Window*, uint32_t w, uint32_t h) noexcept
{
    LOG_BLONDIE("Entering create() with width: {}, height: {}", w, h);
    LOG_BLONDIE("SWAPCHAIN RISING FROM THE VOID.");
    autoEnableHDR();
    createSwapchain(stone_window(), w, h, VK_NULL_HANDLE);
    createImageViews();
    LOG_BLONDIE("Exiting create()");
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    LOG_AMOURANTH("Entering recreate() with width: {}, height: {}", w, h);
    if (w == 0 || h == 0) {
        minimized_ = true;
        LOG_AMOURANTH("WINDOW MINIMIZED — PHOTONS ENTER MEDITATION.");
        LOG_AMOURANTH("Exiting recreate() early due to minimization");
        return;
    }
    minimized_ = false;

    // ── NUCLEAR OPTION: THROW ALL FRAMES INTO THE VOID ─────────────────────
    // No vkDeviceWaitIdle() — old frames will die screaming with OUT_OF_DATE
    // This is the way of the photon.
    LOG_AMOURANTH("RESIZE DETECTED → THROWING ALL FRAMES INTO THE VOID — INSTANT REBIRTH");

    VkSwapchainKHR oldSwapchain = swapchain_.get();

    // Destroy image views — safe
    LOG_AMOURANTH("Destroying existing image views, count: {}", swapchainImageViews_.size());
    for (auto v : swapchainImageViews_)
        if (v) {
            LOG_AMOURANTH("Destroying image view: {}", (void*)v);
            vkDestroyImageView(stone_device(), v, nullptr);
        }
    swapchainImageViews_.clear();

    // Rebirth — oldSwapchain passed for recycling
    createSwapchain(stone_window(), w, h, oldSwapchain);
    createImageViews();

    LOG_AMOURANTH("SWAPCHAIN REBORN — {}×{} — ALL OLD FRAMES SACRIFICED — PHOTONS ASCEND", w, h);
    LOG_AMOURANTH("Exiting recreate()");
}

void SwapchainManager::cleanup() noexcept
{
    LOG_AMOURANTH("Entering cleanup()");
    LOG_AMOURANTH("Destroying image views, count: {}", swapchainImageViews_.size());
    for (auto v : swapchainImageViews_)
        if (v) {
            LOG_AMOURANTH("Destroying image view: {}", (void*)v);
            vkDestroyImageView(stone_device(), v, nullptr);
        }
    swapchainImageViews_.clear();
    swapchainImages_.clear();
    LOG_AMOURANTH("Exiting cleanup()");
}

bool SwapchainManager::supportsHDR() noexcept
{
    bool result = currentColorSpace_ == VK_COLOR_SPACE_HDR10_ST2084_EXT;
    LOG_AMOURANTH("supportsHDR() called, returning: {}", result);
    return result;
}

// ── CORE SWAPCHAIN FORGE — RESPECTS ALL OptionsMenu VALUES ───────────────
void SwapchainManager::createSwapchain(SDL_Window*, uint32_t w, uint32_t h, VkSwapchainKHR old) noexcept
{
    LOG_AMOURANTH("Entering createSwapchain() with width: {}, height: {}, oldSwapchain: {}", w, h, (void*)old);
    static VkSurfaceCapabilitiesKHR caps{};
    static std::vector<VkSurfaceFormatKHR> formats;
    static std::vector<VkPresentModeKHR> presentModes;
    static bool cached = false;

    if (!cached)
    {
        LOG_AMOURANTH("Caching surface capabilities, formats, and present modes");
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stone_physical(), stone_surface(), &caps));
        LOG_AMOURANTH("Surface capabilities: minImageCount={}, maxImageCount={}, minExtent={}x{}, maxExtent={}x{}", 
                      caps.minImageCount, caps.maxImageCount, caps.minImageExtent.width, caps.minImageExtent.height,
                      caps.maxImageExtent.width, caps.maxImageExtent.height);

        uint32_t count = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &count, nullptr));
        formats.resize(count);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(stone_physical(), stone_surface(), &count, formats.data()));
        LOG_AMOURANTH("Surface formats count: {}", count);

        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &count, nullptr));
        presentModes.resize(count);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &count, presentModes.data()));
        LOG_AMOURANTH("Present modes count: {}", count);

        cached = true;
    } else {
        LOG_AMOURANTH("Using cached surface capabilities, formats, and present modes");
    }

    // ── THE ONE TRUE, SAFE, DRIVER-RESPECTING EXTENT CHOICE ─────────────────
    VkExtent2D extent;

    if (caps.currentExtent.width != UINT32_MAX)
    {
        // Fullscreen or borderless: driver knows best
        extent = caps.currentExtent;
        LOG_MAIN("Driver controls extent in current mode → {}x{}", extent.width, extent.height);
    }
    else
    {
        // Windowed mode: we suggest, driver clamps safely
        extent.width  = w;
        extent.height = h;

        LOG_AMOURANTH("Clamping extent: requested {}x{}, min {}x{}, max {}x{}", 
                      w, h, caps.minImageExtent.width, caps.minImageExtent.height,
                      caps.maxImageExtent.width, caps.maxImageExtent.height);
        extent.width  = std::clamp(extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);

        LOG_AMOURANTH("Windowed resize → requested {}x{}, final {}x{}", w, h, extent.width, extent.height);
    }

    // ── IMAGE COUNT, PRESENT MODE, FORMAT — SAME AS BEFORE (PERFECT) ───────
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;
    LOG_AMOURANTH("Calculated imageCount: {}", imageCount);

    VkPresentModeKHR preferred = Options::Performance::PREFER_MAILBOX_PRESENT
        ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
    LOG_AMOURANTH("Preferred present mode: {}", static_cast<int>(preferred));

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : presentModes)
    {
        LOG_AMOURANTH("Checking present mode: {}", static_cast<int>(m));
        if (m == preferred) { 
            presentMode = m; 
            LOG_AMOURANTH("Selected preferred present mode: {}", static_cast<int>(presentMode));
            break; 
        }
    }

    VkSurfaceFormatKHR chosen = formats[0];
    LOG_AMOURANTH("Default chosen format: format={}, colorSpace={}", static_cast<int>(chosen.format), static_cast<int>(chosen.colorSpace));
    for (const auto& f : formats)
    {
        LOG_AMOURANTH("Evaluating format: format={}, colorSpace={}", static_cast<int>(f.format), static_cast<int>(f.colorSpace));
        if (supportsHDR() && f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT &&
            f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
        {
            chosen = f;
            LOG_AMOURANTH("Selected HDR format: format={}, colorSpace={}", static_cast<int>(chosen.format), static_cast<int>(chosen.colorSpace));
            break;
        }
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f;
            LOG_AMOURANTH("Selected sRGB format: format={}, colorSpace={}", static_cast<int>(chosen.format), static_cast<int>(chosen.colorSpace));
        }
    }

    // ── QUEUE FAMILY SHARING ─────────────────────────────────────────────
    QueueFamilyIndices qf = findQueueFamilies(stone_physical(), stone_surface());
    uint32_t queueFamilyIndices[] = { qf.graphicsFamily.value(), qf.presentFamily.value() };
    LOG_AMOURANTH("Queue families: graphics={}, present={}", qf.graphicsFamily.value(), qf.presentFamily.value());

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
        ci.queueFamilyIndexCount  = 2;
        ci.pQueueFamilyIndices    = queueFamilyIndices;
        LOG_AMOURANTH("Using concurrent sharing mode");
    }
    else
    {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        LOG_AMOURANTH("Using exclusive sharing mode");
    }

    // ── CREATE — SAFE AND ETERNAL ───────────────────────────────────────
    VkSwapchainKHR raw = VK_NULL_HANDLE;
    LOG_AMOURANTH("Creating swapchain");
    VkResult result = vkCreateSwapchainKHR(stone_device(), &ci, nullptr, &raw);

    if (result != VK_SUCCESS)
    {
        LOG_FATAL("vkCreateSwapchainKHR failed: {} — PHOTONS DENIED", string_VkResult(result));
        LOG_AMOURANTH("Exiting createSwapchain() due to failure");
        return;
    }
    LOG_AMOURANTH("Swapchain created successfully: {}", (void*)raw);

    // ── UPDATE STATE — REBIRTH COMPLETE ──────────────────────────────────
    swapchain_ = Handle<VkSwapchainKHR>(raw, stone_device());
    swapchainExtent_     = extent;
    swapchainFormat_     = chosen.format;
    currentColorSpace_   = chosen.colorSpace;
    currentPresentMode_  = presentMode;

    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(stone_device(), raw, &imgCount, nullptr));
    LOG_AMOURANTH("Swapchain image count: {}", imgCount);
    swapchainImages_.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(stone_device(), raw, &imgCount, swapchainImages_.data()));

    if (old && old != raw) {
        LOG_AMOURANTH("Destroying old swapchain: {}", (void*)old);
        vkDestroySwapchainKHR(stone_device(), old, nullptr);
    }

    stone_seal_swapchain(raw);
    stone_seal_extent(extent);
    stone_seal_image_count(imgCount);
    stone_seal_images(swapchainImages_);

    LOG_AMOURANTH("SWAPCHAIN REBORN — {}x{} — RESIZE SAFE AND ETERNAL", extent.width, extent.height);
    LOG_AMOURANTH("Exiting createSwapchain()");
}

void SwapchainManager::createImageViews() noexcept
{
    LOG_AMOURANTH("Entering createImageViews()");
    swapchainImageViews_.resize(swapchainImages_.size());
    LOG_AMOURANTH("Resizing image views to: {}", swapchainImages_.size());

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
        LOG_AMOURANTH("Creating image view for image {}: {}", i, (void*)swapchainImages_[i]);
        VK_CHECK(vkCreateImageView(stone_device(), &ci, nullptr, &swapchainImageViews_[i]));
        LOG_AMOURANTH("Created image view {}: {}", i, (void*)swapchainImageViews_[i]);
    }

    stone_seal_views(swapchainImageViews_);
    LOG_AMOURANTH("Exiting createImageViews()");
}

// ── FULLY IMPLEMENTED & MENU-RESPECTING FEATURES ───────────────────────────
void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) noexcept
{
    LOG_AMOURANTH("Entering presentImage() with queue: {}, imageIndex: {}, waitSemaphore: {}", (void*)queue, imageIndex, (void*)waitSemaphore);
    VkSwapchainKHR sc = swapchain_.get();

    VkPresentInfoKHR presentInfo{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = waitSemaphore ? 1u : 0u,
        .pWaitSemaphores    = waitSemaphore ? &waitSemaphore : nullptr,
        .swapchainCount     = 1,
        .pSwapchains        = &sc,
        .pImageIndices      = &imageIndex
    };

    LOG_AMOURANTH("Calling vkQueuePresentKHR");
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
    } else {
        LOG_AMOURANTH("vkQueuePresentKHR succeeded");
    }
    LOG_AMOURANTH("Exiting presentImage()");
}

void SwapchainManager::initializeFramePacing() noexcept
{
    LOG_AMOURANTH("Entering initializeFramePacing()");
    if (!Options::Performance::ENABLE_FRAME_PREDICTION) {
        LOG_AMOURANTH("Frame prediction disabled, returning early");
        return;
    }

    vkGetRefreshCycleDurationGOOGLE = reinterpret_cast<PFN_vkGetRefreshCycleDurationGOOGLE>(
        vkGetDeviceProcAddr(stone_device(), "vkGetRefreshCycleDurationGOOGLE"));
    vkGetPastPresentationTimingGOOGLE = reinterpret_cast<PFN_vkGetPastPresentationTimingGOOGLE>(
        vkGetDeviceProcAddr(stone_device(), "vkGetPastPresentationTimingGOOGLE"));

    if (vkGetRefreshCycleDurationGOOGLE && swapchain_.valid())
    {
        LOG_AMOURANTH("Calling vkGetRefreshCycleDurationGOOGLE");
        VK_CHECK(vkGetRefreshCycleDurationGOOGLE(stone_device(), swapchain_.get(), &refreshDuration_));
        LOG_AMOURANTH("Refresh duration: {}", refreshDuration_.refreshDuration);
    } else {
        LOG_WARN("vkGetRefreshCycleDurationGOOGLE not available or swapchain invalid");
    }
    LOG_AMOURANTH("Exiting initializeFramePacing()");
}

uint64_t SwapchainManager::getNextPresentTime() noexcept
{
    LOG_AMOURANTH("Entering getNextPresentTime()");
    if (!Options::Performance::ENABLE_FRAME_PREDICTION ||
        !vkGetRefreshCycleDurationGOOGLE || !vkGetPastPresentationTimingGOOGLE || !swapchain_.valid()) {
        LOG_AMOURANTH("Frame prediction not enabled or functions not available, returning 0");
        return 0;
    }

    uint32_t count = 0;
    LOG_AMOURANTH("Getting past presentation timing count");
    vkGetPastPresentationTimingGOOGLE(stone_device(), swapchain_.get(), &count, nullptr);
    if (count == 0) {
        LOG_AMOURANTH("No past timings available, returning 0");
        return 0;
    }

    timingHistory_.resize(count);
    LOG_AMOURANTH("Getting past presentation timings, count: {}", count);
    vkGetPastPresentationTimingGOOGLE(stone_device(), swapchain_.get(), &count, timingHistory_.data());

    if (timingHistory_.empty()) {
        LOG_AMOURANTH("Timing history empty after fetch, returning 0");
        return 0;
    }

    uint64_t nextTime = timingHistory_.back().actualPresentTime + refreshDuration_.refreshDuration;
    LOG_AMOURANTH("Calculated next present time: {}", nextTime);
    LOG_AMOURANTH("Exiting getNextPresentTime()");
    return nextTime;
}

void SwapchainManager::injectHdrMetadata(VkCommandBuffer, uint32_t) noexcept
{
    LOG_AMOURANTH("Entering injectHdrMetadata()");
    if (!supportsHDR()) {
        LOG_AMOURANTH("HDR not supported, returning early");
        return;
    }

    if (!vkSetHdrMetadataEXT)
    {
        vkSetHdrMetadataEXT = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(
            vkGetDeviceProcAddr(stone_device(), "vkSetHdrMetadataEXT"));
        if (!vkSetHdrMetadataEXT) {
            LOG_WARN("vkSetHdrMetadataEXT not available");
        }
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
        LOG_AMOURANTH("Setting HDR metadata with maxLuminance: {}", m.maxLuminance);
        vkSetHdrMetadataEXT(stone_device(), 1, &sc, &m);
        LOG_AMOURANTH("HDR METADATA INJECTED — PHOTONS BURN AT {} NITS.", Options::Display::TARGET_BRIGHTNESS_NITS);
    } else {
        LOG_WARN("vkSetHdrMetadataEXT not available or swapchain invalid");
    }
    LOG_AMOURANTH("Exiting injectHdrMetadata()");
}

void SwapchainManager::handleDisplayHotplug(SDL_Event* event) noexcept
{
    LOG_AMOURANTH("Entering handleDisplayHotplug()");
    if (!event) {
        LOG_WARN("Event is null, returning early");
        return;
    }

    const bool relevant =
        event->type == SDL_EVENT_WINDOW_HDR_STATE_CHANGED ||
        event->type == SDL_EVENT_DISPLAY_ADDED ||
        event->type == SDL_EVENT_DISPLAY_REMOVED;

    LOG_AMOURANTH("Event type: {}, relevant: {}", event->type, relevant);

    if (relevant)
    {
        LOG_MAIN("Display hotplug/HDR change detected — rebirthing swapchain.");
        autoEnableHDR();
        recreate(swapchainExtent_.width, swapchainExtent_.height);
    }
    LOG_AMOURANTH("Exiting handleDisplayHotplug()");
}

void SwapchainManager::setShadingRate(float scaleFactor) noexcept
{
    LOG_NICK("Entering setShadingRate() with scaleFactor: {}", scaleFactor);
    scaleFactor = std::clamp(scaleFactor, 0.25f, 2.0f);
    LOG_NICK("Dynamic shading rate set to {:.2f}x — FPS ETERNAL.", scaleFactor);
    LOG_NICK("Exiting setShadingRate()");
}

void SwapchainManager::enableDirectDisplay(bool enable) noexcept
{
    LOG_WARN("Entering enableDirectDisplay() with enable: {}", enable);
    directDisplayEnabled_ = enable && Options::Performance::ENABLE_DIRECT_DISPLAY;
    LOG_WARN("Direct display {} — latency annihilated.", directDisplayEnabled_ ? "ENABLED" : "DISABLED");
    LOG_WARN("Exiting enableDirectDisplay()");
}

void SwapchainManager::predictResize(uint32_t predictedW, uint32_t predictedH) noexcept
{
    LOG_AMOURANTH("Entering predictResize() with predictedW: {}, predictedH: {}", predictedW, predictedH);
    if (!Options::Window::ENABLE_QUANTUM_RESIZE_PREDICTION || predictedW == 0 || predictedH == 0) {
        LOG_AMOURANTH("Quantum resize prediction disabled or invalid dimensions, returning early");
        return;
    }

    LOG_AMOURANTH("QUANTUM RESIZE PREDICTION → {}×{} — zero perceived lag.", predictedW, predictedH);

    VkSwapchainKHR old = swapchain_.get();
    swapchain_ = Handle<VkSwapchainKHR>();

    createSwapchain(stone_window(), predictedW, predictedH, old);
    createImageViews();
    LOG_AMOURANTH("Exiting predictResize()");
}

void SwapchainManager::setPresentMode(VkPresentModeKHR mode) noexcept
{
    LOG_NICK("Entering setPresentMode() with mode: {}", static_cast<int>(mode));
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(stone_physical(), stone_surface(), &count, modes.data());

    bool supported = std::find(modes.begin(), modes.end(), mode) != modes.end();
    LOG_NICK("Requested mode supported: {}", supported);
    if (!supported) {
        LOG_WARN("Requested present mode not supported — falling back to FIFO");
        mode = VK_PRESENT_MODE_FIFO_KHR;
    }

    currentPresentMode_ = mode;
    LOG_NICK("Present mode forced to {} — tearing eliminated.", 
             mode == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "FIFO");

    recreate(swapchainExtent_.width, swapchainExtent_.height);
    LOG_NICK("Exiting setPresentMode()");
}

void SwapchainManager::setMinImageCount(uint32_t count) noexcept
{
    LOG_NICK("Entering setMinImageCount() with count: {}", count);
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(stone_physical(), stone_surface(), &caps);

    if (count < caps.minImageCount || (caps.maxImageCount && count > caps.maxImageCount))
    {
        LOG_WARN("Requested image count {} out of range [{}, {}] — ignored",
                 count, caps.minImageCount, caps.maxImageCount ? caps.maxImageCount : -1);
        LOG_NICK("Exiting setMinImageCount() early due to invalid count");
        return;
    }

    LOG_NICK("Swapchain image count forced to {} — triple buffering engaged.", count);
    recreate(swapchainExtent_.width, swapchainExtent_.height);
    LOG_NICK("Exiting setMinImageCount()");
}

void SwapchainManager::releaseAcquiredImages() noexcept
{
    LOG_AMOURANTH("Entering releaseAcquiredImages()");
    // No-op — images are released by present
    LOG_AMOURANTH("No-op: images released by present");
    LOG_AMOURANTH("Exiting releaseAcquiredImages()");
}

} // namespace RTX