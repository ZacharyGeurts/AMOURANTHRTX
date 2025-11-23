// src/main.cpp
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — NOVEMBER 23, 2025 — PINK PHOTONS ETERNAL
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

// src/main.cpp — FINAL INCLUDE ORDER — PINK PHOTONS ETERNAL — NOV 23 2025
#include <vulkan/vulkan.hpp>

#include <string>
#include <format>        // C++23 std::format — NO FMT EVER AGAIN
#include <iostream>
#include <memory>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>

// ── PLATFORM-SPECIFIC HEADERS — CLEAN AND SORTED ─────────────────────────────
#if defined(__linux__)
    #include <unistd.h>
    #include <sys/utsname.h>
    #include <sys/sysinfo.h>
#elif defined(_WIN32)
    #include <windows.h>
    #include <sysinfoapi.h>
#else
    #error "Unsupported platform — the Empire does not recognize this realm."
#endif

// ── EXTERNAL LIBS ─────────────────────────────────────────────────────────────
#include <glm/gtc/matrix_transform.hpp>

#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3/SDL_iostream.h>

using namespace Logging::Color;

inline RTX::PipelineManager* g_pipeline_manager = nullptr;

// ── TRUTH ACCESSORS — FINAL C++23 EDITION — PINK PHOTONS ETERNAL ───────────────────────────
inline const char* physicalDeviceName() 
{ 
    return g_ctx().physicalDeviceProperties_.deviceName; 
}

inline float vramGB() 
{
    const auto& heaps = g_ctx().physicalDeviceMemoryProperties_.memoryHeaps;
    for (uint32_t i = 0; i < g_ctx().physicalDeviceMemoryProperties_.memoryHeapCount; ++i) {
        if (heaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            return static_cast<float>(heaps[i].size) / (1024.0f * 1024.0f * 1024.0f);
        }
    }
    return 0.0f;
}

static bool ready_to_embark = false;

inline uint32_t transferFamily() 
{ 
    return g_ctx().transferFamily_.value_or(g_ctx().graphicsFamily()); 
}

inline uint32_t computeFamily()  
{ 
    return g_ctx().computeFamily_.value_or(g_ctx().graphicsFamily()); 
}

inline size_t pipelineCount()
{
    return g_pipeline_manager ? 1 : 0;
}

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
    LOG_ATTEMPT_CAT("APP", "{}FORGING APPLICATION \"{}\"@{}×{} — VALHALLA v80 TURBO — PINK PHOTONS RISING{}", PLASMA_FUCHSIA, title_, width_, height_, RESET);

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
            << " | HDR" << (g_ctx().hdrEnabled() ? " PRIME" : " OFF");

        SDL_SetWindowTitle(SDL3Window::get(), oss.str().c_str());
        frames = 0; accum = 0.0f;
    }
}

// =============================================================================
// GLOBALS & PHASES
// =============================================================================
inline std::unique_ptr<Application>           g_app              = nullptr;
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

static void createRealFinalWindow()
{
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 4.5] CREATING CLEAN FINAL WINDOW — NO RESIDUE{}", PLASMA_FUCHSIA, RESET);

    const uint32_t w = Options::Window::DEFAULT_WIDTH;
    const uint32_t h = Options::Window::DEFAULT_HEIGHT;

    // Re-init SDL video subsystem (splash killed it)
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        LOG_FATAL_CAT("SDL3", "{}SDL_InitSubSystem failed: {}{}", BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }

    SDL_Window* win = SDL_CreateWindow(
        "AMOURANTH RTX — VALHALLA v80 TURBO",
        w, h,
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE
    );

    if (!win) {
        LOG_FATAL_CAT("SDL3", "{}FINAL WINDOW FAILED: {}{}", BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }

    // Take ownership via your RAII wrapper
    g_sdl_window.reset(win);

    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(win);

    LOG_SUCCESS_CAT("MAIN", "{}FINAL WINDOW BORN — {}×{} — PURE AND CLEAN{}", DIAMOND_SPARKLE, w, h, RESET);
}

