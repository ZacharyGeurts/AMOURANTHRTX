// =============================================================================
// include/engine/GLOBAL/SDL3.hpp
// AMOURANTH RTX Engine 2027 — QUANTUM PINK v∞ — APOCALYPSE ETERNAL
// MINIMAL SDL3 VIDEO SUBSYSTEM — PURE EMPIRE — FINAL FORM — NO BULLSHIT
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <memory>
#include <vector>

namespace Video {

void showSplashScreen(
    const char* title,
    int         width,
    int         height,
    const char* gpuName,
    const char* driverName,
    const char* vulkanVersionStr
) noexcept;

// -----------------------------------------------------------------------------
// RAII window
// -----------------------------------------------------------------------------
struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept { if (w) SDL_DestroyWindow(w); }
};
using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

// -----------------------------------------------------------------------------
// Core functions
// -----------------------------------------------------------------------------
bool init(const char* title = "AMOURANTH RTX — VALHALLA v∞",
          int width = 3840, int height = 2160,
          bool fullscreen = false);

void destroy() noexcept;
void getDrawableSize(int& w, int& h) noexcept;
bool pollEvents() noexcept;
[[nodiscard]] SDL_Window* getWindow() noexcept;
[[nodiscard]] VkSurfaceKHR createVulkanSurface(VkInstance instance) noexcept;
[[nodiscard]] std::vector<const char*> getRequiredVulkanInstanceExtensions() noexcept;
void presentFrame(VkImage rtImage, VkImageView rtView);
void recordBlitToSwapchain(VkCommandBuffer cmd, VkImage rtImage);
void presentFromRecorded();
bool setWindowIcon();
void showSplashScreen() noexcept;

} // namespace Video