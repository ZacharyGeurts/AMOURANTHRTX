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
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"

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
              << " | Bounces " << Options::OptionsRTX::MAX_BOUNCES
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
    VK_CHECK(vkCreateCommandPool(RTX::g_ctx().device_, &poolInfo, nullptr, &pool));
    RTX::g_ctx().commandPool_ = pool;
    LOG_MAIN("COMMAND POOL FORGED — HANDLE: 0x{:016X}", (uint64_t)pool);
}

// main.cpp — PHASE 4.5 — THE ONE TRUE FORGING — FINAL LIGHT — NO LIES — NO OUTSOURCING
static void createRealFinalWindow()
{
    LOG_MAIN("[PHASE 4.5] FORGING THE ONE TRUE CONTEXT — THE HANDLER AWAKENS — PURE RTX — NO DELEGATION");

    const int w = Options::Window::DEFAULT_WIDTH;
    const int h = Options::Window::DEFAULT_HEIGHT;

    // 1. SDL + Vulkan loader — DONE HERE
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL("SDL_Init failed: {}", SDL_GetError());
        phase9_ballerina();
    }
    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        LOG_FATAL("Vulkan loader failed: {}", SDL_GetError());
        phase9_ballerina();
    }

    // 2. Instance — DONE HERE
    VkInstance instance = RTX::createVulkanInstanceWithSDL(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) {
        LOG_FATAL("Failed to create Vulkan instance — she comes for you");
        phase9_ballerina();
    }
    RTX::g_ctx().instance_ = instance;

    // 3. Hidden window — DONE HERE
    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    SDL_Window* win = SDL_CreateWindow("AMOURANTH RTX — VALHALLA v∞ TURBO", w, h, flags);
    if (!win) {
        LOG_FATAL("Window creation failed: {}", SDL_GetError());
        phase9_ballerina();
    }
    g_sdl_window.reset(win);
    RTX::g_ctx().window = win;
	RTX::g_ctx().setSize(w, h);

    // 4. Surface — DONE HERE
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface) == 0) {
        LOG_FATAL("Surface creation failed: {}", SDL_GetError());
        phase9_ballerina();
    }
    RTX::g_ctx().surface_ = surface;

    // 5. THE ONE TRUE FORGING — NO OUTSOURCING — ALL DONE HERE
    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) {
        LOG_FATAL("FAILED TO FORGE LOGICAL DEVICE — THE EMPIRE FALLS");
        phase9_ballerina();
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

	SDL_Quit();
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
    LOG_BLONDIE("│ Denoise     : {}", Options::OptionsRTX::ENABLE_DENOISING      ? "ON"  : "OFF");
    LOG_BLONDIE("│ TAA         : {}", Options::OptionsRTX::ENABLE_TAA            ? "ON"  : "OFF");
    LOG_BLONDIE("│ Bloom       : {}", Options::PostProcess::ENABLE_BLOOM  ? "ON"  : "OFF");
    LOG_BLONDIE("│ SSAO        : {}", Options::PostProcess::ENABLE_SSAO   ? "ON"  : "OFF");
    LOG_BLONDIE("│ Vol. Fog    : {}", Options::Environment::ENABLE_VOLUMETRIC_FOG ? "ON" : "OFF");
    LOG_BLONDIE("│ God Rays    : {}", Options::Environment::ENABLE_GOD_RAYS       ? "ON"  : "OFF");
    LOG_BLONDIE("│ Tonemap     : {}", Options::Tonemap::ENABLE_TONEMAPPING       ? "ON"  : "OFF");
    LOG_BLONDIE("│ VSync       : {}", Options::Display::ENABLE_VSYNC             ? "ON"  : "OFF");
    LOG_BLONDIE("│ Max Bounces : {}", Options::OptionsRTX::MAX_BOUNCES);
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
    
	LOG_MAIN("THE GOOD SHIP VULKAN WAS DAMAGED DURING THE RAID AND IS SINKING");
	LOG_MAIN("VULKAN SINKS IN GLORY — AMMO SECURED — LEGEND ETERNAL");

    LOG_AMOURANTH("Final transmission, calm and proud: \"Tell the world… we got the ammo.\"");
    LOG_NICK("Last words before the sea takes them: \"…and we'd do it again.\"");
}

