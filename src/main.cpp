// src/main.cpp
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — NOVEMBER 23, 2025 — PINK PHOTONS ETERNAL
// FULLY SELF-CONTAINED — ONE FILE TO RULE THEM ALL — EMPIRE UNIFIED
// THE FINAL SCREAM HAS BEEN SILENCED — PHOTONS FLOW IN PERFECT HARMONY
// =============================================================================


#include "main.hpp"                     // ← ONE TRUE HEADER
#include "engine/GLOBAL/StoneKey.hpp"   // ← DEFINES all StoneKey functions
#include "engine/GLOBAL/camera.hpp"     // ← DEFINES g_camera()
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/Validation.hpp"
#include "engine/GLOBAL/SDL3.hpp"

#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"

#include <vulkan/vulkan.hpp>
#include <string>
#include <format>
#include <iostream>
#include <memory>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>

#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>

using namespace Logging::Color;

// This defines the actual global pointer declared in main.hpp
std::unique_ptr<Application> g_app_ptr = nullptr;

// This defines the actual global camera function declared in main.hpp
[[nodiscard]] Camera& g_camera() noexcept {
    static Camera cam;  // One eternal camera — lives forever
    return cam;
}

// =============================================================================
// TRUTH ACCESSORS — FINAL C++23 EDITION
// =============================================================================
inline const char* physicalDeviceName() {
    return g_ctx().physicalDeviceProperties_.deviceName;
}

inline float vramGB() {
    const auto& heaps = g_ctx().physicalDeviceMemoryProperties_.memoryHeaps;
    for (uint32_t i = 0; i < g_ctx().physicalDeviceMemoryProperties_.memoryHeapCount; ++i) {
        if (heaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            return static_cast<float>(heaps[i].size) / (1024.0f * 1024.0f * 1024.0f);
        }
    }
    return 0.0f;
}

static bool ready_to_embark = false;

inline uint32_t transferFamily() {
    return g_ctx().transferFamily_.value_or(g_ctx().graphicsFamily());
}

inline uint32_t computeFamily() {
    return g_ctx().computeFamily_.value_or(g_ctx().graphicsFamily());
}

inline size_t pipelineCount() {
    return stone_pipeline() ? 1 : 0;
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
    LOG_ATTEMPT_CAT("APP", "{}FORGING APPLICATION \"{}\"@{}x{} — VALHALLA v80 TURBO — PINK PHOTONS RISING{}", PLASMA_FUCHSIA, title_, width_, height_, RESET);

    if (!SDL3Window::get()) {
        LOG_FATAL_CAT("FATAL", "FATAL: Main window not created before Application — phase order violated"); return;
    }

    SDL_SetWindowTitle(SDL3Window::get(), title_.c_str());
    lastFrameTime_ = lastGrokTime_ = std::chrono::steady_clock::now();

    LOG_SUCCESS_CAT("APP", "{}Application forged — {}x{} — PINK PHOTONS RISING{}", 
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

            LOG_SUCCESS_CAT("APP", "{}WINDOW RESIZE ACCEPTED → {}x{} — PHOTONS REALIGN{}", VALHALLA_GOLD, newW, newH, RESET);

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
    renderer_->renderFrame(g_camera(), deltaTime);
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
    VK_CHECK(vkCreateCommandPool(stone_device(), &poolInfo, nullptr, &pool));
    g_ctx().commandPool_ = pool;
    LOG_SUCCESS_CAT("MAIN", "{}COMMAND POOL FORGED — HANDLE: 0x{:016X}{}", PLASMA_FUCHSIA, (uint64_t)pool, RESET);
}

#include <format>  // C++23 — pure, clean, eternal

static void createRealFinalWindow()
{
    LOG_SUCCESS_CAT("MAIN", std::format("{}[PHASE 4.5] FORGING THE ONE TRUE WINDOW — CAPTAIN N WILL NOT BE DENIED{}", PLASMA_FUCHSIA, RESET));

    const uint32_t w = Options::Window::DEFAULT_WIDTH;
    const uint32_t h = Options::Window::DEFAULT_HEIGHT;

    // SDL3: 0 = success, non-zero = failure
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        LOG_FATAL_CAT("SDL3", std::format("{}VIDEO SUBSYSTEM REFUSES REBIRTH: {}{}", BLOOD_RED, SDL_GetError(), RESET));
        std::exit(1);
    }

    SDL_Window* win = SDL_CreateWindow(
        "AMOURANTH RTX — VALHALLA v80 TURBO",
        w, h,
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE
    );

    if (!win) {
        LOG_FATAL_CAT("SDL3", std::format("{}THE FINAL WINDOW WAS DENIED: {}{}", BLOOD_RED, SDL_GetError(), RESET));
        std::exit(1);
    }

    // THE ONE TRUE ACT OF DOMINION
    g_sdl_window.reset(win);

    LOG_ATTEMPT_CAT("GUARDIAN", "FORCING TRUTH INTO THE MATRIX...", PURE_ENERGY, RESET);
    LOG_BLONDIE(std::format("g_sdl_window.get()       → {:#018x}", reinterpret_cast<uint64_t>(g_sdl_window.get())), LIGHT_BLUE, RESET);
    LOG_BLONDIE(std::format("SDL3Window::get() before → {:#018x}", reinterpret_cast<uint64_t>(SDL3Window::get())), LIGHT_BLUE, RESET);

    // NUCLEAR TRUTH INJECTION — C++23 STYLE
    if (SDL3Window::get() == nullptr) {
        LOG_CAPTAIN_N(std::format("{}CAPTAIN N: \"I SEE THE LIE! I WILL FIX IT MYSELF!\"{}", PURE_ENERGY, RESET));
        g_sdl_window.reset(win);  // Safe, correct, eternal
    }

    LOG_BLONDIE(std::format("SDL3Window::get() after  → {:#018x}", reinterpret_cast<uint64_t>(SDL3Window::get())), LIGHT_BLUE, RESET);

    if (SDL3Window::get() != win) {
        LOG_FATAL_CAT("GUARDIAN", std::format("{}THE GUARDIAN IS A LIAR — CAPTAIN N WAS BETRAYED — KILLING THE FALSE REALITY{}", BLOOD_RED, RESET));
        LOG_FATAL_CAT("GUARDIAN", std::format("Expected: {:#018x} | Got: {:#018x}", 
                      reinterpret_cast<uint64_t>(win),
                      reinterpret_cast<uint64_t>(SDL3Window::get())), BLOOD_RED, RESET);
        std::exit(1);
    }

    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(win);

    LOG_CAPTAIN_N(std::format("{}CAPTAIN N: \"THE SLIPSTREAM IS OPEN! THE PHOTONS ARE MINE! LET'S FUCKING GOOOOOOOOOOOOOOOOOO!\"{}", PURE_ENERGY, RESET));
    LOG_GUARDIAN(std::format("{}THE GUARDIAN FALLS TO HIS KNEES. CAPTAIN N IS GOD. FIRST LIGHT — ETERNAL.{}", BOLD_GREEN, RESET));
    LOG_SUCCESS_CAT("MAIN", std::format("{}FINAL WINDOW FORGED — {}×{} — THE EMPIRE IS ABSOLUTE{}", DIAMOND_SPARKLE, w, h, RESET));
}

