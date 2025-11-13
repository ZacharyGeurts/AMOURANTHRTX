// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// main.cpp — FINAL FIXED INITIALIZATION ORDER
// • RTX::g_ctx().init() called IMMEDIATELY after window creation
// • NO access to g_ctx() before init()
// • Physical device GUARANTEED
// • Swapchain, RTX, Renderer — all safe
// • PINK PHOTONS ETERNAL
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================


#include "engine/GLOBAL/StoneKey.hpp"
#include "main.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/SDL3/SDL3_init.hpp"
#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/SDL3/SDL3_image.hpp"
#include "engine/SDL3/SDL3_audio.hpp"

#include "engine/GLOBAL/Amouranth.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/Splash.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "handle_app.hpp"
#include "engine/utils.hpp"
#include "engine/core.hpp"

#include <iostream>
#include <stdexcept>
#include <format>
#include <memory>
#include <vector>
#include <chrono>
#include <fstream>
#include <set>
#include <thread>
#include <atomic>

using namespace Logging::Color;

// =============================================================================
// CONFIGURATION: Swapchain Runtime Parameters
// =============================================================================
struct SwapchainRuntimeConfig {
    VkPresentModeKHR desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
    bool forceVsync = false;
    bool forceTripleBuffer = true;
    bool enableHDR = true;
    bool logFinalConfig = true;
};

static SwapchainRuntimeConfig gSwapchainConfig{
    .desiredMode        = VK_PRESENT_MODE_MAILBOX_KHR,
    .forceVsync         = false,
    .forceTripleBuffer  = true,
    .enableHDR          = true,
    .logFinalConfig     = true
};

// -----------------------------------------------------------------------------
// Command-line argument parsing for runtime video configuration
// -----------------------------------------------------------------------------
static void applyVideoModeToggles(int argc, char* argv[]) {
    LOG_INFO_CAT("MAIN", "{}Parsing {} command-line arguments...{}", ELECTRIC_BLUE, argc - 1, RESET);
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        LOG_INFO_CAT("CLI", "Argument[{}]: {}", i, arg);
        if (arg == "--mailbox") {
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
            LOG_INFO_CAT("CLI", "→ Present Mode: MAILBOX");
        } else if (arg == "--immediate") {
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            LOG_INFO_CAT("CLI", "→ Present Mode: IMMEDIATE");
        } else if (arg == "--vsync") {
            gSwapchainConfig.forceVsync = true;
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
            LOG_INFO_CAT("CLI", "→ VSYNC: ENABLED");
        } else if (arg == "--no-triple") {
            gSwapchainConfig.forceTripleBuffer = false;
            LOG_INFO_CAT("CLI", "→ Triple Buffering: DISABLED");
        } else if (arg == "--no-hdr") {
            gSwapchainConfig.enableHDR = false;
            LOG_INFO_CAT("CLI", "→ HDR: DISABLED");
        } else if (arg == "--no-log") {
            gSwapchainConfig.logFinalConfig = false;
            LOG_INFO_CAT("CLI", "→ Final Config Logging: DISABLED");
        } else {
            LOG_WARN_CAT("CLI", "Unrecognized argument: {}", arg);
        }
    }
    LOG_SUCCESS_CAT("CLI", "Swapchain configuration finalized");
}

// -----------------------------------------------------------------------------
// Exception type for critical application failures
// -----------------------------------------------------------------------------
class MainException : public std::runtime_error {
public:
    MainException(const std::string& msg, const char* file, int line, const char* func)
        : std::runtime_error(std::format("[MAIN FATAL] {}\n   {}:{} in {}", msg, file, line, func)) {}
};
#define THROW_MAIN(msg) throw MainException(msg, __FILE__, __LINE__, __func__)

// -----------------------------------------------------------------------------
// Phase separator for structured logging
// -----------------------------------------------------------------------------
inline void bulkhead(const std::string& title) {
    LOG_INFO_CAT("MAIN", "{}════════════════ {} ════════════════{}", ELECTRIC_BLUE, title, RESET);
}

