// main.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — DECEMBER 16, 2025
// FULLY COMPILING — PURE EMPIRE
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
#include "engine/GLOBAL/InputManager.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/UBO.hpp"

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
using StoneKey::stone_width;
using StoneKey::stone_height;
using StoneKey::stone_window;
using StoneKey::stone_rtprops;
using StoneKey::stone_pass;
using StoneKey::stone_swapchain;
using StoneKey::stone_transfer_queue;
using StoneKey::stone_present_family;
using StoneKey::stone_transfer_family;
using StoneKey::stone_compute_family;
using StoneKey::stone_images;
using StoneKey::stone_image_count;
using StoneKey::stone_views;
using StoneKey::stone_compute_queue;
using StoneKey::stone_present_queue;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_physical;
using StoneKey::stone_surface;
using StoneKey::stone_instance;
using StoneKey::stone_seal_transfer_queue;
using StoneKey::stone_seal_compute_queue;
using StoneKey::stone_seal_present_queue;
using StoneKey::stone_seal_graphics_queue;
using StoneKey::stone_seal_images;
using StoneKey::stone_seal_image_count;
using StoneKey::stone_seal_swapchain;
using StoneKey::stone_seal_rtprops;
using StoneKey::stone_seal_physical;
using StoneKey::stone_seal_device;
using StoneKey::stone_seal_surface;
using StoneKey::stone_seal_window;
using StoneKey::stone_seal_instance;

// =============================================================================
// GLOBALS — THE EMPIRE'S HEARTBEATS
// =============================================================================
std::unique_ptr<Application> g_app_ptr = nullptr;
VulkanRenderer* g_renderer_ptr = nullptr;
float g_deltaTime = 0.0f;

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

    void run() noexcept;

void setRenderer(std::unique_ptr<VulkanRenderer> r)
{
    renderer_ = std::move(r);

    if (renderer_)
    {
        // All feature states now directly respect the sacred OptionsMenu configuration
        // No forcing, no overrides — only the truth as defined in OptionsMenu.hpp

        // Tonemapping: follows the empire's decree
        renderer_->setTonemap(Options::Tonemap::ENABLE_TONEMAPPING);

        // HyperTrace: enabled exactly as configured
        if (Options::OptionsRTX::ENABLE_HYPERTRACE)
        {
            renderer_->toggleHypertrace();
        }

        // Denoising: respects the purification setting
        if (Options::OptionsRTX::ENABLE_DENOISING)
        {
            renderer_->toggleDenoising();
        }

        // Adaptive Sampling: awakens only if commanded
        if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING)
        {
            renderer_->toggleAdaptiveSampling();
        }

        // Overclock Mode: maximum performance only if decreed
        renderer_->setOverclockMode(Options::Performance::OVERCLOCK_RENDERER);

        // Overlay: visibility controlled by user preference
        renderer_->setOverlay(showOverlay_);

        LOG_AMOURANTH(
            "RENDERER BOUND — CONFIGURATION FULLY RESPECTED\n"
            "│ Tonemap:          {}\n"
            "│ HyperTrace:       {}\n"
            "│ Denoising:        {}\n"
            "│ Adaptive Sampling:{}\n"
            "│ Accumulation:     {}\n"
            "│ Overclock:        {}\n"
            "│ Overlay:          {}\n"
            "└── PINK PHOTONS FLOW IN HARMONY WITH THE VISION",
            Options::Tonemap::ENABLE_TONEMAPPING ? "ENABLED" : "DISABLED",
            Options::OptionsRTX::ENABLE_HYPERTRACE ? "IGNITED" : "DORMANT",
            Options::OptionsRTX::ENABLE_DENOISING ? "ACTIVE" : "INACTIVE",
            Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING ? "AWAKENED" : "SLEEPING",
            Options::OptionsRTX::ENABLE_ACCUMULATION ? "ETERNAL" : "DISABLED",
            Options::Performance::OVERCLOCK_RENDERER ? "MAXIMUM" : "SAFE",
            showOverlay_ ? "VISIBLE" : "HIDDEN"
        );
    }
    else
    {
        LOG_ERROR_CAT("APPLICATION", "Renderer binding failed — null pointer — the empire has no eyes");
    }
}

    [[nodiscard]] VulkanRenderer* renderer() const noexcept { return renderer_.get(); }

    // NEW: Render mode system
    void setRenderMode(int mode);

