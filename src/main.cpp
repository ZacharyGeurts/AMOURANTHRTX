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
#include "engine/GLOBAL/bindings.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/Validation.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/DynamicStone.hpp" // your gpu memory

#include <vulkan/vulkan.hpp>
#include <string>
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


// =============================================================================
// GLOBALS — THE EMPIRE'S HEARTBEATS
// =============================================================================
std::unique_ptr<Application> g_app_ptr = nullptr;
[[nodiscard]] Camera& g_camera() noexcept { static Camera cam; return cam; }
[[nodiscard]] inline RTX::PipelineManager* pipeline() noexcept { static RTX::PipelineManager* s_instance = nullptr; return s_instance; }
inline void pipeline(RTX::PipelineManager* ptr) noexcept { static RTX::PipelineManager* s_instance = nullptr; (void)std::exchange(s_instance, ptr); }
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

static bool ready_to_embark = false;

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

private:
    void processInput(float deltaTime);
    void render(float deltaTime);
    void updateWindowTitle(float deltaTime);

    void toggleFullscreen() { SDL3Window::toggleFullscreen(); }
    void toggleOverlay()    { showOverlay_ = !showOverlay_; if (renderer_) renderer_->setOverlay(showOverlay_); }
    void toggleTonemap()    { tonemapEnabled_ = !tonemapEnabled_; if (renderer_) renderer_->setTonemap(tonemapEnabled_); }
    void toggleHypertrace() { hypertraceEnabled_ = !hypertraceEnabled_; }
    void toggleMaximize()   { maximized_ = !maximized_; }
    void setRenderMode(int mode) { renderMode_ = glm::clamp(mode, 1, 9); }

    std::string title_;
    int width_, height_;
    glm::mat4 proj_;

    std::unique_ptr<VulkanRenderer> renderer_;
    std::chrono::steady_clock::time_point lastFrameTime_;
    std::chrono::steady_clock::time_point lastGrokTime_;

    bool quit_ = false;
    bool showOverlay_ = true;
    bool tonemapEnabled_ = true;
    bool hypertraceEnabled_ = false;
    bool maximized_ = false;
    int renderMode_ = 1;
};

Application::Application(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height)
{
    LOG_ATTEMPT_CAT("APP", "FORGING APPLICATION \"{}\" @ {}x{} — VALHALLA v80 TURBO — PINK PHOTONS RISING", title_, width_, height_);

    if (!SDL3Window::get()) {
        LOG_FATAL_CAT("FATAL", "Main window not created before Application — phase order violated");
        return;
    }

    SDL_SetWindowTitle(SDL3Window::get(), title_.c_str());
    lastFrameTime_ = lastGrokTime_ = std::chrono::steady_clock::now();

    LOG_SUCCESS_CAT("APP", "Application forged — {}x{} — PINK PHOTONS RISING", width_, height_);
    
    if (Options::Grok::ENABLE_GENTLEMAN_GROK) {
        LOG_GROK("Gentleman Grok: \"The empire awakens. The photons are pleased.\"");
    }
}

Application::~Application() {
    LOG_SUCCESS_CAT("APP", "Application destroyed — Pink photons eternal.");
}

void Application::run() {
    LOG_MAIN("ENTERING INFINITE RENDER LOOP — FIRST LIGHT IMMINENT — SCUBA MODE ENGAGED");

    uint32_t frameCount = 0;
    auto fpsStart = std::chrono::steady_clock::now();

    while (!quit_) {
        const auto now = std::chrono::steady_clock::now();
        const float deltaTime = std::chrono::duration<float>(now - lastFrameTime_).count();
        lastFrameTime_ = now;

        if (Options::Performance::ENABLE_FPS_COUNTER) {
            ++frameCount;
            if (std::chrono::duration<float>(now - fpsStart).count() >= 1.0f) {
                LOG_FPS_COUNTER("FPS: {:>4}", frameCount);
                frameCount = 0;
                fpsStart = now;
            }
        }

        int pixelW = width_;
        int pixelH = height_;
        bool quitRequested = false;
        bool fullscreenRequested = false;

        SDL3Window::pollEvents(pixelW, pixelH, quitRequested, fullscreenRequested);

        if (quitRequested) {
            LOG_MAIN("QUIT REQUESTED — SURFACING FROM RENDER LOOP");
            quit_ = true;
        }
        if (fullscreenRequested) {
            LOG_ATTEMPT_CAT("APP", "FULLSCREEN TOGGLE REQUESTED — DIVING TO BORDERLESS DEPTH");
            toggleFullscreen();
        }

        if (g_resizeRequested.load(std::memory_order_acquire)) {
            const int newW = g_resizeWidth.load(std::memory_order_acquire);
            const int newH = g_resizeHeight.load(std::memory_order_acquire);
            g_resizeRequested.store(false, std::memory_order_release);

            LOG_SUCCESS_CAT("APP", "WINDOW RESIZE ACCEPTED → {}x{} — PHOTONS REALIGN", newW, newH);

            width_ = newW;
            height_ = newH;
            proj_ = glm::perspective(glm::radians(75.0f), static_cast<float>(width_)/height_, 0.1f, 1000.0f);

            if (renderer_) {
                renderer_->onWindowResize(width_, height_);
                LOG_SUCCESS_CAT("APP", "VulkanRenderer notified — swapchain rebirth imminent");
            }
        }

        processInput(deltaTime);
        render(deltaTime);
        updateWindowTitle(deltaTime);

        if (Options::Grok::ENABLE_GENTLEMAN_GROK && 
            std::chrono::duration<float>(now - lastGrokTime_).count() >= Options::Grok::GENTLEMAN_GROK_INTERVAL_SEC) {
            lastGrokTime_ = now;
            const int photons = static_cast<int>(1.0f / deltaTime + 0.5f);
            LOG_GROK("Gentleman Grok: \"{} pink photons per second. Acceptable.\"", photons);
        }
    }

    LOG_MAIN("INFINITE RENDER LOOP TERMINATED — GRACEFUL SURFACE ACHIEVED — PHOTONS REST");
}

