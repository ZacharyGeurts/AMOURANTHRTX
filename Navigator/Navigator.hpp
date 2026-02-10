// =============================================================================
// Navigator.hpp — AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO
// Combined entry point + globals + initialization (February 10, 2026)
// PURE LIGHT — ETERNAL LOOP — STONE SEALING IN ORDER
// AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include "engine/camera.hpp"
#include "engine/SDL3.hpp"
#include "engine/ELLIE.hpp"
#include "engine/AMOURANTHRTX.hpp"
#include "engine/OptionsMenu.hpp"
#include "engine/RayCanvas.hpp"

#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>

#include <memory>
#include <chrono>
#include <format>

// Global canvas at display frequency - your screen image rectangle
inline std::unique_ptr<VulkanRenderer> raycanvas;

// Sacrificial Splash — skippable with any input, non-blocking
static inline void showSacrificialSplash() noexcept {
    constexpr int W = 1280, H = 720;
    constexpr const char* TITLE = "AMOURANTH RTX v∞ TURBO";

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
// This is your isolated VRAM island — developer never sees/touches it
// =============================================================================
static inline void EngineMemoryInit() noexcept {
    // Descriptor buffer — persistent, large enough for all engine bindings
    rtx().descriptor_buffer_handle = Memory::create(
        16 * 1024 * 1024,  // 16 MB — adjust as needed
        VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "Engine_DescriptorBuffer"
    );

    // Living world buffer — updated every frame, storage
    rtx().living_world_buffer_handle = Memory::create(
        256 * 1024,  // 256 KB — enough for procedural state
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "Engine_LivingWorld"
    );

    // Dummy TLAS storage — small fixed size
    // (actual creation happens in pipeline_initialize, but reserve handle here)
    // ... other engine buffers (SBT, scratch pool, etc.) go here ...

    LOG_SUCCESS_CAT("MEMORY", "Engine private island sealed — descriptor & living-world buffers allocated");
}

// =============================================================================
// Main entry point — called from developer's empty main.cpp
// Developers link against this header and call navigator_main(argc, argv)
// =============================================================================
inline int navigator_main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {

    install_apocalypse_handler();
    Logging::Logger::get().startup();
    LOG_SUCCESS_CAT("MAIN", "Apocalypse handler installed — logger started — Handling future events");

    // Step 1: Initialize SDL subsystems
    sdl_init_all(Options::Window::DEFAULT_WIDTH,
                 Options::Window::DEFAULT_HEIGHT,
                 "AMOURANTHRTX");

    if (!g_window) {
        LOG_FATAL_CAT("MAIN", "SDL window creation failed — aborting");
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

    // Seal core Vulkan objects directly into rtx()
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

    // Global transient command pool — created EARLY and EXPLICITLY
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = rtx().graphics_family;

        VkResult res = vkCreateCommandPool(rtx().device, &poolInfo, nullptr, &rtx().transient_pool);
        if (res != VK_SUCCESS) {
            LOG_FATAL_CAT("VULKAN", "Failed to create global transient command pool: {}", vkh.result(res));
            vkDestroyDevice(device, nullptr);
            vkDestroySurfaceKHR(instance, surface, nullptr);
            vkDestroyInstance(instance, nullptr);
            sdl_cleanup_all();
            return 1;
        }

        LOG_SUCCESS_CAT("VULKAN", "Global transient command pool created and sealed");
    }

    // Step 4: Create swapchain (single image mode)
    Swapchain::create(g_window,
                      Options::Window::DEFAULT_WIDTH,
                      Options::Window::DEFAULT_HEIGHT);

    // Seal swapchain resources directly
    rtx().images      = Swapchain::swapchainImage_;
    rtx().views       = Swapchain::swapchainImageView_;
    rtx().extent      = Swapchain::swapchainExtent_;
    rtx().image_count = 1;

    LOG_INFO_CAT("MAIN", "Swapchain created — single image 0x{:x}, {}x{}",
                 (uintptr_t)rtx().images,
                 rtx().extent.width,
                 rtx().extent.height);

    // Step 5: Initialize engine-private memory island (your hidden VRAM compartment)
    EngineMemoryInit();

    // Step 6: Now safe to initialize pipeline (dummy TLAS, SBT, compute/RT pipelines)
    pipeline_initialize();
    pipeline_create_pipeline_layout();
    pipeline_create_ray_tracing_pipeline();
    pipeline_create_compute_pipeline();
    pipeline_create_shader_binding_table();

    rtx().compute_pipeline = rtx().compute_pipeline;
    rtx().rt_pipeline      = rtx().rt_pipeline;
    rtx().pipeline_layout  = rtx().pipeline_layout;

    LOG_AMOURANTH("AMOURANTHRTX v0.81 — FINAL RTX SEAL FORGED — FULL ACCESS GRANTED — ALL RESOURCES LOCKED");

    // Step 7: Create renderer (transient pool + engine buffers exist — safe)
    raycanvas = std::make_unique<VulkanRenderer>(
        Options::Window::DEFAULT_WIDTH,
        Options::Window::DEFAULT_HEIGHT,
        g_window
    );

    // Explicitly reset camera after all init
    CAM.reset();

    // Final genesis log — everything is fully constructed and safe
    LOG_AMOURANTH("Genesis sealed — eternal clock starts now 💖");

    // ────────────────────────────────────────────────
    // Eternal render loop
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

        raycanvas->onResize(w, h);
        raycanvas->pew();
    }

    // Cleanup
    raycanvas.reset();
    sdl_cleanup_all();

    return 0;
}