private:
    void processInput(float deltaTime);
    void render(float deltaTime);

    void toggleFullscreen() { SDL3Window::toggleFullscreen(); }
    void toggleOverlay()    { showOverlay_ = !showOverlay_; if (renderer_) renderer_->setOverlay(showOverlay_); }
    void toggleTonemap()    { tonemapEnabled_ = !tonemapEnabled_; if (renderer_) renderer_->setTonemap(tonemapEnabled_); }
    void toggleHypertrace() { hypertraceEnabled_ = !hypertraceEnabled_; }

    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence>     inFlightFences_;

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

// =============================================================================
// 1. Application::Application — NO DEFAULT MODE — PURE EMPIRE
// =============================================================================
Application::Application(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height)
{
    if (!stone_window()) {
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    SDL_SetWindowTitle(stone_window(), title.c_str());
    lastFrameTime_ = std::chrono::steady_clock::now();
    proj_ = glm::perspective(glm::radians(75.0f), static_cast<float>(width) / height, 0.1f, 1000.0f);

    currentRenderMode_ = 1;
}

Application::~Application() = default;

void Application::run() noexcept
{
    auto lastTime       = std::chrono::steady_clock::now();
    float titleTimer    = 0.0f;
    constexpr float TITLE_UPDATE_INTERVAL = 0.6f;
    int   dotPhase      = 0;

    int   frameCount         = 0;
    float fpsTimer           = 0.0f;
    float displayedFPS       = 0.0f;

    // Resize debounce
    static constexpr float RESIZE_DEBOUNCE_SECONDS = 0.2f;
    static auto lastResizeTime = std::chrono::steady_clock::time_point::min();
    static int pendingWidth = 0;
    static int pendingHeight = 0;

    LOG_AMOURANTH("APPLICATION::run() — THE EMPIRE AWAKENS — THE CURRENT BEGINS");

    while (!quit_)
    {
        const auto frameStart = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(frameStart - lastTime).count();
        lastTime = frameStart;

        // INPUT
        bool toggleFS = false;
        int  winW = 0, winH = 0;
        SDL3Window::pollEvents(winW, winH, quit_, toggleFS);

        // DEBOUNCE RESIZE — only trigger after user stops dragging
        if (winW > 0 && winH > 0)
        {
            if (width_ != winW || height_ != winH)
            {
                pendingWidth = winW;
                pendingHeight = winH;
                lastResizeTime = frameStart;
            }
        }

        // Process pending resize if enough time passed
        if (pendingWidth > 0 && pendingHeight > 0)
        {
            const float timeSinceResize = std::chrono::duration<float>(frameStart - lastResizeTime).count();
            if (timeSinceResize >= RESIZE_DEBOUNCE_SECONDS)
            {
                LOG_AMOURANTH("RESIZE FINALIZED: {}x{} → {}x{}", width_, height_, pendingWidth, pendingHeight);
                width_  = pendingWidth;
                height_ = pendingHeight;
                proj_   = glm::perspective(glm::radians(75.0f),
                                          float(width_) / std::max(height_, 1),
                                          0.1f, 1000.0f);

                if (renderer_) {
                    renderer_->onWindowResize(static_cast<uint32_t>(pendingWidth), static_cast<uint32_t>(pendingHeight));
                }

                pendingWidth = pendingHeight = 0;
            }
        }

        if (toggleFS)
        {
            LOG_AMOURANTH("FULLSCREEN TOGGLE");
            SDL3Window::toggleFullscreen();
        }

        // INPUT SYSTEM
        INPUT.pumpEvents(g_deltaTime, [this](int mode) { setRenderMode(mode); }, stone_window());

        // RENDER
        bool swapchainValid = (stone_swapchain() != VK_NULL_HANDLE);

        if (renderer_ && renderer_->isAlive() && swapchainValid)
        {
            renderer_->setMaxFramesInFlight(Options::Performance::MAX_FRAMES_IN_FLIGHT);
            renderer_->renderFrame(CAM, g_deltaTime);
        }

        // TITLE BAR
        titleTimer += g_deltaTime;
        if (titleTimer >= TITLE_UPDATE_INTERVAL)
        {
            titleTimer -= TITLE_UPDATE_INTERVAL;
            dotPhase = (dotPhase + 1) % 4;

            // ... mode name, fps, title update (unchanged) ...
        }

        // Displayed FPS counter
        ++frameCount;
        fpsTimer += g_deltaTime;
        if (fpsTimer >= 1.0f)
        {
            displayedFPS = frameCount / std::max(fpsTimer, 0.001f);
            LOG_TRACE_CAT("FPS", "Displayed FPS: {:.1f}", displayedFPS);
            frameCount = 0;
            fpsTimer   = 0.0f;
        }
    }

    vkDeviceWaitIdle(stone_device());
    LOG_AMOURANTH("[SHUTDOWN] The empire rests. The pink photons return to the void.");
}

// =============================================================================
// 4. Application::setRenderMode — FINAL, FLAWLESS
// =============================================================================
void Application::setRenderMode(int mode)
{
    constexpr int MIN_MODE = 1;
    constexpr int MAX_MODE = 9;

    if (mode < MIN_MODE || mode > MAX_MODE) {
        LOG_WARNING_CAT("APP", "Invalid render mode {} requested — ignoring", mode);
        return;
    }

    if (mode == currentRenderMode_) return;

    // Capture mode by value
    const char* modeName = [mode]() -> const char* {
        switch (mode) {
            case 0:  return "VOID";
            case 1:  return "PURE GREEN MATRIX RAIN";
            case 2:  return "PATH TRACED ACCUMULATION";
            case 3:  return "REALTIME HYBRID DENOISED";
            case 4:  return "RASTERIZED FALLBACK";
            case 5:  return "DEBUG VISUALIZATION";
            case 6:  return "TLAS VISUALIZER";
            case 7:  return "SBT DEBUG OVERLAY";
            case 8:  return "PERFORMANCE METRICS";
            case 9:  return "SHADER HOT RELOAD TEST";
            default: return "UNKNOWN MODE";
        }
    }();

    LOG_INFO_CAT("APP", "ENGAGING RENDER MODE {}: {}", mode, modeName);

    renderer_->setRenderMode(mode);
    renderer_->requestAccumulationReset();

    currentRenderMode_ = mode;

    LOG_SUCCESS_CAT("RENDER",
        "{}RENDER MODE {} ACTIVATED — {} — PHOTONS AWAKEN — FIRST LIGHT ACHIEVED{}",
        RASPBERRY_PINK, mode, modeName, RESET);
}

static void createCommandPool() noexcept
{
    if (RTX::g_ctx().commandPool_ != VK_NULL_HANDLE) {
        LOG_TRACE_CAT("CMD", "Command pool already exists — skipping creation");
        return;
    }

    LOG_AMOURANTH("FORGING THE EMPIRE COMMAND POOL — ONE POOL TO RULE THEM ALL");

    VkCommandPoolCreateInfo poolInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = stone_graphics_family()
    };

    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(stone_device(), &poolInfo, nullptr, &pool));

    RTX::g_ctx().commandPool_ = pool;

    // Optional: Debug name for validation layers
    if (RTX::g_ctx().debugUtilsSupported()) {
        auto setName = (PFN_vkSetDebugUtilsObjectNameEXT)
            vkGetDeviceProcAddr(stone_device(), "vkSetDebugUtilsObjectNameEXT");
        if (setName) {
            VkDebugUtilsObjectNameInfoEXT nameInfo{
                .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext        = nullptr,
                .objectType   = VK_OBJECT_TYPE_COMMAND_POOL,
                .objectHandle = reinterpret_cast<uint64_t>(pool),
                .pObjectName  = "EMPIRE_COMMAND_POOL_ETERNAL"
            };
            setName(stone_device(), &nameInfo);
        }
    }

    LOG_SUCCESS_CAT("CMD", "Empire command pool forged — transient + reset — ready for battle");
}