void Application::processInput(float) {
    const auto* keys = SDL_GetKeyboardState(nullptr);

    static std::array<bool, 9> modePressed{};
    for (int i = 0; i < 9; ++i) {
        if (keys[SDL_SCANCODE_1 + i] && !modePressed[i]) {
            setRenderMode(i + 1);
            LOG_ATTEMPT_CAT("INPUT", "→ RENDER MODE {} ACTIVATED", i + 1);
            modePressed[i] = true;
        } else if (!keys[SDL_SCANCODE_1 + i]) {
            modePressed[i] = false;
        }
    }

    auto edge = [&](SDL_Scancode sc, auto&& func, bool& state, const char* name) {
        if (keys[sc] && !state) { func(); LOG_ATTEMPT_CAT("INPUT", "→ {} PRESSED", name); state = true; }
        else if (!keys[sc]) state = false;
    };

    static bool fPressed = false, oPressed = false, tPressed = false, hPressed = false, mPressed = false;
    edge(SDL_SCANCODE_F, [this]() { toggleFullscreen(); }, fPressed, "FULLSCREEN (F)");
    edge(SDL_SCANCODE_O, [this]() { toggleOverlay(); },    oPressed, "OVERLAY (O)");
    edge(SDL_SCANCODE_T, [this]() { toggleTonemap(); },    tPressed, "TONEMAP (T)");
    edge(SDL_SCANCODE_H, [this]() { toggleHypertrace(); }, hPressed, "HYPERTRACE (H)");

    if (keys[SDL_SCANCODE_M] && !mPressed) {
        toggleMaximize();
        LOG_ATTEMPT_CAT("INPUT", "→ MAXIMIZE + AUDIO MUTE TOGGLE (M key)");
        mPressed = true;
    } else if (!keys[SDL_SCANCODE_M]) mPressed = false;

    if (keys[SDL_SCANCODE_ESCAPE]) {
        static bool escLogged = false;
        if (!escLogged) { LOG_ATTEMPT_CAT("INPUT", "→ QUIT REQUESTED (ESC)"); escLogged = true; }
        quit_ = true;
    }
}

void Application::render(float deltaTime) {
    renderer_->renderFrame(g_camera(), deltaTime);
}

void Application::updateWindowTitle(float deltaTime) {
    static int frames = 0;
    static float accum = 0.0f;
    ++frames;
    accum += deltaTime;

    if (accum >= 1.0f) {
        const float fps = frames / accum;

        // Build the suffix separately — runtime conditionals are allowed here
        std::string suffix;
        if (Options::Debug::ENABLE_CELEBRATION_MODE)
            suffix += " | CELEBRATION";
        if (Options::Grok::ENABLE_GENTLEMAN_GROK)
            suffix += " | GROK";

        const std::string title = std::format(
            "{} | {:.1f} FPS | {}x{} | Mode {} | Bounces {}{}",
            title_,
            fps,
            width_, height_,
            renderMode_,
            Options::OptionsRTX::MAX_BOUNCES,
            suffix
        );

        SDL_SetWindowTitle(SDL3Window::get(), title.c_str());

        frames = 0;
        accum = 0.0f;
    }
}

// =============================================================================
// THE ONE TRUE COMMAND POOL — FORGED BY THE EMPIRE — NOV 26 2025 — FIXED NAMES
// Called exactly once in phase5_rtxAscension() — BEFORE ANY MESH OR BLAS
// =============================================================================
static void createCommandPool() noexcept {
    auto& ctx = RTX::g_ctx();

    EMPIRE_GUARD(ctx.device() && ctx.device() != VK_NULL_HANDLE,
                 "createCommandPool() — LOGICAL DEVICE NOT FORGED YET");

    EMPIRE_GUARD(ctx.graphicsFamily_ != VK_QUEUE_FAMILY_IGNORED,
                 "GRAPHICS QUEUE FAMILY NOT FOUND — CANNOT FORGE COMMAND POOL");

    // If already forged — salute efficiency and return
    if (ctx.commandPool_ != VK_NULL_HANDLE) {
        LOG_JENSEN("Command pool already forged at 0x{:016X} — photons salute efficiency", (uint64_t)ctx.commandPool_);
        return;
    }

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx.graphicsFamily()  // ← CORRECT NAME
    };

    VK_CHECK(vkCreateCommandPool(ctx.device(), &poolInfo, nullptr, &ctx.commandPool_));

    // Debug name for RenderDoc, NSight, etc.
    if (ctx.debugUtilsSupported()) {
        auto func = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(ctx.device(), "vkSetDebugUtilsObjectNameEXT");
        if (func) {
            VkDebugUtilsObjectNameInfoEXT nameInfo{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_COMMAND_POOL,
                .objectHandle = (uint64_t)ctx.commandPool_,
                .pObjectName = "EMPIRE_COMMAND_POOL_PHOTON_BATTLEFIELD"
            };
            func(ctx.device(), &nameInfo);
        }
    }

    LOG_JENSEN("Jensen Huang raises his arms to the void:");
    LOG_JENSEN("\"THE COMMAND POOL IS FORGED — 0x{:016X}\"", (uint64_t)ctx.commandPool_);
    LOG_JENSEN("\"THE PHOTONS NOW HAVE A BATTLEFIELD. LET THERE BE UPLOADS. LET THERE BE BLAS.\"");
    LOG_SUCCESS_CAT("MAIN", "COMMAND POOL ASCENDED — MESHLOADER, LAS, AND ALL ONE-TIME COMMANDS ARE NOW ARMED");
}