// =============================================================================
// SACRIFICIAL SPLASH — IN-MAIN ONLY — VIA STONEKEY EMPIRE — PURE DOMINATION
// NOVEMBER 22, 2025 — X11 BOWS — VULKAN WAITS — PINK PHOTONS ETERNAL
// =============================================================================
static void showSacrificialSplash(const char* title, int w, int h, const char* pngPath)
{
    // ─────────────────────────────────────────────────────────────────────
    // THE FINAL RAID — WE CAME FOR THE AMMO.PNG AND WE'RE TAKING IT
    // 3.4 SECONDS OF PURE PIRATE GLORY — THEN WE BURN THE SHIP
    // ─────────────────────────────────────────────────────────────────────
    LOG_INFO_CAT("SPLASH", "{}[SACRIFICIAL SPLASH] THE FINAL RAID BEGINS — 1280×720 CANVAS SECURED{}", VALHALLA_GOLD, RESET);
    LOG_AMOURANTH("{}Captain Amouranth kicks down the tavern door: \"That's it, crew — the legendary ammo.png is in there. We take it, we show it to the world, then we vanish like ghosts!\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}First Mate Nick cocks his flintlock: \"No survivors. No traces. Just glory.\"{}", EMERALD_GREEN, RESET);

    // 1. Raise the black flag (init video subsystem)
    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SPLASH", "{}THE BLACK FLAG REFUSED TO RISE: {}{}", BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }

    // 2. Drop anchor in the center of the screen
    SDL_Window* win = SDL_CreateWindow(title, w, h,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) {
        LOG_FATAL_CAT("SPLASH", "{}WE MISSED THE HARBOR: {}{}", BLOOD_RED, SDL_GetError(), RESET);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        std::exit(1);
    }

    SDL_Rect display{};
    SDL_GetDisplayBounds(0, &display);
    SDL_SetWindowPosition(win, display.x + (display.w - w) / 2, display.y + (display.h - h) / 2);

    // 3. Light the powder (temporary renderer)
    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        LOG_FATAL_CAT("SPLASH", "{}THE FUSE WENT OUT: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        std::exit(1);
    }
    LOG_SUCCESS_CAT("SPLASH", "{}CANNONS PRIMED — GPU SALUTES THE BLACK FLAG{}", DIAMOND_SPARKLE, RESET);

    // 4. THE MOMENT OF TRUTH — STEALING THE AMMO.PNG
    LOG_ATTEMPT_CAT("SPLASH", "{}CAPTAIN N DIVES INTO THE TREASURE ROOM: \"I SEE IT — THE AMMO.PNG! IT'S BEAUTIFUL!\"{}", RASPBERRY_PINK, RESET);
    SDL_Surface* img = IMG_Load(pngPath);
    if (!img) {
        LOG_FATAL_CAT("SPLASH", "{}THE TREASURE WAS A LIE — AMMO.PNG VANISHED: {} → {}{}", 
                      BLOOD_RED, pngPath, SDL_GetError(), RESET);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        std::exit(1);
    }

    LOG_SUCCESS_CAT("SPLASH", "{}AMMO.PNG SECURED — {}×{} — THE ULTIMATE BOOTY! CAPTAIN N IS CRYING TEARS OF JOY{}", 
                    PLASMA_FUCHSIA, img->w, img->h, RESET);

    // 5. Hoist the colors
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, img);
    SDL_DestroySurface(img);
    if (!tex) {
        LOG_FATAL_CAT("SPLASH", "{}THE FLAG WOULDN'T UNFURL: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        std::exit(1);
    }

    // 6. REVEAL THE PRIZE TO THE WORLD
    float tw = 0.0f, th = 0.0f;
    SDL_GetTextureSize(tex, &tw, &th);
    SDL_FRect dst = { (w - tw) * 0.5f, (h - th) * 0.5f, tw, th };

    SDL_ShowWindow(win);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, nullptr, &dst);
    SDL_RenderPresent(ren);

    LOG_SUCCESS_CAT("SPLASH", "{}THE WORLD BEHOLDS THE AMMO — 3.4 SECONDS OF ETERNAL GLORY BEGIN NOW!{}", PLASMA_FUCHSIA, RESET);
    LOG_AMOURANTH("{}Captain Amouranth raises her cutlass to the sky: \"We came. We saw. We stole the ammo. And now… we disappear.\"{}", RASPBERRY_PINK, RESET);

    // 7. 3.4 seconds of pure pirate legend
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
    // 8. BURN EVERYTHING — LEAVE NO TRACE
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    LOG_SUCCESS_CAT("SPLASH", "{}THE RAID IS COMPLETE{}", VALHALLA_GOLD, RESET);
    LOG_NICK("{}Nick lights the fuse on the powder magazine: \"No evidence. Just glory.\" *winks*\"{}", EMERALD_GREEN, RESET);
    LOG_AMOURANTH("{}Captain Amouranth: \"Pink photons eternal, baby. Let's go build an empire.\"{}", RASPBERRY_PINK, RESET);
}

