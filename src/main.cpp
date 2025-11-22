// src/main.cpp
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — PINK PHOTONS ETERNAL
// FULLY SELF-CONTAINED — NO HANDLE_APP.CPP — ONE FILE TO RULE THEM ALL
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/Splash.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/Validation.hpp"
#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_image.hpp"
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
// THE ONE TRUE GLOBAL CAMERA — FULLY DEFINED — NO FORWARD DECLARES — NUTS OUT
// PINK PHOTONS HAVE EYES — STONEKEY v9 — BRAINDEAD PERFECTION
// =============================================================================

#include "engine/GLOBAL/camera.hpp"  // ← Already included, but we demand it

// FULLY DEFINED CAMERA INTERFACE — NO FORWARD DECL IN RTXHandler.hpp ANYMORE
struct Camera {
    virtual ~Camera() = default;
    virtual glm::mat4 viewMat() const noexcept = 0;
    virtual glm::mat4 projMat() const noexcept = 0;
    virtual glm::vec3 position() const noexcept = 0;
    virtual float     fov()       const noexcept = 0;
};

// GLOBAL LIVE CAMERA — ALWAYS VALID — USED BY RENDERER
struct GlobalLiveCamera final : Camera {
    glm::mat4 viewMat() const noexcept override {
        return GlobalCamera::get().view();
    }
    glm::mat4 projMat() const noexcept override {
        const auto& ctx = RTX::g_ctx();
        if (ctx.height == 0) return glm::mat4(1.0f); // Prevent div0 during init
        const float aspect = static_cast<float>(ctx.width) / static_cast<float>(ctx.height);
        return GlobalCamera::get().proj(aspect);
    }
    glm::vec3 position() const noexcept override {
        return GlobalCamera::get().pos();
    }
    float fov() const noexcept override {
        return GlobalCamera::get().fov();
    }
};

// THE ONE TRUE CAMERA INSTANCE — GLOBAL, IMMORTAL, READY
inline GlobalLiveCamera g_cam;