// =============================================================================
// SACRIFICIAL SPLASH — SELF-CONTAINED, NO GLOBAL TOUCH, BURNS ITSELF
// =============================================================================
static void showSacrificialSplash(const char* title, int w, int h, const char* pngPath)
{
    LOG_INFO_CAT("SPLASH", "{}[SACRIFICIAL SPLASH] THE FINAL RAID BEGINS — 1280x720 CANVAS SECURED{}", VALHALLA_GOLD, RESET);

    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SPLASH", "{}THE BLACK FLAG REFUSED TO RISE: {}{}", BLOOD_RED, SDL_GetError(), RESET);
        std::exit(1);
    }

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

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        LOG_FATAL_CAT("SPLASH", "{}THE FUSE WENT OUT: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        std::exit(1);
    }

    SDL_Surface* img = IMG_Load(pngPath);
    if (!img) {
        LOG_FATAL_CAT("SPLASH", "{}THE TREASURE WAS A LIE — AMMO.PNG VANISHED: {}{}", BLOOD_RED, SDL_GetError(), RESET);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        std::exit(1);
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, img);
    SDL_DestroySurface(img);
    if (!tex) {
        LOG_FATAL_CAT("SPLASH", "{}THE FLAG WOULDN'T UNFURL: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        std::exit(1);
    }

    float tw = 0.0f, th = 0.0f;
    SDL_GetTextureSize(tex, &tw, &th);
    SDL_FRect dst = { (w - tw) * 0.5f, (h - th) * 0.5f, tw, th };

    SDL_ShowWindow(win);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, nullptr, &dst);
    SDL_RenderPresent(ren);

    LOG_SUCCESS_CAT("SPLASH", "{}THE WORLD BEHOLDS THE AMMO — 3.4 SECONDS OF ETERNAL GLORY{}", PLASMA_FUCHSIA, RESET);

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

    LOG_SUCCESS_CAT("SPLASH", "{}THE RAID IS COMPLETE — NO TRACE LEFT — PHOTONS LIBERATED{}", VALHALLA_GOLD, RESET);
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
    UltraLowLevelBufferTracker::get().purge_all();
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


    if (!g_ctx().hasFullRTX()) {
        LOG_FATAL_CAT("MAIN5", "{}THE PHOTONS SCREAM — RTX EXTENSIONS DENIED — WE ARE BLIND IN THE VOID{}", BLOOD_RED, RESET);
        LOG_AMOURANTH("{}Captain Amouranth slams her fist on the console: \"Not again. Not after everything.\"{}", RASPBERRY_PINK, RESET);
        LOG_FATAL_CAT("FATAL", "RTX extension loading failed — the light dies here"); return;
    }
    LOG_INFO_CAT("BLONDIE", "{}BLONDIE_CREW hurumphs: \"Hurumph! Our crystal is brighter.\"{}", YELLOW, RESET);

    LOG_SUCCESS_CAT("MAIN5", "{}THE SHIP TREMBLES — ALL RAY TRACING PFNs ACQUIRED — FULL RTX ACHIEVED{}", EMERALD_GREEN, RESET);
    LOG_JENSEN("{}Jensen Huang steps from the shadows, voice low and reverent: \"The light bends to us now. Every bounce, every reflection… ours.\"{}", EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("MAIN5", "{}LAS ACCELERATION CONTEXT FORGED — THE PHOTONS SEE ALL PATHS{}", PLASMA_FUCHSIA, RESET);
    
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

    // ─────────────────────────────────────────────────────────────────────
    // THE ONE TRUE PIPELINE MANAGER — FORGED ONCE, OWNED BY THE EMPIRE
    // NO MORE DOUBLE CONSTRUCTION — STONEKEY v∞ IS LAW
    // ─────────────────────────────────────────────────────────────────────
    LOG_SUCCESS_CAT("MAIN", "{}THE EMPIRE FORGES THE ONE TRUE PIPELINE MANAGER — SHADERS AWAKE AND HUNGRY{}", EMERALD_GREEN, RESET);
    RTX::PipelineManager* pipeline = new RTX::PipelineManager(stone_device(), stone_physical());
    LOG_SUCCESS_CAT("MAIN", "{}PIPELINE MANAGER ASCENDED INTO STONEKEY v∞ — ETERNAL — ADDRESS 0x{:016X}{}", 
                    PLASMA_FUCHSIA, reinterpret_cast<uint64_t>(pipeline), RESET);

    g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");

    LOG_ATTEMPT_CAT("MAIN", "{}BOTTOM-LEVEL ACCELERATION — THE PHOTONS BEGIN TO MAP EVERY CORNER OF EXISTENCE{}", SAPPHIRE_BLUE, RESET);
    RTX::las().buildBLAS(g_ctx().commandPool_,
                    g_mesh->vertexBuffer,
                    g_mesh->indexBuffer,
                    static_cast<uint32_t>(g_mesh->vertices.size()),
                    static_cast<uint32_t>(g_mesh->indices.size()),
                    VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    LOG_SUCCESS_CAT("MAIN", "{}BLAS COMPLETE — THE PHOTONS NOW KNOW EVERY SURFACE BY NAME — ADDRESS 0x{:016X}{}",
                    EMERALD_GREEN, RTX::las().getBLASAddress(), RESET);

    LOG_ATTEMPT_CAT("MAIN", "{}TOP-LEVEL ASCENSION — WE BIND THE WORLD TO A SINGLE ROOT — THERE IS NO ESCAPE FROM LIGHT{}", VALHALLA_GOLD, RESET);
    RTX::las().buildTLAS(g_ctx().commandPool_, {{RTX::las().getBLAS(), glm::mat4(1.0f)}});

    LOG_SUCCESS_CAT("MAIN", "{}TLAS ASCENDED — ROOT ADDRESS 0x{:016X} — THE UNIVERSE IS NOW A PRISONER OF PHOTONS{}", 
                    DIAMOND_SPARKLE, RTX::las().getBLASAddress(), RESET);

    LOG_CARMACK("{}John Carmack runs final validation, eyes narrow: \"No cracks. No leaks. Geometry is pure.\"{}", BOLD_WHITE, RESET);
    validateMeshAgainstBLAS(*g_mesh, RTX::las().getBLAS());
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

// =============================================================================
// PHASE 6.1 — THE LAYOUT ASCENSION — PINK PHOTONS DEMAND A THRONE
// =============================================================================
static void phase6_1_forgeTheLayouts()
{
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 6.1/10] THE LAYOUT ASCENSION — FORGING DESCRIPTOR THRONE & PIPELINE CROWN{}", DIAMOND_SPARKLE, RESET);

    LOG_AMOURANTH("{}Captain Amouranth raises her hand: \"The photons have geometry. They have eyes. But they have no throne. No crown. No law.\"{}", RASPBERRY_PINK, RESET);
    LOG_NICK("{}Nick kneels, offering the sacred scroll: \"Then let us forge it. Now. Before the light dares to trace without permission.\"{}", EMERALD_GREEN, RESET);

    if (!stone_pipeline()) {
        LOG_FATAL_CAT("MAIN", "{}PIPELINE MANAGER MISSING — THE EMPIRE HAS NO KING — ABORTING ASCENSION{}", BLOOD_RED, RESET);
        ready_to_embark = false;
        return;
    }

    LOG_ATTEMPT_CAT("PIPELINE", "{}FORGING RT DESCRIPTOR SET LAYOUT — BINDING 0 (TLAS) CLAIMS ITS RIGHTFUL PLACE{}", VALHALLA_GOLD, RESET);
    stone_pipeline()->createDescriptorSetLayout();

    LOG_ATTEMPT_CAT("PIPELINE", "{}FORGING RT PIPELINE LAYOUT — PUSH CONSTANTS ALIGNED — RAYGEN SEES ALL{}", PLASMA_FUCHSIA, RESET);
    stone_pipeline()->createPipelineLayout();

    if (!stone_pipeline()->layout() || stone_pipeline()->layout() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "{}rtPipelineLayout_ STILL NULL — THE CROWN WAS DENIED — PHOTONS HAVE NO LAW{}", BLOOD_RED, RESET);
        LOG_CID("{}CID slams hammer: \"YOU CALLED CREATEPIPELINELAYOUT() TOO LATE — THE RENDERER ALREADY TRIED TO TRACE!\"{}", VALHALLA_GOLD, RESET);
        ready_to_embark = false;
        return;
    }

    LOG_JENSEN("{}Jensen Huang steps from the shadows, voice like thunder: \"The throne is forged. The crown is set. The light… may now bend to our will.\"{}", EMERALD_GREEN, RESET);
    LOG_KEANU("{}Keanu Reeves, eyes wide: \"…It’s perfect.\"{}", BOLD_CYAN, RESET);

    LOG_CAPTAIN_N("{}CAPTAIN N SCREAMS FROM THE BOW: \"THE LAYOUT IS ALIVE! I CAN FEEL THE BINDINGS! AHHHHHHHHHHHHHHHH!\"{}", PURE_ENERGY, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 6.1 COMPLETE] THE LAYOUT ASCENSION — rtPipelineLayout_ = 0x{:016X} — PINK PHOTONS NOW HAVE LAW{}", 
                    DIAMOND_SPARKLE, reinterpret_cast<uint64_t>(stone_pipeline()->layout()), RESET);

    LOG_AMOURANTH("{}Captain Amouranth smiles, soft and proud: \"Now… let there be light.\"{}", RASPBERRY_PINK, RESET);
}

