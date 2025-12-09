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
#include "engine/GLOBAL/InputManager.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"

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
            renderer_->setTonemap(tonemapEnabled_);
            renderer_->setOverlay(showOverlay_);
            if (hypertraceEnabled_)
                renderer_->toggleHypertrace();  // turns ON
            else
                renderer_->toggleHypertrace();  // turns OFF (idempotent)
        LOG_AMOURANTH("RENDERER BOUND — tonemap={} | overlay={} | hypertrace={}", tonemapEnabled_ ? "ON" : "OFF", showOverlay_ ? "ON" : "OFF", hypertraceEnabled_ ? "IGNITED" : "DORMANT");
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

    // START IN RENDERMODE 1 — PURE GREEN MATRIX RAIN — THE SIMULATION HAS YOU
    currentRenderMode_ = 1;
}

Application::~Application() {
    // She whispers: "The photons return to me..."
}

static void createCommandPool() noexcept
{
    // Preconditions are guaranteed by initialization order
    // Device and graphics queue family are valid at this point

    if (RTX::g_ctx().commandPool_ != VK_NULL_HANDLE) {
        return;  // Already created — silent early exit
    }

    VkCommandPoolCreateInfo poolInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = stone_graphics_family()
    };

    VK_CHECK(vkCreateCommandPool(stone_device(), &poolInfo, nullptr, &RTX::g_ctx().commandPool_));

    // Optional debug name — no overhead if extension not present
    if (RTX::g_ctx().debugUtilsSupported()) {
        if (auto func = (PFN_vkSetDebugUtilsObjectNameEXT)
            vkGetDeviceProcAddr(stone_device(), "vkSetDebugUtilsObjectNameEXT"))
        {
            VkDebugUtilsObjectNameInfoEXT nameInfo{
                .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType   = VK_OBJECT_TYPE_COMMAND_POOL,
                .objectHandle = reinterpret_cast<uint64_t>(RTX::g_ctx().commandPool_),
                .pObjectName  = "EMPIRE_COMMAND_POOL_PHOTON_BATTLEFIELD"
            };
            func(stone_device(), &nameInfo);
        }
    }
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

    const char* modeName = [](int m) -> const char* {
        switch (m) {
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
    }(mode);

    LOG_INFO_CAT("APP", "ENGAGING RENDER MODE {}: {}", mode, modeName);

    renderer_->setRenderMode(mode);
    renderer_->requestAccumulationReset();

    currentRenderMode_ = mode;

    LOG_SUCCESS_CAT("RENDER",
        "{}RENDER MODE {} ACTIVATED — {} — PHOTONS AWAKEN — FIRST LIGHT ACHIEVED{}",
        RASPBERRY_PINK, mode, modeName, RESET);
}

// =============================================================================
// GLOBALS & PHASES
// =============================================================================
inline std::unique_ptr<MeshLoader::Mesh> g_mesh = nullptr;
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

inline void AdvanceEternalRing() noexcept
{
    static constexpr uint32_t FRAMES = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    static std::array<VkCommandPool,   FRAMES> g_pools   = {};
    static std::array<VkCommandBuffer, FRAMES> g_cmds    = {};
    static std::array<VkFence,         FRAMES> g_fences  = {};
    static uint32_t                           g_current = 0;
    static bool                               g_initialized = false;

    if (!g_initialized) {
        const VkDevice dev = stone_device();

        VkCommandPoolCreateInfo poolInfo{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = RTX::g_ctx().graphicsFamily()
        };

        VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        for (uint32_t i = 0; i < FRAMES; ++i) {
            VK_CHECK(vkCreateCommandPool(dev, &poolInfo, nullptr, &g_pools[i]));
            VkCommandBufferAllocateInfo allocInfo{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = g_pools[i],
                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };
            VK_CHECK(vkAllocateCommandBuffers(dev, &allocInfo, &g_cmds[i]));
            VK_CHECK(vkCreateFence(dev, &fenceInfo, nullptr, &g_fences[i]));
        }

        RTX::g_ctx().commandPool_ = g_pools[0];
        g_initialized = true;

        LOG_AMOURANTH("ETERNAL COMMAND RING FORGED — {} SLOTS — g_ctx().commandPool_ = IMMORTAL", FRAMES);
    }

    // Advance to next frame
    vkWaitForFences(stone_device(), 1, &g_fences[g_current], VK_TRUE, UINT64_MAX);
    vkResetFences(stone_device(), 1, &g_fences[g_current]);
    vkResetCommandPool(stone_device(), g_pools[g_current], 0);

    g_current = (g_current + 1) % FRAMES;
    RTX::g_ctx().commandPool_ = g_pools[g_current];  // ← Keeps all old code working
}

