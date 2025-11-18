// src/main.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 — APOCALYPSE v10.0 — FINAL BUILD
// FULLY COMPILES — RESIZE WORKS — FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL
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
#include "engine/Vulkan/Compositor.hpp"

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

// Preloaded Icons
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

// Swapchain config
struct SwapchainRuntimeConfig {
    VkPresentModeKHR desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
    bool forceVsync = false;
    bool forceTripleBuffer = true;
    bool enableHDR = false;
    bool logFinalConfig = true;
};
static SwapchainRuntimeConfig gSwapchainConfig;

// Present mode detection
static void detectBestPresentMode(VkPhysicalDevice phys, VkSurfaceKHR surface) {
    LOG_INFO_CAT("MAIN", "Detecting optimal present mode...");
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, nullptr);
    if (count == 0) { gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR; return; }

    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, modes.data());

    if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) {
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
        LOG_SUCCESS_CAT("MAIN", "MAILBOX SELECTED — TRIPLE BUFFERED LOW LATENCY");
    } else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) {
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        LOG_INFO_CAT("MAIN", "IMMEDIATE SELECTED — MIN LATENCY");
    } else {
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
        LOG_INFO_CAT("MAIN", "FIFO SELECTED — VSYNCSAFE");
    }
    SwapchainManager::setDesiredPresentMode(gSwapchainConfig.desiredMode);
}

inline void bulkhead(const std::string& title) {
    LOG_INFO_CAT("MAIN", "════════════════════════════════ {} ════════════════════════════════", title);
}

// =============================================================================
// PHASES — FINAL v10.0
// =============================================================================
static void phase0_cliAndStonekey(int argc, char* argv[]) {
    bulkhead("PHASE 0: CLI + STONEKEY");
    (void)get_kStone1(); (void)get_kStone2();
    LOG_SUCCESS_CAT("MAIN", "StoneKey active — XOR: 0x{:016X}", get_kStone1() ^ get_kStone2());
}

static void phase0_5_iconPreload() {
    bulkhead("PHASE 0.5: ICON PRELOAD");
    g_base_icon = IMG_Load("assets/textures/ammo32.ico");
    g_hdpi_icon = IMG_Load("assets/textures/ammo.ico");
    if (g_base_icon && g_hdpi_icon) {
        LOG_SUCCESS_CAT("MAIN", "Dual icons loaded");
    } else if ((g_base_icon = IMG_Load("assets/textures/ammo.ico"))) {
        LOG_SUCCESS_CAT("MAIN", "Fallback icon loaded");
    }
}

static void prePhase1_earlySdlInit() {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        LOG_FATAL_CAT("MAIN", "SDL_InitSubSystem(VIDEO) failed: {}", SDL_GetError());
        throw std::runtime_error("SDL video subsystem init failed");
    }
}

static void phase1_splash() {
    bulkhead("PHASE 1: SPLASH");
    Splash::show("AMOURANTH RTX", 1280, 720, "assets/textures/ammo.png", "assets/audio/ammo.wav");
    LOG_SUCCESS_CAT("MAIN", "Splash complete — PINK PHOTONS AWAKENED");
}

static void phase2_mainWindow() {
    bulkhead("PHASE 2: MAIN WINDOW");
    constexpr int W = 3840, H = 2160;
    Uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (Options::Performance::ENABLE_IMGUI) flags |= SDL_WINDOW_RESIZABLE;

    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", W, H, flags);
    auto* win = SDL3Window::get();
    if (!win) throw std::runtime_error("Window creation failed");

    if (g_base_icon) {
        if (g_hdpi_icon) SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
        SDL_SetWindowIcon(win, g_base_icon);
    }
    LOG_SUCCESS_CAT("MAIN", "Main window created — PHASE 2 COMPLETE");
}

static void phase3_vulkanContext(SDL_Window* window) {
    bulkhead("PHASE 3: VULKAN CONTEXT");
    if (!window) throw std::runtime_error("Null window");

    VkInstance instance = RTX::createVulkanInstanceWithSDL(window, true);
    if (!instance) throw std::runtime_error("Vulkan instance failed");

    RTX::initContext(instance, window, 3840, 2160);
    if (!RTX::g_ctx().isValid()) throw std::runtime_error("Vulkan context invalid");

    set_g_PhysicalDevice(RTX::g_ctx().physicalDevice());
    SDL_ShowWindow(window);
    SDL_Delay(16);
    detectBestPresentMode(RTX::g_ctx().physicalDevice(), RTX::g_ctx().surface());
    LOG_SUCCESS_CAT("MAIN", "Vulkan empire forged — PHASE 3 COMPLETE");
}

static std::unique_ptr<Application> phase4_appAndRendererConstruction() {
    bulkhead("PHASE 4: APP + RENDERER");

    auto app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);
    createGlobalRTX(3840, 2160, nullptr);

    // Renderer now uses the already-initialized swapchain
    auto renderer = std::make_unique<VulkanRenderer>(3840, 2160, SDL3Window::get(), !Options::Window::VSYNC);
    app->setRenderer(std::move(renderer));

    LOG_SUCCESS_CAT("MAIN", "Renderer ascended — RAY TRACING ONLINE — PINK PHOTONS ETERNAL");
    return app;
}

static void phase5_renderLoop(std::unique_ptr<Application>& app) {
    bulkhead("PHASE 5: RENDER LOOP");
    app->run();
}

static void phase6_shutdown(std::unique_ptr<Application>& app) {
    bulkhead("PHASE 6: SHUTDOWN");
    app.reset();
    RTX::shutdown();
    SDL_Quit();
    LOG_SUCCESS_CAT("MAIN", "Clean exit — PINK PHOTONS ETERNAL");
}

// =============================================================================
// MAIN — APOCALYPSE v10.0 — FINAL
// =============================================================================
int main(int argc, char* argv[]) {
    LOG_ATTEMPT_CAT("MAIN", "=== AMOURANTH RTX — VALHALLA v80 TURBO ===");

    std::unique_ptr<Application> app;

    try {
        phase0_cliAndStonekey(argc, argv);
        prePhase1_earlySdlInit();
        phase0_5_iconPreload();
        phase1_splash();
        phase2_mainWindow();
        phase3_vulkanContext(SDL3Window::get());
        app = phase4_appAndRendererConstruction();
        phase5_renderLoop(app);
        phase6_shutdown(app);
    }
    catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        if (app) app.reset();
        RTX::shutdown();
        SDL_Quit();
        return -1;
    }

    LOG_SUCCESS_CAT("MAIN", "=== EXIT — EMPIRE ETERNAL ===");
    return 0;
}