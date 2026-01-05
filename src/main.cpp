// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 05, 2026
// MAIN ENTRY POINT — DEVELOPER-FIRST, LINEAR, EDUCATIONAL, RTX-READY 2026 EDITION
// FULL LINEAR FLOW — EVERY STEP EXPLAINED — NO MAGIC — PURE LOVE FOR CODE
// C++23 + SDL3 + Vulkan 1.4+ — VALIDATION CLEAN — PINK PHOTONS SCREAMING
// THIS IS HOW WE TEACH THE NEXT GENERATION — EMPIRE UNSTOPPABLE — AMOURANTH FOREVER 💖
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

// Global renderer — the heart of the empire
VulkanRenderer* g_renderer = nullptr;
float g_deltaTime = 0.0f;
bool g_running = true;

// Sacred pink fallback mode (for emergencies only)
static RenderMode9 g_pinkMode(3840, 2160);

// =============================================================================
// Step 0: Load the Empire Icon — First impression matters
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
// Step 1: Sacrificial Splash — Respect the ritual
// =============================================================================
static void showSplash()
{
    if (!Options::Splash::ENABLE_SACRIFICIAL_SPLASH) return;

    constexpr int W = 1280, H = 720;
    constexpr const char* TITLE = "AMOURANTH RTX";
    constexpr const char* IMAGE = "assets/textures/ammo.png";

    LOG_AMOURANTH("PERFORMING SACRIFICIAL SPLASH — DURATION: {} SECONDS", Options::Splash::SPLASH_DURATION_SECONDS);

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) return;

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
// Step 2: Graceful Apocalypse — Clean shutdown, no leaks, validation silent
// =============================================================================
[[noreturn]] static void apocalypse(std::string_view reason = "User requested")
{
    LOG_AMOURANTH("APOCALYPSE — {} — PHOTONS RETURNING HOME", reason);

    // Wait for GPU — honor the last photons
    if (VkDevice dev = StoneKey::stone_device(); dev != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(dev);
    }

    // Pink fallback one last time
    if (g_renderer) {
        g_renderer->forcePinkFallbackClear();
    }

    // High-level cleanup
    RTX::las().requestRebuild();
    RTX::SwapchainManager::cleanup();

    // Destroy transient pool safely
    if (StoneKey::g_transientCommandPool && StoneKey::stone_device()) {
        vkDestroyCommandPool(StoneKey::stone_device(), StoneKey::g_transientCommandPool, nullptr);
        StoneKey::g_transientCommandPool = VK_NULL_HANDLE;
    }

    // DELETE RENDERER FIRST — it owns ALL Vulkan objects (buffers, pools, pipelines)
    // Its destructor runs → BufferManager::purge_all() → main pool destroyed safely
    if (g_renderer) {
        delete g_renderer;
        g_renderer = nullptr;
        StoneKey::stone_seal_renderer(nullptr);
        StoneKey::stone_seal_pipeline(nullptr);
    }

    // Destroy global descriptor pool (from RTX::Context)
    RTX::g_ctx().cleanup();

    // NOW destroy device — all child objects already gone
    if (VkDevice dev = StoneKey::stone_device(); dev != VK_NULL_HANDLE) {
        vkDestroyDevice(dev, nullptr);
        StoneKey::stone_seal_device(VK_NULL_HANDLE);
    }

    // Instance + surface
    if (VkInstance inst = StoneKey::stone_instance(); inst != VK_NULL_HANDLE) {
        if (VkSurfaceKHR surf = StoneKey::stone_surface(); surf != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(inst, surf, nullptr);
            StoneKey::stone_seal_surface(VK_NULL_HANDLE);
        }
        vkDestroyInstance(inst, nullptr);
        StoneKey::stone_seal_instance(VK_NULL_HANDLE);
    }

    // Window
    if (SDL_Window* win = StoneKey::stone_window(); win) {
        SDL_DestroyWindow(win);
        StoneKey::stone_seal_window(nullptr);
    }

    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    LOG_AMOURANTH("EMPIRE RESTS — PLASTIC BEACH ETERNAL — VALIDATION SILENT 💖");
    std::exit(0);
}