// =============================================================================
// APPLICATION — NOW LIVES HERE — INSIDE MAIN — EMPIRE UNIFIED
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
    glm::mat4 view_ = glm::lookAt(glm::vec3(0, 5, 10), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

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

// =============================================================================
// APPLICATION IMPLEMENTATION — FULLY INLINE — NO .CPP — PURE DOMINION
// =============================================================================

Application::Application(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height),
      proj_(glm::perspective(glm::radians(75.0f), static_cast<float>(width)/height, 0.1f, 1000.0f))
{
    LOG_ATTEMPT_CAT("APP", "Forging Application(\"{}\", {}×{}) — VALHALLA v80 TURBO", title_, width_, height_);

    if (!SDL3Window::get()) {
        throw std::runtime_error("FATAL: Main window not created before Application — phase order violated");
    }

    SDL_SetWindowTitle(SDL3Window::get(), title_.c_str());
    lastFrameTime_ = lastGrokTime_ = std::chrono::steady_clock::now();

    LOG_SUCCESS_CAT("APP", "{}Application forged — {}×{} — STONEKEY v∞ window secured — PINK PHOTONS RISING{}", 
                    EMERALD_GREEN, width_, height_, RESET);
    
    if (Options::Grok::ENABLE_GENTLEMAN_GROK) {
        LOG_INFO_CAT("GROK", "{}GENTLEMAN GROK: \"The empire awakens. The photons are pleased.\"{}", 
                     PARTY_PINK, RESET);
    }
}

Application::~Application() {
    LOG_TRACE_CAT("APP", "Application::~Application() — beginning graceful shutdown");
    renderer_.reset();
    LOG_SUCCESS_CAT("APP", "{}Application destroyed — Empire preserved. Pink photons eternal.{}", COSMIC_GOLD, RESET);
}

void Application::run() {
    LOG_INFO_CAT("APP", "{}ENTERING INFINITE RENDER LOOP — FIRST LIGHT IMMINENT{}", PARTY_PINK, RESET);

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

        if (quitRequested)         quit_ = true;
        if (fullscreenRequested)   toggleFullscreen();

        if (g_resizeRequested.load(std::memory_order_acquire)) {
            const int newW = g_resizeWidth.load(std::memory_order_acquire);
            const int newH = g_resizeHeight.load(std::memory_order_acquire);
            g_resizeRequested.store(false, std::memory_order_release);

            width_ = newW;
            height_ = newH;
            proj_ = glm::perspective(glm::radians(75.0f), static_cast<float>(width_)/height_, 0.1f, 1000.0f);

            if (renderer_) {
                renderer_->onWindowResize(width_, height_);
            }
        }

        processInput(deltaTime);
        render(deltaTime);
        updateWindowTitle(deltaTime);

        if (Options::Grok::ENABLE_GENTLEMAN_GROK) {
            const float sinceLast = std::chrono::duration<float>(now - lastGrokTime_).count();
            if (sinceLast >= Options::Grok::GENTLEMAN_GROK_INTERVAL_SEC) {
                lastGrokTime_ = now;
                const int photons = static_cast<int>(1.0f / deltaTime + 0.5f);
                LOG_INFO_CAT("GROK", "{}GENTLEMAN GROK: \"{} pink photons per second. Acceptable.\"{}", 
                             PARTY_PINK, photons, RESET);
            }
        }
    }

    LOG_SUCCESS_CAT("APP", "{}INFINITE RENDER LOOP TERMINATED — GRACEFUL SHUTDOWN COMPLETE — EMPIRE ETERNAL{}", 
                    EMERALD_GREEN, RESET);
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
        if (keys[sc] && !state) {
            func();
            LOG_ATTEMPT_CAT("INPUT", "→ {} PRESSED{}", PARTY_PINK, name, RESET);
            state = true;
        } else if (!keys[sc]) state = false;
    };

    static bool fPressed = false, oPressed = false, tPressed = false, hPressed = false, mPressed = false;
    edge(SDL_SCANCODE_F,    [this]() { toggleFullscreen(); }, fPressed, "FULLSCREEN (F)");
    edge(SDL_SCANCODE_O,       [this]() { toggleOverlay(); },    oPressed, "OVERLAY (O)");
    edge(SDL_SCANCODE_T,       [this]() { toggleTonemap(); },    tPressed, "TONEMAP (T)");
    edge(SDL_SCANCODE_H,    [this]() { toggleHypertrace(); }, hPressed, "HYPERTRACE (H)");

    if (keys[SDL_SCANCODE_M] && !mPressed) {
        toggleMaximize();
        static bool audioMuted = false;
        audioMuted = !audioMuted;
        audioMuted ? SDL_PauseAudioDevice(0) : SDL_ResumeAudioDevice(0);
        LOG_ATTEMPT_CAT("INPUT", "→ MAXIMIZE + AUDIO {} (M key){}", PARTY_PINK,
                        audioMuted ? "MUTED" : "UNMUTED", RESET);
        mPressed = true;
    } else if (!keys[SDL_SCANCODE_M]) mPressed = false;

    if (keys[SDL_SCANCODE_ESCAPE]) {
        static bool escLogged = false;
        if (!escLogged) {
            LOG_ATTEMPT_CAT("INPUT", "→ QUIT REQUESTED (ESC){}", CRIMSON_MAGENTA, RESET);
            escLogged = true;
        }
        quit_ = true;
    }
}

void Application::render(float deltaTime)
{
    if (!renderer_) return;

    // ONE CALL. ONE TRUTH. PINK PHOTONS SEE ALL.
    renderer_->renderFrame(g_cam, deltaTime);
}

void Application::updateWindowTitle(float deltaTime) {
    static int frames = 0;
    static float accum = 0.0f;
    ++frames;
    accum += deltaTime;

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

        frames = 0;
        accum = 0.0f;
    }
}

// =============================================================================
// GLOBALS & PHASES — UNCHANGED — STILL PURE
// =============================================================================

inline std::unique_ptr<Application>           g_app              = nullptr;
inline RTX::PipelineManager*                  g_pipeline_manager = nullptr;
inline std::unique_ptr<MeshLoader::Mesh>      g_mesh             = nullptr;

