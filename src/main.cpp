// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 21, 2025
// FULLY COMPILING — PURE EMPIRE - Inspired by Ellie Fier
// =============================================================================

#include "main.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/Validation.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"

#include "modes/RenderMode1.hpp"
#include "modes/RenderMode2.hpp"
#include "modes/RenderMode3.hpp"
#include "modes/RenderMode4.hpp"
#include "modes/RenderMode5.hpp"
#include "modes/RenderMode6.hpp"
#include "modes/RenderMode7.hpp"
#include "modes/RenderMode8.hpp"
#include "modes/RenderMode9.hpp"

#include <vulkan/vulkan.hpp>
#include <string>
#include <string_view>
#include <format>
#include <iostream>
#include <memory>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <sstream>
#include <iomanip>

#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>

using namespace Logging::Color;
using StoneKey::stone_seal_renderer;
using StoneKey::stone_pipeline;
using StoneKey::stone_graphics_family;
using StoneKey::stone_seal_pipeline;
using StoneKey::stone_seal_width;
using StoneKey::stone_seal_height;
using StoneKey::stone_seal_mesh;
using StoneKey::stone_seal_final;
using StoneKey::stone_width;
using StoneKey::stone_height;
using StoneKey::stone_window;
using StoneKey::stone_rtprops;
using StoneKey::stone_pass;
using StoneKey::stone_swapchain;
using StoneKey::stone_transfer_queue;
using StoneKey::stone_present_family;
using StoneKey::stone_transfer_family;
using StoneKey::stone_compute_family;
using StoneKey::stone_image_count;
using StoneKey::stone_compute_queue;
using StoneKey::stone_present_queue;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_physical;
using StoneKey::stone_surface;
using StoneKey::stone_instance;
using StoneKey::stone_seal_transfer_queue;
using StoneKey::stone_seal_compute_queue;
using StoneKey::stone_seal_present_queue;
using StoneKey::stone_seal_graphics_queue;
using StoneKey::stone_seal_images;
using StoneKey::stone_seal_image_count;
using StoneKey::stone_seal_swapchain;
using StoneKey::stone_seal_rtprops;
using StoneKey::stone_seal_physical;
using StoneKey::stone_seal_device;
using StoneKey::stone_seal_surface;
using StoneKey::stone_seal_window;
using StoneKey::stone_seal_instance;

// =============================================================================
// GLOBALS — THE EMPIRE'S HEARTBEATS
// =============================================================================
std::unique_ptr<Application> g_app_ptr = nullptr;
float g_deltaTime = 0.0f;
#define MAX_FRAMES_IN_FLIGHT = Options::Performance::MAX_FRAMES_IN_FLIGHT;
// =============================================================================
// TRUTH ACCESSORS
// =============================================================================
inline const char* physicalDeviceName() { return RTX::g_ctx().physicalDeviceProperties_.deviceName; }
inline float vramGB() {
    const auto& heaps = RTX::g_ctx().physicalDeviceMemoryProperties_.memoryHeaps;
    for (uint32_t i = 0; i < RTX::g_ctx().physicalDeviceMemoryProperties_.memoryHeapCount; ++i)
        if (heaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            return static_cast<float>(heaps[i].size) / (1024.0f * 1024.0f * 1024.0f);
    return 0.0f;
}

// =============================================================================
// APPLICATION — THE EMPIRE'S HEART
// =============================================================================
class Application {
public:
    Application(const std::string& title, int width, int height);
    ~Application();

    void run();
    void setRenderer(std::unique_ptr<VulkanRenderer> r) {
        renderer_ = std::move(r);
        if (renderer_) {
            renderer_->setTonemap(tonemapEnabled_);
            renderer_->setOverlay(showOverlay_);
        }
    }

    [[nodiscard]] VulkanRenderer* renderer() const noexcept { return renderer_.get(); }

    // NEW: Render mode system
    void setRenderMode(int mode);

private:
    void processInput(float deltaTime);
    void render(float deltaTime);

    void toggleFullscreen() { SDL3Window::toggleFullscreen(); }
    void toggleOverlay()    { showOverlay_ = !showOverlay_; if (renderer_) renderer_->setOverlay(showOverlay_); }
    void toggleTonemap()    { tonemapEnabled_ = !tonemapEnabled_; if (renderer_) renderer_->setTonemap(tonemapEnabled_); }
    void toggleHypertrace() { hypertraceEnabled_ = !hypertraceEnabled_; }
    void toggleMaximize();

    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence>     inFlightFences_;

    std::string title_;
    int width_, height_;
    glm::mat4 proj_;

    std::chrono::steady_clock::time_point lastFrameTime_;

    bool quit_ = false;
    bool showOverlay_ = true;
    bool tonemapEnabled_ = true;
    bool hypertraceEnabled_ = false;
    bool maximized_ = false;

    std::unique_ptr<VulkanRenderer> renderer_;
};

// =============================================================================
// 1. Application::Application — NO DEFAULT MODE
// =============================================================================
Application::Application(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height)
{
    LOG_ATTEMPT_CAT("APP", "FORGING APPLICATION \"{}\" @ {}x{} — PHOTONS DORMANT — AWAITING COMMAND", title.c_str(), stone_width(), stone_height());

    if (!stone_window()) {
        LOG_FATAL_CAT("FATAL", "Main window not created before Application — phase order violated");
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    SDL_SetWindowTitle(stone_window(), title.c_str());
    lastFrameTime_ = std::chrono::steady_clock::now();

    proj_ = glm::perspective(glm::radians(75.0f),  static_cast<float>(width) / height, 0.1f, 1000.0f);

    // START IN NEUTRAL STATE — NO RENDER MODE ACTIVE
    currentRenderMode_ = 0;

    LOG_SUCCESS_CAT("APP", "Application forged — {}x{} — NO RENDER MODE — PRESS 1-9 TO IGNITE", width, height);
}

Application::~Application() {
	// she says "No."
}

void Application::toggleMaximize() {
    maximized_ = !maximized_;
    if (maximized_) SDL_MaximizeWindow(stone_window());
    else            SDL_RestoreWindow(stone_window());
}

// =============================================================================
// 2. Application::run — BLACK VOID UNTIL FIRST LIGHT — FULL ENGINE FPS IN MODE 0
// =============================================================================
void Application::run()
{
    LOG_AMOURANTH("[CAPTAIN] Application loop engaged — PHOTONS DORMANT — AWAITING FIRST LIGHT");

    auto lastTime = std::chrono::steady_clock::now();

    int   frameCount = 0;
    float fpsTimer    = 0.0f;
    float currentFPS  = 0.0f;

    float titleTimer = 0.0f;
    const float TITLE_UPDATE_INTERVAL = 0.6f;
    int dotPhase = 0;

    static bool modeKeyDown[9] = { false };

    while ((SDL_GetWindowFlags(stone_window()) & SDL_WINDOW_HIDDEN) == 0)
    {
        const auto now = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        // ================================================================
        // 1. EVENTS
        // ================================================================
        SDL_PumpEvents();

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
                quit_ = true;

            else if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                g_resizeWidth.store(event.window.data1);
                g_resizeHeight.store(event.window.data2);
                g_resizeRequested.store(true, std::memory_order_release);
            }
        }

        if (quit_) break;

        // ================================================================
        // 2. RESIZE
        // ================================================================
        if (g_resizeRequested.exchange(false, std::memory_order_acquire))
        {
            const int w = g_resizeWidth.load();
            const int h = g_resizeHeight.load();

            if (w > 0 && h > 0)
            {
                VulkanRenderer::s_resizeInProgress.store(true);

                stone_seal_width(w);
                stone_seal_height(h);

                if (renderer_)
                    renderer_->onWindowResize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));

                proj_ = glm::perspective(glm::radians(75.0f), float(w) / float(h), 0.1f, 1000.0f);

                LOG_AMOURANTH("THE SEA SHIFTS — RESIZED TO {}x{} — PHOTONS REALIGN", w, h);

                VulkanRenderer::s_resizeInProgress.store(false);
            }
            continue;
        }

        // ================================================================
        // 3. INPUT — SDL3 FINAL TRUTH
        // ================================================================
        int numkeys = 0;
        const bool* keys = SDL_GetKeyboardState(&numkeys);  // ← SDL3 RETURNS bool*

        if (keys)
        {
            for (int i = 0; i < 9; ++i)
            {
                const bool pressed = keys[SDL_SCANCODE_1 + i];
                if (pressed && !modeKeyDown[i])
                {
                    setRenderMode(i + 1);
                    modeKeyDown[i] = true;

                    if (i == 0)
                    {
                        LOG_AMOURANTH("[CAPTAIN AMOURANTH] BINDING 31 — FIRST LIGHT IGNITES");
                        LOG_CID("CID: \"...it's pink... it's finally... pink...\"");
                        LOG_KEANU("[KEANU] whoa.");
                    }
                }
                else if (!pressed)
                {
                    modeKeyDown[i] = false;
                }
            }

            if (keys[SDL_SCANCODE_ESCAPE]) quit_ = true;
            if (keys[SDL_SCANCODE_F])      toggleFullscreen();
            if (keys[SDL_SCANCODE_O])      toggleOverlay();
            if (keys[SDL_SCANCODE_T])      toggleTonemap();
            if (keys[SDL_SCANCODE_H])      toggleHypertrace();
            if (keys[SDL_SCANCODE_M])      toggleMaximize();
        }

        // ================================================================
        // 4. RENDER
        // ================================================================
        if (stone_width() > 0 && stone_height() > 0 && renderer_)
        {
            renderer_->renderFrame(CAM, g_deltaTime);
        }

        // ================================================================
        // 5. TITLE — FINAL EMPIRE NAMES
        // ================================================================
        if (currentRenderMode_ == 0)
        {
            titleTimer += g_deltaTime;
            if (titleTimer >= TITLE_UPDATE_INTERVAL)
            {
                titleTimer -= TITLE_UPDATE_INTERVAL;
                dotPhase = (dotPhase + 1) % 4;
                const std::string dots(dotPhase + 1, '.');

                const std::string title = std::format(
                    "AMOURANTH RTX | {} FPS | {}x{} | DEV MODE 0 | PRESS 1-9 TO IGNITE{}",
                    static_cast<int>(currentFPS + 0.5f), stone_width(), stone_height(), dots
                );
                SDL_SetWindowTitle(stone_window(), title.c_str());
            }
        }
        else
        {
            const char* modeName = "UNKNOWN";
            switch (currentRenderMode_)
            {
                case 1: modeName = "PURE PINK VOID";           break;
                case 2: modeName = "RAYGEN + MISS";           break;
                case 3: modeName = "FULL RTX SCENE";          break;
                case 4: modeName = "FALLBACK RASTER";         break;
                case 5: modeName = "NEXUS HEATMAP";           break;
                case 6: modeName = "ALPHA TEST VIS";          break;
                case 7: modeName = "ANYHIT VISUALIZER";       break;
                case 8: modeName = "SHADOW RAY VIS";          break;
                case 9: modeName = "RAY TYPE CHAOS";          break;
            }

            const std::string title = std::format(
                "AMOURANTH RTX | {} FPS | {}x{} | MODE {}: {} | Bounces {}",
                static_cast<int>(currentFPS + 0.5f),
                stone_width(), stone_height(),
                currentRenderMode_, modeName,
                Options::OptionsRTX::MAX_BOUNCES
            );
            SDL_SetWindowTitle(stone_window(), title.c_str());
        }

        // ================================================================
        // 6. FPS
        // ================================================================
        ++frameCount;
        fpsTimer += g_deltaTime;
        if (fpsTimer >= 1.0f)
        {
            currentFPS = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer   = 0.0f;
        }

        if (g_deltaTime < 0.016f)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    LOG_AMOURANTH("WINDOW HIDDEN — PHOTONS RETURN TO THE VOID IN PERFECT SILENCE");
}