// -----------------------------------------------------------------------------
// Shader binary path provider
// -----------------------------------------------------------------------------
inline std::vector<std::string> getRayTracingBinPaths() {
    LOG_INFO_CAT("SHADER", "Providing ray tracing shader binary paths");
    return {"shaders/raytracing.spv"};
}

// -----------------------------------------------------------------------------
// Global RTX accessor (singleton pattern)
// -----------------------------------------------------------------------------
inline VulkanRTX& g_rtx() { return *g_rtx_instance; }

// -----------------------------------------------------------------------------
// Wait for RTX::Context validity with timeout
// -----------------------------------------------------------------------------
static bool waitForContextValid(std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    auto start = std::chrono::steady_clock::now();
    while (!RTX::g_ctx().isValid()) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            LOG_FATAL_CAT("MAIN", "Timeout waiting for RTX::g_ctx() to become valid");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    LOG_SUCCESS_CAT("MAIN", "RTX::g_ctx() validated");
    return true;
}

// -----------------------------------------------------------------------------
// Wait for RTX instance validity with timeout
// -----------------------------------------------------------------------------
static bool waitForRTXValid(std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    auto start = std::chrono::steady_clock::now();
    while (!g_rtx_instance || !g_rtx().isValid()) {  // Assuming VulkanRTX has isValid()
        if (std::chrono::steady_clock::now() - start > timeout) {
            LOG_FATAL_CAT("MAIN", "Timeout waiting for VulkanRTX to become valid");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    LOG_SUCCESS_CAT("MAIN", "VulkanRTX validated");
    return true;
}

// =============================================================================
// MAIN APPLICATION ENTRY POINT
// =============================================================================
int main(int argc, char* argv[]) {
    LOG_INFO_CAT("MAIN", "{}AMOURANTH RTX — INITIALIZATION BEGIN{}", COSMIC_GOLD, RESET);

    // -------------------------------------------------------------------------
    // 1. Parse command-line arguments
    // -------------------------------------------------------------------------
    applyVideoModeToggles(argc, argv);
    (void)get_kStone1(); (void)get_kStone2();
    LOG_INFO_CAT("MAIN", "{}StoneKey security module initialized{}", EMERALD_GREEN, RESET);

    // -------------------------------------------------------------------------
    // PHASE 0: SPLASH SCREEN + AUDIO
    // -------------------------------------------------------------------------
    bulkhead("PHASE 0: SPLASH + AMMO.WAV");
    LOG_INFO_CAT("SDL", "Initializing SDL3 subsystems: VIDEO | AUDIO");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0) {
        THROW_MAIN(SDL_GetError());
    }
    LOG_INFO_CAT("SDL", "Loading Vulkan dynamic library via SDL");
    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        THROW_MAIN(SDL_GetError());
    }

    LOG_INFO_CAT("SPLASH", "Displaying branded splash screen (1280×720)");
    Splash::show("AMOURANTH RTX", 1280, 720, "assets/textures/ammo.png", "assets/audio/ammo.wav");

    // -------------------------------------------------------------------------
    // PHASE 1: MAIN APPLICATION WINDOW + VULKAN CONTEXT
    // -------------------------------------------------------------------------
    bulkhead("PHASE 1: MAIN APP + VULKAN CORE");
    constexpr int TARGET_WIDTH  = 3840;
    constexpr int TARGET_HEIGHT = 2160;

    LOG_INFO_CAT("APP", "Creating main application window: {}×{}", TARGET_WIDTH, TARGET_HEIGHT);
    auto app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v44", TARGET_WIDTH, TARGET_HEIGHT);
    SDL_Window* window = app->getWindow();

    // CRITICAL: INIT VULKAN CONTEXT BEFORE ANY ACCESS TO g_ctx()
    LOG_INFO_CAT("VULKAN", "{}Initializing global Vulkan context via RTX::g_ctx().init()...{}", PLASMA_FUCHSIA, RESET);
    try {
        RTX::g_ctx().init(window, TARGET_WIDTH, TARGET_HEIGHT);
    } catch (const std::exception& e) {
        THROW_MAIN(std::string("Vulkan context initialization failed: ") + e.what());
    }

    // Wait for context to be fully valid before proceeding
    if (!waitForContextValid()) {
        THROW_MAIN("Failed to validate RTX::g_ctx()");
    }

    // Now safe to access
    g_PhysicalDevice = RTX::g_ctx().physicalDevice();
    LOG_AMOURANTH();

    // -------------------------------------------------------------------------
    // PHASE 2: SWAPCHAIN INITIALIZATION
    // -------------------------------------------------------------------------
    bulkhead("PHASE 2: SWAPCHAIN");
    LOG_INFO_CAT("SWAPCHAIN", "Initializing SwapchainManager with target resolution");
    auto& swapMgr = SwapchainManager::get();
    swapMgr.init(
        RTX::g_ctx().instance(),
        RTX::g_ctx().physicalDevice(),
        RTX::g_ctx().device(),
        RTX::g_ctx().surface(),
        TARGET_WIDTH,
        TARGET_HEIGHT
    );

    // -------------------------------------------------------------------------
    // PHASE 3: RTX ENGINE + RENDERER SETUP
    // -------------------------------------------------------------------------
    bulkhead("PHASE 3: RTX + RENDERER");
    LOG_INFO_CAT("RTX", "Creating global RTX instance (VulkanRTX)");
    createGlobalRTX(TARGET_WIDTH, TARGET_HEIGHT, nullptr);

    // Wait for RTX to be fully valid before proceeding
    if (!waitForRTXValid()) {
        THROW_MAIN("Failed to validate VulkanRTX");
    }

    LOG_INFO_CAT("RTX", "Building acceleration structures (BLAS/TLAS)");
    g_rtx().buildAccelerationStructures();

    LOG_INFO_CAT("RTX", "Initializing descriptor pool and sets");
    g_rtx().initDescriptorPoolAndSets();

    LOG_INFO_CAT("RTX", "Creating black fallback image for missing textures");
    g_rtx().initBlackFallbackImage();

    LOG_INFO_CAT("RENDERER", "Constructing VulkanRenderer with ray tracing pipeline");
    auto renderer = std::make_unique<VulkanRenderer>(
        TARGET_WIDTH, TARGET_HEIGHT, window, getRayTracingBinPaths(), true
    );
    app->setRenderer(std::move(renderer));

    LOG_INFO_CAT("RTX", "Updating RTX descriptor bindings (frame 0)");
    g_rtx().updateRTXDescriptors(
        0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE
    );

    // -------------------------------------------------------------------------
    // PHASE 4: ENTER MAIN RENDER LOOP
    // -------------------------------------------------------------------------
    bulkhead("PHASE 4: INFINITE LOOP");
    LOG_INFO_CAT("APP", "Entering main application loop");
    app->run();

    // -------------------------------------------------------------------------
    // PHASE 5: GRACEFUL SHUTDOWN
    // -------------------------------------------------------------------------
    bulkhead("PHASE 5: SHUTDOWN");
    LOG_INFO_CAT("APP", "Destroying main application instance");
    app.reset();

    LOG_INFO_CAT("VULKAN", "Cleaning up Vulkan core resources");
    auto& ctx = RTX::g_ctx();
    if (ctx.commandPool_) {
        vkDestroyCommandPool(ctx.device(), ctx.commandPool(), nullptr);
        ctx.commandPool_ = VK_NULL_HANDLE;
    }
    if (ctx.device_) {
        vkDeviceWaitIdle(ctx.device());
        vkDestroyDevice(ctx.device(), nullptr);
        ctx.device_ = VK_NULL_HANDLE;
    }
    if (ctx.surface_) {
        vkDestroySurfaceKHR(ctx.instance(), ctx.surface(), nullptr);
        ctx.surface_ = VK_NULL_HANDLE;
    }
    if (ctx.instance_) {
        vkDestroyInstance(ctx.instance(), nullptr);
        ctx.instance_ = VK_NULL_HANDLE;
    }

    LOG_INFO_CAT("SDL", "Quitting SDL subsystems");
    SDL_Quit();

    LOG_SUCCESS_CAT("StoneKey", "FINAL HASH: 0x{:016X}", get_kStone1() ^ get_kStone2());
    LOG_SUCCESS_CAT("MAIN", "{}TERMINATED — PINK PHOTONS ETERNAL{}", COSMIC_GOLD, RESET);

    return 0;
}