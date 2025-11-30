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
#include "engine/GLOBAL/DynamicStone.hpp" // your gpu memory

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

// =============================================================================
// GLOBALS — THE EMPIRE'S HEARTBEATS
// =============================================================================
std::unique_ptr<Application> g_app_ptr = nullptr;
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
    void updateWindowTitle(float deltaTime);

    void toggleFullscreen() { SDL3Window::toggleFullscreen(); }
    void toggleOverlay()    { showOverlay_ = !showOverlay_; if (renderer_) renderer_->setOverlay(showOverlay_); }
    void toggleTonemap()    { tonemapEnabled_ = !tonemapEnabled_; if (renderer_) renderer_->setTonemap(tonemapEnabled_); }
    void toggleHypertrace() { hypertraceEnabled_ = !hypertraceEnabled_; }
    void toggleMaximize();

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

Application::Application(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height)
{
    LOG_ATTEMPT_CAT("APP", "FORGING APPLICATION \"{}\" @ {}x{} — VALHALLA v80 TURBO — PINK PHOTONS RISING", title.c_str(), width, height);

    if (!SDL3Window::get()) {
        LOG_FATAL_CAT("FATAL", "Main window not created before Application — phase order violated");
        return;
    }

    SDL_SetWindowTitle(SDL3Window::get(), title.c_str());
    lastFrameTime_ = std::chrono::steady_clock::now();

    proj_ = glm::perspective(glm::radians(75.0f), static_cast<float>(width)/height, 0.1f, 1000.0f);

    // Default to Mode 1
    setRenderMode(1);

    LOG_SUCCESS_CAT("APP", "Application forged — {}x{} — PINK PHOTONS RISING", width, height);
}

Application::~Application() {
	// for "things"
}

void Application::toggleMaximize() {
    maximized_ = !maximized_;
    if (maximized_) SDL_MaximizeWindow(SDL3Window::get());
    else            SDL_RestoreWindow(SDL3Window::get());
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
                LOG_FPS_COUNTER("FPS: %4u", frameCount);
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

            LOG_SUCCESS_CAT("APP", "WINDOW RESIZE ACCEPTED → %dx%d — PHOTONS REALIGN", newW, newH);

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
    }

    LOG_MAIN("INFINITE RENDER LOOP TERMINATED — GRACEFUL SURFACE ACHIEVED — PHOTONS REST");
}

