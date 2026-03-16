#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
//
// Main engine entry point — called from developer's empty main.cpp
// =============================================================================

#include "engine/Camera.hpp"
#include "engine/SDL3.hpp"
#include "engine/ELLIE.hpp"
#include "engine/AMOURANTHRTX.hpp"
#include "engine/OptionsMenu.hpp"
#include "engine/RayCanvas.hpp"
#include "engine/Pipeline.hpp"

#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>

#include <memory>
#include <format>
#include <chrono>

// Global canvas
inline std::unique_ptr<RayCanvas> raycanvas;

// Sacrificial Splash — skippable with any input, plays single WAV audio
static inline void showSacrificialSplash() noexcept {
    constexpr int W = 1280, H = 720;
    constexpr const char* TITLE = "AMOURANTHRTX";

    SDL_Window* splashWin = SDL_CreateWindow(
        TITLE, W, H, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN
    );
    if (!splashWin) {
        LOG_ERROR_CAT("SPLASH", "Create splash window failed: {}", SDL_GetError());
        return;
    }

    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(0, &bounds) == 0) {
        SDL_SetWindowPosition(splashWin,
                              bounds.x + (bounds.w - W)/2,
                              bounds.y + (bounds.h - H)/2);
    }

    const char* iconPaths[] = {"assets/textures/ammo.ico", nullptr};
    for (int i = 0; iconPaths[i]; ++i) {
        if (SDL_Surface* surf = IMG_Load(iconPaths[i])) {
            SDL_SetWindowIcon(splashWin, surf);
            SDL_DestroySurface(surf);
            break;
        }
    }

    SDL_Renderer* splashRen = SDL_CreateRenderer(splashWin, nullptr);
    if (!splashRen) {
        LOG_ERROR_CAT("SPLASH", "Renderer create failed: {}", SDL_GetError());
        SDL_DestroyWindow(splashWin);
        return;
    }

    SDL_Texture* tex = nullptr;
    const char* texPaths[] = {"assets/textures/ammo.png", nullptr};
    for (int i = 0; texPaths[i]; ++i) {
        if (SDL_Surface* surf = IMG_Load(texPaths[i])) {
            tex = SDL_CreateTextureFromSurface(splashRen, surf);
            SDL_DestroySurface(surf);
            if (tex) break;
        }
    }

    if (tex) {
        float tw = 0, th = 0;
        SDL_GetTextureSize(tex, &tw, &th);
        SDL_FRect dst{ (W - tw)*0.5f, (H - th)*0.5f, tw, th };
        SDL_SetRenderDrawColor(splashRen, 0, 0, 0, 255);
        SDL_RenderClear(splashRen);
        SDL_RenderTexture(splashRen, tex, nullptr, &dst);
    } else {
        SDL_SetRenderDrawColor(splashRen, 255, 20, 147, 255);
        SDL_RenderClear(splashRen);
    }

    SDL_ShowWindow(splashWin);
    SDL_RenderPresent(splashRen);

    // Play splash sound (will use whatever free slot the dynamic system assigns)
    SDL3System::get().playSound("assets/audio/splash.wav", "play");

    // Visual timeout + input skip (~3.4s max)
    double start = TotalTime::get().seconds();
    bool skip = false;

    while (TotalTime::get().seconds() - start < 3.0 && !skip) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            SDL3System::get().pump(e);

            if (e.type == SDL_EVENT_QUIT ||
                e.type == SDL_EVENT_KEY_DOWN ||
                e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
                skip = true;
                break;
            }
        }

        SDL_Delay(1);
    }

    // Stop ALL currently playing tracks (dynamic system — no fixed slot count)
    size_t playing_count = SDL3System::get().getPlayingCount();
    if (playing_count > 0) {
        LOG_INFO_CAT("SPLASH", "Stopping {} lingering audio track(s)", playing_count);
        // Since we don't track which slot the splash used, safest is to stop everything
        for (size_t i = 0; i < SDL3System::get().getActiveSlotCount(); ++i) {
            if (SDL3System::get().isTrackPlaying(static_cast<int>(i))) {
                SDL3System::get().playSound("", "stop", static_cast<int>(i));
            }
        }
    }

    // Cleanup
    if (tex) SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(splashRen);
    SDL_DestroyWindow(splashWin);

    LOG_SUCCESS_CAT("SPLASH", "Sacrificial splash completed (skipped={})", skip ? "yes" : "no");
}