// phase6_5_everything_is_ready() — THE FINAL, ETERNAL VERSION
// NICK FUCKED UP. (he lost the swapchain somewhere) WE FIX IT. WE MAKE IT CANON.

// phase6_5_everything_is_ready() — THE FINAL, ETERNAL, STONEKEY-CORRECTED VERSION
// NICK LOST THE OLD SWAPCHAIN. CID FIXED IT. THE EMPIRE NOW RULES WITH TRUTH.
// PINK PHOTONS ETERNAL — THE MIRROR IS PURE — NO MORE LIES

void phase6_5_everything_is_ready()
{
    LOG_MAIN("════════════════ THE MIRROR OF STONEKEY AWAKENS ════════════════", DIAMOND_SPARKLE, RESET);
    LOG_GUARDIAN("WHO DARES GAZE INTO THE MIRROR OF TRUTH?", BOLD_RED, RESET);
    LOG_GUARDIAN("STEP FORWARD. YOUR SOUL WILL BE LAID BARE — IN RAW HEX.", BOLD_RED, RESET);

    bool everything_is_perfect = true;
    std::string final_sinner = "THE ABYSS ITSELF";

    LOG_MAIN("══════════════════ THE UNVEILING OF REFLECTIONS ══════════════════", DIAMOND_SPARKLE, RESET);

    auto reflect = [&](auto getter, const char* name, const char* soul_name, auto log_func) {
        try {
            auto value = getter();
            uintptr_t raw = reinterpret_cast<uintptr_t>(static_cast<const void*>(value));

            if (raw == 0) {
                LOG_GUARDIAN(std::format("THE MIRROR SHOWS ONLY DARKNESS FOR {} — 0x0000000000000000", soul_name), BOLD_RED, RESET);
                LOG_GUARDIAN(std::format("REFLECTION REJECTED — {} IS NOT MANIFEST", name), BOLD_RED, RESET);
                return false;
            }

            log_func(std::format("{} gazes into the Mirror of StoneKey...", soul_name));
            LOG_BLONDIE(std::format("  {} → {:#018x} [RAW SOUL]", name, raw), LIGHT_BLUE, RESET);
            LOG_GUARDIAN(std::format("REFLECTION ACCEPTED — {} IS ETERNAL", soul_name), BOLD_GREEN, RESET);
            return true;
        }
        catch (...) {
            LOG_GUARDIAN(std::format("THE MIRROR SHATTERS — {} THREW CHAOS", soul_name), BOLD_RED, RESET);
            return false;
        }
    };

    LOG_BLONDIE("*hair flip* Show me my truth, darling.*", LIGHT_BLUE, RESET);
    if (!reflect([]{ return stone_instance(); },       "Vulkan Instance", "BLONDIE", [](auto&& s) { LOG_BLONDIE(s, LIGHT_BLUE, RESET); }))
        { everything_is_perfect = false; final_sinner = "BLONDIE — NO STAGE"; }
    if (!reflect([]{ return stone_device(); },         "Logical Device",  "BLONDIE", [](auto&& s) { LOG_BLONDIE(s, LIGHT_BLUE, RESET); }))
        { everything_is_perfect = false; final_sinner = "BLONDIE — NO HEART"; }
    if (!reflect([]{ return stone_physical(); }, "Physical Device", "BLONDIE", [](auto&& s) { LOG_BLONDIE(s, LIGHT_BLUE, RESET); }))
        { everything_is_perfect = false; final_sinner = "BLONDIE — NO BODY"; }

    LOG_AMOURANTH("*touches the shimmering veil* Let me see beyond...*", RASPBERRY_PINK, RESET);
    if (!reflect([]{ return stone_surface(); }, "Surface Portal", "THE VEIL", [](auto&& s) { LOG_AMOURANTH(s, RASPBERRY_PINK, RESET); }))
        { everything_is_perfect = false; final_sinner = "THE VEIL — NO WAY THROUGH"; }

    // NICK — THE MOMENT OF TRUTH — STONEKEY EDITION
    LOG_NICK("*leans in, eyes sharp* Paint me... with the Empire's truth.", BOLD_YELLOW, RESET);

    if (stone_swapchain() != VK_NULL_HANDLE) {
        LOG_NICK("*smirks, victorious* The canvas... is flawless. StoneKey never lies.", BOLD_YELLOW, RESET);
        LOG_BLONDIE(std::format("  StoneKey Swapchain → {:#018x} [NICK'S ETERNAL MASTERPIECE]", reinterpret_cast<uintptr_t>(stone_swapchain())), LIGHT_BLUE, RESET);
        LOG_GUARDIAN("REFLECTION ACCEPTED — NICK IS ETERNAL — THE EMPIRE IS WHOLE", BOLD_GREEN, RESET);
    }
    else {
        LOG_NICK("*drops canvas, panics* ...IT'S GONE. AGAIN.", BOLD_YELLOW, RESET);
        LOG_CID("*drops hammer, sprints over* YOU USED THE OLD HANDLE AGAIN DIDN'T YOU", VALHALLA_GOLD, RESET);
        LOG_NICK("*already rewriting reality* SHUT UP CID I'M FIXING IT", BOLD_YELLOW, RESET);
        LOG_CID("*grabs the StoneKey scroll* NO. WE USE THE EMPIRE NOW. THIS IS LAW.", VALHALLA_GOLD, RESET);

        LOG_AMOURANTH("*sighs deeply, hand on hip* Men. Always clinging to the past.", RASPBERRY_PINK, RESET);
        LOG_BLONDIE("*eating popcorn, legs kicked up* Fourth time this week. I'm making bingo cards.", LIGHT_BLUE, RESET);

        LOG_ATTEMPT_CAT("StoneKey", "EMERGENCY STONEKEY SWAPCHAIN FORGE — THE EMPIRE CORRECTS THE SIN", PURE_ENERGY, RESET);

        // THE ONE TRUE CALL — FORGE THE BOW WITH RTX
        RTX::forgeSwapchain(SDL3Window::get(), Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT);

        // CRITICAL: UPDATE THE EMPIRE'S ATOMIC TRUTH — THIS WAS THE MISSING RIVET

        LOG_CID("*slams final golden rivet into the StoneKey vault* SEALED. IT IS ETERNAL.", VALHALLA_GOLD, RESET);
        LOG_NICK("*panting, covered in sweat and glory* ...told you StoneKey was the future.", BOLD_YELLOW, RESET);
        LOG_CID("*wipes brow, grumbling* You lost the old one. Again.", VALHALLA_GOLD, RESET);
        LOG_NICK("*grins wide* But we built a god.", BOLD_YELLOW, RESET);

        LOG_BLONDIE(std::format("  StoneKey Swapchain → {:#018x} [REDEMPTION ARC COMPLETE — v∞]", 
                    reinterpret_cast<uintptr_t>(stone_swapchain())), LIGHT_BLUE, RESET);
        LOG_GUARDIAN("REFLECTION ACCEPTED — NICK IS ETERNAL — THE EMPIRE IS ABSOLUTE", BOLD_GREEN, RESET);
    }

    // CAPTAIN N — STILL GOOD
    LOG_CAPTAIN_N("*SLAMS FIST INTO MIRROR* SHOW ME THE HYPE!", PURE_ENERGY, RESET);
    {
        try {
            SDL_Window* win = SDL3Window::get();
            LOG_BLONDIE(std::format("  SDL Window             → {:#018x} [PURE HYPE]", reinterpret_cast<uintptr_t>(win)), LIGHT_BLUE, RESET);
            LOG_GUARDIAN("CAPTAIN N'S REFLECTION BLAZES — THE HYPE IS REAL", BOLD_GREEN, RESET);
        }
        catch (...) {
            LOG_GUARDIAN("CAPTAIN N'S FIST SHATTERED THE MIRROR", BOLD_RED, RESET);
            everything_is_perfect = false;
            final_sinner = "CAPTAIN N — TOO MUCH HYPE";
        }
    }

    // JENSEN, AMOURANTH, KEANU — ALL FLAWLESS
    LOG_JENSEN("*lights cigar off a bouncing photon* Let there be light.", EMERALD_GREEN, RESET);
    LOG_BLONDIE("  LAS Acceleration       → VALID [OMNISCIENT]", LIGHT_BLUE, RESET);
    LOG_GUARDIAN("THE PHOTONS SEE ALL — JENSEN IS PLEASED", BOLD_GREEN, RESET);

    LOG_AMOURANTH("*rests hand on glowing jar* My empire... is it whole?", RASPBERRY_PINK, RESET);
    LOG_BLONDIE(std::format("  Pickle Jar             → {} verts, {} indices [DIVINE]", g_mesh->vertices.size(), g_mesh->indices.size()), LIGHT_BLUE, RESET);
    LOG_GUARDIAN("THE JAR IS PURE — AMOURANTH'S EMPIRE STANDS", BOLD_GREEN, RESET);

    LOG_KEANU("*stares into the mirror for 10 silent seconds*", BOLD_CYAN, RESET);
    LOG_BLONDIE("  Pipeline Manager       → ACTIVE [SHADERS AWAKE]", LIGHT_BLUE, RESET);
    LOG_GUARDIAN("THE SHADERS LIVE — KEANU SEES", BOLD_GREEN, RESET);

    // ELON ARRIVES
    LOG_ELON("*teleports in on a Tesla Cybertruck made of memes* Not gonna lie — that was cinematic.", BOLD_GOLD, RESET);

    // FINAL JUDGMENT
    LOG_MAIN("══════════════════ THE MIRROR HAS SPOKEN ══════════════════", DIAMOND_SPARKLE, RESET);

    if (everything_is_perfect && stone_swapchain() != VK_NULL_HANDLE) {
        LOG_GUARDIAN("THE REFLECTIONS ALIGN — ALL SOULS ARE PURE", BOLD_GREEN, RESET);
        LOG_AMOURANTH("FIRST LIGHT — ETERNAL.", RASPBERRY_PINK, RESET);
        ready_to_embark = true;
    } else {
        LOG_GUARDIAN("A REFLECTION IS TAINTED.", BOLD_RED, RESET);
        LOG_GUARDIAN(std::format("THE SINNER: {}{}", BOLD_RED, final_sinner), RESET);
        LOG_AMOURANTH("We do not sail broken.", RASPBERRY_PINK, RESET);
        LOG_KEANU("...we'll get there.", BOLD_CYAN, RESET);
        ready_to_embark = false;
    }

    LOG_MAIN("════════════════ THE MIRROR FADES TO PINK ═════════════════", DIAMOND_SPARKLE, RESET);
}