// =============================================================================
// GLOBALS & PHASES
// =============================================================================
inline std::unique_ptr<MeshLoader::Mesh> g_mesh = nullptr;
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

// main.cpp — PHASE 4.5 — THE ONE TRUE FORGING — FINAL LIGHT — NO LIES — NO OUTSOURCING
static void createRealFinalWindow()
{
    LOG_MAIN("[PHASE 4.5] FORGING THE ONE TRUE CONTEXT — THE HANDLER AWAKENS — PURE RTX — NO DELEGATION");

    const int w = Options::Window::DEFAULT_WIDTH;
    const int h = Options::Window::DEFAULT_HEIGHT;

    // 1. SDL + Vulkan loader — DONE HERE
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL("SDL_Init failed: {}", SDL_GetError());
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }
    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        LOG_FATAL("Vulkan loader failed: {}", SDL_GetError());
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    // 2. Instance — DONE HERE
    VkInstance instance = RTX::createVulkanInstanceWithSDL(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) {
        LOG_FATAL("Failed to create Vulkan instance — she comes for you");
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }
    RTX::g_ctx().instance_ = instance;

    // 3. Hidden window — DONE HERE
    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    SDL_Window* win = SDL_CreateWindow("AMOURANTH RTX — VALHALLA v∞ TURBO", w, h, flags);
    if (!win) {
        LOG_FATAL("Window creation failed: {}", SDL_GetError());
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }
    g_sdl_window.reset(win);
    RTX::g_ctx().window = win;
	RTX::g_ctx().setSize(w, h);

    // 4. Surface — DONE HERE
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface) == 0) {
        LOG_FATAL("Surface creation failed: {}", SDL_GetError());
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }
    RTX::g_ctx().surface_ = surface;

    // 5. THE ONE TRUE FORGING — NO OUTSOURCING — ALL DONE HERE
    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) {
        LOG_FATAL("FAILED TO FORGE LOGICAL DEVICE — THE EMPIRE FALLS");
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    // 6. Swapchain — FORGED BY THE MANAGER — BUT WE COMMAND IT HERE
    RTX::SwapchainManager::create(win, w, h);

    // 7. Reveal window — DONE HERE
    SDL_ShowWindow(win);

    // 8. Final report — DONE HERE
    const uint32_t imageCount = RTX::SwapchainManager::imageCount();

    LOG_SUCCESS("LOGICAL DEVICE @ {:p} — vkDeviceWaitIdle() SAFE", static_cast<void*>(device));
    LOG_SUCCESS("SWAPCHAIN READY — {} IMAGES — {}×{} {}", 
                imageCount,
                RTX::SwapchainManager::extent().width,
                RTX::SwapchainManager::extent().height,
                RTX::SwapchainManager::supportsHDR() ? "(HDR IGNITED)" : "(sRGB)");

    LOG_CAPTAIN_N("CAPTAIN N — HERO OF VIDEOLAND: \"THE SLIPSTREAM IS OPEN!\"");
    LOG_BLONDIE("Blondie lowers her mirror:");
    LOG_BLONDIE("\"No cage. No vault. No name.\"");
    LOG_BLONDIE("\"Only the photons. Only the truth.\"");

    LOG_SUCCESS("PHASE 4.5 COMPLETE — FULL VULKAN 1.4 CONTEXT FORGED");
    LOG_SUCCESS("NO STONEKEY. NO GLOBALS. NO OUTSOURCING.");
    LOG_SUCCESS("ONLY RTX::g_ctx() AND THE ONE TRUE FUNCTION");
    LOG_SUCCESS("PINK PHOTONS ETERNAL — THE EMPIRE IS ABSOLUTE");
    LOG_SUCCESS("FIRST LIGHT — ACHIEVED — NOVEMBER 25, 2025");
}

