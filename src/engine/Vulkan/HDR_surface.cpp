// src/engine/Vulkan/HDR_surface.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary "gzac" Geurts — PINK PHOTONS ETERNAL
// HDR SURFACE FORGE IMPLEMENTATION — 2025 Standard — GBM Direct + Platform Fallback
// RELAXED: Fallback to 8-bit if HDR not supported; numeric enums for compatibility
// =============================================================================

#include "engine/Vulkan/HDR_surface.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"  // For RTX::g_ctx() and g_context_instance
#include "engine/Vulkan/VulkanCore.hpp"  // For VK_CHECK macro
#include "engine/GLOBAL/logging.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>  // For core types
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#ifdef __linux__
#include <gbm.h>
#include <xf86drm.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>  // strcmp
#endif

// RELAXED: Numeric defines for compatibility (if headers incomplete/old)
#ifndef VK_FORMAT_R10G10B10A2_UNORM
#define VK_FORMAT_R10G10B10A2_UNORM 30
#endif
#ifndef VK_COLOR_SPACE_HDR10_ST2084_EXT
#define VK_COLOR_SPACE_HDR10_ST2084_EXT 1000102003
#endif
#ifdef __linux__
#ifndef GBM_FORMAT_XRGB2101010
#define GBM_FORMAT_XRGB2101010 __gbm_fourcc_code('X', 'R', '3', '0')
#endif
#endif

// =============================================================================
// ACCEPT_8BIT FLAG — RESPECT THE ENVIRONMENT
// =============================================================================
namespace {
    bool ACCEPT_8BIT = false;  // Default: Attempt HDR; set via env var or config
}

// Optional: Set from environment (e.g., ACCEPT_8BIT=1 for 8-bit fallback)
void initAccept8Bit() {
    const char* env = std::getenv("ACCEPT_8BIT");
    if (env) {
        ACCEPT_8BIT = (std::strcmp(env, "1") == 0 || std::strcmp(env, "true") == 0 || std::strcmp(env, "yes") == 0);
        LOG_INFO_CAT("HDRSurface", "ACCEPT_8BIT set to {} from environment", ACCEPT_8BIT ? "true" : "false");
    } else {
        LOG_DEBUG_CAT("HDRSurface", "ACCEPT_8BIT default: false (attempt HDR)");
    }
}

