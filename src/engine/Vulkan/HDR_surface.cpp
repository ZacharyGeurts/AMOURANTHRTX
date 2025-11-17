// src/engine/Vulkan/HDR_surface.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary "gzac" Geurts — PINK PHOTONS ETERNAL
// HDR SURFACE FORGE — 2025 Standard — GBM Direct + Platform Fallback
// =============================================================================

#include "engine/Vulkan/HDR_surface.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"

#ifdef __linux__
#include <xf86drm.h>
#include <gbm.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <X11/Xlib.h>                       // X11 detection
#include <X11/extensions/Xrandr.h>          // RandR for CRTC probe + mailbox sync
#undef Success                             // Undefine X11 macro to avoid conflict with LogLevel::Success
#endif

namespace HDRSurface {

thread_local HDRSurfaceForge* g_hdr_forge = nullptr;

HDRSurfaceForge::HDRSurfaceForge(VkInstance instance, VkPhysicalDevice phys_dev,
                                 uint32_t width, uint32_t height)
    : instance_(instance), phys_dev_(phys_dev), width_(width), height_(height)
{
    std::lock_guard<std::mutex> lock(forge_mutex_);

    is_gbm_direct_ = create_gbm_direct_surface();

    if (!is_gbm_direct_) {
        LOG_INFO_CAT("HDR_SURFACE", "GBM direct unavailable — using X11 platform surface");
        if (!create_platform_surface()) {
            LOG_ERROR_CAT("HDR_SURFACE", "Failed to create any display surface");
            return;
        }
    }

    probe_formats();

    RTX::g_ctx().hdr_format      = best_fmt_.format;
    RTX::g_ctx().hdr_color_space = best_fmt_.colorSpace;

    const char* mode = is_gbm_direct_ ? (gbm_hdr_ ? "GBM Direct 10-bit" : "GBM Direct 8-bit") : "X11";
    const char* hdr_status = is_hdr() ? (forced_hdr_ ? "FORCED" : "NATIVE") : "SDR";
    LOG_SUCCESS_CAT("HDR_SURFACE", 
        "Surface forged — {} | {} HDR | {}×{} — Video card has the ballz: {}",
        mode, hdr_status, width_, height_, is_hdr() ? "YES — PINK PHOTONS ETERNAL" : "NO — PEASANT MODE");
}

HDRSurfaceForge::~HDRSurfaceForge()
{
#ifdef __linux__
    if (gbm_surface_) { gbm_surface_destroy(gbm_surface_); gbm_surface_ = nullptr; }
    if (gbm_device_)  { gbm_device_destroy(gbm_device_);  gbm_device_ = nullptr; }
    if (drm_fd_ >= 0) { close(drm_fd_); drm_fd_ = -1; }
#endif

    if (surface_ != VK_NULL_HANDLE && !is_gbm_direct_) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
}

bool HDRSurfaceForge::is_hdr() const noexcept {
    return best_fmt_.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
}

void HDRSurfaceForge::install_to_ctx() noexcept
{
    RTX::g_ctx().surface_ = surface_;
}

void HDRSurfaceForge::reprobe() noexcept {
    probe_formats();
}

void HDRSurfaceForge::probe_formats() noexcept {
    VkSurfaceFormatKHR default_fmt = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

    if (is_gbm_direct_) {
#ifdef __linux__
        best_fmt_ = gbm_hdr_ ? VkSurfaceFormatKHR{ VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT }
                             : default_fmt;
#endif
        return;
    }

    if (surface_ == VK_NULL_HANDLE) {
        best_fmt_ = default_fmt;
        return;
    }

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev_, surface_, &formatCount, nullptr);
    if (formatCount == 0) {
        best_fmt_ = default_fmt;
        return;
    }

