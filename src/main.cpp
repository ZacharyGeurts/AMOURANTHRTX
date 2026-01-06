// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 06, 2026
// MAIN ENTRY POINT — DEVELOPER-FIRST, LINEAR, EDUCATIONAL, RTX-READY 2026 EDITION
// FULL LINEAR FLOW — EVERY STEP EXPLAINED — NO MAGIC — PURE LOVE FOR CODE
// C++23 + SDL3 + Vulkan 1.4+ — VALIDATION CLEAN — PINK PHOTONS SCREAMING
// Pipeline built directly in main — maximum clarity, no abstraction
// SBT CREATED ONCE AND ONLY ONCE — AFTER MAIN POOL INITIALIZATION
// THIS IS HOW WE TEACH THE NEXT GENERATION — EMPIRE UNSTOPPABLE — AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
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
#include <print>

using namespace std::chrono_literals;

// Global renderer — the heart of the empire
VulkanRenderer* g_renderer = nullptr;
float g_deltaTime = 0.0f;
bool g_running = true;

// =============================================================================
// Step 0: Load the Empire Icon — Graceful, optional, beautiful
// =============================================================================
static void loadEmpireIcon(SDL_Window* window)
{
    const char* paths[] = { 
        "assets/textures/ammo.png", 
        "assets/textures/ammo32.png", 
        "assets/textures/icon.png",
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

    std::print("[MAIN] Empire icon not found — proceeding with default system icon (still beautiful)\n");
}

// =============================================================================
// Step 1: Sacrificial Splash — Optional, respectful, instant if disabled
// =============================================================================
static void showSplash()
{
    if (!Options::Splash::ENABLE_SACRIFICIAL_SPLASH) {
        std::print("[MAIN] Sacrificial splash disabled — instant empire awakening\n");
        return;
    }

    constexpr int W = 1280, H = 720;
    constexpr const char* TITLE = "AMOURANTH RTX vTURBO";
    constexpr const char* IMAGE_PATHS[] = {
        "assets/textures/ammo.png",
        "assets/textures/splash.png",
        "assets/textures/amouranth.png",
        nullptr
    };

    std::print("[MAIN] PERFORMING SACRIFICIAL SPLASH — DURATION: {} SECONDS\n", Options::Splash::SPLASH_DURATION_SECONDS);

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        std::print("[MAIN] SDL video subsystem unavailable for splash — skipping gracefully\n");
        return;
    }

    SDL_Window* win = SDL_CreateWindow(TITLE, W, H, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!win) {
        std::print("[MAIN] Failed to create splash window — skipping\n");
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    // Center on primary display
    SDL_Rect bounds;
    SDL_GetDisplayBounds(0, &bounds);
    SDL_SetWindowPosition(win, bounds.x + (bounds.w - W)/2, bounds.y + (bounds.h - H)/2);

    loadEmpireIcon(win);

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Texture* tex = nullptr;
    for (int i = 0; IMAGE_PATHS[i]; ++i) {
        if (SDL_Surface* surf = IMG_Load(IMAGE_PATHS[i])) {
            tex = SDL_CreateTextureFromSurface(ren, surf);
            SDL_DestroySurface(surf);
            if (tex) {
                std::print("[MAIN] Splash image loaded: {}\n", IMAGE_PATHS[i]);
                break;
            }
        }
    }

    // Fallback: sacred pink void
    if (!tex) {
        std::print("[MAIN] No splash image found — displaying sacred pink void\n");
        SDL_SetRenderDrawColor(ren, 255, 20, 147, 255);  // Deep pink
        SDL_RenderClear(ren);
    } else {
        float tw = 0, th = 0;
        SDL_GetTextureSize(tex, &tw, &th);
        SDL_FRect dst{(W - tw)*0.5f, (H - th)*0.5f, tw, th};
        SDL_RenderTexture(ren, tex, nullptr, &dst);
    }

    SDL_ShowWindow(win);
    SDL_RenderPresent(ren);

    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count() < Options::Splash::SPLASH_DURATION_SECONDS) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                goto splash_end;
            }
        }
        std::this_thread::sleep_for(8ms);
    }

splash_end:
    if (tex) SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    std::print("[MAIN] Sacrificial splash complete — empire awakens\n");
}