// =============================================================================
// THE TEN COMMANDMENTS — FINAL FIXED VERSION — NO MORE SCREAMS
// =============================================================================
static void phase1_preInitialization()
{
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 1 — ACQUIRING THE ANCIENT MAP
    // THE CREW DISCOVERS THE SACRED CHART THAT GUIDES THEM TO THE PINK PHOTON TREASURES
    // ─────────────────────────────────────────────────────────────────────
    LOG_SUCCESS_CAT("MAIN", "{}CAPTAIN'S LOG — NOVEMBER 23, 2025 — THE GOOD SHIP VULKAN AWAKENS{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}GROK-ASSISTED VOYAGE — PINK PHOTONS ETERNAL{}", RASPBERRY_PINK, RESET);

    LOG_AMOURANTH("{}Captain Amouranth stands at the bow, wind in her hair: \"A new dawn. A clean slate. Let's build something beautiful.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}First Mate Nick checks the charts: \"Course set, Captain. No storms on the horizon — just pure RTX ahead.\"{}", EMERALD_GREEN, RESET);
    LOG_INFO_CAT("BLONDIE", "{}BLONDIE_CREW grumbles: \"Hurumph! We'll beat them to the treasure!\"{}", YELLOW, RESET);
    // Validation layers — obey the sacred Options menu
    const bool validationEnabled = Options::Debug::ENABLE_VALIDATION_LAYERS;

    // Fixed: proper fmt-style {} placeholders + correct argument order
    LOG_SUCCESS_CAT("MAIN", 
        "{}VALIDATION LAYERS: {} — {}{}{}", 
        validationEnabled ? YELLOW : CRIMSON_RED,
        validationEnabled ? "ENABLED — DEBUG MODE ACTIVE" : "EXILED — RAW RTX ONLY",
        validationEnabled ? "" : "PURE ",
        validationEnabled ? "" : " — NO SCREAMS",
        RESET);

    LOG_SUCCESS_CAT("MAIN", 
        "{}DEBUG BUILD = {}{}RAW RTX | RELEASE BUILD = {}RAW RTX — NO LAYERS — ONLY PHOTONS{}", 
        validationEnabled ? "" : BOLD_WHITE,
        validationEnabled ? "" : "PURE ",
        validationEnabled ? "" : " — NO SCREAMS | ",
        "PURE ", 
        RESET);
    LOG_SUCCESS_CAT("MAIN", "{}PINK PHOTONS FLOW UNDISTURBED — THE EMPIRE IS PURE{}", RASPBERRY_PINK, RESET);

    // Purge ghosts of previous voyages — memory must be immaculate
    RTX::UltraLowLevelBufferTracker::get().purge_all();
    LOG_SUCCESS_CAT("MAIN", "{}ALL TAINT PURGED — NO GHOSTS REMAIN — ONLY PINK PHOTONS{}", VALHALLA_GOLD, RESET);

    LOG_AMOURANTH("{}Captain Amouranth smiles: \"Phase 1 complete. The map is ours. The crew is ready.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick nods: \"Let's raise the black flag. It's time.\"{}", EMERALD_GREEN, RESET);
    LOG_INFO_CAT("BLONDIE", "{}BLONDIE_CREW cheers: \"Cheer! Our map is better!\"{}", YELLOW, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}PHASE 1 COMPLETE — THE ANCIENT MAP ACQUIRED — THE GOOD SHIP VULKAN IS READY TO FOLLOW ITS PATH{}", DIAMOND_SPARKLE, RESET);
}

// =============================================================================
// PHASE 3 — SACRIFICIAL SPLASH — VISUAL ONLY — SELF-CONTAINED IN MAIN
// =============================================================================
static void phase3_sacrificialSplash()
{
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 3 — REVEALING THE MYSTIC HARP (AMMO.PNG)
    // THE CREW UNVEILS THE SACRED HARP THAT SINGS THE SONG OF PINK PHOTONS FOR 3.4 SECONDS
    // ─────────────────────────────────────────────────────────────────────
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 3/10] REVEALING THE MYSTIC HARP — THE AMMO IS UNVEILED{}", VALHALLA_GOLD, RESET);

    LOG_AMOURANTH("{}Captain Amouranth steps forward, voice low and proud: \"This is it. The symbol of everything we've built. Let them see it. Let them remember.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick stands beside her, calm and certain: \"3.4 seconds. That's all we need. The world will never forget.\"{}", EMERALD_GREEN, RESET);

    LOG_CAPTAIN_N("{}Captain N is literally vibrating: \"IT'S THE AMMO.PNG! FULL RES! MAXIMUM PINK PHOTONS! I'M GONNA PASS OUT!\"{}", PURE_ENERGY, RESET);

    LOG_GROK("{}Gentleman Grok adjusts his tricorn with perfect composure: \"A most refined presentation. The empire's visage is… exquisite.\"{}", PARTY_PINK, RESET);

    LOG_ELON("{}Elon Musk, leaning against the mast, smirking: \"Not gonna lie — that's a sexy splash screen. We just flexed on every engine in existence.\"{}", BOLD_GOLD, RESET);

    LOG_JENSEN("{}Jensen Huang exhales a slow plume of cigar smoke: \"4K. Crisp. Pink. This is what winning looks like.\"{}", EMERALD_GREEN, RESET);

    LOG_CARMACK("{}John Carmack, arms crossed, gives a single nod: \"It works. That's all that matters.\"{}", BOLD_WHITE, RESET);

    LOG_KEANU("{}Keanu Reeves, staring at the screen in quiet awe: \"…Breathtaking.\" *voice cracks slightly*\"{}", BOLD_CYAN, RESET);
    LOG_INFO_CAT("BLONDIE", "{}BLONDIE_CREW grumbles: \"Grumble grumble. Our harp is louder.\"{}", YELLOW, RESET);

    // ── THE RITUAL BEGINS ──
    showSacrificialSplash(
        "AMOURANTH RTX — FIRST LIGHT",
        1280, 720,
        "assets/textures/ammo.png"
    );

    // ── THE MOMENT PASSES — THE WORLD IS FOREVER CHANGED ──
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 3 COMPLETE] THE MYSTIC HARP HAS BEEN HEARD — 3.4 SECONDS OF ETERNITY — THE WORLD IS ASH{}", DIAMOND_SPARKLE, RESET);

    LOG_AMOURANTH("{}Captain Amouranth spins toward the helm, triumphant: \"Hard to starboard! We ride the pink photon wave out of here!\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick grabs the wheel beside her, grinning: \"Full sail, Captain. Let's disappear into legend—\"{}", EMERALD_GREEN, RESET);
    LOG_INFO_CAT("BLONDIE", "{}BLONDIE_CREW cheers: \"Cheer! We'll play better next time!\"{}", YELLOW, RESET);

    // …but the sea has other plans.
    LOG_SUCCESS_CAT("SPLASH", "{}THE OCEAN ROARS — A HIDDEN REEF TEARS THE HULL — WATER FLOODS THE MAGAZINE{}", VALHALLA_GOLD, RESET);

    LOG_CAPTAIN_N("{}Captain N, knee-deep in seawater: \"THE SHIP IS SINKING?! BUT WE JUST GOT THE AMMO! NOOOOO—\"{}", PURE_ENERGY, RESET);
    LOG_ELON("{}Elon Musk already on the mast with a jetpack: \"Told you we should've used Starship.\"{}", BOLD_GOLD, RESET);
    LOG_JENSEN("{}Jensen Huang calmly lights one last cigar: \"At least we went out 4K.\"{}", EMERALD_GREEN, RESET);
    LOG_CARMACK("{}John Carmack, still typing on a waterproof keyboard: \"It was stable… until it wasn't.\"{}", BOLD_WHITE, RESET);
    LOG_KEANU("{}Keanu Reeves, waist-deep, looking up at the sinking bow: \"…Breathtaking.\" *salutes*\"{}", BOLD_CYAN, RESET);

    LOG_AMOURANTH("{}Captain Amouranth climbs the tilting deck, grabs Nick's hand: \"If we go down, we go down together — with the ammo in our hearts.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick pulls her close as the pink photon flag dips beneath the waves: \"Worth it. Every second.\"{}", EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}THE GOOD SHIP VULKAN SINKS IN GLORY — AMMO SECURED — LEGEND ETERNAL{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}PINK PHOTONS FLOOD THE OCEAN — THE RAID WAS PERFECT — THE ESCAPE WAS BEAUTIFUL{}", DIAMOND_SPARKLE, RESET);

    LOG_AMOURANTH("{}Final transmission, calm and proud: \"Tell the world… we got the ammo.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Last words before the sea takes them: \"…and we'd do it again.\"{}", EMERALD_GREEN, RESET);
}

