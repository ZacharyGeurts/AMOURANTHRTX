#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
//
// Main engine entry point — called from developer's main.cpp
// =============================================================================

#include "engine/Camera.hpp"
#include "engine/SDL3.hpp"
#include "engine/ELLIE.hpp"
#include "engine/AMOURANTHRTX.hpp"
#include "engine/OptionsMenu.hpp"
#include "engine/RayCanvas.hpp"
#include "engine/Pipeline.hpp"

#include <SDL3/SDL_vulkan.h>

#include "FieldRtxHelp.hpp"
#include "FieldPlatformChrome.hpp"
#include "EngineCompat.hpp"

#include <memory>
#include <format>
#include <chrono>
#include <cstdlib>  // getenv for AMOURANTHRTX_HEADLESS / debug mode
#include <cstring>
#include <csignal>
#include <unistd.h>

static volatile sig_atomic_t g_gracefulQuitRequested = 0;

static void onGracefulSignal(int) noexcept {
    g_gracefulQuitRequested = 1;
    Options::SDL3::RequestQuit = true;
}

static void shutdownVulkanAndWindow(SDL_Window*& window) noexcept {
    const bool compileActive = Pipeline::hotswap_compile_still_running();
    if (compileActive) {
        LOG_WARNING_CAT("MAIN",
            "Background Vk pipeline compile still running — skipping vkDestroyDevice (process exit reclaims)");
    } else if (rtx().device) {
        vkDeviceWaitIdle(rtx().device);
    }
    Swapchain::cleanup();
    if (!compileActive) {
        if (rtx().transient_pool && rtx().device) {
            vkDestroyCommandPool(rtx().device, rtx().transient_pool, nullptr);
            rtx().transient_pool = VK_NULL_HANDLE;
        }
        if (rtx().device) {
            vkDestroyDevice(rtx().device, nullptr);
            rtx().device = VK_NULL_HANDLE;
        }
        if (rtx().surface && rtx().instance) {
            vkDestroySurfaceKHR(rtx().instance, rtx().surface, nullptr);
            rtx().surface = VK_NULL_HANDLE;
        }
        if (rtx().instance) {
            vkDestroyInstance(rtx().instance, nullptr);
            rtx().instance = VK_NULL_HANDLE;
        }
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
}

// Vulkan + window must be torn down before SDL_Quit (INPUT.shutdown); otherwise GLX/X11 faults.
static void finalizeEngineShutdown(SDL_Window*& window) noexcept {
    shutdownVulkanAndWindow(window);
    INPUT.shutdown();
}

// Global canvas - TV or monitor display
inline std::unique_ptr<RayCanvas> raycanvas;

// Main entry point
inline int navigator_main(int argc, char* argv[]) {
    if (FieldRtxHelp::argcWantsHelp(argc, argv)) {
        FieldRtxHelp::printBinaryHelp();
        return 0;
    }
    for (int i = 1; i < argc; ++i) {
        if (argv[i] && std::strcmp(argv[i], "--extended-field") == 0)
            amx::gOptions.extendedFieldDispatch = true;
    }
    if (std::getenv("AMOURANTHRTX_EXTENDED_FIELD") != nullptr)
        amx::gOptions.extendedFieldDispatch = true;
    install_apocalypse_handler();
    std::signal(SIGTERM, onGracefulSignal);
    std::signal(SIGINT, onGracefulSignal);
    Logging::Logger::get().startup();
    LOG_SUCCESS_CAT("MAIN", "Apocalypse handler & logger ready");
    LOG_DEBUG_CAT("MAIN", "navigator_main entry (source_location logged via ELLIE) | TotalTime.us()={:.1f} | entropy=0x{:016x} | TotalTime sealed?={}",
                  TotalTime::get().us(), TotalTime::get().entropy(), TotalTime::get().isSealed());
    LOG_DEBUG_CAT("THERMO", "Startup trace: pre-splash, pre-RayCanvas ctor — THERMO accountant + fabric will be populated in ctor/dispatch (pre-transitions, descriptor writes, clears, field seeds, entropy calcs, prevMaintCost, freeEnergyIncome, boundaryThermo, status)");
    LOG_SUCCESS_CAT("THERMO", "NAVIGATOR THERMO: apocalypse+ELLIE+TotalTime ready; full THERMO category (THERMO_PINK) + source_location for RayCanvas/Pipeline heavy usage in accountant pop + all thermo steps");
    LOG_INFO_CAT("MAIN", "Startup hex: entropy=0x{:016x} genesis~{:.1f}us | source_loc + colors + THERMO_PINK active", TotalTime::get().entropy(), TotalTime::get().us());

    // Headless / debug test mode detection (AMOURANTHRTX_HEADLESS=1 or HEADLESS or AMOURANTHRTX_DEBUG or AMOURANTHRTX_MAX_FRAMES=N>0)
    // Robust for constrained/headless envs: dummy audio, hidden window (already), skip real swapchain acquire/present waits.
    // MAX_FRAMES also forces headless for clean bounded run + dispatch/accountant guarantee.
    Options::SDL3::HeadlessMode = (std::getenv("AMOURANTHRTX_HEADLESS") != nullptr) ||
                                  (std::getenv("HEADLESS") != nullptr) ||
                                  (std::getenv("AMOURANTHRTX_DEBUG") != nullptr);
    if (const char* mf = std::getenv("AMOURANTHRTX_MAX_FRAMES")) {
        Options::SDL3::MaxFrames = std::atoll(mf);
        if (Options::SDL3::MaxFrames > 0) {
            Options::SDL3::HeadlessMode = true;
            LOG_INFO_CAT("MAIN", "AMOURANTHRTX_MAX_FRAMES={} — forcing HEADLESS + bounded run for clean exit after dispatch+accountant pop", Options::SDL3::MaxFrames);
        }
    }
    if (const char* bw = std::getenv("AMOURANTHRTX_BENCH_W")) {
        const int w = std::atoi(bw);
        if (w >= 640) Options::SDL3::DefaultWidth = w;
    }
    if (const char* bh = std::getenv("AMOURANTHRTX_BENCH_H")) {
        const int h = std::atoi(bh);
        if (h >= 480) Options::SDL3::DefaultHeight = h;
    }
    if (std::getenv("AMOURANTHRTX_VALIDATION") == nullptr
            || std::getenv("AMOURANTHRTX_NO_VALIDATION") != nullptr) {
        unsetenv("VK_INSTANCE_LAYERS");
        unsetenv("VK_LOADER_LAYERS_ENABLE");
        LOG_INFO_CAT("MAIN", "Validation layers disabled (set AMOURANTHRTX_VALIDATION=1 to enable)");
    }

    if (Options::SDL3::HeadlessMode) {
        if (!std::getenv("AMOURANTHRTX_BENCH_W"))
            Options::SDL3::DefaultWidth = 3840;
        if (!std::getenv("AMOURANTHRTX_BENCH_H"))
            Options::SDL3::DefaultHeight = 2160;
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        LOG_INFO_CAT("MAIN", "HEADLESS/DEBUG mode detected — forcing offscreen/dummy hints + hidden window + offscreen dispatch path (no swap waits). Accountant + fabric will still run N frames.");
    }

    FieldPlatformChrome::initAppIdentity();
    int sdlInitResult = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);
    if (sdlInitResult != 0) {
        const char* sdlErr = SDL_GetError();
        LOG_WARNING_CAT("SDL3", "SDL_Init returned non-zero ({}). This env has X11/DRI/NVIDIA but static SDL3 combined init is picky — continuing with real CreateWindow for trace.", sdlErr ? sdlErr : "no error string");
    }

    SDL_Window* window = SDL_CreateWindow("AMOURANTHRTX", Options::SDL3::DefaultWidth, Options::SDL3::DefaultHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN
    );

    if (!window) {
        LOG_FATAL_CAT("SDL3", "Window creation failed: {}", SDL_GetError());
        return 1;
    }
    FieldPlatformChrome::applyWindowIcon(window);
    if (!INPUT.init(window)) {
        if (Options::SDL3::HeadlessMode) {
            LOG_WARNING_CAT("SDL3", "INPUT.init failed in HEADLESS mode (likely dummy audio or constrained SDL) — continuing with partial input (dispatch/test path will still work; accountant populates independently)");
        } else {
            LOG_FATAL_CAT("SDL3", "SDL3/INPUT.init failed");
            SDL_DestroyWindow(window);
            return 1;
        }
    }

    if (!Options::SDL3::HeadlessMode) {
        SDL_ShowWindow(window);
        FieldPlatformChrome::applyWindowIcon(window);
        LOG_INFO_CAT("MAIN", "Main window shown (non-headless)");
    } else {
        LOG_INFO_CAT("MAIN", "Headless: main window kept HIDDEN; swapchain present/acquire skipped for test stability");
    }
    Pipeline::pump_startup_events();
    if (Options::SDL3::RequestQuit) {
        if (window) { SDL_DestroyWindow(window); window = nullptr; }
        INPUT.shutdown();
        return 0;
    }

    VkInstance instance = createVulkanInstance();
    if (instance == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("VULKAN", "Instance creation failed");
        if (window) { SDL_DestroyWindow(window); window = nullptr; }
        INPUT.shutdown();
        return 1;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface) == 0) {
        if (Options::SDL3::HeadlessMode) {
            LOG_WARNING_CAT("VULKAN", "HEADLESS: SDL_Vulkan_CreateSurface returned 0 (picky offscreen/SDL/minimized env, no real display) — continuing without real surface/swap (direct dispatch path; dispatch + accountant pop + thermo logs unaffected)");
            surface = VK_NULL_HANDLE;
        } else {
            LOG_FATAL_CAT("VULKAN", "Failed to create Vulkan surface: {}", "SDL_Vulkan_CreateSurface returned 0");
            vkDestroyInstance(instance, nullptr);
            if (window) { SDL_DestroyWindow(window); window = nullptr; }
            INPUT.shutdown();
            return 1;
        }
    }

    uint32_t graphics_family = 0, present_family = 0;
    uint32_t compute_family = 0, transfer_family = 0;

    VkDevice device = createLogicalDeviceAndSelectGPU(instance, surface, &graphics_family, &present_family, &compute_family, &transfer_family);

    if (device == VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        if (window) { SDL_DestroyWindow(window); window = nullptr; }
        INPUT.shutdown();
        return 1;
    }

    VkQueue graphics_queue{}, present_queue{}, compute_queue{}, transfer_queue{};
    vkGetDeviceQueue(device, graphics_family,   0, &graphics_queue);
    vkGetDeviceQueue(device, present_family,    0, &present_queue);
    vkGetDeviceQueue(device, compute_family,    0, &compute_queue);
    vkGetDeviceQueue(device, transfer_family,   0, &transfer_queue);

    rtx().instance         = instance;
    rtx().device           = device;
    rtx().surface          = surface;
    rtx().graphics_queue   = graphics_queue;
    rtx().present_queue    = present_queue;
    rtx().compute_queue    = compute_queue;
    rtx().transfer_queue   = transfer_queue;
    rtx().graphics_family  = graphics_family;
    rtx().present_family   = present_family;
    rtx().transfer_family  = transfer_family;
    rtx().compute_family   = compute_family;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = rtx().graphics_family;

    vkCreateCommandPool(device, &poolInfo, nullptr, &rtx().transient_pool);
    LOG_SUCCESS_CAT("VULKAN", "Transient command pool created");

    Swapchain::create(window, Options::SDL3::DefaultWidth, Options::SDL3::DefaultHeight);

    if (Options::SDL3::HeadlessMode) {
        LOG_INFO_CAT("MAIN", "HEADLESS: Swapchain (dummy extent {}x{} — real acquire/present/blit skipped; dispatch path active)", Swapchain::getExtent().width, Swapchain::getExtent().height);
    } else {
        LOG_INFO_CAT("MAIN", "Swapchain ready — {}x{}", Swapchain::getExtent().width, Swapchain::getExtent().height);
    }

    Pipeline::pump_startup_events();
    if (Options::SDL3::RequestQuit) {
        Pipeline::prepare_shutdown();
        finalizeEngineShutdown(window);
        return 0;
    }
    raycanvas = std::make_unique<RayCanvas>(Options::SDL3::DefaultWidth, Options::SDL3::DefaultHeight, window);
    Pipeline::pump_startup_events();
    if (Options::SDL3::RequestQuit || raycanvas->isDestroyed()) {
        Pipeline::prepare_shutdown();
        raycanvas.reset();
        Pipeline::shutdown();
        finalizeEngineShutdown(window);
        return 0;
    }

    if (Options::SDL3::HeadlessMode) {
        LOG_SUCCESS_CAT("MAIN", "RayCanvas created in HEADLESS mode — first dispatch + thermo accountant population guaranteed; status/thermo logs will fire. Max frames guarded inside maybeUpdateCanvas for clean test exit.");
    }

