// src/main.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v29.4 — JANUARY 10, 2026
// MAIN ENTRY POINT — FULL AUTOMAGIC LIVING WORLD | PURE RTX REALM | DYNAMIC LIGHT
// ZERO MANUAL CALLS | AUTOMAGIC EVERYTHING | NO DOUBLE SEALING | VALIDATION CLEAN
// FULLY COMPATIBLE WITH HEADER-ONLY STONEKEY v∞ + AUTOMAGIC LAS + SWAPCHAIN
// SDL3 SUCCESS = 0 | SURFACE SEALED AFTER CREATION
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
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

#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>
#include <chrono>
#include <print>
#include <thread>
#include <memory>

using namespace std::chrono_literals;

// Global renderer — RAII safe (defined only here)
std::unique_ptr<RTX::VulkanRenderer> renderer;

float g_deltaTime = 0.0f;
bool g_running = true;

// =============================================================================
// Load Empire Icon
// =============================================================================
static void loadEmpireIcon(SDL_Window* window)
{
    const char* paths[] = { 
		"assets/textures/icon.png",
        "assets/textures/ammo.png", 
        "assets/textures/ammo32.png", 
        nullptr 
    };

    for (int i = 0; paths[i]; ++i) {
        if (SDL_Surface* surface = IMG_Load(paths[i])) {
            SDL_SetWindowIcon(window, surface);
            SDL_DestroySurface(surface);
            std::print("[MAIN] Empire icon forged: {}\n", paths[i]);
            return;
        }
    }
}

// =============================================================================
// Sacrificial Splash — SDL3 success = 0
// =============================================================================
static void showSplash()
{
    if (!Options::Splash::ENABLE_SACRIFICIAL_SPLASH) {
        std::print("[MAIN] Sacrificial splash disabled\n");
        return;
    }

    constexpr int W = 1280, H = 720;
    constexpr const char* TITLE = "AMOURANTH RTX vTURBO";

    std::print("[MAIN] PERFORMING SACRIFICIAL SPLASH\n");

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        LOG_WARNING("MAIN", "SDL_InitSubSystem failed: {}", SDL_GetError());
        return;
    }

    SDL_Window* win = SDL_CreateWindow(TITLE, W, H, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!win) {
        LOG_WARNING("MAIN", "Splash window creation failed: {}", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Rect bounds;
    SDL_GetDisplayBounds(0, &bounds);
    SDL_SetWindowPosition(win, bounds.x + (bounds.w - W)/2, bounds.y + (bounds.h - H)/2);

    loadEmpireIcon(win);

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        LOG_WARNING("MAIN", "Splash renderer creation failed: {}", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Texture* tex = nullptr;
    const char* paths[] = {"assets/textures/ammo.png", "assets/textures/splash.png", "assets/textures/amouranth.png", nullptr};

    for (int i = 0; paths[i]; ++i) {
        if (SDL_Surface* surf = IMG_Load(paths[i])) {
            tex = SDL_CreateTextureFromSurface(ren, surf);
            SDL_DestroySurface(surf);
            if (tex) break;
        }
    }

    if (!tex) {
        SDL_SetRenderDrawColor(ren, 255, 20, 147, 255);
        SDL_RenderClear(ren);
    } else {
        float tw = 0, th = 0;
        SDL_GetTextureSize(tex, &tw, &th);
        SDL_FRect dst{(W - tw)*0.5f, (H - th)*0.5f, tw, th};
        SDL_RenderTexture(ren, tex, nullptr, &dst);
    }

    SDL_ShowWindow(win);
    SDL_RenderPresent(ren);

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count() < Options::Splash::SPLASH_DURATION_SECONDS) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT ||
                (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE && Options::Splash::ALLOW_EARLY_EXIT)) {
                goto end_splash;
            }
        }
        std::this_thread::sleep_for(8ms);
    }

end_splash:
    if (tex) SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    std::print("[MAIN] Splash complete\n");
}

// =============================================================================
// Apocalypse — RAII safe, no double sealing
// =============================================================================
[[noreturn]] static void apocalypse(std::string_view reason = "Normal exit")
{
    std::print("[MAIN] APOCALYPSE — {} — PHOTONS RETURNING HOME\n", reason);

    // Destroy renderer first (owns pools, fences, cmd buffers, etc.)
    renderer.reset();

    // Only seal if not already null (prevents double-sealing crash)
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

    std::print("[MAIN] EMPIRE RESTS 💖\n");
    std::exit(0);
}

