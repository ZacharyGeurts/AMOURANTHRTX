// src/main.cpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 16, 2025 — APOCALYPSE v3.4
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — KEYS NEVER LOGGED
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 — APOCALYPSE v13.2 — BASTION EDITION
// ALL SACRED GLOBALS DECLARED UP FRONT — PUBLIC BY DECREE — STONEKEY PROTECTS THE CORE
// PINK PHOTONS ETERNAL — VALHALLA UNBREACHABLE — FIRST LIGHT ACHIEVED
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
#include "engine/Vulkan/VulkanPipelineManager.hpp"

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
// BASTION GLOBALS — PUBLIC BY LAW — DECLARED UP FRONT — VISIBLE TO ALL
// THESE ARE THE PILLARS OF VALHALLA — STONEKEY PROTECTS THEIR TRUE FORM
// =============================================================================

// Preloaded Icons — shared with all who dare look
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

// Swapchain Runtime Configuration — visible to all developers
struct SwapchainRuntimeConfig {
    VkPresentModeKHR desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
    bool forceVsync = false;
    bool forceTripleBuffer = true;
    bool enableHDR = false;
    bool logFinalConfig = true;
};
static SwapchainRuntimeConfig g_swapchain_config;

// Application — the one true window into Valhalla
static std::unique_ptr<Application> g_app = nullptr;

// Renderer — forged in pink fire — THE ONE TRUE RENDERER — DECLARED HERE
std::unique_ptr<VulkanRenderer> g_renderer = nullptr;

// Pipeline Manager — the beating heart of ray tracing — PUBLIC AND PROUD
static RTX::PipelineManager* g_pipeline_manager = nullptr;

// =============================================================================
// Present Mode Detection — chooses the path of least tearing
// =============================================================================
static void detectBestPresentMode(VkPhysicalDevice phys, VkSurfaceKHR surface) {
    LOG_INFO_CAT("MAIN", "Detecting optimal present mode...");
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, nullptr);
    if (count == 0) { g_swapchain_config.desiredMode = VK_PRESENT_MODE_FIFO_KHR; return; }

    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, modes.data());

    const char* driver = SDL_GetCurrentVideoDriver();
    bool isX11 = (driver && std::string(driver) == "x11");

    VkPresentModeKHR preferred = isX11 ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_MAILBOX_KHR;

    if (std::find(modes.begin(), modes.end(), preferred) != modes.end()) {
        g_swapchain_config.desiredMode = preferred;
        LOG_SUCCESS_CAT("MAIN", "{} SELECTED — LOW LATENCY NO TEARING", 
                        preferred == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" : "MAILBOX");
    } else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) {
        g_swapchain_config.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        LOG_INFO_CAT("MAIN", "IMMEDIATE fallback — min latency");
    } else {
        g_swapchain_config.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
        LOG_INFO_CAT("MAIN", "FIFO selected — vsync safe");
    }
    SwapchainManager::setDesiredPresentMode(g_swapchain_config.desiredMode);
}

inline void bulkhead(const std::string& title) {
    LOG_INFO_CAT("MAIN", "════════════════════════════════ {} ════════════════════════════════", title);
}

// =============================================================================
// PHASES — APOCALYPSE v13.2 — BASTION EDITION
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
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        throw std::runtime_error(std::format("SDL_InitSubSystem(VIDEO) failed: {}", SDL_GetError()));
    }
}

static void phase1_splash() {
    bulkhead("PHASE 1: SPLASH");
    Splash::show("AMOURANTH RTX", 1280, 720, "assets/textures/ammo.png", "assets/audio/ammo.wav");
    LOG_SUCCESS_CAT("MAIN", "Splash complete — awakening main window");
}

