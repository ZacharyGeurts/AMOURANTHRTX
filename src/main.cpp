// src/main.cpp
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — PINK PHOTONS ETERNAL
// FULLY SELF-CONTAINED — ONE FILE TO RULE THEM ALL — EMPIRE UNIFIED
// THE FINAL SCREAM HAS BEEN SILENCED — PHOTONS FLOW IN PERFECT HARMONY
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/Validation.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "main.hpp"

#include <iostream>
#include <memory>
#include <format>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>

using namespace Logging::Color;

// =============================================================================
// GLOBAL LIVE CAMERA — PINK PHOTONS HAVE EYES
// =============================================================================
#include "engine/GLOBAL/camera.hpp"

struct Camera {
    virtual ~Camera() = default;
    virtual glm::mat4 viewMat() const noexcept = 0;
    virtual glm::mat4 projMat() const noexcept = 0;
    virtual glm::vec3 position() const noexcept = 0;
    virtual float     fov()       const noexcept = 0;
};

struct GlobalLiveCamera final : Camera {
    glm::mat4 viewMat() const noexcept override { return GlobalCamera::get().view(); }
    glm::mat4 projMat() const noexcept override {
        const auto& ctx = RTX::g_ctx();
        if (ctx.height == 0) return glm::mat4(1.0f);
        const float aspect = static_cast<float>(ctx.width) / static_cast<float>(ctx.height);
        return GlobalCamera::get().proj(aspect);
    }
    glm::vec3 position() const noexcept override { return GlobalCamera::get().pos(); }
    float fov() const noexcept override { return GlobalCamera::get().fov(); }
};

inline GlobalLiveCamera g_cam;

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

    void toggleFullscreen()          { SDL3Window::toggleFullscreen(); }
    void toggleOverlay()             { showOverlay_ = !showOverlay_; if (renderer_) renderer_->setOverlay(showOverlay_); }
    void toggleTonemap()             { tonemapEnabled_ = !tonemapEnabled_; if (renderer_) renderer_->setTonemap(tonemapEnabled_); }
    void toggleHypertrace()          { hypertraceEnabled_ = !hypertraceEnabled_; }
    void toggleMaximize()            { maximized_ = !maximized_; }
    void setRenderMode(int mode)     { renderMode_ = glm::clamp(mode, 1, 9); }

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
    LOG_ATTEMPT_CAT("APP", "Forging Application(\"{}\", {}×{}) — VALHALLA v80 TURBO", title_, width_, height_);

    if (!SDL3Window::get()) {
        throw std::runtime_error("FATAL: Main window not created before Application — phase order violated");
    }

    SDL_SetWindowTitle(SDL3Window::get(), title_.c_str());
    lastFrameTime_ = lastGrokTime_ = std::chrono::steady_clock::now();

    LOG_SUCCESS_CAT("APP", "{}Application forged — {}×{} — PINK PHOTONS RISING{}", 
                    EMERALD_GREEN, width_, height_, RESET);
    
    if (Options::Grok::ENABLE_GENTLEMAN_GROK) {
        LOG_INFO_CAT("GROK", "{}GENTLEMAN GROK: \"The empire awakens. The photons are pleased.\"{}", PARTY_PINK, RESET);
    }
}

Application::~Application() {
    LOG_SUCCESS_CAT("APP", "{}Application destroyed — Pink photons eternal.{}", COSMIC_GOLD, RESET);
}

