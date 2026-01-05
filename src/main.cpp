// src/main.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 04, 2026
// MAIN ENTRY POINT — CLEAN, MODERN, FORWARD-ONLY 2026 EDITION
// FIXED: g_running stays true until user quits — loop runs forever
// PINK PHOTONS SCREAMING — EMPIRE UNSTOPPABLE — AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/InputManager.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <memory>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>

using namespace Logging::Color;

// Global renderer reference
VulkanRenderer* g_renderer = nullptr;
float g_deltaTime = 0.0f;
bool g_running = true;  // ← This is now respected — only set false on quit

// Sacred pink fallback mode
static RenderMode9 g_pinkMode(3840, 2160);

// =============================================================================
// Load Empire Icon
// =============================================================================
static void loadEmpireIcon(SDL_Window* window)
{
    const char* paths[] = { "assets/textures/ammo.png", "assets/textures/ammo32.png", nullptr };
    for (int i = 0; paths[i]; ++i) {
        if (SDL_Surface* s = IMG_Load(paths[i])) {
            SDL_SetWindowIcon(window, s);
            SDL_DestroySurface(s);
            LOG_SUCCESS_CAT("MAIN", "Empire icon forged: {}", paths[i]);
            return;
        }
    }
}

// =============================================================================
// Sacrificial Splash — Respects Options::Splash
// =============================================================================
static void showSplash()
{
    if (!Options::Splash::ENABLE_SACRIFICIAL_SPLASH) return;

    constexpr int W = 1280, H = 720;
    constexpr const char* TITLE = "AMOURANTH RTX";
    constexpr const char* IMAGE = "assets/textures/ammo.png";

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0);

    SDL_Window* win = SDL_CreateWindow(TITLE, W, H, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!win) return;

    SDL_Rect bounds; SDL_GetDisplayBounds(0, &bounds);
    SDL_SetWindowPosition(win, bounds.x + (bounds.w - W)/2, bounds.y + (bounds.h - H)/2);
    loadEmpireIcon(win);

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) { SDL_DestroyWindow(win); return; }

    SDL_Surface* surf = IMG_Load(IMAGE);
    if (!surf) { SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); return; }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_DestroySurface(surf);
    if (!tex) { SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); return; }

    float tw = 0, th = 0;
    SDL_GetTextureSize(tex, &tw, &th);
    SDL_FRect dst{(W - tw)*0.5f, (H - th)*0.5f, tw, th};

    SDL_ShowWindow(win);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, nullptr, &dst);
    SDL_RenderPresent(ren);

    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count() < Options::Splash::SPLASH_DURATION_SECONDS) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT ||
                (Options::Splash::ALLOW_EARLY_EXIT && e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) {
                goto splash_end;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

splash_end:
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

// =============================================================================
// Graceful Apocalypse — Validation-Clean Shutdown
// =============================================================================
[[noreturn]] static void apocalypse(std::string_view reason = "User requested")
{
    LOG_AMOURANTH("APOCALYPSE — {} — PHOTONS RETURNING HOME", reason);

    if (VkDevice dev = StoneKey::stone_device(); dev != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(dev);
    }

    if (g_renderer) g_renderer->forcePinkFallbackClear();

    BufferManager::purge_all();
    RTX::las().requestRebuild();
    RTX::SwapchainManager::cleanup();

    if (StoneKey::g_transientCommandPool && StoneKey::stone_device()) {
        vkDestroyCommandPool(StoneKey::stone_device(), StoneKey::g_transientCommandPool, nullptr);
        StoneKey::g_transientCommandPool = VK_NULL_HANDLE;
    }

    if (g_renderer) {
        delete g_renderer;
        g_renderer = nullptr;
        StoneKey::stone_seal_renderer(nullptr);
        StoneKey::stone_seal_pipeline(nullptr);
    }

    if (VkDevice dev = StoneKey::stone_device(); dev != VK_NULL_HANDLE) {
        vkDestroyDevice(dev, nullptr);
        StoneKey::stone_seal_device(VK_NULL_HANDLE);
    }

    if (VkInstance inst = StoneKey::stone_instance(); inst != VK_NULL_HANDLE) {
        if (VkSurfaceKHR surf = StoneKey::stone_surface(); surf != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(inst, surf, nullptr);
            StoneKey::stone_seal_surface(VK_NULL_HANDLE);
        }
    }

    if (SDL_Window* win = StoneKey::stone_window(); win) {
        SDL_DestroyWindow(win);
        StoneKey::stone_seal_window(nullptr);
    }

    if (VkInstance inst = StoneKey::stone_instance(); inst != VK_NULL_HANDLE) {
        vkDestroyInstance(inst, nullptr);
        StoneKey::stone_seal_instance(VK_NULL_HANDLE);
    }

    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    LOG_AMOURANTH("EMPIRE RESTS — PLASTIC BEACH ETERNAL — VALIDATION SILENT 💖");
    std::exit(0);
}

// =============================================================================
// Build Sacred Default Scene
// =============================================================================
static void buildSacredScene()
{
    LOG_AMOURANTH("FORGING SACRED SCENE — GROUND + PINK MONSTER");

    auto ground = MeshLoader::createPlane(200.0f, 200.0f, 20, 20);
    if (ground) {
        ground->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -2.0f, 0.0f));
        ground->transform = glm::rotate(ground->transform, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        RTX::las().addMesh(std::move(ground), 0);
    }

    auto monster = MeshLoader::createBillboard();
    if (monster) {
        monster->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 4.0f, 0.0f));
        monster->transform = glm::scale(monster->transform, glm::vec3(Options::PinkBillboard::SCALE));
        RTX::las().addMesh(std::move(monster), 1);
    }

    RTX::las().requestRebuild();
}