// Main loop — RayCanvas polls SDL (mouse/keyboard); do not drain the queue here.
    while (bool isRunning = true) {
        isRunning = raycanvas->maybeUpdateCanvas(isRunning);

        if (!isRunning || raycanvas->isDestroyed()) {
            if (window) SDL_HideWindow(window);
            LOG_INFO_CAT("MAIN", "Canvas destroyed — exiting main loop");
            break;
        }

        // Thermo heartbeat logs in loop (per task): force visibility of dispatch/accountant in headless + early status
        if (Options::SDL3::HeadlessMode) {
            static int hb = 0;
            if ((++hb % 15 == 0) || (raycanvas->debugFrameCount() < 5)) {
                LOG_DEBUG_CAT("THERMO", "THERMO HEARTBEAT (navigator loop): headless dispatch path active, frames~{} (accountant populated in dispatch_canvas; status/thermo logs forced early)", raycanvas->debugFrameCount());
            }
        }

        if (Options::SDL3::HeadlessMode && (raycanvas->debugFrameCount() % 60 == 0)) {
            // Extra visibility in headless: periodic heartbeat so user sees loop is alive and dispatches happening
            LOG_DEBUG_CAT("MAIN", "HEADLESS heartbeat: loop alive, dispatch path active");
        }

        // Extra guard for MAX_FRAMES clean exit (if RayCanvas didn't set destroyed yet)
        if (Options::SDL3::MaxFrames > 0 && raycanvas->totalFrameCount() >= static_cast<uint64_t>(Options::SDL3::MaxFrames)) {
            LOG_INFO_CAT("MAIN", "MAX_FRAMES bound reached in loop — clean exit");
            break;
        }
    }
// o7

    if (window) SDL_HideWindow(window);
    Pipeline::prepare_shutdown();
    if (Pipeline::hotswap_compile_still_running()) {
        std::fprintf(stderr,
            "[MAIN] Fast exit — x86 pipeline compile in flight (avoids Vk/heap teardown race)\n");
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        std::_Exit(0);
    }
    raycanvas.reset();
    Pipeline::shutdown();
    finalizeEngineShutdown(window);

    LOG_SUCCESS_CAT("MAIN", "Engine shutdown complete");
    return 0;
}