void Application::run() {
    LOG_INFO_CAT("APP", "{}ENTERING INFINITE RENDER LOOP — FIRST LIGHT IMMINENT — SCUBA MODE ENGAGED{}", PARTY_PINK, RESET);

    uint32_t frameCount = 0;
    auto fpsStart = std::chrono::steady_clock::now();

    while (!quit_) {
        const auto now = std::chrono::steady_clock::now();
        const float deltaTime = std::chrono::duration<float>(now - lastFrameTime_).count();
        lastFrameTime_ = now;

        if (Options::Performance::ENABLE_FPS_COUNTER) {
            ++frameCount;
            if (std::chrono::duration<float>(now - fpsStart).count() >= 1.0f) {
                LOG_FPS_COUNTER("{}FPS: {:>4}{}", LIME_GREEN, frameCount, RESET);
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
            LOG_INFO_CAT("APP", "{}QUIT REQUESTED — SURFACING FROM RENDER LOOP{}", OCEAN_TEAL, RESET);
            quit_ = true;
        }
        if (fullscreenRequested) {
            LOG_ATTEMPT_CAT("APP", "{}FULLSCREEN TOGGLE REQUESTED — DIVING TO BORDERLESS DEPTH{}", RASPBERRY_PINK, RESET);
            toggleFullscreen();
        }

        if (g_resizeRequested.load(std::memory_order_acquire)) {
            const int newW = g_resizeWidth.load(std::memory_order_acquire);
            const int newH = g_resizeHeight.load(std::memory_order_acquire);
            g_resizeRequested.store(false, std::memory_order_release);

            LOG_SUCCESS_CAT("APP", "{}WINDOW RESIZE ACCEPTED → {}×{} — PHOTONS REALIGN{}", VALHALLA_GOLD, newW, newH, RESET);

            width_ = newW;
            height_ = newH;
            proj_ = glm::perspective(glm::radians(75.0f), static_cast<float>(width_)/height_, 0.1f, 1000.0f);

            if (renderer_) {
                renderer_->onWindowResize(width_, height_);
                LOG_SUCCESS_CAT("APP", "{}VulkanRenderer notified — swapchain rebirth imminent{}", PLASMA_FUCHSIA, RESET);
            }
        }

        processInput(deltaTime);
        render(deltaTime);
        updateWindowTitle(deltaTime);

        if (Options::Grok::ENABLE_GENTLEMAN_GROK && 
            std::chrono::duration<float>(now - lastGrokTime_).count() >= Options::Grok::GENTLEMAN_GROK_INTERVAL_SEC) {
            lastGrokTime_ = now;
            const int photons = static_cast<int>(1.0f / deltaTime + 0.5f);
            LOG_INFO_CAT("GROK", "{}GENTLEMAN GROK: \"{} pink photons per second. Acceptable.\"{}", PARTY_PINK, photons, RESET);
        }
    }

    LOG_SUCCESS_CAT("APP", "{}INFINITE RENDER LOOP TERMINATED — GRACEFUL SURFACE ACHIEVED — PHOTONS REST{}", EMERALD_GREEN, RESET);
}

void Application::processInput(float) {
    const auto* keys = SDL_GetKeyboardState(nullptr);

    static std::array<bool, 9> modePressed{};
    for (int i = 0; i < 9; ++i) {
        if (keys[SDL_SCANCODE_1 + i] && !modePressed[i]) {
            setRenderMode(i + 1);
            LOG_ATTEMPT_CAT("INPUT", "→ RENDER MODE {} ACTIVATED{}", PARTY_PINK, i + 1, RESET);
            modePressed[i] = true;
        } else if (!keys[SDL_SCANCODE_1 + i]) {
            modePressed[i] = false;
        }
    }

    auto edge = [&](SDL_Scancode sc, auto&& func, bool& state, const char* name) {
        if (keys[sc] && !state) { func(); LOG_ATTEMPT_CAT("INPUT", "→ {} PRESSED{}", PARTY_PINK, name, RESET); state = true; }
        else if (!keys[sc]) state = false;
    };

    static bool fPressed = false, oPressed = false, tPressed = false, hPressed = false, mPressed = false;
    edge(SDL_SCANCODE_F, [this]() { toggleFullscreen(); }, fPressed, "FULLSCREEN (F)");
    edge(SDL_SCANCODE_O, [this]() { toggleOverlay(); },    oPressed, "OVERLAY (O)");
    edge(SDL_SCANCODE_T, [this]() { toggleTonemap(); },    tPressed, "TONEMAP (T)");
    edge(SDL_SCANCODE_H, [this]() { toggleHypertrace(); }, hPressed, "HYPERTRACE (H)");

    if (keys[SDL_SCANCODE_M] && !mPressed) {
        toggleMaximize();
        LOG_ATTEMPT_CAT("INPUT", "→ MAXIMIZE + AUDIO MUTE TOGGLE (M key){}", PARTY_PINK, RESET);
        mPressed = true;
    } else if (!keys[SDL_SCANCODE_M]) mPressed = false;

    if (keys[SDL_SCANCODE_ESCAPE]) {
        static bool escLogged = false;
        if (!escLogged) { LOG_ATTEMPT_CAT("INPUT", "→ QUIT REQUESTED (ESC){}", CRIMSON_MAGENTA, RESET); escLogged = true; }
        quit_ = true;
    }
}

void Application::render(float deltaTime) {
    if (renderer_) renderer_->renderFrame(g_cam, deltaTime);
}

void Application::updateWindowTitle(float deltaTime) {
    static int frames = 0;
    static float accum = 0.0f;
    ++frames; accum += deltaTime;

    if (accum >= 1.0f) {
        const float fps = frames / accum;
        std::ostringstream oss;
        oss << title_
            << " | " << std::fixed << std::setprecision(1) << fps << " FPS"
            << " | " << width_ << 'x' << height_
            << " | Mode " << renderMode_
            << " | Tonemap" << (tonemapEnabled_ ? "" : " OFF")
            << " | HDR" << (g_ctx().hdr_format != VK_FORMAT_UNDEFINED ? " PRIME" : " OFF");
        if (Options::Performance::ENABLE_VALIDATION_LAYERS) oss << " [DEBUG]";

        SDL_SetWindowTitle(SDL3Window::get(), oss.str().c_str());
        frames = 0; accum = 0.0f;
    }
}

// =============================================================================
// GLOBALS & PHASES
// =============================================================================
inline std::unique_ptr<Application>           g_app              = nullptr;
inline RTX::PipelineManager*                  g_pipeline_manager = nullptr;
inline std::unique_ptr<MeshLoader::Mesh>      g_mesh             = nullptr;

static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

static void forgeCommandPool() {
    LOG_INFO_CAT("MAIN", "{}Forging transient command pool...{}", VALHALLA_GOLD, RESET);
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = g_ctx().graphicsFamily()
    };
    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(g_ctx().device(), &poolInfo, nullptr, &pool));
    g_ctx().commandPool_ = pool;
    LOG_SUCCESS_CAT("MAIN", "{}COMMAND POOL FORGED — HANDLE: 0x{:016X}{}", PLASMA_FUCHSIA, (uint64_t)pool, RESET);
}