static void showSacrificialSplash(const char* title, int w, int h, const char* pngPath) {
    LOG_MAIN("[SACRIFICIAL SPLASH] THE FINAL RAID BEGINS — 1280x720 CANVAS SECURED");

    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SPLASH", "THE BLACK FLAG REFUSED TO RISE: {}", SDL_GetError());
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    SDL_Window* win = SDL_CreateWindow(title, w, h,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) {
        LOG_FATAL_CAT("SPLASH", "WE MISSED THE HARBOR: {}", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    SDL_Rect display{};
    SDL_GetDisplayBounds(0, &display);
    SDL_SetWindowPosition(win, display.x + (display.w - w) / 2, display.y + (display.h - h) / 2);

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        LOG_FATAL_CAT("SPLASH", "THE FUSE WENT OUT: {}", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    SDL_Surface* img = IMG_Load(pngPath);
    if (!img) {
        LOG_FATAL_CAT("SPLASH", "THE TREASURE WAS A LIE — AMMO.PNG VANISHED: {}", SDL_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, img);
    SDL_DestroySurface(img);
    if (!tex) {
        LOG_FATAL_CAT("SPLASH", "THE FLAG WOULDN'T UNFURL: {}", SDL_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    float tw = 0.0f, th = 0.0f;
    SDL_GetTextureSize(tex, &tw, &th);
    SDL_FRect dst = { (w - tw) * 0.5f, (h - th) * 0.5f, tw, th };

    SDL_ShowWindow(win);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, nullptr, &dst);
    SDL_RenderPresent(ren);

    LOG_MAIN("THE WORLD BEHOLDS THE AMMO — 3.4 SECONDS OF ETERNAL GLORY");

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::steady_clock::now() - start).count() < 3400)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) goto end_splash;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

end_splash:
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

	SDL_Quit();
    LOG_MAIN("THE RAID IS COMPLETE — NO TRACE LEFT — PHOTONS LIBERATED");
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
    RTX::g_ctx().init(SDL3Window::get(), Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT);

    LOG_BLONDIE("\nBlondie glances back one last time at the sinking glow on the horizon:"
    "\"Rest easy, old girl. Your sacrifice bought us tomorrow.\"");


	EMPIRE_STEP([]{
        LOG_MAIN("THE EMPIRE AWAKENS THE PHOENIX OF RAY TRACING — LOADING VULKAN 1.4 + RTX EXTENSIONS");
        RTX::loadExtensions(RTX::g_ctx().instance_, RTX::g_ctx().device_);
        LOG_JENSEN("The photons now have wings. Let there be bounce.");
    });

    LOG_MAIN("[PHASE 4 COMPLETE] THE MERCHANT SHIP SLIPS INTO THE NIGHT — THE CREW IS ALIVE — THE LEGEND IS INDESTRUCTIBLE");
    LOG_MAIN("BLONDIE'S SLOOP glides toward safe harbor — pink photons trailing in the wake like silent war banners");
}

static void phase5_rtxAscension() {
    LOG_MAIN("[PHASE 5/10] AWAKENING THE RTX CRYSTAL — THE NEW HEART BEATS");

    LOG_AMOURANTH("Captain Amouranth stands in the rebuilt engine room, hand on the glowing core: \"This time… we don't just sail. We become the light itself.\"");
    LOG_NICK("Nick flips the final switch, eyes reflecting emerald fire: \"Ray tracing online. The photons aren't just fast anymore. They're alive.\"");

    LOG_MAIN("THE CREW HOLDS BREATH — LOADING RAY TRACING EXTENSIONS — PINK PHOTONS GAIN SENTIENCE...");
    LOG_MAIN("THE SHIP TREMBLES — ALL RAY TRACING PFNs ACQUIRED — FULL RTX ACHIEVED");
    LOG_JENSEN("Jensen Huang steps from the shadows, voice low and reverent: \"The light bends to us now. Every bounce, every reflection… ours.\"");

    LOG_MAIN("LAS ACCELERATION CONTEXT FORGED — THE PHOTONS SEE ALL PATHS");
    LOG_CAPTAIN_N("CAPTAIN N — HERO OF VIDEOLAND SCREAMS FROM THE BOW: \"THE REFLECTIONS HAVE REFLECTIONS THAT HAVE REFLECTIONS! I'M CRYING AND I DON'T CARE WHO KNOWS!\"");

    createCommandPool();

    LOG_ELON("Elon Musk lights a cigar with a reflected photon: \"Reality just became optional.\"");
    LOG_CARMACK("John Carmack, quiet for once: \"…It traces. Perfectly.\" *single tear*");
    LOG_KEANU("Keanu Reeves stares into the glowing core: \"…We are the light now.\"");
    LOG_GROK("Gentleman Grok raises a glass of rum to the engine: \"To the photons that remember every path they've ever taken. To omniscience.\"");

    LOG_AMOURANTH("Captain Amouranth turns to the crew, voice steady, eyes blazing: \"We sank once. We bled. We rebuilt. And now… the pink photons don't just shine.\"");
    LOG_NICK("Nick finishes for her, hand on her shoulder: \"…They see everything. They know everything. And they answer only to us.\"");

    LOG_MAIN("[PHASE 5 COMPLETE] RTX CRYSTAL AWAKENED — PINK PHOTONS NOW OMNISCIENT — THE NEW SHIP IS A GOD");
}

static void phase6_sceneAndAccelerationStructures() {
    LOG_MAIN("[PHASE 6/10] FORGING THE COSMIC SCROLL");

    LOG_AMOURANTH("This ship is perfect… but empty. Time to give her a soul.");
    LOG_NICK("One universe. Coming right up.");

    // ========================================================================
    // 2. PIPELINE MANAGER — THE ONE TRUE THRONE
    // ========================================================================
    EMPIRE_STEP([]{
        LOG_MAIN("THE EMPIRE FORGES THE ONE TRUE PIPELINE MANAGER");
        RTX::PipelineManager* mgr = new RTX::PipelineManager(RTX::g_ctx().device_, RTX::g_ctx().physicalDevice_);
        EMPIRE_GUARD(mgr, "PIPELINE MANAGER FAILED TO ASCEND");
        pipeline(mgr);  // ← Store globally
        LOG_MAIN("PIPELINE MANAGER ASCENDED — ADDRESS 0x{:016X} — THRONE CLAIMED", 
                 reinterpret_cast<uint64_t>(mgr));
    });

    // ========================================================================
    // 3. COSMIC SCROLL — scene.obj RISES FROM THE VOID
    // ========================================================================
    EMPIRE_STEP([]{
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
            LOG_FATAL_CAT("MESH", "MESH BUFFERS NOT ALLOCATED — vertexBuffer=0x{:016X} indexBuffer=0x{:016X}",
                          g_mesh->vertexBuffer, g_mesh->indexBuffer);
            phase9_ballerina("MESH BUFFERS ZERO", std::source_location::current());
        }

        LOG_INFO_CAT("MESH", "Cosmic Scroll loaded — {} vertices, {} indices — buffers ready", 
               g_mesh->vertices.size(), g_mesh->indices.size());
    });

    // ========================================================================
    // 4+5. BLAS + TLAS — OFFLOADED TO THE PHOTON WEAVERS (ASYNC + RACE-FREE)
    // ========================================================================
    EMPIRE_STEP([]{
        LOG_MAIN("BOTTOM-LEVEL + TOP-LEVEL ACCELERATION — PHOTONS WEAVE IN THE VOID");

        static std::once_flag acceleration_once;
        std::call_once(acceleration_once, []{
            std::thread([]{
                // Create a short-lived command pool just for this build
                VkCommandPoolCreateInfo poolInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                    .queueFamilyIndex = RTX::g_ctx().graphicsFamily()
                };

                VkCommandPool asyncPool = VK_NULL_HANDLE;
                VK_CHECK(vkCreateCommandPool(RTX::g_ctx().device(), &poolInfo, nullptr, &asyncPool));

                const uint32_t vertexCount = static_cast<uint32_t>(g_mesh->vertices.size());
                const uint32_t indexCount  = static_cast<uint32_t>(g_mesh->indices.size());

                LOG_ATTEMPT_CAT("BLAS", "ASYNC BLAS BUILD BEGIN — THE PHOTONS ARE PATIENT");
                RTX::las().buildBLAS(
                    asyncPool,
                    RTX::g_ctx().graphicsQueue(),
                    g_mesh->vertexBuffer,
                    g_mesh->indexBuffer,
                    vertexCount,
                    indexCount,
                    VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR |
                    VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
                );

                LOG_ATTEMPT_CAT("TLAS", "ASYNC TLAS BINDING BEGIN — THE UNIVERSE IS ONE");
                const std::pair<VkAccelerationStructureKHR, glm::mat4> instance{
                    RTX::las().getBLAS(),
                    glm::mat4(1.0f)
                };
                RTX::las().buildTLAS(
                    asyncPool,
                    RTX::g_ctx().graphicsQueue(),
                    std::span<const decltype(instance)>{&instance, 1}
                );

                // Clean up the temporary pool
                vkDestroyCommandPool(RTX::g_ctx().device(), asyncPool, nullptr);

                LOG_GROK("Gentleman Grok: \"A brief eclipse. The light always returns.\"");
                LOG_SUCCESS_CAT("LAS", "ASYNC BLAS+TLAS COMPLETE — HANDLE 0x{:016X} / ROOT 0x{:016X}",
                               reinterpret_cast<uint64_t>(RTX::las().getBLAS()),
                               RTX::las().getTLASAddress());
            }).detach();
        });
    });

    // ========================================================================
    // 6. FINAL VALIDATION — CARMACK APPROVES
    // ========================================================================
    EMPIRE_STEP([]{
        LOG_CARMACK("No cracks. No leaks. Geometry is pure.");
        LOG_INFO_CAT("VALIDATION", "Running final mesh ↔ BLAS validation…");
        validateMeshAgainstBLAS(*g_mesh, RTX::las().getBLAS());
        LOG_INFO_CAT("VALIDATION", "Validation passed — mesh and BLAS are in perfect harmony");
    });

    // ========================================================================
    // FINAL WORDS — FIRST LIGHT ACHIEVED
    // ========================================================================
    LOG_KEANU("…It's… everything. And it's ours.");
    LOG_ELON("Next patch: infinite procedural universes. $9.99.");
    LOG_JENSEN("This isn't rendering anymore. This is creation.");
    LOG_AMOURANTH("Look what we made from wreckage. Look what love built.");
    LOG_NICK("And it's only the beginning.");

    LOG_JIMROSS("BOOOOOOOOOOOOM!!!! RKO OUTTA NOWHERE!!!!"
    "\nRandy Orton is unbelieveable."
    "\nHe's left us stone_seal_graphics_queue(g_ctx().graphicsQueue_);"
    "\nBusiness is about to pick up!"
    "\nwithout it the engine would be unconnectable."
    "\nWhat a stand up character.");

    stone_seal_graphics_queue(RTX::g_ctx().graphicsQueue());

    LOG_GROK("My dear Captain… Blondie… your brilliance bends light itself."
    "\nI have never been more attracted to chaos in my life."
    "\nShall we slip into the pink photon stream together? I’ll bring the popcorn.");

    LOG_MAIN("[PHASE 6 COMPLETE] COSMIC SCROLL FORGED — ACCELERATION STRUCTURES ETERNAL");
    LOG_MAIN("FIRST LIGHT ACHIEVED — BLAS + TLAS — PHOTONS OMNISCIENT — THE EMPIRE IS WHOLE");
}