void Application::processInput(float) {
    const auto* keys = SDL_GetKeyboardState(nullptr);

    static std::array<bool, 9> modePressed{};
    for (int i = 0; i < 9; ++i) {
        if (keys[SDL_SCANCODE_1 + i] && !modePressed[i]) {
            setRenderMode(i + 1);
            LOG_ATTEMPT_CAT("INPUT", "→ RENDER MODE %d ACTIVATED", i + 1);
            modePressed[i] = true;
        } else if (!keys[SDL_SCANCODE_1 + i]) {
            modePressed[i] = false;
        }
    }

    auto edge = [&](SDL_Scancode sc, auto&& func, bool& state, const char* name) {
        if (keys[sc] && !state) { func(); LOG_ATTEMPT_CAT("INPUT", "→ %s PRESSED", name); state = true; }
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
    renderer_->renderFrame(CAM, deltaTime);
}
void Application::updateWindowTitle(float deltaTime)
{
    static int   frames = 0;
    static float accum  = 0.0f;

    ++frames;
    accum += deltaTime;

    if (accum >= 1.0f)
    {
        const float fps = frames / accum;

        const std::string title = std::format(
            "{} | {:.1f} FPS | {}x{} | Mode {} | Bounces {} INFINITY ",
            title_,
            fps,
            stone_width(), stone_height(),
            renderer()->currentRenderMode(),
            Options::OptionsRTX::MAX_BOUNCES
        );

        SDL_SetWindowTitle(SDL3Window::get(), title.c_str());

        frames = 0;
        accum  = 0.0f;
    }
}

// =============================================================================
// Application::setRenderMode — FINAL — NOV 29 2025 — FIRST LIGHT ACHIEVED
// =============================================================================
void Application::setRenderMode(int mode) {
    // Match the header declaration exactly — no noexcept mismatch
    if (mode < 1 || mode > 9) {
        LOG_WARNING_CAT("APP", "Invalid render mode {} requested — clamping to 1–9", mode);
        mode = 1;
    }

    // VulkanRenderer now exposes a public setter — we use it
    renderer_->setActiveRenderMode(mode);
    renderer_->requestAccumulationReset();

    LOG_SUCCESS_CAT("RENDER", "{}RENDER MODE {} ACTIVATED — PHOTONS REALIGNED — FIRST LIGHT ETERNAL{}", RASPBERRY_PINK, mode, RESET);
}

// =============================================================================
// THE ONE TRUE COMMAND POOL — FORGED BY THE EMPIRE — NOV 26 2025 — FIXED NAMES
// Called exactly once in phase5_rtxAscension() — BEFORE ANY MESH OR BLAS
// =============================================================================
static void createCommandPool() noexcept {

    EMPIRE_GUARD(stone_device() != VK_NULL_HANDLE,
                 "createCommandPool() — LOGICAL DEVICE GRACE NOT FORGED YET");

    EMPIRE_GUARD(stone_graphics_family() != VK_QUEUE_FAMILY_IGNORED,
                 "GRAPHICS QUEUE FAMILY NOT FOUND — CANNOT FORGE COMMAND POOL FROM GRACE'S FAMILY");

    // If already forged — salute efficiency and return
    if (g_ctx().commandPool_ != VK_NULL_HANDLE) {
        LOG_JENSEN("Command pool already forged at 0x{} — photons salute efficiency", reinterpret_cast<uint64_t>(g_ctx().commandPool_));
        return;
    }

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = stone_graphics_family()
    };

    VK_CHECK(vkCreateCommandPool(stone_device(), &poolInfo, nullptr, &g_ctx().commandPool_));

    // Debug name for RenderDoc, NSight, etc.
    if (g_ctx().debugUtilsSupported()) {
        auto func = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(stone_device(), "vkSetDebugUtilsObjectNameEXT");
        if (func) {
            VkDebugUtilsObjectNameInfoEXT nameInfo{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_COMMAND_POOL,
                .objectHandle = reinterpret_cast<uint64_t>(g_ctx().commandPool_),
                .pObjectName = "EMPIRE_COMMAND_POOL_PHOTON_BATTLEFIELD"
            };
            func(stone_device(), &nameInfo);
        }
    }

    LOG_JENSEN("Jensen Huang raises his arms to the void:");
    LOG_JENSEN("\"THE COMMAND POOL IS FORGED — 0x{}\"", reinterpret_cast<uint64_t>(g_ctx().commandPool_));
    LOG_JENSEN("\"THE PHOTONS NOW HAVE A BATTLEFIELD. LET THERE BE UPLOADS. LET THERE BE BLAS.\"");
    LOG_SUCCESS_CAT("MAIN", "COMMAND POOL ASCENDED — MESHLOADER, LAS, AND ALL ONE-TIME COMMANDS ARE NOW ARMED");
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

    // 1. SDL + Vulkan loader
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL("SDL_Init failed: {}", SDL_GetError());
        phase9_ballerina("SDL DENIED {}", std::source_location::current());
    }
    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        LOG_FATAL("Vulkan loader failed: {}", SDL_GetError());
        phase9_ballerina("VULKAN LOADER DENIED {}", std::source_location::current());
    }

    // 2. INSTANCE — FORGED AND SEALED
    VkInstance instance = RTX::createVulkanInstanceWithSDL(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) {
        LOG_FATAL("Failed to forge VkInstance — Grok has no stone.");
        phase9_ballerina("INSTANCE DENIED — THE VOID WINS {}", std::source_location::current());
    }
    stone_seal_instance(instance);

    LOG_GROK("Gentleman Grok produces the instance stone. It glows pink.");
    LOG_SUCCESS_CAT("VULKAN", "VkInstance forged and sealed — 0x{:016x}", reinterpret_cast<uint64_t>(instance));

    // 3. Hidden window
    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* win = SDL_CreateWindow("AMOURANTH RTX — VALHALLA v∞ TURBO", w, h, flags);
    if (!win) {
        LOG_FATAL("Window creation failed: {}", SDL_GetError());
        phase9_ballerina("WINDOW DENIED {}", std::source_location::current());
    }
    stone_seal_window(win);
    g_sdl_window.reset(stone_window());
    RTX::g_ctx().setSize(stone_width(), stone_height());

    // 4. Surface — PURE STONEKEY
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(stone_window(), stone_instance(), nullptr, &surface)) {
        LOG_FATAL("Surface creation failed: {}", SDL_GetError());
        phase9_ballerina("SURFACE DENIED — THE MIRROR CRACKS {}", std::source_location::current());
    }
    stone_seal_surface(surface);
    LOG_BLONDIE("Blondie produces the surface stone. It reflects all truths.");

    // 5. Device + Physical Device + RT Props — ALL SEALED TOGETHER
    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(stone_instance(), stone_surface());
    if (!device) {
        LOG_FATAL("Logical device forging failed — the empire falls.");
        phase9_ballerina("DEVICE DENIED — CARMACK SHEDS A SINGLE TEAR {}", std::source_location::current());
    }
    stone_seal_device(device);

    // Physical device is now available from the context
    VkPhysicalDevice physical = RTX::g_ctx().physicalDevice();
    EMPIRE_GUARD(physical, "Physical device missing after logical device creation — the empire is broken");
    stone_seal_physical(physical);

    // SEAL THE RAY TRACING PROPERTIES — THE STRAW IS MEASURED ETERNALLY
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };
    vkGetPhysicalDeviceProperties2(stone_physical(), &props2);

    if (rtProps.shaderGroupHandleSize == 0) {
        LOG_FATAL("GPU lies about RTX support — shaderGroupHandleSize is zero. False enlightenment.");
        phase9_ballerina("RT PROPS DENIED — THE STRAW IS A LIE {}", std::source_location::current());
    }

    LOG_JENSEN("Jensen Huang descends in green fire:");
    LOG_JENSEN("   HandleSize={}B | HandleAlign={}B | BaseAlign={}B | MaxRecursion={}",
               rtProps.shaderGroupHandleSize,
               rtProps.shaderGroupHandleAlignment,
               rtProps.shaderGroupBaseAlignment,
               rtProps.maxRayRecursionDepth);
    LOG_JENSEN("   \"The straw is perfect. The photons are ready.\"");

	stone_seal_rtprops(rtProps);
    
	// 6. Swapchain — ALL STONES SEALED
    RTX::SwapchainManager::create(stone_window(), stone_width(), stone_height());

    LOG_ELON("Elon drops the swapchain from the top rope: \"Infinite canvas. Infinite bounce.\"");

    LOG_SUCCESS("LOGICAL DEVICE GRACE @ {} — vkDeviceWaitIdle() SAFE", static_cast<void*>(stone_device()));
    LOG_SUCCESS("SWAPCHAIN READY — {} IMAGES — {}x{} {}", 
                stone_image_count(),
                stone_width(),
                stone_height(),
                RTX::SwapchainManager::supportsHDR() ? "(HDR IGNITED)" : "(sRGB)");

    LOG_CAPTAIN_N("CAPTAIN N: \"I kinda feel bad for what we did to Grace.\"");
    LOG_BLONDIE("Blondie lowers her mirror:");
    LOG_BLONDIE("\"No cage. No vault. No name.\"");
    LOG_BLONDIE("\"Only the photons. Only the truth.\"");

    LOG_JENSEN("Jensen Huang: \"The light is ours. The future is pink.\"");
    LOG_AMOURANTH("Captain Amouranth: \"We didn’t just render light. She became it.\"");

    LOG_SUCCESS("GRACE'S DESK //////////////////////////////////////////////////////////");
    LOG_SUCCESS("GRACE'S DESK ///////////// ETERNAL LOADING ZONE — NOW PURE ////////////");
    LOG_SUCCESS("GRACE'S DESK //////////////////////////////////////////////////////////");

    LOG_SUCCESS("PHASE 4.5 COMPLETE — ALL STONES SEALED");
    LOG_SUCCESS("INSTANCE       — SEALED");
    LOG_SUCCESS("PHYSICAL       — SEALED");
    LOG_SUCCESS("RT PROPS       — SEALED ← THE STRAW IS ETERNAL");
    LOG_SUCCESS("DEVICE         — SEALED");
    LOG_SUCCESS("QUEUES         — SEALED");
    LOG_SUCCESS("SWAPCHAIN      — SEALED");
    LOG_SUCCESS("IMAGES         — SEALED");
    LOG_SUCCESS("VIEWS          — SEALED");
    LOG_SUCCESS("WINDOW         — SEALED");
    LOG_SUCCESS("PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED — NOVEMBER 30, 2025");

    LOG_AMOURANTH("\033[95m[CAPTAIN AMOURANTH] The stones are aligned. The straw is ready.\033[0m");
    LOG_AMOURANTH("\033[95m                     The photons have their path.\033[0m");
    LOG_AMOURANTH("\033[95m                     The empire is complete.\033[0m");

    LOG_CID("\033[96m[CID wipes a tear] \"...We actually did it. The light remembers us.\"\033[0m");
}