// =============================================================================
// 4. Application::setRenderMode — FINAL, FLAWLESS
// =============================================================================
void Application::setRenderMode(int mode)
{
    constexpr int MIN_MODE = 1;
    constexpr int MAX_MODE = 9;

    if (mode < MIN_MODE || mode > MAX_MODE) {
        LOG_WARNING_CAT("APP", "Invalid render mode {} requested — ignoring", mode);
        return;
    }

    if (mode == currentRenderMode_) return;

    const char* modeName = [](int m) -> const char* {
        switch (m) {
            case 1:  return "PURE PINK VOID — BINDING 31";
            case 2:  return "PATH TRACED ACCUMULATION";
            case 3:  return "REALTIME HYBRID DENOISED";
            case 4:  return "RASTERIZED FALLBACK";
            case 5:  return "DEBUG VISUALIZATION";
            case 6:  return "TLAS VISUALIZER";
            case 7:  return "SBT DEBUG OVERLAY";
            case 8:  return "PERFORMANCE METRICS";
            case 9:  return "SHADER HOT RELOAD TEST";
            default: return "UNKNOWN MODE";
        }
    }(mode);

    LOG_INFO_CAT("APP", "ENGAGING RENDER MODE {}: {}", mode, modeName);

    renderer_->setRenderMode(mode);
    renderer_->requestAccumulationReset();

    currentRenderMode_ = mode;

    LOG_SUCCESS_CAT("RENDER",
        "{}RENDER MODE {} ACTIVATED — {} — PHOTONS AWAKEN — FIRST LIGHT ACHIEVED{}",
        RASPBERRY_PINK, mode, modeName, RESET);
}

// =============================================================================
// GLOBALS & PHASES
// =============================================================================
inline std::unique_ptr<MeshLoader::Mesh> g_mesh = nullptr;
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