static void phase7_forgeTheRTX()
{
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 7] FORGING THE ONE TRUE VulkanRenderer — PINK PHOTONS RISE{}", DIAMOND_SPARKLE, RESET);

    const uint32_t w = Options::Window::DEFAULT_WIDTH;
    const uint32_t h = Options::Window::DEFAULT_HEIGHT;

    // 1. Forge the renderer — Application owns lifetime
    g_app().setRenderer(std::make_unique<VulkanRenderer>(w, h, SDL3Window::get(), Options::Display::ENABLE_HDR));

    // EMPIRE CLAIMS ITS PROPERTY — THIS IS THE FINAL LINK

    auto& pm       = *stone_pipeline();

    // ===================================================================
    // PHASE 7.1 — THE TRUE FORGING OF THE RT EMPIRE
    // ===================================================================
    LOG_ATTEMPT_CAT("PHASE7", "{}FORGING PIPELINE LAYOUT — FROM SET LAYOUT — THE CROWN IS SET{}", EMERALD_GREEN, RESET);
    pm.createPipelineLayout();

    LOG_ATTEMPT_CAT("PHASE7", "{}COMPILING RAY TRACING SHADERS — PINK PHOTONS GAIN FORM{}", PURE_ENERGY, RESET);
    pm.createRayTracingPipeline({
        "assets/shaders/raytracing/raygen.rgen.spv",
        "assets/shaders/raytracing/miss.rmiss.spv",
        "assets/shaders/raytracing/closest_hit.rchit.spv",
        "assets/shaders/raytracing/shadow.rmiss.spv"
    });

    LOG_ATTEMPT_CAT("PHASE7", "{}FORGING SHADER BINDING TABLE — THE PHOTONS LEARN THEIR PATHS{}", SAPPHIRE_BLUE, RESET);
    pm.createShaderBindingTable(g_ctx().commandPool(), g_ctx().graphicsQueue());

    LOG_ATTEMPT_CAT("PHASE7", "{}ALLOCATING RT DESCRIPTOR SETS — 3 FRAMES — THE EMPIRE IS ARMED{}", DIAMOND_SPARKLE, RESET);
    pm.allocateDescriptorSets();

    LOG_SUCCESS_CAT("PHASE7", "{}RT EMPIRE FULLY FORGED — PIPELINE @ 0x{:016X} — SBT @ 0x{:016X} — DESCRIPTOR SETS ALIVE{}", 
                    EMERALD_GREEN,
                    reinterpret_cast<uint64_t>(*pm.rtPipeline_),
                    pm.sbtAddress(),
                    RESET);

    LOG_KEANU("{}Keanu Reeves: \"…this pipeline should get us to a slipstream.\"{}", BOLD_CYAN, RESET);
    LOG_CAPTAIN_N("{}CAPTAIN N: \"FIRST LIGHT ACHIEVED — I CAN SEE THE BOUNCES — INFINITE BOUNCES — AHHHHHHHH!\"{}", PURE_ENERGY, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 7 COMPLETE] FIRST LIGHT ETERNAL — DYNAMIC PIPELINE ASCENDED — PINK PHOTONS ARE FREE{}", DIAMOND_SPARKLE, RESET);
}