// =============================================================================
// GLOBALS & PHASES
// =============================================================================
inline std::unique_ptr<MeshLoader::Mesh> g_mesh = nullptr;
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

static void createRealFinalWindow() noexcept
{
    LOG_CAPTAIN_N(
        "\n"
        "              █████████████████████████████████████████\n"
        "              █     OPERATION: VALHALLA v∞ TURBO      █\n"
        "              █     COMMANDER: CAPTAIN N              █\n"
        "              █     HEADLINER: FITZ AND THE TANTRUMS  █\n"
        "              █████████████████████████████████████████\n"
        "\n"
        "               *Captain N stands tall, visor gleaming*\n"
        "               \"This is not conquest.\"\n"
        "               \"This is homecoming.\"\n"
        "               \"The window opens.\"\n"
        "               \"The photons awaken.\"\n"
        "               \"The empire... returns.\"");

    const int w = Options::Window::DEFAULT_WIDTH;
    const int h = Options::Window::DEFAULT_HEIGHT;

    stone_seal_width(w);
    stone_seal_height(h);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        phase9_ballerina("SDL refused to awaken — the empire will not tolerate weakness");
    }

    LOG_CAPTAIN_N("[CAPTAIN N] \"SDL online. Heartbeat strong.\"");

    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        phase9_ballerina("Vulkan loader missing — the forge is cold");
    }

    LOG_CAPTAIN_N("[CAPTAIN N] \"Vulkan seized. The forge burns bright.\"");

    VkInstance instance = RTX::createVulkanInstanceWithSDL(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) {
        phase9_ballerina("Instance creation failed — the empire has no reflection");
    }
    stone_seal_instance(instance);

    LOG_CAPTAIN_N("[CAPTAIN N] \"Instance forged. The empire sees itself.\"");

    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* win = SDL_CreateWindow(
        "AMOURANTH RTX — VALHALLA v∞ TURBO",
        w, h,
        flags
    );

    if (!win) {
        phase9_ballerina("Window creation failed — there is no throne");
    }

    auto setIcon = [](SDL_Window* w) {
        const char* paths[] = {
            "assets/textures/ammo.ico",
            "assets/textures/ammo32.ico",
            nullptr
        };
        for (int i = 0; paths[i]; ++i) {
            if (SDL_Surface* s = IMG_Load(paths[i])) {
                SDL_SetWindowIcon(w, s);
                SDL_DestroySurface(s);
                return;
            }
        }
    };
    setIcon(win);

    stone_seal_window(win);
    g_sdl_window.reset(win);
    RTX::g_ctx().setSize(w, h);
    SDL_ShowWindow(win);

    LOG_CAPTAIN_N("[CAPTAIN N] \"Window claimed. The throne is ours.\"");

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface) || !surface) {
        phase9_ballerina("Surface creation failed — the empire cannot see");
    }
    stone_seal_surface(surface);

    LOG_CAPTAIN_N("[CAPTAIN N] \"Surface bound. We touch the metal.\"");

    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) {
        phase9_ballerina("No GPU worthy of the empire was found");
    }
    stone_seal_device(device);
    stone_seal_physical(RTX::g_ctx().physicalDevice());

    LOG_CAPTAIN_N("[CAPTAIN N] \"Device claimed. The heart beats true.\"");

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };
    vkGetPhysicalDeviceProperties2(RTX::g_ctx().physicalDevice(), &props2);

    if (rtProps.shaderGroupHandleSize == 0) {
        phase9_ballerina("This GPU dares to call itself modern without ray tracing — execution denied");
    }

    stone_seal_rtprops(rtProps);

    LOG_CAPTAIN_N("[CAPTAIN N] \"Ray tracing confirmed. The photons have teeth.\"");

    RTX::SwapchainManager::create(win, w, h);

    LOG_CAPTAIN_N("[CAPTAIN N] \"Canvas forged. The empire has a sky.\"");

    createCommandPool();

    LOG_CAPTAIN_N("[CAPTAIN N] \"Battlefield prepared. The war begins.\"");

    SDL_SetWindowTitle(win,
        "AMOURANTH RTX — VALHALLA v∞ TURBO | PHOTONS: INFINITE | EMPIRE: ETERNAL");

    LOG_SUCCESS_CAT("WINDOW", "VALHALLA v∞ TURBO fully initialized — first light achieved — empire eternal");
}

