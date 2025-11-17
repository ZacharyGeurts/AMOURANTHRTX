// include/engine/Vulkan/HDR_surface.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary "gzac" Geurts — PINK PHOTONS ETERNAL
// HDR SURFACE FORGE — 2025 Standard — GBM Direct + Platform Fallback
// =============================================================================

#pragma once

#include <vulkan/vulkan.hpp>
#include <mutex>

#ifdef __linux__
struct gbm_device;
struct gbm_surface;
#endif

namespace HDRSurface {

class HDRSurfaceForge {
public:
    HDRSurfaceForge(VkInstance instance, VkPhysicalDevice phys_dev, uint32_t width, uint32_t height);
    ~HDRSurfaceForge();

    [[nodiscard]] VkSurfaceKHR surface() const noexcept { return surface_; }
    [[nodiscard]] VkSurfaceFormatKHR best_format() const noexcept { return best_fmt_; }
    [[nodiscard]] VkColorSpaceKHR best_color_space() const noexcept { return best_fmt_.colorSpace; }

    [[nodiscard]] bool forged_success() const noexcept { return surface_ != VK_NULL_HANDLE || is_gbm_direct_; }
    [[nodiscard]] bool is_hdr() const noexcept;
    [[nodiscard]] bool is_gbm_direct() const noexcept { return is_gbm_direct_; }
    [[nodiscard]] bool is_forced_hdr() const noexcept { return forced_hdr_; }

    void install_to_ctx() noexcept;
    void reprobe() noexcept;
    void set_hdr_metadata(VkSwapchainKHR swapchain, float max_lum = 1000.0f, float min_lum = 0.001f);

private:
    bool create_gbm_direct_surface() noexcept;
    bool create_platform_surface() noexcept;
    bool has_hdr_metadata_ext() const noexcept;
    void probe_formats() noexcept;

    VkInstance       instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice phys_dev_  = VK_NULL_HANDLE;
    VkSurfaceKHR     surface_   = VK_NULL_HANDLE;

    uint32_t width_  = 0;
    uint32_t height_ = 0;

    VkSurfaceFormatKHR best_fmt_{};
    bool is_gbm_direct_ = false;
    bool forced_hdr_ = false;

#ifdef __linux__
    int                 drm_fd_       = -1;
    struct gbm_device*  gbm_device_   = nullptr;
    struct gbm_surface* gbm_surface_  = nullptr;
    bool gbm_hdr_ = false;  // True if 10-bit GBM format
#endif

    static inline std::mutex forge_mutex_;
};

extern thread_local HDRSurfaceForge* g_hdr_forge;

inline HDRSurfaceForge* g_hdr_surface() noexcept { return g_hdr_forge; }
inline void set_hdr_surface(HDRSurfaceForge* forge) noexcept { g_hdr_forge = forge; }

} // namespace HDRSurface