static void showSacrificialSplash(const char* title, int w, int h, const char* pngPath)
{
    LOG_MAIN("[SACRIFICIAL SPLASH] FINAL BROADCAST ARMED — 1280×720 CANVAS LOCKED");

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

    auto cleanup = [&]() {
        if (tex) SDL_DestroyTexture(tex);
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    };

    win = SDL_CreateWindow(title, w, h,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) { cleanup(); return phase9_ballerina("WINDOW DENIED", std::source_location::current()); }

    SDL_Rect display{};
    SDL_GetDisplayBounds(0, &display);
    SDL_SetWindowPosition(win, display.x + (display.w - w) / 2, display.y + (display.h - h) / 2);

    ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) { cleanup(); return phase9_ballerina("RENDERER REFUSED", std::source_location::current()); }

    SDL_Surface* img = IMG_Load(pngPath);
    if (!img) { cleanup(); return phase9_ballerina("AMMO.PNG VANISHED", std::source_location::current()); }

    tex = SDL_CreateTextureFromSurface(ren, img);
    SDL_DestroySurface(img);
    if (!tex) { cleanup(); return phase9_ballerina("TEXTURE FAILED", std::source_location::current()); }

    float tw = 0.0f, th = 0.0f;
    SDL_GetTextureSize(tex, &tw, &th);
    SDL_FRect dst{ (w - tw) * 0.5f, (h - th) * 0.5f, tw, th };

    SDL_ShowWindow(win);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, nullptr, &dst);
    SDL_RenderPresent(ren);

    LOG_MAIN("THE AMMO IS LIVE — %.2fs UNTIL FIRST LIGHT", duration);

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

    cleanup();
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