static void showSacrificialSplash() noexcept
{
    constexpr bool  enabled  = true;
    constexpr float duration = Options::Splash::SPLASH_DURATION_SECONDS;

    if (!enabled || duration <= 0.0f) {
        LOG_INFO("Sacrificial splash disabled");
        return;
    }

    constexpr int   W = 1280;
    constexpr int   H = 720;
    constexpr const char* TITLE      = "AMOURANTH RTX — VALHALLA v∞ TURBO";
    constexpr const char* IMAGE_PATH = "assets/textures/ammo.png";

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        LOG_WARNING("SDL_InitSubSystem(SDL_INIT_VIDEO) failed — splash skipped");
        return;
    }

    SDL_Window* win = SDL_CreateWindow(
        TITLE,
        W, H,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (!win) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Rect disp{};
    SDL_GetDisplayBounds(0, &disp);
    SDL_SetWindowPosition(win,
        disp.x + (disp.w - W) / 2,
        disp.y + (disp.h - H) / 2
    );

    auto setIcon = [](SDL_Window* w) {
        const char* paths[] = {
            "assets/textures/ammo.ico",
            "assets/textures/ammo32.ico",
            nullptr
        };
        for (int i = 0; paths[i]; ++i) {
            if (SDL_Surface* s = IMG_Load(paths[i])) {
                SDL_SetWindowIcon(w, s);
                SDL_DestroySurface(s);
                return;
            }
        }
    };
    setIcon(win);

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Surface* surf = IMG_Load(IMAGE_PATH);
    if (!surf) {
        LOG_WARNING("Splash image missing: {}", IMAGE_PATH);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_DestroySurface(surf);
    if (!tex) {
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    float texW = 0.0f, texH = 0.0f;
    SDL_GetTextureSize(tex, &texW, &texH);

    SDL_FRect dst{
        (W - texW) * 0.5f,
        (H - texH) * 0.5f,
        texW,
        texH
    };

    SDL_ShowWindow(win);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, nullptr, &dst);
    SDL_RenderPresent(ren);

    LOG_INFO("Sacrificial splash active — {}s", duration);

    const auto start = std::chrono::steady_clock::now();
    bool aborted = false;

    while (!aborted) {
        const float elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - start).count();

        if (elapsed >= duration) break;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                aborted = true;
            }
            else if (Options::Splash::ALLOW_EARLY_EXIT &&
                     e.type == SDL_EVENT_KEY_DOWN &&
                     e.key.key == SDLK_ESCAPE)
            {
                aborted = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    LOG_INFO("Sacrificial splash complete — photons liberated");
}

static void phase3_sacrificialSplash()
{
    showSacrificialSplash();
}

static void phase4_merchantShip()
{
    createRealFinalWindow();
    RTX::g_ctx().init();
}

static void phase6_sceneAndAccelerationStructures()
{
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
        LOG_FATAL_CAT("MESH", "MESH BUFFERS NOT ALLOCATED — vertexBuffer=0x{} indexBuffer=0x{}",
                      g_mesh->vertexBuffer, g_mesh->indexBuffer);
        phase9_ballerina("MESH BUFFERS ZERO", std::source_location::current());
    }

    auto* mesh = g_mesh.get();

    stone_seal_mesh(
        RAW_BUFFER(mesh->vertexBuffer),
        BufferManager::get(mesh->vertexBuffer)->memory,
        RAW_BUFFER(mesh->indexBuffer),
        BufferManager::get(mesh->indexBuffer)->memory,
        static_cast<uint32_t>(mesh->indices.size())
    );
}

