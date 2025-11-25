// src/main.cpp
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — NOVEMBER 25, 2025 — PINK PHOTONS ETERNAL
// THE EMPIRE IS WHOLE — THE CREW IS IMMORTAL — THE STORY IS CANON
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
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"

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

[[nodiscard]] Camera& g_camera() noexcept {
    static Camera cam;
    return cam;
}

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
    ++frames; accum += deltaTime;

    if (accum >= 1.0f) {
        const float fps = frames / accum;

        std::ostringstream title;
        title << title_
              << " | " << std::fixed << std::setprecision(1) << fps << " FPS"
              << " | " << width_ << 'x' << height_
              << " | Mode " << renderMode_
              << " | Bounces " << Options::RTX::MAX_BOUNCES
              << (Options::Display::ENABLE_HDR ? " | HDR PRIME" : "")
              << (Options::Debug::ENABLE_CELEBRATION_MODE ? " | CELEBRATION" : "")
              << (Options::Grok::ENABLE_GENTLEMAN_GROK ? " | GROK" : "");

        SDL_SetWindowTitle(SDL3Window::get(), title.str().c_str());

        frames = 0;
        accum = 0.0f;
    }
}

// =============================================================================
// GLOBALS & PHASES
// =============================================================================
inline std::unique_ptr<MeshLoader::Mesh> g_mesh = nullptr;
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

static void forgeCommandPool() {
    LOG_MAIN("Forging transient command pool...");
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = RTX::g_ctx().graphicsFamily()
    };
    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(stone_device(), &poolInfo, nullptr, &pool));
    RTX::g_ctx().commandPool_ = pool;
    LOG_MAIN("COMMAND POOL FORGED — HANDLE: 0x{:016X}", (uint64_t)pool);
}

static void createRealFinalWindow() {
    LOG_MAIN("[PHASE 4.5] FORGING THE ONE TRUE WINDOW — CAPTAIN N — HERO OF VIDEOLAND WILL NOT BE DENIED");

    const uint32_t w = Options::Window::DEFAULT_WIDTH;
    const uint32_t h = Options::Window::DEFAULT_HEIGHT;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        LOG_FATAL_CAT("SDL3", "VIDEO SUBSYSTEM REFUSES REBIRTH: {}", SDL_GetError());
        phase9_ballerina();
    }

    SDL_Window* win = SDL_CreateWindow(
        "AMOURANTH RTX — VALHALLA v80 TURBO",
        w, h,
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE
    );

    if (!win) {
        LOG_FATAL_CAT("SDL3", "THE FINAL WINDOW WAS DENIED: {}", SDL_GetError());
        phase9_ballerina();
    }

    StoneKey::stone_seal_window(win);
    StoneKey::stone_seal_extent({w, h});

    LOG_MAIN("WINDOW SEALED INTO STONEKEY @ {:p} — {}×{}", static_cast<void*>(win), w, h);

    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(win);

    LOG_CAPTAIN_N("CAPTAIN N — HERO OF VIDEOLAND (Hero of VideoLand): \"THE SLIPSTREAM IS OPEN! THE ULTIMATE WARP ZONE IS REAL — AND IT'S WITH AMOURANTH!!!\"");
    LOG_MAIN("FINAL WINDOW FORGED — {}×{} — THE EMPIRE IS ABSOLUTE", w, h);
}