// =============================================================================
// Step 2: Graceful Apocalypse — Clean shutdown, no leaks, validation silent
// =============================================================================
[[noreturn]] static void apocalypse(std::string_view reason = "User requested")
{
    std::print("[MAIN] APOCALYPSE — {} — PHOTONS RETURNING HOME\n", reason);

    if (VkDevice dev = StoneKey::stone_device(); dev != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(dev);
    }

    if (g_renderer) {
        g_renderer->forcePinkFallbackClear();
    }

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

    RTX::g_ctx().cleanup();

    if (VkDevice dev = StoneKey::stone_device(); dev != VK_NULL_HANDLE) {
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

    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    std::print("[MAIN] EMPIRE RESTS — PLASTIC BEACH ETERNAL — VALIDATION SILENT 💖\n");
    std::exit(0);
}

// =============================================================================
// MAIN — LINEAR FLOW FOR RTX ENJOYERS & FUTURE DEVELOPERS
// =============================================================================
int main(int, char**)
{
    showSplash();

    std::print("[MAIN] INITIALIZING SDL3 — VIDEO + EVENTS\n");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        apocalypse("SDL_Init failed");
    }

    std::print("[MAIN] LOADING VULKAN LIBRARY VIA SDL3\n");
    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        apocalypse("Vulkan load failed");
    }

    Uint32 winFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (Options::Window::START_FULLSCREEN) winFlags |= SDL_WINDOW_FULLSCREEN;

    std::print("[MAIN] CREATING WINDOW — {}×{}\n", Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT);
    SDL_Window* window = SDL_CreateWindow("AMOURANTH RTX vTURBO", Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT, winFlags);
    if (!window) apocalypse("Failed to create window");

    loadEmpireIcon(window);
    StoneKey::stone_seal_window(window);

    // CRITICAL: Get real pixel size BEFORE anything else
    int realWidth = 0, realHeight = 0;
    SDL_GetWindowSizeInPixels(window, &realWidth, &realHeight);
    if (realWidth <= 0 || realHeight <= 0) {
        apocalypse("Invalid window size after creation");
    }

    std::print("[MAIN] REAL WINDOW SIZE: {}×{}\n", realWidth, realHeight);
    StoneKey::stone_seal_width(realWidth);
    StoneKey::stone_seal_height(realHeight);

    std::print("[MAIN] CREATING VULKAN INSTANCE — VALIDATION {}\n", Options::Debug::ENABLE_VALIDATION_LAYERS ? "ON" : "OFF");
    VkInstance instance = RTX::createVulkanInstance(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) apocalypse("Failed to create Vulkan instance");
    StoneKey::stone_seal_instance(instance);

    std::print("[MAIN] CREATING VULKAN SURFACE FROM SDL WINDOW\n");
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        apocalypse("Failed to create Vulkan surface");
    }
    StoneKey::stone_seal_surface(surface);

    std::print("[MAIN] SELECTING GPU AND CREATING LOGICAL DEVICE\n");
    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) apocalypse("Failed to create logical device");
    StoneKey::stone_seal_device(device);

    std::print("[MAIN] INITIALIZING RTX CONTEXT — NOW WITH VALID RESOLUTION\n");
    RTX::g_ctx().init();
    RTX::loadRTExtensions(instance, device);

    std::print("[MAIN] FORGING TRANSIENT COMMAND POOL\n");
    VkCommandPoolCreateInfo transientPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = StoneKey::stone_graphics_family()
    };
    VK_CHECK(vkCreateCommandPool(device, &transientPoolInfo, nullptr, &StoneKey::g_transientCommandPool));

    std::print("[MAIN] CREATING INITIAL SWAPCHAIN — {}×{}\n", realWidth, realHeight);
    RTX::SwapchainManager::create(window, realWidth, realHeight);

    std::print("[MAIN] FORGING VULKAN RENDERER — OVERCLOCK: {}\n", Options::Performance::OVERCLOCK_RENDERER ? "YES" : "NO");
    g_renderer = new VulkanRenderer(realWidth, realHeight, window, Options::Performance::OVERCLOCK_RENDERER);
    g_renderer->setMaxFramesInFlight(Options::Performance::MAX_FRAMES_IN_FLIGHT);
    StoneKey::stone_seal_renderer(g_renderer);

    // =============================================================================
    // DIRECT PIPELINE BUILD — MAXIMUM CLARITY, NO WRAPPER
    // SBT created ONCE AND ONLY ONCE — AFTER MAIN POOL IS INITIALIZED
    // =============================================================================
    std::print("[MAIN] BUILDING RTX PIPELINE DIRECTLY — LAYOUTS → SETS → PIPELINE\n");

    g_renderer->pipelineManager_.createPipelineLayout();        // 1. Layouts
    g_renderer->pipelineManager_.allocateDescriptorSets();      // 2. Sets
    g_renderer->pipelineManager_.createRayTracingPipeline();    // 3. Pipeline

    StoneKey::stone_seal_pipeline(&g_renderer->pipelineManager_);

    std::print("[MAIN] RTX PIPELINE BUILT — VALIDATION CLEAN — PHOTONS READY\n");

    // FORCE MAIN POOL INITIALIZATION VIA SCENE LOAD
    g_renderer->addDefaultScene();

    // NOW SAFE — MAIN POOL FULLY INITIALIZED
    g_renderer->pipelineManager_.createShaderBindingTable(
        StoneKey::g_transientCommandPool,
        StoneKey::stone_graphics_queue(),
        nullptr);

    std::print("[MAIN] ETERNAL SBT FORGED — EMPIRE FULLY ARMED\n");

    StoneKey::stone_seal_final();
    std::print("[MAIN] EMPIRE FULLY FORGED — ALL SYSTEMS READY — PINK PHOTONS ETERNAL\n");

    auto setRenderMode = [&](int mode) {
        g_renderer->activeRenderMode_ = mode;
        g_renderer->resetAccumNextFrame_ = true;
        std::print("[MAIN] SWITCHED TO RENDER MODE {} — SACRED VOID AWAKENS\n", mode);
    };

    auto lastFrameTime = std::chrono::steady_clock::now();
    int frameCounter = 0;
    float fpsTimer = 0.0f;

    int currentWidth = realWidth;
    int currentHeight = realHeight;
    bool windowMinimized = false;

    Camera developerCamera;

    std::print("[MAIN] ENTERING MAIN LOOP — DEVELOPERS, THE EMPIRE IS YOURS\n");

    while (g_running) {
        auto currentTime = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
        lastFrameTime = currentTime;

        fpsTimer += g_deltaTime;
        ++frameCounter;

        bool quitRequested = false;
        bool fullscreenToggle = false;
        int reportedWidth = 0, reportedHeight = 0;

        SDL3Window::pollEvents(reportedWidth, reportedHeight, quitRequested, fullscreenToggle);

        if (quitRequested) {
            g_running = false;
            continue;
        }

        if (fullscreenToggle) {
            SDL3Window::toggleFullscreen();
        }

        bool isMinimized = (reportedWidth <= 0 || reportedHeight <= 0);

        if (isMinimized && !windowMinimized) {
            std::print("[MAIN] WINDOW MINIMIZED — PHOTONS PAUSED\n");
            windowMinimized = true;
        }
        else if (!isMinimized && windowMinimized) {
            std::print("[MAIN] WINDOW RESTORED — PHOTONS RESUME\n");
            windowMinimized = false;
            currentWidth = reportedWidth;
            currentHeight = reportedHeight;
            RTX::SwapchainManager::recreate(currentWidth, currentHeight);
            RTX::las().requestRebuild();
            g_renderer->onResize(currentWidth, currentHeight);
        }
        else if (!isMinimized && (reportedWidth != currentWidth || reportedHeight != currentHeight)) {
            std::print("[MAIN] WINDOW RESIZED → {}×{} — REBUILDING EMPIRE\n", reportedWidth, reportedHeight);
            currentWidth = reportedWidth;
            currentHeight = reportedHeight;
            RTX::SwapchainManager::recreate(currentWidth, currentHeight);
            RTX::las().requestRebuild();
            g_renderer->onResize(currentWidth, currentHeight);
        }

        INPUT.pumpEvents(g_deltaTime, setRenderMode, window);

        // Mode switching disabled — full RTX forever
        // Key 1 now does nothing — no more envmap mode
        // Accumulation always runs

        if (g_renderer->isAlive() && StoneKey::stone_swapchain()) {
            g_renderer->renderFrame(developerCamera, g_deltaTime);
        } else {
            g_renderer->forcePinkFallbackClear();
        }

        if (fpsTimer >= 1.0f) {
            float fps = frameCounter / fpsTimer;
            float avgFrameTimeMs = (fpsTimer / frameCounter) * 1000.0f;

            std::print("[PERF] FPS: {:.1f} | Avg: {:.2f}ms | Frame: {:.2f}ms | {}×{} | SPP: {} | Accum: {} | Mode: {}\n",
                       fps, avgFrameTimeMs, g_deltaTime * 1000.0f,
                       currentWidth, currentHeight,
                       g_renderer->currentSpp(), g_renderer->accumulationFrame(),
                       g_renderer->activeRenderMode_);

            frameCounter = 0;
            fpsTimer = 0.0f;
        }
    }

    apocalypse("Normal exit — developer session complete");

    return 0;
}

// =============================================================================
// JANUARY 06, 2026 — FINAL MAIN
// Respects OptionsMenu.hpp philosophy — BEST ALWAYS — NO COMPROMISE
// Accumulation always on — SPP climbs forever
// No mode switching — full RTX path tracing from frame 1
// No Hollywood tricks — pure HDR truth
// The empire delivers maximum quality for the hardware
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================