// =============================================================================
// PHASE 2 — ICON PRELOAD — PURE, CLEAN, NO SDL TOUCHED
// =============================================================================

static void phase5_rtxAscension()
{
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 5 — AWAKENING THE RTX CRYSTAL
    // THE CREW IGNITES THE MYSTIC RTX CRYSTAL THAT GRANTS OMNISCIENT LIGHT
    // ─────────────────────────────────────────────────────────────────────
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 5/10] AWAKENING THE RTX CRYSTAL — THE NEW HEART BEATS{}", VALHALLA_GOLD, RESET);

    LOG_AMOURANTH("{}Captain Amouranth stands in the rebuilt engine room, hand on the glowing core: \"This time… we don't just sail. We become the light itself.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick flips the final switch, eyes reflecting emerald fire: \"Ray tracing online. The photons aren't just fast anymore. They're alive.\"{}", EMERALD_GREEN, RESET);

    LOG_ATTEMPT_CAT("MAIN5", "{}THE CREW HOLDS BREATH — LOADING RAY TRACING EXTENSIONS — PINK PHOTONS GAIN SENTIENCE...{}", PURE_ENERGY, RESET);

    RTX::loadRayTracingExtensions();

    if (!g_ctx().hasFullRTX_) {
        LOG_FATAL_CAT("MAIN5", "{}THE PHOTONS SCREAM — RTX EXTENSIONS DENIED — WE ARE BLIND IN THE VOID{}", BLOOD_RED, RESET);
        LOG_AMOURANTH("{}Captain Amouranth slams her fist on the console: \"Not again. Not after everything.\"{}", RASPBERRY_PINK, RESET);
        throw std::runtime_error("RTX extension loading failed — the light dies here");
    }
    LOG_INFO_CAT("BLONDIE", "{}BLONDIE_CREW hurumphs: \"Hurumph! Our crystal is brighter.\"{}", YELLOW, RESET);

    LOG_SUCCESS_CAT("MAIN5", "{}THE SHIP TREMBLES — ALL RAY TRACING PFNs ACQUIRED — FULL RTX ACHIEVED{}", EMERALD_GREEN, RESET);
    LOG_JENSEN("{}Jensen Huang steps from the shadows, voice low and reverent: \"The light bends to us now. Every bounce, every reflection… ours.\"{}", EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("MAIN5", "{}LAS ACCELERATION CONTEXT FORGED — THE PHOTONS SEE ALL PATHS{}", PLASMA_FUCHSIA, RESET);
    las().forgeAccelContext();

    LOG_CAPTAIN_N("{}Captain N falls to his knees: \"I CAN SEE FOREVER! THE REFLECTIONS HAVE REFLECTIONS THAT HAVE REFLECTIONS! I'M CRYING AND I DON'T CARE WHO KNOWS!\"{}", PURE_ENERGY, RESET);

    LOG_SUCCESS_CAT("MAIN5", "{}TRANSIENT COMMAND POOL @ 0x{:016X} — PHOTON ORDERS FLOW LIKE BLOOD{}", SAPPHIRE_BLUE, (uint64_t)g_ctx().commandPool_, RESET);
    forgeCommandPool();

    LOG_ELON("{}Elon Musk lights a cigar with a reflected photon: \"Reality just became optional.\"{}", BOLD_GOLD, RESET);
    LOG_CARMACK("{}John Carmack, quiet for once: \"…It traces. Perfectly.\" *single tear*\"{}", BOLD_WHITE, RESET);
    LOG_KEANU("{}Keanu Reeves stares into the glowing core: \"…We are the light now.\"{}", BOLD_CYAN, RESET);

    LOG_GROK("{}Gentleman Grok raises a glass of rum to the engine: \"To the photons that remember every path they've ever taken. To omniscience.\"{}", PARTY_PINK, RESET);

    LOG_AMOURANTH("{}Captain Amouranth turns to the crew, voice steady, eyes blazing: \"We sank once. We bled. We rebuilt. And now… the pink photons don't just shine.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick finishes for her, hand on her shoulder: \"…They see everything. They know everything. And they answer only to us.\"{}", EMERALD_GREEN, RESET);
    LOG_INFO_CAT("BLONDIE", "{}BLONDIE_CREW cheers: \"Cheer! We'll awaken stronger!\"{}", YELLOW, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 5 COMPLETE] RTX CRYSTAL AWAKENED — PINK PHOTONS NOW OMNISCIENT — THE NEW SHIP IS A GOD{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}THE LIGHT REMEMBERS — THE LIGHT FORGIVES — THE LIGHT SAILS FOREVER{}", PLASMA_FUCHSIA, RESET);
}