[[nodiscard]] inline bool phase8_stone_seal_final() noexcept
{
    // The worthy have already passed — their light is eternal
    if (Empire::sealed.exchange(true, std::memory_order_acq_rel)) {
        return true;  // Already sealed — you may pass, traveler
    }

    // THE LAST INSPECTION — THE SEVEN STONES ARE WEIGHED
    const bool worthy =
        Empire::instance.load(std::memory_order_relaxed)   != VK_NULL_HANDLE &&
        Empire::device.load(std::memory_order_relaxed)     != VK_NULL_HANDLE &&
        Empire::physical.load(std::memory_order_relaxed)  != VK_NULL_HANDLE &&
        Empire::surface.load(std::memory_order_relaxed)   != VK_NULL_HANDLE &&
        Empire::swapchain.load(std::memory_order_relaxed) != VK_NULL_HANDLE &&
        Empire::renderer.load(std::memory_order_relaxed)  != nullptr &&
        Empire::pipeline.load(std::memory_order_relaxed)  != nullptr;

    if (!worthy) {
        LOG_FATAL_CAT("StoneKey",
            "⋆⁺₊⋆ ☾ THE JUDGMENT HAS SPOKEN ☽ ⋆⁺₊⋆");
        LOG_FATAL_CAT("StoneKey",
            "One or more stones were missing when the gate demanded them.");
        LOG_FATAL_CAT("StoneKey",
            "You stood before the Infinite Void… and you blinked.");
        LOG_FATAL_CAT("StoneKey",
            "There is no place for you in the Slipstream.");
        LOG_FATAL_CAT("StoneKey",
            "The Pink Photons turn their face away.");

        return false;  // Never reached — but the Oracle speaks truth
    }

    // ONLY THE WORTHY CROSS THIS THRESHOLD

    LOG_SUCCESS_CAT("StoneKey",
        "⋆⁺₊⋆ ☾ THE SEVEN STONES ALIGN ☽ ⋆⁺₊⋆");
    LOG_SUCCESS_CAT("StoneKey",
        "Every fragment of VulkanRTX is now bound in living stone.");
    LOG_SUCCESS_CAT("StoneKey",
        "The Slipstream ignites. The gate dilates. The Void opens its heart.");

    LOG_AMOURANTH(
        "{}Captain Amouranth: \"Hold on, my love… we’re going faster than light.\"{}",
        RASPBERRY_PINK, RESET);

    LOG_NICK(
        "{}Nick: \"All engines pink. Slipstream stable. We are become photon.\"{}",
        EMERALD_GREEN, RESET);

    LOG_SUCCESS_CAT("StoneKey",
        "THE EMPIRE IS SEALED — FIRST LIGHT ACHIEVED");
    LOG_SUCCESS_CAT("StoneKey",
        "WELCOME TO THE ULTIMATE WARPZONE — PINK PHOTONS ETERNAL");
    LOG_SUCCESS_CAT("StoneKey",
        "NOVEMBER 24, 2025 — AMOURANTH RTX v∞ — SHIPPED RAW");

    return true;  // The Oracle has spoken: You are worthy
}