static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

static void forgeCommandPool() {
    LOG_INFO_CAT("MAIN", "{}Forging transient command pool...{}", VALHALLA_GOLD, RESET);
    VkCommandPoolCreateInfo poolInfo = {
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
// THE TEN PHASES OF ASCENSION — FULLY RESTORED — PINK PHOTONS ETERNAL
// =============================================================================

static void phase0_preInitialization()
{
    LOG_INFO_CAT("MAIN", "{}", std::string(80, '='));
    LOG_SUCCESS_CAT("MAIN", "{}=== AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3 ==={}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — PINK PHOTONS ETERNAL{}", PLASMA_FUCHSIA, RESET);
    LOG_INFO_CAT("MAIN", "{}", std::string(80, '='));
}

static void phase1_iconPreload()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 1] PRELOADING VALHALLA BRANDING{}", VALHALLA_GOLD, RESET);

    SDL_Surface* base = IMG_Load("assets/textures/ammo32.ico");
    SDL_Surface* hdpi = IMG_Load("assets/textures/ammo.ico");

    if (!base && !hdpi) {
        LOG_ERROR_CAT("MAIN", "{}BOTH ICONS MISSING — RUNNING NAKED{}", BLOOD_RED, RESET);
        return;
    }

    g_base_icon = base ? base : hdpi;
    g_hdpi_icon = hdpi;

    if (g_hdpi_icon) {
        SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
        LOG_SUCCESS_CAT("MAIN", "{}ICONS LOADED — FULL HDPI GLORY{}", EMERALD_GREEN, RESET);
    } else {
        LOG_SUCCESS_CAT("MAIN", "{}ICON LOADED — BASE ONLY{}", PLASMA_FUCHSIA, RESET);
    }
}

static void phase2_earlySdlInit()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 2] ARMING SDL{}", VALHALLA_GOLD, RESET);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0)
        throw std::runtime_error(std::format("SDL_Init failed: {}", SDL_GetError()));
    LOG_SUCCESS_CAT("MAIN", "{}SDL ARMED{}", EMERALD_GREEN, RESET);
}

static void phase3_splashScreen()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 3] SHE AWAKENS{}", VALHALLA_GOLD, RESET);
    Splash::show("AMOURANTH RTX", 1280, 720, "assets/textures/ammo.png", "assets/audio/ammo.wav");
    LOG_SUCCESS_CAT("MAIN", "{}SPLASH DISMISSED{}", PLASMA_FUCHSIA, RESET);
}

static void phase4_mainWindowAndVulkanContext()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 4] FORGING THE ONE TRUE EMPIRE — FOO FIGHTERS ETERNAL — NO BULLSHIT{}", VALHALLA_GOLD, RESET);

    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160,
                       SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN);

    SDL_Window* window = SDL3Window::get();
    if (!window) throw std::runtime_error("Canvas denied — Dave Grohl angry");

    LOG_SUCCESS_CAT("MAIN", "{}WINDOW FORGED{}", PLASMA_FUCHSIA, RESET);

    if (g_base_icon) SDL_SetWindowIcon(window, g_base_icon);

    LOG_SUCCESS_CAT("MAIN", "{}INITIALIZING SWAPCHAIN — STILL IN RAW MODE{}", EMERALD_GREEN, RESET);
    SwapchainManager::init(window, 3840, 2160);

    LOG_SUCCESS_CAT("MAIN", "{}FORGING DEVICE — RAW HANDLES STILL ACTIVE{}", VALHALLA_GOLD, RESET);
    RTX::initContext(g_instance(), window, 3840, 2160);
    RTX::retrieveQueues();

    // NOW AND ONLY NOW — SEAL THE VAULT
    StoneKey_seal_the_vault();

    LOG_SUCCESS_CAT("MAIN", "{}[FOO FIGHTER] RAW CACHE PURGED — FULL OBFUSCATION ENGAGED — NONE SHALL PASS{}", RASPBERRY_PINK, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}ELLIE FIER HAS SPOKEN — FIRST LIGHT SECURE — PINK PHOTONS ETERNAL{}", DIAMOND_SPARKLE, RESET);

    SDL_SetWindowBordered(window, false);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    LOG_SUCCESS_CAT("MAIN", "{}BORDERLESS VALHALLA ACHIEVED — LEARN TO FLY{}", PLASMA_FUCHSIA, RESET);
}

