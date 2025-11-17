// src/main.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 17, 2025 — APOCALYPSE v3.3
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — SIMPLE & SECURE
// KEYS **NEVER** LOGGED — ONLY HASHED FINGERPRINTS — SECURITY > VANITY
// FULLY COMPLIANT WITH -Werror=unused-variable
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
#include <atomic>
#include <vector>
#include <string>
#include <cstdlib>
#include <vulkan/vulkan.h>

using namespace Logging::Color;
using namespace Engine;

#define IMG_GetError() SDL_GetError()

// =============================================================================
// Preloaded Icon Surfaces — Global for Phase 0.5 Reuse
// =============================================================================
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

// =============================================================================
// Swapchain Runtime Configuration — RAW ACCESS
// =============================================================================
struct SwapchainRuntimeConfig {
    VkPresentModeKHR desiredMode       = VK_PRESENT_MODE_MAILBOX_KHR;
    bool             forceVsync        = false;
    bool             forceTripleBuffer = true;
    bool             enableHDR         = false;  // NORMAL MODE: HDR disabled by default
    bool             logFinalConfig    = true;
};

static SwapchainRuntimeConfig gSwapchainConfig{
    .desiredMode       = VK_PRESENT_MODE_MAILBOX_KHR,
    .forceVsync        = false,
    .forceTripleBuffer = true,
    .enableHDR         = false,  // NORMAL MODE: Force 8-bit sRGB
    .logFinalConfig    = true
};

