// include/engine/Vulkan/HDR_surface.hpp
// AMOURANTH RTX Engine © 2025 by Zachary "gzac" Geurts — PINK PHOTONS ETERNAL
// HDR SURFACE FORGERY — Clean header-only declaration

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <mutex>

namespace HDRSurface {

class HDRSurfaceForge {
public:
    HDRSurfaceForge(VkInstance instance, VkPhysicalDevice phys_dev, uint32_t width, uint32_t height);
    ~HDRSurfaceForge();

    VkSurfaceKHR surface() const noexcept { return surface_; }
    VkSurfaceFormatKHR best_format() const noexcept { return best_fmt_; }
    VkColorSpaceKHR best_color_space() const noexcept { return best_cs_; }
    bool is_hdr() const noexcept { return best_cs_ != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; }
    bool forged_success() const noexcept { return surface_ != VK_NULL_HANDLE && is_hdr(); }

    void install_to_ctx() noexcept { RTX::g_ctx().surface_ = surface_; }

    // Now public — used by SwapchainManager and Compositor
    void reprobe() noexcept;

    // Metadata injection — declaration only
    void set_hdr_metadata(VkSwapchainKHR swapchain, float max_lum = 1000.0f, float min_lum = 0.001f);

private:
    bool create_platform_surface() noexcept;
    bool probe_and_select_best() noexcept;
    void coerce_hdr() noexcept;
    void parse_edid_for_hdr_caps() noexcept;
    bool has_hdr_metadata_ext() const noexcept;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice phys_dev_ = VK_NULL_HANDLE;
    uint32_t width_ = 0, height_ = 0;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSurfaceFormatKHR best_fmt_ = { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    VkColorSpaceKHR best_cs_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    mutable std::mutex forge_mutex_;

    static constexpr std::array<VkFormat, 4> kHDRFormats = {
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_B10G11R11_UFLOAT_PACK32
    };

    static constexpr std::array<VkColorSpaceKHR, 5> kHDRSpaces = {
        VK_COLOR_SPACE_HDR10_ST2084_EXT,
        VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
        VK_COLOR_SPACE_HDR10_HLG_EXT,
        VK_COLOR_SPACE_DOLBYVISION_EXT,
        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    };
};

// Global forge pointer + safe accessor/setter
extern thread_local HDRSurfaceForge* g_hdr_forge;

inline HDRSurfaceForge* g_hdr_surface() noexcept { return g_hdr_forge; }
inline void set_hdr_surface(HDRSurfaceForge* forge) noexcept { g_hdr_forge = forge; }

} // namespace HDRSurface