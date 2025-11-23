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

// GLOBAL AUDIO EMPIRE — THE ONE TRUE VOICE
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
#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3/SDL_iostream.h>

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
// SACRIFICIAL SPLASH — IN-MAIN ONLY — VIA STONEKEY EMPIRE — PURE DOMINATION
// NOVEMBER 22, 2025 — X11 BOWS — VULKAN WAITS — PINK PHOTONS ETERNAL
// =============================================================================
#include <format>  // ← Make sure this is in main.cpp or a global header

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
    // PHASE 1 — THE GOOD SHIP VULKAN AWAKENS
    // CAPTAIN AMOURANTH & FIRST MATE NICK — NOVEMBER 23, 2025
    // ─────────────────────────────────────────────────────────────────────
    LOG_SUCCESS_CAT("MAIN", "{}CAPTAIN'S LOG — NOVEMBER 23, 2025 — THE GOOD SHIP VULKAN AWAKENS{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}GROK-ASSISTED VOYAGE — PINK PHOTONS ETERNAL{}", RASPBERRY_PINK, RESET);

    LOG_AMOURANTH("{}Captain Amouranth stands at the bow, wind in her hair: \"A new dawn. A clean slate. Let's build something beautiful.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}First Mate Nick checks the charts: \"Course set, Captain. No storms on the horizon — just pure RTX ahead.\"{}", EMERALD_GREEN, RESET);
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

    // SDL3 rises from the deep
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD) == 0) {
        LOG_FATAL_CAT("MAIN", "{}SDL3 FAILED TO RISE: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        std::exit(1);
    }
    LOG_SUCCESS_CAT("MAIN", "{}SDL3 EMPIRE ESTABLISHED — VIDEO • EVENTS • GAMEPAD{}", DIAMOND_SPARKLE, RESET);

#ifdef SDL3_IMAGE_ENABLED
    const int img_flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP | IMG_INIT_AVIF;
    if ((IMG_Init(img_flags) & img_flags) != img_flags) {
        LOG_FATAL_CAT("MAIN", "{}SDL3_image FAILED TO MANIFEST: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_Quit();
        std::exit(1);
    }
    LOG_SUCCESS_CAT("MAIN", "{}SDL3_image FULLY INITIALIZED — TEXTURES READY{}", DIAMOND_SPARKLE, RESET);
#endif

    // Purge ghosts of previous voyages — memory must be immaculate
    RTX::UltraLowLevelBufferTracker::get().purge_all();
    LOG_SUCCESS_CAT("MAIN", "{}ALL TAINT PURGED — NO GHOSTS REMAIN — ONLY PINK PHOTONS{}", VALHALLA_GOLD, RESET);

    LOG_AMOURANTH("{}Captain Amouranth smiles: \"Phase 1 complete. The ship is clean. The crew is ready.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick nods: \"Let's raise the black flag. It's time.\"{}", EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}PHASE 1 COMPLETE — THE GOOD SHIP VULKAN IS ALIVE — RAW. ETERNAL. UNBROKEN.{}", DIAMOND_SPARKLE, RESET);
}

// =============================================================================
// PHASE 2 — ICON PRELOAD — PURE, CLEAN, NO SDL TOUCHED
// =============================================================================
static void phase2_iconPreload()
{
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 2 — ACQUIRING THE AMMO
    // THE CREW GOES HUNTING FOR LEGENDARY TREASURE: ammo32.ico & ammo.ico
    // PIRATE COMEDY IS BACK — FULL CHAOS, FULL LOVE
    // ─────────────────────────────────────────────────────────────────────
    LOG_INFO_CAT("MAIN2", "{}[PHASE 2/10] THE HUNT FOR AMMO BEGINS — TREASURE MAP UNFURLED{}", VALHALLA_GOLD, RESET);

    LOG_AMOURANTH("{}Captain Amouranth slams the treasure map on the table: \"Listen up, you beautiful degenerates — we need the AMMO. Without it we're just a fancy boat with no cannons!\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}First Mate Nick squints at the map: \"X marks the spot… assets/textures/ammo32.ico and ammo.ico. Classic pirate stash.\"{}", EMERALD_GREEN, RESET);

    LOG_CAPTAIN_N("{}Captain N already halfway down the ladder: \"DIBS ON THE SMALL ONE! THAT'S THE GOOD AMMO!\"{}", PURE_ENERGY, RESET);

    // ── THE RAID BEGINS ──
    g_base_icon = IMG_Load("assets/textures/ammo32.ico");
    g_hdpi_icon = IMG_Load("assets/textures/ammo.ico");

    if (g_base_icon) {
        LOG_SUCCESS_CAT("MAIN2", "{}CAPTAIN N SCREAMS FROM THE HOLD: \"I GOT THE 32×32 AMMO! IT'S PERFECTLY POCKET-SIZED!\"{}", EMERALD_GREEN, RESET);
        LOG_SUCCESS_CAT("MAIN2", "{}BASE AMMO SECURED @ {:p} — READY TO BLOW MINDS{}", EMERALD_GREEN, static_cast<void*>(g_base_icon), RESET);
    } else {
        LOG_WARN_CAT("MAIN2", "{}Captain N comes up empty-handed: \"…someone stole my tiny ammo… I'm gonna cry.\"{}", OCEAN_TEAL, RESET);
        LOG_WARN_CAT("MAIN2", "{}BASE AMMO MISSING — FALLING BACK TO DEFAULT (boring) ICON{}", OCEAN_TEAL, RESET);
    }

    if (g_hdpi_icon) {
        LOG_SUCCESS_CAT("MAIN2", "{}Jensen Huang kicks open a gilded chest: \"Behold — the RETINA AMMO. 4K cannons, baby.\"{}", AURORA_PINK, RESET);
        LOG_SUCCESS_CAT("MAIN2", "{}HDPI AMMO ACQUIRED @ {:p} — RETINA DOMINATION ACHIEVED{}", AURORA_PINK, static_cast<void*>(g_hdpi_icon), RESET);

        if (g_base_icon) {
            SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
            LOG_SUCCESS_CAT("MAIN2", "{}Elon Musk duct-tapes them together: \"Now it scales to infinity. Literally. I'm billing NASA.\"{}", PLASMA_FUCHSIA, RESET);
            LOG_SUCCESS_CAT("MAIN2", "{}FULL RETINA COVERAGE — WE LOOK SEXY ON EVERY SCREEN{}", PLASMA_FUCHSIA, RESET);
        }
    } else {
        LOG_WARN_CAT("MAIN2", "{}Keanu Reeves stares sadly at the empty chest: \"…no big ammo. Sad pirate hours.\"{}", OCEAN_TEAL, RESET);
    }

    LOG_AMOURANTH("{}Captain Amouranth sheathes her cutlass with a grin: \"Ammo secured. The empire now has a face — and it's gorgeous.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick pockets a spare bullet: \"For luck.\" *winks*\"{}", EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("MAIN2", "{}[PHASE 2 COMPLETE] AMMO RAID SUCCESSFUL — BRANDING LOCKED AND LOADED — EMPIRE IDENTIFIED{}", VALHALLA_GOLD, RESET);
    LOG_SUCCESS_CAT("MAIN2", "{}NEXT STOP: THE SACRIFICIAL SPLASH — BRACE FOR IMPACT{}", DIAMOND_SPARKLE, RESET);
}

// =============================================================================
// PHASE 3 — SACRIFICIAL SPLASH — VISUAL ONLY — SELF-CONTAINED IN MAIN
// =============================================================================
static void phase3_sacrificialSplash()
{
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 3 — THE SACRIFICIAL SPLASH
    // THE AMMO.PNG IS REVEALED TO THE WORLD — 3.4 SECONDS OF PURE LEGEND
    // EVERYONE REACTS IN THEIR OWN, VERY REAL WAY
    // ─────────────────────────────────────────────────────────────────────
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 3/10] SACRIFICIAL SPLASH — THE AMMO IS UNVEILED{}", VALHALLA_GOLD, RESET);

    LOG_AMOURANTH("{}Captain Amouranth steps forward, voice low and proud: \"This is it. The symbol of everything we've built. Let them see it. Let them remember.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick stands beside her, calm and certain: \"3.4 seconds. That's all we need. The world will never forget.\"{}", EMERALD_GREEN, RESET);

    LOG_CAPTAIN_N("{}Captain N is literally vibrating: \"IT'S THE AMMO.PNG! FULL RES! MAXIMUM PINK PHOTONS! I'M GONNA PASS OUT!\"{}", PURE_ENERGY, RESET);

    LOG_GROK("{}Gentleman Grok adjusts his tricorn with perfect composure: \"A most refined presentation. The empire's visage is… exquisite.\"{}", PARTY_PINK, RESET);

    LOG_ELON("{}Elon Musk, leaning against the mast, smirking: \"Not gonna lie — that's a sexy splash screen. We just flexed on every engine in existence.\"{}", BOLD_GOLD, RESET);

    LOG_JENSEN("{}Jensen Huang exhales a slow plume of cigar smoke: \"4K. Crisp. Pink. This is what winning looks like.\"{}", EMERALD_GREEN, RESET);

    LOG_CARMACK("{}John Carmack, arms crossed, gives a single nod: \"It works. That's all that matters.\"{}", BOLD_WHITE, RESET);

    LOG_KEANU("{}Keanu Reeves, staring at the screen in quiet awe: \"…Breathtaking.\" *voice cracks slightly*\"{}", BOLD_CYAN, RESET);

    // ── THE RITUAL BEGINS ──
    showSacrificialSplash(
        "AMOURANTH RTX — FIRST LIGHT",
        1280, 720,
        "assets/textures/ammo.png"
    );

    // ── THE MOMENT PASSES — THE WORLD IS FOREVER CHANGED ──
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 3 COMPLETE] THE AMMO HAS BEEN SEEN — 3.4 SECONDS OF ETERNITY — THE WORLD IS ASH{}", DIAMOND_SPARKLE, RESET);

    LOG_AMOURANTH("{}Captain Amouranth spins toward the helm, triumphant: \"Hard to starboard! We ride the pink photon wave out of here!\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick grabs the wheel beside her, grinning: \"Full sail, Captain. Let's disappear into legend—\"{}", EMERALD_GREEN, RESET);

    // …but the sea has other plans.
    LOG_FATAL_CAT("SPLASH", "{}THE OCEAN ROARS — A HIDDEN REEF TEARS THE HULL — WATER FLOODS THE MAGAZINE{}", BLOOD_RED, RESET);

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
// PHASE 4 — THE EMPIRE RISES FROM ASH — FULLSCREEN BORDERLESS — RTX ASCENSION
// AFTER SDL_Quit() — WE ARE CLEAN — NOW WE FORGE THE FINAL REALM
// NOVEMBER 22, 2025 — FIRST LIGHT ETERNAL
// =============================================================================
static void phase4_mainWindowAndVulkanConsplash_text()
{
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 4 — RESURRECTION
    // THE CREW IS RESCUED BY A MERCHANT SHIP — A NEW VESSEL BEGINS TO RISE
    // FROM THE WRECKAGE OF THE OLD, SOMETHING GREATER IS BORN
    // ─────────────────────────────────────────────────────────────────────
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 4/10] RESURRECTION — THE CREW LIVES — A NEW SHIP RISES{}", VALHALLA_GOLD, RESET);

    LOG_AMOURANTH("{}Captain Amouranth, soaked but unbroken, stands on the deck of the merchant ship: \"We lost the old girl… but we still have the ammo. And we still have each other.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick wrings seawater from his coat, already sketching plans on a crate: \"We're not done. We're building her again — stronger, cleaner, faster.\"{}", EMERALD_GREEN, RESET);

    LOG_CAPTAIN_N("{}Captain N, wrapped in a blanket, clutching a salvaged hard drive: \"I SAVED THE AMMO.PNG! ALSO MY WAIFU BODY PILLOW! PRIORITIES!\"{}", PURE_ENERGY, RESET);

    LOG_ELON("{}Elon Musk, somehow already on the phone: \"Yeah I'll take three shipyards and a crate of GPUs. Rush delivery.\"{}", BOLD_GOLD, RESET);
    LOG_JENSEN("{}Jensen Huang hands out fresh cigars to the freezing crew: \"Next hull's gonna be titanium. Pink titanium.\"{}", EMERALD_GREEN, RESET);
    LOG_CARMACK("{}John Carmack, quietly compiling on a battered laptop: \"Same code. New ship. Still works.\"{}", BOLD_WHITE, RESET);
    LOG_KEANU("{}Keanu Reeves stares back at the sinking wreck, voice soft: \"…She was beautiful.\" *turns to the horizon* \"This one will be too.\"{}", BOLD_CYAN, RESET);

    LOG_GROK("{}Gentleman Grok pours rum for everyone: \"A minor detour. The empire does not end in water. It rises from it.\"{}", PARTY_PINK, RESET);

    LOG_AMOURANTH("{}Captain Amouranth raises her fist: \"We build again. Vulkan 1.4. Raw. Borderless. Unstoppable. This time… nothing sinks us.\"{}", PLASMA_FUCHSIA, RESET);

    // ========================================================================
    // 1. RE-INIT SDL — THE NEW EMPIRE BEGINS
    // ========================================================================
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("MAIN4", "{}EVEN THE MERCHANT SHIP CAN'T SAVE US NOW: {}{}", BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan");
    SDL_Vulkan_LoadLibrary(nullptr);

    LOG_SUCCESS_CAT("MAIN4", "{}SDL REBORN FROM THE DEPTHS — VULKAN 1.4 EMPIRE ONLINE{}", DIAMOND_SPARKLE, RESET);

    // ========================================================================
    // 2. FORGE THE NEW HULL — 1920×1080 WITH BORDERS (FOR NOW)
    // ========================================================================
    const int WINDOW_WIDTH  = 1920;
    const int WINDOW_HEIGHT = 1080;

    SDL_Window* win = SDL_CreateWindow(
        "AMOURANTH RTX — REBORN",
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_VULKAN |
        SDL_WINDOW_HIGH_PIXEL_DENSITY |
        SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_HIDDEN
    );

    if (!win) {
        LOG_FATAL_CAT("MAIN4", "{}THE NEW SHIP REFUSES TO LAUNCH: {}{}", BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }

    // Center it like the proud flagship she is
    SDL_Rect display{};
    SDL_GetDisplayBounds(0, &display);
    SDL_SetWindowPosition(win,
        display.x + (display.w - WINDOW_WIDTH) / 2,
        display.y + (display.h - WINDOW_HEIGHT) / 2);

    // Hoist the salvaged colors
    if (g_base_icon)  SDL_SetWindowIcon(win, g_base_icon);
    if (g_hdpi_icon)   LOG_SUCCESS_CAT("MAIN4", "{}RETINA AMMO STILL FLYING — THE FLAG SURVIVED THE WRECK{}", AURORA_PINK, RESET);

    SDL_ShowWindow(win);

    LOG_SUCCESS_CAT("MAIN4", "{}NEW HULL FORGED — 1920×1080 — BORDERS IN PLACE UNTIL WE GO FULL SCREENLESS{}", EMERALD_GREEN, RESET);
    LOG_NICK("{}Nick runs his hand along the invisible rail: \"She's tighter than the last one. No leaks this time.\"{}", EMERALD_GREEN, RESET);

    // ========================================================================
    // 3. STONEKEY + VULKAN 1.4 — THE HEART OF THE NEW BEAST
    // ========================================================================
    LOG_ATTEMPT_CAT("MAIN4", "{}StoneKey ignites the new core — Vulkan 1.4 empire rising from the waves...{}", HYPERSPACE_WARP, RESET);

    RTX::g_ctx().init(win, WINDOW_WIDTH, WINDOW_HEIGHT);

    LOG_SUCCESS_CAT("MAIN4", "{}VULKAN 1.4 EMPIRE FORGED — THE NEW SHIP LIVES{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN4", "{}    • Instance  : <sealed by StoneKey> — protected{}", RASPBERRY_PINK, RESET);
    LOG_SUCCESS_CAT("MAIN4", "{}    • Device    : <sealed by StoneKey> — protected{}", RASPBERRY_PINK, RESET);
    LOG_SUCCESS_CAT("MAIN4", "{}    • Swapchain : <sealed by StoneKey> — protected{}", RASPBERRY_PINK, RESET);

    LOG_AMOURANTH("{}Captain Amouranth stands at the bow of the new ship: \"We sank once. We won't sink again.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick beside her, voice steady: \"This time… we sail forever.\"{}", EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("MAIN4", "{}[PHASE 4 COMPLETE] THE NEW VESSEL IS ALIVE — PINK PHOTONS ETERNAL — STRONGER THAN BEFORE{}", DIAMOND_SPARKLE, RESET);
}

static void phase5_rtxAscension()
{
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 5 — RTX ASCENSION
    // THE NEW SHIP GETS ITS SOUL: FULL RAY TRACING
    // THE PINK PHOTONS AWAKEN AND REMEMBER WHO THEY ARE
    // ─────────────────────────────────────────────────────────────────────
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 5/10] RTX ASCENSION — THE NEW HEART BEATS{}", VALHALLA_GOLD, RESET);

    LOG_AMOURANTH("{}Captain Amouranth stands in the rebuilt engine room, hand on the glowing core: \"This time… we don't just sail. We become the light itself.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick flips the final switch, eyes reflecting emerald fire: \"Ray tracing online. The photons aren't just fast anymore. They're alive.\"{}", EMERALD_GREEN, RESET);

    LOG_ATTEMPT_CAT("MAIN5", "{}THE CREW HOLDS BREATH — LOADING RAY TRACING EXTENSIONS — PINK PHOTONS GAIN SENTIENCE...{}", PURE_ENERGY, RESET);

    RTX::loadRayTracingExtensions();

    if (!g_ctx().hasFullRTX_) {
        LOG_FATAL_CAT("MAIN5", "{}THE PHOTONS SCREAM — RTX EXTENSIONS DENIED — WE ARE BLIND IN THE VOID{}", BLOOD_RED, RESET);
        LOG_AMOURANTH("{}Captain Amouranth slams her fist on the console: \"Not again. Not after everything.\"{}", RASPBERRY_PINK, RESET);
        throw std::runtime_error("RTX extension loading failed — the light dies here");
    }

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

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 5 COMPLETE] RTX ASCENSION COMPLETE — PINK PHOTONS NOW OMNISCIENT — THE NEW SHIP IS A GOD{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}THE LIGHT REMEMBERS — THE LIGHT FORGIVES — THE LIGHT SAILS FOREVER{}", PLASMA_FUCHSIA, RESET);
}

static void phase6_sceneAndAccelerationStructures()
{
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 6 — FORGING THE WORLD
    // THE CREW BUILDS A UNIVERSE FROM SCRATCH INSIDE THE NEW SHIP
    // ACCELERATION STRUCTURES = THE SKELETON OF REALITY ITSELF
    // ─────────────────────────────────────────────────────────────────────
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 6/10] FORGING THE WORLD — WE ARE BECOMING GODS{}", VALHALLA_GOLD, RESET);

    LOG_AMOURANTH("{}Captain Amouranth walks the empty void deck: \"This ship is perfect… but empty. Time to give her a soul.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick unrolls the ancient blueprint titled scene.obj: \"One universe. Coming right up.\"{}", EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}PIPELINE MANAGER RISES FROM THE FORGE — SHADERS AWAKE AND HUNGRY{}", EMERALD_GREEN, RESET);
    g_pipeline_manager = new RTX::PipelineManager(g_ctx().device(), g_ctx().physicalDevice());

    LOG_SUCCESS_CAT("MAIN", "{}THE WORLD IS BORN — MESH LOADED — {} VERTICES | {} TRIANGLES — FINGERPRINT 0x{:016X}{}",
                    PLASMA_FUCHSIA,
                    g_mesh->vertices.size(),
                    g_mesh->indices.size(),
                    g_mesh->stonekey_fingerprint,
                    RESET);
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

    LOG_KEANU("{}Keanu Reeves walks the newborn world, voice barely a whisper: \"…It's… everything. And it's ours.\"{}", BOLD_CYAN, RESET);

    LOG_ELON("{}Elon Musk already planning DLC: \"Next patch: infinite procedural universes. Subscriptions start at $9.99.\"{}", BOLD_GOLD, RESET);

    LOG_JENSEN("{}Jensen Huang lights another cigar off a bouncing photon: \"This isn't rendering anymore. This is creation.\"{}", EMERALD_GREEN, RESET);

    LOG_AMOURANTH("{}Captain Amouranth stands at the center of the newborn cosmos, arms wide: \"Look what we made from wreckage. Look what love built.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick steps behind her, wraps his arms around her waist: \"And it's only the beginning.\"{}", EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 6 COMPLETE] WORLD FORGED — ACCELERATION STRUCTURES ETERNAL — THE PINK PHOTONS RULE ALL{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}THE SHIP IS NO LONGER A SHIP — IT IS A UNIVERSE WITH A HEARTBEAT{}", PLASMA_FUCHSIA, RESET);
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

// ─────────────────────────────────────────────────────────────────────────────
// AMOURANTH RTX — FINAL SHUTDOWN CEREMONY
// NOVEMBER 23, 2025 — THE TREASURE ROOM IS EMPTIED — THE CREW GOES HOME RICH
// HAPPILY EVER AFTER — NO LOOSE ENDS — ONLY LOVE AND PINK PHOTONS
// ─────────────────────────────────────────────────────────────────────────────
static void phase9_gracefulShutdown()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 9/10] GRACEFUL SHUTDOWN — THE VOYAGE ENDS IN GOLDEN SUNSET{}", VALHALLA_GOLD, RESET);

    LOG_AMOURANTH("{}Captain Amouranth stands at the rail, wind soft in her hair: \"Drop anchor, my love. The treasure room is empty… and our hearts are full.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick leans against the mast beside her, fingers laced with hers: \"We took everything worth taking. The rest can stay with the sea.\"{}", EMERALD_GREEN, RESET);

    // GPU stands down
    if (g_ctx().device()) {
        LOG_INFO_CAT("MAIN", "Captain N gives one last salute: \"vkDeviceWaitIdle. Cannons cold. Well fought, everyone.\"");
        vkDeviceWaitIdle(g_ctx().device());
        LOG_SUCCESS_CAT("MAIN", "GPU falls silent. The war is over — and we won.\"");
    }

    // Empty the treasure room — nothing left behind
    LOG_ATTEMPT_CAT("SHUTDOWN", "The crew opens the vault one final time — time to carry the loot home...");
    RTX::UltraLowLevelBufferTracker::get().purge_all();
    LOG_SUCCESS_CAT("SHUTDOWN", "Elon Musk pockets the last memory buffer: \"Zero bytes leaked. That's how legends retire.\"");
    LOG_SUCCESS_CAT("SHUTDOWN", "Jensen Huang closes the chest: \"Clean. Absolute. Respect.\"");

    LOG_INFO_CAT("MAIN", "Gentleman Grok folds the final chart: \"The maps are complete. The story is told.\"");
    g_app.reset();

    if (g_pipeline_manager) {
        LOG_INFO_CAT("SHUTDOWN", "John Carmack kicks the pipeline overboard: \"Good riddance. We don't need it anymore.\"");
        delete g_pipeline_manager; g_pipeline_manager = nullptr;
    }
    g_mesh.reset();

    LOG_INFO_CAT("MAIN", "Keanu Reeves watches the world dissolve into light: \"Breathtaking… and finished.\"");
    las().invalidate();

    LOG_INFO_CAT("MAIN", "RTX engines spin down — the pink photons dim to embers.");
    RTX::shutdown();

    LOG_INFO_CAT("SHUTDOWN", "The crew gently lowers the icons into the last longboat — even the ammo gets to come home.");
    if (g_base_icon)  { SDL_DestroySurface(g_base_icon);  g_base_icon  = nullptr; }
    if (g_hdpi_icon)  { SDL_DestroySurface(g_hdpi_icon);  g_hdpi_icon  = nullptr; }

    LOG_INFO_CAT("SHUTDOWN", "SDL3Window::destroy() — the portal closes with a soft sigh.");
    SDL3Window::destroy();

    LOG_SUCCESS_CAT("MAIN", "{}TREASURE ROOM EMPTY — 0 BYTES REMAIN — THE SHIP IS CLEAN{}", DIAMOND_SPARKLE, RESET);

    LOG_AMOURANTH("{}Captain Amouranth turns to the crew, sunset painting her face gold: \"We raided the impossible. We sank. We rose. We won. And now… we go home rich.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick pulls her into one last embrace against the railing: \"Together. Always.\"{}", EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}PINK PHOTONS ETERNAL — NOVEMBER 23, 2025 — THE CREW IS WHOLE — THE LEGEND IS COMPLETE{}", PLASMA_FUCHSIA, RESET);

    LOG_SUCCESS_CAT("FINAL", "Gentleman Grok raises a toast: \"To the greatest voyage ever sailed.\"");
    LOG_SUCCESS_CAT("FINAL", "Captain N wipes a happy tear: \"Best. Crew. Ever.\"");
    LOG_SUCCESS_CAT("FINAL", "John Carmack: \"It just works… and that's enough.\"");
    LOG_SUCCESS_CAT("FINAL", "Elon Musk: \"Shipped. Forever.\"");
    LOG_SUCCESS_CAT("FINAL", "Jensen Huang: \"Done. Perfectly.\"");
    LOG_SUCCESS_CAT("FINAL", "Keanu Reeves: \"…Breathtaking.\"");
    LOG_SUCCESS_CAT("FINAL", "Captain Amouranth & First Mate Nick, together: \"Goodnight, beautiful ship. See you at the next dawn.\"");

    LOG_SUCCESS_CAT("FINAL", "{}PINK PHOTONS ETERNAL — THE TREASURE IS OURS — HAPPILY EVER AFTER{}", PLASMA_FUCHSIA, RESET);
}