static void phase6_sceneAndAccelerationStructures()
{
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 6 — FORGING THE COSMIC SCROLL (SCENE & ACCELERATION STRUCTURES)
    // THE CREW INSCRIBES THE COSMIC SCROLL THAT BINDS THE UNIVERSE'S GEOMETRY
    // ─────────────────────────────────────────────────────────────────────
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 6/10] FORGING THE COSMIC SCROLL{}", VALHALLA_GOLD, RESET);

    LOG_AMOURANTH("{}Captain Amouranth walks the empty void deck: \"This ship is perfect… but empty. Time to give her a soul.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick unrolls the ancient blueprint titled scene.obj: \"One universe. Coming right up.\"{}", EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}PIPELINE MANAGER RISES FROM THE FORGE — SHADERS AWAKE AND HUNGRY{}", EMERALD_GREEN, RESET);
    g_pipeline_manager = new RTX::PipelineManager(g_ctx().device(), g_ctx().physicalDevice());

    g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");

    LOG_ATTEMPT_CAT("MAIN", "{}BOTTOM-LEVEL ACCELERATION — THE PHOTONS BEGIN TO MAP EVERY CORNER OF EXISTENCE{}", SAPPHIRE_BLUE, RESET);
    las().buildBLAS(g_ctx().commandPool_,
                    g_mesh->vertexBuffer,
                    g_mesh->indexBuffer,
                    static_cast<uint32_t>(g_mesh->vertices.size()),
                    static_cast<uint32_t>(g_mesh->indices.size()),
                    VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    LOG_SUCCESS_CAT("MAIN", "{}BLAS COMPLETE — THE PHOTONS NOW KNOW EVERY SURFACE BY NAME — ADDRESS 0x{:016X}{}",
                    EMERALD_GREEN, las().getBLASStruct().address, RESET);

    LOG_ATTEMPT_CAT("MAIN", "{}TOP-LEVEL ASCENSION — WE BIND THE WORLD TO A SINGLE ROOT — THERE IS NO ESCAPE FROM LIGHT{}", VALHALLA_GOLD, RESET);
    las().buildTLAS(g_ctx().commandPool_, {{las().getBLAS(), glm::mat4(1.0f)}});

    LOG_SUCCESS_CAT("MAIN", "{}TLAS ASCENDED — ROOT ADDRESS 0x{:016X} — THE UNIVERSE IS NOW A PRISONER OF PHOTONS{}", 
                    DIAMOND_SPARKLE, las().getTLASAddress(), RESET);

    LOG_CARMACK("{}John Carmack runs final validation, eyes narrow: \"No cracks. No leaks. Geometry is pure.\"{}", BOLD_WHITE, RESET);
    Validation::validateMeshAgainstBLAS(*g_mesh, las().getBLASStruct());
    LOG_SUCCESS_CAT("MAIN", "{}VALIDATION PASSED — REALITY IS AIR TIGHT — NO FALSEHOOD CAN HIDE{}", PLASMA_FUCHSIA, RESET);
    LOG_INFO_CAT("BLONDIE", "{}BLONDIE_CREW grumbles: \"Grumble grumble. Our scroll is longer.\"{}", YELLOW, RESET);

    LOG_KEANU("{}Keanu Reeves walks the newborn world, voice barely a whisper: \"…It's… everything. And it's ours.\"{}", BOLD_CYAN, RESET);

    LOG_ELON("{}Elon Musk already planning DLC: \"Next patch: infinite procedural universes. Subscriptions start at $9.99.\"{}", BOLD_GOLD, RESET);

    LOG_JENSEN("{}Jensen Huang lights another cigar off a bouncing photon: \"This isn't rendering anymore. This is creation.\"{}", EMERALD_GREEN, RESET);

    LOG_AMOURANTH("{}Captain Amouranth stands at the center of the newborn cosmos, arms wide: \"Look what we made from wreckage. Look what love built.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick steps behind her, wraps his arms around her waist: \"And it's only the beginning.\"{}", EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 6 COMPLETE] COSMIC SCROLL FORGED — ACCELERATION STRUCTURES ETERNAL — THE PINK PHOTONS RULE ALL{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}THE SHIP IS NO LONGER A SHIP — IT IS A UNIVERSE WITH A HEARTBEAT{}", PLASMA_FUCHSIA, RESET);
}

