#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
//
// Main entry point — called from developer's empty main.cpp
// Developers link against this header and call navigator_main(argc, argv)
// =============================================================================

#include "engine/camera.hpp"
#include "engine/SDL3.hpp"
#include "engine/ELLIE.hpp"
#include "engine/AMOURANTHRTX.hpp"
#include "engine/OptionsMenu.hpp"
#include "engine/RayCanvas.hpp"
#include "engine/Pipeline.hpp"

#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>

#include <memory>
#include <chrono>
#include <format>

// Global canvas — persistent compute-driven screen updater
inline std::unique_ptr<RayCanvas> raycanvas;

// Sacrificial Splash — skippable with any input, non-blocking
static inline void showSacrificialSplash() noexcept {
    constexpr int W = 1280, H = 720;
    constexpr const char* TITLE = "AMOURANTHRTX";

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        return;
    }

    SDL_Window* splashWin = SDL_CreateWindow(
        TITLE,
        W, H,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN
    );
    if (!splashWin) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(0, &bounds) == 0) {
        SDL_SetWindowPosition(splashWin,
                              bounds.x + (bounds.w - W)/2,
                              bounds.y + (bounds.h - H)/2);
    }

    // Try to set icon (silent fail OK)
    const char* iconPaths[] = {
        "assets/textures/ammo.ico",
        "assets/textures/ammo.png",
        nullptr
    };
    for (int i = 0; iconPaths[i]; ++i) {
        if (SDL_Surface* surf = IMG_Load(iconPaths[i])) {
            SDL_SetWindowIcon(splashWin, surf);
            SDL_DestroySurface(surf);
            break;
        }
    }

    SDL_Renderer* splashRen = SDL_CreateRenderer(splashWin, nullptr);
    if (!splashRen) {
        SDL_DestroyWindow(splashWin);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    // Try to load splash texture (fallback hot pink)
    SDL_Texture* tex = nullptr;
    const char* texPaths[] = {
        "assets/textures/amouranth.png",
        "assets/textures/splash.png",
        "assets/textures/ammo.png",
        nullptr
    };
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

    // Display ~3.4s or until input
    auto start = std::chrono::steady_clock::now();
    bool skip = false;
    while (std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count() < 3.4f && !skip) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
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

    if (tex) SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(splashRen);
    SDL_DestroyWindow(splashWin);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

// =============================================================================
// Engine-private memory initialization — called AFTER device & swapchain exist
// =============================================================================
static inline void EngineMemoryInit() noexcept {
    LOG_SUCCESS_CAT("MEMORY", "Engine memory ready (no global buffers needed for single-shader mode)");
}

// =============================================================================
// Main entry point — called from developer's empty main.cpp
// Developers link against this header and call navigator_main(argc, argv)
// =============================================================================
inline int navigator_main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {

    install_apocalypse_handler();
    Logging::Logger::get().startup();
    LOG_SUCCESS_CAT("MAIN", "Apocalypse handler installed — logger started");

    // Step 1: Initialize SDL subsystems
    sdl_init_all(Options::Window::DEFAULT_WIDTH,
                 Options::Window::DEFAULT_HEIGHT,
                 "AMOURANTHRTX");

    if (!g_window) {
        LOG_FATAL_CAT("MAIN", "SDL window creation failed");
        sdl_cleanup_all();
        return 1;
    }

    // Step 2: Show splash
    showSacrificialSplash();

    // Step 3: Vulkan instance
    VkInstance instance = createVulkanInstance();
    if (instance == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("VULKAN", "Instance creation failed");
        sdl_cleanup_all();
        return 1;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(g_window, instance, nullptr, &surface) == 0) {
        LOG_FATAL_CAT("VULKAN", "Failed to create Vulkan surface");
        vkDestroyInstance(instance, nullptr);
        sdl_cleanup_all();
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
        sdl_cleanup_all();
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

    // Global transient command pool
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = rtx().graphics_family;

        VkResult res = vkCreateCommandPool(rtx().device, &poolInfo, nullptr, &rtx().transient_pool);
        if (res != VK_SUCCESS) {
            LOG_FATAL_CAT("VULKAN", "Failed to create transient command pool: {}", vkh.result(res));
            vkDestroyDevice(device, nullptr);
            vkDestroySurfaceKHR(instance, surface, nullptr);
            vkDestroyInstance(instance, nullptr);
            sdl_cleanup_all();
            return 1;
        }

        LOG_SUCCESS_CAT("VULKAN", "Transient command pool created");
    }

    // Step 4: Create swapchain
    Swapchain::create(g_window,
                      Options::Window::DEFAULT_WIDTH,
                      Options::Window::DEFAULT_HEIGHT);

    if (!Swapchain::swapchain.valid()) {
        LOG_FATAL_CAT("SWAPCHAIN", "Swapchain creation failed");
        vkDestroyCommandPool(device, rtx().transient_pool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        sdl_cleanup_all();
        return 1;
    }

    LOG_INFO_CAT("MAIN", "Swapchain ready — {}x{}", 
                 Swapchain::getExtent().width, Swapchain::getExtent().height);

    // Step 5: Engine memory init (minimal now)
    EngineMemoryInit();

    // Step 6: Pipeline setup — single compute shader only
    Pipeline::initialize();               // creates descriptor layout
    Pipeline::create_pipeline_layout();   // creates pipeline layout with push constants
    Pipeline::create_canvas_pipeline();   // creates the actual compute pipeline

    LOG_AMOURANTH("AMOURANTHRTX v0.91 — SINGLE SHADER SEAL FORGED — CANVAS ACTIVE 💖");

    // Step 7: Create RayCanvas
    raycanvas = std::make_unique<RayCanvas>(
        Options::Window::DEFAULT_WIDTH,
        Options::Window::DEFAULT_HEIGHT,
        g_window
    );

    // IMPORTANT: Force correct initial size handling (fixes potential initial mismatch)
    int realWidth = 0, realHeight = 0;
    SDL_GetWindowSize(g_window, &realWidth, &realHeight);
    LOG_INFO_CAT("MAIN", "Forcing initial resize to actual window size: {}x{}", realWidth, realHeight);
    raycanvas->onResize(realWidth, realHeight);

    // Reset camera
    CAM.reset();

    LOG_AMOURANTH("Genesis sealed — eternal compute begins");

    // ────────────────────────────────────────────────
    // Eternal loop
    // ────────────────────────────────────────────────
    while (true) {
        int w = 0, h = 0;
        bool quit = false, fullscreen_toggle = false;

        sdl_poll_events(w, h, quit, fullscreen_toggle);

        if (fullscreen_toggle) {
            sdl_toggle_fullscreen();
        }

        if (quit) {
            break;
        }

        // Update canvas size only if changed (sdl_poll_events sets w/h)
        raycanvas->onResize(w, h);

        // Render the frame
        raycanvas->maybeUpdateCanvas();
    }

    // Cleanup
    raycanvas.reset();
    Pipeline::shutdown();
    sdl_cleanup_all();

    return 0;
}