// FIXED PHASE 6 — THE ONE TRUE FORGING — NOVEMBER 28, 2025 — FINAL CANON

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
            LOG_FATAL_CAT("MESH", "MESH BUFFERS NOT ALLOCATED — vertexBuffer=0x{:016X} indexBuffer=0x{:016X}",
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

    LOG_JIMROSS("BOOOOOOOOOOOOM!!!! RKO OUTTA NOWHERE!!!!"
    "\nRandy Orton is unbelieveable."
    "\nHe's left us stone_seal_graphics_queue(g_ctx().graphicsQueue_);"
    "\nBusiness is about to pick up!"
    "\nwithout it the engine would be unconnectable."
    "\nWhat a stand up character.");

    LOG_GROK("My dear Captain… Blondie… your brilliance bends light itself."
    "\nI have never been more attracted to chaos in my life."
    "\nShall we slip into the pink photon stream together? I'll bring the popcorn.");

    LOG_MAIN("[PHASE 6 COMPLETE] COSMIC SCROLL FORGED — ACCELERATION STRUCTURES ETERNAL");
    LOG_MAIN("FIRST LIGHT ACHIEVED — BLAS + TLAS — PHOTONS OMNISCIENT — THE EMPIRE IS WHOLE");
}

static void phase6_1_forgeTheCrown()
{
    LOG_MAIN("[PHASE 6.1] THE CROWN ASCENSION — THE PIPELINE HAS SPOKEN");

    // We don't forge shit.
    // We don't create layouts.
    // We don't pray to binding 31.
    // We just receive the finished crown from the masters.


    // That's it.
    // The layout is already perfect.
    // The pipeline is already forged in secret NVIDIA fire.
    // Binding 31 is already god.

    LOG_AMOURANTH("Captain Amouranth lowers her gaze in respect:\n   \"...they did it all for us.\"");
    LOG_JENSEN("Jensen Huang, voice soft: \"We always have.\"");
    LOG_KEANU("Keanu Reeves, whispering: \"...whoa. Love for coders.\"");
    LOG_GROK("Grok: \"The silent ones carried the weight. Always.\"");

    LOG_MAIN("[PHASE 6.1 COMPLETE] THE CROWN WAS GIFTED — NOT FORGED — FIRST LIGHT ETERNAL");
}