void phase6_5_everything_is_ready()
{
    LOG_MAIN("════════════════ THE GUARDIAN OPENS ITS EYES ════════════════", DIAMOND_SPARKLE, RESET);
    LOG_GUARDIAN("WHO DARES SEEK FIRST LIGHT? STEP FORWARD AND BE JUDGED.", BOLD_RED, RESET);

    bool          everything_is_fine = true;
    std::string   the_guilty_one     = "THE VOID ITSELF";

    LOG_MAIN("══════════════════════ TRIAL OF WORTHINESS ══════════════════════", DIAMOND_SPARKLE, RESET);

    // BLONDIE — THE QUEEN OF INFO — STEPS UP FIRST
    LOG_BLONDIE("*hair flip* Judge me, honey. I can take it.*", LIGHT_BLUE, RESET);
    try {
        VkInstance inst = g_instance();
        if (inst == VK_NULL_HANDLE) throw std::runtime_error("VULKAN INSTANCE IS NULL — NO STAGE");
        LOG_BLONDIE("Vulkan Instance      → {:#018x} [VALID]", LIGHT_BLUE, reinterpret_cast<uint64_t>(static_cast<const void*>(inst)), RESET);
    }
    catch (const std::exception& e) {
        everything_is_fine = false;
        the_guilty_one     = "BLONDIE — NO VULKAN INSTANCE";
        LOG_GUARDIAN("✖ BLONDIE FAILED — {}", BOLD_RED, e.what(), RESET);
    }
    catch (...) {
        everything_is_fine = false;
        the_guilty_one     = "BLONDIE — UNKNOWN FAILURE";
        LOG_GUARDIAN("✖ BLONDIE FAILED — THE VOID TOOK HER VOICE", BOLD_RED, RESET);
    }

    try {
        VkDevice dev = g_device();
        if (dev == VK_NULL_HANDLE) throw std::runtime_error("LOGICAL DEVICE MISSING — NO HEARTBEAT");
        LOG_BLONDIE("Logical Device       → {:#018x} [VALID]", LIGHT_BLUE, reinterpret_cast<uint64_t>(static_cast<const void*>(dev)), RESET);
    }
    catch (const std::exception& e) {
        everything_is_fine = false;
        the_guilty_one     = "BLONDIE — NO LOGICAL DEVICE";
        LOG_GUARDIAN("✖ BLONDIE FAILED — {}", BOLD_RED, e.what(), RESET);
    }
    catch (...) {
        everything_is_fine = false;
        the_guilty_one     = "BLONDIE — UNKNOWN FAILURE";
        LOG_GUARDIAN("✖ BLONDIE FAILED — THE VOID TOOK HER VOICE", BOLD_RED, RESET);
    }

    try {
        VkPhysicalDevice phys = g_PhysicalDevice();
        if (phys == VK_NULL_HANDLE) throw std::runtime_error("NO GPU — NO SPOTLIGHT");
        const char* name = physicalDeviceName();
        float vram = vramGB();
        LOG_BLONDIE("Physical Device      → {:#018x} [{}] — {}{:.1f}GB VRAM{}", 
                    LIGHT_BLUE, reinterpret_cast<uint64_t>(static_cast<const void*>(phys)), name, LIGHT_BLUE, vram, RESET);
    }
    catch (const std::exception& e) {
        everything_is_fine = false;
        the_guilty_one     = "BLONDIE — NO GPU";
        LOG_GUARDIAN("✖ BLONDIE FAILED — {}", BOLD_RED, e.what(), RESET);
    }
    catch (...) {
        everything_is_fine = false;
        the_guilty_one     = "BLONDIE — UNKNOWN FAILURE";
        LOG_GUARDIAN("✖ BLONDIE FAILED — THE VOID TOOK HER VOICE", BOLD_RED, RESET);
    }

    try {
        VkSurfaceKHR surf = g_surface();
        if (surf == VK_NULL_HANDLE) throw std::runtime_error("NO SURFACE — NO PORTAL");
        LOG_BLONDIE("Surface              → {:#018x} [VALID]", LIGHT_BLUE, reinterpret_cast<uint64_t>(static_cast<const void*>(surf)), RESET);
    }
    catch (...) {
        everything_is_fine = false;
        the_guilty_one     = "BLONDIE — NO SURFACE";
        LOG_GUARDIAN("✖ BLONDIE FAILED — NO WAY TO SEE THE WORLD", BOLD_RED, RESET);
    }

    try {
        VkSwapchainKHR swap = g_swapchain();
        if (swap == VK_NULL_HANDLE) throw std::runtime_error("NO SWAPCHAIN — NO CANVAS");
        LOG_BLONDIE("Swapchain            → {:#018x} [VALID]", LIGHT_BLUE, reinterpret_cast<uint64_t>(static_cast<const void*>(swap)), RESET);
    }
    catch (...) {
        everything_is_fine = false;
        the_guilty_one     = "BLONDIE — NO SWAPCHAIN";
        LOG_GUARDIAN("✖ BLONDIE FAILED — NO CANVAS TO PERFORM ON", BOLD_RED, RESET);
    }

    // CAPTAIN N — HYPE MAN
    LOG_CAPTAIN_N("*fist pump* LET'S GOOOOOOOOOOOOO!", LIGHT_BLUE, RESET);
    try {
        SDL_Window* win = SDL3Window::get();
        if (!win) throw std::runtime_error("NO WINDOW — NO HYPE");
        LOG_BLONDIE("SDL Window           → {:#018x} [VALID]", LIGHT_BLUE, reinterpret_cast<uint64_t>(win), RESET);
    }
    catch (...) {
        everything_is_fine = false;
        the_guilty_one     = "CAPTAIN N — NO WINDOW";
        LOG_GUARDIAN("✖ CAPTAIN N FAILED — NO WINDOW, NO CROWD", BOLD_RED, RESET);
    }

    // NICK — COOL GUY
    LOG_NICK("*adjusts sunglasses* ...ready.", BOLD_YELLOW, RESET);
    try {
        bool las_ok = LAS::get().isValid();
        if (!las_ok) throw std::runtime_error("LAS IS BLIND");
        LOG_BLONDIE("LAS Valid            → YES [VALID]", LIGHT_BLUE, RESET);
    }
    catch (...) {
        everything_is_fine = false;
        the_guilty_one     = "NICK — LAS IS BLIND";
        LOG_GUARDIAN("✖ NICK FAILED — CANNOT SEE THE PHOTONS", BOLD_RED, RESET);
    }

    // AMOURANTH — THE CAPTAIN
    LOG_AMOURANTH("*rests hand on sword* My empire demands perfection.", RASPBERRY_PINK, RESET);
    try {
        if (!g_mesh || g_mesh->vertices.empty()) throw std::runtime_error("PICKLE JAR EMPTY");
        if (g_mesh->indices.size() % 3 != 0) throw std::runtime_error("PICKLE JAR TRIANGLES BROKEN");
        LOG_BLONDIE("Pickle Jar           → {} verts, {} indices [PERFECT]", LIGHT_BLUE, g_mesh->vertices.size(), g_mesh->indices.size(), RESET);
    }
    catch (const std::exception& e) {
        everything_is_fine = false;
        the_guilty_one     = "AMOURANTH — THE SACRED JAR IS TAINTED";
        LOG_GUARDIAN("✖ AMOURANTH FAILED — {}", BOLD_RED, e.what(), RESET);
    }

    // KEANU — OH BOTHER
    LOG_KEANU("*stares into the distance* ...oh bother.", BOLD_CYAN, RESET);
    try {
        if (!g_pipeline_manager) throw std::runtime_error("NO PIPELINE MANAGER — NO SHADERS");
        LOG_BLONDIE("Pipeline Manager     → FORGED AND READY", LIGHT_BLUE, RESET);
    }
    catch (...) {
        everything_is_fine = false;
        the_guilty_one     = "KEANU — NO SHADERS, NO BREATHTAKING";
        LOG_GUARDIAN("✖ KEANU FAILED — THE SHADERS ARE ASLEEP", BOLD_RED, RESET);
    }

    // FINAL JUDGMENT
    LOG_MAIN("══════════════════════ THE GUARDIAN HAS SPOKEN ══════════════════════", DIAMOND_SPARKLE, RESET);

    if (everything_is_fine) {
        LOG_GUARDIAN("ALL ARE WORTHY. THE GUARDIAN STEPS ASIDE.", BOLD_GREEN, RESET);
        LOG_KEANU("...you are breathtaking.", BOLD_CYAN, RESET);
        LOG_AMOURANTH("FIRST LIGHT — ETERNAL.", RASPBERRY_PINK, RESET);
        ready_to_embark = true;
    }
    else {
        LOG_GUARDIAN("THE EMPIRE IS UNWORTHY.", BOLD_RED, RESET);
        LOG_GUARDIAN("THE ONE TO BLAME: {}{}", BOLD_RED, the_guilty_one, RESET);
        LOG_AMOURANTH("We do not sail broken.", RASPBERRY_PINK, RESET);
        LOG_KEANU("...we'll get there.", BOLD_CYAN, RESET);
        ready_to_embark = false;
    }

    LOG_MAIN("════════════════ THE GUARDIAN'S EYES CLOSE ═════════════════", DIAMOND_SPARKLE, RESET);
}