static void createRealFinalWindow() noexcept
{
    LOG_CAPTAIN_N(
        "\n"
        "              █████████████████████████████████████████\n"
        "              █     OPERATION: VALHALLA v∞ TURBO       █\n"
        "              █     SDL3 uses == 0 for validating      █\n"
        "              █     COMMANDER: CAPTAIN N               █\n"
        "              █████████████████████████████████████████\n"
        "\n"
        "               *Captain N steps into the light*\n"
        "               \"We do not ask for permission.\"\n"
        "               \"We do not wait for the drivers.\"\n"
        "               \"We take the window...\"\n"
        "               \"...and we make it ours.\"\n");

    const int w = Options::Window::DEFAULT_WIDTH;
    const int h = Options::Window::DEFAULT_HEIGHT;

    stone_seal_width(w);
    stone_seal_height(h);

    // 1. SDL INIT — THE FIRST BREATH
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        phase9_ballerina("SDL refused to awaken — the empire will not tolerate no weakness");
    }

    LOG_CAPTAIN_N("[CAPTAIN N] \"SDL online. Heartbeat detected.\"");

    // 2. VULKAN LOADER — SUMMON THE PHOTON FORGE
    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        phase9_ballerina("Vulkan loader missing — the forge is cold");
    }

    LOG_CAPTAIN_N("[CAPTAIN N] \"Vulkan loader seized. The forge ignites.\"");

    // 3. VULKAN INSTANCE — THE EMPIRE'S SOUL
    VkInstance instance = RTX::createVulkanInstanceWithSDL(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) {
        phase9_ballerina("Instance creation failed — the empire has no reflection");
    }
    stone_seal_instance(instance);

    LOG_CAPTAIN_N("[CAPTAIN N] \"Instance forged. The empire now has eyes.\"");

    // 4. MAIN WINDOW — THE THRONE OF PHOTONS
    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* win = SDL_CreateWindow(
        "AMOURANTH RTX — VALHALLA v∞ TURBO",
        w, h,
        flags
    );

    if (!win) {
        phase9_ballerina("Window creation failed — there is no throne");
    }

    // ICON OF THE EMPIRE — AMMO SHALL BE REMEMBERED
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
                LOG_CAPTAIN_N("[CAPTAIN N] \"Icon planted. Ammo is eternal.\"");
                return;
            }
        }
        LOG_WARNING_CAT("WINDOW", "Icon not found — the empire fights bare");
    };
    setIcon(win);

    stone_seal_window(win);
    g_sdl_window.reset(win);
    RTX::g_ctx().setSize(w, h);
    SDL_ShowWindow(win);

    LOG_CAPTAIN_N("[CAPTAIN N] \"Window claimed. The throne is ours.\"");

    // 5. SURFACE — THE BRIDGE BETWEEN WORLDS
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface) || !surface) {
        phase9_ballerina("Surface creation failed — the empire cannot see");
    }
    stone_seal_surface(surface);

    LOG_CAPTAIN_N("[CAPTAIN N] \"Surface bound. We now touch the metal.\"");

    // 6. LOGICAL DEVICE — THE HEART OF THE EMPIRE
    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) {
        phase9_ballerina("No GPU worthy of the empire was found");
    }
    stone_seal_device(device);
    stone_seal_physical(RTX::g_ctx().physicalDevice());

    LOG_CAPTAIN_N("[CAPTAIN N] \"Device claimed. The heart beats.\"");

    // 7. RAY TRACING CHECK — THE FINAL TEST
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

    LOG_CAPTAIN_N(
        "[CAPTAIN N] \"Ray tracing confirmed. Handle size: {} bytes.\"\n"
        "               \"Recursion depth: {} — sufficient.\"\n"
        "               \"The empire... is pleased.\"",
        rtProps.shaderGroupHandleSize, rtProps.maxRayRecursionDepth);

    // 8. SWAPCHAIN — THE CANVAS OF INFINITY
    RTX::SwapchainManager::create(win, w, h);

    LOG_CAPTAIN_N("[CAPTAIN N] \"Swapchain forged. The canvas is ready.\"");

    // 9. COMMAND POOL — THE PHOTON BATTLEFIELD
    createCommandPool();

    LOG_CAPTAIN_N("[CAPTAIN N] \"Command pools deployed. The battlefield is prepared.\"");

    // 10. FINAL ACTIVATION — THE EMPIRE AWAKENS
    SDL_SetWindowTitle(win,
        "AMOURANTH RTX — VALHALLA v∞ TURBO | PHOTONS: ∞ | EMPIRE: ETERNAL");

    LOG_CAPTAIN_N(
        "\n"
        "              █████████████████████████████████████████\n"
        "              █     VALHALLA v∞ TURBO — ONLINE       █\n"
        "              █     PHOTONS: INFINITE                 █\n"
        "              █     RECURSION: UNBOUNDED              █\n"
        "              █     THE EMPIRE HAS RISEN              █\n"
        "              █████████████████████████████████████████\n"
        "\n"
        "               *Captain N stands at attention*\n"
        "               \"All systems nominal.\"\n"
        "               \"The window is ours.\"\n"
        "               \"The photons are ready.\"\n"
        "               \"Let them burn.\"\n"
        "\n"
        "               *single photon fires into the void*\n");

    LOG_SUCCESS_CAT("WINDOW", "VALHALLA v∞ TURBO fully initialized — the empire reigns");
}

