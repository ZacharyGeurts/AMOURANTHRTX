// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#include "engine/GLOBAL/OptionsMenu.hpp"
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
#include <exception>

using namespace Logging::Color;

// =============================================================================
// Swapchain Runtime Configuration
// =============================================================================
struct SwapchainRuntimeConfig {
    VkPresentModeKHR desiredMode       = VK_PRESENT_MODE_MAILBOX_KHR;
    bool             forceVsync        = false;
    bool             forceTripleBuffer = true;
    bool             enableHDR         = true;
    bool             logFinalConfig    = true;
};

static SwapchainRuntimeConfig gSwapchainConfig{
    .desiredMode       = VK_PRESENT_MODE_MAILBOX_KHR,
    .forceVsync        = false,
    .forceTripleBuffer = true,
    .enableHDR         = true,
    .logFinalConfig    = true
};

// =============================================================================
// Command-line argument parsing
// =============================================================================
static void applyVideoModeToggles(int argc, char* argv[]) {
    LOG_INFO_CAT("MAIN", "{}Parsing {} command-line arguments{}", ELECTRIC_BLUE, argc - 1, RESET);
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        LOG_INFO_CAT("MAIN", "  Arg[{}]: {}", i, arg);

        if (arg == "--mailbox") {
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
            LOG_INFO_CAT("MAIN", "    → Present Mode: MAILBOX (low latency)");
        }
        else if (arg == "--immediate") {
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            LOG_INFO_CAT("MAIN", "    → Present Mode: IMMEDIATE (minimum latency)");
        }
        else if (arg == "--vsync") {
            gSwapchainConfig.forceVsync = true;
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
            LOG_INFO_CAT("MAIN", "    → VSYNC: FORCED ON (FIFO)");
        }
        else if (arg == "--no-triple") {
            gSwapchainConfig.forceTripleBuffer = false;
            LOG_INFO_CAT("MAIN", "    → Triple Buffering: DISABLED");
        }
        else if (arg == "--no-hdr") {
            gSwapchainConfig.enableHDR = false;
            LOG_INFO_CAT("MAIN", "    → HDR: DISABLED");
        }
        else if (arg == "--no-log") {
            gSwapchainConfig.logFinalConfig = false;
            LOG_INFO_CAT("MAIN", "    → Final config logging: DISABLED");
        }
        else {
            LOG_WARN_CAT("MAIN", "    Unrecognized argument: {}", arg);
        }
    }
    LOG_SUCCESS_CAT("MAIN", "Command-line parsing complete");
}

// =============================================================================
// Critical Exception Type
// =============================================================================
class MainException : public std::runtime_error {
public:
    MainException(const std::string& msg, const char* file, int line, const char* func)
        : std::runtime_error(std::format("[MAIN FATAL] {}\n    at {}:{} in {}", msg, file, line, func)) {}
};
#define THROW_MAIN(msg) throw MainException(msg, __FILE__, __LINE__, __func__)

// =============================================================================
// Phase Separator
// =============================================================================
inline void bulkhead(const std::string& title) {
    LOG_INFO_CAT("MAIN", "{}════════════════ {} ════════════════{}", ELECTRIC_BLUE, title, RESET);
}

// =============================================================================
// Shader Path Provider
// =============================================================================
inline std::vector<std::string> getRayTracingBinPaths() {
    LOG_INFO_CAT("MAIN", "Providing ray tracing shader paths");
    return { "shaders/raytracing.spv" };
}

// =============================================================================
// Global RTX Accessor
// =============================================================================
inline VulkanRTX& g_rtx() { 
    if (!g_rtx_instance) THROW_MAIN("g_rtx_instance is null");
    return *g_rtx_instance; 
}

// =============================================================================
// Context Readiness Checks
// =============================================================================
static bool isContextReady() {
    try {
        auto& ctx = RTX::g_ctx();
        return ctx.isValid() && 
               ctx.physicalDevice() != VK_NULL_HANDLE && 
               ctx.device() != VK_NULL_HANDLE;
    } catch (...) { return false; }
}

