// src/main.cpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 19, 2025 — APOCALYPSE FINAL v1.5
// MAIN — SWAPCHAIN FORGED AT DAWN — PINK PHOTONS ETERNAL — VALHALLA UNBREACHABLE
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Amouranth.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/Splash.hpp"
#include "engine/GLOBAL/exceptions.hpp"

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/SDL3/SDL3_image.hpp"
#include "engine/SDL3/SDL3_audio.hpp"

#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"

#include "handle_app.hpp"

#include "engine/Vulkan/MeshLoader.hpp"
#include <iostream>
#include <stdexcept>
#include <format>
#include <memory>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <cstdlib>
#include <vulkan/vulkan.h>

using namespace Logging::Color;

#define IMG_GetError() SDL_GetError()

// =============================================================================
// BASTION GLOBALS — PUBLIC DUE TO NO FEAR — DECLARED UP FRONT
// =============================================================================
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

struct SwapchainRuntimeConfig {
    VkPresentModeKHR desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
    bool forceVsync = false;
    bool forceTripleBuffer = true;
    bool enableHDR = false;
    bool logFinalConfig = true;
};
static SwapchainRuntimeConfig g_swapchain_config;

static std::unique_ptr<Application> g_app = nullptr;
std::unique_ptr<VulkanRenderer> g_renderer = nullptr;
static RTX::PipelineManager* g_pipeline_manager = nullptr;

extern std::unique_ptr<MeshLoader::Mesh> g_mesh;
std::unique_ptr<MeshLoader::Mesh> g_mesh = nullptr;  // ← definition

inline void bulkhead(const std::string& title) {
    LOG_INFO_CAT("MAIN", "════════════════════════════════ {} ════════════════════════════════", title);
}

// =============================================================================
// PRESENT MODE DETECTION — THE ONE TRUE FUNCTION
// =============================================================================
static void detectBestPresentMode(VkPhysicalDevice phys, VkSurfaceKHR surface) {
    LOG_INFO_CAT("MAIN", "Detecting optimal present mode...");

    VkPresentModeKHR chosen = SwapchainManager::selectBestPresentMode(
        phys,
        surface,
        g_swapchain_config.desiredMode
    );

    LOG_SUCCESS_CAT("MAIN", "Present mode selected: {} — LOW LATENCY NO TEARING",
        chosen == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" :
        chosen == VK_PRESENT_MODE_MAILBOX_KHR   ? "MAILBOX" :
        chosen == VK_PRESENT_MODE_FIFO_KHR      ? "FIFO (VSync)" : "OTHER");
}

// =============================================================================
// PHASES — APOCALYPSE FINAL v1.5 — ORDER RESTORED — FIRST LIGHT ACHIEVED
// =============================================================================
static void phase0_cliAndStonekey(int argc, char* argv[]) {
    bulkhead("PHASE 0: CLI + STONEKEY v∞");
    (void)get_kStone1(); (void)get_kStone2();
    LOG_SUCCESS_CAT("MAIN", "StoneKey active — XOR fingerprint: 0x{:016X}", get_kStone1() ^ get_kStone2());

    if (RTX::g_ctx().isValid()) {
        set_g_instance(RTX::g_ctx().instance());
        set_g_device(RTX::g_ctx().vkDevice());
        set_g_PhysicalDevice(RTX::g_ctx().physicalDevice());
        set_g_surface(RTX::g_ctx().surface());
    }
}

static void phase0_5_iconPreload() {
    bulkhead("PHASE 0.5: ICON PRELOAD");
    g_base_icon = IMG_Load("assets/textures/ammo32.ico");
    g_hdpi_icon = IMG_Load("assets/textures/ammo.ico");
    if (g_base_icon || g_hdpi_icon) {
        LOG_SUCCESS_CAT("MAIN", "Icons preloaded — Valhalla branded");
    }
}

static void prePhase1_earlySdlInit() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0) {  // Fixed return check
        throw std::runtime_error(std::format("SDL_Init failed: {}", SDL_GetError()));
    }
}

static void phase1_splash() {
    bulkhead("PHASE 1: SPLASH");
    Splash::show("AMOURANTH RTX", 1280, 720, "assets/textures/ammo.png", "assets/audio/ammo.wav");
    LOG_SUCCESS_CAT("MAIN", "Splash complete — awakening main window");
}