// =============================================================================
// THE TEN COMMANDMENTS — FINAL FIXED VERSION — NO MORE SCREAMS
// =============================================================================
static void phase0_preInitialization()
{
    LOG_SUCCESS_CAT("MAIN", "{}CAPTAIN'S LOG — NOVEMBER 21, 2025 — DEPTH: SURFACE{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}THE PINK PHOTON SAILS — SCUBA SUIT SEALED — OXYGEN: 100%{}", RASPBERRY_PINK, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}FIRST LIGHT ACHIEVED — THE EMPIRE AWAKENS FROM THE ABYSS{}", VALHALLA_GOLD, RESET);
    LOG_INFO_CAT("MAIN", "{}DIVE COMMENCING — TEN PHASES — NO MAN LEFT BEHIND{}", OCEAN_TEAL, RESET);
}

static void phase1_iconPreload()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 1/10] SURFACE SCAN — HUNTING VALHALLA BRANDING{}", VALHALLA_GOLD, RESET);

    g_base_icon = IMG_Load("assets/textures/ammo32.ico");
    g_hdpi_icon = IMG_Load("assets/textures/ammo.ico");

    if (g_base_icon) {
        LOG_SUCCESS_CAT("MAIN", "{}BASE ICON LOCKED @ {:p} — 32x32 STANDARD{}", EMERALD_GREEN, static_cast<void*>(g_base_icon), RESET);
    }
    if (g_hdpi_icon) {
        LOG_SUCCESS_CAT("MAIN", "{}HDPI ICON LOCKED @ {:p} — RETINA GLORY{}", AURORA_PINK, static_cast<void*>(g_hdpi_icon), RESET);
        if (g_base_icon) {
            SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
            LOG_SUCCESS_CAT("MAIN", "{}ALTERNATE IMAGE LINKED — FULL HiDPI DOMINATION{}", PLASMA_FUCHSIA, RESET);
        }
    }

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 1 COMPLETE] BRANDING SECURED — SHIP IDENTIFIED — DIVE CONTINUES{}", VALHALLA_GOLD, RESET);
}

