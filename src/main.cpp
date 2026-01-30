// =============================================================================
// AMOURANTH RTX Engine © 2026 — JANUARY 30, 2026
// main.cpp — PURE LIGHT EMPIRE LAUNCHER
// - Splash always skippable (any input)
// - SDL3 high-DPI pixel size respected
// - No sleeps or idles — eternal loop, no throttling
// - StoneKey sealed early — empire locked before renderer
// - Renderer owns everything — LAS, pipeline, SBT, eternal accumulation
// - Lifetime log throttled (1/sec)
// - Clean shutdown on quit
// =============================================================================

#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/camera.hpp"

#include <SDL3/SDL_vulkan.h>
#include <chrono>
#include <print>
#include <memory>

// StoneKey accessors & sealers — using at top, no qualification
using StoneKey::stone_device;
using StoneKey::stone_window;
using StoneKey::stone_instance;
using StoneKey::stone_surface;
using StoneKey::stone_seal_device_resources;
using StoneKey::stone_seal_queues;
using StoneKey::stone_seal_families;
using StoneKey::stone_seal_final;

// Global renderer — owns the eternal light
std::unique_ptr<RTX::VulkanRenderer> renderer;

// =============================================================================
// Sacrificial Splash — skippable with any input
// =============================================================================
static void showSacrificialSplash() {
    constexpr int W = 1280, H = 720;
    constexpr const char* TITLE = "AMOURANTH RTX v∞ TURBO";

    std::print("[SPLASH] Starting...\n");

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        std::print("[SPLASH] Video init failed: {}\n", SDL_GetError());
        return;
    }

    SDL_Window* splashWin = SDL_CreateWindow(TITLE, W, H,
                                             SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!splashWin) {
        std::print("[SPLASH] Window failed: {}\n", SDL_GetError());
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
        std::print("[SPLASH] Renderer failed: {}\n", SDL_GetError());
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
    while (std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count() < 3.4f) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT ||
                e.type == SDL_EVENT_KEY_DOWN ||
                e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
                goto splash_cleanup;
            }
        }
    }

splash_cleanup:
    if (tex) SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(splashRen);
    SDL_DestroyWindow(splashWin);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    std::print("[SPLASH] Complete\n");
}

// =============================================================================
// Safe shutdown — empire lockdown
// =============================================================================
[[noreturn]] static void apocalypse(std::string_view reason = "Normal exit") {
    std::print("[MAIN] Shutdown: {}\n", reason);

    renderer.reset();

    if (VkDevice dev = stone_device(); dev != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(dev);
        vkDestroyDevice(dev, nullptr);
    }

    if (VkInstance inst = stone_instance(); inst != VK_NULL_HANDLE) {
        if (VkSurfaceKHR surf = stone_surface(); surf != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(inst, surf, nullptr);
        }
        vkDestroyInstance(inst, nullptr);
    }

    if (SDL_Window* win = stone_window(); win) {
        SDL_DestroyWindow(win);
    }

    SDL_Quit();
    std::print("[MAIN] Shutdown complete\n");
    std::exit(0);
}

// =============================================================================
// Main — Minimal launcher — pure light takes over
// =============================================================================
int main(int, char**) {
    if (SDL_Init(SDL_INIT_EVENTS) == 0) {
        std::print("[FATAL] Early SDL init failed: {}\n", SDL_GetError());
        return 1;
    }

    showSacrificialSplash();

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        std::print("[FATAL] Video subsystem init failed: {}\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        std::print("[FATAL] Vulkan loader failed: {}\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    SDL_Window* window = SDL_CreateWindow("AMOURANTH RTX v∞ TURBO",
                                          Options::Window::DEFAULT_WIDTH,
                                          Options::Window::DEFAULT_HEIGHT,
                                          flags);
    if (!window) {
        std::print("[FATAL] Window creation failed: {}\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    const char* iconPaths[] = {"assets/textures/icon.ico", "assets/textures/ammo.png", nullptr};
    for (int i = 0; iconPaths[i]; ++i) {
        if (SDL_Surface* surf = IMG_Load(iconPaths[i])) {
            SDL_SetWindowIcon(window, surf);
            SDL_DestroySurface(surf);
            break;
        }
    }

    int pixelW = 0, pixelH = 0;
    SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH);

    VkInstance instance = RTX::createVulkanInstance();
    if (!instance) apocalypse("Instance creation failed");

    RTX::loadInstanceExtensions(instance);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface) == 0) {
        std::print("[FATAL] Surface creation failed: {}\n", SDL_GetError());
        apocalypse("Surface creation failed");
    }

    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) apocalypse("Device creation failed");

    // Seal the empire — unbreakable lockdown
    stone_seal_device_resources(instance, device, RTX::g_ctx().physical,
                                surface, VK_NULL_HANDLE);  // swapchain sealed in SwapchainManager
    stone_seal_queues(RTX::g_ctx().graphicsQueue, RTX::g_ctx().presentQueue,
                      RTX::g_ctx().computeQueue, RTX::g_ctx().transferQueue);
    stone_seal_families(RTX::g_ctx().graphicsFamily, RTX::g_ctx().presentFamily,
                        RTX::g_ctx().transferFamily, RTX::g_ctx().computeFamily);
    stone_seal_final();  // LOCKED — tamper = death

    RTX::loadDeviceExtensions(device);
    RTX::g_ctx().init();

    RTX::SwapchainManager::create(window, pixelW, pixelH);

    // Renderer owns the eternal light — LAS, pipeline, SBT, accumulation
    renderer = std::make_unique<RTX::VulkanRenderer>(pixelW, pixelH, window);

    std::print("[MAIN] Empire launched — pure light engaged\n");

    auto last_log_time = std::chrono::steady_clock::now();

    while (true) {
        renderer->pew();  // Eternal light loop — no sleep, no idle, pure fire

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                apocalypse("Quit requested");
            }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_F11) {
                SDL3Window::toggleFullscreen();
            }
        }

        // Throttled lifetime log — once per second (non-blocking)
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last_log_time).count();
        if (elapsed >= 1.0) {
            LOG_INFO_CAT("MAIN", "Lifetime: {:.6f}s | Pink photons eternal!",
                         renderer->getLifetimeSeconds());
            last_log_time = now;
        }
    }

    // Unreachable
    return 0;
}