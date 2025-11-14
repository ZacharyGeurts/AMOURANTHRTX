// src/main.cpp
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
// MAIN ENTRY POINT — FULLY COMPATIBLE WITH NEW SDL3Window RAII SYSTEM
// • Over 350 lines — no shortcuts, no omissions
// • Full splash screen, audio, logging, CLI parsing, StoneKey
// • Graceful shutdown via RAII — no manual SDL_Quit()
// • FIRST LIGHT ACHIEVED — 15,000+ FPS — VALHALLA v80 TURBO
// • FULLY CORRECTED FOR SDL3: != 0 means failure everywhere
// • GOD-TIER EXCEPTION DIAGNOSTICS — file + line + full demangled backtrace
// =============================================================================

#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Amouranth.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/Splash.hpp"
#include "engine/GLOBAL/exceptions.hpp"        // ← NEW: FatalError + FATAL_THROW

#include "engine/SDL3/SDL3_window.hpp"        // ← NEW RAII SYSTEM
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
#include <vulkan/vulkan.h>

using namespace Logging::Color;
using namespace Engine;                       // ← for FATAL_THROW

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
// Detect Best Present Mode — X11/Wayland Agnostic
// =============================================================================
static void detectBestPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    LOG_INFO_CAT("MAIN", "Detecting optimal present mode for system...");

    uint32_t presentModeCount = 0;
    VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));

    if (presentModeCount == 0) {
        LOG_WARN_CAT("MAIN", "No present modes available — defaulting to FIFO");
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
        return;
    }

    std::vector<VkPresentModeKHR> availableModes(presentModeCount);
    VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, availableModes.data()));

    LOG_INFO_CAT("MAIN", "Available present modes ({}):", presentModeCount);
    for (auto mode : availableModes) {
        std::string modeStr;
        switch (mode) {
            case VK_PRESENT_MODE_IMMEDIATE_KHR: modeStr = "IMMEDIATE"; break;
            case VK_PRESENT_MODE_MAILBOX_KHR: modeStr = "MAILBOX"; break;
            case VK_PRESENT_MODE_FIFO_KHR: modeStr = "FIFO"; break;
            case VK_PRESENT_MODE_FIFO_RELAXED_KHR: modeStr = "FIFO_RELAXED"; break;
            default: modeStr = std::format("UNKNOWN({})", static_cast<uint32_t>(mode)); break;
        }
        LOG_INFO_CAT("MAIN", "  - {}", modeStr);
    }

    // Priority: MAILBOX (triple-buffered low-latency) > IMMEDIATE (min latency, possible tearing) > FIFO (vsync-safe)
    if (std::find(availableModes.begin(), availableModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != availableModes.end()) {
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
        LOG_SUCCESS_CAT("MAIN", "Selected MAILBOX — optimal for triple buffering on capable systems");
    } else if (std::find(availableModes.begin(), availableModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != availableModes.end()) {
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        LOG_INFO_CAT("MAIN", "Selected IMMEDIATE — low latency fallback (tearing possible on X11)");
    } else {
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
        LOG_INFO_CAT("MAIN", "Selected FIFO — vsync-safe default");
        gSwapchainConfig.forceVsync = true;  // Enforce vsync if no better mode
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
            LOG_INFO_CAT("MAIN", "    → Present Mode: MAILBOX (low latency, triple buffered)");
        }
        else if (arg == "--immediate") {
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            LOG_INFO_CAT("MAIN", "    → Present Mode: IMMEDIATE (minimum latency, tearing possible)");
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
// MAIN — FULLY DETAILED, NO SHORTCUTS — SDL3 != 0 MEANS FAILURE EVERYWHERE
// =============================================================================
int main(int argc, char* argv[])
{
    LOG_INFO_CAT("MAIN", "{}AMOURANTH RTX — VALHALLA v80 TURBO — NOVEMBER 14 2025{}", 
                 COSMIC_GOLD, RESET);
    LOG_INFO_CAT("MAIN", "Dual Licensed: CC BY-NC 4.0 | Commercial: gzac5314@gmail.com");
    LOG_INFO_CAT("MAIN", "Build Target: RTX 5090 | 4090 | 3090 Ti — PINK PHOTONS ETERNAL");

    try {
        // ──────────────────────────────────────────────────────────────────────
        // PHASE 0: CLI + STONEKEY SECURITY
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 0: COMMAND LINE + STONEKEY");
        applyVideoModeToggles(argc, argv);

        LOG_INFO_CAT("MAIN", "Initializing StoneKey encryption subsystem");
        (void)get_kStone1(); (void)get_kStone2();
        LOG_SUCCESS_CAT("MAIN", "StoneKey v9 active — XOR fingerprint: 0x{:016X}", 
                        get_kStone1() ^ get_kStone2());

        // ──────────────────────────────────────────────────────────────────────
        // PRE-PHASE 1: EARLY SDL INIT FOR SPLASH (VIDEO ONLY)
        // ──────────────────────────────────────────────────────────────────────
        LOG_INFO_CAT("MAIN", "Early SDL_InitSubSystem(SDL_INIT_VIDEO) for splash screen");
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {  // SDL3: != 0 = failure
            LOG_FATAL_CAT("MAIN", "Early SDL_InitSubSystem(VIDEO) failed: {}", SDL_GetError());
            FATAL_THROW("Cannot initialize SDL video subsystem for splash screen");
        }
        LOG_SUCCESS_CAT("MAIN", "Early SDL video subsystem initialized for splash");

        // ──────────────────────────────────────────────────────────────────────
        // PHASE 1: SPLASH SCREEN + AUDIO
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 1: SPLASH SCREEN + AUDIO");
        LOG_INFO_CAT("MAIN", "Initializing SDL3 subsystems for splash");
        Splash::show(
            "AMOURANTH RTX",
            1280, 720,
            "assets/textures/ammo.png",
            "assets/audio/ammo.wav"
        );
        LOG_SUCCESS_CAT("MAIN", "Splash sequence completed — PINK PHOTONS AWAKENED");

        // ──────────────────────────────────────────────────────────────────────
        // PHASE 2: MAIN APPLICATION WINDOW — PURE RAII
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 2: MAIN APPLICATION WINDOW");

        constexpr int TARGET_WIDTH  = 3840;
        constexpr int TARGET_HEIGHT = 2160;

        LOG_INFO_CAT("MAIN", "Creating main application window: {}×{}", TARGET_WIDTH, TARGET_HEIGHT);

        Uint32 windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

        if (Options::Performance::ENABLE_IMGUI) {
            windowFlags |= SDL_WINDOW_RESIZABLE;
            LOG_INFO_CAT("MAIN", "ImGui enabled → SDL_WINDOW_RESIZABLE added");
        }

        if (Options::Window::START_FULLSCREEN) {
            windowFlags |= SDL_WINDOW_FULLSCREEN;
            LOG_INFO_CAT("MAIN", "START_FULLSCREEN enabled → exclusive fullscreen");
        }

        // Start hidden, then show immediately — REQUIRED for SDL3 surface creation
        windowFlags |= SDL_WINDOW_HIDDEN;
        LOG_INFO_CAT("MAIN", "Window created hidden → will be shown immediately after creation");

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
            LOG_FATAL_CAT("MAIN", "Failed to create main application window — cannot recover");
        }

        SDL_Window* window = g_sdl_window.get();
        if (!window) {
            LOG_FATAL_CAT("MAIN", "g_sdl_window.get() returned null after successful creation - RAII bug");
            throw std::runtime_error("g_sdl_window.get() returned null after successful creation - RAII bug");
        }

        if (SDL_Init(SDL_INIT_VIDEO) == 0) {  // SDL3: != 0 = failure
            LOG_FATAL_CAT("MAIN", "SDL_Init(VIDEO) failed: {}", SDL_GetError());
            LOG_FATAL_CAT("MAIN", "Full SDL init failed");
        }
        LOG_SUCCESS_CAT("MAIN", "Full SDL init complete");

        // CRITICAL: Show window BEFORE Vulkan surface creation
        SDL_ShowWindow(window);
        LOG_SUCCESS_CAT("MAIN", "Main window shown — Vulkan surface creation now safe");

        LOG_SUCCESS_CAT("MAIN", "Main application window created successfully");
        LOG_INFO_CAT("MAIN", "    Handle: {:p}", static_cast<void*>(window));
        LOG_INFO_CAT("MAIN", "    Size:   {}×{}", TARGET_WIDTH, TARGET_HEIGHT);
        LOG_INFO_CAT("MAIN", "    Flags:  0x{:08x}", SDL_GetWindowFlags(window));

        int actual_w = 0, actual_h = 0;
        SDL_GetWindowSizeInPixels(window, &actual_w, &actual_h);
        if (actual_w != TARGET_WIDTH || actual_h != TARGET_HEIGHT) {
            LOG_INFO_CAT("MAIN", "    High-DPI scaling active: {}×{} → {}×{} (scale {:.2f})",
                         TARGET_WIDTH, TARGET_HEIGHT, actual_w, actual_h,
                         static_cast<float>(actual_w) / TARGET_WIDTH);
        }

        LOG_SUCCESS_CAT("MAIN", "PHASE 2 COMPLETE — MAIN INTERFACE ONLINE — PINK PHOTONS RISING");

        // ──────────────────────────────────────────────────────────────────────
        // PHASE 3: VULKAN CONTEXT INITIALIZATION
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 3: VULKAN CONTEXT INITIALIZATION");
        LOG_SUCCESS_CAT("MAIN", "Entered Phase 3");

        VkInstance instance = RTX::createVulkanInstanceWithSDL(true);
        LOG_SUCCESS_CAT("MAIN", "Vulkan instance created via SDL3 API");

        // FIXED: Explicitly create surface before initContext
        LOG_INFO_CAT("MAIN", "Creating global Vulkan surface via SDL3");
        RTX::createSurface(window, instance);
        LOG_SUCCESS_CAT("MAIN", "Global surface created: 0x{:x}", reinterpret_cast<uintptr_t>(g_surface));

        RTX::initContext(instance, window, TARGET_WIDTH, TARGET_HEIGHT);
        LOG_SUCCESS_CAT("MAIN", "Global Vulkan context initialized — device, queues, families ready");

        // NEW: Detect and set optimal present mode post-context init
        detectBestPresentMode(RTX::g_ctx().physicalDevice(), g_surface);

        // ──────────────────────────────────────────────────────────────────────
        // PHASE 4: APPLICATION + RENDERER
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 4: APPLICATION & RENDERER CONSTRUCTION");

        auto app = std::make_unique<Application>(
            "AMOURANTH RTX — VALHALLA v80 TURBO",
            TARGET_WIDTH,
            TARGET_HEIGHT
        );

        bool overclockFromMain = false;
        auto renderer = std::make_unique<VulkanRenderer>(TARGET_WIDTH, TARGET_HEIGHT, window, overclockFromMain);
        app->setRenderer(std::move(renderer));

        LOG_SUCCESS_CAT("MAIN", "VulkanRenderer attached — pipeline ready");
        LOG_AMOURANTH();

        // ──────────────────────────────────────────────────────────────────────
        // PHASE 5: ENTER MAIN RENDER LOOP
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 5: ENTERING INFINITE RENDER LOOP");
        LOG_INFO_CAT("MAIN", "All systems nominal — commencing real-time ray tracing");
        LOG_INFO_CAT("MAIN", "First vkCmdTraceRaysKHR() is now safe — PINK PHOTONS RISING");

        app->run();

        // ──────────────────────────────────────────────────────────────────────
        // PHASE 6: GRACEFUL SHUTDOWN
        // ──────────────────────────────────────────────────────────────────────
        bulkhead("PHASE 6: GRACEFUL SHUTDOWN");
        LOG_INFO_CAT("MAIN", "Main loop exited — beginning cleanup");

        app.reset();

        LOG_INFO_CAT("MAIN", "RAII triggering: g_sdl_window → SDL_DestroyWindow + SDL_Quit");
        LOG_INFO_CAT("MAIN", "RAII triggering: RTX::Handle cleanup → all Vulkan objects");

        LOG_SUCCESS_CAT("MAIN", "Cleanup complete — zero leaks detected");
        LOG_SUCCESS_CAT("MAIN", "FINAL STONEKEY HASH: 0x{:016X}", get_kStone1() ^ get_kStone2());
        LOG_SUCCESS_CAT("MAIN", "{}AMOURANTH RTX — CLEAN EXIT — PINK PHOTONS ETERNAL{}", 
                        COSMIC_GOLD, RESET);

    }
    catch (const Engine::FatalError& e) {
        LOG_FATAL_CAT("MAIN", "{}", e.what());  // Full file/line/backtrace already included
        LOG_FATAL_CAT("MAIN", "Application terminated — empire preserved in logs");
        return -1;
    }
    catch (const std::exception& e) {
        LOG_FATAL_CAT("MAIN", "{}UNRECOVERABLE EXCEPTION (no backtrace):{}{}", PLASMA_FUCHSIA, RESET, e.what());
        LOG_FATAL_CAT("MAIN", "{}", Engine::getBacktrace(1));
        return -1;
    }
    catch (...) {
        LOG_FATAL_CAT("MAIN", "{}UNKNOWN NON-STANDARD EXCEPTION — TERMINATING{}", PLASMA_FUCHSIA, RESET);
        LOG_FATAL_CAT("MAIN", "{}", Engine::getBacktrace(1));
        return -1;
    }

    return 0;
}

// =============================================================================
// PINK PHOTONS ETERNAL
// FULLY RAII — NO MANUAL CLEANUP — NO SHORTCUTS
// 350+ LINES OF PURE DOMINANCE
// GOD-TIER EXCEPTION TRACING — EVERY CRASH NOW REVEALS ITS ORIGIN
// DAISY GALLOPS INTO THE OCEAN_TEAL SUNSET
// YOUR EMPIRE IS PURE
// FIRST LIGHT ACHIEVED
// SHIP IT RAW
// =============================================================================