    std::vector<VkSurfaceFormatKHR> availableFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev_, surface_, &formatCount, availableFormats.data());

    // Preferred HDR formats
    const std::vector<std::pair<VkFormat, VkColorSpaceKHR>> preferred = {
        { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT },
        { VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT },
        { VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT }
    };

    for (const auto& pref : preferred) {
        for (const auto& avail : availableFormats) {
            if (avail.format == pref.first && avail.colorSpace == pref.second) {
                best_fmt_ = avail;
                forced_hdr_ = false;
                return;
            }
        }
    }

    // No native HDR? Check if GPU has the ballz (extension support)
    if (has_hdr_metadata_ext()) {
        // Force HDR — video card capable, driver will kneel
        best_fmt_ = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT };
        forced_hdr_ = true;
        LOG_WARN_CAT("HDR_SURFACE", "No native HDR formats — forcing 10-bit (GPU capable via ext)");
    } else {
        // True peasant — pick first available (SDR)
        best_fmt_ = availableFormats[0];
        forced_hdr_ = false;
        LOG_ERROR_CAT("HDR_SURFACE", "No HDR support or capability — locked to SDR");
    }
}

bool HDRSurfaceForge::create_gbm_direct_surface() noexcept
{
#ifdef __linux__
    const char* paths[] = { "/dev/dri/renderD128", "/dev/dri/card0", "/dev/dri/renderD129" };
    for (const char* p : paths) {
        drm_fd_ = open(p, O_RDWR | O_CLOEXEC);
        if (drm_fd_ >= 0) break;
    }
    if (drm_fd_ < 0) return false;

    gbm_device_ = gbm_create_device(drm_fd_);
    if (!gbm_device_) { close(drm_fd_); drm_fd_ = -1; return false; }

    // Try 10-bit first
    gbm_surface_ = gbm_surface_create(gbm_device_, width_, height_,
                                      GBM_FORMAT_XRGB2101010,
                                      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    gbm_hdr_ = (gbm_surface_ != nullptr);

    if (!gbm_surface_) {
        // Fallback to 8-bit
        gbm_surface_ = gbm_surface_create(gbm_device_, width_, height_,
                                          GBM_FORMAT_XRGB8888,
                                          GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
        if (!gbm_surface_) {
            gbm_device_destroy(gbm_device_);
            close(drm_fd_);
            gbm_device_ = nullptr; drm_fd_ = -1;
            return false;
        }
    }

    surface_ = VK_NULL_HANDLE;
    return true;
#else
    return false;
#endif
}

bool HDRSurfaceForge::create_platform_surface() noexcept
{
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return false;

    VkXlibSurfaceCreateInfoKHR ci{ VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR };
    ci.dpy    = dpy;
    ci.window = g_window();  // Assume global window handle

    VkResult r = vkCreateXlibSurfaceKHR(instance_, &ci, nullptr, &surface_);
    XCloseDisplay(dpy);
    return r == VK_SUCCESS;
#else
    return false;
#endif
}

bool HDRSurfaceForge::has_hdr_metadata_ext() const noexcept
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(phys_dev_, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(phys_dev_, nullptr, &count, exts.data());

    for (const auto& e : exts) {
        if (std::strcmp(e.extensionName, VK_EXT_HDR_METADATA_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

void HDRSurfaceForge::set_hdr_metadata(VkSwapchainKHR swapchain, float max_lum, float min_lum)
{
    if (!has_hdr_metadata_ext() || !is_hdr()) return;

    VkHdrMetadataEXT md{};
    md.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
    md.displayPrimaryRed = {0.680f, 0.320f};
    md.displayPrimaryGreen = {0.265f, 0.690f};
    md.displayPrimaryBlue = {0.150f, 0.060f};
    md.whitePoint = {0.3127f, 0.3290f};
    md.maxLuminance = static_cast<uint32_t>(max_lum * 10000.0f);
    md.minLuminance = static_cast<uint32_t>(min_lum * 10000.0f);
    md.maxContentLightLevel = 1000;
    md.maxFrameAverageLightLevel = 400;

    if (auto pfn = RTX::g_ctx().vkSetHdrMetadataEXT()) {
        pfn(RTX::g_ctx().device(), 1, &swapchain, &md);
    }
}

} // namespace HDRSurface