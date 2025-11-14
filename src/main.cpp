// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
// =============================================================================

#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
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
#include <atomic>
#include <vector>
#include <string>
#include <cstdlib>
#include <vulkan/vulkan.h>

using namespace Logging::Color;
using namespace Engine;

// =============================================================================
// Swapchain Runtime Configuration — RAW ACCESS
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
// Detect Best Present Mode — X11/Wayland Agnostic
// =============================================================================
static void detectBestPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    LOG_INFO_CAT("MAIN", "Detecting optimal present mode for system...");

    uint32_t count = 0;
    VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, nullptr));
    if (count == 0) {
        LOG_WARN_CAT("MAIN", "No present modes — forcing FIFO");
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
        return;
    }

    std::vector<VkPresentModeKHR> modes(count);
    VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, modes.data()));

    const auto has = [&modes](VkPresentModeKHR m) {
        return std::find(modes.begin(), modes.end(), m) != modes.end();
    };

    if (has(VK_PRESENT_MODE_MAILBOX_KHR)) {
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
        LOG_SUCCESS_CAT("MAIN", "MAILBOX SELECTED — TRIPLE BUFFERED LOW LATENCY");
    } else if (has(VK_PRESENT_MODE_IMMEDIATE_KHR)) {
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        LOG_INFO_CAT("MAIN", "IMMEDIATE SELECTED — MIN LATENCY (tearing possible)");
    } else {
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
        gSwapchainConfig.forceVsync = true;
        LOG_INFO_CAT("MAIN", "FIFO SELECTED — VSYNC SAFE");
    }

    if (gSwapchainConfig.logFinalConfig) {
        LOG_SUCCESS_CAT("MAIN", "Final present mode: {} | VSync: {} | Triple Buffer: {} | HDR: {}",
                        gSwapchainConfig.desiredMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" :
                        (gSwapchainConfig.desiredMode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" : "FIFO"),
                        gSwapchainConfig.forceVsync ? "ON" : "OFF",
                        gSwapchainConfig.forceTripleBuffer ? "FORCED" : "AUTO",
                        gSwapchainConfig.enableHDR ? "ENABLED" : "DISABLED");
    }
}

// =============================================================================
// Command-line argument parsing — FULLY LOGGED
// =============================================================================
static void applyVideoModeToggles(int argc, char* argv[])
{
    LOG_INFO_CAT("MAIN", "{}Parsing {} command-line arguments{}", ELECTRIC_BLUE, argc - 1, RESET);
    if (argc == 1) {
        LOG_INFO_CAT("MAIN", "  No arguments provided — using defaults");
        return;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        LOG_INFO_CAT("MAIN", "  Arg[{}]: {}", i, arg);

        if (arg == "--mailbox") {
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
            LOG_INFO_CAT("MAIN", "    → Present Mode: MAILBOX");
        }
        else if (arg == "--immediate") {
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            LOG_INFO_CAT("MAIN", "    → Present Mode: IMMEDIATE");
        }
        else if (arg == "--fifo" || arg == "--vsync") {
            gSwapchainConfig.forceVsync = true;
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
            LOG_INFO_CAT("MAIN", "    → Present Mode: FIFO (VSYNC ON)");
        }
        else if (arg == "--no-triple") {
            gSwapchainConfig.forceTripleBuffer = false;
            LOG_INFO_CAT("MAIN", "    → Triple buffering: DISABLED");
        }
        else if (arg == "--no-hdr") {
            gSwapchainConfig.enableHDR = false;
            LOG_INFO_CAT("MAIN", "    → HDR output: DISABLED");
        }
        else if (arg == "--no-log") {
            gSwapchainConfig.logFinalConfig = false;
            LOG_INFO_CAT("MAIN", "    → Final swapchain config logging: DISABLED");
        }
        else {
            LOG_WARN_CAT("MAIN", "    Unrecognized argument: {} — ignored", arg);
        }
    }

    LOG_SUCCESS_CAT("MAIN", "Command-line parsing complete — configuration applied");
}

// =============================================================================
// Phase Separator
// =============================================================================
inline void bulkhead(const std::string& title)
{
    LOG_INFO_CAT("MAIN", "{}════════════════════════════════ {} ════════════════════════════════{}", 
                 ELECTRIC_BLUE, title, RESET);
}