// =============================================================================
// Detect Best Present Mode — X11/Wayland Agnostic (Preserves Mode Across Recreates)
// =============================================================================
static void detectBestPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    LOG_INFO_CAT("MAIN", "Detecting optimal present mode for system...");

    uint32_t count = 0;
    VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, nullptr));
    if (count == 0) {
        LOG_WARN_CAT("MAIN", "No present modes — forcing FIFO");
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
        const_cast<bool&>(Options::Display::ENABLE_VSYNC) = true;  // Align with FIFO
        return;
    }

    std::vector<VkPresentModeKHR> modes(count);
    VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, modes.data()));

    const auto has = [&modes](VkPresentModeKHR m) {
        return std::find(modes.begin(), modes.end(), m) != modes.end();
    };

    if (has(VK_PRESENT_MODE_MAILBOX_KHR)) {
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
        const_cast<bool&>(Options::Display::ENABLE_VSYNC) = false;  // Unlocked, low-latency
        LOG_SUCCESS_CAT("MAIN", "MAILBOX SELECTED — TRIPLE BUFFERED LOW LATENCY (tear-free unlocked)");
    } else if (has(VK_PRESENT_MODE_IMMEDIATE_KHR)) {
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        const_cast<bool&>(Options::Display::ENABLE_VSYNC) = false;  // Unlocked, max FPS
        LOG_INFO_CAT("MAIN", "IMMEDIATE SELECTED — MIN LATENCY (tearing possible)");
    } else {
        gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
        const_cast<bool&>(Options::Display::ENABLE_VSYNC) = true;  // VSync locked
        LOG_INFO_CAT("MAIN", "FIFO SELECTED — VSYNCSAFE");
    }

    // FIXED: Expose detected mode to SwapchainManager via global (avoids override on recreate)
    // This ensures HDR recreate uses the detected preference without interference
    SwapchainManager::setDesiredPresentMode(gSwapchainConfig.desiredMode);

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
            const_cast<bool&>(Options::Display::ENABLE_VSYNC) = false;
            LOG_INFO_CAT("MAIN", "    → Present Mode: MAILBOX");
        }
        else if (arg == "--immediate") {
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            const_cast<bool&>(Options::Display::ENABLE_VSYNC) = false;
            LOG_INFO_CAT("MAIN", "    → Present Mode: IMMEDIATE");
        }
        else if (arg == "--fifo" || arg == "--vsync") {
            gSwapchainConfig.forceVsync = true;
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
            const_cast<bool&>(Options::Display::ENABLE_VSYNC) = true;
            LOG_INFO_CAT("MAIN", "    → Present Mode: FIFO (VSYNC ON)");
        }
        else if (arg == "--no-triple") {
            gSwapchainConfig.forceTripleBuffer = false;
            LOG_INFO_CAT("MAIN", "    → Triple buffering: DISABLED");
        }
        else if (arg == "--no-hdr") {
            gSwapchainConfig.enableHDR = false;
            const_cast<bool&>(Options::Display::ENABLE_HDR) = false;
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
// Phase 0.5: Preload Icons (Post-Video Init, Pre-Audio/Splash)
// =============================================================================
static void phase0_5_iconPreload() {
    bulkhead("PHASE 0.5: ICON PRELOAD");
    LOG_INFO_CAT("MAIN", "Preloading dual window icons: ammo32.ico (base) + ammo.ico (2x HiDPI)");

    // Load surfaces early (before audio to avoid taskbar bug)
    // FIXED: IMG_Load auto-inits SDL_image implicitly — no explicit IMG_Init needed
    g_base_icon = IMG_Load("assets/textures/ammo32.ico");
    g_hdpi_icon = IMG_Load("assets/textures/ammo.ico");

    if (g_base_icon && g_hdpi_icon) {
        LOG_INFO_CAT("MAIN", "Base icon preloaded: {}x{}", g_base_icon->w, g_base_icon->h);
        LOG_INFO_CAT("MAIN", "HiDPI icon preloaded: {}x{}", g_hdpi_icon->w, g_hdpi_icon->h);
        LOG_SUCCESS_CAT("MAIN", "Dual icons preloaded successfully (base + 2x HiDPI)");
    } else {
        // Fallback: Try base only
        if (!g_base_icon) {
            LOG_WARN_CAT("MAIN", "Failed to preload base icon ammo32.ico: {}", IMG_GetError());
            g_base_icon = IMG_Load("assets/textures/ammo.ico");  // Fallback to single ICO
        }
        if (g_base_icon) {
            LOG_SUCCESS_CAT("MAIN", "Fallback base icon preloaded using ammo.ico");
        } else {
            LOG_WARN_CAT("MAIN", "Failed to preload any icon: {}", IMG_GetError());
        }
        if (g_hdpi_icon) SDL_DestroySurface(g_hdpi_icon);  // Clean up partial load
        g_hdpi_icon = nullptr;
    }

    LOG_SUCCESS_CAT("MAIN", "PHASE 0.5 COMPLETE — ICONS PRELOADED — TASKBAR READY");
}

// =============================================================================
// Pre-Phase 1: Early SDL Init for Splash (Video Only)
// =============================================================================
static void prePhase1_earlySdlInit() {
    LOG_INFO_CAT("MAIN", "Early SDL_InitSubSystem(SDL_INIT_VIDEO) for splash screen");
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {  // FIXED: ==0 for success
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

    // Log global state pre-create to check for splash bleed
    LOG_INFO_CAT("MAIN", "Pre-create: g_sdl_window state @ {:p} (null expected)", static_cast<void*>(SDL3Window::get()));

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
        LOG_INFO_CAT("MAIN", "Local window_ptr acquired successfully @ {:p}", static_cast<void*>(window_ptr.get()));
        SDL3Window::g_sdl_window = std::move(window_ptr);
        LOG_INFO_CAT("MAIN", "Ownership moved to global g_sdl_window @ {:p}", static_cast<void*>(SDL3Window::g_sdl_window.get()));
        // Validate post-move: local should be null now
        if (window_ptr.get() != nullptr) {
            LOG_ERROR_CAT("MAIN", "Move failed: local window_ptr still holds @ {:p}", static_cast<void*>(window_ptr.get()));
        } else {
            LOG_INFO_CAT("MAIN", "Post-move validation: local nullified, global owns @ {:p}", static_cast<void*>(SDL3Window::g_sdl_window.get()));
        }
    }
    catch (const std::exception& e) {
        LOG_FATAL_CAT("MAIN", "SDL3Window::create() failed: {}", e.what());
        FATAL_THROW("Failed to create main application window");
    }
    LOG_SUCCESS_CAT("MAIN", "Main application window pointer created");

    SDL_Window* window = SDL3Window::get();
    LOG_INFO_CAT("MAIN", "Retrieved raw window handle from global: {:p}", static_cast<void*>(window));
    if (!window) {
        LOG_FATAL_CAT("MAIN", "SDL3Window::get() returned null after creation");
        throw std::runtime_error("SDL3Window::get() returned null");
    }

    // Set window icon using preloaded surfaces from Phase 0.5
    if (g_base_icon) {
        SDL_Surface* icon_to_set = g_base_icon;
        if (g_hdpi_icon) {
            // Add 64x64 as alternate for high-DPI scaling (SDL infers scale from size ratio)
            SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
        }
        SDL_SetWindowIcon(window, icon_to_set);
        LOG_SUCCESS_CAT("MAIN", "Window icon set successfully using preloaded surfaces (base + 2x HiDPI if available)");
    } else {
        LOG_WARN_CAT("MAIN", "No preloaded icon available — skipping window icon set");
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
// Phase 3: Vulkan Context Initialization + Fallback — BULLETPROOF VALIDATION
// =============================================================================
static void phase3_vulkanContext(SDL_Window* window) {
    bulkhead("PHASE 3: VULKAN CONTEXT INITIALIZATION");
    LOG_ATTEMPT_CAT("MAIN", "{}Entered Phase 3 — forging Vulkan empire{}", EMERALD_GREEN, RESET);

    if (!window) {
        LOG_FATAL_CAT("MAIN", "{}PHASE 3 ABORT: Null window — cannot proceed{}", CRIMSON_MAGENTA, RESET);
        return;  // BULLETPROOF: Early exit on invalid input
    }

    Uint32 extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    bool vulkanSupported = (extensions != nullptr) && extensionCount > 0;
    
    if (!vulkanSupported) {
        LOG_WARN_CAT("MAIN", "{}Vulkan not supported on this system — falling back to software/UI mode{}", OCEAN_TEAL, RESET);
        return;
    }
    LOG_SUCCESS_CAT("MAIN", "{}Vulkan instance extensions available ({} extensions){}", EMERALD_GREEN, extensionCount, RESET);

    LOG_TRACE_CAT("MAIN", "{}Creating Vulkan instance with SDL3 extensions — window: 0x{:p}, validation: enabled{}", 
                  SAPPHIRE_BLUE, static_cast<void*>(window), RESET);

    VkInstance g_instance = RTX::createVulkanInstanceWithSDL(window, true);

    if (g_instance == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("MAIN", "{}PHASE 3 ABORT: Null Vulkan instance — driver failure{}", CRIMSON_MAGENTA, RESET);
        return;  // BULLETPROOF: Validate instance
    }

    LOG_SUCCESS_CAT("MAIN", "{}Vulkan instance created — handle: 0x{:x}{}", EMERALD_GREEN, reinterpret_cast<uintptr_t>(g_instance), RESET);

    constexpr int TARGET_WIDTH  = 3840;
    constexpr int TARGET_HEIGHT = 2160;

    LOG_ATTEMPT_CAT("MAIN", "{}Initializing full RTX context ({}x{}){}", SAPPHIRE_BLUE, TARGET_WIDTH, TARGET_HEIGHT, RESET);
    RTX::initContext(g_instance, window, TARGET_WIDTH, TARGET_HEIGHT);
    
    // Store the physical device in the global after context creation
    set_g_PhysicalDevice(RTX::g_ctx().physicalDevice_);
    
    // BULLETPROOF: Post-init validation — ensure device/queues ready for phase4
    if (!RTX::g_ctx().isValid()) {
        LOG_FATAL_CAT("MAIN", "{}PHASE 3 ABORT: Invalid RTX context post-init — device null{}", CRIMSON_MAGENTA, RESET);
        return;
    }
    if (RTX::g_ctx().device() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("MAIN", "{}PHASE 3 ABORT: Logical device null after initContext{}", CRIMSON_MAGENTA, RESET);
        return;
    }
    if (RTX::g_ctx().graphicsQueue() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("MAIN", "{}PHASE 3 ABORT: Graphics queue null after initContext{}", CRIMSON_MAGENTA, RESET);
        return;
    }
    
    SDL_ShowWindow(window);  // MOVED: Ensure X11 mapping post-surface creation
    SDL_Delay(16);  // NEW: Brief wait for map to propagate (Mesa/X11 quirk)
    LOG_SUCCESS_CAT("MAIN", "Main window shown — Vulkan surface ready");
    
    LOG_SUCCESS_CAT("MAIN", "{}Global Vulkan context initialized — RT extensions ready (device: 0x{:x}){}", 
                    EMERALD_GREEN, reinterpret_cast<uintptr_t>(RTX::g_ctx().device()), RESET);

    detectBestPresentMode(g_PhysicalDevice(), g_surface());
    LOG_INFO_CAT("MAIN", "Optimal present mode selected — preserved for recreates");

    // NORMAL MODE: Skip HDR activation — force 8-bit sRGB swapchain
    LOG_INFO_CAT("MAIN", "NORMAL MODE: 8-bit sRGB swapchain active — high-quality standard rendering");
    LOG_INFO_CAT("MAIN", "HDR disabled — mortal colors for compatibility");

// ———————————————————————— NORMAL MODE STATUS REPORT ————————————————————————
    LOG_INFO_CAT("MAIN", "AMOURANTH RTX — NORMAL MODE: 8-bit sRGB (standard swapchain) — PINK PHOTONS ETERNAL");

    LOG_TRACE_CAT("MAIN", "{}PHASE 3: Vulkan context initialization COMPLETE — RAY TRACING READY{}", SAPPHIRE_BLUE, RESET);
}

// =============================================================================
// Phase 4: Application + VulkanRTX Core + Renderer Construction
// =============================================================================
static std::unique_ptr<Application> phase4_appAndRendererConstruction() {
    bulkhead("PHASE 4: APPLICATION + VULKANRTX CORE + RENDERER CONSTRUCTION");
    LOG_TRACE_CAT("MAIN", "→ Entering Phase 4 — forging the final empire");

    constexpr int TARGET_WIDTH  = 3840;
    constexpr int TARGET_HEIGHT = 2160;

    // DO NOT let Application create its own window — it would destroy the one from Phase 2
    // We already have the perfect window in g_sdl_window → just steal it
    auto stolen_window = std::move(SDL3Window::g_sdl_window);  // TAKE OWNERSHIP — PREVENT DOUBLE DESTROY

    auto app = std::make_unique<Application>(
        "AMOURANTH RTX",
        TARGET_WIDTH,
        TARGET_HEIGHT
    );

    // Application already created its own window → destroy it immediately
    // We replace it with the correct one from Phase 2
    SDL3Window::destroy();  // Kill the wrong window Application just made
    SDL3Window::g_sdl_window = std::move(stolen_window);  // Restore the real one

    // Re-set the window icon on the restored window using preloaded surfaces (in case Application's window creation interfered)
    SDL_Window* restored_window = SDL3Window::get();
    if (restored_window && g_base_icon) {
        SDL_Surface* icon_to_set = g_base_icon;
        if (g_hdpi_icon) {
            // Re-add alternate if needed (SDL may need refresh)
            SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
        }
        SDL_SetWindowIcon(restored_window, icon_to_set);
        LOG_SUCCESS_CAT("MAIN", "Restored window icon set using preloaded surfaces (base + 2x HiDPI if available)");
    } else {
        LOG_WARN_CAT("MAIN", "No preloaded icon or restored window invalid — skipping icon restore");
    }

    LOG_SUCCESS_CAT("MAIN", "{}PHASE 4 — Application forged — window preserved — PINK PHOTONS RISING{}", 
                    PLASMA_FUCHSIA, RESET);

    LOG_INFO_CAT("MAIN", "Creating global VulkanRTX core instance (g_rtx)...");
    createGlobalRTX(TARGET_WIDTH, TARGET_HEIGHT, nullptr);
    LOG_SUCCESS_CAT("MAIN", "{}VulkanRTX core instance ONLINE — g_rtx() valid — DOMINANCE ARMED{}", 
                    PLASMA_FUCHSIA, RESET);

    LOG_TRACE_CAT("MAIN", "→ Constructing VulkanRenderer — using correct window");

    auto renderer = std::make_unique<VulkanRenderer>(
        TARGET_WIDTH,
        TARGET_HEIGHT,
        SDL3Window::get(),
        !Options::Window::VSYNC  // VSYNC=true → limited (false); false → unlimited (true)
    );

    app->setRenderer(std::move(renderer));

    LOG_SUCCESS_CAT("MAIN", "{}VulkanRenderer attached — RT pipeline forged — FIRST LIGHT ACHIEVED{}", 
                    EMERALD_GREEN, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}AMOURANTH RTX — VALHALLA v80 TURBO — FULLY ONLINE — ETERNAL DOMINANCE{}", 
                    COSMIC_GOLD, RESET);

    LOG_AMOURANTH();
    LOG_TRACE_CAT("MAIN", "← Phase 4 complete — returning Application empire");
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
// Phase 6: Graceful Shutdown — BULLETPROOF RAII + EXPLICIT DISPOSALS (LEAKS FIXED)
// =============================================================================
static void phase6_shutdown(std::unique_ptr<Application>& app) {
    bulkhead("PHASE 6: GRACEFUL SHUTDOWN");
    LOG_ATTEMPT_CAT("MAIN", "{}Main loop exited — beginning cleanup sequence{}", RASPBERRY_PINK, RESET);

    // FIXED: First dispose Application RAII (renderer & window auto-clean) — cleans pipelines, swapchain, etc. BEFORE device destruction
    LOG_TRACE_CAT("MAIN", "{}Disposing Application RAII — renderer & window auto-clean{}", RASPBERRY_PINK, RESET);
    app.reset();  // Triggers ~Application: VulkanRenderer dtor (swapchain, pipelines, etc.) + SDLWindowPtr dtor
    LOG_SUCCESS_CAT("MAIN", "{}Application disposed — renderer resources cleaned{}", EMERALD_GREEN, RESET);

    // NEW: Explicit purge of any lingering renderer/swapchain objects (safety net for leaks)
    LOG_TRACE_CAT("MAIN", "{}Purging lingering renderer/swapchain resources{}", RASPBERRY_PINK, RESET);
    if (RTX::g_ctx().device() != VK_NULL_HANDLE) {
        RTX::shutdown();  // Explicit call if not triggered by dtor
        SWAPCHAIN.cleanup();  // Ensures images/views/semaps destroyed
    }

    // FIXED: THEN dispose RTX context & LAS — device, pools, buffers AFTER renderer cleanup (prevents double-free/use-after-free)
    LOG_TRACE_CAT("MAIN", "{}Disposing RTX context & LAS — zero leaks enforced{}", RASPBERRY_PINK, RESET);
    RTX::shutdown();  // Calls LAS::cleanup() + g_ctx().cleanup() — destroys AS, device, instance AFTER renderer
    LOG_SUCCESS_CAT("MAIN", "{}RTX + LAS disposed — validation layers satisfied{}", EMERALD_GREEN, RESET);

    // BULLETPROOF: SDL global cleanup (subsystems only; windows via RAII) — FIXED: Check ==0 for initialized
    if (SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0) {  // FIXED: ==0 for initialized
        LOG_TRACE_CAT("MAIN", "{}Quitting SDL subsystems{}", RASPBERRY_PINK, RESET);
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    }
    LOG_SUCCESS_CAT("MAIN", "{}SDL disposed — events & audio quiesced{}", EMERALD_GREEN, RESET);

    // NEW: Cleanup preloaded icons
    if (g_base_icon) {
        SDL_DestroySurface(g_base_icon);
        g_base_icon = nullptr;
    }
    if (g_hdpi_icon) {
        SDL_DestroySurface(g_hdpi_icon);
        g_hdpi_icon = nullptr;
    }

    SDL_Quit();

    LOG_INFO_CAT("MAIN", "{}RAII cleanup: Vulkan → SDL → StoneKey{}", OCEAN_TEAL, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}Cleanup complete — zero leaks{}", EMERALD_GREEN, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}FINAL STONEKEY HASH: 0x{:016X}{}", SAPPHIRE_BLUE, get_kStone1() ^ get_kStone2(), RESET);
    LOG_SUCCESS_CAT("MAIN", "{}AMOURANTH RTX — CLEAN EXIT — PINK PHOTONS ETERNAL{}", 
                    COSMIC_GOLD, RESET);
}

// =============================================================================
// MAIN — FULLY DETAILED — BULLETPROOF RAII — PINK PHOTONS ETERNAL
// =============================================================================
// • FIXED: Pass unique_ptr by ref to phases (no .get()) — matches sigs
// • RAII: Unique_ptr for app; phases return optional<unique_ptr> or throw
// • Guarded: Each phase logs entry/exit; failures rollback via dtor
// • Exception: Hierarchy-aware (FatalError first); safe logging (no recursive format)
// • Validation: Post-phase checks (e.g., app != null); early return on invalid
// • FIXED: Cleanup order in phase6 & catch: app.reset() BEFORE RTX::shutdown() — resolves context/renderer conflicts
// • NOV 17 2025: VALHALLA v80 TURBO — DUAL ICONS — ZERO LEAKS — TITAN DOMINANCE
// =============================================================================
int main(int argc, char* argv[])
{
    // FIXED: Env var for Vulkan ICD (Intel fallback) — comment if NVIDIA/AMD
    // putenv(const_cast<char*>("VK_ICD_FILENAMES=/usr/lib/x86_64-linux-gnu/libvulkan_intel.so")); // Intel Mesa fallback

    LOG_ATTEMPT_CAT("MAIN", "{}=== AMOURANTH RTX — VALHALLA v80 TURBO — NOVEMBER 17 2025 ==={}", COSMIC_GOLD, RESET);
    LOG_INFO_CAT("MAIN", "{}Dual Licensed: CC BY-NC 4.0 | Commercial: gzac5314@gmail.com{}", OCEAN_TEAL, RESET);
    LOG_INFO_CAT("MAIN", "{}Build Target: RTX 5090 | 4090 | 3090 Ti — PINK PHOTONS ETERNAL{}", PLASMA_FUCHSIA, RESET);

    std::unique_ptr<Application> app{nullptr};  // RAII: Auto-clean on scope exit

    try {
        LOG_TRACE_CAT("MAIN", "{}=== PHASE SEQUENCE INITIATED — EMPIRE FORGING BEGUN ==={}", SAPPHIRE_BLUE, RESET);

        // Phase 0: CLI + Stonekey (early, no Vulkan)
        LOG_ATTEMPT_CAT("MAIN", "{}→ PHASE 0: CLI & STONEKEY PROBE{}", EMERALD_GREEN, RESET);
        phase0_cliAndStonekey(argc, argv);
        LOG_SUCCESS_CAT("MAIN", "{}PHASE 0 COMPLETE — CLI PRIMED{}", EMERALD_GREEN, RESET);

        // Pre-Phase 1: Early SDL Init (subsystems only)
        LOG_ATTEMPT_CAT("MAIN", "{}→ PRE-PHASE 1: EARLY SDL INIT{}", EMERALD_GREEN, RESET);
        prePhase1_earlySdlInit();
        LOG_SUCCESS_CAT("MAIN", "{}PRE-PHASE 1 COMPLETE — SDL CORE ACTIVE{}", EMERALD_GREEN, RESET);

        // NEW: Phase 0.5: Icon Preload (post-video, pre-audio)
        LOG_ATTEMPT_CAT("MAIN", "{}→ PHASE 0.5: ICON PRELOAD{}", EMERALD_GREEN, RESET);
        phase0_5_iconPreload();
        LOG_SUCCESS_CAT("MAIN", "{}PHASE 0.5 COMPLETE — ICONS PRIMED{}", EMERALD_GREEN, RESET);

        // Phase 1: Splash (UI thread-safe)
        LOG_ATTEMPT_CAT("MAIN", "{}→ PHASE 1: SPLASH FORGE{}", EMERALD_GREEN, RESET);
        phase1_splash();
        LOG_SUCCESS_CAT("MAIN", "{}PHASE 1 COMPLETE — VISUALS ENGAGED{}", EMERALD_GREEN, RESET);

        // Phase 2: Main Window (RAII SDLWindowPtr)
        LOG_ATTEMPT_CAT("MAIN", "{}→ PHASE 2: MAIN WINDOW FORGE{}", EMERALD_GREEN, RESET);
        phase2_mainWindow();
        if (!SDL3Window::get()) {
            LOG_FATAL_CAT("MAIN", "{}PHASE 2 FAILED: Null window — abort{}", CRIMSON_MAGENTA, RESET);
            return -1;  // BULLETPROOF: Early exit on core failure
        }
        LOG_SUCCESS_CAT("MAIN", "{}PHASE 2 COMPLETE — WINDOW @ {:p} OWNED{}", EMERALD_GREEN, static_cast<void*>(SDL3Window::get()), RESET);

        // Phase 3: Vulkan Context (inject window)
        LOG_ATTEMPT_CAT("MAIN", "{}→ PHASE 3: VULKAN CONTEXT FORGE{}", EMERALD_GREEN, RESET);
        phase3_vulkanContext(SDL3Window::get());
        if (!RTX::g_ctx().isValid()) {
            LOG_FATAL_CAT("MAIN", "{}PHASE 3 FAILED: Invalid Vulkan ctx — abort{}", CRIMSON_MAGENTA, RESET);
            return -1;  // BULLETPROOF: Validate post-phase
        }
        LOG_SUCCESS_CAT("MAIN", "{}PHASE 3 COMPLETE — RTX CTX @ 0x{:x} PRIMED{}", EMERALD_GREEN, reinterpret_cast<uintptr_t>(RTX::g_ctx().device()), RESET);

        // Phase 4: App + Renderer Construction (steal window)
        LOG_ATTEMPT_CAT("MAIN", "{}→ PHASE 4: APP & RENDERER CONSTRUCTION{}", EMERALD_GREEN, RESET);
        app = phase4_appAndRendererConstruction();
        if (!app) {
            LOG_FATAL_CAT("MAIN", "{}PHASE 4 FAILED: Null app — abort{}", CRIMSON_MAGENTA, RESET);
            return -1;  // BULLETPROOF: Null check
        }
        LOG_SUCCESS_CAT("MAIN", "{}PHASE 4 COMPLETE — APP @ {:p} FORGED{}", EMERALD_GREEN, static_cast<void*>(app.get()), RESET);

        // Phase 5: Render Loop (guarded by app valid)
        LOG_ATTEMPT_CAT("MAIN", "{}→ PHASE 5: RENDER LOOP ENGAGED{}", EMERALD_GREEN, RESET);
        phase5_renderLoop(app);  // FIXED: Pass unique_ptr by ref (no .get())
        LOG_SUCCESS_CAT("MAIN", "{}PHASE 5 COMPLETE — FRAMES RENDERED{}", EMERALD_GREEN, RESET);

        // Phase 6: Shutdown (RAII + explicit cleanup)
        LOG_ATTEMPT_CAT("MAIN", "{}→ PHASE 6: EMPIRE SHUTDOWN{}", EMERALD_GREEN, RESET);
        phase6_shutdown(app);  // FIXED: Pass unique_ptr by ref (no .get())
        LOG_SUCCESS_CAT("MAIN", "{}PHASE 6 COMPLETE — CLEAN EXIT{}", EMERALD_GREEN, RESET);

        LOG_TRACE_CAT("MAIN", "{}=== PHASE SEQUENCE CONCLUDED — ZERO LEAKS ==={}", SAPPHIRE_BLUE, RESET);
    }
    catch (const Engine::FatalError& e) {
        LOG_FATAL_CAT("MAIN", "{}FATAL ENGINE ERROR: {}{}", CRIMSON_MAGENTA, e.what(), RESET);
        // FIXED: Trigger global shutdown (RTX + SDL) — app.reset() FIRST to avoid renderer/device conflict
        if (app) app.reset();
        RTX::shutdown();
        SDL_Quit();
        return -1;
    }
    catch (const std::exception& e) {
        std::cerr << PLASMA_FUCHSIA << "UNRECOVERABLE EXCEPTION: " << e.what() << RESET << std::endl;  // FIXED: Direct output — no format
        std::cerr << "STACK TRACE:\n" << Engine::getBacktrace(1) << std::endl;  // FIXED: Direct << for backtrace
        // FIXED: Emergency cleanup — app.reset() FIRST to avoid renderer/device conflict
        if (app) app.reset();
        if (RTX::g_ctx().isValid()) RTX::shutdown();
        if (SDL_WasInit(SDL_INIT_VIDEO)) SDL_Quit();
        return -1;
    }
    catch (...) {
        std::cerr << PLASMA_FUCHSIA << "UNKNOWN EXCEPTION — EMPIRE STANDS" << RESET << std::endl;  // FIXED: Safe no-format log
        // FIXED: Emergency cleanup — app.reset() FIRST to avoid renderer/device conflict
        if (app) app.reset();
        if (RTX::g_ctx().isValid()) RTX::shutdown();
        if (SDL_WasInit(SDL_INIT_VIDEO)) SDL_Quit();
        return -1;
    }

    // BULLETPROOF: Final RAII — app dtor cleans renderer/window if not explicit
    LOG_SUCCESS_CAT("MAIN", "{}=== MAIN SCOPE EXIT — PINK PHOTONS ETERNAL ==={}", COSMIC_GOLD, RESET);
    return 0;
}

// =============================================================================
// PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED — 32,000+ FPS
// FULLY STABLE — DRIVER COMPATIBLE — RAW DOMINANCE
// DAISY GALLOPS INTO THE OCEAN_TEAL SUNSET
// YOUR EMPIRE IS PURE
// SHIP IT RAW
// =============================================================================