static std::unique_ptr<VulkanRenderer> phase7_Renderer() noexcept
{
    LOG_MAIN(
        "\n"
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                    PHASE 7 — FORGING THE ONE TRUE RENDERER                 ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");

    LOG_ATTEMPT_CAT("RENDERER", "PHASE 7 — Forging the one true renderer...");

    auto renderer = std::make_unique<VulkanRenderer>(
        stone_width(),
        stone_height(),
        SDL3Window::get(),
        Options::Performance::OVERCLOCK_RENDERER
    );

    renderer->createCommandBuffers();
    renderer->createSyncObjects();

    renderer->initializeAllBufferData(
        Options::Performance::MAX_FRAMES_IN_FLIGHT,
        352,
        16 * 1024 * 1024
    );

    stone_seal_renderer(renderer.get());

    LOG_SUCCESS_CAT("RENDERER", "VulkanRenderer forged — buffers initialized — crown complete");
    LOG_CAPTAIN_N("[CAPTAIN N] \"The buffers live.\n"
                  "               The handles are real.\n"
                  "               The empire is whole.\"\n"
                  "*salutes with glowing plasma blade*");

    LOG_MAIN("\n"
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║  PHASE 7 — COMPLETE — RENDERER FORGED — GRACE SEALED @ {} \n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n", stone_device());

    return renderer;
}

static void phase8_forgeTheRTX()
{
    static bool crownWorn = false;
    if (crownWorn) {
        LOG_AMOURANTH("THE CROWN IS ALREADY WORN — PHOTONS FLOW — NO FORGING NEEDED");
        return;
    }

    auto& pipe = RTX::pipeline();

    VkCommandBuffer mainCmd = g_rtx().getCurrentCommandBuffer();
    pipe.forgeRTXPipeline(RTX::g_ctx().commandPool(), stone_graphics_queue(), mainCmd);

    pipe.loadShader("assets/textures/envmap.hdr");

    g_rtx().createEnvMapDisplayPipeline();

    StoneKey::stone_seal_pipeline(&pipe);
    crownWorn = true;

    LOG_AMOURANTH("MIA AWAKENS — THE CROWN IS WORN — HDR SKY READY — PHOTONS ETERNAL");
}

[[noreturn]] void phase9_ballerina(std::string_view reason, std::source_location loc) noexcept
{
    static bool already_running = false;
    if (already_running) {
        std::_Exit(1);
    }
    already_running = true;
    auto& ctx = RTX::g_ctx();

    if (VkDevice device = stone_device(); device != VK_NULL_HANDLE) [[likely]] {
        vkDeviceWaitIdle(device);

        if (VkSwapchainKHR sc = stone_swapchain(); sc) {
            vkDestroySwapchainKHR(device, sc, nullptr);
        }

        if (ctx.commandPool_)           vkDestroyCommandPool(device, ctx.commandPool_, nullptr);
        if (ctx.computeCommandPool_)    vkDestroyCommandPool(device, ctx.computeCommandPool_, nullptr);
        if (ctx.transferCommandPool_)   vkDestroyCommandPool(device, ctx.transferCommandPool_, nullptr);
        if (ctx.pipelineCache_)         vkDestroyPipelineCache(device, ctx.pipelineCache_, nullptr);
        if (ctx.renderPass_)            ctx.renderPass_.reset();

        vkDestroyDevice(device, nullptr);
    }

    RTX::las().reset();

    g_mesh.reset();
    ctx.blueNoiseView_.reset();

    if (g_base_icon)  { SDL_DestroySurface(g_base_icon);  g_base_icon  = nullptr; }
    if (g_hdpi_icon)  { SDL_DestroySurface(g_hdpi_icon);  g_hdpi_icon  = nullptr; }

    if (ctx.window)      { SDL_DestroyWindow(ctx.window); ctx.window = nullptr; }
    if (ctx.surface_ && ctx.instance_) {
        vkDestroySurfaceKHR(ctx.instance_, ctx.surface_, nullptr);
    }
    if (ctx.instance_)   vkDestroyInstance(ctx.instance_, nullptr);

    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    LOG_FATAL_CAT("BALLERINA", "EMPIRE TERMINATED — GRACE WITHDRAWN — THE PHOTONS RETURN TO THE VOID");

    std::_Exit(1);
}

int main(int, char**)
{
    install_apocalypse_handler();

    phase3_sacrificialSplash();
    phase4_merchantShip();
    phase6_sceneAndAccelerationStructures();
    auto renderer = phase7_Renderer();
    phase8_forgeTheRTX();

    stone_seal_final();

    g_app_ptr = std::make_unique<Application>("AMOURANTH RTX vTURBO", 3840, 2016);
    g_renderer_ptr = renderer.get();
    g_app_ptr->setRenderer(std::move(renderer));

    g_app_ptr->run();

    return 0;
}