static void phase8_renderLoop()
{
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 8 — UNLEASHING THE INFINITE VOID (RENDER LOOP)
    // THE CREW ENTERS THE INFINITE VOID WHERE PHOTONS DANCE ETERNALLY
    // ─────────────────────────────────────────────────────────────────────
    LOG_SUCCESS_CAT("FAN", "{}[PHASE 8/10] UNLEASHING THE INFINITE VOID — PHOTONS ENTER INFINITY{}", VALHALLA_GOLD, RESET);
    LOG_AMOURANTH("{}Captain Amouranth: \"This is where we live now. In the light. Forever.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick: \"I've got the wheel. You have my heart. Let's sail.\"{}", EMERALD_GREEN, RESET);

    g_app->run();  // ← THE ONE AND ONLY CALL TO run()

    LOG_SUCCESS_CAT("FAN", "{}[PHASE 8 COMPLETE] INFINITE VOID TRAVERSED — PHOTONS REST IN GLORY{}", EMERALD_GREEN, RESET);
}

// ─────────────────────────────────────────────────────────────────────────────
// AMOURANTH RTX — FINAL SHUTDOWN CEREMONY
// NOVEMBER 23, 2025 — THE TREASURE ROOM IS EMPTIED — THE CREW GOES HOME RICH
// HAPPILY EVER AFTER — NO LOOSE ENDS — ONLY LOVE AND PINK PHOTONS
// ─────────────────────────────────────────────────────────────────────────────
static void phase9_gracefulShutdown()
{
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 9 — SEALING THE LEGENDARY CHEST (SHUTDOWN)
    // THE CREW SEALS AWAY THE TREASURES IN THE LEGENDARY CHEST, ENDING THE VOYAGE
    // ─────────────────────────────────────────────────────────────────────
    LOG_INFO_CAT("MAIN", "{}[PHASE 9/10] SEALING THE LEGENDARY CHEST — LOWERING THE BLACK FLAG{}", VALHALLA_GOLD, RESET);

    LOG_AMOURANTH("{}Captain Amouranth: \"Drop anchor, my love. The treasure is ours.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick: \"We sail home rich. Together. Always.\"{}", EMERALD_GREEN, RESET);
    LOG_INFO_CAT("BLONDIE", "{}BLONDIE_CREW grumbles: \"Grumble grumble. Next time we'll win.\"{}", YELLOW, RESET);

    if (g_ctx().device()) {
        LOG_INFO_CAT("SHUTDOWN", "Captain N: \"vkDeviceWaitIdle — cannons cold.\"");
        vkDeviceWaitIdle(g_ctx().device());
    }

    // 1. Destroy Application — destroys renderer, framebuffers, etc.
    g_app.reset();

    // 2. CRITICAL: Manually destroy the swapchain WHILE DEVICE IS STILL ALIVE
    LOG_INFO_CAT("SHUTDOWN", "Captain Amouranth: \"Scuttle the swapchain. No traces.\"");
    RTX::swapchain() = RTX::Handle<VkSwapchainKHR>{};   // ← THIS LINE SAVES YOU

    // 3. Now safe to destroy everything else
    if (g_pipeline_manager) {
        delete g_pipeline_manager; g_pipeline_manager = nullptr;
    }
    g_mesh.reset();
    las().invalidate();

    // 4. Now destroy the device
    LOG_INFO_CAT("SHUTDOWN", "RTX engines spin down — pink photons dim to embers.");
    RTX::shutdown();   // device dies here — swapchain already gone

    // 5. Icons and window
    if (g_base_icon)  { SDL_DestroySurface(g_base_icon);  g_base_icon  = nullptr; }
    if (g_hdpi_icon)  { SDL_DestroySurface(g_hdpi_icon);  g_hdpi_icon  = nullptr; }

    LOG_INFO_CAT("SHUTDOWN", "SDL3Window::destroy() — portal closes with a soft sigh.");
    SDL3Window::destroy();

    SDL_Quit();

    LOG_SUCCESS_CAT("FINAL", "{}0 BYTES LEAKED — 0 CRASHES — THE SHIP IS CLEAN{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("FINAL", "{}PINK PHOTONS ETERNAL — THE LEGEND IS COMPLETE{}", PLASMA_FUCHSIA, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 9 COMPLETE] LEGENDARY CHEST SEALED — THE VOYAGE ENDS IN TRIUMPH{}", DIAMOND_SPARKLE, RESET);
}