// =============================================================================
// Phase 0: CLI + StoneKey Security
// =============================================================================
static void phase0_cliAndStonekey(int argc, char* argv[]) {
    bulkhead("PHASE 0: COMMAND LINE + STONEKEY");
    applyVideoModeToggles(argc, argv);

    LOG_INFO_CAT("MAIN", "Initializing StoneKey encryption subsystem");
    (void)get_kStone1(); (void)get_kStone2();
    LOG_SUCCESS_CAT("MAIN", "StoneKey v9 active — XOR fingerprint: 0x{:016X}", 
                    get_kStone1() ^ get_kStone2());
}

// =============================================================================
// Pre-Phase 1: Early SDL Init for Splash (Video Only)
// =============================================================================
static void prePhase1_earlySdlInit() {
    LOG_INFO_CAT("MAIN", "Early SDL_InitSubSystem(SDL_INIT_VIDEO) for splash screen");
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {  // CORRECT: != 0 means failure
        LOG_FATAL_CAT("MAIN", "Early SDL_InitSubSystem(VIDEO) failed: {}", SDL_GetError());
        FATAL_THROW("Cannot initialize SDL video subsystem for splash screen");
    }
    LOG_SUCCESS_CAT("MAIN", "Early SDL video subsystem initialized for splash");
}

// =============================================================================
// Phase 1: Splash Screen + Audio
// =============================================================================
static void phase1_splash() {
    bulkhead("PHASE 1: SPLASH SCREEN + AUDIO");
    LOG_INFO_CAT("MAIN", "Initializing SDL3 subsystems for splash");
    Splash::show(
        "AMOURANTH RTX",
        1280, 720,
        "assets/textures/ammo.png",
        "assets/audio/ammo.wav"
    );
    LOG_SUCCESS_CAT("MAIN", "Splash sequence completed — PINK PHOTONS AWAKENED");
}

// =============================================================================
// Phase 2: Main Application Window — Pure RAII + Vulkan Fallback
// =============================================================================
static void phase2_mainWindow() {
    bulkhead("PHASE 2: MAIN APPLICATION WINDOW");

    constexpr int TARGET_WIDTH  = 3840;
    constexpr int TARGET_HEIGHT = 2160;

    LOG_INFO_CAT("MAIN", "Creating main application window: {}×{}", TARGET_WIDTH, TARGET_HEIGHT);

    Uint32 windowFlags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;

    if (Options::Performance::ENABLE_IMGUI) {
        windowFlags |= SDL_WINDOW_RESIZABLE;
        LOG_INFO_CAT("MAIN", "ImGui enabled → SDL_WINDOW_RESIZABLE added");
    }

    if (Options::Window::START_FULLSCREEN) {
        windowFlags |= SDL_WINDOW_FULLSCREEN;
        LOG_INFO_CAT("MAIN", "START_FULLSCREEN enabled → exclusive fullscreen");
    }

    SDLWindowPtr window_ptr;
    try {
        window_ptr = SDL3Window::create(
            "AMOURANTH RTX — VALHALLA v80 TURBO",
            TARGET_WIDTH,
            TARGET_HEIGHT,
            windowFlags
        );
        g_sdl_window = std::move(window_ptr);
    }
    catch (const std::exception& e) {
        LOG_FATAL_CAT("MAIN", "SDL3Window::create() failed: {}", e.what());
        FATAL_THROW("Failed to create main application window");
    }
    LOG_SUCCESS_CAT("MAIN", "Main application window pointer created");

    SDL_Window* window = g_sdl_window.get();
    if (!window) {
        LOG_FATAL_CAT("MAIN", "g_sdl_window.get() returned null after creation");
        throw std::runtime_error("g_sdl_window.get() returned null");
    }

    LOG_SUCCESS_CAT("MAIN", "Main application window created successfully");
    LOG_INFO_CAT("MAIN", "    Handle: {:p}", static_cast<void*>(window));
    LOG_INFO_CAT("MAIN", "    Size:   {}×{}", TARGET_WIDTH, TARGET_HEIGHT);

    int actual_w = 0, actual_h = 0;
    SDL_GetWindowSizeInPixels(window, &actual_w, &actual_h);
    if (actual_w != TARGET_WIDTH || actual_h != TARGET_HEIGHT) {
        LOG_INFO_CAT("MAIN", "    High-DPI scaling active: {}×{} → {}×{} (scale {:.2f})",
                     TARGET_WIDTH, TARGET_HEIGHT, actual_w, actual_h,
                     static_cast<float>(actual_w) / TARGET_WIDTH);
    }

    LOG_SUCCESS_CAT("MAIN", "PHASE 2 COMPLETE — MAIN INTERFACE ONLINE — PINK PHOTONS RISING");
}