// Main entry point
inline int navigator_main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    install_apocalypse_handler();
    Logging::Logger::get().startup();
    LOG_SUCCESS_CAT("MAIN", "Apocalypse handler & logger ready");

    // Window — uses Options::SDL3 defaults
    SDL_Window* window = SDL_CreateWindow(
        "AMOURANTHRTX",
        Options::SDL3::DefaultWidth,
        Options::SDL3::DefaultHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (!window) {
        LOG_FATAL_CAT("SDL3", "Window creation failed: {}", SDL_GetError());
        return 1;
    }

    // SDL3 init (this also preloads all files from Options::SDL3::PreloadedAudioFiles)
    if (!SDL3System::get().init(window)) {
        LOG_FATAL_CAT("SDL3", "SDL3.init failed");
        SDL_DestroyWindow(window);
        return 1;
    }

    showSacrificialSplash();

    // Vulkan setup
    VkInstance instance = createVulkanInstance();
    if (instance == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("VULKAN", "Instance creation failed");
        SDL3System::get().shutdown();
        SDL_DestroyWindow(window);
        return 1;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface) == 0) {
        LOG_FATAL_CAT("VULKAN", "Failed to create Vulkan surface");
        vkDestroyInstance(instance, nullptr);
        SDL3System::get().shutdown();
        SDL_DestroyWindow(window);
        return 1;
    }

    uint32_t graphics_family = 0, present_family = 0;
    uint32_t compute_family = 0, transfer_family = 0;

    VkDevice device = createLogicalDeviceAndSelectGPU(instance, surface,
                                                      &graphics_family, &present_family,
                                                      &compute_family, &transfer_family);
    if (device == VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL3System::get().shutdown();
        SDL_DestroyWindow(window);
        return 1;
    }

    VkQueue graphics_queue{}, present_queue{}, compute_queue{}, transfer_queue{};
    vkGetDeviceQueue(device, graphics_family,   0, &graphics_queue);
    vkGetDeviceQueue(device, present_family,    0, &present_queue);
    vkGetDeviceQueue(device, compute_family,    0, &compute_queue);
    vkGetDeviceQueue(device, transfer_family,   0, &transfer_queue);

    rtx().instance         = instance;
    rtx().device           = device;
    rtx().surface          = surface;
    rtx().graphics_queue   = graphics_queue;
    rtx().present_queue    = present_queue;
    rtx().compute_queue    = compute_queue;
    rtx().transfer_queue   = transfer_queue;
    rtx().graphics_family  = graphics_family;
    rtx().present_family   = present_family;
    rtx().transfer_family  = transfer_family;
    rtx().compute_family   = compute_family;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = rtx().graphics_family;

    vkCreateCommandPool(device, &poolInfo, nullptr, &rtx().transient_pool);
    LOG_SUCCESS_CAT("VULKAN", "Transient command pool created");

    Swapchain::create(window, Options::SDL3::DefaultWidth, Options::SDL3::DefaultHeight);

    LOG_INFO_CAT("MAIN", "Swapchain ready — {}x{}", 
                 Swapchain::getExtent().width, Swapchain::getExtent().height);

    raycanvas = std::make_unique<RayCanvas>(
        Options::SDL3::DefaultWidth,
        Options::SDL3::DefaultHeight,
        window
    );

    // Main loop — everything flows from the sealed eternal clock
    while (true) {

        raycanvas->maybeUpdateCanvas();

        if (raycanvas->isDestroyed()) {
            LOG_INFO_CAT("MAIN", "Canvas destroyed — exiting main loop");
            break;
        }
    }

    raycanvas.reset();
    Pipeline::shutdown();
    SDL3System::get().shutdown();
    SDL_DestroyWindow(window);

    LOG_SUCCESS_CAT("MAIN", "Engine shutdown complete");
    return 0;
}