namespace HDRSurface {

thread_local HDRSurfaceForge* g_hdr_forge = nullptr;

HDRSurfaceForge::HDRSurfaceForge(VkInstance instance, VkPhysicalDevice phys_dev, uint32_t width, uint32_t height)
    : instance_(instance), phys_dev_(phys_dev), width_(width), height_(height) {
    initAccept8Bit();  // Initialize flag once

    std::lock_guard<std::mutex> lock(forge_mutex_);

    bool success = false;
#ifdef __linux__
    if (create_gbm_direct_surface()) {
        success = true;
        LOG_INFO_CAT("HDRSurface", "GBM direct surface forged successfully");
    } else {
#endif
        success = create_platform_surface();
        if (success) LOG_INFO_CAT("HDRSurface", "Platform surface forged successfully");
#ifdef __linux__
    }
#endif

    if (success) {
        probe_formats();
        LOG_SUCCESS_CAT("HDRSurface", "{}HDR probe complete — Format: {} | Space: {} | HDR: {}{}",
                        PLASMA_FUCHSIA,
                        static_cast<int>(best_fmt_.format),
                        static_cast<int>(best_fmt_.colorSpace),
                        is_hdr() ? " ENABLED" : " DISABLED (8-bit fallback)",
                        RESET);
    } else {
        LOG_ERROR_CAT("HDRSurface", "Surface creation failed — fallback to basic");
        best_fmt_.format = VK_FORMAT_R8G8B8A8_SRGB;
        best_fmt_.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
}

HDRSurfaceForge::~HDRSurfaceForge() {
    std::lock_guard<std::mutex> lock(forge_mutex_);

#ifdef __linux__
    if (gbm_surface_) {
        gbm_surface_destroy(gbm_surface_);
        gbm_surface_ = nullptr;
        LOG_TRACE_CAT("HDRSurface", "GBM surface destroyed");
    }
    if (gbm_device_) {
        gbm_device_destroy(gbm_device_);
        gbm_device_ = nullptr;
        LOG_TRACE_CAT("HDRSurface", "GBM device destroyed");
    }
    if (drm_fd_ != -1) {
        close(drm_fd_);
        drm_fd_ = -1;
        LOG_TRACE_CAT("HDRSurface", "DRM FD closed");
    }
#endif

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
        LOG_TRACE_CAT("HDRSurface", "VkSurfaceKHR destroyed");
    }
}

bool HDRSurfaceForge::create_gbm_direct_surface() noexcept {
#ifdef __linux__
    // Open primary DRM device (respect environment: /dev/dri/card0 or env var)
    const char* drm_dev = std::getenv("DRM_DEVICE") ? std::getenv("DRM_DEVICE") : "/dev/dri/card0";
    drm_fd_ = open(drm_dev, O_RDWR | O_CLOEXEC);
    if (drm_fd_ < 0) {
        LOG_WARN_CAT("HDRSurface", "Failed to open DRM device {}: {}", drm_dev, strerror(errno));
        return false;
    }

    // Create GBM device
    gbm_device_ = gbm_create_device(drm_fd_);
    if (!gbm_device_) {
        LOG_WARN_CAT("HDRSurface", "Failed to create GBM device");
        close(drm_fd_);
        drm_fd_ = -1;
        return false;
    }

    // Select format: Respect ACCEPT_8BIT — force 8-bit if true, else prefer HDR-capable
    uint32_t gbm_format = GBM_FORMAT_XRGB8888;
    gbm_hdr_ = false;
    if (!ACCEPT_8BIT) {
        // Attempt 10-bit HDR format (e.g., XRGB2101010 for PQ/HDR10)
        if (gbm_device_is_format_supported(gbm_device_, GBM_FORMAT_XRGB2101010,
                                           GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING)) {
            gbm_format = GBM_FORMAT_XRGB2101010;
            gbm_hdr_ = true;
            LOG_INFO_CAT("HDRSurface", "10-bit HDR GBM format supported & selected");
        } else {
            LOG_WARN_CAT("HDRSurface", "10-bit HDR format unsupported — fallback to 8-bit");
        }
    } else {
        LOG_INFO_CAT("HDRSurface", "ACCEPT_8BIT=true: Forcing 8-bit GBM format to respect environment");
    }

    // Create GBM surface for scanout/rendering
    gbm_surface_ = gbm_surface_create(gbm_device_, width_, height_, gbm_format,
                                      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!gbm_surface_) {
        LOG_WARN_CAT("HDRSurface", "Failed to create GBM surface: {}", strerror(errno));
        gbm_device_destroy(gbm_device_);
        gbm_device_ = nullptr;
        close(drm_fd_);
        drm_fd_ = -1;
        return false;
    }

    // Forge VkSurfaceKHR via VK_KHR_display (direct KMS, no window system)
    // 1. Enumerate displays
    uint32_t display_count = 0;
    VK_CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(phys_dev_, &display_count, nullptr),
             "Failed to query display count");
    if (display_count == 0) {
        LOG_WARN_CAT("HDRSurface", "No displays available via VK_KHR_display");
        return false;
    }
    std::vector<VkDisplayPropertiesKHR> displays(display_count);
    VK_CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(phys_dev_, &display_count, displays.data()),
             "Failed to query displays");

    VkDisplayKHR primary_display = displays[0].display;  // Use primary

    // 2. Enumerate display modes
    uint32_t mode_count = 0;
    VK_CHECK(vkGetDisplayModePropertiesKHR(phys_dev_, primary_display, &mode_count, nullptr),
             "Failed to query mode count");
    std::vector<VkDisplayModePropertiesKHR> modes(mode_count);
    VK_CHECK(vkGetDisplayModePropertiesKHR(phys_dev_, primary_display, &mode_count, modes.data()),
             "Failed to query modes");

    // Find matching mode (width/height)
    VkDisplayModeKHR target_mode = VK_NULL_HANDLE;
    for (const auto& mode_prop : modes) {
        const VkDisplayModeParametersKHR& params = mode_prop.parameters;
        if (params.visibleRegion.width == width_ && params.visibleRegion.height == height_) {
            target_mode = mode_prop.displayMode;
            break;
        }
    }
    if (target_mode == VK_NULL_HANDLE) {
        LOG_WARN_CAT("HDRSurface", "No matching display mode for {}x{}", width_, height_);
        return false;
    }

    // 3. Enumerate planes
    uint32_t plane_count = 0;
    VK_CHECK(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(phys_dev_, &plane_count, nullptr),
             "Failed to query plane count");
    std::vector<VkDisplayPlanePropertiesKHR> planes(plane_count);
    VK_CHECK(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(phys_dev_, &plane_count, planes.data()),
             "Failed to query planes");

    // Find plane supporting the display
    uint32_t plane_idx = UINT32_MAX;
    for (uint32_t i = 0; i < plane_count; ++i) {
        uint32_t supported_display_count = 0;
        VK_CHECK(vkGetDisplayPlaneSupportedDisplaysKHR(phys_dev_, i, &supported_display_count, nullptr),
                 "Failed to query supported displays for plane");
        if (supported_display_count > 0) {
            std::vector<VkDisplayKHR> supported_displays(supported_display_count);
            VK_CHECK(vkGetDisplayPlaneSupportedDisplaysKHR(phys_dev_, i, &supported_display_count, supported_displays.data()),
                     "Failed to query supported displays");
            if (std::find(supported_displays.begin(), supported_displays.end(), primary_display) != supported_displays.end()) {
                plane_idx = i;
                break;
            }
        }
    }
    if (plane_idx == UINT32_MAX) {
        LOG_WARN_CAT("HDRSurface", "No suitable plane for display");
        return false;
    }

    // 4. Create display plane surface
    VkDisplaySurfaceCreateInfoKHR create_info{VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR};
    create_info.displayMode = target_mode;
    create_info.planeIndex = plane_idx;
    create_info.planeStackIndex = 0;
    create_info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    create_info.globalAlpha = 1.0f;
    create_info.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
    create_info.imageExtent = {width_, height_};

    VkResult res = vkCreateDisplayPlaneSurfaceKHR(instance_, &create_info, nullptr, &surface_);
    if (res != VK_SUCCESS) {
        LOG_WARN_CAT("HDRSurface", "Failed to create display plane surface: {}", res);
        return false;
    }

    forced_hdr_ = gbm_hdr_;
    is_gbm_direct_ = true;
    LOG_SUCCESS_CAT("HDRSurface", "GBM direct VkSurfaceKHR forged — HDR: {} (10-bit: {})",
                    forced_hdr_ ? "ENABLED" : "DISABLED", gbm_hdr_);
    return true;