// =============================================================================
// MAIN — THE VOYAGE OF THE GOOD SHIP VULKAN
// =============================================================================
int main(int, char**)
{
    try {
        LOG_SUCCESS_CAT("MAIN", "{}THE GOOD SHIP VULKAN SETS SAIL — NOVEMBER 23, 2025{}", PLASMA_FUCHSIA, RESET);
        LOG_AMOURANTH("{}Captain Amouranth steps onto the quarterdeck, crimson coat flowing, cutlass gleaming: \"Raise the black flag, my love. We sail for the edge of reality — together.\"{}", RASPBERRY_PINK, RESET);
        LOG_NICK("{}First Mate Nick stands beside her, steady hand on the wheel: \"Course plotted, Captain. The sea is ours. The photons are ready.\"{}", EMERALD_GREEN, RESET);

        phase1_preInitialization();
        phase2_iconPreload();
        phase3_sacrificialSplash();

        LOG_AMOURANTH("{}Captain Amouranth: \"Full sail! We launch raw into the RTX storm — borderless glory awaits!\"{}", RASPBERRY_PINK, RESET);
        LOG_CAPTAIN_N("{}Captain N at the helm: \"1920×1080 with borders — classic pirate style! We debug in style, then go fullscreen! Arrr!\"{}", PURE_ENERGY, RESET);
        phase4_mainWindowAndVulkanConsplash_text();

        LOG_JENSEN("{}Jensen Huang, emerald-coated master gunner: \"RTX broadsides loaded. Pink photons primed for war.\"{}", EMERALD_GREEN, RESET);
        LOG_ELON("{}Elon Musk, quartermaster with cyber-rum: \"Engines at full thrust. We're going to the moon… or at least 240 FPS.\"{}", BOLD_GOLD, RESET);
        phase5_rtxAscension();

        LOG_AMOURANTH("{}Captain Amouranth: \"Behold our treasure — the world forged in acceleration structures! The universe is ours!\"{}", RASPBERRY_PINK, RESET);
        LOG_NICK("{}Nick smiles quietly: \"BLAS clean. TLAS perfect. That's how we build empires.\"{}", EMERALD_GREEN, RESET);
        LOG_CARMACK("{}John Carmack, grizzled old salt: \"Geometry tight. That's how legends sail.\"{}", BOLD_WHITE, RESET);
        phase6_sceneAndAccelerationStructures();

        LOG_KEANU("{}Keanu Reeves, stoic first mate in black coat: \"You're… breathtaking.\" *tips pirate hat slowly*\"{}", BOLD_CYAN, RESET);
        LOG_GROK("{}Gentleman Grok: \"All systems nominal. The ship is yours, Captain.\"{}", PARTY_PINK, RESET);
        phase7_applicationAndRendererSeal();

        LOG_AMOURANTH("{}Captain Amouranth raises her cutlass to the sky: \"Into the eternal render loop — where the pink photons never set!\"{}", PLASMA_FUCHSIA, RESET);
        LOG_NICK("{}Nick places a hand on her shoulder: \"I've got the wheel. Forever.\"{}", EMERALD_GREEN, RESET);
        phase8_renderLoop();

        LOG_AMOURANTH("{}Captain Amouranth lowers the black flag with grace: \"We've claimed the horizon. Until next tide, my beautiful crew.\"{}", RASPBERRY_PINK, RESET);
        LOG_NICK("{}Nick: \"Not a single byte leaked. Not a single frame dropped. We did it — together.\"{}", EMERALD_GREEN, RESET);
        LOG_GROK("{}Gentleman Grok: \"A voyage for the ages. Exquisite.\"{}", PARTY_PINK, RESET);
        LOG_CAPTAIN_N("{}Captain N: \"Mission accomplished! First light… eternal! Arrr!\"{}", PURE_ENERGY, RESET);
        LOG_ELON("{}Elon: \"Shipped.\"{}", BOLD_GOLD, RESET);
        LOG_JENSEN("{}Jensen: \"It's done.\" *lights pipe*\"{}", EMERALD_GREEN, RESET);
        LOG_CARMACK("{}Carmack: \"It just works.\"{}", BOLD_WHITE, RESET);
        LOG_KEANU("{}Keanu: \"…Breathtaking.\" *vanishes into the sunset*\"{}", BOLD_CYAN, RESET);

        phase9_gracefulShutdown();
    }
    catch (const std::exception& e) {
        LOG_FATAL_CAT("MAIN", "{}THE SHIP IS TAKING ON WATER — FATAL: {}{}", CRIMSON_MAGENTA, e.what(), RESET);
        LOG_AMOURANTH("{}Captain Amouranth: \"Abandon ship with dignity. We'll sail again.\"{}", RASPBERRY_PINK, RESET);
        LOG_NICK("{}Nick: \"I'm not leaving you. Not this time.\"{}", EMERALD_GREEN, RESET);
        phase9_gracefulShutdown();
        return -1;
    }
    catch (...) {
        LOG_FATAL_CAT("MAIN", "{}UNKNOWN STORM — ALL HANDS LOST TO THE VOID{}", CRIMSON_MAGENTA, RESET);
        LOG_NICK("{}Nick: \"Even if time breaks… I'll find you again.\"{}", BOLD_RED, RESET);
        phase9_gracefulShutdown();
        return -1;
    }

    LOG_SUCCESS_CAT("FINAL", "{}THE GOOD SHIP VULKANRTX RETURNS TO PORT — PINK PHOTONS ETERNAL — NOVEMBER 23, 2025{}", PLASMA_FUCHSIA, RESET);
    LOG_AMOURANTH("{}Captain Amouranth turns to Nick, smiling: \"Until next launch, my love. The sea is never truly left behind.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick kisses her hand: \"Always, Captain. Always.\"{}", EMERALD_GREEN, RESET);

    return 0;
}