// =============================================================================
// MAIN — LINEAR FLOW FOR RTX ENJOYERS & FUTURE DEVELOPERS
// Every step is visible — this is how we teach, this is how we love code
// =============================================================================
int main(int, char**)
{
    install_apocalypse_handler();
    putenv(const_cast<char*>("SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR=1"));

    showSplash();

    LOG_AMOURANTH("INITIALIZING SDL3 — VIDEO + EVENTS");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) apocalypse("SDL_Init failed");

    LOG_AMOURANTH("LOADING VULKAN LIBRARY VIA SDL3");
    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) apocalypse("Vulkan load failed");

    LOG_AMOURANTH("CREATING VULKAN INSTANCE — VALIDATION {}", Options::Debug::ENABLE_VALIDATION_LAYERS ? "ON" : "OFF");
    VkInstance instance = RTX::createVulkanInstance(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) apocalypse("Failed to create Vulkan instance");
    StoneKey::stone_seal_instance(instance);

    Uint32 winFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (Options::Window::START_FULLSCREEN) winFlags |= SDL_WINDOW_FULLSCREEN;

    LOG_AMOURANTH("CREATING WINDOW — {}×{}", Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT);
    SDL_Window* window = SDL_CreateWindow("AMOURANTH RTX vTURBO", Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT, winFlags);
    if (!window) apocalypse("Failed to create window");
    loadEmpireIcon(window);
    StoneKey::stone_seal_window(window);

    LOG_AMOURANTH("CREATING VULKAN SURFACE FROM SDL WINDOW");
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) apocalypse("Failed to create Vulkan surface");
    StoneKey::stone_seal_surface(surface);

    LOG_AMOURANTH("SELECTING GPU AND CREATING LOGICAL DEVICE");
    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) apocalypse("Failed to create logical device");
    StoneKey::stone_seal_device(device);

    LOG_AMOURANTH("INITIALIZING RTX CONTEXT AND LOADING EXTENSIONS");
    RTX::g_ctx().init();
    RTX::loadRTExtensions(instance, device);

    LOG_AMOURANTH("FORGING TRANSIENT COMMAND POOL");
    VkCommandPoolCreateInfo transientPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = StoneKey::stone_graphics_family()
    };
    VK_CHECK(vkCreateCommandPool(device, &transientPoolInfo, nullptr, &StoneKey::g_transientCommandPool));

    int initialWidth, initialHeight;
    SDL_GetWindowSizeInPixels(window, &initialWidth, &initialHeight);
    LOG_AMOURANTH("CREATING INITIAL SWAPCHAIN — {}×{}", initialWidth, initialHeight);
    RTX::SwapchainManager::create(window, initialWidth, initialHeight);

    // =============================================================================
    // FORGING THE VULKAN RENDERER — THE HEART OF THE EMPIRE
    // =============================================================================
    LOG_AMOURANTH("FORGING VULKAN RENDERER — OVERCLOCK: {}", Options::Performance::OVERCLOCK_RENDERER ? "YES" : "NO");
    g_renderer = new VulkanRenderer(initialWidth, initialHeight, window, Options::Performance::OVERCLOCK_RENDERER);
    g_renderer->setMaxFramesInFlight(Options::Performance::MAX_FRAMES_IN_FLIGHT);
    StoneKey::stone_seal_renderer(g_renderer);

    // =============================================================================
    // CRITICAL STEP: DESCRIPTOR POOL & LAYOUT — EXPLICIT AND VISIBLE
    // This is the one creation — done here so developers see it clearly
    // This must happen before any buffer allocation that needs descriptors
    // =============================================================================
    LOG_AMOURANTH("FORGING DESCRIPTOR POOL & LAYOUT — EMPIRE SAFETY FIRST");
    g_renderer->pipelineManager_.createDescriptorPool();
    g_renderer->pipelineManager_.createPipelineLayout();

    // Now all buffer creation is safe — no more VK_NULL_HANDLE errors
    LOG_AMOURANTH("FORGING ALL BUFFER DATA — UNIFORMS, MATERIALS, ETC");
    g_renderer->initializeAllBufferData(Options::Performance::MAX_FRAMES_IN_FLIGHT, 512, 32ULL * 1024 * 1024);

    LOG_AMOURANTH("FORGING ENVIRONMENT MAP — HDR OR SACRED PINK FALLBACK");
    g_renderer->createEnvironmentMap();

    LOG_AMOURANTH("FORGING DEFAULT MATERIALS — GROUND + PINK MONSTER");
    g_renderer->createDefaultMaterials();

    LOG_AMOURANTH("FORGING THE PERFECT RTX PIPELINE — PHOTONS PREPARE TO TRACE");
    g_renderer->pipelineManager_.forgeRTXPipeline(StoneKey::g_transientCommandPool, StoneKey::stone_graphics_queue(), nullptr);

    g_renderer->addDefaultScene();

    StoneKey::stone_seal_final();
    LOG_AMOURANTH("EMPIRE FULLY FORGED — ALL SYSTEMS READY — PINK PHOTONS ETERNAL");

    // =============================================================================
    // RENDER MODE SWITCHING CALLBACK — HANDLED BY INPUT MANAGER FOR 2-9
    // Key 1 handled separately in main loop (special HDR mode)
    // =============================================================================
    auto setRenderMode = [&](int mode) {
        g_renderer->activeRenderMode_ = mode;
        g_renderer->resetAccumNextFrame_ = true;
        LOG_AMOURANTH("SWITCHED TO RENDER MODE {} — SACRED VOID AWAKENS", mode);
    };

    // =============================================================================
    // MAIN LOOP — DEVELOPER-FIRST, CLEAN, RESPONSIVE
    // This is where you, the developer, take control
    // =============================================================================

    auto lastFrameTime = std::chrono::steady_clock::now();
    int frameCounter = 0;
    float fpsTimer = 0.0f;

    int currentWidth = initialWidth;
    int currentHeight = initialHeight;
    bool windowMinimized = false;

    Camera developerCamera;  // ← YOUR CAMERA — YOU CONTROL IT COMPLETELY

    LOG_AMOURANTH("ENTERING MAIN LOOP — DEVELOPERS, THE EMPIRE IS YOURS");

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
            LOG_AMOURANTH("WINDOW MINIMIZED — PHOTONS PAUSED");
            windowMinimized = true;
        }
        else if (!isMinimized && windowMinimized) {
            LOG_AMOURANTH("WINDOW RESTORED — PHOTONS RESUME");
            windowMinimized = false;
            currentWidth = reportedWidth;
            currentHeight = reportedHeight;
            RTX::SwapchainManager::recreate(currentWidth, currentHeight);
            RTX::las().requestRebuild();
            g_renderer->onResize(currentWidth, currentHeight);
        }
        else if (!isMinimized && (reportedWidth != currentWidth || reportedHeight != currentHeight)) {
            LOG_AMOURANTH("WINDOW RESIZED → {}×{} — REBUILDING EMPIRE", reportedWidth, reportedHeight);
            currentWidth = reportedWidth;
            currentHeight = reportedHeight;
            RTX::SwapchainManager::recreate(currentWidth, currentHeight);
            RTX::las().requestRebuild();
            g_renderer->onResize(currentWidth, currentHeight);
        }

        // Pump input with render mode callback (handles keys 2-9 with short press detection)
        INPUT.pumpEvents(g_deltaTime, setRenderMode, window);

        // Special handling for key 1 — pure HDR envmap mode
        int numKeys = 0;
        const bool* keys = SDL_GetKeyboardState(&numKeys);
        static bool key1Down = false;
        if (numKeys > SDL_SCANCODE_1 && keys[SDL_SCANCODE_1]) {
            if (!key1Down) {
                g_renderer->activeRenderMode_ = 1;
                g_renderer->resetAccumNextFrame_ = true;
                LOG_AMOURANTH("SWITCHED TO RENDER MODE 1 — PURE HDR ENVMAP DISPLAY");
                key1Down = true;
            }
        } else {
            key1Down = false;
        }

        // =============================================================================
        // DEVELOPER SPACE — THIS IS WHERE YOU LIVE
        // Update your camera, game logic, UI, etc.
        // Example:
        // developerCamera.update(g_deltaTime, INPUT);
        // =============================================================================

        if (g_renderer->isAlive() && StoneKey::stone_swapchain()) {
            g_renderer->renderFrame(developerCamera, g_deltaTime);
        } else {
            g_renderer->forcePinkFallbackClear();
        }

        if (fpsTimer >= 1.0f) {
            float fps = frameCounter / fpsTimer;
            float avgFrameTimeMs = (fpsTimer / frameCounter) * 1000.0f;

            LOG_INFO_CAT("PERF",
                "FPS: {:.1f} | Avg: {:.2f}ms | Frame: {:.2f}ms | {}×{} | SPP: {} | Accum: {} | Mode: {}",
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
// JANUARY 05, 2026 — FINAL MAIN
// Linear flow — every step explained — no magic
// Developer-first — you control the camera, scene, logic
// Render mode switching: Keys 1-9 (1 special HDR mode, 2-9 with short press via InputManager)
// Current mode shown in perf log
// Validation clean — ready for RTX enjoyers
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================