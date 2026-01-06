// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 06, 2026
// MAIN ENTRY POINT — POLISHED EMPIRE EDITION
// SACRIFICIAL SPLASH RESTORED — BRIEF BUT GLORIOUS
// EMPTY VOID — FULL RTX PATH TRACING — ACCUMULATION ETERNAL
// PERF COUNTER FORCED — GUARANTEED VISIBLE EVERY SECOND
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/InputManager.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
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

// Total time accumulator for debug
float totalTime_ = 0.0f;

// =============================================================================
// Step 0: Load the Empire Icon — Graceful, beautiful
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
}

// =============================================================================
// Step 1: Sacrificial Splash — Restored, polished, respectful
// =============================================================================
static void showSplash()
{
    if (!Options::Splash::ENABLE_SACRIFICIAL_SPLASH) {
        std::print("[MAIN] Sacrificial splash disabled — instant awakening\n");
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

    std::print("[MAIN] PERFORMING SACRIFICIAL SPLASH — EMPIRE AWAKENS IN GLORY\n");

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        std::print("[MAIN] SDL video unavailable for splash — proceeding gracefully\n");
        return;
    }

    SDL_Window* win = SDL_CreateWindow(TITLE, W, H, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!win) {
        std::print("[MAIN] Failed to create splash window — proceeding\n");
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

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
                std::print("[MAIN] Sacred splash image loaded: {}\n", IMAGE_PATHS[i]);
                break;
            }
        }
    }

    // Fallback: sacred deep pink void
    if (!tex) {
        std::print("[MAIN] No splash image — displaying sacred deep pink void\n");
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

    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count() < Options::Splash::SPLASH_DURATION_SECONDS) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT || 
                (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE && Options::Splash::ALLOW_EARLY_EXIT)) {
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

    std::print("[MAIN] Sacrificial splash complete — empire fully awakened\n");
}