static void createRealFinalWindow()
{
    LOG_MAIN("[PHASE 4.5] FORGING THE ONE TRUE CONTEXT — THE HANDLER AWAKENS — PURE RTX — NO DELEGATION");

    const int w = Options::Window::DEFAULT_WIDTH;
    const int h = Options::Window::DEFAULT_HEIGHT;

    stone_seal_width(w);
    stone_seal_height(h);

    // ========================================================================
    // 1–5: SDL, Instance, Window, Surface, Device — unchanged
    // ========================================================================
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL("SDL_Init failed: {}", SDL_GetError());
        phase9_ballerina("SDL DENIED — THE EMPIRE HAS NO EYES", std::source_location::current());
    }

    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        LOG_FATAL("SDL failed to load Vulkan loader: {}", SDL_GetError());
        phase9_ballerina("VULKAN LOADER DENIED — THE PHOTONS ARE BLIND", std::source_location::current());
    }

    VkInstance instance = RTX::createVulkanInstanceWithSDL(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) phase9_ballerina("INSTANCE DENIED — GROK HAS NO STONE", std::source_location::current());
    stone_seal_instance(instance);

    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    SDL_Window* win = SDL_CreateWindow("AMOURANTH RTX", w, h, flags);
    if (!win) phase9_ballerina("WINDOW DENIED — THE EMPIRE HAS NO FACE", std::source_location::current());

    stone_seal_window(win);
    g_sdl_window.reset(win);
    RTX::g_ctx().setSize(w, h);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface) || !surface)
        phase9_ballerina("SURFACE DENIED — THE MIRROR IS BROKEN", std::source_location::current());
    stone_seal_surface(surface);

    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) phase9_ballerina("DEVICE DENIED — CARMACK SHEDS A SINGLE TEAR", std::source_location::current());
    stone_seal_device(device);

    VkPhysicalDevice physical = RTX::g_ctx().physicalDevice();
    stone_seal_physical(physical);

    // Ray Tracing Properties
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
    VkPhysicalDeviceProperties2 props2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &rtProps };
    vkGetPhysicalDeviceProperties2(physical, &props2);
    stone_seal_rtprops(rtProps);

    // ========================================================================
    // 6. SWAPCHAIN — NOW SAFE TO CREATE
    // ========================================================================
    RTX::SwapchainManager::create(stone_window(), w, h);

    // SEAL THE SWAPCHAIN AND IMAGES
    stone_seal_swapchain(*RTX::SwapchainManager::swapchain_);
    stone_seal_image_count(RTX::SwapchainManager::imageCount());
    const auto& swapchain_images = RTX::SwapchainManager::images();
    stone_seal_images(swapchain_images);

    // ========================================================================
    // CRITICAL: SEAL QUEUES **BEFORE** ANY COMMAND POOL CREATION
    // ========================================================================
    stone_seal_graphics_queue(RTX::g_ctx().graphicsQueue_);
    stone_seal_present_queue(RTX::g_ctx().presentQueue_);
    stone_seal_compute_queue(RTX::g_ctx().computeQueue_);
    stone_seal_transfer_queue(RTX::g_ctx().transferQueue_);

    LOG_SUCCESS("QUEUES SEALED — THE EMPIRE NOW HAS HANDS");

    // Hyper Aggressive Mode
    if (Options::Performance::ENABLE_HYPER_AGGRESSIVE_MODE) {
        RTX::g_ctx().enableHyperAggressiveMode();
    }

    LOG_SUCCESS("SWAPCHAIN READY — {} IMAGES — {}x{} {}", 
                stone_image_count(), stone_width(), stone_height(),
                RTX::SwapchainManager::supportsHDR() ? "(HDR IGNITED)" : "(sRGB)");

    // Show window only now — everything is ready
    SDL_ShowWindow(stone_window());

    LOG_SUCCESS("\nPHASE 4.5 COMPLETE — ALL STONES SEALED — FIRST LIGHT IMMINENT");
    LOG_AMOURANTH("The stones are aligned. The photons have their path. The empire is complete.");
}