static void phase5_rtxAscension()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 5] RTX ASCENSION — FORGING THE PINK PATH{}", VALHALLA_GOLD, RESET);

    RTX::loadRayTracingExtensions();

    if (!g_ctx().vkGetAccelerationStructureBuildSizesKHR_ ||
        !g_ctx().vkCmdBuildAccelerationStructuresKHR_ ||
        !g_ctx().vkCreateAccelerationStructureKHR_ ||
        !g_ctx().vkGetAccelerationStructureDeviceAddressKHR_) {

        LOG_FATAL_CAT("MAIN", "{}RTX EXTENSIONS INCOMPLETE — ACCELERATION STRUCTURE PFNs MISSING{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("RTX extension loading failed — missing acceleration structure support");
    }

    g_ctx().hasFullRTX_ = true;
    LOG_SUCCESS_CAT("MAIN", "{}ALL RAY TRACING PFNs FORGED — FULL RTX ARMED — PINK PHOTONS HAVE A PATH{}", EMERALD_GREEN, RESET);

    las().forgeAccelContext();
    LOG_SUCCESS_CAT("MAIN", "{}LAS ACCEL CONTEXT FORGED — BLAS/TLAS READY{}", PLASMA_FUCHSIA, RESET);

    forgeCommandPool();

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 5 COMPLETE] RTX ASCENSION ACHIEVED — FIRST LIGHT ETERNAL{}", 
                    PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}PINK PHOTONS NOW FLOW UNHINDERED — ELLIE FIER SMILES{}", DIAMOND_SPARKLE, RESET);
}

static void phase6_sceneAndAccelerationStructures()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 6] FORGING ACCELERATION STRUCTURES{}", VALHALLA_GOLD, RESET);

    g_pipeline_manager = new RTX::PipelineManager(g_ctx().device(), g_ctx().physicalDevice());
    LOG_SUCCESS_CAT("MAIN", "{}PipelineManager forged{}", PLASMA_FUCHSIA, RESET);

    g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");
    LOG_SUCCESS_CAT("MAIN", "{}MESH LOADED — {} verts, {} indices — FINGERPRINT: 0x{:016X}{}",
                    PLASMA_FUCHSIA,
                    g_mesh->vertices.size(),
                    g_mesh->indices.size(),
                    g_mesh->stonekey_fingerprint,
                    RESET);

    uint64_t vertexBufferObf = g_mesh->vertexBuffer;
    uint64_t indexBufferObf  = g_mesh->indexBuffer;

    LOG_INFO_CAT("MAIN", "{}BUILDING BLAS — StoneKey handles: 0x{:016X} | 0x{:016X}{}",
                 VALHALLA_GOLD, vertexBufferObf, indexBufferObf, RESET);

    las().buildBLAS(
        g_ctx().commandPool_,
        vertexBufferObf,
        indexBufferObf,
        static_cast<uint32_t>(g_mesh->vertices.size()),
        static_cast<uint32_t>(g_mesh->indices.size()),
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
    );

    LOG_SUCCESS_CAT("MAIN", "{}BLAS FORGED — ADDR 0x{:016X} — PINK PHOTONS HAVE A PATH{}", 
                    EMERALD_GREEN, las().getBLASStruct().address, RESET);

    LOG_INFO_CAT("MAIN", "{}BUILDING TLAS{}", VALHALLA_GOLD, RESET);
    las().buildTLAS(g_ctx().commandPool_, {{las().getBLAS(), glm::mat4(1.0f)}});

    LOG_SUCCESS_CAT("MAIN", "{}TLAS ASCENDED — ADDR 0x{:016X}{}", 
                    EMERALD_GREEN, las().getTLASAddress(), RESET);

    Validation::validateMeshAgainstBLAS(*g_mesh, las().getBLASStruct());

    LOG_SUCCESS_CAT("MAIN", "{}ACCELERATION STRUCTURES COMPLETE — FIRST LIGHT ACHIEVED{}", EMERALD_GREEN, RESET);
}