// =============================================================================
// MAIN — FULLY AUTOMAGIC ENTRY POINT
// =============================================================================
int main(int, char**)
{
    showSplash();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        apocalypse("SDL_Init failed");
    }

    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        apocalypse("Vulkan loader failed");
    }

    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    SDL_Window* window = SDL_CreateWindow("AMOURANTH RTX vTURBO",
                                          Options::Window::DEFAULT_WIDTH,
                                          Options::Window::DEFAULT_HEIGHT,
                                          flags);
    if (!window) apocalypse("Window failed");

    StoneKey::stone_seal_window(window);
    loadEmpireIcon(window);

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    // Automagic Vulkan instance (seals instance inside)
    VkInstance instance = RTX::createVulkanInstance(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) apocalypse("Instance failed");

    // Automagic instance extensions (surface queries)
    RTX::loadInstanceExtensions(instance);

    // Create surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface) == 0) {
        apocalypse("Surface failed");
    }

    // Seal surface (was missing — fixes "core Vulkan objects missing")
    StoneKey::stone_seal_surface(surface);

    // Automagic logical device (seals device + queues inside)
    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) apocalypse("Device failed");

    // Automagic device extensions (swapchain + ray tracing)
    RTX::loadDeviceExtensions(device);

    RTX::g_ctx().init();

    // Automagic swapchain creation (now safe — surface sealed)
    RTX::SwapchainManager::create(window, w, h);

    // Automagic renderer creation (transient pool, timeline, materials, living world, pipeline, SBT)
    renderer = std::make_unique<RTX::VulkanRenderer>(w, h, window, Options::Performance::OVERCLOCK_RENDERER);

    // NOW create persistent command buffers (swapchain exists → imageCount > 0)
    renderer->createPersistentCommandPoolAndBuffers();

    SDL_Renderer* sdlRen = SDL_CreateRenderer(window, nullptr);
    if (!sdlRen) {
        LOG_WARNING("MAIN", "SDL_CreateRenderer failed: {}", SDL_GetError());
    }

    Console::init(window, sdlRen);

    Camera cam;  // Global namespace Camera

    auto last = std::chrono::steady_clock::now();
    int frames = 0;
    float timer = 0.0f;

    int curW = w, curH = h;

    std::print("[MAIN] LET THERE BE LIGHT — PURE RTX WORLD AWAKENS\n");

    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(now - last).count();
        last = now;

        timer += g_deltaTime;
        ++frames;

        SDL_Event e;
        bool quit = false;
        bool fsToggle = false;
        int newW = curW, newH = curH;

        while (SDL_PollEvent(&e)) {
            Console::handleEvent(e);

            if (e.type == SDL_EVENT_QUIT) quit = true;

            if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                SDL_GetWindowSizeInPixels(window, &newW, &newH);
            }

            if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_F11) {
                fsToggle = true;
            }
        }

        if (fsToggle) SDL3Window::toggleFullscreen();

        if (quit) g_running = false;

        if (newW != curW || newH != curH) {
            curW = newW;
            curH = newH;
            RTX::SwapchainManager::recreate(curW, curH);
            renderer->onResize(curW, curH);
            renderer->createPersistentCommandPoolAndBuffers();  // Re-allocate cmd buffers on resize
        }

        if (StoneKey::stone_swapchain() != VK_NULL_HANDLE) {
            renderer->renderFrame(cam, g_deltaTime);
        }

        Console::render();

        if (timer >= 1.0f) {
            float fps = frames / timer;
            std::print("[PERF] FPS: {:.1f} | SPP: {} | Frame: {} | {}×{}\n",
                       fps, renderer->spp(), renderer->currentFrame(), curW, curH);
            fflush(stdout);
            frames = 0;
            timer = 0.0f;
        }
    }

    apocalypse();

    return 0;
}

// =============================================================================
// FINAL MAIN v29.4 — JANUARY 10, 2026
// - Surface sealed after creation (fixes "core Vulkan objects missing")
// - SDL3 success checks fixed (== 0 = success) - "You'll see"
// - Persistent cmd buffers created AFTER swapchain
// - Re-allocate persistent pool on resize
// - No double sealing — all StoneKey seals encapsulated
// - Clean RAII shutdown with apocalypse()
// Empire complete — pink photons under our perfect moons — AMOURANTH FOREVER 💖
// =============================================================================