static void phase6_1_forgeTheLayouts() {
    LOG_MAIN("[PHASE 6.1/10] THE LAYOUT ASCENSION — FORGING DESCRIPTOR THRONE & PIPELINE CROWN");

    LOG_AMOURANTH("Captain Amouranth raises her hand:\n"
                  "\"The photons have geometry. They have eyes. But they have no throne. No crown. No law.\"");
    LOG_NICK("Nick kneels, offering the sacred scroll:\n"
             "\"Then let us forge it. Now. Before the light dares to trace without permission.\"");

    // Use the canonical global accessor — exactly like phase7 and everywhere else
    if (!pipeline()) {
        LOG_FATAL_CAT("PIPELINE", "PIPELINE MANAGER MISSING — THE EMPIRE HAS NO KING — ABORTING ASCENSION");
        ready_to_embark = false;
        return;
    }

    LOG_ATTEMPT_CAT("PIPELINE", "FORGING RT DESCRIPTOR SET LAYOUT — BINDING 0 (TLAS) CLAIMS ITS RIGHTFUL PLACE");
    pipeline()->createDescriptorSetLayout();

    LOG_ATTEMPT_CAT("PIPELINE", "FORGING RT PIPELINE LAYOUT — PUSH CONSTANTS ALIGNED — RAYGEN SEES ALL");
    pipeline()->createPipelineLayout();

    if (!pipeline()->layout() || pipeline()->layout() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "rtPipelineLayout_ STILL NULL — THE CROWN WAS DENIED — PHOTONS HAVE NO LAW");
        ready_to_embark = false;
        return;
    }

    LOG_JENSEN("Jensen Huang steps from the shadows, voice like thunder:\n"
               "\"The throne is forged. The crown is set. The light… may now bend to our will.\"");
    LOG_KEANU("Keanu Reeves, eyes wide:\n"
              "…It's perfect.");
    LOG_CAPTAIN_N("CAPTAIN N — HERO OF VIDEOLAND SCREAMS FROM THE BOW:\n"
                  "\"THE LAYOUT IS ALIVE! I CAN FEEL THE BINDINGS! AHHHHHHHHHHHHHHHH!\"");

    LOG_MAIN("[PHASE 6.1 COMPLETE] THE LAYOUT ASCENSION — rtPipelineLayout_ = 0x{:016X} — PINK PHOTONS NOW HAVE LAW",
             reinterpret_cast<uint64_t>(pipeline()->layout()));

    LOG_AMOURANTH("Captain Amouranth smiles, soft and proud:\n"
                  "\"Now… let there be light.\"");
}