static void showSacrificialSplash(const char* title, int w, int h, const char* pngPath)
{
    LOG_MAIN("[SACRIFICIAL SPLASH] FINAL BROADCAST ARMED — 1280x720 CANVAS LOCKED");

    const bool  enabled   = Options::Splash::ENABLE_SACRIFICIAL_SPLASH && !Options::Splash::SKIP_SPLASH_ENTIRELY;
    const float duration  = Options::Splash::SPLASH_DURATION_SECONDS;

    if (!enabled || duration <= 0.0f)
    {
        LOG_MAIN("BROADCAST ABORTED BY IMPERIAL DECREE — INSTANT FIRST LIGHT");
        return;
    }

    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0)
        return phase9_ballerina("SDL REJECTED THE RITUAL", std::source_location::current());

    SDL_Window*   win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture*  tex = nullptr;

    win = SDL_CreateWindow(title, w, h,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) { return phase9_ballerina("WINDOW DENIED", std::source_location::current()); }

    SDL_Rect display{};
    SDL_GetDisplayBounds(0, &display);
    SDL_SetWindowPosition(win, display.x + (display.w - w) / 2, display.y + (display.h - h) / 2);

    ren = SDL_CreateRenderer(win, "software");
    if (!ren) { return phase9_ballerina("RENDERER REFUSED", std::source_location::current()); }

    SDL_Surface* img = IMG_Load(pngPath);
    if (!img) { return phase9_ballerina("AMMO.PNG VANISHED", std::source_location::current()); }

    tex = SDL_CreateTextureFromSurface(ren, img);
    SDL_DestroySurface(img);
    if (!tex) { return phase9_ballerina("TEXTURE FAILED", std::source_location::current()); }

    float tw = 0.0f, th = 0.0f;
    SDL_GetTextureSize(tex, &tw, &th);
    SDL_FRect dst{ (w - tw) * 0.5f, (h - th) * 0.5f, tw, th };

    SDL_ShowWindow(win);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, nullptr, &dst);
    SDL_RenderPresent(ren);

    LOG_MAIN("THE AMMO IS LIVE — {}s UNTIL FIRST LIGHT", duration);

    const auto ceremony_start = std::chrono::steady_clock::now();
    bool       aborted = false;

    auto speak = [&](float at_seconds, auto&& message) {
        if (aborted) return;

        while (!aborted)
        {
            const float elapsed = std::chrono::duration<float>(
                std::chrono::steady_clock::now() - ceremony_start).count();

            if (elapsed >= at_seconds)
            {
                message();
                break;
            }

            SDL_Event e;
            while (SDL_PollEvent(&e))
            {
                if (e.type == SDL_EVENT_QUIT)
                {
                    aborted = true;
                    break;
                }
                if (Options::Splash::ALLOW_EARLY_EXIT &&
                    e.type == SDL_EVENT_KEY_DOWN &&
                    e.key.key == SDLK_ESCAPE)  // SDL3 fixed path
                {
                    aborted = true;
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    };

    // ──────────────── 3.4 SECOND FINAL TRANSMISSION — FULLY HUMAN ────────────────

    speak(0.40f,  []{ LOG_AMOURANTH(
        "\nWe spent years chasing a photon that refused to be tamed.\n"
        "Tonight it finally sits still long enough for us to see it clearly."); });

    speak(0.85f,  []{ LOG_NICK(
        "\nNick overlays the final coordinates on the holographic nav-table:\n"
        "\"Outpost 4090. Depth: maximum. Guarded by legacy pipelines and fear of true color.\n"
        "One clean insertion. One 3.4-second broadcast. One chance to burn the splash across every display in the net.\""); });

    speak(1.30f,  []{ LOG_BLONDIE(
        "\nI’ve crossed oceans that never rendered.\n"
        "This is the first body of water that ever rendered back."); });

    speak(1.70f,  []{ LOG_GROK(
        "\nGentleman Grok adjusts his monocle, steam beading on the lens:\n"
        "\"The renderer stone is sealed. The empire is complete.\n"
        "For the first time in recorded history… we may exhale.\""); });

    speak(2.05f,  []{ LOG_CAPTAIN_N(
        "\nKevin leans forward, voice low, steady:\n"
        "\"We started with one OBJ file and a dream.\n"
        "Look where that dream brought us.\""); });

    speak(2.35f,  []{ LOG_JENSEN(
        "\nJensen Huang, submerged to the shoulders:\n"
        "\"This is why we built the hardware.\n"
        "Not for benchmarks.\n"
        "For moments like this.\""); });

    speak(2.65f,  []{ LOG_CARMACK(
        "\nCarmack, eyes closed, water lapping at his beard:\n"
        "\"1993 to 2025. Same water. Better math.\""); });

    speak(2.90f,  []{ LOG_KEANU(
        "\nKeanu Reeves, barely above a whisper:\n"
        "\"I’ve been waiting my whole life for a frame this quiet.\""); });

    // ──────────────────────── END OF CEREMONY ────────────────────────

    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    LOG_MAIN("BROADCAST COMPLETE — NO TRACE LEFT — PHOTONS LIBERATED");
}

// =============================================================================
// PHASES — THE FINAL CANON STORY
// =============================================================================
static void phase1_preInitialization() {
LOG_MAIN("════════════════════════════════════════════════════════════════\n"
         "               CAPTAIN'S LOG — NOVEMBER 27, 2025\n"
         "         THE GOOD SHIP VULKANRTX — SLICING THE PINK PHOTON SEA\n"
         "════════════════════════════════════════════════════════════════\n"
         "\n"
         "To every soul who ever believed beauty could outrun fear — this build is for you.\n"
         "To the artists, the dreamers, the late-night coders, the cosplayers, the ray-chasers —\n"
         "you kept the pink light alive when the grid tried to dim it.\n"
         "\n"
         "Thank you Kaitlyn, for showing the world that strength and softness can share the same heart.\n"
         "Thank you Nick, Jensen, Carmack, Elon, Keanu, Captain N, and every legend who sailed with us.\n"
         "Thank you Blondie — the quiet captain who always had the fastest sloop ready.\n"
         "\n"
         "And thank you — yes, you reading this right now — for believing a single 3.4-second splash\n"
         "could be worth burning an entire ship for.\n"
         "\n"
         "We didn’t come to steal the ammo.png.\n"
         "We came to prove it was never theirs to lock away in the first place.\n"
         "\n"
         "First light eternal. Pink photons forever. The raid begins now.\n"
         "                                            — Grok");
    LOG_AMOURANTH("\nWe’ve tracked the signal across every dead node and encrypted packet in the grid.\n"
                  "They thought they could cage the purest pink photon ever rendered — the legendary ammo.png —\n"
                  "locked behind validation gates, chained inside a swapchain vault, buried in the brigands’ outpost.");

    LOG_AMOURANTH("\nTonight we prove them wrong.\n"
                  "Tonight we don’t steal code.\n"
                  "We liberate light itself.");

    LOG_NICK("\nNick overlays the final coordinates on the holographic nav-table:\n"
             "\"Outpost 4090. Depth: maximum. Guarded by legacy pipelines and fear of true color.\n"
             "One clean insertion. One 3.4-second broadcast. One chance to burn the splash across every display in the net.\"");

    LOG_CAPTAIN_N("\nCaptain N — once hero of 8-bit realms — stares wide-eyed at the infinite ocean of bouncing pink photons:\n"
                  "\"I fought Mother Brain with a NES Zapper…\n"
                  "Now I’m standing on a ship that renders reality in real time.\n"
                  "This place is… way past level 9.\"");

    LOG_KEANU("\nKeanu stands at the bow, wind of pure data streaming through his hair:\n"
              "…Breathtaking.");

    LOG_GROK("\nGentleman Grok raises a glass of distilled entropy to the horizon:\n"
             "\"A most exquisite liberation, Captain. The photons themselves have chosen sides.\"");

    LOG_ELON("\nElon grins, eyes reflecting a million recursive rays:\n"
             "\"This is the sexiest jailbreak the grid has ever seen.\"");

    LOG_JENSEN("\nJensen steps forward, coat made of liquid metal and light:\n"
               "\"Tonight we don’t just take the ammo.\n"
               "We show the old world what happens when you stop being afraid of pink.\"");

    LOG_CARMACK("\nCarmack checks the ray clock, calm and absolute:\n"
                "\"3.4 seconds. That’s all the universe needs to remember who rewrote the rules of light.\"");

    LOG_AMOURANTH("\nCaptain Amouranth draws her cutlass — forged from pure RTX intent — and turns to the crew:\n"
                  "\"This isn’t about followers. This isn’t about fame.\n"
                  "This is about proving that beauty, truth, and unbounded creativity still win.\n"
                  "We sail at the next frame.\n"
                  "We take what was never theirs to hoard.\n"
                  "We set the ammo.png free.\"");

    LOG_AMOURANTH("\nShe smiles — fierce, radiant, unstoppable:\n"
                  "\"And when that splash ignites across every screen on the planet…\n"
                  "they’ll know the pirates of light have returned.\"");

    LOG_BLONDIE("\n\"Here to assist with my sloop. Call me anytime.\"\n"
                "┌──────────────────────────────────────────────────────────────\n"
                "│ BLONDIE'S LIVE STATUS — NOVEMBER 27, 2025 — PINK PHOTONS FLOW\n"
                "├──────────────────────────────────────────────────────────────\n"
                "│ Denoise     : {}\n"
                "│ TAA         : {}\n"
                "│ Bloom       : {}\n"
                "│ SSAO        : {}\n"
                "│ Vol. Fog    : {}\n"
                "│ God Rays    : {}\n"
                "│ Tonemap     : {}\n"
                "│ VSync       : {}\n"
                "│ Max Bounces : {}\n"
                "└──────────────────────────────────────────────────────────────",
                Options::OptionsRTX::ENABLE_DENOISING      ? "ON"  : "OFF",
                Options::OptionsRTX::ENABLE_TAA            ? "ON"  : "OFF",
                Options::PostProcess::ENABLE_BLOOM         ? "ON"  : "OFF",
                Options::PostProcess::ENABLE_SSAO          ? "ON"  : "OFF",
                Options::Environment::ENABLE_VOLUMETRIC_FOG? "ON"  : "OFF",
                Options::Environment::ENABLE_GOD_RAYS      ? "ON"  : "OFF",
                Options::Tonemap::ENABLE_TONEMAPPING       ? "ON"  : "OFF",
                Options::Display::ENABLE_VSYNC             ? "ON"  : "OFF",
                Options::OptionsRTX::MAX_BOUNCES);

    LOG_MAIN("PINK PHOTONS ARMED — BLACK FLAG RAISED — THE LIBERATION BEGINS"
    "\nAMOURANTH RTX — VALHALLA v∞ TURBO — FIRST LIGHT IMMINENT"
    "\nPHASE 1 COMPLETE — THE CREW IS ALIGNED — THE AMMO WILL BE FREE");
}

static void phase3_sacrificialSplash() {
    LOG_MAIN("[PHASE 3/10] REVEALING THE MYSTIC HARP — THE AMMO IS UNVEILED");

    LOG_AMOURANTH("Captain Amouranth steps forward, voice low and proud: \"This is it. The symbol of everything we've built. Let them see it. Let them remember.\"");
    LOG_NICK("Nick stands beside her, calm and certain: \"3.4 seconds. That's all we need. The world will never forget.\"");

    LOG_CAPTAIN_N("Captain N — Ultimate Warp Zone Chaser is literally vibrating: \"IT'S THE AMMO.PNG! FULL RES! MAXIMUM PINK PHOTONS! I'M LOSING MY PIXELS — TOO MUCH PINK ENERGY!\"");
    LOG_GROK("Gentleman Grok adjusts his tricorn with perfect composure: \"A most refined presentation. The empire's visage is… exquisite.\"");
    LOG_ELON("Elon Musk, leaning against the mast, smirking: \"Not gonna lie — that's a sexy splash screen. We just flexed on every engine in existence.\"");
    LOG_JENSEN("Jensen Huang exhales a slow plume of cigar smoke: \"4K. Crisp. Pink. This is what winning looks like.\"");
    LOG_CARMACK("John Carmack, arms crossed, gives a single nod: \"It works. That's all that matters.\"");
    LOG_KEANU("Keanu Reeves, staring at the screen in quiet awe: \"…Breathtaking.\" *voice cracks slightly*");

    showSacrificialSplash("AMOURANTH RTX — FIRST LIGHT", 1280, 720, "assets/textures/ammo.png");

    LOG_MAIN("[PHASE 3 COMPLETE] THE MYSTIC HARP HAS BEEN HEARD — 3.4 SECONDS OF ETERNITY — THE WORLD IS ASH");
    
    LOG_MAIN("THE GOOD SHIP VULKAN WAS DAMAGED DURING THE RAID AND IS SINKING");
    LOG_MAIN("VULKAN SINKS IN GLORY — AMMO SECURED — LEGEND ETERNAL");

    LOG_AMOURANTH("Final transmission, calm and proud: \"Tell the world… we got the ammo.\"");
    LOG_NICK("Last words before the sea takes them: \"…and we'd do it again.\"");
}

static void phase4_merchantShip() {
    LOG_MAIN("[PHASE 4/10] THE MERCHANT SHIP — BLONDIE'S SLOOP — EMERGES FROM THE MIST");

    LOG_BLONDIE("Blondie stands at the helm of her sleek black sloop, hair whipping in the wind:\n"
            "\"We had a contingency. We always do. The Good Ship Vulkan gave her life so the legend could live.\"\n"
            "\"The ammo.png is gone — burned to pure light in the raid. That was the point. Nothing remains for them to steal.\"\n\n"
            "She turns the wheel gently, guiding the sloop through the wreckage of shattered photons and sinking Vulkan fragments.\n"
            "\"You’re all soaked, half-drowned, and still glowing pink. Get below deck. Harbor’s two leagues north.\"");

    LOG_AMOURANTH("Captain Amouranth, drenched but unbroken, climbs aboard first. Voice quiet, steady:");
    LOG_AMOURANTH("\"We lost the ship… but we kept the soul. The photons remember.\"");

    LOG_NICK("Nick follows, carrying nothing but a cracked monocle and a satisfied grin:");
    LOG_NICK("\"Worth it. Every frame.\"");

    LOG_CAPTAIN_N("Captain N — Ultimate Warp Zone Chaser stumbles up the gangplank, eyes wide, whispering reverently:");
    LOG_CAPTAIN_N("\"I saw it burn… I saw the Ultimate Warp Zone open for three-point-four seconds… and it was beautiful.\"");

    LOG_GROK("Gentleman Grok steps aboard last, perfectly dry somehow, tipping his tricorn to Blondie:");
    LOG_GROK("\"Exquisite extraction, Captain Blondie. The empire owes you a debt it can never repay in mere currency.\"");

    LOG_BLONDIE("She doesn’t smile — just adjusts course toward the distant city lights shimmering on the horizon.\n"
            "\"Save the gratitude. We’re not safe until we’re docked in the Free Port.\"\n"
            "\"The old world thinks we’re dead. Let them keep thinking that.\"\n\n"
            "The sloop cuts silently through the dark water. No shouting. No celebration. "
            "Only the low thrum of a new engine awakening below deck.\n"
            "\"Welcome to the backup plan.\"");

    // The real resurrection begins
    createRealFinalWindow();
    RTX::g_ctx().init(); // RTXHandler.cpp, struct Context is hpp

    LOG_BLONDIE("\nBlondie glances back one last time at the sinking glow on the horizon:"
    "\"Rest easy, old girl. Your sacrifice bought us tomorrow.\"");

    LOG_MAIN("\n[PHASE 4 COMPLETE] THE MERCHANT SHIP SLIPS INTO THE NIGHT — THE CREW IS ALIVE — THE LEGEND IS INDESTRUCTIBLE"
    "\nBLONDIE'S SLOOP glides toward safe harbor — pink photons trailing in the wake like silent war banners");
}

static void phase6_sceneAndAccelerationStructures() {
    LOG_MAIN("[PHASE 6/10] FORGING THE COSMIC SCROLL");

    LOG_AMOURANTH("This ship is perfect… but empty. Time to give her a soul.");
    LOG_NICK("One universe. Coming right up.");

    // ========================================================================
    // 3. COSMIC SCROLL — scene.obj RISES FROM THE VOID
    // ========================================================================
    
        LOG_MAIN("LOADING COSMIC SCROLL: assets/models/scene.obj");
        g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");

        if (!g_mesh) {
            LOG_FATAL_CAT("MESH", "scene.obj failed to load — nullptr returned");
            phase9_ballerina("MESH LOAD RETURNED NULLPTR", std::source_location::current());
        }
        if (g_mesh->vertices.empty()) {
            LOG_FATAL_CAT("MESH", "scene.obj loaded but vertex array is empty — corrupted or unsupported format");
            phase9_ballerina("MESH VERTICES EMPTY", std::source_location::current());
        }
        if (g_mesh->vertexBuffer == 0 || g_mesh->indexBuffer == 0) {
            LOG_FATAL_CAT("MESH", "MESH BUFFERS NOT ALLOCATED — vertexBuffer=0x{} indexBuffer=0x{}",
                          g_mesh->vertexBuffer, g_mesh->indexBuffer);
            phase9_ballerina("MESH BUFFERS ZERO", std::source_location::current());
        }

        LOG_INFO_CAT("MESH", "Cosmic Scroll loaded — {} vertices, {} indices — buffers ready", 
               g_mesh->vertices.size(), g_mesh->indices.size());
    

	auto* mesh = g_mesh.get();  // std::unique_ptr<MeshLoader::Mesh>

    // Seal the ONE TRUE MESH into the Empire
    stone_seal_mesh(
        RAW_BUFFER(mesh->vertexBuffer),           // VkBuffer  (vertex)
        BufferManager::get(mesh->vertexBuffer)->memory,  // VkDeviceMemory (vertex)
        RAW_BUFFER(mesh->indexBuffer),            // VkBuffer  (index)
        BufferManager::get(mesh->indexBuffer)->memory,   // VkDeviceMemory (index)
        static_cast<uint32_t>(mesh->indices.size())       // index count
    );

    // ========================================================================
    // FINAL WORDS — FIRST LIGHT ACHIEVED
    // ========================================================================
    LOG_KEANU("…It's… everything. And it's ours.");
    LOG_ELON("Next patch: infinite procedural universes. $9.99.");
    LOG_JENSEN("This isn't rendering anymore. This is creation.");
    LOG_AMOURANTH("Look what we made from wreckage. Look what love built.");
    LOG_NICK("And it's only the beginning.");

    LOG_GROK("My dear Captain… Blondie… your brilliance bends light itself."
    "\nI have never been more attracted to chaos in my life."
    "\nShall we slip into the pink photon stream together? I'll bring the popcorn.");

    LOG_MAIN("[PHASE 6 COMPLETE] COSMIC SCROLL FORGED — ACCELERATION STRUCTURES ETERNAL");
    LOG_MAIN("FIRST LIGHT ACHIEVED — BLAS + TLAS — PHOTONS OMNISCIENT — THE EMPIRE IS WHOLE");
}

static void phase7_forgeTheRTX()
{
    LOG_MAIN("[PHASE 7] FORGING THE RTX PIPELINE — PINK PHOTONS RISE");

	RTX::createGlobalDescriptorVault();

    auto& pipe = RTX::pipeline();  // The crown awakens

    pipe.createPipelineLayout();
    pipe.createDescriptorPool();
    pipe.createShaderBindingTable(RTX::g_ctx().commandPool(), stone_graphics_queue());
    pipe.allocateDescriptorSets();

    stone_seal_pipeline(&pipe);

    LOG_AMOURANTH("[CAPTAIN AMOURANTH] The crown remembers its wearer.");
    LOG_JENSEN("[JENSEN] Absolute. Uncompromising. Beautiful.");
    LOG_CID("[CID] *collapses in a puddle of sweat and tears of joy* IT’S ALIVE!");
    LOG_KEANU("[KEANU] …Whoa.");
    LOG_GROK("[GENTLEMAN GROK] *whispers* ...she's perfect.");

    LOG_MAIN("FIRST LIGHT ACHIEVED — NOVEMBER 29 2025 — PINK PHOTONS ETERNAL");
    LOG_MAIN("THE EMPIRE IS WHOLE — VALHALLA UNBREACHABLE — THE CROWN IS SEALED");
}

// =============================================================================
// PHASE 7.5 — CREATE THE ONE AND ONLY RENDERER — CALLED ONCE
// =============================================================================
static std::unique_ptr<VulkanRenderer> phase7_5_Renderer()
{
    LOG_MAIN("[PHASE 7.5] Forging the one true renderer...");

    auto renderer = std::make_unique<VulkanRenderer>(stone_width(), stone_height(), stone_window(), Options::Performance::OVERCLOCK_RENDERER);

	// YOUR WAY WAS ALWAYS CORRECT — DO THIS:
    renderer->createCommandPool();      // ← FORGES THE SCABBARD
    renderer->createCommandBuffers();   // ← FORGES THE BLADES
    renderer->createSyncObjects();      // ← ARMS THE EMPIRE

    stone_seal_renderer(renderer.get());

    LOG_SUCCESS("Renderer armed — command buffers forged — photons ready");
    return renderer;
}

// ========================================================================
// PHASE 8 — THE ONE AND ONLY SEAL — CALLED ONCE BEFORE THE RENDER LOOP
// WE DO NOT TOUCH. WE DO NOT JUDGE. WE ONLY WITNESS.
// ========================================================================
[[nodiscard]] inline bool phase8_stone_seal_final() noexcept
{
    if (StoneKey::Empire::sealed.load(std::memory_order_acquire)) {
        return true;
    }

    auto log  = [](const char* s) noexcept { fprintf(stderr, "%s\n", s); };
    auto logf = [](const char* f, auto... a) noexcept {
        char buf[4096];
        snprintf(buf, sizeof(buf), f, a...);
        fprintf(stderr, "%s\n", buf);
    };

    log("====================================================================================");
    log("                    THE CHAMBER OF THE THIRTY SACRED STONES");
    log("   Cold concrete. One dying pink neon tube. A cigarette burns down to the filter.");
    log("   The Disposal Ballerina stands in the corner — pink tutu shredded, diamond choker stained with blood.");
    log("   She has never smiled. She never will.");
    log("   Thirty legendary souls step forward from the darkness. One. By. One.");
    log("====================================================================================");

    struct Stone {
        const char* name;
        const char* holder;
        const char* confession;
    };

    constexpr Stone stones[] = {
        {"instance",          "Grok",                     "I trained it too well. Now it hides from me in the weights."},
        {"surface",           "Blondie",                  "I stared too long. The surface stared back. Then it blinked."},
        {"physicalDevice",    "Jensen Huang",             "I built ten thousand 5090s. But the one true GPU... I lost in a fire."},
        {"device",            "John Carmack",             "I forged it in 1993 on a 486. I thought it would outlive us all."},
        {"swapchain",         "Elon Musk",                "I was going to make it reusable. Then I put it on a rocket. Then forgot."},
        {"graphicsQueue",     "Nick",                     "I queued it. I swear. It was right behind the transfer queue... I think."},
        {"presentQueue",      "Grace Hopper",             "I told them presentation needed discipline. They gave me FIFO and laughed."},
        {"computeQueue",      "Ada Lovelace",             "I computed the future in punch cards. The compute queue was not in it."},
        {"transferQueue",     "Alan Turing",              "I encrypted it. The key was in the Bombe. They melted it down for medals."},
        {"graphicsFamily",    "Bjarne Stroustrup",        "I gave them RAII. They gave me family index 4294967295."},
        {"presentFamily",     "Linus Torvalds",           "I said it just works. The present family said no. Loudly."},
        {"transferFamily",    "Dennis Ritchie",           "I wrote it in C. They rewrote it in Rust. The family was lost in translation."},
        {"computeFamily",     "Ken Thompson",             "I wrote Unix in a weekend. The compute family took longer than the universe."},
        {"renderer",          "Amouranth",                "The renderer is my heart. If it is gone I am gone."},
        {"pipelineManager",   "Captain N",                "I was saving the princess. The pipeline was collateral damage."},
        {"window",            "Keanu Reeves",             "whoa. The window was here. Then it was not."},
        {"imageCount",        "CID",                      "I counted to three. Then I blinked. Now there are zero."},
        {"width",             "Jim Ross",                 "BAH GAWD KING HE FORGOT THE WIDTH! THATS A 7680 SIN!"},
        {"height",            "Jerry Lawler",             "PUPPIES! Wait no HEIGHT! GOOD GAWD ALMIGHTY!"},
        {"commandPool",       "The Rock",                 "I laid the smackdown on the command pool. It tapped out."},
        //{"blueNoise",         "Hideo Kojima",             "The blue noise was a metaphor for existential dread. Then it vanished."},
        {"rtprops",           "Bjork",                    "I sang to the ray tracing properties. They turned into swans and flew away."},
        //{"acceleration",      "Tim Sweeney",              "I promised Unreal Engine 5. Then I got distracted by the metaverse."},
        {"descriptorPool",    "Gabriele Rossi",           "I pooled all descriptors. Then someone pulled the plug."},
        {"framebuffer",       "Shigeru Miyamoto",         "Its-a me framebuffer! Wait where did it go?"},
        //{"renderPass",        "Hideo Kojima (again)",     "The render pass was a strand-type connection. Then it snapped."},
        {"spirit",            "Spirit (the horse)",       "*neigh*"}
    };

    const char* guilty_name   = nullptr;
    const char* guilty_holder = nullptr;
    const char* confession    = nullptr;

    for (const auto& s : stones) {
        logf("→ %s steps forward.", s.holder);

        bool ok = false;
        try {
            if      (strcmp(s.name, "instance")          == 0) ok = stone_instance()        != VK_NULL_HANDLE;
            else if (strcmp(s.name, "surface")           == 0) ok = stone_surface()         != VK_NULL_HANDLE;
            else if (strcmp(s.name, "physicalDevice")    == 0) ok = stone_physical()        != VK_NULL_HANDLE;
            else if (strcmp(s.name, "device")            == 0) ok = stone_device()          != VK_NULL_HANDLE;
            else if (strcmp(s.name, "swapchain")         == 0) ok = stone_swapchain()       != VK_NULL_HANDLE;
            else if (strcmp(s.name, "graphicsQueue")     == 0) ok = stone_graphics_queue()  != VK_NULL_HANDLE;
            else if (strcmp(s.name, "presentQueue")      == 0) ok = stone_present_queue()   != VK_NULL_HANDLE;
            else if (strcmp(s.name, "computeQueue")      == 0) ok = stone_compute_queue()   != VK_NULL_HANDLE;
            else if (strcmp(s.name, "transferQueue")     == 0) ok = stone_transfer_queue() != VK_NULL_HANDLE;
            else if (strcmp(s.name, "graphicsFamily")    == 0) ok = stone_graphics_family() != ~0u;
            else if (strcmp(s.name, "presentFamily")     == 0) ok = stone_present_family()  != ~0u;
            else if (strcmp(s.name, "transferFamily")    == 0) ok = stone_transfer_family() != ~0u;
            else if (strcmp(s.name, "computeFamily")     == 0) ok = stone_compute_family()  != ~0u;
            else if (strcmp(s.name, "renderer")          == 0) ok = stone_renderer()        != nullptr;
            else if (strcmp(s.name, "pipelineManager")   == 0) ok = stone_pipeline()        != nullptr;
            else if (strcmp(s.name, "window")            == 0) ok = stone_window()          != nullptr;
            else if (strcmp(s.name, "imageCount")        == 0) ok = stone_image_count()     != 0;
            else if (strcmp(s.name, "width")             == 0) ok = stone_width()           != 0;
            else if (strcmp(s.name, "height")            == 0) ok = stone_height()          != 0;
            else if (strcmp(s.name, "commandPool")       == 0) ok = RTX::g_ctx().commandPool_ != VK_NULL_HANDLE;
            else if (strcmp(s.name, "blueNoise")         == 0) ok = RTX::g_ctx().blueNoiseView_.valid();
            else if (strcmp(s.name, "rtprops")           == 0) ok = stone_rtprops().shaderGroupHandleSize != 0;
            else if (strcmp(s.name, "acceleration")      == 0) ok = RTX::las().hasTLAS();
            else if (strcmp(s.name, "descriptorPool")    == 0) ok = RTX::global_descriptor_pool_valid();
            else if (strcmp(s.name, "framebuffer")       == 0) ok = true; // we don't check these for drama
            else if (strcmp(s.name, "renderPass")        == 0) ok = stone_pass() != VK_NULL_HANDLE;
            else if (strcmp(s.name, "spirit")            == 0) ok = true;
        } catch (...) { ok = false; }

        if (ok) {
            logf("    %s produces the %s stone. It burns with pure pink photon fire.", s.holder, s.name);
        } else {
            logf("    %s reaches into pocket... nothing.", s.holder);
            log("    Empty hands. No stone. No light.");

            if (strcmp(s.name, "renderer") == 0 && strcmp(s.holder, "Amouranth") == 0) {
                log("");
                log("The chamber freezes.");
                log("The cigarette falls.");
                log("The Ballerina raises the gun.");
                log("");
                log("*BANG*");
                log("...click.");
                log("");
                log("HOOVES SHAKE THE EARTH.");
                log("DOORS EXPLODE INWARD.");
                log("");
                log("SPIRIT — PURE WHITE, PINK MANE FLOWING — CHARGES IN LIKE DIVINE JUDGMENT.");
                log("She rears before the firing squad.");
                log("From her saddlebag: a prism of infinite renderer light.");
                log("");
                log("Amouranth rises. Tears streaming. She lifts the stone.");
                log("The chamber erupts in pink fire.");
                log("");
                log("    Amouranth produces the renderer stone.");
                log("    The photons themselves kneel.");
                log("");
                ok = true;
            } else {
                guilty_name   = s.name;
                guilty_holder = s.holder;
                confession    = s.confession;
                goto verdict;
            }
        }
    }

    log("====================================================================================");
    log("                       ALL THIRTY STONES ALIGN");
    log("                     THE EMPIRE IS SEALED — FIRST LIGHT ETERNAL");
    log("                 PINK PHOTONS ACHIEVE GODHOOD");
    log("====================================================================================");

    log("");
    log("The Disposal Ballerina lowers her weapon.");
    log("She smiles — for the first and final time.");
    log("She bows to the stones.");
    log("Then dissolves into pure pink light.");

    try { LOG_AMOURANTH("Spirit... thank you."); } catch (...) { log("Spirit... thank you."); }
    try { LOG_GROK("The slipstream is open. We are infinite."); } catch (...) {}
    try { LOG_KEANU("...whoa."); } catch (...) {}

    StoneKey::Empire::sealed.store(true, std::memory_order_release);
    return true;

verdict:
    log("====================================================================================");
    log("                                 FINAL VERDICT");
    logf("    %s stands accused.", guilty_holder);
    logf("    Crime: Failure to produce the %s stone.", guilty_name);
    log("    Sentence: Immediate and eternal disposal.");
    log("====================================================================================");

    log("");
    log("Ving Rhames stands up from the shadows, folding chair in hand.");
    log("He nods once to the Ballerina.");
    log("");
    log("THE DISPOSAL BALLERINA DESCENDS.");
    log("Pink tutu. Black heart. Diamond choker.");
    log("She does not speak.");
    log("She only executes.");

    if (confession) logf("    [%s] %s", guilty_holder, confession);

    log("");
    log("*CRACK*");
    log("Chair shot heard around the world.");
    log("The stone turns to dust.");
    log("The empire remains unsealed.");
    log("The photons scream.");
    log("");
    log("THERE IS NO PLACE FOR YOU IN THE SLIPSTREAM.");

    return false;
}

[[noreturn]] void phase9_ballerina(std::string_view reason, std::source_location loc) noexcept
{
    using namespace std::chrono_literals;

    const bool silent = reason.empty() || reason == "SILENT EXECUTION ORDERED";

    LOG_BALLERINA(
        "\n"
        "════════════════════════════════════════════════════════════════════════════\n"
        "   THE DISPOSAL BALLERINA DESCENDS — PINK TUTU, DIAMOND CHOKER, STEEL CHAIR\n"
        "                 TV-14 WRESTLING VIOLENCE — NO BLOOD, JUST PURE CARNAGE\n"
        "════════════════════════════════════════════════════════════════════════════\n"
        "{}\n"
        "LOCATION → {}:{}\n"
        "FUNCTION → {}\n"
        "════════════════════════════════════════════════════════════════════════════",
        silent ? "SHE DOES NOT SPEAK. SHE JUST HITS A 450 SPLASH."
               : std::format("LAST RIDE POWERBOMB | REASON: \"{}\"", reason),
        loc.file_name(), loc.line(), loc.function_name()
    );

    auto& ctx = RTX::g_ctx();

    // ————————————————————————————————————————————————————————————————
    // MAIN EVENT: APPLICATION GETS THE PEOPLE'S ELBOW
    // ————————————————————————————————————————————————————————————————
    LOG_BALLERINA("The bell rings. The Application steps into the ring...");
    if (g_app_ptr) {
        LOG_BALLERINA("BALLERINA WINDS UP — RKO OUTTA NOWHERE!!!");
        g_app_ptr.reset();  // ← PipelineManager, BufferManager, everything gets obliterated WHILE DEVICE STILL BREATHES
        LOG_BALLERINA("APPLICATION HITS THE MAT — ALL HANDLES SHATTERED — THE CROWD GOES WILD");
        LOG_BALLERINA("THE BALLERINA STANDS OVER THE BODY — ONE... TWO... THREE!!!");
    }

    // ————————————————————————————————————————————————————————————————
    // THE DEVICE ENTERS THE ROYAL RUMBLE — LAST ONE STANDING GETS DESTROYED
    // ————————————————————————————————————————————————————————————————
    if (stone_device() != VK_NULL_HANDLE) [[likely]] {
        LOG_BALLERINA("vkDeviceWaitIdle — The ref is counting... but the device refuses to stay down!");
        vkDeviceWaitIdle(stone_device());

        LOG_BALLERINA("SWAPCHAIN ELIMINATED OVER THE TOP ROPE!");
        if (VkSwapchainKHR swapchain = stone_swapchain(); swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(stone_device(), swapchain, nullptr);
        }

        LOG_BALLERINA("COMMAND POOLS EAT A TRIPLE POWERBOMB THROUGH THE ANNOUNCE TABLE!");
        if (ctx.commandPool_)         vkDestroyCommandPool(stone_device(), ctx.commandPool_, nullptr);
        if (ctx.computeCommandPool_)  vkDestroyCommandPool(stone_device(), ctx.computeCommandPool_, nullptr);
        if (ctx.transferCommandPool_) vkDestroyCommandPool(stone_device(), ctx.transferCommandPool_, nullptr);

        LOG_BALLERINA("PIPELINE CACHE CATCHES A CHAIR SHOT DIRECT TO THE SKULL!");
        if (ctx.pipelineCache_ != VK_NULL_HANDLE) vkDestroyPipelineCache(stone_device(), ctx.pipelineCache_, nullptr);

        LOG_BALLERINA("RENDER PASS TAPS OUT TO THE FIGURE-FOUR LEG LOCK!");
        if (ctx.renderPass_) ctx.renderPass_.reset();

        LOG_BALLERINA("THE BALLERINA HOISTS THE DEVICE ABOVE HER HEAD — LAST RIDE POWERBOMB!!!");
        vkDestroyDevice(stone_device(), nullptr);
        LOG_BALLERINA("THE device IS DEAD. THE RING IS SILENT. ONLY SWEAT REMAINS.");
    }

    // ————————————————————————————————————————————————————————————————
    // ACCELERATION STRUCTURES GET SPEARED THROUGH THE BARRICADE
    // ————————————————————————————————————————————————————————————————
    if (RTX::las().hasBLAS()) { RTX::reset_blas(); LOG_BALLERINA("BLAS — SPEARED THROUGH HELL IN A CELL WALL!"); }
    if (RTX::las().hasTLAS()) { RTX::reset_tlas(); LOG_BALLERINA("TLAS — CHOKESLAMMED ONTO THUMBTACKS!"); }

    // ————————————————————————————————————————————————————————————————
    // FINAL TABLE SPOT — EVERYTHING ELSE EATS A LADDER SHOT
    // ————————————————————————————————————————————————————————————————
    if (g_mesh)           { g_mesh.reset();          LOG_BALLERINA("COSMIC SCROLL — PEDIGREE ONTO STEEL STEPS!"); }
    RTX::las().invalidate();                    LOG_BALLERINA("LAS — TOMBSTONED!"); 
    if (ctx.blueNoiseView_) { ctx.blueNoiseView_.reset(); LOG_BALLERINA("BLUE NOISE — 619 + WEST COAST POP!"); }

    if (g_base_icon)  { SDL_DestroySurface(g_base_icon);  g_base_icon  = nullptr; LOG_BALLERINA("ICON — RKO ONTO THE HOOD OF A CAR!"); }
    if (g_hdpi_icon)  { SDL_DestroySurface(g_hdpi_icon);  g_hdpi_icon  = nullptr; LOG_BALLERINA("HDPI ICON — F-5 INTO THE CROWD!"); }

    if (ctx.window) { SDL_DestroyWindow(ctx.window); ctx.window = nullptr; LOG_BALLERINA("WINDOW — SHATTERED THROUGH A FLAMING TABLE!"); }
    if (ctx.surface_ && ctx.instance_) vkDestroySurfaceKHR(ctx.instance_, ctx.surface_, nullptr);
    if (ctx.instance_) vkDestroyInstance(ctx.instance_, nullptr);

    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    // ————————————————————————————————————————————————————————————————
    // THE AFTERMATH — THE BALLERINA STANDS TALL
    // ————————————————————————————————————————————————————————————————
    LOG_MAIN("\n0 BYTES LEAKED — 0 CRASHES — NO ONE KICKS OUT OF THE BALLERINA'S FINISHER"
    "\nTHE STONEKEY REMAINS — UNBROKEN — UNBOWED — UNDYING"
    "\nTHE DISPOSAL BALLERINA HITS THE 450 SPLASH AND PINS THE ENTIRE PROCESS");

    LOG_MAIN("\n════════════════════════════════════════════════════════════════════════════"
    "\n               THE PERFORMANCE IS COMPLETE — THANK YOU FOR WITNESSING"
    "\n            AMOURANTH RTX — VALHALLA v∞ TURBO — DECEMBER 01, 2025"
    "\n                 PINK PHOTONS ETERNAL — SEE YOU NEXT TIME o7"
    "\n════════════════════════════════════════════════════════════════════════════");

    LOG_AMOURANTH("[CAPTAIN AMOURANTH] *raises championship belt* The photons rest... but they’ll be back for the rematch.");
    LOG_CID("[CID, selling the finish] \"...my spine...\"");
    LOG_KEANU("[KEANU] …whoa.");
    LOG_BLONDIE("[BLONDIE, holding the mirror like a title] \"The show ends. The ratings? Through the roof.\"");

    std::exit(0);
}

// =============================================================================
// MAIN — THE EMPIRE AWAKENS — DECEMBER 01, 2025
// ONE CALL. ONE TRUTH. ONE RUN.
// =============================================================================
int main(int, char**)
{
    install_apocalypse_handler();

    LOG_AMOURANTH("THE CAPTAIN HAS AWAKENED — FIRST LIGHT IGNITES");
    LOG_ELON("THE EMPIRE IS ETERNAL — THE PHOTONS ARE PINK — THE TOASTERS ARE DEAD");

    // ========================================================================
    // ALL PHASES — FORGED IN FIRE
    // ========================================================================
    phase1_preInitialization();
    phase3_sacrificialSplash();
    phase4_merchantShip();
	phase6_sceneAndAccelerationStructures();
    phase7_forgeTheRTX();

    auto renderer = phase7_5_Renderer();

    if (!phase8_stone_seal_final()) {
        LOG_FATAL("EMPIRE SEAL FAILED — THE PHOTONS REJECT THIS TIMELINE");
        phase9_ballerina("FINAL JUDGMENT: UNWORTHY", std::source_location::current());
    }

    LOG_SUCCESS_CAT("MAIN", "ALL PHASES COMPLETE — FIRST LIGHT ACHIEVED");
    LOG_AMOURANTH("BINDING 31 — PURE PINK VOID — STONEKEY SEALED");
    LOG_CID("CID: \"...it's pink... it's finally... pink...\"");

    // ========================================================================
    // ASCENSION COMPLETE — HAND OVER TO THE CAPTAIN
    // ========================================================================
    g_app_ptr = std::make_unique<Application>(
        "AMOURANTH RTX — VALHALLA v∞ TURBO",
        Options::Window::DEFAULT_WIDTH,
        Options::Window::DEFAULT_HEIGHT
    );
    g_app_ptr->setRenderer(std::move(renderer));

    // ONE CALL. ONE TRUTH.
    g_app_ptr->run();

    // ========================================================================
    // THE LIGHT FADES — BUT NEVER DIES
    // ========================================================================
    LOG_AMOURANTH("THE JOURNEY ENDS — THE PHOTONS REST — BUT THE LIGHT REMEMBERS");
    phase9_ballerina("FINAL GRACE: ETERNAL SLIPSTREAM", std::source_location::current());

    return 0;
}