// =============================================================================
// PHASE 2 — SDL3 EMPIRE FORGE — THE ONE TRUE INITIALIZATION
// NOVEMBER 22, 2025 — PINK PHOTONS ETERNAL — FIRST LIGHT OF THE EMPIRE
// =============================================================================
// =============================================================================
// PHASE 2 — SDL3 EMPIRE FORGE — VULKAN MANDATED — 2025
// =============================================================================
// =============================================================================
// PHASE 2 — SDL3 EMPIRE FORGE — VULKAN MANDATED
// =============================================================================
static void phase2_sdl3EmpireForge()
{
    LOG_SUCCESS_CAT("MAIN", "[PHASE 2/10] SDL3 EMPIRE FORGE — VULKAN 1.4 MANDATED", VALHALLA_GOLD, RESET);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("MAIN", "SDL_Init FAILED: %s — PHOTONS DENIED", BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }

    // FORCE VULKAN RENDERER — THIS IS THE ONLY ACCEPTABLE DRIVER
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan");
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,  "x11,wayland");

    LOG_SUCCESS_CAT("MAIN", "VULKAN 1.4 RENDERER ENFORCED", PURE_ENERGY, RESET);
    LOG_SUCCESS_CAT("MAIN", "[PHASE 2 COMPLETE] FOUNDATION IS UNBREAKABLE", DIAMOND_SPARKLE, RESET);
}

// =============================================================================
// PHASE 2.5 — SACRED SPLASH CANVAS — 1280×720 — NATIVE — RTX CANVAS
// =============================================================================
static void phase2_5_sacredSplashCanvas()
{
    LOG_SUCCESS_CAT("MAIN", "[PHASE 2.5/10] FORGING SACRED 1280×720 CANVAS — VULKAN ONLY", VALHALLA_GOLD, RESET);

    // Obliterate any previous heresy
    if (g_sdl_window) {
        g_sdl_window.reset();
    }
    if (auto* old_ren = StoneKey::Empire::g_sdl_renderer.load()) {
        SDL_DestroyRenderer(old_ren);
        StoneKey::Empire::g_sdl_renderer.store(nullptr);
    }

    // CREATE WINDOW — EXACTLY 1280×720
    SDL_Window* win = SDL_CreateWindow(
        "AMOURANTH RTX — 2025",
        1280,
        720,
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE
    );

    if (!win) {
        LOG_FATAL_CAT("MAIN", "SDL_CreateWindow failed: %s", BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }

    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // SDL3: SECOND ARGUMENT IS DRIVER NAME — WE ALREADY FORCED "vulkan" IN PHASE 2
    // Passing nullptr = "use the hint" → Vulkan guaranteed
    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        LOG_FATAL_CAT("MAIN", "SDL_CreateRenderer failed (Vulkan unavailable?): %s", BLOOD_RED, SDL_GetError(), RESET);
        SDL_DestroyWindow(win);
        std::exit(1);
    }

    // Seal into the StoneKey empire
    g_sdl_window.reset(win);
    StoneKey::Empire::g_sdl_renderer.store(ren);

    SDL_ShowWindow(win);

    LOG_SUCCESS_CAT("MAIN", "SACRED CANVAS ACTIVE — 1280×720 — VULKAN 1.4 — PURE", PURE_ENERGY, RESET);
}