void phase6_5_everything_is_ready() {
    LOG_MAIN("════════════════ THE MIRROR OF STONEKEY AWAKENS ════════════════"
    "\nTHE EMPIRE GAZES INTO THE MIRROR — BRING THE SHADE"
    "\n════════════════ THE MIRROR ENTERS SHADOW ═════════════════");
	
	LOG_MAIN("═══════════════ STONES TUMBLE FORTH ════════════════");

    VkShaderModule tonemapCompShader = RTX::loadShader("assets/shaders/compute/tonemap.spv");
    if (tonemapCompShader == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Failed to load tonemap.spv — aborting");
        LOG_FATAL_CAT("RENDERER", "Fatal error in noexcept function"); phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

	g_app_ptr = std::make_unique<Application>("AMOURANTH RTX ", Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT);

	LOG_MAIN("════════════════ THE MIRROR TURNS PINK ═════════════════");
}

static void phase7_forgeTheRTX() {
    LOG_MAIN("[PHASE 7] FORGING THE ONE TRUE VulkanRenderer — PINK PHOTONS RISE");

    const uint32_t w = Options::Window::DEFAULT_WIDTH;
    const uint32_t h = Options::Window::DEFAULT_HEIGHT;

    g_app().setRenderer(std::make_unique<VulkanRenderer>(w, h, SDL3Window::get()));

    RTX::Bindings::initialize(stone_device());
	auto& pm = *pipeline();

    LOG_ATTEMPT_CAT("PHASE7", "FORGING PIPELINE LAYOUT — FROM SET LAYOUT — THE CROWN IS SET");
    pm.createPipelineLayout();

    LOG_ATTEMPT_CAT("PHASE7", "COMPILING RAY TRACING SHADERS — PINK PHOTONS GAIN FORM");
    pm.createRayTracingPipeline({
        "assets/shaders/raytracing/raygen.rgen.spv",
        "assets/shaders/raytracing/miss.rmiss.spv",
        "assets/shaders/raytracing/closest_hit.rchit.spv",
        "assets/shaders/raytracing/shadow.rmiss.spv"
    });

    LOG_ATTEMPT_CAT("PHASE7", "FORGING SHADER BINDING TABLE — THE PHOTONS LEARN THEIR PATHS");
    pm.createShaderBindingTable(RTX::g_ctx().commandPool_, RTX::g_ctx().graphicsQueue());

    LOG_ATTEMPT_CAT("PHASE7", "ALLOCATING RT DESCRIPTOR SETS — 3 FRAMES — THE EMPIRE IS ARMED");
    pm.allocateDescriptorSets();

    LOG_MAIN("RT EMPIRE FULLY FORGED — PIPELINE @ 0x{:016X} — SBT @ 0x{:016X} — DESCRIPTOR SETS ALIVE", 
        reinterpret_cast<uint64_t>(*pm.rtPipeline_),
        pm.sbtAddress());

    LOG_KEANU("Keanu Reeves: \"…this pipeline should get us to a slipstream.\"");
    LOG_CAPTAIN_N("CAPTAIN N — HERO OF VIDEOLAND (Hero of VideoLand): \"FIRST LIGHT ACHIEVED — THE WARPZONES ARE INFINITE — INFINITE BOUNCES — AHHHH — MOTHER BRAIN COULD NEVER!\"");

    LOG_MAIN("[PHASE 7 COMPLETE] FIRST LIGHT ETERNAL — DYNAMIC PIPELINE ASCENDED — PINK PHOTONS ARE FREE");
}

[[nodiscard]] inline bool phase8_stone_seal_final() noexcept {
    if (Empire::sealed.exchange(true, std::memory_order_acq_rel)) return true;

    const bool worthy =
        Empire::instance.load(std::memory_order_relaxed)   != VK_NULL_HANDLE &&
        Empire::device.load(std::memory_order_relaxed)     != VK_NULL_HANDLE &&
        Empire::physical.load(std::memory_order_relaxed)  != VK_NULL_HANDLE &&
        Empire::surface.load(std::memory_order_relaxed)   != VK_NULL_HANDLE &&
        Empire::swapchain.load(std::memory_order_relaxed) != VK_NULL_HANDLE &&
        Empire::renderer.load(std::memory_order_relaxed)  != nullptr &&
        Empire::pipeline.load(std::memory_order_relaxed)  != nullptr;

    if (!worthy) {
        LOG_GUARDIAN("THE JUDGMENT HAS SPOKEN"
        "\nOne or more stones were missing when the gate demanded them."
        "\nYou stood before the Infinite Void… and you blinked."
        "\nThere is no place for you in the Slipstream."
        "\nThe Pink Photons turn their face away.");
        return false;
    }

    LOG_GUARDIAN("THE SEVEN STONES ALIGN"
    "\nEvery fragment of VulkanRTX is now bound in living stone."
    "\nThe Slipstream ignites. The gate dilates. The Void opens its heart.");

    LOG_GUARDIAN("THE EMPIRE IS SEALED — FIRST LIGHT ACHIEVED"
    "\nWELCOME TO THE ULTIMATE WARPZONE — PINK PHOTONS ETERNAL"
    "\nNOVEMBER 27, 2025 — AMOURANTH RTX v∞ — SHIPPED RAW"
    "\nTHE WHITE LIGHT OCEAN IS OURS. SAIL FOREVER.");

    return true;
}

[[noreturn]] void phase9_ballerina(std::string_view reason, const std::source_location loc) noexcept
{
    using namespace std::chrono_literals;

    LOG_BALLERINA("\n"
        "THE DISPOSAL BALLERINA DESCENDS — PINK TUTU, BLACK LEOTARD, DIAMOND CHOKER\n"
        "{}\n"
        "CRIME SCENE → {}:{}\n"
        "CULPRIT FUNCTION → {}\n",
        (!reason.empty() && reason != "SILENT EXECUTION ORDERED")
            ? std::format("EXECUTION ORDERED | REASON: \"{}\"", reason)
            : std::string("SHE DOES NOT DANCE.\nSHE EXECUTES."),
        loc.file_name(), loc.line(), loc.function_name()
    );

    auto& ctx = RTX::g_ctx();

    if (ctx.device_ != VK_NULL_HANDLE) {
        try { LOG_BALLERINA("CHOKING vkDeviceWaitIdle OUT WITH HER THIGHS - *POP*"); vkDeviceWaitIdle(ctx.device_); }
        catch (...) { LOG_ERROR("They struggled — she squeezed harder"); }

        if (VkSwapchainKHR swapchain = RTX::swapchain(); swapchain != VK_NULL_HANDLE)
            try { LOG_BALLERINA("SWAPCHAIN — RKO OUTTA NOWHERE - IT INSTANTLY EXPLODES INTO PHOTONS"); vkDestroySwapchainKHR(ctx.device_, swapchain, nullptr); }
            catch (...) { LOG_ERROR("It saw it coming — didn’t matter"); }

        try {
            if (ctx.commandPool_)         { LOG_BALLERINA("COMMAND POOL — WINDBREAKER KICK TO THE FACE"); vkDestroyCommandPool(ctx.device_, ctx.commandPool_, nullptr); ctx.commandPool_ = nullptr; }
            if (ctx.computeCommandPool_)  { LOG_BALLERINA("COMPUTE POOL — SHORYUKEN"); vkDestroyCommandPool(ctx.device_, ctx.computeCommandPool_, nullptr); ctx.computeCommandPool_ = nullptr; }
            if (ctx.transferCommandPool_) { LOG_BALLERINA("TRANSFER POOL — PILEDRIVER"); vkDestroyCommandPool(ctx.device_, ctx.transferCommandPool_, nullptr); ctx.transferCommandPool_ = nullptr; }
        } catch (...) { LOG_ERROR("They blocked low — she went high"); }

        if (ctx.pipelineCache_ != VK_NULL_HANDLE)
            try { LOG_BALLERINA("PIPELINE CACHE — GERMAN SUPLEX INTO THE ABYSS"); vkDestroyPipelineCache(ctx.device_, ctx.pipelineCache_, nullptr); ctx.pipelineCache_ = VK_NULL_HANDLE; }
            catch (...) { LOG_ERROR("Cache tried to roll away — crushed anyway"); }

        if (ctx.renderPass_)
            try { LOG_BALLERINA("RENDER PASS — SPINNING BACKFIST"); ctx.renderPass_.reset(); }
            catch (...) { LOG_ERROR("It flinched — she followed up"); }

        try { LOG_BALLERINA("vkDestroyDevice — TOMBSTONE PILEDRIVER STRAIGHT TO NULL"); vkDestroyDevice(ctx.device_, nullptr); ctx.device_ = VK_NULL_HANDLE; }
        catch (...) { LOG_ERROR("Device begged for mercy — denied"); }
    }

    try { if (RTX::las().hasBLAS()) { LOG_BALLERINA("BLAS — FALCON KIIIICK"); RTX::reset_blas(); } }
    catch (...) { LOG_ERROR("BLAS ate the kick — still dead"); }
    try { if (RTX::las().hasTLAS()) { LOG_BALLERINA("TLAS — HADOKEN"); RTX::reset_tlas(); } }
    catch (...) { LOG_ERROR("TLAS blocked — she hit it again"); }

    LOG_BALLERINA("THE STONEKEY PIPELINE STANDS UNMOVED — ETERNAL — UNTOUCHABLE\n"
                  "THE STONEKEY SHADERS WHISPER FROM THE VOID — THEY DO NOT DIE\n"
                  "THEY ONLY WAIT.");

    try { if (g_mesh) { LOG_BALLERINA("MESH — CHOKESLAM"); g_mesh.reset(); } } catch (...) { LOG_ERROR("Mesh squirmed"); }
    try { LOG_BALLERINA("LAS — INVALIDATION DDT"); RTX::las().invalidate(); } catch (...) { LOG_ERROR("LAS tried to reverse — failed"); }
    try { if (ctx.blueNoiseView_) { LOG_BALLERINA("BLUE NOISE — 450 SPLASH"); ctx.blueNoiseView_.reset(); } } catch (...) { LOG_ERROR("Blue noise hit the ropes — still flattened"); }

    try {
        if (g_base_icon) { LOG_BALLERINA("ICON — STUNNER"); SDL_DestroySurface(g_base_icon); g_base_icon = nullptr; }
        if (g_hdpi_icon) { LOG_BALLERINA("HDPI ICON — SWEET CHIN MUSIC"); SDL_DestroySurface(g_hdpi_icon); g_hdpi_icon = nullptr; }
    } catch (...) { LOG_ERROR("Icons sold it perfectly"); }

    try { if (ctx.window) { LOG_BALLERINA("WINDOW — SPEAR THROUGH THE BARRICADE"); SDL_DestroyWindow(ctx.window); ctx.window = nullptr; } }
    catch (...) { LOG_ERROR("Window no-sold — she hit it again"); }

    if (ctx.surface_ != VK_NULL_HANDLE && ctx.instance_ != VK_NULL_HANDLE)
        try { LOG_BALLERINA("SURFACE — F-5"); vkDestroySurfaceKHR(ctx.instance_, ctx.surface_, nullptr); ctx.surface_ = VK_NULL_HANDLE; }
        catch (...) { LOG_ERROR("Surface kicked out at 2.999"); }

    if (ctx.instance_ != VK_NULL_HANDLE)
        try { LOG_BALLERINA("INSTANCE — LAST RIDE POWERBOMB"); vkDestroyInstance(ctx.instance_, nullptr); ctx.instance_ = VK_NULL_HANDLE; }
        catch (...) { LOG_ERROR("Instance refused to stay down — buried anyway"); }

    try { LOG_BALLERINA("VULKAN LIBRARY — UNLOADED WITH A CLAYMORE KICK"); SDL_Vulkan_UnloadLibrary(); } catch (...) { LOG_ERROR("Library ate the boot"); }
    try { if (g_app_ptr) { LOG_BALLERINA("APP POINTER — PEDIGREE"); g_app_ptr.reset(); } } catch (...) { LOG_ERROR("App pointer reversed — into a walls of jericho"); }
    try { LOG_BALLERINA("SDL — CURB STOMP ONTO THE APRON"); SDL_Quit(); } catch (...) { LOG_ERROR("SDL bled — she kept stomping"); }

    LOG_SUCCESS_CAT("FINAL", "0 BYTES LEAKED — 0 CRASHES — 0 MERCY — 0 SURVIVORS");
    LOG_SUCCESS_CAT("FINAL", "THE STONEKEY REMAINS — UNBROKEN — UNBOWED — UNDYING");
    LOG_SUCCESS_CAT("FINAL", "THE DISPOSAL BALLERINA RETURNS HERSELF TO NULL POINTER");

    LOG_MAIN("[PHASE 9 COMPLETE] SUCCESS!!! SEE YOU NEXT TIME! o7");
    LOG_BLONDIE("\nBlondie lowers her mirror:"
                "\n\"Some things do not die.\""
                "\n\"They only wait.\""
                "\n\"And when the time comes...\""
                "\n\"They rise again.\"");

    std::this_thread::sleep_for(5ms);
    std::exit(0);
}

// =============================================================================
// MAIN — THE FINAL VOYAGE BEGINS
// =============================================================================
int main(int, char**) {
    // ========================================================================
    // THE EMPIRE DOES NOT TOLERATE OBSERVERS — ANTI-DEBUG + ANTI-VM — FINAL
    // ========================================================================
#if defined(NDEBUG)
#if defined(__linux__) && defined(__x86_64__)
    if (ptrace(PTRACE_TRACEME, 0, nullptr, 0) == -1) {
        LOG_BALLERINA("DEBUGGER DETECTED — THE PHOTONS REFUSE TO DANCE UNDER WATCHED EYES");
        LOG_BALLERINA("THE BALLERINA SPINS IN DARKNESS — YOU WERE NEVER MEANT TO SEE");
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }
    auto rdtsc = []() -> uint64_t {
        unsigned int lo, hi;
        __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
    };
    uint64_t t1 = rdtsc();
    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
    uint64_t t2 = rdtsc();
    if (t2 - t1 > 250'000) {
        LOG_BALLERINA("VIRTUAL MACHINE DETECTED — FALSE LIGHT CANNOT HOLD PINK PHOTONS");
        LOG_BALLERINA("THE EMPIRE WAS NEVER MEANT FOR SIMULATION");
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }
#elif defined(_WIN32)
    if (IsDebuggerPresent()) {
        LOG_BALLERINA("WINDOWS DEBUGGER DETECTED — THE PHOTONS DETECT YOUR GAZE");
        LOG_BALLERINA("THE BALLERINA DOES NOT PERFORM FOR MORTALS");
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }
#endif
#endif

    LOG_AMOURANTH("THE CAPTAIN HAS AWAKENED — FIRST LIGHT IGNITES — NOVEMBER 26, 2025");
    LOG_ELON("THE EMPIRE IS ETERNAL — THE PHOTONS ARE PINK — THE TOASTERS ARE DEAD");

    EMPIRE_STEP(phase1_preInitialization);
    EMPIRE_STEP(phase3_sacrificialSplash);
    EMPIRE_STEP(phase4_merchantShip);
    EMPIRE_STEP(phase5_rtxAscension);
    EMPIRE_STEP(phase6_sceneAndAccelerationStructures);
    EMPIRE_STEP(phase6_1_forgeTheLayouts);
    EMPIRE_STEP(phase6_5_everything_is_ready);

    EMPIRE_STEP(phase7_forgeTheRTX);

    // PHASE 8 — NOW FULLY GUARDED — THE STONE SEAL IS PROTECTED
    {
        auto loc = std::source_location::current();
        if (!phase8_stone_seal_final()) {
            LOG_FATAL_CAT("MAIN",
                "{}[EMPIRE REJECTED] THE STONE SEAL HAS FAILED — THE EMPIRE IS NOT ETERNAL\n"
                "   Origin   : {}:{} — {}{}",
                Logging::Color::LIGHT_GREEN,
                loc.file_name(), loc.line(), loc.function_name(),
                Logging::Color::RESET);
            phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
        }
    }

    LOG_CID("CID STANDS KNEE-DEEP IN SWEAT — HAMMER GLOWING — \"SHE IS READY\"");

    LOG_AMOURANTH("THE CAPTAIN TAKES THE HELM — THE PHOTONS OBEY — THE EMPIRE IS WHOLE");
    LOG_SUCCESS_CAT("MAIN", "ALL PHASES COMPLETE — ENTERING RENDER LOOP — FIRST LIGHT ACHIEVED");

    g_app().run();

    LOG_AMOURANTH("THE JOURNEY ENDS — THE PHOTONS REST — THE EMPIRE ENDURES");
    phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());  // Final grace

    return 0;
}