// =============================================================================
// Step 2: Graceful Apocalypse — Clean shutdown
// =============================================================================
[[noreturn]] static void apocalypse(std::string_view reason = "User requested")
{
    std::print("[MAIN] APOCALYPSE — {} — PHOTONS RETURNING HOME\n", reason);

    if (VkDevice dev = StoneKey::stone_device(); dev != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(dev);
    }

    if (g_renderer) {
        delete g_renderer;
        g_renderer = nullptr;
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

    SDL_Quit();

    std::print("[MAIN] EMPIRE RESTS — VALIDATION SILENT 💖\n");
    std::exit(0);
}

// =============================================================================
// MAIN — POLISHED EMPTY SCENE — PERF DATA GUARANTEED EVERY SECOND
// =============================================================================
int main(int, char**)
{
    showSplash();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        apocalypse("SDL_Init failed");
    }

    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        apocalypse("Vulkan load failed");
    }

    Uint32 winFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    SDL_Window* window = SDL_CreateWindow("AMOURANTH RTX vTURBO", Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT, winFlags);
    if (!window) apocalypse("Failed to create window");

    loadEmpireIcon(window);
    StoneKey::stone_seal_window(window);

    int realWidth = 0, realHeight = 0;
    SDL_GetWindowSizeInPixels(window, &realWidth, &realHeight);
    StoneKey::stone_seal_width(realWidth);
    StoneKey::stone_seal_height(realHeight);

    VkInstance instance = RTX::createVulkanInstance(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) apocalypse("Failed to create Vulkan instance");
    StoneKey::stone_seal_instance(instance);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        apocalypse("Failed to create Vulkan surface");
    }
    StoneKey::stone_seal_surface(surface);

    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) apocalypse("Failed to create logical device");
    StoneKey::stone_seal_device(device);

    RTX::g_ctx().init();
    RTX::loadRTExtensions(instance, device);

    VkCommandPoolCreateInfo transientPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = StoneKey::stone_graphics_family()
    };
    VK_CHECK(vkCreateCommandPool(device, &transientPoolInfo, nullptr, &StoneKey::g_transientCommandPool));

    RTX::SwapchainManager::create(window, realWidth, realHeight);

    g_renderer = new VulkanRenderer(realWidth, realHeight, window, Options::Performance::OVERCLOCK_RENDERER);
    g_renderer->setMaxFramesInFlight(Options::Performance::MAX_FRAMES_IN_FLIGHT);
    StoneKey::stone_seal_renderer(g_renderer);

    g_renderer->pipelineManager_.createPipelineLayout();
    g_renderer->pipelineManager_.allocateDescriptorSets();
    g_renderer->pipelineManager_.createRayTracingPipeline();
    StoneKey::stone_seal_pipeline(&g_renderer->pipelineManager_);

    // EMPTY SCENE — PURE VOID
    // No default geometry — developer owns the empire

    g_renderer->pipelineManager_.createShaderBindingTable(
        StoneKey::g_transientCommandPool,
        StoneKey::stone_graphics_queue(),
        nullptr);

    StoneKey::stone_seal_final();

    std::print("[MAIN] EMPIRE FORGED — PURE VOID — FULL RTX PATH TRACING\n");
    std::print("[MAIN] ACCUMULATION ETERNAL — SPP RISES FOREVER\n");

    Camera developerCamera;

    auto lastFrameTime = std::chrono::steady_clock::now();
    int frameCounter = 0;
    float fpsTimer = 0.0f;

    int currentWidth = realWidth;
    int currentHeight = realHeight;

    while (g_running) {
        auto currentTime = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
        lastFrameTime = currentTime;

        totalTime_ += g_deltaTime;

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

        if (isMinimized) {
            std::this_thread::sleep_for(16ms);
            continue;
        }

        if (reportedWidth != currentWidth || reportedHeight != currentHeight) {
            currentWidth = reportedWidth;
            currentHeight = reportedHeight;
            RTX::SwapchainManager::recreate(currentWidth, currentHeight);
            RTX::las().requestRebuild();
            g_renderer->onResize(currentWidth, currentHeight);
        }

        INPUT.pumpEvents(g_deltaTime, nullptr, window);

        if (g_renderer->isAlive() && StoneKey::stone_swapchain()) {
            g_renderer->renderFrame(developerCamera, g_deltaTime);
        }

        // GUARANTEED PERF DATA — EVERY SECOND — FORCED FLUSH
        if (fpsTimer >= 1.0f) {
            float fps = frameCounter / fpsTimer;
            float avgFrameMs = (fpsTimer / frameCounter) * 1000.0f;
            float currentFrameMs = g_deltaTime * 1000.0f;

            std::print("[PERF] FPS: {:.1f} | Avg: {:.2f}ms | Curr: {:.2f}ms | {}×{} | SPP: {} | Accum: {} | Exp: {:.3f} | TotalTime: {:.1f}s\n",
                       fps,
                       avgFrameMs,
                       currentFrameMs,
                       currentWidth, currentHeight,
                       g_renderer ? g_renderer->currentSpp() : 0,
                       g_renderer ? g_renderer->accumulationFrame() : 0,
                       g_renderer ? g_renderer->currentExposure_ : 1.0f,
                       totalTime_);

            fflush(stdout);  // Force immediate visibility

            frameCounter = 0;
            fpsTimer = 0.0f;
        }

        // DEBUG HEARTBEAT — every 0.1s to prove loop is alive
        static float debugTimer = 0.0f;
        debugTimer += g_deltaTime;
        if (debugTimer >= 0.1f) {
            std::print("[HEARTBEAT] Loop alive | delta: {:.4f}s | total: {:.1f}s\n", g_deltaTime, totalTime_);
            fflush(stdout);
            debugTimer = 0.0f;
        }
    }

    apocalypse("Normal exit");

    return 0;
}

// =============================================================================
// JANUARY 06, 2026 — POLISHED EMPIRE EDITION
// Perf data GUARANTEED visible every second
// Debug heartbeat every 0.1s
// Forced flush — no more silence
// The empire speaks — loud and clear
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================