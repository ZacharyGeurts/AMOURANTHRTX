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

    sdl_init_all(Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT, "AMOURANTHRTX");

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

    // Direct sealing to rtx() — no stone_ wrappers
    rtx().instance = instance;
    rtx().device = device;
    rtx().surface = surface;
    rtx().graphics_queue = graphics_queue;
    rtx().present_queue = present_queue;
    rtx().compute_queue = compute_queue;
    rtx().transfer_queue = transfer_queue;
    rtx().graphics_family = graphics_family;
    rtx().present_family = present_family;
    rtx().transfer_family = transfer_family;
    rtx().compute_family = compute_family;

    // ────────────────────────────────────────────────
    // Create and seal the REAL global transient command pool — early, explicit, once
    // This happens BEFORE swapchain, BEFORE pipeline, BEFORE renderer
    // No dummies, no lazy creation, no chicken-egg
    // ────────────────────────────────────────────────
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | 
                                    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = rtx().graphics_family;

        VkResult res = vkCreateCommandPool(rtx().device, &poolInfo, nullptr, &rtx().transient_pool);
        if (res != VK_SUCCESS) {
            LOG_FATAL_CAT("VULKAN", "Failed to create global transient command pool: {}", string_VkResult(res));
            vkDestroyDevice(device, nullptr);
            vkDestroySurfaceKHR(instance, surface, nullptr);
            vkDestroyInstance(instance, nullptr);
            sdl_cleanup_all();
            return 1;
        }

        LOG_SUCCESS_CAT("VULKAN", "Global transient command pool created and sealed");
    }

    // Create swapchain
    Swapchain::create(g_window, Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT);
    rtx().images = Swapchain::swapchainImages_;
    rtx().views = Swapchain::swapchainImageViews_;
    rtx().extent = Swapchain::swapchainExtent_;
    rtx().image_count = Swapchain::swapchainImages_.size();

    // Pipeline initialization — now with guaranteed real pool
    pipeline_initialize();
    pipeline_create_pipeline_layout();
    pipeline_create_ray_tracing_pipeline();
    pipeline_create_compute_pipeline();
    pipeline_create_shader_binding_table();

    rtx().compute_pipeline = rtx().compute_pipeline;
    rtx().rt_pipeline = rtx().rt_pipeline;
    rtx().pipeline_layout = rtx().pipeline_layout;

    LOG_AMOURANTH("AMOURANTHRTX v0.81 — FINAL RTX SEAL FORGED — FULL ACCESS GRANTED — ALL RESOURCES LOCKED");

    // Renderer — safe to allocate from rtx().transient_pool
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