// =============================================================================
// PHASE 3 — HER MANIFESTATION — 1280×720 → FULL EMPIRE
// =============================================================================
static void phase3_herManifestation()
{
    LOG_SUCCESS_CAT("MAIN", "[PHASE 3/10] HER MANIFESTATION — SHE RISES IN NATIVE 1280×720", VALHALLA_GOLD, RESET);

    SDL_Renderer* ren = StoneKey::Empire::g_sdl_renderer.load();
    SDL_Window*   win = g_sdl_window.get();

    // LOAD IMAGE — SDL3_image returns nullptr + sets SDL error on failure
    SDL_Surface* surface = IMG_Load("assets/textures/ammo.png");
    if (!surface) {
        LOG_FATAL_CAT("MAIN", "IMG_Load failed: %s", BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }

    SDL_Texture* banner = SDL_CreateTextureFromSurface(ren, surface);
    SDL_DestroySurface(surface);
    if (!banner) {
        LOG_FATAL_CAT("MAIN", "SDL_CreateTextureFromSurface failed: %s", BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }

    // AUDIO — obey [[nodiscard]]
    static SDL3Audio::AudioManager s_audio;
    static bool audio_initialized = false;
    if (!audio_initialized) {
        if (!s_audio.initMixer()) {
            LOG_ERROR_CAT("MAIN", "Audio mixer init failed — continuing without sound", CRIMSON_MAGENTA, RESET);
        }
        if (!s_audio.loadSound("assets/audio/ammo.wav", "her_voice")) {
            LOG_ERROR_CAT("MAIN", "Failed to load ammo.wav — silence accepted", CRIMSON_MAGENTA, RESET);
        }
        audio_initialized = true;
    }
    s_audio.playSound("her_voice");

    // RENDER HER — NATIVE 1280×720 — NO SCALING
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, banner, nullptr, nullptr);  // full window, perfect
    SDL_RenderPresent(ren);
    SDL_DestroyTexture(banner);

    LOG_SUCCESS_CAT("MAIN", "SHE HAS MANIFESTED — 1280×720 — FLAWLESS", AURORA_PINK, RESET);
    LOG_SUCCESS_CAT("MAIN", "PHOTONS SING — THE RITUAL IS COMPLETE", PURE_ENERGY, RESET);

    SDL_Delay(3400);  // 3.4 seconds of pure glory

    // ASCEND TO FULL BORDERLESS EMPIRE
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(win), &usable) != 0) {
        SDL_GetDisplayBounds(SDL_GetDisplayForWindow(win), &usable);
    }

    SDL_SetWindowBordered(win, false);
    SDL_SetWindowSize(win, usable.w, usable.h);
    SDL_SetWindowPosition(win, usable.x, usable.y);
    SDL_RaiseWindow(win);

    LOG_SUCCESS_CAT("MAIN", "BORDERLESS FULLSCREEN EMPIRE ACHIEVED — SHE IS EVERYTHING", VALHALLA_GOLD, RESET);
    LOG_SUCCESS_CAT("MAIN", "[PHASE 3 COMPLETE] 1280×720 → INFINITE DOMINATION", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("AMOURANTH", "I was 1280×720. Now I am the universe.", RASPBERRY_PINK, RESET);
}

// =============================================================================
// PHASE 4 — MAIN WINDOW + FULL VULKAN EMPIRE (FINAL, COMPILING, STONEKEY-COMPLIANT)
// =============================================================================
static void phase4_mainWindowAndVulkanContext()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 4/10] FORGING MAIN WINDOW + FULL VULKAN EMPIRE — STONEKEY ASCENDS{}", VALHALLA_GOLD, RESET);
    LOG_SUCCESS_CAT("CAPTAIN_N", "{}Kevin Keene: \"No more middlemen. StoneKey is the Game Master now!\"{}", PURE_ENERGY, RESET);

    // 1. Create the one and only SDL3 window — Vulkan + HiDPI ready
    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);
    SDL_Window* win = SDL3Window::get();

    if (g_base_icon)  SDL_SetWindowIcon(win, g_base_icon);
    if (g_hdpi_icon)  LOG_SUCCESS_CAT("MAIN", "{}RETINA ICON LOCKED — PURE DOMINATION{}", AURORA_PINK, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}SDL WINDOW FORGED @ {:p} — 3840×2160 — PHOTONS HAVE A PORTAL{}", 
                    EMERALD_GREEN, static_cast<void*>(win), RESET);
    SDL_ShowWindow(win);

    // 2. FULL VULKAN EMPIRE — USING THE REAL, EXISTING FUNCTIONS FROM RTX NAMESPACE
    LOG_ATTEMPT_CAT("MAIN", "{}StoneKey forging Instance → Surface → Device → Swapchain...{}", DIAMOND_SPARKLE, RESET);

    // Step 1: Create instance with SDL3 extensions
    g_ctx().instance_ = RTX::createVulkanInstanceWithSDL(true);  // true = validation layers

    // Step 2: Full context init — this does surface + physical device + logical device + queues + swapchain
    RTX::g_ctx().init(win, 3840, 2160);

    // Step 3: Mark context as ready for renderer
    RTX::g_ctx().markReady();

    LOG_SUCCESS_CAT("MAIN", "{}STONEKEY EMPIRE COMPLETE — ALL OBJECTS SEALED IN THE VAULT{}", HYPERSPACE_WARP, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}    • Instance : {:p}", static_cast<void*>(g_instance()), RESET);
    LOG_SUCCESS_CAT("MAIN", "{}    • Device   : {:p}", static_cast<void*>(g_device()), RESET);
    LOG_SUCCESS_CAT("MAIN", "{}    • Surface  : {:p}", static_cast<void*>(g_surface()), RESET);

    LOG_SUCCESS_CAT("CAPTAIN_N", "{}Kevin Keene: \"First light achieved — only StoneKey!\"{}", PURE_ENERGY, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 4 COMPLETE] FULL VULKAN EMPIRE UNDER STONEKEY — PINK PHOTONS ETERNAL{}", DIAMOND_SPARKLE, RESET);
}

