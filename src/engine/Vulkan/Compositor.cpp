// src/engine/Vulkan/Compositor.cpp
// CROSS-PLATFORM HDR COMPOSITOR — FINAL HONEST EDITION
// November 17, 2025 — No lies. No illusions. Only truth about the photons.

#include "engine/Vulkan/Compositor.hpp"
#include "engine/Vulkan/HDR_surface.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <algorithm>
#include <string>
#include <mutex>
#include <cstdio>
#include <format>
#include <print>

#ifdef __linux__
#include <cstdlib>
#include <sys/utsname.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#undef Success
#elif _WIN32
#include <windows.h>
#include <stdlib.h>
#endif

namespace HDRCompositor {

// ————————————————————————————————————————————————————————————————
// THE ONE TRUE TOGGLE — RESPECTED EVERYWHERE
// Change to false when you're ready to reject 8-bit forever
static constexpr bool ACCEPT_8BIT = false;
// ————————————————————————————————————————————————————————————————

static bool g_hdr_active = false;
static bool g_hdr_extension_loaded = false;
static std::mutex g_init_mutex;

struct HDRStatus {
    bool platform_detected   = false;
    bool vendor_override     = false;
    bool env_boost           = false;
    bool version_ok          = true;
    bool formats_query_ok    = false;
    bool preferred_found     = false;
    bool forced              = false;
    bool actually_10bit      = false;
    std::vector<std::string> found_formats;
    std::string final_fmt_str = "UNKNOWN";
    std::string final_cs_str  = "UNKNOWN";
    bool declared_active     = false;
};

bool has_hdr_extension(VkInstance instance) noexcept {
    if (g_hdr_extension_loaded != false) return g_hdr_extension_loaded;

    uint32_t ext_count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr) != VK_SUCCESS) {
        g_hdr_extension_loaded = false;
        return false;
    }
    std::vector<VkExtensionProperties> exts(ext_count);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, exts.data()) != VK_SUCCESS) {
        g_hdr_extension_loaded = false;
        return false;
    }
    g_hdr_extension_loaded = std::any_of(exts.begin(), exts.end(),
        [](const VkExtensionProperties& e) { return strcmp(e.extensionName, "VK_KHR_hdr_metadata") == 0; });
    return g_hdr_extension_loaded;
}

bool set_env_safe(const std::string& name, const std::string& value) noexcept {
    std::string full = name + "=" + value;
#ifdef __linux__
    return putenv(const_cast<char*>(full.c_str())) == 0;
#elif _WIN32
    return _putenv_s(name.c_str(), value.c_str()) == 0;
#else
    return false;
#endif
}

void print_hdr_table(const HDRStatus& status,
                     bool is_linux, bool is_x11, bool is_wayland, bool is_windows,
                     const std::vector<VkSurfaceFormatKHR>& formats) noexcept
{
    std::ios::sync_with_stdio(false);
    auto yesno = [](bool b) { return b ? "YES" : "NO"; };

    std::println("\nHDR COMPOSITOR — FINAL VERDICT (November 17, 2025)");
    std::println("══════════════════════════════════════════════════════════════════════════════");
    std::println("  ACCEPT_8BIT TOGGLE    : {}", ACCEPT_8BIT ? "ENABLED (8-bit mercy granted)" : "DISABLED (10-bit or death)");
    std::println("  Platform              : Linux={}  X11={}  Wayland={}  Windows={}",
                 yesno(is_linux), yesno(is_x11), yesno(is_wayland), yesno(is_windows));
    std::println("  Vendor/Env Tweaks     : {}", yesno(status.vendor_override || status.env_boost));
    std::println("  Surface Formats       : {} ({} total)", yesno(status.formats_query_ok), formats.size());
    std::println("  Preferred 10-bit      : {}", yesno(status.preferred_found));
    std::println("  HDR Declared Active   : {}", yesno(status.declared_active));
    std::println("  Actual Pipeline       : {} / {}", status.final_fmt_str, status.final_cs_str);
    std::println("  10-BIT HDR ACHIEVED   : {}", status.actually_10bit ? "YES — PINK PHOTONS ETERNAL" : "NO — PEASANT 8-BIT SDR");
    std::println("  └─ Final Truth        : {}", 
                 status.actually_10bit ? "INFINITE LUMINANCE REIGNS" 
                                       : (ACCEPT_8BIT ? "8-bit accepted with dignity" : "REJECTED — DEATH BY 8-BIT"));
    std::println("══════════════════════════════════════════════════════════════════════════════\n");
    fflush(stdout);
}