#else
    LOG_WARN_CAT("HDRSurface", "GBM direct not supported on this platform");
    return false;
#endif
}

bool HDRSurfaceForge::create_platform_surface() noexcept {
    // Fallback: Use SDL for windowed surface (assumes SDL_Window* g_sdl_window exists globally)
    extern SDL_Window* g_sdl_window;  // From main/SDL setup
    if (!g_sdl_window) {
        LOG_ERROR_CAT("HDRSurface", "No global SDL window for platform fallback");
        return false;
    }

    VkSurfaceKHR temp_surface = VK_NULL_HANDLE;
    bool success = SDL_Vulkan_CreateSurface(g_sdl_window, instance_, nullptr, &temp_surface);
    if (!success || temp_surface == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("HDRSurface", "SDL_Vulkan_CreateSurface failed");
        return false;
    }
    surface_ = temp_surface;

    LOG_INFO_CAT("HDRSurface", "SDL platform VkSurfaceKHR forged");
    return true;
}

bool HDRSurfaceForge::has_hdr_metadata_ext() const noexcept {
    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(phys_dev_, nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> extensions(ext_count);
    vkEnumerateDeviceExtensionProperties(phys_dev_, nullptr, &ext_count, extensions.data());

    for (const auto& ext : extensions) {
        if (std::strcmp(ext.extensionName, VK_EXT_HDR_METADATA_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

void HDRSurfaceForge::probe_formats() noexcept {
    // Query supported surface formats
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev_, surface_, &format_count, nullptr);
    if (format_count == 0) {
        LOG_WARN_CAT("HDRSurface", "No surface formats available");
        return;
    }

    std::vector<VkSurfaceFormatKHR> available_formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev_, surface_, &format_count, available_formats.data());

    // Respect ACCEPT_8BIT: Prioritize 8-bit sRGB if true, else seek HDR (e.g., 10-bit PQ)
    VkFormat fallback_format = VK_FORMAT_R8G8B8A8_SRGB;
    VkColorSpaceKHR fallback_cs = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    best_fmt_.format = available_formats[0].format;
    best_fmt_.colorSpace = available_formats[0].colorSpace;  // Default

    if (available_formats[0].format == VK_FORMAT_UNDEFINED) {
        // All formats supported — choose explicitly
        if (ACCEPT_8BIT) {
            best_fmt_.format = fallback_format;
            best_fmt_.colorSpace = fallback_cs;
            LOG_INFO_CAT("HDRSurface", "ACCEPT_8BIT=true: Forced 8-bit sRGB format");
        } else if (has_hdr_metadata_ext()) {
            // Prefer HDR10 (PQ) with 10-bit format
            best_fmt_.format = static_cast<VkFormat>(VK_FORMAT_R10G10B10A2_UNORM);
            best_fmt_.colorSpace = static_cast<VkColorSpaceKHR>(VK_COLOR_SPACE_HDR10_ST2084_EXT);
            LOG_INFO_CAT("HDRSurface", "HDR metadata ext available: Selected 10-bit PQ");
        } else {
            best_fmt_.format = fallback_format;
            best_fmt_.colorSpace = fallback_cs;
            LOG_INFO_CAT("HDRSurface", "No HDR ext: Fallback to 8-bit sRGB");
        }
    } else {
        // Select from available: Prefer sRGB 8-bit if ACCEPT_8BIT, else HDR if present
        VkSurfaceFormatKHR preferred = {fallback_format, fallback_cs};
        bool found_preferred = false;

        for (const auto& fmt : available_formats) {
            if (fmt.format == VK_FORMAT_R8G8B8A8_SRGB) {
                preferred = fmt;
                found_preferred = true;
                break;
            }
        }

        if (ACCEPT_8BIT || !found_preferred) {
            best_fmt_ = preferred;
        } else {
            // Seek HDR color space (requires VK_EXT_swapchain_colorspace)
            // Assume if 10-bit format present, use HDR space
            for (const auto& fmt : available_formats) {
                if (fmt.format == static_cast<VkFormat>(VK_FORMAT_R10G10B10A2_UNORM)) {
                    best_fmt_.format = fmt.format;
                    best_fmt_.colorSpace = static_cast<VkColorSpaceKHR>(VK_COLOR_SPACE_HDR10_ST2084_EXT);
                    LOG_INFO_CAT("HDRSurface", "10-bit format found: Using HDR color space");
                    break;
                }
            }
        }
    }

    LOG_DEBUG_CAT("HDRSurface", "Best format selected: {} / color space: {} (HDR: {})",
                  static_cast<int>(best_fmt_.format),
                  static_cast<int>(best_fmt_.colorSpace),
                  is_hdr() ? "yes" : "no");
}

bool HDRSurfaceForge::is_hdr() const noexcept {
    return best_fmt_.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
}

void HDRSurfaceForge::install_to_ctx() noexcept {
    set_hdr_surface(this);
    // Integrate with RTX context (e.g., override surface in g_context_instance)
    RTX::g_context_instance.surface_ = surface_;
    LOG_SUCCESS_CAT("HDRSurface", "HDR forge installed to RTX context — Surface: 0x{:x}", reinterpret_cast<uintptr_t>(surface_));
}

void HDRSurfaceForge::reprobe() noexcept {
    probe_formats();
    LOG_INFO_CAT("HDRSurface", "Surface formats reprobed — HDR: {}", is_hdr() ? "enabled" : "disabled");
}

void HDRSurfaceForge::set_hdr_metadata(VkSwapchainKHR swapchain, float max_lum, float min_lum) {
    if (!has_hdr_metadata_ext() || !is_hdr()) {
        LOG_WARN_CAT("HDRSurface", "HDR metadata not applicable (ext missing or not HDR mode)");
        return;
    }

    VkHdrMetadataEXT metadata{VK_STRUCTURE_TYPE_HDR_METADATA_EXT};
    // Standard Rec. 2020 primaries & D65 white point for HDR10
    metadata.displayPrimaryRed.x = 0.708f;     metadata.displayPrimaryRed.y = 0.292f;
    metadata.displayPrimaryGreen.x = 0.170f;   metadata.displayPrimaryGreen.y = 0.797f;
    metadata.displayPrimaryBlue.x = 0.131f;    metadata.displayPrimaryBlue.y = 0.046f;
    metadata.whitePoint.x = 0.3127f;           metadata.whitePoint.y = 0.3290f;
    metadata.maxLuminance = max_lum;           // e.g., 1000 nits
    metadata.minLuminance = min_lum;           // e.g., 0.001 nits
    metadata.maxContentLightLevel = 1000.0f;   // Max scene light
    metadata.maxFrameAverageLightLevel = 400.0f;  // Avg frame light

    auto device = RTX::g_ctx().device();
    auto pfn = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(vkGetDeviceProcAddr(device, "vkSetHdrMetadataEXT"));
    if (pfn) {
        pfn(device, 1, &swapchain, &metadata);
        LOG_SUCCESS_CAT("HDRSurface", "HDR metadata set on swapchain — Max: {:.1f} nits, Min: {:.3f} nits",
                        max_lum, min_lum);
    } else {
        LOG_WARN_CAT("HDRSurface", "vkSetHdrMetadataEXT unavailable — metadata injection skipped");
    }
}

}  // namespace HDRSurface