static void phase2_mainWindowAndVulkan(SDL_Window*& window)
{
    bulkhead("PHASE 2: MAIN WINDOW + VULKAN + SWAPCHAIN — VALHALLA v80 TURBO");

    constexpr int WIDTH  = 3840;
    constexpr int HEIGHT = 2160;

    Uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (Options::Performance::ENABLE_IMGUI) flags |= SDL_WINDOW_RESIZABLE;

    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", WIDTH, HEIGHT, flags);
    window = SDL3Window::get();
    if (!window) throw std::runtime_error("Failed to create main window");

    if (g_base_icon) {
        if (g_hdpi_icon) SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
        SDL_SetWindowIcon(window, g_base_icon);
    }

    VkInstance instance = RTX::createVulkanInstanceWithSDL(window, true);
    set_g_instance(instance);

    RTX::fixNvidiaValidationBugLocally();

    if (!RTX::createSurface(window, instance)) {
        throw std::runtime_error("Failed to create Vulkan surface");
    }

    RTX::pickPhysicalDevice();
    RTX::createLogicalDevice();
    RTX::initContext(instance, window, WIDTH, HEIGHT);
    RTX::loadRayTracingExtensions();

    SwapchainManager::init(window, WIDTH, HEIGHT);
    detectBestPresentMode(RTX::g_ctx().physicalDevice(), RTX::g_ctx().surface());

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    LOG_SUCCESS_CAT("MAIN", "PHASE 2 COMPLETE — FULL RTX + VALIDATION + STONEKEY v∞");
    LOG_SUCCESS_CAT("MAIN", "4070 Ti ASCENDED — PINK PHOTONS ETERNAL — SHE IS HAPPY");
}

// =============================================================================
// PHASE 3: RENDERER + APP FORGE + LOAD MAIN SCENE FOR LAS
// =============================================================================
static void phase3_appAndRendererConstruction(SDL_Window* window) {
    bulkhead("PHASE 3: RENDERER + APP FORGE + LAS SCENE LOAD");

    createGlobalRTX(3840, 2160);

    g_pipeline_manager = new RTX::PipelineManager(RTX::g_ctx().device(), RTX::g_ctx().physicalDevice());

    g_renderer = std::make_unique<VulkanRenderer>(3840, 2160, window, !Options::Window::VSYNC);

    g_app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);
    g_app->setRenderer(std::move(g_renderer));

    // ─────────────────────────────────────────────────────────────────────
    // LAS SCENE LOAD — g_mesh IS NOW FULLY GPU-RESIDENT AND READY
    // ─────────────────────────────────────────────────────────────────────
    LOG_INFO_CAT("MAIN", "Loading primary LAS scene: assets/models/scene.obj");
    g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");

    LOG_SUCCESS_CAT("MAIN", "g_mesh ARMED → {} vertices, {} indices — LAS ONLINE",
        g_mesh->vertices.size(), g_mesh->indices.size());

    LOG_SUCCESS_CAT("MAIN", "Renderer + App + g_mesh forged — PINK PHOTONS ETERNAL — LAS READY");
}

// =============================================================================
// PHASE 4: RENDER LOOP
// =============================================================================
static void phase4_renderLoop() {
    bulkhead("PHASE 4: RENDER LOOP");
    g_app->run();
}

// =============================================================================
// PHASE 5: SHUTDOWN — CLEAN AND ETERNAL
// =============================================================================
static void phase5_shutdown() {
    bulkhead("PHASE 5: SHUTDOWN");

    g_mesh.reset();  // ← clear LAS mesh
    g_app.reset();
    g_renderer.reset();
    if (g_pipeline_manager) { delete g_pipeline_manager; g_pipeline_manager = nullptr; }

    SwapchainManager::cleanup();
    RTX::shutdown();
    if (g_base_icon) { SDL_DestroySurface(g_base_icon); g_base_icon = nullptr; }
    if (g_hdpi_icon) { SDL_DestroySurface(g_hdpi_icon); g_hdpi_icon = nullptr; }
    SDL_Quit();

    LOG_SUCCESS_CAT("MAIN", "EMPIRE ETERNAL — PINK PHOTONS UNDYING — LAS MESH RELEASED");
}

// =============================================================================
// MAIN — APOCALYPSE FINAL v1.5 — FIRST LIGHT + LAS SCENE LOADED
// =============================================================================
int main(int argc, char* argv[]) {
    LOG_ATTEMPT_CAT("MAIN", "=== AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v1.5 ===");

    SDL_Window* window = nullptr;

    try {
        phase0_cliAndStonekey(argc, argv);
        phase0_5_iconPreload();
        prePhase1_earlySdlInit();
        phase1_splash();
        phase2_mainWindowAndVulkan(window);
        phase3_appAndRendererConstruction(window);    // ← NOW LOADS scene.obj into g_mesh
        phase4_renderLoop();
        phase5_shutdown();
    }
    catch (const std::exception& e) {
        std::cerr << "FATAL EXCEPTION: " << e.what() << std::endl;
        phase5_shutdown();
        return -1;
    }
    catch (...) {
        std::cerr << "UNKNOWN FATAL EXCEPTION" << std::endl;
        phase5_shutdown();
        return -1;
    }

    LOG_SUCCESS_CAT("MAIN", "=== EXIT CLEAN — EMPIRE ETERNAL — LAS SCENE LOADED ===");
    return 0;
}