// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO
// main.cpp — February 03, 2026
// PURE LIGHT — ETERNAL LOOP — STONE SEALING IN ORDER
// AMOURANTH FOREVER 💖
// =============================================================================

#include "main.hpp"
#include <SDL3_image/SDL_image.h>
#include <memory>
#include <chrono>

std::unique_ptr<VulkanRenderer> renderer;

// Sacrificial Splash — skippable, uses SDL3_image
static void showSacrificialSplash() noexcept {
    constexpr int W = 1280, H = 720;
    constexpr const char* TITLE = "AMOURANTH RTX v∞ TURBO";

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) return;

    SDL_Window* splashWin = SDL_CreateWindow(TITLE, W, H, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!splashWin) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(0, &bounds) == 0) {
        SDL_SetWindowPosition(splashWin, bounds.x + (bounds.w - W)/2, bounds.y + (bounds.h - H)/2);
    }

    const char* iconPaths[] = {"assets/textures/ammo.ico", "assets/textures/ammo.png", nullptr};
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

    SDL_Texture* tex = nullptr;
    const char* texPaths[] = {"assets/textures/amouranth.png", "assets/textures/splash.png", "assets/textures/ammo.png", nullptr};
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
        SDL_FRect dst{(W - tw)*0.5f, (H - th)*0.5f, tw, th};
        SDL_SetRenderDrawColor(splashRen, 0, 0, 0, 255);
        SDL_RenderClear(splashRen);
        SDL_RenderTexture(splashRen, tex, nullptr, &dst);
    } else {
        SDL_SetRenderDrawColor(splashRen, 255, 20, 147, 255);
        SDL_RenderClear(splashRen);
    }

    SDL_ShowWindow(splashWin);
    SDL_RenderPresent(splashRen);

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

// Main — Vulkan creation, sealing in correct order, eternal loop
int main(int, char**) {
    install_apocalypse_handler();

    sdl_init_all(Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT, "AMOURANTH RTX v∞ TURBO");

    if (!g_window) {
        sdl_cleanup_all();
        return 1;
    }

    showSacrificialSplash();

    VkInstance instance = createVulkanInstance();
    if (instance == VK_NULL_HANDLE) {
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

    uint32_t graphics_family = 0, present_family = 0, compute_family = 0, transfer_family = 0;
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
    VkQueue compute_queue = VK_NULL_HANDLE, transfer_queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(device, present_family, 0, &present_queue);
    vkGetDeviceQueue(device, compute_family, 0, &compute_queue);
    vkGetDeviceQueue(device, transfer_family, 0, &transfer_queue);

    // Sealing in correct order — device + queues + families first
    stone_seal_device_resources(instance, device, rtx().physical, surface, VK_NULL_HANDLE);
    stone_seal_queues(graphics_queue, present_queue, compute_queue, transfer_queue);
    stone_seal_families(graphics_family, present_family, transfer_family, compute_family);

    // Create swapchain early (needed for surface capabilities)
    Swapchain::create(g_window, Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT);
    stone_seal_swapchain_resources(Swapchain::swapchainImages_, 
                                   Swapchain::swapchainImageViews_, 
                                   Swapchain::swapchainExtent_, 
                                   Swapchain::swapchainImages_.size());

    // ────────────────────────────────────────────────
    // CRITICAL: Pipeline setup BEFORE renderer
    // This is where rtx().transient_pool is created
    // ────────────────────────────────────────────────
    pipeline_initialize();                    // ← usually creates transient_pool
    pipeline_create_pipeline_layout();
    pipeline_create_ray_tracing_pipeline();
    pipeline_create_compute_pipeline();
    pipeline_create_shader_binding_table();

    stone_seal_pipelines(rtx().compute_pipeline, rtx().rt_pipeline, rtx().pipeline_layout);
    stone_seal_final();

    // ────────────────────────────────────────────────
    // NOW safe — transient pool exists
    // ────────────────────────────────────────────────
    renderer = std::make_unique<VulkanRenderer>(Options::Window::DEFAULT_WIDTH, 
                                                Options::Window::DEFAULT_HEIGHT, 
                                                g_window);

    // Eternal loop
    while (true) {
        int w, h;
        bool quit = false, fs = false;
        sdl_poll_events(w, h, quit, fs);

        if (fs) sdl_toggle_fullscreen();
        if (quit) break;

        renderer->onResize(w, h);
        renderer->pew();
    }

    renderer.reset();
    sdl_cleanup_all();

    return 0;
}