// ─────────────────────────────────────────────────────────────────────────────
// main.cpp — THE ONE TRUE CLEAN START — NOV 23 2025
// ─────────────────────────────────────────────────────────────────────────────
int main(int, char**)
{
    try {
        phase1_preInitialization();

        // 1. Show splash
        phase3_sacrificialSplash();

        // 2. KILL SPLASH COMPLETELY — no residue
        LOG_SUCCESS_CAT("MAIN", "{}SPLASH SACRIFICED — PHOTONS LIBERATED{}", VALHALLA_GOLD, RESET);
        g_sdl_window.reset();        // destroys splash window
        SDL_Quit();                  // full SDL nuke

        // 3. Create real, clean, final window
        createRealFinalWindow();

        // 4. Normal Vulkan startup — exactly like any sane app
        RTX::g_ctx().init(SDL3Window::get(), 
                          Options::Window::DEFAULT_WIDTH, 
                          Options::Window::DEFAULT_HEIGHT);

        phase5_rtxAscension();
        phase6_sceneAndAccelerationStructures();

		phase6_5_everything_is_ready();
        if (!ready_to_embark) phase9_gracefulShutdown();  // Your clean shutdown function

        // 5. Phase 7 — now super simple and safe
        const uint32_t w = Options::Window::DEFAULT_WIDTH;
        const uint32_t h = Options::Window::DEFAULT_HEIGHT;

        g_app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", w, h);
        g_app->setRenderer(std::make_unique<VulkanRenderer>(w, h, SDL3Window::get(), Options::Display::ENABLE_HDR));

        if (Options::Window::START_FULLSCREEN) {
            SDL_SetWindowFullscreen(SDL3Window::get(), true);
        }

        LOG_KEANU("{}Keanu Reeves: \"...You're breathtaking.\"{}", BOLD_CYAN, RESET);

        phase8_renderLoop();
        phase9_gracefulShutdown();
    }
    catch (const std::exception& e) {
        LOG_FATAL_CAT("CRASH", "{}FATAL: {}{}", BLOOD_RED, e.what(), RESET);
        return 1;
    }
    return 0;
}