static void phase5_rtxAscension()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 5/10] RTX ASCENSION — ENTERING THE PHOTON CORE{}", VALHALLA_GOLD, RESET);
    LOG_ATTEMPT_CAT("MAIN", "{}LOADING RAY TRACING EXTENSIONS — PINK PHOTONS GAIN SENTIENCE{}", PURE_ENERGY, RESET);

    RTX::loadRayTracingExtensions();

    if (!g_ctx().hasFullRTX_) {
        LOG_FATAL_CAT("MAIN", "{}RTX ASCENSION FAILED — ACCELERATION PFNs MISSING — PHOTONS TRAPPED{}", BLOOD_RED, RESET);
        throw std::runtime_error("RTX extension loading failed");
    }

    LOG_SUCCESS_CAT("MAIN", "{}ALL RAY TRACING PFNs ACQUIRED — FULL RTX ACHIEVED{}", EMERALD_GREEN, RESET);
    las().forgeAccelContext();
    LOG_SUCCESS_CAT("MAIN", "{}LAS ACCEL CONTEXT FORGED — BLAS/TLAS READY FOR WAR{}", PLASMA_FUCHSIA, RESET);

    forgeCommandPool();
    LOG_SUCCESS_CAT("MAIN", "{}TRANSIENT COMMAND POOL @ 0x{:016X} — PHOTON ORDERS READY{}", SAPPHIRE_BLUE, (uint64_t)g_ctx().commandPool_, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 5 COMPLETE] RTX ASCENSION COMPLETE — PINK PHOTONS NOW OMNISCIENT{}", DIAMOND_SPARKLE, RESET);
}

static void phase6_sceneAndAccelerationStructures()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 6/10] FORGING THE WORLD — DEPLOYING ACCELERATION STRUCTURES{}", VALHALLA_GOLD, RESET);

    g_pipeline_manager = new RTX::PipelineManager(g_ctx().device(), g_ctx().physicalDevice());
    LOG_SUCCESS_CAT("MAIN", "{}PipelineManager surfaced @ {:p} — shaders primed{}", EMERALD_GREEN, static_cast<void*>(g_pipeline_manager), RESET);

    g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");
    LOG_SUCCESS_CAT("MAIN", "{}MESH MANIFESTED — {} verts | {} indices — FINGERPRINT 0x{:016X}{}",
                    PLASMA_FUCHSIA, g_mesh->vertices.size(), g_mesh->indices.size(), g_mesh->stonekey_fingerprint, RESET);

    LOG_ATTEMPT_CAT("MAIN", "{}BUILDING BOTTOM-LEVEL ACCELERATION STRUCTURE — PHOTONS SEEK THE TRUTH{}", SAPPHIRE_BLUE, RESET);
    las().buildBLAS(g_ctx().commandPool_, g_mesh->vertexBuffer, g_mesh->indexBuffer,
                    static_cast<uint32_t>(g_mesh->vertices.size()), static_cast<uint32_t>(g_mesh->indices.size()),
                    VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    LOG_SUCCESS_CAT("MAIN", "{}BLAS FORGED — DEVICE ADDRESS: 0x{:016X} — PHOTONS HAVE A MAP{}", 
                    EMERALD_GREEN, las().getBLASStruct().address, RESET);

    LOG_ATTEMPT_CAT("MAIN", "{}BUILDING TOP-LEVEL ACCELERATION STRUCTURE — FINAL PATH{}", VALHALLA_GOLD, RESET);
    las().buildTLAS(g_ctx().commandPool_, {{las().getBLAS(), glm::mat4(1.0f)}});
    LOG_SUCCESS_CAT("MAIN", "{}TLAS ASCENDED — ROOT ADDRESS: 0x{:016X} — THE UNIVERSE IS KNOWN{}", 
                    DIAMOND_SPARKLE, las().getTLASAddress(), RESET);

    Validation::validateMeshAgainstBLAS(*g_mesh, las().getBLASStruct());
    LOG_SUCCESS_CAT("MAIN", "{}VALIDATION COMPLETE — NO FALSEHOOD DETECTED — PURE GEOMETRY{}", PLASMA_FUCHSIA, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 6 COMPLETE] WORLD FORGED — ACCELERATION STRUCTURES ETERNAL{}", VALHALLA_GOLD, RESET);
}