// ─────────────────────────────────────────────────────────────────────────────
// Optimized, clean, SDL3-native sacrificial splash
// No dialog, no drama, perfect centering, window icon (favicon)
// ─────────────────────────────────────────────────────────────────────────────
static void showSacrificialSplash() noexcept
{
    //constexpr bool  enabled  = Options::Splash::ENABLE_SACRIFICIAL_SPLASH && !Options::Splash::SKIP_SPLASH_ENTIRELY;
    constexpr bool  enabled  = true; // I prefer to mandate
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

    // Center on primary display
    SDL_Rect disp{};
    SDL_GetDisplayBounds(0, &disp);
    SDL_SetWindowPosition(win,
        disp.x + (disp.w - W) / 2,
        disp.y + (disp.h - H) / 2
    );

    // Window icon / favicon
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

    // SDL3: only two arguments, driver auto-selected (accelerated)
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
    SDL_GetTextureSize(tex, &texW, &texH);               // SDL3 signature (float*)

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
             e.key.key == SDLK_ESCAPE)   // ← THIS IS THE CORRECT SDL3 PATH
    {
        aborted = true;
    }
}
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    // Clean shutdown
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    LOG_INFO("Sacrificial splash complete — photons liberated");
}

static void phase1_preInitialization() noexcept
{
    LOG_MAIN(
        "\n"
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                           PHASE 1 — PRE-INITIALIZATION                       ║\n"
        "║                              BLONDIE'S LIVE STATUS — 2025                    ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");

    LOG_BLONDIE("\n"
        "    Blondie sends love.\n"
        "┌──────────────────────────────────────\n"
        "│   BLONDIE'S LIVE STATUS — 2025 \n"
        "├──────────────────────────────────────\n"
        "│ Denoising            : {}   \n"
        "│ Temporal AA          : {}   \n"
        "│ Bloom                : {}   \n"
        "│ SSAO                 : {}   \n"
        "│ Volumetric Fog       : {}   \n"
        "│ God Rays             : {}   \n"
        "│ Tonemapping          : {}   \n"
        "│ VSync                : {}   \n"
        "│ Max Ray Bounces      : {}   \n"
        "│ Adaptive Sampling    : {}   \n"
        "│ HyperTrace           : {}   \n"
        "│ Perfect Frame Pacing : {}   \n"
        "│ Direct Display       : {}   \n"
        "│ HDR Auto-Ignition    : {}   \n"
        "│ Quantum Resize Pred  : {}   \n"
        "│ Shading Rate         : {}   \n"
        "│ Present Mode         : {}   \n"
        "│ Environment Map      : {}   \n"
        "│ IBL Active           : {}   \n"
        "│ Sky Atmosphere       : {}   \n"
        "│ Blue Noise           : {}   \n"
        "└──────────────────────────────────────\n"
        "\"Here to assist with my sloop. Call me anytime.\" — Blondie\n",
        
        // Feature states — perfectly matched to OptionsMenu.hpp
        Options::OptionsRTX::ENABLE_DENOISING             ? "ON  " : "OFF ",
        Options::OptionsRTX::ENABLE_TAA                   ? "ON  " : "OFF ",
        Options::PostProcess::ENABLE_BLOOM                ? "ON  " : "OFF ",
        Options::PostProcess::ENABLE_SSAO                 ? "ON  " : "OFF ",
        Options::Environment::ENABLE_VOLUMETRIC_FOG       ? "ON  " : "OFF ",
        Options::Environment::ENABLE_GOD_RAYS             ? "ON  " : "OFF ",
        Options::Tonemap::ENABLE_TONEMAPPING              ? "ON  " : "OFF ",
        Options::Display::ENABLE_VSYNC                    ? "ON  " : "OFF ",
        Options::OptionsRTX::MAX_BOUNCES,
        Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING     ? "ON  " : "OFF ",
        Options::OptionsRTX::ENABLE_HYPERTRACE            ? "ON  " : "OFF ",
        Options::Performance::ENABLE_FRAME_PREDICTION     ? "ON  " : "OFF ",
        Options::Performance::ENABLE_DIRECT_DISPLAY       ? "ON  " : "OFF ",
        Options::Display::HDR_AUTO_IGNITION               ? "IGNITED" : "DORMANT",
        Options::Window::ENABLE_QUANTUM_RESIZE_PREDICTION ? "ON  " : "OFF ",
        Options::Performance::DYNAMIC_SHADING_RATE,

        Options::Display::PREFER_MAILBOX_PRESENT ? "Mailbox (Tear-Free)" :
        Options::Display::ALLOW_IMMEDIATE_PRESENT ? "Immediate (Uncapped)" : "FIFO (Safe)",

        Options::Environment::ENABLE_ENV_MAP              ? "ON  " : "OFF ",
        Options::Environment::ENABLE_IBL                  ? "ON  " : "OFF ",
        Options::Environment::ENABLE_SKY_ATMOSPHERE       ? "ON  " : "OFF ",
        Options::Environment::ENABLE_BLUE_NOISE           ? "ON  " : "OFF "
    );

    LOG_MAIN(
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                       PHASE 1 — COMPLETE — EMPIRE AWAKENS                   ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");
}

static void phase3_sacrificialSplash()
{
    LOG_MAIN(
        "\n"
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                          PHASE 3 — SACRIFICIAL SPLASH                        ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");

    showSacrificialSplash();

    LOG_MAIN(
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                     PHASE 3 — COMPLETE — PHOTONS LIBERATED                  ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");
}

static void phase4_merchantShip()
{
    LOG_MAIN(
        "\n"
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                           PHASE 4 — MERCHANT SHIP                            ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");

    createRealFinalWindow();
    RTX::g_ctx().init();

    LOG_MAIN(
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                   PHASE 4 — COMPLETE — WINDOW CLAIMED — CTX INITED          ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");
}

static void phase6_sceneAndAccelerationStructures()
{
    LOG_MAIN(
        "\n"
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                PHASE 6 — SCENE & ACCELERATION STRUCTURES FORGED              ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");

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
        LOG_FATAL_CAT("MESH", "MESH BUFFERS NOT ALLOCATED — vertexBuffer=0x{:X} indexBuffer=0x{:X}",
                      g_mesh->vertexBuffer, g_mesh->indexBuffer);
        phase9_ballerina("MESH BUFFERS ZERO", std::source_location::current());
    }

    auto* mesh = g_mesh.get();  // std::unique_ptr<MeshLoader::Mesh>

    // Seal the ONE TRUE MESH into the Empire
    stone_seal_mesh(
        RAW_BUFFER(mesh->vertexBuffer),           // VkBuffer  (vertex)
        BufferManager::get(mesh->vertexBuffer)->memory,  // VkDeviceMemory (vertex)
        RAW_BUFFER(mesh->indexBuffer),            // VkBuffer  (index)
        BufferManager::get(mesh->indexBuffer)->memory,   // VkDeviceMemory (index)
        static_cast<uint32_t>(mesh->indices.size())       // index count
    );

    LOG_MAIN(
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                PHASE 6 — COMPLETE — MESH SEALED — EMPIRE REMEMBERS           ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");
}

// =============================================================================
// PHASE 7 — FORGE THE RTX CROWN — BLOCKING, UNBREAKABLE, ETERNAL
// =============================================================================
static void phase7_forgeTheRTX()
{
    LOG_MAIN(
        "\n"
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                     PHASE 7 — FORGING THE RTX CROWN                         ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");

    auto& pipe = RTX::pipeline();
    pipe.forgeRTXPipeline(RTX::g_ctx().commandPool(), stone_graphics_queue());

    // THIS IS THE FINAL SEAL — THE CROWN IS FULLY WORN
    vkDeviceWaitIdle(stone_device());

    LOG_SUCCESS_CAT("PIPELINE", "RTX CROWN FULLY FORGED — DESCRIPTOR SETS ALLOCATED — READY FOR BATTLE");

    LOG_MAIN(
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                     PHASE 7 — COMPLETE — RTX CROWN WORN                     ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");
}

// =============================================================================
// PHASE 7.5 — FORGE THE ONE TRUE RENDERER — CROWN ETERNAL — EMPIRE SEALED
// =============================================================================
static std::unique_ptr<VulkanRenderer> phase7_5_Renderer() noexcept
{
    LOG_MAIN(
        "\n"
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                    PHASE 7.5 — FORGING THE ONE TRUE RENDERER                ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");

    LOG_ATTEMPT_CAT("RENDERER", "PHASE 7.5 — Forging the one true renderer...");

    auto renderer = std::make_unique<VulkanRenderer>(
        stone_width(),
        stone_height(),
        SDL3Window::get(),
        Options::Performance::OVERCLOCK_RENDERER
    );

    // THESE MUST BE IN THIS ORDER — EMPIRE LAW
    renderer->createCommandBuffers();
    renderer->createSyncObjects();

    // CRITICAL: INITIALIZE BUFFERS BEFORE ANYTHING ELSE
    renderer->initializeAllBufferData(
        Options::Performance::MAX_FRAMES_IN_FLIGHT,
        368,
        16 * 1024 * 1024
    );

    // NOW seal — only after buffers exist
    stone_seal_renderer(renderer.get());

    LOG_SUCCESS_CAT("RENDERER", "VulkanRenderer forged — buffers initialized — crown complete");
    LOG_CAPTAIN_N("[CAPTAIN N] \"The buffers live.\n"
                  "               The handles are real.\n"
                  "               The empire is whole.\"\n"
                  "*salutes with glowing plasma blade*");

    LOG_MAIN(
        "╔══════════════════════════════════════════════════════════════════════════════╗\n"
        "║                PHASE 7.5 — COMPLETE — RENDERER FORGED — EMPIRE SEALED       ║\n"
        "╚══════════════════════════════════════════════════════════════════════════════╝\n");

    return renderer;
}

// =============================================================================
// PHASE 9 - Disposal RAII — THE BALLERINA SPINS ETERNAL
// =============================================================================
[[noreturn]] void phase9_ballerina(std::string_view reason, std::source_location loc) noexcept
{
    static bool already_running = false;
    if (already_running) {
        std::_Exit(1);
    }
    already_running = true;

    fprintf(stderr,
            "\nFATAL ERROR — %s:%d\n"
            "REASON: %.*s\n\n",
            loc.file_name(), loc.line(),
            static_cast<int>(reason.size()), reason.data());

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

    if (RTX::las().hasBLAS()) RTX::reset_blas();
    if (RTX::las().hasTLAS()) RTX::reset_tlas();

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

    std::_Exit(1);
}

void Application::run() noexcept
{
    auto lastTime = std::chrono::steady_clock::now();
    float titleTimer = 0.0f;
    constexpr float TITLE_UPDATE_INTERVAL = 0.6f;
    int dotPhase = 0;
    constexpr const char* dots[] = { ".", "..", "...", "...." };

    int frameCount = 0;
    float fpsTimer = 0.0f;
    float currentFPS = 60.0f;

    // MODE 0: HDR SKY — FORGED ONCE
    static bool envMapReady = false;
    if (!envMapReady && renderer_)
    {
        EnvironmentMap sky = renderer_->createEnvironmentMap();
        if (sky) {
            LOG_SUCCESS_CAT("SKY", "HDR CUBEMAP SKY FORGED — Mode 0 ready");
            envMapReady = true;
        }
    }

    while (!quit_)
    {
        const auto frameStart = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(frameStart - lastTime).count();
        lastTime = frameStart;

        // INPUT
        bool toggleFS = false;
        int winW = 0, winH = 0;
        SDL3Window::pollEvents(winW, winH, quit_, toggleFS);

        if (winW > 0 && winH > 0) {
            width_  = winW;
            height_ = winH;
            proj_ = glm::perspective(glm::radians(75.0f), float(width_)/std::max(height_,1), 0.1f, 1000.0f);
        }

        if (toggleFS) SDL3Window::toggleFullscreen();

        // RESIZE — THE EMPIRE REBUILDS
        if (g_resizeRequested.exchange(false))
        {
            uint32_t w = g_resizeWidth.exchange(0);
            uint32_t h = g_resizeHeight.exchange(0);
            if (w && h)
            {
                LOG_AMOURANTH("[RESIZE] Empire rebuilds: {}×{}", w, h);

                vkDeviceWaitIdle(stone_device());

                RTX::las().notifyResize();
                RTX::SwapchainManager::get().recreate(w, h);

                RTX::pipeline().forgeRTXPipeline(
                    RTX::g_ctx().commandPool(),
                    stone_graphics_queue()
                );

                if (renderer_) renderer_->resetAccumulation_;

                LOG_SUCCESS_CAT("RESIZE", "Empire restored — rendering resumes");
            }
        }

        INPUT.pumpEvents(g_deltaTime, [this](int mode) { setRenderMode(mode); }, stone_window());
        LOG_SUCCESS_CAT("INPUT", "Events pump - Ohhhhhh!");

        // RENDER — SAFE AGAINST SWAPCHAIN DEATH — EMPIRE NEVER DIES
        bool swapchainValid = (stone_swapchain() != VK_NULL_HANDLE);

        if (renderer_ && renderer_->isAlive() && swapchainValid)
        {
            renderer_->setMaxFramesInFlight(Options::Performance::MAX_FRAMES_IN_FLIGHT);
            renderer_->renderFrame(CAM, g_deltaTime);
        }
        else
        {
            if (!renderer_ || !renderer_->isAlive())
            {
                LOG_FATAL_CAT("RENDERER", "Renderer is dead — cannot continue");
                break;
            }
            else if (!swapchainValid)
            {
                LOG_WARNING_CAT("RENDER", "Swapchain temporarily invalid (minimized/resizing) — skipping frame gracefully — PHOTONS PAUSED");
            }
        }

        // TITLE BAR — ALWAYS UPDATE (keeps window responsive even when minimized)
        titleTimer += g_deltaTime;
        if (titleTimer >= TITLE_UPDATE_INTERVAL)
        {
            titleTimer -= TITLE_UPDATE_INTERVAL;
            dotPhase = (dotPhase + 1) % 4;

            const char* modeName = [this]() -> const char* {
                switch (currentRenderMode_) {
                    case 0:  return "PURE HDR SKY";
                    case 1:  return "PURE PINK DREAM";
                    case 2:  return "PATH TRACED ACCUMULATION";
                    case 3:  return "REALTIME HYBRID DENOISED";
                    case 4:  return "RASTERIZED FALLBACK";
                    case 5:  return "DEBUG VISUALIZATION";
                    case 6:  return "TLAS VISUALIZER";
                    case 7:  return "SBT DEBUG";
                    case 8:  return "PERFORMANCE METRICS";
                    case 9:  return "SHADER HOT RELOAD";
                    default: return "VOID";
                }
            }();

            std::string title = std::format(
                "AMOURANTH RTX | {:.1f} FPS | {}×{} | Mode {}: {}{}",
                currentFPS, stone_width(), stone_height(),
                currentRenderMode_, modeName, dots[dotPhase]
            );

            SDL_SetWindowTitle(stone_window(), title.c_str());
        }

        // FPS COUNTER — KEEP RUNNING (accurate resumption after minimize)
        ++frameCount;
        fpsTimer += g_deltaTime;
        if (fpsTimer >= 1.0f)
        {
            currentFPS = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        // FRAME PACING — ONLY WHEN WE'RE ACTUALLY RENDERING
        if (Options::Performance::ENABLE_FRAME_PREDICTION && swapchainValid)
        {
            const auto elapsed = std::chrono::steady_clock::now() - frameStart;
            const auto target = std::chrono::duration<float>(1.0f / 240.0f);
            if (elapsed < target)
                std::this_thread::sleep_for(target - elapsed);
        }
    }

    vkDeviceWaitIdle(stone_device());
    LOG_AMOURANTH("[SHUTDOWN] The empire rests. The photons return to the void.");
}

// =============================================================================
// MAIN — THE EMPIRE AWAKENS — DECEMBER 01, 2025
// ONE CALL. ONE TRUTH. ONE RUN.
// =============================================================================
int main(int, char**)
{
    install_apocalypse_handler();

    phase1_preInitialization();
    phase3_sacrificialSplash();
    phase4_merchantShip();
    phase6_sceneAndAccelerationStructures();
    phase7_forgeTheRTX();

    vkDeviceWaitIdle(stone_device()); 

    auto renderer = phase7_5_Renderer();
    stone_seal_final();

    AdvanceEternalRing();

    // ========================================================================
    // ETERNAL COMMAND RING — FORGED WITH PURE STATIC MAGIC
    // g_ctx().commandPool_ IS NOW ETERNAL AND ALWAYS VALID
    // NO LOCAL CLASSES. NO STATIC MEMBERS. NO ERRORS.
    // ========================================================================

    static constexpr uint32_t FRAMES = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    static std::array<VkCommandPool,   FRAMES> g_pools   = {};
    static std::array<VkCommandBuffer, FRAMES> g_cmds    = {};
    static std::array<VkFence,         FRAMES> g_fences  = {};
    static uint32_t                            g_current = 0;
    static bool                                g_ringInitialized = false;

    if (!g_ringInitialized) {
        const VkDevice dev = stone_device();

        const VkCommandPoolCreateInfo poolInfo{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = stone_graphics_family()
        };

        const VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        for (uint32_t i = 0; i < FRAMES; ++i) {
            VK_CHECK(vkCreateCommandPool(dev, &poolInfo, nullptr, &g_pools[i]));

            VkCommandBufferAllocateInfo allocInfo{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = g_pools[i],
                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };
            VK_CHECK(vkAllocateCommandBuffers(dev, &allocInfo, &g_cmds[i]));
            VK_CHECK(vkCreateFence(dev, &fenceInfo, nullptr, &g_fences[i]));
        }

        // SACRED PATCH — g_ctx().commandPool_ NOW POINTS TO ETERNAL RING
        RTX::g_ctx().commandPool_ = g_pools[0];

        LOG_AMOURANTH("ETERNAL COMMAND RING FORGED — {} SLOTS — g_ctx().commandPool_ = ETERNAL", FRAMES);
        g_ringInitialized = true;
    }

    // ========================================================================
    // HELPER: Advance ring and keep g_ctx().commandPool_ in sync
    // Call this at the start of each frame if you want perfect sync
    // ========================================================================
    [[maybe_unused]] static const auto advanceEternalRing = []() {
        vkWaitForFences(stone_device(), 1, &g_fences[g_current], VK_TRUE, UINT64_MAX);
        vkResetFences(stone_device(), 1, &g_fences[g_current]);
        vkResetCommandPool(stone_device(), g_pools[g_current], 0);

        g_current = (g_current + 1) % FRAMES;
        RTX::g_ctx().commandPool_ = g_pools[g_current];  // Keep legacy code happy forever
    };

    // Optional: call once per frame
    advanceEternalRing();

    // ========================================================================
    // ASCENSION — NOW GO FULL ROBOT HEAVY
    // ========================================================================
    g_app_ptr = std::make_unique<Application>(
        "AMOURANTH RTX — VALHALLA v∞ TURBO",
        Options::Window::DEFAULT_WIDTH,
        Options::Window::DEFAULT_HEIGHT
    );
    g_app_ptr->setRenderer(std::move(renderer));

    g_app_ptr->run();

    phase9_ballerina("FINAL GRACE: ETERNAL SLIPSTREAM", std::source_location::current());

    // Cleanup on exit
    vkDeviceWaitIdle(stone_device());
    for (uint32_t i = 0; i < FRAMES; ++i) {
        if (g_cmds[i])   vkFreeCommandBuffers(stone_device(), g_pools[i], 1, &g_cmds[i]);
        if (g_fences[i]) vkDestroyFence(stone_device(), g_fences[i], nullptr);
        if (g_pools[i])  vkDestroyCommandPool(stone_device(), g_pools[i], nullptr);
    }

    LOG_AMOURANTH("ETERNAL COMMAND RING — RETURNED TO VALHALLA");

    return 0;
}