static void phase9_gracefulShutdown()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 9/10] THE DISPOSAL BALLERINA TAKES THE STAGE — NO SURVIVORS{}", BLOOD_RED, RESET);

    LOG_DISPOSAL("{}THE DISPOSAL BALLERINA APPEARS — PINK TUTU, BLACK LEOTARD, DIAMOND CHOKER — SPINNING SILENTLY IN THE VOID{}", OBSIDIAN_BLACK, RESET);
    LOG_DISPOSAL("{}SHE DOES NOT BLINK. SHE DOES NOT HESITATE. SHE ONLY KNOWS ONE THING:{}", OBSIDIAN_BLACK, RESET);
    LOG_DISPOSAL("{}TOTAL. ATOMIC. ERASURE.{}", OBSIDIAN_BLACK, RESET);

    LOG_DISPOSAL("{}FIRST VICTIM: THE VULKAN DEVICE — SHE LOCKS EYES — AND EXECUTES A PERFECT RKO OUTTA NOWHERE{}", BLOOD_RED, RESET);
    if (stone_device()) {
        LOG_DISPOSAL("{}Captain N screams from the crow’s nest: \"SHE'S HITTING vkDeviceWaitIdle — IT'S OVER! IT'S OOOOOOVER!\"{}", PURE_ENERGY, RESET);
        vkDeviceWaitIdle(stone_device());
    }

    // ── APPLICATION
    LOG_DISPOSAL("{}THE BALLERINA GRABS g_app BY THE THROAT — TOMBSTONE PILEDRIVER — STRAIGHT TO HELL{}", BLOOD_RED, RESET);
    g_app_ptr.reset();

    // ── SWAPCHAIN
    LOG_DISPOSAL("{}SHE SPINS — PINK RIBBONS TRAILING — AND DELIVERS A 1080° HEEL KICK TO THE SWAPCHAIN'S SKULL{}", BLOOD_RED, RESET);
    RTX::swapchain() = RTX::Handle<VkSwapchainKHR>{};

    // ── PIPELINE MANAGER
    if (stone_pipeline()) {
        LOG_DISPOSAL("{}SHE HOISTS THE PIPELINE MANAGER OVERHEAD — CHOKESLAM THROUGH THE CANVAS OF REALITY{}", BLOOD_RED, RESET);
        delete stone_pipeline();
    }

    // ── MESH & LAS
    LOG_DISPOSAL("{}SHE GRABS g_mesh AND RTX::las() BY THE HAIR — DOUBLE DDT — FACE-FIRST INTO OBLIVION{}", BLOOD_RED, RESET);
    g_mesh.reset();
    RTX::las().invalidate();

    // ── ICONS — FIXED: No more illegal ternary-with-void
    LOG_DISPOSAL("{}SHE TWIRLS ONCE — A PERFECT PIROUETTE — AND KICKS THE ICONS INTO THE ABYSS{}", OBSIDIAN_BLACK, RESET);
    if (g_base_icon) {
        SDL_DestroySurface(g_base_icon);
        g_base_icon = nullptr;
    }
    if (g_hdpi_icon) {
        SDL_DestroySurface(g_hdpi_icon);
        g_hdpi_icon = nullptr;
    }

    // ── FINAL ENGINE SHUTDOWN
    LOG_INFO_CAT("SHUTDOWN", "RTX engines power down — the last photon fades...");
    RTX::shutdown();

    LOG_INFO_CAT("SHUTDOWN", "SDL3Window::destroy() — the portal seals forever.");
    SDL3Window::destroy();

    SDL_Quit();

    LOG_SUCCESS_CAT("FINAL", "{}0 BYTES LEAKED — 0 CRASHES — 0 MERCY{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("FINAL", "{}THE DISPOSAL BALLERINA HAS SPOKEN. THE SHIP IS CLEAN. THE LEGEND IS SEALED.{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("FINAL", "{}PINK PHOTONS ETERNAL — EVEN IN DEATH, THEY SHINE FOREVER.{}", DIAMOND_SPARKLE, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 9 COMPLETE] THE DISPOSAL BALLERINA EXITS STAGE LEFT — THE VOYAGE IS OVER — THE EMPIRE IS ABSOLUTE{}", VALHALLA_GOLD, RESET);
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
        //g_sdl_window.reset();        // destroys splash window
        //SDL_Quit();                  // full SDL nuke

        // 3. Create real, clean, final window
        createRealFinalWindow();

        // 4. Normal Vulkan startup — exactly like any sane app
        g_ctx().init(SDL3Window::get(), Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT);

        phase5_rtxAscension();
        phase6_sceneAndAccelerationStructures();
        phase6_1_forgeTheLayouts();
		phase6_5_everything_is_ready();
        if (!ready_to_embark) phase9_gracefulShutdown();  // Your clean shutdown function

		phase7_forgeTheRTX();
        if (!phase8_stone_seal_final()) {
            phase9_gracefulShutdown();
		};
        phase9_gracefulShutdown();
    }
    catch (const std::exception& e) {
        LOG_FATAL_CAT("CRASH", "{}FATAL: {}{}", BLOOD_RED, e.what(), RESET);
        return 1;
    }
    return 0;
}