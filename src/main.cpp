// =============================================================================
// AMOURANTH RTX Engine - Main Entry Point
// Naked Vulkan startup — no fallback, no pink, just beams
// Version 30.3 — January 20, 2026
// Production ready: Main handles streaking — LAS to swapchain, rays every frame
// No guards, no slowdown — crash loud, learn fast, render turf
// =============================================================================

#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/console.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/camera.hpp"

#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>
#include <chrono>
#include <print>
#include <thread>
#include <memory>

using namespace std::chrono_literals;

// Global renderer — RAII
std::unique_ptr<RTX::VulkanRenderer> renderer;

float g_deltaTime = 0.0f;
bool g_running = true;

// =============================================================================
// Sacrificial Splash — Isolated, disposable
// =============================================================================
static void showSacrificialSplash() {
    if (!Options::Splash::ENABLE_SACRIFICIAL_SPLASH) return;

    constexpr int W = 1280, H = 720;
    constexpr const char* TITLE = "AMOURANTH RTX vTURBO";

    std::print("[SPLASH] Starting sacrificial splash...\n");

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        std::print("[SPLASH] SDL video init failed: {}\n", SDL_GetError());
        return;
    }

    SDL_Window* splashWin = SDL_CreateWindow(TITLE, W, H,
                                             SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!splashWin) {
        std::print("[SPLASH] Window creation failed: {}\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(0, &bounds) == 0) {
        SDL_SetWindowPosition(splashWin, bounds.x + (bounds.w - W)/2, bounds.y + (bounds.h - H)/2);
    }

    const char* iconPaths[] = {"assets/textures/icon.png", "assets/textures/ammo.png", nullptr};
    for (int i = 0; iconPaths[i]; ++i) {
        if (SDL_Surface* surf = IMG_Load(iconPaths[i])) {
            SDL_SetWindowIcon(splashWin, surf);
            SDL_DestroySurface(surf);
            break;
        }
    }

    SDL_Renderer* splashRen = SDL_CreateRenderer(splashWin, nullptr);
    if (!splashRen) {
        std::print("[SPLASH] Renderer creation failed: {}\n", SDL_GetError());
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
    while (std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count() < Options::Splash::SPLASH_DURATION_SECONDS) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT ||
                (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE && Options::Splash::ALLOW_EARLY_EXIT)) {
                goto splash_cleanup;
            }
        }
        std::this_thread::sleep_for(8ms);
    }

splash_cleanup:
    if (tex) SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(splashRen);
    SDL_DestroyWindow(splashWin);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    std::print("[SPLASH] Complete — clean slate ready\n");
}

// =============================================================================
// Safe shutdown
// =============================================================================
[[noreturn]] static void apocalypse(std::string_view reason = "Normal exit") {
    std::print("[MAIN] Shutdown: {} — cleaning resources\n", reason);

    renderer.reset();

    if (VkDevice dev = StoneKey::stone_device(); dev != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(dev);
        vkDestroyDevice(dev, nullptr);
        StoneKey::stone_seal_device(VK_NULL_HANDLE);
    }

    if (VkInstance inst = StoneKey::stone_instance(); inst != VK_NULL_HANDLE) {
        if (VkSurfaceKHR surf = StoneKey::stone_surface(); surf != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(inst, surf, nullptr);
            StoneKey::stone_seal_surface(VK_NULL_HANDLE);
        }
        vkDestroyInstance(inst, nullptr);
        StoneKey::stone_seal_instance(VK_NULL_HANDLE);
    }

    if (SDL_Window* win = StoneKey::stone_window(); win) {
        SDL_DestroyWindow(win);
        StoneKey::stone_seal_window(nullptr);
    }

    SDL_Quit();
    std::print("[MAIN] Shutdown complete\n");
    std::exit(0);
}