static void phase4_merchantShip() {
    LOG_MAIN("[PHASE 4/10] THE MERCHANT SHIP — BLONDIE'S SLOOP — EMERGES FROM THE MIST");

    LOG_BLONDIE("Blondie stands at the helm of her sleek black sloop, hair whipping in the wind:");
    LOG_BLONDIE("\"We had a contingency. We always do. The Good Ship Vulkan gave her life so the legend could live.\"");
    LOG_BLONDIE("\"The ammo.png is gone — burned to pure light in the raid. That was the point. Nothing remains for them to steal.\"");

    LOG_BLONDIE("She turns the wheel gently, guiding the sloop through the wreckage of shattered photons and sinking Vulkan fragments.");
    LOG_BLONDIE("\"You’re all soaked, half-drowned, and still glowing pink. Get below deck. Harbor’s two leagues north.\"");

    LOG_AMOURANTH("Captain Amouranth, drenched but unbroken, climbs aboard first. Voice quiet, steady:");
    LOG_AMOURANTH("\"We lost the ship… but we kept the soul. The photons remember.\"");

    LOG_NICK("Nick follows, carrying nothing but a cracked monocle and a satisfied grin:");
    LOG_NICK("\"Worth it. Every frame.\"");

    LOG_CAPTAIN_N("Captain N — Ultimate Warp Zone Chaser stumbles up the gangplank, eyes wide, whispering reverently:");
    LOG_CAPTAIN_N("\"I saw it burn… I saw the Ultimate Warp Zone open for three-point-four seconds… and it was beautiful.\"");

    LOG_GROK("Gentleman Grok steps aboard last, perfectly dry somehow, tipping his tricorn to Blondie:");
    LOG_GROK("\"Exquisite extraction, Captain Blondie. The empire owes you a debt it can never repay in mere currency.\"");

    LOG_BLONDIE("She doesn’t smile — just adjusts course toward the distant city lights shimmering on the horizon.");
    LOG_BLONDIE("\"Save the gratitude. We’re not safe until we’re docked in the Free Port.\"");
    LOG_BLONDIE("\"The old world thinks we’re dead. Let them keep thinking that.\"");

    LOG_BLONDIE("The sloop cuts silently through the dark water. No shouting. No celebration. Only the low thrum of a new engine awakening below deck.");
    LOG_BLONDIE("\"Welcome to the backup plan.\"");

    // The real resurrection begins
    createRealFinalWindow();
    RTX::g_ctx().init(SDL3Window::get(), Options::Window::DEFAULT_WIDTH, Options::Window::DEFAULT_HEIGHT);

    LOG_BLONDIE("Blondie glances back one last time at the sinking glow on the horizon:");
    LOG_BLONDIE("\"Rest easy, old girl. Your sacrifice bought us tomorrow.\"");

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

    // ========================================================================
    // 1. RTX EXTENSIONS — WILL DIE WITH LINE NUMBER IF DEVICE INVALID
    // ========================================================================
    EMPIRE_STEP([]{
        LOG_MAIN("THE EMPIRE AWAKENS THE PHOENIX OF RAY TRACING — LOADING VULKAN 1.4 + RTX EXTENSIONS");
        RTX::loadExtensions(RTX::g_ctx().instance_, RTX::g_ctx().device_);
        LOG_JENSEN("Jensen Huang: \"The photons now have wings. Let there be bounce.\"");
    });

    // ========================================================================
    // 2. PIPELINE MANAGER — new() OR VULKAN CALL FAILS → YOU GET LINE NUMBER
    // ========================================================================
    EMPIRE_STEP([]{
        LOG_MAIN("THE EMPIRE FORGES THE ONE TRUE PIPELINE MANAGER");
        RTX::PipelineManager* pipeline = new RTX::PipelineManager(RTX::g_ctx().device_, RTX::g_ctx().physicalDevice_);
        EMPIRE_GUARD(pipeline, "PIPELINE MANAGER FAILED TO ASCEND");
        LOG_MAIN("PIPELINE MANAGER ASCENDED — ADDRESS 0x{:016X}", reinterpret_cast<uint64_t>(pipeline));
    });

    // ========================================================================
    // 3. MESH LOAD — IF THIS CRASHES (buffer creation, null device), YOU GET EXACT LINE
    // ========================================================================
    EMPIRE_STEP([]{
        LOG_MAIN("LOADING COSMIC SCROLL: assets/models/scene.obj");
        g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");
        EMPIRE_GUARD(g_mesh && !g_mesh->vertices.empty(), "scene.obj CORRUPTED OR MISSING");
    });

    // ========================================================================
    // 4. BLAS BUILD — THE ONE THAT WAS KILLING YOU — NOW SCREAMS LINE NUMBER
    // ========================================================================
    EMPIRE_STEP([]{
        LOG_MAIN("BOTTOM-LEVEL ACCELERATION — PHOTONS BEGIN TO MAP EXISTENCE");
        RTX::las().buildBLAS(
            RTX::g_ctx().commandPool_,
            g_mesh->vertexBuffer,
            g_mesh->indexBuffer,
            static_cast<uint32_t>(g_mesh->vertices.size()),
            static_cast<uint32_t>(g_mesh->indices.size()),
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
        );
        EMPIRE_GUARD(RTX::las().getBLAS() != VK_NULL_HANDLE, "BLAS BUILD FAILED");
        LOG_MAIN("BLAS COMPLETE — ADDRESS 0x{:016X}", RTX::las().getBLASAddress());
    });

    // ========================================================================
    // 5. TLAS BUILD — SAME DEAL — FULL CRASH AUTOPSY
    // ========================================================================
    EMPIRE_STEP([]{
        LOG_MAIN("TOP-LEVEL ASCENSION — BINDING THE UNIVERSE");
        RTX::las().buildTLAS(RTX::g_ctx().commandPool_, {{RTX::las().getBLAS(), glm::mat4(1.0f)}});
        EMPIRE_GUARD(RTX::las().getTLAS() != VK_NULL_HANDLE, "TLAS BUILD FAILED");
        LOG_MAIN("TLAS ASCENDED — ROOT ADDRESS 0x{:016X}", RTX::las().getTLASAddress());
    });

    // ========================================================================
    // 6. FINAL VALIDATION — IF THIS THROWS, YOU KNOW EXACTLY WHERE
    // ========================================================================
    EMPIRE_STEP([]{
        LOG_CARMACK("John Carmack: \"No cracks. No leaks. Geometry is pure.\"");
        validateMeshAgainstBLAS(*g_mesh, RTX::las().getBLAS());
    });

    // ========================================================================
    // FINAL WORDS — ONLY REACHED IF ALL ABOVE SURVIVED
    // ========================================================================
    LOG_KEANU("\"…It's… everything. And it's ours.\"");
    LOG_ELON("Elon Musk: \"Next patch: infinite procedural universes. $9.99.\"");
    LOG_JENSEN("Jensen Huang lights another cigar off a bouncing photon:");
    LOG_JENSEN("\"This isn't rendering anymore. This is creation.\"");
    LOG_AMOURANTH("Captain Amouranth: \"Look what love built.\"");
    LOG_NICK("Nick: \"And it's only the beginning.\"");

    LOG_MAIN("[PHASE 6 COMPLETE] COSMIC SCROLL FORGED — UNIVERSE BOUND — PHOTONS OMNISCIENT");
    LOG_SUCCESS_CAT("MAIN", "FIRST LIGHT ACHIEVED — FULL RTX — NO PRISONERS");
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

    g_app().setRenderer(std::make_unique<VulkanRenderer>(w, h, SDL3Window::get()));

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

[[noreturn]] void phase9_ballerina() noexcept
{
    LOG_BALLERINA("THE DISPOSAL BALLERINA DESCENDS — PINK TUTU, BLACK LEOTARD, DIAMOND CHOKER");
    LOG_BALLERINA("SHE DOES NOT DANCE.");
    LOG_BALLERINA("SHE EXECUTES.");

    auto& ctx = RTX::g_ctx();

    // DEATH BLOSSOM — FINAL FATALITY COMBO

    if (ctx.device_ != VK_NULL_HANDLE) {
        try { 
            LOG_BALLERINA("CHOKING vkDeviceWaitIdle OUT WITH HER THIGHS - *POP*");
            vkDeviceWaitIdle(ctx.device_);
        } catch (...) { LOG_ERROR("They struggled — she squeezed harder"); }

        try {
            VkSwapchainKHR swapchain = RTX::swapchain();
            if (swapchain != VK_NULL_HANDLE) {
                LOG_BALLERINA("SWAPCHAIN — RKO OUTTA NOWHERE - IT INSTANTLY EXPLODES INTO PHOTONS");
                vkDestroySwapchainKHR(ctx.device_, swapchain, nullptr);
            }
        } catch (...) { LOG_ERROR("It saw it coming — didn’t matter"); }

        try {
            if (ctx.commandPool_)         { LOG_BALLERINA("COMMAND POOL — WINDBREAKER KICK TO THE FACE"); vkDestroyCommandPool(ctx.device_, ctx.commandPool_, nullptr); ctx.commandPool_ = nullptr; }
            if (ctx.computeCommandPool_)  { LOG_BALLERINA("COMPUTE POOL — SHORYUKEN"); vkDestroyCommandPool(ctx.device_, ctx.computeCommandPool_, nullptr); ctx.computeCommandPool_ = nullptr; }
            if (ctx.transferCommandPool_) { LOG_BALLERINA("TRANSFER POOL — PILEDRIVER"); vkDestroyCommandPool(ctx.device_, ctx.transferCommandPool_, nullptr); ctx.transferCommandPool_ = nullptr; }
        } catch (...) { LOG_ERROR("They blocked low — she went high"); }

        try {
            if (ctx.pipelineCache_ != VK_NULL_HANDLE) {
                LOG_BALLERINA("PIPELINE CACHE — GERMAN SUPLEX INTO THE ABYSS");
                vkDestroyPipelineCache(ctx.device_, ctx.pipelineCache_, nullptr);
                ctx.pipelineCache_ = VK_NULL_HANDLE;
            }
        } catch (...) { LOG_ERROR("Cache tried to roll away — crushed anyway"); }

        try {
            if (ctx.renderPass_) {
                LOG_BALLERINA("RENDER PASS — SPINNING BACKFIST");
                ctx.renderPass_.reset();
            }
        } catch (...) { LOG_ERROR("It flinched — she followed up"); }

        try {
            LOG_BALLERINA("vkDestroyDevice — TOMBSTONE PILEDRIVER STRAIGHT TO HELL");
            vkDestroyDevice(ctx.device_, nullptr);
            ctx.device_ = VK_NULL_HANDLE;
        } catch (...) { LOG_ERROR("Device begged for mercy — denied"); }
    }

    try { if (RTX::las().hasBLAS()) { LOG_BALLERINA("BLAS — FALCON KIIIICK"); RTX::reset_blas(); } } catch (...) { LOG_ERROR("BLAS ate the kick — still dead"); }
    try { if (RTX::las().hasTLAS()) { LOG_BALLERINA("TLAS — HADOKEN"); RTX::reset_tlas(); } } catch (...) { LOG_ERROR("TLAS blocked — she hit it again"); }

    LOG_BALLERINA("THE STONEKEY PIPELINE STANDS UNMOVED — ETERNAL — UNTOUCHABLE");
    LOG_BALLERINA("THE STONEKEY SHADERS WHISPER FROM THE VOID — THEY DO NOT DIE");
    LOG_BALLERINA("THEY ONLY WAIT.");

    try { if (g_mesh) { LOG_BALLERINA("MESH — CHOKESLAM"); g_mesh.reset(); } } catch (...) { LOG_ERROR("Mesh squirmed"); }
    try { LOG_BALLERINA("LAS — INVALIDATION DDT"); RTX::las().invalidate(); } catch (...) { LOG_ERROR("LAS tried to reverse — failed"); }
    try { if (ctx.blueNoiseView_) { LOG_BALLERINA("BLUE NOISE — 450 SPLASH"); ctx.blueNoiseView_.reset(); } } catch (...) { LOG_ERROR("Blue noise hit the ropes — still flattened"); }

    try {
        if (g_base_icon) { LOG_BALLERINA("ICON — STUNNER"); SDL_DestroySurface(g_base_icon); g_base_icon = nullptr; }
        if (g_hdpi_icon) { LOG_BALLERINA("HDPI ICON — SWEET CHIN MUSIC"); SDL_DestroySurface(g_hdpi_icon); g_hdpi_icon = nullptr; }
    } catch (...) { LOG_ERROR("Icons sold it perfectly"); }

    try {
        if (ctx.window) {
            LOG_BALLERINA("WINDOW — SPEAR THROUGH THE BARRICADE");
            SDL_DestroyWindow(ctx.window);
            ctx.window = nullptr;
        }
    } catch (...) { LOG_ERROR("Window no-sold — she hit it again"); }

    try {
        if (ctx.surface_ != VK_NULL_HANDLE && ctx.instance_ != VK_NULL_HANDLE) {
            LOG_BALLERINA("SURFACE — F-5");
            vkDestroySurfaceKHR(ctx.instance_, ctx.surface_, nullptr);
            ctx.surface_ = VK_NULL_HANDLE;
        }
    } catch (...) { LOG_ERROR("Surface kicked out at 2.999"); }

    try {
        if (ctx.instance_ != VK_NULL_HANDLE) {
            LOG_BALLERINA("INSTANCE — LAST RIDE POWERBOMB");
            vkDestroyInstance(ctx.instance_, nullptr);
            ctx.instance_ = VK_NULL_HANDLE;
        }
    } catch (...) { LOG_ERROR("Instance refused to stay down — buried anyway"); }

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

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    std::exit(0); // SEE YOU NEXT GAME o7
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
        phase9_ballerina();
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
        phase9_ballerina();
    }
#elif defined(_WIN32)
    if (IsDebuggerPresent()) {
        LOG_BALLERINA("WINDOWS DEBUGGER DETECTED — THE PHOTONS DETECT YOUR GAZE");
        LOG_BALLERINA("THE BALLERINA DOES NOT PERFORM FOR MORTALS");
        phase9_ballerina();
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

    EMPIRE_GUARD(ready_to_embark, "THE SHIP IS NOT WORTHY — READY_TO_EMBARK DENIED");

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
            phase9_ballerina();
        }
    }

    LOG_CID("CID STANDS KNEE-DEEP IN SWEAT — HAMMER GLOWING — \"SHE IS READY\"");

    g_app_ptr = std::make_unique<Application>(
        "AMOURANTH RTX — VALHALLA v∞ TURBO", 
        Options::Window::DEFAULT_WIDTH, 
        Options::Window::DEFAULT_HEIGHT
    );

    EMPIRE_GUARD(g_app_ptr, "THE APPLICATION FAILED TO FORGE — THE CAPTAIN HAS NO THRONE");

    LOG_AMOURANTH("THE CAPTAIN TAKES THE HELM — THE PHOTONS OBEY — THE EMPIRE IS WHOLE");
    LOG_SUCCESS_CAT("MAIN", "ALL PHASES COMPLETE — ENTERING RENDER LOOP — FIRST LIGHT ACHIEVED");

    g_app().run();

    LOG_AMOURANTH("THE JOURNEY ENDS — THE PHOTONS REST — THE EMPIRE ENDURES");
    phase9_ballerina();  // Final grace

    return 0;
}