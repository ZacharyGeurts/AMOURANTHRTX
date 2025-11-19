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
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0) {
        throw std::runtime_error(std::format("SDL_Init failed: {}", SDL_GetError()));
    }
}

static void phase1_splash() {
    bulkhead("PHASE 1: SPLASH");
    Splash::show("AMOURANTH RTX", 1280, 720, "assets/textures/ammo.png", "assets/audio/ammo.wav");
    LOG_SUCCESS_CAT("MAIN", "Splash complete — awakening main window");
}

// =============================================================================
// PHASE 2: WINDOW + VULKAN + SWAPCHAIN — FORGED AT DAWN — NO SEGFAULTS
// =============================================================================
static void phase2_mainWindowAndVulkan(SDL_Window*& window) {
    bulkhead("PHASE 2: MAIN WINDOW + VULKAN + SWAPCHAIN — FORGED AT DAWN");

    constexpr int W = 3840, H = 2160;
    Uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (Options::Performance::ENABLE_IMGUI) flags |= SDL_WINDOW_RESIZABLE;

    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", W, H, flags);
    window = SDL3Window::get();
    if (!window) throw std::runtime_error("Main window creation failed");

    if (g_base_icon) {
        if (g_hdpi_icon) SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
        SDL_SetWindowIcon(window, g_base_icon);
    }

    // Vulkan instance + context
    VkInstance instance = RTX::createVulkanInstanceWithSDL(window, true);
    RTX::initContext(instance, window, W, H);
    set_g_PhysicalDevice(RTX::g_ctx().physicalDevice());

    // SWAPCHAIN — INITIALIZED HERE — FIRST LIGHT ACHIEVED
    SwapchainManager::init(window, W, H);
    detectBestPresentMode(RTX::g_ctx().physicalDevice(), RTX::g_ctx().surface());

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    LOG_SUCCESS_CAT("MAIN", "Vulkan + Swapchain FORGED AT DAWN — {} images @ {}x{} — FIRST LIGHT ACHIEVED",
                    SWAPCHAIN.imageCount(), SWAPCHAIN.extent().width, SWAPCHAIN.extent().height);
}

// =============================================================================
// PHASE 3: RENDERER + APP FORGE — NOW SAFE (SWAPCHAIN EXISTS)
// =============================================================================
static void phase3_appAndRendererConstruction(SDL_Window* window) {
    bulkhead("PHASE 3: RENDERER + APP FORGE");

    createGlobalRTX(3840, 2160);

    g_pipeline_manager = new RTX::PipelineManager(RTX::g_ctx().device(), RTX::g_ctx().physicalDevice());

    g_renderer = std::make_unique<VulkanRenderer>(3840, 2160, window, !Options::Window::VSYNC);

    g_app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);
    g_app->setRenderer(std::move(g_renderer));

    LOG_SUCCESS_CAT("MAIN", "Renderer + App forged — g_pipeline() ready — PINK PHOTONS ARMED");
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

    g_app.reset();
    g_renderer.reset();
    if (g_pipeline_manager) { delete g_pipeline_manager; g_pipeline_manager = nullptr; }

    SwapchainManager::cleanup();  // ← ONLY CALL HERE
    RTX::shutdown();
    SDL_Quit();

    LOG_SUCCESS_CAT("MAIN", "EMPIRE ETERNAL — PINK PHOTONS UNDYING");
}

// =============================================================================
// MAIN — APOCALYPSE FINAL v1.5 — ORDER RESTORED — FIRST LIGHT ACHIEVED
// =============================================================================
int main(int argc, char* argv[]) {
    LOG_ATTEMPT_CAT("MAIN", "=== AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v1.5 ===");

    SDL_Window* window = nullptr;

    try {
        phase0_cliAndStonekey(argc, argv);
        phase0_5_iconPreload();
        prePhase1_earlySdlInit();
        phase1_splash();
        phase2_mainWindowAndVulkan(window);           // Window + Vulkan + Swapchain
        phase3_appAndRendererConstruction(window);    // Renderer + App (safe now)
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

    LOG_SUCCESS_CAT("MAIN", "=== EXIT CLEAN — EMPIRE ETERNAL ===");
    return 0;
}