// =============================================================================
// Main — Naked startup, straight to beams
// =============================================================================
int main(int, char**) {
    if (SDL_Init(SDL_INIT_EVENTS) == 0) {
        std::print("[FATAL] Early SDL init failed: {}\n", SDL_GetError());
        return 1;
    }

    showSacrificialSplash();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        std::print("[FATAL] Main SDL init failed: {}\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) { // you'll see
        std::print("[FATAL] Vulkan loader failed: {}\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    SDL_Window* window = SDL_CreateWindow("AMOURANTH RTX vTURBO",
                                          Options::Window::DEFAULT_WIDTH,
                                          Options::Window::DEFAULT_HEIGHT,
                                          flags);
    if (!window) {
        std::print("[FATAL] Window creation failed: {}\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    StoneKey::stone_seal_window(window);

    const char* iconPaths[] = {"assets/textures/icon.png", "assets/textures/ammo.png", nullptr};
    for (int i = 0; iconPaths[i]; ++i) {
        if (SDL_Surface* surf = IMG_Load(iconPaths[i])) {
            SDL_SetWindowIcon(window, surf);
            SDL_DestroySurface(surf);
            break;
        }
    }

    int pixelW = 0, pixelH = 0;
    SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH);

    VkInstance instance = RTX::createVulkanInstance(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) apocalypse("Instance creation failed");

    StoneKey::stone_seal_instance(instance);

    RTX::loadInstanceExtensions(instance);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        std::print("[FATAL] Surface creation failed: {}\n", SDL_GetError());
        apocalypse("Surface creation failed");
    }
    StoneKey::stone_seal_surface(surface);

    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) apocalypse("Device creation failed");

    // Seal device and related objects (idempotent, safe)
    StoneKey::stone_seal_device(device);
    StoneKey::stone_seal_physical(RTX::g_ctx().physical_);
    StoneKey::stone_seal_graphics_family(RTX::g_ctx().graphicsFamily_);
    StoneKey::stone_seal_present_family(RTX::g_ctx().presentFamily_);
    StoneKey::stone_seal_transfer_family(RTX::g_ctx().transferFamily_);
    StoneKey::stone_seal_compute_family(RTX::g_ctx().computeFamily_);
    StoneKey::stone_seal_graphics_queue(RTX::g_ctx().graphicsQueue_);
    StoneKey::stone_seal_present_queue(RTX::g_ctx().presentQueue_);
    StoneKey::stone_seal_transfer_queue(RTX::g_ctx().transferQueue_);
    StoneKey::stone_seal_compute_queue(RTX::g_ctx().computeQueue_);

    RTX::loadDeviceExtensions(device);
    RTX::g_ctx().init();

    RTX::SwapchainManager::create(window, pixelW, pixelH);

    renderer = std::make_unique<RTX::VulkanRenderer>(pixelW, pixelH, window, Options::Performance::OVERCLOCK_RENDERER);
    renderer->createPersistentCommandPoolAndBuffers();

    // Force initial acceleration structure build
    RTX::LAS::instance().getTLAS();

    // Create descriptor pool after AS build (driver stability)
    RTX::createGlobalDescriptorPool();

    SDL_Renderer* sdlRen = SDL_CreateRenderer(window, nullptr);
    if (!sdlRen) std::print("[WARN] SDL overlay renderer failed: {}\n", SDL_GetError());
    Console::init(window, sdlRen);

    // Local camera instance
    Camera cam;

    auto lastTime = std::chrono::steady_clock::now();
    int frameCount = 0;
    float fpsTimer = 0.0f;

    int curW = pixelW, curH = pixelH;

    std::print("[MAIN] Engine fully initialized — RTX realm active — naked mode\n");

    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        fpsTimer += g_deltaTime;
        ++frameCount;

        // FPS counter at the top — prints even on crash
        if (fpsTimer >= 1.0f) {
            float fps = frameCount / fpsTimer;
            std::print("[PERF] FPS: {:.1f} | SPP: {} | Frame: {} | {}×{}\n",
                       fps, renderer->spp(), renderer->currentFrame(), curW, curH);
            fflush(stdout);
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        SDL_Event e;
        bool quit = false;
        bool toggleFS = false;
        int newW = curW, newH = curH;

        while (SDL_PollEvent(&e)) {
            Console::handleEvent(e);
            if (e.type == SDL_EVENT_QUIT) quit = true;
            if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                SDL_GetWindowSizeInPixels(window, &newW, &newH);
            }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_F11) toggleFS = true;
        }

        if (toggleFS) SDL3Window::toggleFullscreen();
        if (quit) g_running = false;

        if (newW != curW || newH != curH) {
            curW = std::max(newW, 1);
            curH = std::max(newH, 1);
            RTX::SwapchainManager::recreate(curW, curH);
            renderer->createPersistentCommandPoolAndBuffers();
            RTX::LAS::instance().requestRebuild();
        }

        if (StoneKey::stone_swapchain() != VK_NULL_HANDLE) {
            renderer->renderFrame(cam, g_deltaTime);
        }

        Console::render();
    }

    apocalypse("Graceful exit");
    return 0;
}

// =============================================================================
// Main v30.3 — January 20, 2026
// - Naked mode: no fallback, no pink, just beams
// - FPS counter at top of loop — prints even on crash
// - LAS to swapchain, rays every frame
// - Production stable (fail fast)
// =============================================================================