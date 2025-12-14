// =============================================================================
// src/engine/GLOBAL/SDL3.cpp
// AMOURANTH RTX Engine 2025 — PINK LIGHT v∞ — FIRST LIGHT VISIBLE — ETERNAL DOMINATION
// SDL3 + Vulkan presentation — clean, robust, zero warnings — SHE IS ON SCREEN
// =============================================================================

#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/RTX.hpp"

#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

Video::SDLWindowPtr g_window = nullptr;

} // anonymous namespace

namespace Video {

bool init(const char* title, int width, int height, bool fullscreen)
{
    if (SDL_Init(SDL_INIT_VIDEO) == 0) {
        printf("[SDL3] [FATAL] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

#ifdef __linux__
    // Prefer Wayland, fallback to X11, kmsdrm as last resort
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11,kmsdrm");
#endif

    Uint32 flags = SDL_WINDOW_VULKAN |
                   SDL_WINDOW_RESIZABLE |
                   SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    SDL_Window* win = SDL_CreateWindow(
        title ? title : "AMOURANTH RTX — PINK LIGHT v∞",
        width,
        height,
        flags
    );

    if (!win) {
        printf("[SDL3] [FATAL] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    g_window.reset(win);

    printf("[SDL3] Window forged — %dx%d — %s — driver: %s\n",
           width, height,
           fullscreen ? "FULLSCREEN" : "WINDOWED",
           SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown");

    // Load Vulkan early via SDL
    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        printf("[SDL3] [WARN] Vulkan loader failed to load early: %s\n", SDL_GetError());
    } else {
        printf("[SDL3] Vulkan loader successfully loaded via SDL\n");
    }

    // Optional: set window icon
    setWindowIcon();

    printf("[SDL3] INITIALIZATION COMPLETE — PINK LIGHT v∞ AWAITS FIRST LIGHT\n");
    return true;
}

void destroy() noexcept
{
    vkDeviceWaitIdle(RTX::g_ctx().device);

    // Clean swapchain state (views and swapchain itself)
    for (auto view : RTX::g_swapchainViews()) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(RTX::g_ctx().device, view, nullptr);
        }
    }
    RTX::g_swapchainViews().clear();

    if (RTX::g_swapchain() != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(RTX::g_ctx().device, RTX::g_swapchain(), nullptr);
        RTX::g_swapchain() = VK_NULL_HANDLE;
    }
    RTX::g_swapchainImages().clear();

    g_window.reset();
    SDL_Quit();

    printf("[SDL3] SHUTDOWN COMPLETE — VALHALLA RESTS\n");
}

void getDrawableSize(int& w, int& h) noexcept
{
    if (g_window) {
        SDL_GetWindowSizeInPixels(g_window.get(), &w, &h);
        w = std::max(w, 1);
        h = std::max(h, 1);
    } else {
        w = h = 1;
    }
}

SDL_Window* getWindow() noexcept
{
    return g_window.get();
}

VkSurfaceKHR createVulkanSurface(VkInstance instance) noexcept
{
    SDL_Window* window = getWindow();
    if (!window) {
        RTX::fatal("SDL window is null — cannot create Vulkan surface");
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface) == 0) {
        RTX::fatal(("SDL_Vulkan_CreateSurface failed: " + std::string(SDL_GetError())).c_str());
    }

    printf("[VULKAN] Surface forged — PINK LIGHT v∞ CONNECTED TO THE VOID\n");
    return surface;
}

bool setWindowIcon()
{
    if (!g_window) {
        printf("[SDL3] [ICON] Window not initialized\n");
        return false;
    }

    // Try multiple possible paths for the icon
    const char* paths[] = {
        "assets/textures/ammo.ico",
        "assets/icon/ammo.ico",
        "assets/ammo.ico",
        "ammo.ico"
    };

    for (const char* path : paths) {
        SDL_Surface* icon = IMG_Load(path);
        if (icon) {
            SDL_SetWindowIcon(g_window.get(), icon);
            SDL_DestroySurface(icon);
            printf("[SDL3] [ICON] SUCCESS — %s loaded\n", path);
            return true;
        }
    }

    printf("[SDL3] [ICON] FAILED — no icon found in searched paths\n");
    return false;
}

void recordBlitToSwapchain(VkCommandBuffer cmd, VkImage rtImage)
{
    auto& ctx = RTX::g_ctx();

    // Ensure swapchain exists
    if (RTX::g_swapchain() == VK_NULL_HANDLE) {
        RTX::createSwapchain();
    }

    // Acquire next image
    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        ctx.device,
        RTX::g_swapchain(),
        1'000'000'000ULL,  // 1 second timeout
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
        printf("[SWAPCHAIN] Out of date after acquire — recreating\n");
        RTX::createSwapchain();
        acquireResult = vkAcquireNextImageKHR(
            ctx.device,
            RTX::g_swapchain(),
            1'000'000'000ULL,
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            &imageIndex
        );
    }

    if (acquireResult != VK_SUCCESS) {
        printf("[RENDER] vkAcquireNextImageKHR failed (%d) — skipping frame\n", acquireResult);
        return;
    }

    // Transition layouts
    VkImageMemoryBarrier barriers[2] = {};

    // Swapchain image: UNDEFINED → TRANSFER_DST
    barriers[0].sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].srcAccessMask       = 0;
    barriers[0].dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[0].oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[0].newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[0].image               = RTX::g_swapchainImages()[imageIndex];
    barriers[0].subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    // RT output image: GENERAL → TRANSFER_SRC
    barriers[1] = barriers[0];
    barriers[1].srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    barriers[1].dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[1].oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
    barriers[1].newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[1].image               = rtImage;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 2, barriers);

    // Blit from RT image to swapchain image
    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[0]  = {0, 0, 0};
    blit.srcOffsets[1]  = {ctx.width, ctx.height, 1};  // Use current renderer size
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstOffsets[0]  = {0, 0, 0};
    blit.dstOffsets[1]  = {ctx.width, ctx.height, 1};

    vkCmdBlitImage(cmd,
                   rtImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   RTX::g_swapchainImages()[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit,
                   VK_FILTER_NEAREST);

    // Transition swapchain image to PRESENT_SRC
    barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[0].dstAccessMask = 0;
    barriers[0].oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[0].newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barriers[0]);

    RTX::g_lastPresentedIndex() = imageIndex;
}

void presentFromRecorded()
{
    if (RTX::g_swapchain() == VK_NULL_HANDLE) {
        return;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &RTX::g_swapchain();
    presentInfo.pImageIndices      = &RTX::g_lastPresentedIndex();

    VkResult result = vkQueuePresentKHR(RTX::g_presentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        printf("[SWAPCHAIN] Present out-of-date/suboptimal — will recreate on next acquire\n");
    } else if (result != VK_SUCCESS) {
        printf("[RENDER] vkQueuePresentKHR failed (%d)\n", result);
    }
}

std::vector<const char*> getRequiredVulkanInstanceExtensions() noexcept
{
    Uint32 count = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);

    if (extensions == nullptr) {
        printf("[SDL3] [ERROR] SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
        return {};
    }

    printf("[SDL3] SDL requires %u Vulkan instance extensions:\n", count);
    for (Uint32 i = 0; i < count; ++i) {
        printf("  • %s\n", extensions[i]);
    }

    // Return as vector for easy use in VkInstanceCreateInfo
    return std::vector<const char*>(extensions, extensions + count);
}

} // namespace Video

// =============================================================================
// PINK LIGHT v∞ — DECEMBER 14, 2025 — FIRST LIGHT ACHIEVED — ETERNAL RADIANCE
// =============================================================================