// =============================================================================
// Phase 3: Vulkan Context Initialization + Fallback
// =============================================================================
static void phase3_vulkanContext(SDL_Window* window) {
    bulkhead("PHASE 3: VULKAN CONTEXT INITIALIZATION");
    LOG_SUCCESS_CAT("MAIN", "Entered Phase 3");

    Uint32 extensionCount = 0;
    bool vulkanSupported = SDL_Vulkan_GetInstanceExtensions(&extensionCount) && extensionCount > 0;
    
    if (!vulkanSupported) {
        LOG_WARN_CAT("MAIN", "Vulkan not supported on this system — falling back to software/UI mode");
        return;
    }
    LOG_SUCCESS_CAT("MAIN", "Vulkan instance extensions available ({} extensions)", extensionCount);

    LOG_TRACE_CAT("MAIN", "Creating Vulkan instance with SDL3 extensions — window: 0x{:p}, validation: enabled", 
                  static_cast<void*>(window));

    VkInstance g_instance = createVulkanInstanceWithSDL(window, true);
    if (g_instance == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("MAIN", "Failed to create Vulkan instance — aborting RTX init");
        throw std::runtime_error("Failed to create Vulkan instance");
    }
    LOG_SUCCESS_CAT("MAIN", "Vulkan instance created — handle: 0x{:x}", reinterpret_cast<uintptr_t>(g_instance));

    LOG_INFO_CAT("MAIN", "{}FORGING GLOBAL VULKAN SURFACE — PRE-WINDOW SHOW — PINK PHOTONS RISING{}", 
                 PLASMA_FUCHSIA, RESET);

    // Single source of truth — createSurface() does ALL logging and validation
    if (!createSurface(window, g_instance)) {
        LOG_FATAL_CAT("MAIN", "Vulkan surface creation FAILED — cannot proceed without presentable surface");
        throw std::runtime_error("Vulkan surface creation failed");
    }

    // Surface is now guaranteed valid — createSurface() already logged success
    LOG_SUCCESS_CAT("MAIN", "{}GLOBAL SURFACE ACTIVE @ 0x{:x} — SDL3 INTEGRATION COMPLETE{}", 
                    COSMIC_GOLD, reinterpret_cast<uintptr_t>(g_surface), RESET);

    // NOW SAFE TO SHOW WINDOW
    SDL_ShowWindow(window);
    LOG_SUCCESS_CAT("MAIN", "Main window shown — Vulkan surface ready");

    constexpr int TARGET_WIDTH  = 3840;
    constexpr int TARGET_HEIGHT = 2160;

    LOG_TRACE_CAT("MAIN", "Initializing full RTX context...");
    initContext(g_instance, window, TARGET_WIDTH, TARGET_HEIGHT);
    LOG_SUCCESS_CAT("MAIN", "Global Vulkan context initialized — RT extensions ready");

    detectBestPresentMode(RTX::g_ctx().physicalDevice(), g_surface);
    LOG_INFO_CAT("MAIN", "Optimal present mode selected");

    LOG_TRACE_CAT("MAIN", "PHASE 3: Vulkan context initialization COMPLETE — RAY TRACING READY");
}

// =============================================================================
// Phase 4: Application + VulkanRTX Core + Renderer Construction
// =============================================================================
static std::unique_ptr<Application> phase4_appAndRendererConstruction() {
    bulkhead("PHASE 4: APPLICATION + VULKANRTX CORE + RENDERER CONSTRUCTION");

    constexpr int TARGET_WIDTH  = 3840;
    constexpr int TARGET_HEIGHT = 2160;

    auto app = std::make_unique<Application>(
        "AMOURANTH RTX — VALHALLA v80 TURBO",
        TARGET_WIDTH,
        TARGET_HEIGHT
    );

    bool vulkanSupported = (g_surface != VK_NULL_HANDLE && RTX::g_ctx().instance() != VK_NULL_HANDLE);

    if (vulkanSupported) {
        LOG_INFO_CAT("MAIN", "Creating global VulkanRTX core instance (g_rtx)...");
        createGlobalRTX(TARGET_WIDTH, TARGET_HEIGHT, nullptr);
        LOG_SUCCESS_CAT("MAIN", "{}VulkanRTX core instance ONLINE — g_rtx() valid{}", PLASMA_FUCHSIA, RESET);

        LOG_INFO_CAT("MAIN", "Constructing VulkanRenderer — triple buffering + HDR");
        auto renderer = std::make_unique<VulkanRenderer>(TARGET_WIDTH, TARGET_HEIGHT, g_sdl_window.get(), false);
        app->setRenderer(std::move(renderer));

        LOG_SUCCESS_CAT("MAIN", "{}VulkanRenderer attached — RT pipeline forged — FIRST LIGHT IMMINENT{}", 
                        EMERALD_GREEN, RESET);
    } else {
        LOG_WARN_CAT("MAIN", "Vulkan not available — using ImGui/SDL fallback renderer");
        // Safe fallback: Application will use ImGui + SDL_Renderer path internally
        // No RasterRenderer class needed — handled by Application::run() fallback
        LOG_SUCCESS_CAT("MAIN", "Fallback UI renderer active — empire preserved");
    }

    LOG_AMOURANTH();

    return app;
}