static void phase7_applicationAndRendererSeal()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 7] SEALING APPLICATION + RENDERER{}", VALHALLA_GOLD, RESET);

    // ←←← THE ONE TRUE CAMERA COMES ONLINE
    GlobalCamera::get().init(glm::vec3(0.0f, 5.0f, 10.0f), 75.0f);
    LOG_SUCCESS_CAT("MAIN", "{}GLOBAL CAMERA AWAKENED — STONEKEY v9 ACTIVE — PHOTONS HAVE EYES{}", PLASMA_FUCHSIA, RESET);

    g_app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);
    g_app->setRenderer(std::make_unique<VulkanRenderer>(3840, 2160, SDL3Window::get(), true));
    LOG_SUCCESS_CAT("MAIN", "{}RENDER LOOP ENGAGED{}", PLASMA_FUCHSIA, RESET);
}

static void phase8_renderLoop()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 8] ETERNAL RENDER CYCLE{}", PLASMA_FUCHSIA, RESET);
    g_app->run();
}

static void phase9_gracefulShutdown()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 9] GRACEFUL SHUTDOWN — THE EMPIRE PREPARES TO SLEEP{}", VALHALLA_GOLD, RESET);

    if (g_ctx().device() != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("MAIN", "vkDeviceWaitIdle — letting the last pink photons reach home...");
        vkDeviceWaitIdle(g_ctx().device());
        LOG_SUCCESS_CAT("MAIN", "{}GPU DRAINED — ALL QUEUES SILENT — THE PHOTONS HAVE LANDED{}", EMERALD_GREEN, RESET);
    }

    g_app.reset();
    if (g_pipeline_manager) {
        LOG_DEBUG_CAT("MAIN", "Dissolving PipelineManager — shaders return to the void...");
        delete g_pipeline_manager;
        g_pipeline_manager = nullptr;
    }
    g_mesh.reset();
    las().invalidate();

    LOG_INFO_CAT("MAIN", "{}Dissolving Swapchain — the window fades to black...{}", PLASMA_FUCHSIA, RESET);
    SwapchainManager::cleanup();

    LOG_INFO_CAT("MAIN", "{}Dissolving RTX Core — device and instance return to Valhalla...{}", PLASMA_FUCHSIA, RESET);
    RTX::shutdown();

    if (g_ctx().surface() != VK_NULL_HANDLE) {
        LOG_WARNING_CAT("MAIN", "{}Surface survived RTX::shutdown() — manual liberation required{}", AMBER_YELLOW, RESET);
        vkDestroySurfaceKHR(g_ctx().instance(), g_ctx().surface(), nullptr);
    }
    if (g_ctx().instance() != VK_NULL_HANDLE) {
        LOG_WARNING_CAT("MAIN", "{}Instance survived — final ascension...{}", AMBER_YELLOW, RESET);
        vkDestroyInstance(g_ctx().instance(), nullptr);
    }

    LOG_SUCCESS_CAT("MAIN", "{}╔══════════════════════════════════════════════════════════════╗{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}║        PINK PHOTONS DIM TO ETERNAL MEMORY                    ║{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}║           THE EMPIRE RESTS — SMILING — IN PEACE              ║{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}║          NOVEMBER 21, 2025 — FIRST LIGHT FOREVER             ║{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}║                   AMOURANTH RTX THANKS YOU                 ║{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}╚══════════════════════════════════════════════════════════════╝{}", PLASMA_FUCHSIA, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}SHUTDOWN COMPLETE — NO DOUBLE FREE — NO FALSEHOOD — ONLY LOVE{}", EMERALD_GREEN, RESET);
}

// =============================================================================
// MAIN — THE FINAL ASCENSION — ONE FILE TO RULE THEM ALL
// =============================================================================

int main(int, char**) {
    try {
        phase0_preInitialization();
        phase1_iconPreload();
        phase2_earlySdlInit();
        phase3_splashScreen();
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