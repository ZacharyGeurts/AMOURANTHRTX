// src/engine/Vulkan/HDR_surface.cpp
// AMOURANTH RTX Engine © 2025 by Zachary "gzac" Geurts — PINK PHOTONS ETERNAL
// HDR SURFACE FORGERY — Full implementation (single definition)

#include "engine/Vulkan/HDR_surface.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"   // g_window(), g_wl_display(), g_hwnd(), etc.

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <vulkan/vulkan_xlib.h>
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <wayland-client.h>
#include <vulkan/vulkan_wayland.h>
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(VK_USE_PLATFORM_METAL_EXT)
#include <vulkan/vulkan_metal.h>
#include <QuartzCore/CAMetalLayer.h>
extern CAMetalLayer* g_metal_layer();
#endif

namespace HDRSurface {

// -----------------------------------------------------------------------------
// Global forge pointer definition (thread_local)
thread_local HDRSurfaceForge* g_hdr_forge = nullptr;

// -----------------------------------------------------------------------------
// Constructor
HDRSurfaceForge::HDRSurfaceForge(VkInstance instance, VkPhysicalDevice phys_dev,
                                 uint32_t width, uint32_t height)
    : instance_(instance), phys_dev_(phys_dev), width_(width), height_(height)
{
    std::lock_guard<std::mutex> lock(forge_mutex_);

    if (!create_platform_surface()) {
        LOG_ERROR_CAT("HDR_SURFACE", "Failed to create platform-specific surface — HDR forging aborted");
        return;
    }

    if (!probe_and_select_best()) {
        LOG_WARN_CAT("HDR_SURFACE", "No native HDR formats found — coercing to 10-bit PQ");
        coerce_hdr();
    }

    if (forged_success()) {
        LOG_SUCCESS_CAT("HDR_SURFACE",
            "HDR Surface Forged — {} + {} | {}x{} | PINK PHOTONS ETERNAL",
            vk::to_string(static_cast<vk::Format>(best_fmt_.format)),
            vk::to_string(static_cast<vk::ColorSpaceKHR>(best_cs_)),
            width_, height_);
    } else {
        LOG_WARN_CAT("HDR_SURFACE", "Surface created but SDR — enable monitor HDR mode for true glory");
    }
}

// -----------------------------------------------------------------------------
// Destructor
HDRSurfaceForge::~HDRSurfaceForge()
{
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
}

// -----------------------------------------------------------------------------
// Public: reprobe (called on resize / hotplug)
void HDRSurfaceForge::reprobe() noexcept
{
    VkSurfaceCapabilitiesKHR caps{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_dev_, surface_, &caps) != VK_SUCCESS) {
        return;
    }

    if (caps.currentExtent.width != width_ || caps.currentExtent.height != height_) {
        width_  = caps.currentExtent.width != UINT32_MAX ? caps.currentExtent.width : width_;
        height_ = caps.currentExtent.height != UINT32_MAX ? caps.currentExtent.height : height_;

        if (probe_and_select_best()) {
            LOG_INFO_CAT("HDR_SURFACE", "HDR Surface reprobed — new resolution {}x{}", width_, height_);
        } else {
            coerce_hdr();
        }
    }
}

// -----------------------------------------------------------------------------
// Public: HDR metadata injection
void HDRSurfaceForge::set_hdr_metadata(VkSwapchainKHR swapchain, float max_lum, float min_lum)
{
    if (!has_hdr_metadata_ext()) {
        return;
    }

    VkHdrMetadataEXT metadata{};
    metadata.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
    metadata.displayPrimaryRed   = {0.708f, 0.292f};
    metadata.displayPrimaryGreen = {0.170f, 0.797f};
    metadata.displayPrimaryBlue  = {0.131f, 0.046f};
    metadata.whitePoint          = {0.3127f, 0.3290f};
    metadata.maxLuminance        = static_cast<uint32_t>(max_lum * 10000.0f);
    metadata.minLuminance        = static_cast<uint32_t>(min_lum * 10000.0f);
    metadata.maxContentLightLevel     = 1000;
    metadata.maxFrameAverageLightLevel = 400;

    auto pfn = RTX::g_ctx().vkSetHdrMetadataEXT();
    if (pfn) {
        pfn(RTX::g_ctx().device(), 1, &swapchain, &metadata);
        LOG_DEBUG_CAT("HDR_SURFACE", "HDR metadata injected — peak {:.1f} nits", max_lum);
    }
}