bool try_enable_hdr() noexcept {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    if (g_hdr_active) return g_hdr_active;

    VkPhysicalDevice phys   = RTX::g_ctx().physicalDevice_;
    VkSurfaceKHR     surface = RTX::g_ctx().surface_;

    HDRStatus status;

    bool is_linux   = false;
    bool is_x11     = true;
    bool is_wayland = false;
    bool is_windows = false;
    [[maybe_unused]] bool is_mesa = false;  // suppresses -Wunused-variable

#ifdef __linux__
    is_linux = true;
    status.platform_detected = true;
    LOG_INFO_CAT("HDR", "PLATFORM: Linux detected — initiating compositor conquest");

    const char* xdg_session = std::getenv("XDG_SESSION_TYPE");
    if (xdg_session && std::string(xdg_session) == "wayland") {
        is_wayland = true;
        is_x11 = false;
    }
#elif defined(_WIN32)
    is_windows = true;
    status.platform_detected = true;
#endif

    // Query surface formats
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    if (count > 0 && vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, formats.data()) == VK_SUCCESS) {
        status.formats_query_ok = true;
        for (const auto& f : formats) {
            status.found_formats.emplace_back(vk::to_string(static_cast<vk::Format>(f.format)) + " / " + vk::to_string(static_cast<vk::ColorSpaceKHR>(f.colorSpace)));
        }
    }

    const VkFormat preferred[] = {
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        VK_FORMAT_R16G16B16A16_SFLOAT
    };

    VkFormat  chosen_fmt = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR chosen_cs  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    for (auto f : preferred) {
        auto it = std::find_if(formats.begin(), formats.end(),
            [f](const VkSurfaceFormatKHR& sf) { return sf.format == f; });
        if (it != formats.end()) {
            chosen_fmt = it->format;
            chosen_cs  = it->colorSpace;
            status.preferred_found = true;
            break;
        }
    }

    // FINAL JUDGMENT — RESPECT THE TOGGLE
    if (status.preferred_found) {
        status.actually_10bit = true;
    } else if (ACCEPT_8BIT) {
        if (!formats.empty()) {
            chosen_fmt = formats[0].format;
            chosen_cs  = formats[0].colorSpace;
        }
        status.actually_10bit = false;
    } else {
        LOG_FATAL_CAT("HDR", "No 10-bit format available and ACCEPT_8BIT = false — refusing to render in peasant mode");
        std::abort();
    }

    status.final_fmt_str   = vk::to_string(static_cast<vk::Format>(chosen_fmt));
    status.final_cs_str    = vk::to_string(static_cast<vk::ColorSpaceKHR>(chosen_cs));

    RTX::g_ctx().hdr_format       = chosen_fmt;
    RTX::g_ctx().hdr_color_space  = chosen_cs;
    const_cast<bool&>(Options::Display::ENABLE_HDR) = status.actually_10bit;  // FIXED
    g_hdr_active                  = status.actually_10bit;

    status.declared_active = true;

    print_hdr_table(status, is_linux, is_x11, is_wayland, is_windows, formats);

    return status.actually_10bit;
}

void force_hdr(VkFormat fmt, VkColorSpaceKHR cs) noexcept {
    RTX::g_ctx().hdr_format      = fmt;
    RTX::g_ctx().hdr_color_space = cs;
    const_cast<bool&>(Options::Display::ENABLE_HDR) = true;
    g_hdr_active                 = true;
}

void inject_hdr_metadata(VkDevice device, VkSwapchainKHR swapchain,
                         float maxCLL, float maxFALL) noexcept
{
    if (!g_hdr_active) return;

    static PFN_vkSetHdrMetadataEXT fn = nullptr;
    if (!fn) fn = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(
        vkGetDeviceProcAddr(device, "vkSetHdrMetadataEXT"));
    if (!fn) return;

    VkHdrMetadataEXT hdr{};
    hdr.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
    hdr.displayPrimaryRed   = {0.708f, 0.292f};
    hdr.displayPrimaryGreen = {0.170f, 0.797f};
    hdr.displayPrimaryBlue  = {0.131f, 0.046f};
    hdr.whitePoint          = {0.3127f, 0.3290f};
    hdr.maxLuminance        = maxCLL;
    hdr.minLuminance        = 0.0f;
    hdr.maxContentLightLevel     = maxCLL;
    hdr.maxFrameAverageLightLevel = maxFALL;

    fn(device, 1, &swapchain, &hdr);
}

bool is_hdr_active() noexcept { return g_hdr_active; }

} // namespace HDRCompositor