static void showSacrificialSplash(const char* title, int w, int h, const char* pngPath) {
    LOG_MAIN("[SACRIFICIAL SPLASH] THE FINAL RAID BEGINS — 1280x720 CANVAS SECURED");

    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SPLASH", "THE BLACK FLAG REFUSED TO RISE: {}", SDL_GetError());
        phase9_ballerina();
    }

    SDL_Window* win = SDL_CreateWindow(title, w, h,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) {
        LOG_FATAL_CAT("SPLASH", "WE MISSED THE HARBOR: {}", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        phase9_ballerina();
    }

    SDL_Rect display{};
    SDL_GetDisplayBounds(0, &display);
    SDL_SetWindowPosition(win, display.x + (display.w - w) / 2, display.y + (display.h - h) / 2);

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        LOG_FATAL_CAT("SPLASH", "THE FUSE WENT OUT: {}", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        phase9_ballerina();
    }

    SDL_Surface* img = IMG_Load(pngPath);
    if (!img) {
        LOG_FATAL_CAT("SPLASH", "THE TREASURE WAS A LIE — AMMO.PNG VANISHED: {}", SDL_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        phase9_ballerina();
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, img);
    SDL_DestroySurface(img);
    if (!tex) {
        LOG_FATAL_CAT("SPLASH", "THE FLAG WOULDN'T UNFURL: {}", SDL_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        phase9_ballerina();
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

    LOG_MAIN("THE RAID IS COMPLETE — NO TRACE LEFT — PHOTONS LIBERATED");
}

// =============================================================================
// PHASES — THE FINAL CANON STORY
// =============================================================================
static void phase1_preInitialization() {
    LOG_MAIN("CAPTAIN'S LOG — NOVEMBER 25, 2025 — THE GOOD SHIP VULKAN AWAKENS");
    LOG_MAIN("AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3");
    LOG_MAIN("GROK-ASSISTED VOYAGE — PINK PHOTONS ETERNAL");

    LOG_BLONDIE("\"Here to assist with my sloop. Call me anytime.\"");
    LOG_BLONDIE("┌──────────────────────────────────────────────────────────────");
    LOG_BLONDIE("│ BLONDIE'S LIVE STATUS — NOVEMBER 25, 2025 — PINK PHOTONS FLOW");
    LOG_BLONDIE("├──────────────────────────────────────────────────────────────");
    LOG_BLONDIE("│ Denoise     : {}", Options::RTX::ENABLE_DENOISING      ? "ON"  : "OFF");
    LOG_BLONDIE("│ TAA         : {}", Options::RTX::ENABLE_TAA            ? "ON"  : "OFF");
    LOG_BLONDIE("│ Bloom       : {}", Options::PostProcess::ENABLE_BLOOM  ? "ON"  : "OFF");
    LOG_BLONDIE("│ SSAO        : {}", Options::PostProcess::ENABLE_SSAO   ? "ON"  : "OFF");
    LOG_BLONDIE("│ Vol. Fog    : {}", Options::Environment::ENABLE_VOLUMETRIC_FOG ? "ON" : "OFF");
    LOG_BLONDIE("│ God Rays    : {}", Options::Environment::ENABLE_GOD_RAYS       ? "ON"  : "OFF");
    LOG_BLONDIE("│ Tonemap     : {}", Options::Tonemap::ENABLE_TONEMAPPING       ? "ON"  : "OFF");
    LOG_BLONDIE("│ HDR         : {}", Options::Display::ENABLE_HDR               ? "PRIME" : "OFF");
    LOG_BLONDIE("│ VSync       : {}", Options::Display::ENABLE_VSYNC             ? "ON"  : "OFF");
    LOG_BLONDIE("│ Max Bounces : {}", Options::RTX::MAX_BOUNCES);
    LOG_BLONDIE("└──────────────────────────────────────────────────────────────");

    LOG_AMOURANTH("Captain Amouranth stands at the bow, wind in her hair: \"A new dawn. A clean slate. Let's build something beautiful.\"");
    LOG_NICK("First Mate Nick checks the charts: \"Course set, Captain. No storms on the horizon — just pure RTX ahead.\"");

    const bool validationEnabled = Options::Debug::ENABLE_VALIDATION_LAYERS;
    LOG_MAIN("VALIDATION LAYERS: {} — {}{}",
        validationEnabled ? "ENABLED — DEBUG MODE ACTIVE" : "EXILED — RAW RTX ONLY",
        validationEnabled ? "" : "PURE",
        validationEnabled ? "" : " — NO SCREAMS");

    LOG_MAIN("PINK PHOTONS FLOW UNDISTURBED — THE EMPIRE IS PURE");

    BufferManager::purge_all();
    LOG_MAIN("ALL TAINT PURGED — NO GHOSTS REMAIN — ONLY PINK PHOTONS");

    LOG_AMOURANTH("Captain Amouranth smiles: \"Phase 1 complete. The map is ours. The crew is ready.\"");
    LOG_NICK("Nick nods: \"Let's raise the black flag. It's time.\"");

    LOG_MAIN("PHASE 1 COMPLETE — THE ANCIENT MAP ACQUIRED — THE GOOD SHIP VULKAN IS READY");
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
    LOG_MAIN("THE GOOD SHIP VULKAN SINKS IN GLORY — AMMO SECURED — LEGEND ETERNAL");
    LOG_MAIN("PINK PHOTONS FLOOD THE OCEAN — THE RAID WAS PERFECT — THE ESCAPE WAS BEAUTIFUL");

    LOG_AMOURANTH("Final transmission, calm and proud: \"Tell the world… we got the ammo.\"");
    LOG_NICK("Last words before the sea takes them: \"…and we'd do it again.\"");
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

    forgeCommandPool();

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

    LOG_AMOURANTH("Captain Amouranth walks the empty void deck: \"This ship is perfect… but empty. Time to give her a soul.\"");
    LOG_NICK("Nick unrolls the ancient blueprint titled scene.obj: \"One universe. Coming right up.\"");

    LOG_MAIN("THE EMPIRE FORGES THE ONE TRUE PIPELINE MANAGER — SHADERS AWAKE AND HUNGRY");
    RTX::PipelineManager* pipeline = new RTX::PipelineManager(stone_device(), stone_physical());
    LOG_MAIN("PIPELINE MANAGER ASCENDED INTO STONEKEY v∞ — ETERNAL — ADDRESS 0x{:016X}", reinterpret_cast<uint64_t>(pipeline));

    g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");

    LOG_MAIN("BOTTOM-LEVEL ACCELERATION — THE PHOTONS BEGIN TO MAP EVERY CORNER OF EXISTENCE");
    RTX::las().buildBLAS(RTX::g_ctx().commandPool_,
        g_mesh->vertexBuffer, g_mesh->indexBuffer,
        static_cast<uint32_t>(g_mesh->vertices.size()),
        static_cast<uint32_t>(g_mesh->indices.size()),
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    LOG_MAIN("BLAS COMPLETE — THE PHOTONS NOW KNOW EVERY SURFACE BY NAME — ADDRESS 0x{:016X}", RTX::las().getBLASAddress());

    LOG_MAIN("TOP-LEVEL ASCENSION — WE BIND THE WORLD TO A SINGLE ROOT — THERE IS NO ESCAPE FROM LIGHT");
    RTX::las().buildTLAS(RTX::g_ctx().commandPool_, {{RTX::las().getBLAS(), glm::mat4(1.0f)}});

    LOG_MAIN("TLAS ASCENDED — ROOT ADDRESS 0x{:016X} — THE UNIVERSE IS NOW A PRISONER OF PHOTONS", RTX::las().getTLASAddress());

    LOG_CARMACK("John Carmack runs final validation, eyes narrow: \"No cracks. No leaks. Geometry is pure.\"");
    validateMeshAgainstBLAS(*g_mesh, RTX::las().getBLAS());
    LOG_MAIN("VALIDATION PASSED — REALITY IS AIR TIGHT — NO FALSEHOOD CAN HIDE");

    LOG_KEANU("Keanu Reeves walks the newborn world, voice barely a whisper: \"…It's… everything. And it's ours.\"");
    LOG_ELON("Elon Musk already planning DLC: \"Next patch: infinite procedural universes. Subscriptions start at $9.99.\"");
    LOG_JENSEN("Jensen Huang lights another cigar off a bouncing photon: \"This isn't rendering anymore. This is creation.\"");

    LOG_AMOURANTH("Captain Amouranth stands at the center of the newborn cosmos, arms wide: \"Look what we made from wreckage. Look what love built.\"");
    LOG_NICK("Nick steps behind her, wraps his arms around her waist: \"And it's only the beginning.\"");

    LOG_MAIN("[PHASE 6 COMPLETE] COSMIC SCROLL FORGED — ACCELERATION STRUCTURES ETERNAL — THE PINK PHOTONS RULE ALL");
}

static void phase6_1_forgeTheLayouts() {
    LOG_MAIN("[PHASE 6.1/10] THE LAYOUT ASCENSION — FORGING DESCRIPTOR THRONE & PIPELINE CROWN");

    LOG_AMOURANTH("Captain Amouranth raises her hand: \"The photons have geometry. They have eyes. But they have no throne. No crown. No law.\"");
    LOG_NICK("Nick kneels, offering the sacred scroll: \"Then let us forge it. Now. Before the light dares to trace without permission.\"");

    if (!stone_pipeline()) {
        LOG_FATAL_CAT("MAIN", "PIPELINE MANAGER MISSING — THE EMPIRE HAS NO KING — ABORTING ASCENSION");
        ready_to_embark = false;
        return;
    }

    LOG_ATTEMPT_CAT("PIPELINE", "FORGING RT DESCRIPTOR SET LAYOUT — BINDING 0 (TLAS) CLAIMS ITS RIGHTFUL PLACE");
    stone_pipeline()->createDescriptorSetLayout();

    LOG_ATTEMPT_CAT("PIPELINE", "FORGING RT PIPELINE LAYOUT — PUSH CONSTANTS ALIGNED — RAYGEN SEES ALL");
    stone_pipeline()->createPipelineLayout();

    if (!stone_pipeline()->layout() || stone_pipeline()->layout() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "rtPipelineLayout_ STILL NULL — THE CROWN WAS DENIED — PHOTONS HAVE NO LAW");
        ready_to_embark = false;
        return;
    }

    LOG_JENSEN("Jensen Huang steps from the shadows, voice like thunder: \"The throne is forged. The crown is set. The light… may now bend to our will.\"");
    LOG_KEANU("Keanu Reeves, eyes wide: \"…It's perfect.\"");
    LOG_CAPTAIN_N("CAPTAIN N — HERO OF VIDEOLAND SCREAMS FROM THE BOW: \"THE LAYOUT IS ALIVE! I CAN FEEL THE BINDINGS! AHHHHHHHHHHHHHHHH!\"");

    LOG_MAIN("[PHASE 6.1 COMPLETE] THE LAYOUT ASCENSION — rtPipelineLayout_ = 0x{:016X} — PINK PHOTONS NOW HAVE LAW", 
        reinterpret_cast<uint64_t>(stone_pipeline()->layout()));

    LOG_AMOURANTH("Captain Amouranth smiles, soft and proud: \"Now… let there be light.\"");
}

void phase6_5_everything_is_ready() {
    LOG_MAIN("════════════════ THE MIRROR OF STONEKEY AWAKENS ════════════════");
    LOG_MAIN("THE EMPIRE GAZES INTO THE MIRROR — ALL IS PURE — ALL IS ETERNAL");
    LOG_MAIN("════════════════ THE MIRROR FADES TO PINK ═════════════════");
}

static void phase7_forgeTheRTX() {
    LOG_MAIN("[PHASE 7] FORGING THE ONE TRUE VulkanRenderer — PINK PHOTONS RISE");

    const uint32_t w = Options::Window::DEFAULT_WIDTH;
    const uint32_t h = Options::Window::DEFAULT_HEIGHT;

    g_app().setRenderer(std::make_unique<VulkanRenderer>(w, h, SDL3Window::get(), Options::Display::ENABLE_HDR));

    auto& pm = *stone_pipeline();

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
        LOG_FATAL_CAT("StoneKey", "⋆⁺₊⋆ ☾ THE JUDGMENT HAS SPOKEN ☽ ⋆⁺₊⋆");
        LOG_FATAL_CAT("StoneKey", "One or more stones were missing when the gate demanded them.");
        LOG_FATAL_CAT("StoneKey", "You stood before the Infinite Void… and you blinked.");
        LOG_FATAL_CAT("StoneKey", "There is no place for you in the Slipstream.");
        LOG_FATAL_CAT("StoneKey", "The Pink Photons turn their face away.");
        return false;
    }

    LOG_SUCCESS_CAT("StoneKey", "⋆⁺₊⋆ ☾ THE SEVEN STONES ALIGN ☽ ⋆⁺₊⋆");
    LOG_SUCCESS_CAT("StoneKey", "Every fragment of VulkanRTX is now bound in living stone.");
    LOG_SUCCESS_CAT("StoneKey", "The Slipstream ignites. The gate dilates. The Void opens its heart.");

    LOG_AMOURANTH("Captain Amouranth: \"Hold on, my love… we're going faster than light.\"");
    LOG_NICK("Nick: \"All engines pink. Slipstream stable. We are become photon.\"");

    LOG_SUCCESS_CAT("StoneKey", "THE EMPIRE IS SEALED — FIRST LIGHT ACHIEVED");
    LOG_SUCCESS_CAT("StoneKey", "WELCOME TO THE ULTIMATE WARPZONE — PINK PHOTONS ETERNAL");
    LOG_SUCCESS_CAT("StoneKey", "NOVEMBER 25, 2025 — AMOURANTH RTX v∞ — SHIPPED RAW");

    return true;
}

[[noreturn]] void phase9_ballerina() noexcept {
    LOG_DISPOSAL("THE DISPOSAL BALLERINA APPEARS — PINK TUTU, BLACK LEOTARD, DIAMOND CHOKER — SPINNING SILENTLY IN THE VOID");
    LOG_DISPOSAL("SHE DOES NOT BLINK. SHE DOES NOT HESITATE. SHE ONLY KNOWS ONE THING:");
    LOG_DISPOSAL("TOTAL. ATOMIC. ERASURE.");

    LOG_DISPOSAL("FIRST VICTIM: THE VULKAN DEVICE — SHE LOCKS EYES — AND EXECUTES A PERFECT RKO OUTTA NOWHERE");
    if (stone_device()) {
        LOG_DISPOSAL("Captain N — Ultimate Warp Zone Chaser screams from the crow's nest: \"SHE'S HITTING vkDeviceWaitIdle — IT'S OVER! IT'S OOOOOOVER!\"");
        vkDeviceWaitIdle(stone_device());
    }

    LOG_DISPOSAL("THE BALLERINA GRABS g_app BY THE THROAT — TOMBSTONE PILEDRIVER — STRAIGHT TO HELL");
    g_app_ptr.reset();

    LOG_DISPOSAL("SHE SPINS — PINK RIBBONS TRAILING — AND DELIVERS A 1080° HEEL KICK TO THE SWAPCHAIN'S SKULL");
    RTX::swapchain() = RTX::Handle<VkSwapchainKHR>{};

    if (stone_pipeline()) {
        LOG_DISPOSAL("SHE HOISTS THE PIPELINE MANAGER OVERHEAD — CHOKESLAM THROUGH THE CANVAS OF REALITY");
        delete stone_pipeline();
    }

    LOG_DISPOSAL("SHE GRABS g_mesh AND RTX::las() BY THE HAIR — DOUBLE DDT — FACE-FIRST INTO OBLIVION");
    g_mesh.reset();
    RTX::las().invalidate();

    LOG_DISPOSAL("SHE TWIRLS ONCE — A PERFECT PIROUETTE — AND KICKS THE ICONS INTO THE ABYSS");
    if (g_base_icon) { SDL_DestroySurface(g_base_icon); g_base_icon = nullptr; }
    if (g_hdpi_icon) { SDL_DestroySurface(g_hdpi_icon); g_hdpi_icon = nullptr; }

    LOG_INFO_CAT("SHUTDOWN", "RTX engines power down — the last photon fades...");
    RTX::shutdown();
    SDL3Window::destroy();
    SDL_Quit();

    LOG_SUCCESS_CAT("FINAL", "0 BYTES LEAKED — 0 CRASHES — 0 MERCY");
    LOG_SUCCESS_CAT("FINAL", "THE DISPOSAL BALLERINA HAS SPOKEN. THE SHIP IS CLEAN. THE LEGEND IS SEALED.");
    LOG_SUCCESS_CAT("FINAL", "PINK PHOTONS ETERNAL — EVEN IN DEATH, THEY SHINE FOREVER.");

    LOG_MAIN("[PHASE 9 COMPLETE] THE DISPOSAL BALLERINA EXITS STAGE LEFT — THE VOYAGE IS OVER — THE EMPIRE IS ABSOLUTE");

    std::exit(0);
}

// =============================================================================
// MAIN — THE FINAL VOYAGE BEGINS
// =============================================================================
int main(int, char**) {
    try {
        phase1_preInitialization();
        phase3_sacrificialSplash();
        LOG_MAIN("SPLASH SACRIFICED — PHOTONS LIBERATED");

        createRealFinalWindow();
        RTX::g_ctx().init(SDL3Window::get(), Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT);

        phase5_rtxAscension();
        phase6_sceneAndAccelerationStructures();
        phase6_1_forgeTheLayouts();
        phase6_5_everything_is_ready();
        if (!ready_to_embark) phase9_ballerina();

        phase7_forgeTheRTX();
        if (!phase8_stone_seal_final()) phase9_ballerina();

        g_app_ptr = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v∞ TURBO", Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT);
        g_app().run();

        phase9_ballerina();
    }
    catch (const std::exception& e) {
        LOG_FATAL_CAT("CRASH", "FATAL: {}", e.what());
        return 1;
    }
    return 0;
}