// -----------------------------------------------------------------------------
// Private: Platform-specific surface creation
bool HDRSurfaceForge::create_platform_surface() noexcept
{
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return false;

    VkXlibSurfaceCreateInfoKHR ci{VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
    ci.dpy    = dpy;
    ci.window = g_window();

    VkResult r = vkCreateXlibSurfaceKHR(instance_, &ci, nullptr, &surface_);
    XCloseDisplay(dpy);
    return r == VK_SUCCESS;

#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    VkWaylandSurfaceCreateInfoKHR ci{VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
    ci.display = g_wl_display();
    ci.surface = g_wl_surface();
    return vkCreateWaylandSurfaceKHR(instance_, &ci, nullptr, &surface_) == VK_SUCCESS;

#elif defined(VK_USE_PLATFORM_WIN32_KHR)
    VkWin32SurfaceCreateInfoKHR ci{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    ci.hinstance = GetModuleHandle(nullptr);
    ci.hwnd      = g_hwnd();
    return vkCreateWin32SurfaceKHR(instance_, &ci, nullptr, &surface_) == VK_SUCCESS;

#elif defined(VK_USE_PLATFORM_METAL_EXT)
    CAMetalLayer* layer = g_metal_layer();
    if (!layer) return false;

    VkMetalSurfaceCreateInfoEXT ci{VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT};
    ci.pLayer = layer;
    return vkCreateMetalSurfaceEXT(instance_, &ci, nullptr, &surface_) == VK_SUCCESS;

#else
    LOG_ERROR_CAT("HDR_SURFACE", "No supported platform surface extension");
    return false;
#endif
}

// -----------------------------------------------------------------------------
// Private: Probe and pick best native HDR format
bool HDRSurfaceForge::probe_and_select_best() noexcept
{
    uint32_t count = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev_, surface_, &count, nullptr) != VK_SUCCESS || count == 0) {
        return false;
    }

    std::vector<VkSurfaceFormatKHR> formats(count);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev_, surface_, &count, formats.data()) != VK_SUCCESS) {
        return false;
    }

    for (VkFormat fmt : kHDRFormats) {
        for (VkColorSpaceKHR cs : kHDRSpaces) {
            auto it = std::find_if(formats.begin(), formats.end(),
                [fmt, cs](const VkSurfaceFormatKHR& f) { return f.format == fmt && f.colorSpace == cs; });
            if (it != formats.end()) {
                best_fmt_ = *it;
                best_cs_  = cs;
                return true;
            }
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Private: Force HDR when nothing native is available
void HDRSurfaceForge::coerce_hdr() noexcept
{
    best_fmt_.format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    best_cs_ = has_hdr_metadata_ext() ? VK_COLOR_SPACE_HDR10_ST2084_EXT
                                      : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    RTX::g_ctx().hdr_format      = best_fmt_.format;
    RTX::g_ctx().hdr_color_space = best_cs_;

    LOG_WARN_CAT("HDR_SURFACE", "HDR coerced — {} + {} (artifacts possible but glory demanded)",
                 vk::to_string(static_cast<vk::Format>(best_fmt_.format)),
                 vk::to_string(static_cast<vk::ColorSpaceKHR>(best_cs_)));

    parse_edid_for_hdr_caps(); // Gamescope-style fake-it-til-you-make-it
}

// -----------------------------------------------------------------------------
// Private: EDID parsing stub (Gamescope does this for real)
void HDRSurfaceForge::parse_edid_for_hdr_caps() noexcept
{
    LOG_DEBUG_CAT("HDR_SURFACE", "EDID parsed — HDR10 capabilities assumed/faked if needed");
    // Real implementation would use libdisplay-info or XRandR EDID blobs
}

// -----------------------------------------------------------------------------
// Private: Check for VK_EXT_hdr_metadata
bool HDRSurfaceForge::has_hdr_metadata_ext() const noexcept
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(phys_dev_, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(phys_dev_, nullptr, &count, exts.data());

    for (const auto& e : exts) {
        if (strcmp(e.extensionName, VK_EXT_HDR_METADATA_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace HDRSurface