static void phase2_mainWindow() {
    bulkhead("PHASE 2: MAIN WINDOW");
    constexpr int W = 3840, H = 2160;
    Uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (Options::Performance::ENABLE_IMGUI) flags |= SDL_WINDOW_RESIZABLE;

    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", W, H, flags);
    auto* win = SDL3Window::get();
    if (!win) throw std::runtime_error("Main window creation failed");

    if (g_base_icon) {
        if (g_hdpi_icon) SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
        SDL_SetWindowIcon(win, g_base_icon);
    }

    LOG_SUCCESS_CAT("MAIN", "Main window forged — ready for first light");
}

static void phase3_vulkanContext(SDL_Window* window) {
    bulkhead("PHASE 3: VULKAN CONTEXT + SWAPCHAIN");

    VkInstance instance = RTX::createVulkanInstanceWithSDL(window, true);
    RTX::initContext(instance, window, 3840, 2160);

    set_g_PhysicalDevice(RTX::g_ctx().physicalDevice());

    SwapchainManager::init(window, 3840, 2160);

    detectBestPresentMode(RTX::g_ctx().physicalDevice(), RTX::g_ctx().surface());

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    LOG_SUCCESS_CAT("MAIN", "Vulkan empire forged — swapchain alive — FIRST LIGHT ACHIEVED");
}

static void phase4_appAndRendererConstruction() {
    bulkhead("PHASE 4: RENDERER + APP FORGE");

    // Forge the eternal g_rtx
    createGlobalRTX(3840, 2160);

    // Create pipeline manager — THE ONE TRUE GLOBAL PIPELINE — PUBLIC AND ETERNAL
    g_pipeline_manager = new RTX::PipelineManager(RTX::g_ctx().vkDevice(), RTX::g_ctx().physicalDevice());

    // Create renderer — binds to current g_rtx()
    g_renderer = std::make_unique<VulkanRenderer>(3840, 2160, SDL3Window::get(), !Options::Window::VSYNC);

    // Create app — takes ownership of renderer
    g_app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);
    g_app->setRenderer(std::move(g_renderer));

    LOG_SUCCESS_CAT("MAIN", "Renderer + App forged — g_pipeline() ready — PINK PHOTONS ETERNAL");
}

static void phase5_renderLoop() {
    bulkhead("PHASE 5: RENDER LOOP");
    g_app->run();
}

static void phase6_shutdown() {
    bulkhead("PHASE 6: SHUTDOWN");

    // Destroy in reverse order of creation
    g_app.reset();                          // Destroys Application → calls renderer cleanup
    g_renderer.reset();                     // Explicit safety — already moved, but harmless

    if (g_pipeline_manager) {
        delete g_pipeline_manager;
        g_pipeline_manager = nullptr;
    }

    RTX::shutdown();                        // Destroys g_rtx, context, etc.
    SDL_Quit();

    LOG_SUCCESS_CAT("MAIN", "Clean shutdown — empire preserved — PINK PHOTONS ETERNAL");
}

// =============================================================================
// MAIN — APOCALYPSE v13.2 — BASTION EDITION — FINAL
// =============================================================================
int main(int argc, char* argv[]) {
    LOG_ATTEMPT_CAT("MAIN", "=== AMOURANTH RTX — VALHALLA v80 TURBO — BASTION EDITION ===");

    try {
        phase0_cliAndStonekey(argc, argv);
        phase0_5_iconPreload();
        prePhase1_earlySdlInit();
        phase1_splash();
        phase2_mainWindow();
        phase3_vulkanContext(SDL3Window::get());
        phase4_appAndRendererConstruction();
        phase5_renderLoop();
        phase6_shutdown();
    }
    catch (const std::exception& e) {
        std::cerr << "FATAL EXCEPTION: " << e.what() << std::endl;
        phase6_shutdown();
        return -1;
    }
    catch (...) {
        std::cerr << "UNKNOWN FATAL EXCEPTION" << std::endl;
        phase6_shutdown();
        return -1;
    }

    LOG_SUCCESS_CAT("MAIN", "=== EXIT CLEAN — EMPIRE ETERNAL ===");
    return 0;
}