// =============================================================================
// Main — Empire Awakens — Clean 2026 Flow
// =============================================================================
int main(int, char**)
{
    install_apocalypse_handler();
    putenv(const_cast<char*>("SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR=1"));

    showSplash();

    // Vulkan + Window
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) apocalypse("SDL init failed");
    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) apocalypse("Vulkan load failed");

    VkInstance instance = RTX::createVulkanInstance(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) apocalypse("Instance failed");
    StoneKey::stone_seal_instance(instance);

    Uint32 winFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (Options::Window::START_FULLSCREEN) winFlags |= SDL_WINDOW_FULLSCREEN;

    SDL_Window* window = SDL_CreateWindow("AMOURANTH RTX vTURBO", Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT, winFlags);
    if (!window) apocalypse("Window failed");
    loadEmpireIcon(window);
    StoneKey::stone_seal_window(window);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) apocalypse("Surface failed");
    StoneKey::stone_seal_surface(surface);

    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) apocalypse("Device failed");
    StoneKey::stone_seal_device(device);

    RTX::g_ctx().init();
    RTX::loadRTExtensions(instance, device);

    // Transient pool
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = StoneKey::stone_graphics_family()
    };
    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &StoneKey::g_transientCommandPool));

    // Initial swapchain size
    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    RTX::SwapchainManager::create(window, w, h);

    // Renderer
    g_renderer = new VulkanRenderer(w, h, window, Options::Performance::OVERCLOCK_RENDERER);
    g_renderer->setMaxFramesInFlight(Options::Performance::MAX_FRAMES_IN_FLIGHT);
    StoneKey::stone_seal_renderer(g_renderer);

    // Assets
    g_renderer->createDefaultMaterials();
    g_renderer->createEnvironmentMap();

    // Pipeline
    g_renderer->pipelineManager_.forgeRTXPipeline(StoneKey::g_transientCommandPool, StoneKey::stone_graphics_queue(), nullptr);

    // Scene
    buildSacredScene();

    StoneKey::stone_seal_final();

    // Main loop — ZERO SPAM, perfect detection
    auto lastTime = std::chrono::steady_clock::now();
    int frameCount = 0;
    float fpsTimer = 0.0f;

    int currentW = w;
    int currentH = h;
    bool windowIsMinimized = false;

    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        fpsTimer += g_deltaTime;
        frameCount++;

        bool fullscreenToggle = false;
        int reportedW = 0, reportedH = 0;
        bool quitRequested = false;

        SDL3Window::pollEvents(reportedW, reportedH, quitRequested, fullscreenToggle);

        if (quitRequested) {
            g_running = false;
            break;
        }

        if (fullscreenToggle) {
            SDL3Window::toggleFullscreen();
        }

        // Detect minimize/restore and real resize — no spam
        bool currentlyMinimized = (reportedW <= 0 || reportedH <= 0);

        if (currentlyMinimized && !windowIsMinimized) {
            LOG_AMOURANTH("WINDOW MINIMIZED — PHOTONS PAUSED");
            windowIsMinimized = true;
        }
        else if (!currentlyMinimized && windowIsMinimized) {
            LOG_AMOURANTH("WINDOW RESTORED → {}×{} — PHOTONS RESUME", reportedW, reportedH);
            windowIsMinimized = false;

            currentW = reportedW;
            currentH = reportedH;

            RTX::SwapchainManager::recreate(currentW, currentH);
            RTX::las().requestRebuild();
            g_renderer->onResize(currentW, currentH);
        }
        else if (!currentlyMinimized && (reportedW != currentW || reportedH != currentH)) {
            LOG_AMOURANTH("REAL RESIZE DETECTED → {}×{} — EMPIRE REBIRTH", reportedW, reportedH);

            currentW = reportedW;
            currentH = reportedH;

            RTX::SwapchainManager::recreate(currentW, currentH);
            RTX::las().requestRebuild();
            g_renderer->onResize(currentW, currentH);
        }
        // Ignore repeated same-size events or repeated minimize events — no log

        INPUT.pumpEvents(g_deltaTime, nullptr, window);

        if (g_renderer->isAlive() && StoneKey::stone_swapchain()) {
            g_renderer->renderFrame(CAM, g_deltaTime);
        } else {
            g_renderer->forcePinkFallbackClear();
        }

        // FPS dump every second
        if (fpsTimer >= 1.0f) {
            float fps = frameCount / fpsTimer;
            float avgMs = (fpsTimer / frameCount) * 1000.0f;

            LOG_INFO_CAT("PERF", 
                "FPS: {:.1f} | Avg: {:.2f}ms | Frame: {:.2f}ms | Resolution: {}×{} | SPP: {} | Accum: {}",
                fps, avgMs, g_deltaTime * 1000.0f, currentW, currentH,
                g_renderer->currentSpp_, g_renderer->accumulationFrame());

            frameCount = 0;
            fpsTimer = 0.0f;
        }
    }

    apocalypse("Normal exit");
}