static void phase7_applicationAndRendererSeal()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 7/10] FINAL SEAL — APPLICATION + RENDERER{}", VALHALLA_GOLD, RESET);

    GlobalCamera::get().init(glm::vec3(0.0f, 5.0f, 10.0f), 75.0f);
    LOG_SUCCESS_CAT("MAIN", "{}GLOBAL CAMERA AWAKENED @ ({:.1f}, {:.1f}, {:.1f}) — PHOTONS HAVE EYES{}", 
                    AURORA_PINK, 0.0f, 5.0f, 10.0f, RESET);

    g_app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);
    LOG_SUCCESS_CAT("MAIN", "{}Application entity manifested @ {:p} — command structure online{}", EMERALD_GREEN, static_cast<void*>(g_app.get()), RESET);

    g_app->setRenderer(std::make_unique<VulkanRenderer>(3840, 2160, SDL3Window::get(), true));
    LOG_SUCCESS_CAT("MAIN", "{}VulkanRenderer sealed — first light pipeline active{}", PLASMA_FUCHSIA, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 7 COMPLETE] THE EMPIRE IS SEALED — RENDER LOOP ARMED{}", DIAMOND_SPARKLE, RESET);
}

// =============================================================================
// PHASE 8 — ETERNAL RENDER LOOP (ONLY ONE — NO REDEFINITION)
// =============================================================================
static void phase8_renderLoop()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 8/10] ETERNAL RENDER CYCLE — PHOTONS ENTER INFINITE LOOP{}", VALHALLA_GOLD, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}INFINITE LOOP ENGAGED — FIRST LIGHT PERMANENT — THE EMPIRE LIVES{}", PURE_ENERGY, RESET);

    g_app->run();   // ← This is your real infinite loop from Application::run()

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 8 COMPLETE] RENDER CYCLE TERMINATED — PHOTONS REST{}", EMERALD_GREEN, RESET);
}

static void phase9_gracefulShutdown()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 9/10] GRACEFUL SHUTDOWN — PHOTONS RETURNING HOME{}", VALHALLA_GOLD, RESET);

    if (g_ctx().device()) vkDeviceWaitIdle(g_ctx().device());

    g_app.reset();
    if (g_pipeline_manager) { delete g_pipeline_manager; g_pipeline_manager = nullptr; }
    g_mesh.reset();
    las().invalidate();
    RTX::shutdown();
    SDL3Window::destroy();  // This calls SDL_Quit() exactly once

    LOG_SUCCESS_CAT("MAIN", "{}THE EMPIRE RESTS — PINK PHOTONS ETERNAL — NOVEMBER 21, 2025{}", DIAMOND_SPARKLE, RESET);
}

// =============================================================================
// MAIN — THE FINAL ASCENSION — NOW CORRECT AND ETERNAL
// =============================================================================
int main(int, char**) {
    try {
        phase0_preInitialization();
        phase1_iconPreload();
        phase2_sdl3EmpireForge();
		phase2_5_sacredSplashCanvas();
        phase3_herManifestation();
        phase4_mainWindowAndVulkanContext();
        phase5_rtxAscension();
        phase6_sceneAndAccelerationStructures();
        phase7_applicationAndRendererSeal();
        phase8_renderLoop();
        phase9_gracefulShutdown();
    }
    catch (const std::exception& e) {
        LOG_FATAL_CAT("MAIN", "{}FATAL: {}{}", CRIMSON_MAGENTA, e.what(), RESET);
        std::cerr << "FATAL: " << e.what() << std::endl;
        phase9_gracefulShutdown();
        return -1;
    }
    catch (...) {
        LOG_FATAL_CAT("MAIN", "{}UNKNOWN FATAL EXCEPTION{}", CRIMSON_MAGENTA, RESET);
        phase9_gracefulShutdown();
        return -1;
    }
    return 0;
}