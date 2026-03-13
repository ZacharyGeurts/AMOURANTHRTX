#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
//
// Main engine entry point — called from developer's empty main.cpp
// Developers link against this header and call navigator_main(argc, argv)
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

// Global canvas — persistent compute-driven screen updater
inline std::unique_ptr<RayCanvas> raycanvas;

// Sacrificial Splash — skippable with any input, plays audio sequence
static inline void showSacrificialSplash() noexcept {
    constexpr int W = 1280, H = 720;
    constexpr const char* TITLE = "AMOURANTHRTX";

    SDL_Window* splashWin = SDL_CreateWindow(
        TITLE,
        W, H,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN
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

    // Try to set icon (silent fail OK)
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

    // Load splash texture (fallback hot pink)
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

    // ─────────────────────────────────────────────────────────────
    // Audio sequence using new SDL3.playSound API
    // ─────────────────────────────────────────────────────────────
    SDL3.playSound("assets/audio/splash.wav", "play");
    SDL3.playSound("assets/audio/splash.mp3", "play");
    SDL3.playSound("assets/audio/splash.mp3", "play");

    // Visual timeout + input skip (~3.4s max)
    double start = TotalTime::get().seconds();
    bool skip = false;

    while (TotalTime::get().seconds() - start < 3.4 && !skip) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            SDL3.pump(e);  // forward to input system
            if (e.type == SDL_EVENT_QUIT ||
                e.type == SDL_EVENT_KEY_DOWN ||
                e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
                skip = true;
                break;
            }
        }
    }

    // ── Cleanup ───────────────────────────────────────────────────
    if (tex) SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(splashRen);
    SDL_DestroyWindow(splashWin);

    LOG_SUCCESS_CAT("SPLASH", "Sacrificial splash completed (skipped={})", skip ? "yes" : "no");
}

// =============================================================================
// Engine-private memory initialization
// =============================================================================
static inline void EngineMemoryInit() noexcept {
    LOG_SUCCESS_CAT("MEMORY", "Engine memory ready (no global buffers needed for single-shader mode)");
}

// =============================================================================
// Main entry point — called from developer's empty main.cpp
// =============================================================================
inline int navigator_main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    install_apocalypse_handler();
    Logging::Logger::get().startup();
    LOG_SUCCESS_CAT("MAIN", "Apocalypse handler installed — logger started");

    // Step 1: Create window (Vulkan-ready)
    SDL_Window* window = SDL_CreateWindow(
        "AMOURANTHRTX",
        Options::Window::DEFAULT_WIDTH,
        Options::Window::DEFAULT_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (!window) {
        LOG_FATAL_CAT("SDL3", "Window creation failed: {}", SDL_GetError());
        return 1;
    }

    // Step 2: Initialize SDL3 system (audio, input, ttf, etc.)
    if (!SDL3.init(window)) {
        LOG_FATAL_CAT("SDL3", "SDL3.init failed");
        SDL_DestroyWindow(window);
        return 1;
    }

    // Step 3: Show sacrificial splash
    showSacrificialSplash();

    // Step 4: Vulkan instance & surface
    VkInstance instance = createVulkanInstance();
    if (instance == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("VULKAN", "Instance creation failed");
        SDL3.shutdown();
        SDL_DestroyWindow(window);
        return 1;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface) == 0) {
        LOG_FATAL_CAT("VULKAN", "Failed to create Vulkan surface");
        vkDestroyInstance(instance, nullptr);
        SDL3.shutdown();
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
        SDL3.shutdown();
        SDL_DestroyWindow(window);
        return 1;
    }

    VkQueue graphics_queue = VK_NULL_HANDLE, present_queue = VK_NULL_HANDLE;
    VkQueue compute_queue  = VK_NULL_HANDLE, transfer_queue  = VK_NULL_HANDLE;

    vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(device, present_family,   0, &present_queue);
    vkGetDeviceQueue(device, compute_family,   0, &compute_queue);
    vkGetDeviceQueue(device, transfer_family,  0, &transfer_queue);

    // Seal core Vulkan objects
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

    // Transient command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = rtx().graphics_family;

    VkResult res = vkCreateCommandPool(device, &poolInfo, nullptr, &rtx().transient_pool);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("VULKAN", "Transient pool failed: {}", vkh.result(res));
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL3.shutdown();
        SDL_DestroyWindow(window);
        return 1;
    }

    LOG_SUCCESS_CAT("VULKAN", "Transient command pool created");

    // Step 5: Create swapchain
    Swapchain::create(window,
                      Options::Window::DEFAULT_WIDTH,
                      Options::Window::DEFAULT_HEIGHT);

    if (!Swapchain::swapchain.valid()) {
        LOG_FATAL_CAT("SWAPCHAIN", "Swapchain creation failed");
        vkDestroyCommandPool(device, rtx().transient_pool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL3.shutdown();
        SDL_DestroyWindow(window);
        return 1;
    }

    LOG_INFO_CAT("MAIN", "Swapchain ready — {}x{}", 
                 Swapchain::getExtent().width, Swapchain::getExtent().height);

    // Step 6: Engine memory init
    EngineMemoryInit();

    // Step 7: Pipeline setup
    Pipeline::initialize();
    Pipeline::create_pipeline_layout();
    Pipeline::create_raymarch_pipeline();

    LOG_AMOURANTH("AMOURANTHRTX v0.91 — SINGLE SHADER SEAL FORGED — CANVAS ACTIVE 💖");

    // Step 8: Create RayCanvas
    raycanvas = std::make_unique<RayCanvas>(
        Options::Window::DEFAULT_WIDTH,
        Options::Window::DEFAULT_HEIGHT,
        window
    );

    // Reset camera
    CAM.reset();
    LOG_INFO_CAT("CAMERA", "Reset complete — pos: {} {} {} | fov: {}",
                 CAM.position().x, CAM.position().y, CAM.position().z, CAM.fov());

    LOG_AMOURANTH("Genesis sealed — eternal compute begins");

    // ────────────────────────────────────────────────
    // Main loop — RayCanvas owns rendering, we pump events
    // ────────────────────────────────────────────────
    // double lastFrameTime = TotalTime::get().seconds();  // unused — removed warning

    while (true) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            SDL3.pump(ev);  // forward to input & callbacks

            // App-level quit / resize handling (example)
            if (ev.type == SDL_EVENT_QUIT) {
                goto cleanup;
            }
            if (ev.type == SDL_EVENT_WINDOW_RESIZED) {
                // Trigger swapchain recreation if needed
            }
        }

        raycanvas->maybeUpdateCanvas();

        if (raycanvas->isDestroyed()) break;
    }

cleanup:
    // Cleanup in reverse order
    raycanvas.reset();
    Pipeline::shutdown();
    SDL3.shutdown();
    SDL_DestroyWindow(window);

    return 0;
}