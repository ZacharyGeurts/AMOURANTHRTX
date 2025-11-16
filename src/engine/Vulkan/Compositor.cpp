// src/engine/Vulkan/Compositor.cpp
// INVISIBLE HDR — FULLY AUTOMATIC — NO DRM, NO HEADERS, NO BULLSHIT
// Works perfectly with your existing SwapchainManager — just drop in and compile

#include "engine/Vulkan/Compositor.hpp"
#include "engine/GLOBAL/StoneKey.hpp"      // g_surface()
#include "engine/GLOBAL/RTXHandler.hpp"     // RTX::g_ctx()
#include "engine/GLOBAL/OptionsMenu.hpp"    // Options::Display::ENABLE_HDR
#include "engine/GLOBAL/logging.hpp"

#include <vector>
#include <algorithm>

namespace HDRCompositor {

static bool g_hdr_active = false;

bool try_enable_invisible_hdr() noexcept {
    if (g_hdr_active) return true;

    VkPhysicalDevice phys = RTX::g_ctx().physicalDevice_;
    VkSurfaceKHR surface = g_surface();
    if (surface == VK_NULL_HANDLE) return false;

    // Query all surface formats
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &format_count, nullptr);
    if (format_count == 0) return false;

    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &format_count, formats.data());

    // Look for any 10-bit HDR format your SwapchainManager already prefers
    const VkFormat desired_formats[] = {
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        VK_FORMAT_R16G16B16A16_SFLOAT
    };

    const VkColorSpaceKHR desired_spaces[] = {
        VK_COLOR_SPACE_HDR10_ST2084_EXT,
        VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
        VK_COLOR_SPACE_DOLBYVISION_EXT,
        VK_COLOR_SPACE_HDR10_HLG_EXT
    };

    for (VkFormat fmt : desired_formats) {
        for (VkColorSpaceKHR cs : desired_spaces) {
            auto it = std::find_if(formats.begin(), formats.end(),
                [fmt, cs](const VkSurfaceFormatKHR& f) {
                    return f.format == fmt && f.colorSpace == cs;
                });

            if (it != formats.end()) {
                // HDR IS AVAILABLE — force it on
                const_cast<bool&>(Options::Display::ENABLE_HDR) = true;

                // Store in global context so SwapchainManager picks it up
                RTX::g_ctx().hdr_format      = fmt;
                RTX::g_ctx().hdr_color_space = cs;

                g_hdr_active = true;

                LOG_SUCCESS_CAT("HDR",
                    "INVISIBLE HDR ACTIVATED — {} + {} — PINK PHOTONS ASCEND",
                    fmt == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ? "A2B10G10R10" :
                    fmt == VK_FORMAT_A2R10G10B10_UNORM_PACK32 ? "A2R10G10B10" : "FP16 scRGB",
                    cs == VK_COLOR_SPACE_HDR10_ST2084_EXT ? "HDR10 PQ" :
                    cs == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT ? "scRGB" : "Dolby/Other");

                return true;
            }
        }
    }

    LOG_INFO_CAT("HDR", "No true HDR surface format found — staying in peasant sRGB");
    return false;
}

bool is_hdr_active() noexcept {
    return g_hdr_active;
}

} // namespace HDRCompositor