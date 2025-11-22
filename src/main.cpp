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
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/Splash.hpp"
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

static void phase2_and_3_sacrificialSplash()
{
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 2+3/10] SACRIFICIAL SPLASH RITUAL — BORN TO DIE{}", VALHALLA_GOLD, RESET);
    LOG_INFO_CAT("MAIN", "{}Splash realm initializing — pure SDL3 — will call SDL_Init() and SDL_Quit(){}", RASPBERRY_PINK, RESET);

    Splash::show(
        "AMOURANTH RTX — VALHALLA v80 TURBO — FIRST LIGHT",
        1280, 720,
        "assets/textures/ammo.png",
        "assets/audio/ammo.wav"
    );

    // NOW WE REBUILD THE LOGGING REALM — THE EMPIRE SPEAKS AGAIN
    LOG_SUCCESS_CAT("MAIN", "{}SPLASH RITUAL COMPLETE — SDL_Quit() PURGED — PHOTONS RESURRECTED{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}THE EMPIRE RISES FROM ABSOLUTE AUDIO — ONLY ECHO REMAINS{}", PURE_ENERGY, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}FIRST LIGHT ACHIEVED — THE PHOTONS HAVE SEEN HER — NOVEMBER 22, 2025{}", VALHALLA_GOLD, RESET);
}

static void phase4_mainWindowAndVulkanContext()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 4/10] FORGING MAIN WINDOW — CRUSH DEPTH{}", VALHALLA_GOLD, RESET);

    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);

    SDL_Window* win = SDL3Window::get();
    if (g_base_icon) SDL_SetWindowIcon(win, g_base_icon);

    LOG_SUCCESS_CAT("MAIN", "{}MAIN WINDOW FORGED — PHOTONS HAVE A HOME{}", EMERALD_GREEN, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 4 COMPLETE] HULL SEALED — PROCEEDING TO RTX CORE{}", VALHALLA_GOLD, RESET);
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

static void phase8_renderLoop()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 8/10] ETERNAL RENDER CYCLE — PHOTONS ENTER INFINITE LOOP{}", VALHALLA_GOLD, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}INFINITE LOOP ENGAGED — FIRST LIGHT PERMANENT — THE EMPIRE LIVES{}", PURE_ENERGY, RESET);
    g_app->run();
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
    SwapchainManager::cleanup();
    RTX::shutdown();
    SDL3Window::destroy();  // This calls SDL_Quit() exactly once

    LOG_SUCCESS_CAT("MAIN", "{}THE EMPIRE RESTS — PINK PHOTONS ETERNAL — NOVEMBER 21, 2025{}", DIAMOND_SPARKLE, RESET);
}

// =============================================================================
// MAIN — THE FINAL ASCENSION — PERFECT FLOW
// =============================================================================
// =============================================================================
// MAIN — THE FINAL ASCENSION — NOW CORRECT AND ETERNAL
// =============================================================================
int main(int, char**) {
    try {
        phase0_preInitialization();
        phase1_iconPreload();

        // SACRIFICIAL SPLASH — self-contained, calls SDL_Init + SDL_Quit
        phase2_and_3_sacrificialSplash();   // ← This is your existing function

        // Empire rises clean — ONE AND ONLY SDL_Init for the real engine
        phase4_mainWindowAndVulkanContext();   // ← You already have this as phase4
        phase5_rtxAscension();
        phase6_sceneAndAccelerationStructures();
        phase7_applicationAndRendererSeal();
        phase8_renderLoop();
        phase9_gracefulShutdown();            // ← Final phase — already correct
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