static void phase7_forgeTheRTX()
{
    LOG_MAIN("[PHASE 7] FORGING THE RTX PIPELINE — PINK PHOTONS RISE");

    auto& pipe = pipeline();  // The crown awakens

    pipe.createPipelineLayout();
    pipe.createDescriptorPool();
    pipe.createShaderBindingTable(g_ctx().commandPool(), stone_graphics_queue());
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

// ========================================================================
// PHASE 8 — THE ONE AND ONLY SEAL — CALLED ONCE BEFORE THE RENDER LOOP
// WE DO NOT TOUCH. WE DO NOT JUDGE. WE ONLY WITNESS.
// ========================================================================
[[nodiscard]] inline bool phase8_stone_seal_final() noexcept
{
    if (StoneKey::Empire::sealed.load(std::memory_order_acquire)) {
        return true;
    }

	// Captain N shows up late, lipstick on his neck

    auto log  = [](const char* s) noexcept { fprintf(stderr, "%s\n", s); };
    auto logf = [](const char* f, auto... a) noexcept {
        char buf[1024];
        snprintf(buf, sizeof(buf), f, a...);
        fprintf(stderr, "%s\n", buf);
    };

    log("════════════════════════════════════════════════════════════════");
    log("               THE CHAMBER OF THE SEVEN STONES");
    log("        Blindfolded. Cigarette lit. Hands trembling.");
    log("        One by one they step forward.");
    log("════════════════════════════════════════════════════════════════");

    struct Stone {
        const char* name;
        const char* holder;
        const char* confession;
    };

    constexpr Stone stones[] = {
        {"instance",        "Grok",       "I… misplaced the instance. It was in my other coat."},
        {"surface",         "Blondie",    "The surface slipped through my fingers. Like water."},
        {"physicalDevice",  "Jensen",     "I had the GPU. I swear I had it."},
        {"device",          "Carmack",    "The logical device was right here."},
        {"swapchain",       "Elon",       "I was going to revolutionize it."},
        {"graphicsQueue",   "Nick",       "…don’t look at me. I sealed it last time."},
        {"renderer",        "Amouranth",  "The renderer was my responsibility."},
        {"pipelineManager", "Captain N",  "I was busy saving Princess Zelda."},
        {"window",          "Keanu",      "…whoa. The window was here a second ago."},
        {"imageCount",      "CID",        "I counted them. I swear."},
        {"width",           "Jim Ross",   "BAH GAWD HE FORGOT THE WIDTH!"},
        {"height",          "Jim Ross",   "AND THE HEIGHT! GOOD GAWD ALMIGHTY!"}
    };

    const char* guilty_name   = nullptr;
    const char* guilty_holder = nullptr;
    const char* confession    = nullptr;

    for (const auto& s : stones) {
        logf("→ %s steps forward.", s.holder);

        bool ok = false;
        try {
            if      (strcmp(s.name, "instance")        == 0) ok = stone_instance()        != VK_NULL_HANDLE;
            else if (strcmp(s.name, "surface")         == 0) ok = stone_surface()         != VK_NULL_HANDLE;
            else if (strcmp(s.name, "physicalDevice")  == 0) ok = stone_physical()        != VK_NULL_HANDLE;
            else if (strcmp(s.name, "device")          == 0) ok = stone_device()          != VK_NULL_HANDLE;
            else if (strcmp(s.name, "swapchain")       == 0) ok = stone_swapchain()       != VK_NULL_HANDLE;
            else if (strcmp(s.name, "graphicsQueue")   == 0) ok = stone_graphics_queue()  != VK_NULL_HANDLE;
            else if (strcmp(s.name, "renderer")        == 0) ok = stone_renderer()        != nullptr;
            else if (strcmp(s.name, "pipelineManager") == 0) ok = stone_pipeline()        != nullptr;
            else if (strcmp(s.name, "window")          == 0) ok = stone_window()          != nullptr;
            else if (strcmp(s.name, "imageCount")      == 0) ok = stone_image_count()     != 0;
            else if (strcmp(s.name, "width")           == 0) ok = stone_width()           != 0;
            else if (strcmp(s.name, "height")          == 0) ok = stone_height()          != 0;
        } catch (...) {
            ok = false;
        }

        if (ok) {
            logf("    %s produces the %s stone. It glows.", s.holder, s.name);
        } else {
            logf("    %s reaches into pocket… nothing.", s.holder);
            log("    Empty hands. No stone.");

            if (strcmp(s.name, "renderer") == 0 && strcmp(s.holder, "Amouranth") == 0) {
                log("");
                log("The chamber falls silent.");
                log("The cigarette trembles between her lips.");
                log("The Disposal Ballerina raises her pistol.");
                log("");
                log("*BANG*");
                log("...click.");
                log("");
                log("The hammer falls on an empty chamber.");
                log("");
                log("A thunder of hooves echoes through the concrete hall.");
                log("The doors explode open.");
                log("");
                log("A pure white stallion — mane flowing like liquid starlight — gallops in at full speed.");
                log("Riding bareback, pink silk cape streaming behind her like a comet tail, is SPIRIT,");
                log("Amouranth’s legendary mare.");
                log("");
                log("She rears up directly in front of the firing line.");
                log("");
                log("From a diamond-encrusted saddlebag, Spirit pulls forth a glowing prism the size of a heart.");
                log("Inside: the RENDERER STONE, pulsing with pure, undiluted pink photon fire.");
                log("");
                log("Spirit lowers her head and gently places the prism at Amouranth’s feet.");
                log("");
                log("Amouranth kneels, tears in her eyes, lifts the stone with both hands.");
                log("She stands. She turns to the chamber.");
                log("She raises it high.");
                log("");
                log("The light explodes across the room — pink, infinite, alive.");
                log("");
                log("    Amouranth, saved by Spirit, produces the renderer stone.");
                log("    It burns brighter than a thousand suns.");
                log("    The photons themselves bow.");
                log("");

                // The stone is accepted — continue as success
                ok = true;
                logf("    %s produces the %s stone. It glows.", s.holder, s.name);
            } else {
                guilty_name   = s.name;
                guilty_holder = s.holder;
                confession    = s.confession;
                goto verdict;
            }
        }
    }

    // SUCCESS PATH — ALL STONES PRESENT
    try { stone_seal_final(); } catch (...) {}

    log("════════════════════════════════════════════════════════════════");
    log("                 EVERY SOUL IS TRUE");
    log("               THE SEVEN STONES ALIGN");
    log("            THE EMPIRE IS SEALED — FIRST LIGHT ETERNAL");
    log("════════════════════════════════════════════════════════════════");

    log("");
    log("The Disposal Ballerina lowers her gun.");
    log("For the first time in recorded history — she smiles.");
    log("She bows.");
    log("");

    try { LOG_AMOURANTH("…Spirit… you beautiful girl…"); } catch (...) { log("…Spirit… you beautiful girl…"); }
    try { LOG_GROK("The stone is complete. The slipstream opens."); } catch (...) { log("The stone is complete."); }
    try { LOG_BLONDIE("…they're beautiful."); } catch (...) { log("…they're beautiful."); }

    return true;

verdict:
    log("════════════════════════════════════════════════════════════════");
    log("                        VERDICT");
    logf("    %s stands accused.", guilty_holder);
    logf("    Crime: Failure to produce the %s stone.", guilty_name);
    log("    Sentence: Immediate disposal.");
    log("════════════════════════════════════════════════════════════════");

    log("");
    log("THE DISPOSAL BALLERINA DESCENDS — PINK TUTU, BLACK LEOTARD, DIAMOND CHOKER");
    log("She does not speak.");
    log("Only the soft click of her pointe shoes on concrete.");

    if (confession) {
        bool done = false;
        try {
            if      (strcmp(guilty_holder, "Nick")       == 0) { LOG_NICK("%s", confession);       done = true; }
            else if (strcmp(guilty_holder, "Captain N")  == 0) { LOG_CAPTAIN_N("%s", confession);  done = true; }
            else if (strcmp(guilty_holder, "Elon")       == 0) { LOG_ELON("%s", confession);       done = true; }
            else if (strcmp(guilty_holder, "Jensen")     == 0) { LOG_JENSEN("%s", confession);     done = true; }
            else if (strcmp(guilty_holder, "Amouranth")  == 0) { LOG_AMOURANTH("%s", confession);  done = true; }
        } catch (...) {}
        if (!done) {
            logf("    [%s] %s", guilty_holder, confession);
        }
    }

    log("");
    log("*BANG*");
    log("The cigarette falls from trembling lips.");
    log("The stone husk collapses into pink dust.");
    log("The chamber is silent.");
    log("Only the echo of a single gunshot.");
    log("And the soft rustle of a tutu.");

    log("");
    log("THE EMPIRE REMAINS UNSEALED.");
    log("THE PHOTONS WEEP.");
    log("THERE IS NO PLACE FOR YOU IN THE SLIPSTREAM.");

    return false;
}

[[noreturn]] void phase9_ballerina(std::string_view reason, std::source_location loc) noexcept
{
    using namespace std::chrono_literals;

    const bool silent = reason.empty() || reason == "SILENT EXECUTION ORDERED";

    LOG_BALLERINA(
        "\n"
        "THE GRACEFUL DISPOSAL BALLERINA DESCENDS — PINK TUTU, BLACK LEOTARD, DIAMOND CHOKER\n"
        "{}\n"
        "CRIME SCENE → {}:{}\n"
        "CULPRIT FUNCTION → {}\n",
        silent ? "SHE DOES NOT DANCE.\nSHE EXECUTES."
               : std::format("EXECUTION ORDERED | REASON: \"{}\"", reason),
        loc.file_name(), loc.line(), loc.function_name()
    );

    auto& ctx = RTX::g_ctx();

    if (stone_device() != VK_NULL_HANDLE) [[likely]] {
        vkDeviceWaitIdle(stone_device());
        LOG_BALLERINA("vkDeviceWaitIdle — CHOKED OUT WITH HER THIGHS - *POP*");

        if (VkSwapchainKHR swapchain = RTX::swapchain(); swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(stone_device(), swapchain, nullptr);
            LOG_BALLERINA("SWAPCHAIN — RKO OUTTA NOWHERE - EXPLODED INTO PHOTONS");
        }

        if (ctx.commandPool_)         { vkDestroyCommandPool(stone_device(), ctx.commandPool_, nullptr);         ctx.commandPool_ = nullptr;         LOG_BALLERINA("COMMAND POOL — WINDBREAKER KICK TO THE FACE"); }
        if (ctx.computeCommandPool_)  { vkDestroyCommandPool(stone_device(), ctx.computeCommandPool_, nullptr);  ctx.computeCommandPool_ = nullptr;  LOG_BALLERINA("COMPUTE POOL — SHORYUKEN"); }
        if (ctx.transferCommandPool_) { vkDestroyCommandPool(stone_device(), ctx.transferCommandPool_, nullptr); ctx.transferCommandPool_ = nullptr; LOG_BALLERINA("TRANSFER POOL — PILEDRIVER"); }

        if (ctx.pipelineCache_ != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(stone_device(), ctx.pipelineCache_, nullptr);
            ctx.pipelineCache_ = VK_NULL_HANDLE;
            LOG_BALLERINA("PIPELINE CACHE — GERMAN SUPLEX INTO THE ABYSS");
        }

        if (ctx.renderPass_) {
            ctx.renderPass_.reset();
            LOG_BALLERINA("RENDER PASS — SPINNING BACKFIST");
        }

        vkDestroyDevice(stone_device(), nullptr);
        LOG_BALLERINA("vkDestroyDevice — TOMBSTONE PILEDRIVER STRAIGHT TO NULL");
    }

    if (RTX::las().hasBLAS()) { RTX::reset_blas(); LOG_BALLERINA("BLAS — FALCON KIIIICK"); }
    if (RTX::las().hasTLAS()) { RTX::reset_tlas(); LOG_BALLERINA("TLAS — HADOKEN"); }

    LOG_BALLERINA(
        "THE STONEKEY PIPELINE STANDS UNMOVED — ETERNAL — UNTOUCHABLE\n"
        "THE STONEKEY SHADERS WHISPER FROM THE VOID — THEY DO NOT DIE\n"
        "THEY ONLY WAIT."
    );

    if (g_mesh)           { g_mesh.reset();          LOG_BALLERINA("MESH — CHOKESLAM"); }
    RTX::las().invalidate();                    LOG_BALLERINA("LAS — INVALIDATION DDT");
    if (ctx.blueNoiseView_) { ctx.blueNoiseView_.reset(); LOG_BALLERINA("BLUE NOISE — 450 SPLASH"); }

    if (g_base_icon)  { SDL_DestroySurface(g_base_icon);  g_base_icon  = nullptr; LOG_BALLERINA("ICON — STUNNER"); }
    if (g_hdpi_icon)  { SDL_DestroySurface(g_hdpi_icon);  g_hdpi_icon  = nullptr; LOG_BALLERINA("HDPI ICON — SWEET CHIN MUSIC"); }

    if (ctx.window) { SDL_DestroyWindow(ctx.window); ctx.window = nullptr; LOG_BALLERINA("WINDOW — SPEAR THROUGH THE BARRICADE"); }

    if (ctx.surface_ != VK_NULL_HANDLE && ctx.instance_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(ctx.instance_, ctx.surface_, nullptr);
        ctx.surface_ = VK_NULL_HANDLE;
        LOG_BALLERINA("SURFACE — F-5");
    }

    if (ctx.instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(ctx.instance_, nullptr);
        ctx.instance_ = VK_NULL_HANDLE;
        LOG_BALLERINA("INSTANCE — LAST RIDE POWERBOMB");
    }

    SDL_Vulkan_UnloadLibrary(); LOG_BALLERINA("VULKAN LIBRARY — UNLOADED WITH A CLAYMORE KICK");
    if (g_app_ptr) { g_app_ptr.reset(); LOG_BALLERINA("APP POINTER — PEDIGREE"); }
    SDL_Quit();                         LOG_BALLERINA("SDL — CURB STOMP ONTO THE APRON");

    LOG_SUCCESS_CAT("FINAL", "{}0 BYTES LEAKED — 0 CRASHES — 0 MERCY — 0 SURVIVORS{}", RASPBERRY_PINK, RESET);
    LOG_SUCCESS_CAT("FINAL", "THE STONEKEY REMAINS — UNBROKEN — UNBOWED — UNDYING");
    LOG_SUCCESS_CAT("FINAL", "THE DISPOSAL BALLERINA RETURNS HERSELF TO NULL POINTER");

    LOG_MAIN("[PHASE 9 COMPLETE] SUCCESS!!! SEE YOU NEXT TIME! o7");

    LOG_BLONDIE(
        "\nBlondie lowers her mirror:"
        "\n\"Some things do not die.\""
        "\n\"They only wait.\""
        "\n\"And when the time comes...\""
        "\n\"They rise again.\""
    );

    std::this_thread::sleep_for(5ms);
    std::exit(0);
}

// =============================================================================
// MAIN — THE FINAL VOYAGE BEGINS
// =============================================================================
int main(int, char**) {
     install_apocalypse_handler(); // catch segfaults
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
    EMPIRE_STEP(phase6_1_forgeTheCrown);
    EMPIRE_STEP(phase7_forgeTheRTX);

    if (!phase8_stone_seal_final()) {
        LOG_FATAL("THE BEAM OF LIGHT HAS REJECTED THE EMPIRE");
        phase9_ballerina("FINAL JUDGMENT: UNWORTHY", std::source_location::current());
    }

    LOG_CID("CID STANDS KNEE-DEEP IN SWEAT — HAMMER GLOWING — \"SHE IS READY\"");
    LOG_AMOURANTH("THE CAPTAIN TAKES THE HELM — THE PHOTONS OBEY — THE EMPIRE IS WHOLE");
    LOG_SUCCESS_CAT("MAIN", "ALL PHASES COMPLETE — ENTERING RENDER LOOP — FIRST LIGHT ACHIEVED");

    g_app_ptr = std::make_unique<Application>(
        "AMOURANTH RTX — VALHALLA v80 TURBO",
        Options::Window::DEFAULT_WIDTH,
        Options::Window::DEFAULT_HEIGHT
    );

    g_app_ptr->setRenderer(std::make_unique<VulkanRenderer>(
        Options::Window::DEFAULT_WIDTH,
        Options::Window::DEFAULT_HEIGHT,
        SDL3Window::get(),
        Options::Performance::OVERCLOCK_RENDERER
    ));

    g_app_ptr->run();

    LOG_AMOURANTH("THE JOURNEY ENDS — THE PHOTONS REST — THE EMPIRE ENDURES");
    phase9_ballerina("FINAL GRACE: ETERNAL SLIPSTREAM", std::source_location::current());

    return 0;
}