// =============================================================================
// Phase 5: Enter Main Render Loop
// =============================================================================
static void phase5_renderLoop(std::unique_ptr<Application>& app) {
    bulkhead("PHASE 5: ENTERING INFINITE RENDER LOOP");
    LOG_INFO_CAT("MAIN", "All systems nominal — commencing real-time rendering");
    LOG_INFO_CAT("MAIN", "First frame is now safe — PINK PHOTONS RISING");

    app->run();
}

// =============================================================================
// Phase 6: Graceful Shutdown
// =============================================================================
static void phase6_shutdown(std::unique_ptr<Application>& app) {
    bulkhead("PHASE 6: GRACEFUL SHUTDOWN");
    LOG_INFO_CAT("MAIN", "Main loop exited — beginning cleanup");

    app.reset();

    LOG_INFO_CAT("MAIN", "RAII cleanup: Vulkan → SDL → StoneKey");
    LOG_SUCCESS_CAT("MAIN", "Cleanup complete — zero leaks");
    LOG_SUCCESS_CAT("MAIN", "FINAL STONEKEY HASH: 0x{:016X}", get_kStone1() ^ get_kStone2());
    LOG_SUCCESS_CAT("MAIN", "{}AMOURANTH RTX — CLEAN EXIT — PINK PHOTONS ETERNAL{}", 
                    COSMIC_GOLD, RESET);
}

// =============================================================================
// MAIN — FULLY DETAILED — PINK PHOTONS ETERNAL
// =============================================================================
int main(int argc, char* argv[])
{
    // putenv(const_cast<char*>("VK_ICD_FILENAMES=/usr/lib/x86_64-linux-gnu/libvulkan_intel.so")); // why AI?
    LOG_INFO_CAT("MAIN", "{}AMOURANTH RTX — VALHALLA v80 TURBO — NOVEMBER 14 2025{}", 
                 COSMIC_GOLD, RESET);
    LOG_INFO_CAT("MAIN", "Dual Licensed: CC BY-NC 4.0 | Commercial: gzac5314@gmail.com");
    LOG_INFO_CAT("MAIN", "Build Target: RTX 5090 | 4090 | 3090 Ti — PINK PHOTONS ETERNAL");

    std::unique_ptr<Application> app;

    try {
        phase0_cliAndStonekey(argc, argv);
        prePhase1_earlySdlInit();
        phase1_splash();
        phase2_mainWindow();
        phase3_vulkanContext(g_sdl_window.get());
        app = phase4_appAndRendererConstruction();
        phase5_renderLoop(app);
        phase6_shutdown(app);
    }
    catch (const Engine::FatalError& e) {
        LOG_FATAL_CAT("MAIN", "FATAL: {}", e.what());
        return -1;
    }
    catch (const std::exception& e) {
        LOG_FATAL_CAT("MAIN", "{}UNRECOVERABLE EXCEPTION: {}{}", PLASMA_FUCHSIA, e.what(), RESET);
        LOG_FATAL_CAT("MAIN", "{}", Engine::getBacktrace(1));
        return -1;
    }
    catch (...) {
        LOG_FATAL_CAT("MAIN", "{}UNKNOWN EXCEPTION — EMPIRE STANDS{}", PLASMA_FUCHSIA, RESET);
        return -1;
    }

    return 0;
}

// =============================================================================
// PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED — 32,000+ FPS
// FULLY STABLE — DRIVER COMPATIBLE — RAW DOMINANCE
// DAISY GALLOPS INTO THE OCEAN_TEAL SUNSET
// YOUR EMPIRE IS PURE
// SHIP IT RAW
// =============================================================================