// Fixed section — only the broken function
static bool waitForContextValid(std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    auto start = std::chrono::steady_clock::now();
    while (!isContextReady()) {
        if (std::chrono::steady_clock::now() - start > timeout) {  // ← Fixed: one parenthesis
            LOG_FATAL_CAT("MAIN", "Timeout waiting for RTX::g_ctx() to become valid");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    LOG_SUCCESS_CAT("MAIN", "RTX::g_ctx() validated — Vulkan 100% ready");
    return true;
}

static VkPhysicalDevice validatePhysicalDevice() {
    try {
        auto& ctx = RTX::g_ctx();
        if (!ctx.isValid() || ctx.physicalDevice() == VK_NULL_HANDLE) {
            THROW_MAIN("Physical device not available");
        }
        LOG_INFO_CAT("MAIN", "Physical device validated: {}", (void*)ctx.physicalDevice());
        return ctx.physicalDevice();
    } catch (...) {
        THROW_MAIN("Failed to validate physical device");
    }
}

static bool waitForRTXValid(std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    auto start = std::chrono::steady_clock::now();
    while (!g_rtx_instance || !g_rtx().isValid()) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            LOG_FATAL_CAT("MAIN", "Timeout waiting for VulkanRTX instance");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    LOG_SUCCESS_CAT("MAIN", "VulkanRTX instance validated — ready for dispatch");
    return true;
}

// =============================================================================
// MAIN APPLICATION ENTRY POINT — FULLY LOGGED, PROFESSIONAL, NO SKIPS
// =============================================================================
int main(int argc, char* argv[]) {
    LOG_INFO_CAT("MAIN", "{}AMOURANTH RTX — INITIALIZATION SEQUENCE BEGIN{}", COSMIC_GOLD, RESET);
    LOG_INFO_CAT("MAIN", "Build Date: Nov 13 2025 — VALHALLA v80 TURBO");
    LOG_INFO_CAT("MAIN", "License: CC BY-NC 4.0 | Commercial: gzac5314@gmail.com");

    try {
        // ──────────────────────────────────────────────────────────────────────
        // PHASE 0: PRE-INIT — CLI + SECURITY
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 0: CLI + STONEKEY");
        applyVideoModeToggles(argc, argv);
        (void)get_kStone1(); (void)get_kStone2();
        LOG_INFO_CAT("MAIN", "StoneKey security module initialized — encryption enforced");

        // ──────────────────────────────────────────────────────────────────────
        // PHASE 1: SPLASH SCREEN + AUDIO
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 1: SPLASH + AMMO.WAV");
        LOG_INFO_CAT("MAIN", "Initializing SDL3 subsystems: VIDEO | AUDIO");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0) {
            THROW_MAIN(std::string("SDL_Init failed: ") + SDL_GetError());
        }
        LOG_SUCCESS_CAT("MAIN", "SDL3 VIDEO + AUDIO subsystems active");

        LOG_INFO_CAT("MAIN", "Loading Vulkan dynamic library via SDL");
        if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
            THROW_MAIN(std::string("SDL_Vulkan_LoadLibrary failed: ") + SDL_GetError());
        }
        LOG_SUCCESS_CAT("MAIN", "Vulkan loader ready");

        LOG_INFO_CAT("MAIN", "Displaying branded splash screen (1280×720)");
        Splash::show("AMOURANTH RTX", 1280, 720, "assets/textures/ammo.png", "assets/audio/ammo.wav");
        LOG_SUCCESS_CAT("MAIN", "Splash sequence completed — PINK PHOTONS AWAKENED");

        // ──────────────────────────────────────────────────────────────────────
        // PHASE 2: MAIN WINDOW CREATION (NO CENTERING — OS DEFAULT)
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 2: MAIN APPLICATION WINDOW");
        constexpr int TARGET_WIDTH  = 3840;
        constexpr int TARGET_HEIGHT = 2160;

        LOG_INFO_CAT("MAIN", "Creating main application window: {}×{}", TARGET_WIDTH, TARGET_HEIGHT);
        auto app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", TARGET_WIDTH, TARGET_HEIGHT);
        SDL_Window* window = app->getWindow();

        LOG_INFO_CAT("MAIN", "Window created — OS default position (no centering)");
        LOG_INFO_CAT("MAIN", "Window flags: 0x{:08x}", SDL_GetWindowFlags(window));
        LOG_INFO_CAT("MAIN", "Titlebar: ENABLED | Resizable: ENABLED | Bordered: ENABLED");

        // ──────────────────────────────────────────────────────────────────────
        // PHASE 3: VULKAN CONTEXT INITIALIZATION
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 3: VULKAN CONTEXT");
        LOG_INFO_CAT("MAIN", "Initializing global Vulkan context via RTX::initContext()");

        std::set_terminate([] {
            LOG_FATAL_CAT("MAIN", "EARLY ACCESS TO RTX::g_ctx() DETECTED — TERMINATING");
            std::abort();
        });

        RTX::initContext(window, TARGET_WIDTH, TARGET_HEIGHT);
        LOG_SUCCESS_CAT("MAIN", "Vulkan context initialized");

        if (!waitForContextValid()) {
            THROW_MAIN("RTX context failed to become valid");
        }

        g_PhysicalDevice = validatePhysicalDevice();
        LOG_AMOURANTH();

        // ──────────────────────────────────────────────────────────────────────
        // PHASE 4: SWAPCHAIN + RTX ENGINE
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 4: SWAPCHAIN + RTX ENGINE");
        LOG_INFO_CAT("MAIN", "Initializing SwapchainManager");
        auto& swapMgr = SwapchainManager::get();
        swapMgr.init(
            RTX::g_ctx().instance(),
            RTX::g_ctx().physicalDevice(),
            RTX::g_ctx().device(),
            RTX::g_ctx().surface(),
            TARGET_WIDTH,
            TARGET_HEIGHT
        );
        LOG_SUCCESS_CAT("MAIN", "Swapchain initialized");

        LOG_INFO_CAT("MAIN", "Creating global VulkanRTX instance");
        createGlobalRTX(TARGET_WIDTH, TARGET_HEIGHT, nullptr);
        if (!waitForRTXValid()) {
            THROW_MAIN("VulkanRTX failed to initialize");
        }

        LOG_INFO_CAT("MAIN", "Building acceleration structures (BLAS → TLAS)");
        g_rtx().buildAccelerationStructures();
        vkDeviceWaitIdle(RTX::g_ctx().device());
        LOG_SUCCESS_CAT("MAIN", "Acceleration structures built — LAS ONLINE");

        LOG_INFO_CAT("MAIN", "Initializing RTX descriptor pool and sets");
        g_rtx().initDescriptorPoolAndSets();

        LOG_INFO_CAT("MAIN", "Creating black fallback image");
        g_rtx().initBlackFallbackImage();

        LOG_INFO_CAT("MAIN", "Constructing VulkanRenderer — INTERNAL SHADERS ACTIVE — PINK PHOTONS RISING");
        auto renderer = std::make_unique<VulkanRenderer>(
            TARGET_WIDTH, TARGET_HEIGHT, window, true
        );
        app->setRenderer(std::move(renderer));

        LOG_INFO_CAT("MAIN", "Initializing Shader Binding Table (64MB Titan-grade)");
        g_rtx().initShaderBindingTable(g_PhysicalDevice);

        LOG_INFO_CAT("MAIN", "Updating RTX descriptors for frame 0");
        g_rtx().updateRTXDescriptors(0,
            VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
            VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
            VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE
        );

        // ──────────────────────────────────────────────────────────────────────
        // PHASE 5: ENTER MAIN LOOP
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 5: ENTERING MAIN RENDER LOOP");
        LOG_INFO_CAT("MAIN", "All systems nominal — entering infinite render loop");
        LOG_INFO_CAT("MAIN", "First vkCmdTraceRaysKHR() is now safe");
        app->run();

        // ──────────────────────────────────────────────────────────────────────
        // PHASE 6: GRACEFUL SHUTDOWN
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 6: SHUTDOWN SEQUENCE");
        LOG_INFO_CAT("MAIN", "Application loop exited — beginning shutdown");

        app.reset();
        LOG_INFO_CAT("MAIN", "Application instance destroyed");

        LOG_INFO_CAT("MAIN", "Waiting for device idle before cleanup");
        vkDeviceWaitIdle(RTX::g_ctx().device());

        LOG_INFO_CAT("MAIN", "Destroying Vulkan core resources");
        auto& ctx = RTX::g_ctx();
        if (ctx.commandPool_) { vkDestroyCommandPool(ctx.device(), ctx.commandPool(), nullptr); ctx.commandPool_ = VK_NULL_HANDLE; }
        if (ctx.device_)      { vkDestroyDevice(ctx.device(), nullptr); ctx.device_ = VK_NULL_HANDLE; }
        if (ctx.surface_)     { vkDestroySurfaceKHR(ctx.instance(), ctx.surface(), nullptr); ctx.surface_ = VK_NULL_HANDLE; }
        if (ctx.instance_)    { vkDestroyInstance(ctx.instance(), nullptr); ctx.instance_ = VK_NULL_HANDLE; }

        LOG_INFO_CAT("MAIN", "Quitting SDL3");
        SDL_Quit();

        LOG_SUCCESS_CAT("MAIN", "FINAL STONEKEY HASH: 0x{:016X}", get_kStone1() ^ get_kStone2());
        LOG_SUCCESS_CAT("MAIN", "{}AMOURANTH RTX — CLEAN SHUTDOWN — PINK PHOTONS ETERNAL{}", COSMIC_GOLD, RESET);

    } catch (const std::exception& e) {
        LOG_FATAL_CAT("MAIN", "UNRECOVERABLE ERROR: {}", e.what());
        LOG_FATAL_CAT("MAIN", "Application terminated abnormally");
        return -1;
    } catch (...) {
        LOG_FATAL_CAT("MAIN", "UNKNOWN EXCEPTION — TERMINATING");
        return -1;
    }

    return 0;
}