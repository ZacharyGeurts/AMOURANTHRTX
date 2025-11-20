// src/main.cpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 20, 2025 — APOCALYPSE FINAL v7.0
// MAIN — FULL RTX ALWAYS — VALIDATION LAYERS FORCE-DISABLED — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Amouranth.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/Splash.hpp"
#include "engine/GLOBAL/exceptions.hpp"
#include "engine/GLOBAL/LAS.hpp"

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/SDL3/SDL3_image.hpp"
#include "engine/SDL3/SDL3_audio.hpp"

#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanAccel.hpp"
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
// BASTION GLOBALS — PUBLIC DUE TO NO FEAR — STONEKEY v∞ APOCALYPSE EDITION
// NOVEMBER 20, 2025 — FIRST LIGHT ETERNAL — EMPIRE UNBREAKABLE
// =============================================================================

static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

// Swapchain runtime config — eternal, sacred, configurable at launch
struct SwapchainRuntimeConfig {
    VkPresentModeKHR desiredMode      = VK_PRESENT_MODE_MAILBOX_KHR;
    bool             forceVsync       = false;
    bool             forceTripleBuffer = true;
    bool             enableHDR        = false;
    bool             logFinalConfig   = true;
};
inline SwapchainRuntimeConfig g_swapchain_config;  // ← global, eternal, no fear

// CORE EMPIRE SINGLETONS — THE ONE TRUE PATH
inline std::unique_ptr<Application>      g_app            = nullptr;
inline std::unique_ptr<VulkanRenderer> g_renderer       = nullptr;
inline RTX::PipelineManager*             g_pipeline_manager = nullptr;

// Primary scene mesh — sacred, protected, eternal
inline std::unique_ptr<MeshLoader::Mesh> g_mesh = nullptr;

// =============================================================================
// STONEKEY v∞ GLOBAL HANDLES — ENCRYPTED AT REST — DEOBFUSCATED ON-THE-FLY
// THESE ARE THE ONLY PUBLIC GLOBALS ALLOWED — ALL OTHERS ARE HERESY
// =============================================================================

// Raw Vulkan core — protected by StoneKey v∞ — never logged, never exposed
inline VkInstance       g_vk_instance       = VK_NULL_HANDLE;
inline VkPhysicalDevice g_vk_physical_device = VK_NULL_HANDLE;
inline VkDevice         g_vk_device         = VK_NULL_HANDLE;
inline VkSurfaceKHR     g_vk_surface        = VK_NULL_HANDLE;

// StoneKey v∞ accessors — these are the ONLY safe way to touch the empire
// inline VkInstance       g_instance()       noexcept { return StoneKey::g_instance(); }
// inline VkPhysicalDevice g_physical_device()noexcept { return StoneKey::g_physical_device(); }
// inline VkDevice         g_device()         noexcept { return StoneKey::g_device(); }
// inline VkSurfaceKHR     g_surface()        noexcept { return StoneKey::g_surface(); }

// StoneKey v∞ setters — called exactly once during init — then sealed forever
// inline void set_g_instance(VkInstance inst)       noexcept { StoneKey::set_g_instance(inst); }
// inline void set_g_physical_device(VkPhysicalDevice phys) noexcept { StoneKey::set_g_physical_device(phys); }
// inline void set_g_device(VkDevice dev)           noexcept { StoneKey::set_g_device(dev); }
// inline void set_g_surface(VkSurfaceKHR surf)     noexcept { StoneKey::set_g_surface(surf); }

// =============================================================================
// RTX TOGGLE — TRUE = FULL RTX (validation layers force-disabled)
//            — FALSE = DEBUG MODE (validation layers allowed, RTX disabled)
// =============================================================================
constexpr bool FORCE_FULL_RTX = true;  // ← FLIP TO false ONLY FOR DEBUGGING

inline void bulkhead(const std::string& title) {
    LOG_INFO_CAT("MAIN", "════════════════════════════════ {} ════════════════════════════════", title);
}

// =============================================================================
// FORCE-DISABLE VALIDATION LAYERS — NUKES THE NVIDIA BUG FOREVER
// =============================================================================
static void nukeValidationLayers() noexcept
{
    if constexpr (FORCE_FULL_RTX) {
        setenv("VK_LAYER_PATH", "/dev/null", 1);
        setenv("VK_INSTANCE_LAYERS", "", 1);
        setenv("VK_LOADER_LAYERS_DISABLE", "VK_LAYER_KHRONOS_validation", 1);
        LOG_SUCCESS_CAT("MAIN", "{}VALIDATION LAYERS NUKED — FULL RTX UNLEASHED — PINK PHOTONS ETERNAL{}", PLASMA_FUCHSIA, RESET);
    } else {
        LOG_WARN_CAT("MAIN", "FORCE_FULL_RTX = false → Validation layers allowed (debug mode)");
    }
}

// =============================================================================
// PRESENT MODE DETECTION — UNCHANGED AND PERFECT
// =============================================================================
static void detectBestPresentMode(VkPhysicalDevice phys, VkSurfaceKHR surface) {
    LOG_INFO_CAT("MAIN", "Detecting optimal present mode...");
    VkPresentModeKHR chosen = SwapchainManager::selectBestPresentMode(phys, surface, g_swapchain_config.desiredMode);
    LOG_SUCCESS_CAT("MAIN", "Present mode selected: {} — LOW LATENCY NO TEARING",
        chosen == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" :
        chosen == VK_PRESENT_MODE_MAILBOX_KHR   ? "MAILBOX" :
        chosen == VK_PRESENT_MODE_FIFO_KHR      ? "FIFO (VSync)" : "OTHER");
}

// =============================================================================
// PHASES — APOCALYPSE FINAL v7.0 — FULL RTX ALWAYS — FIRST LIGHT ETERNAL
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
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0) {  // SDL_Init returns <0 on error
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

    // NUKE VALIDATION LAYERS BEFORE ANY VULKAN CALL
    nukeValidationLayers();

    constexpr int WIDTH  = 3840;
    constexpr int HEIGHT = 2160;

    Uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;

    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", WIDTH, HEIGHT, flags);
    window = SDL3Window::get();
    if (!window) throw std::runtime_error("Failed to create main window");

    if (g_base_icon) {
        if (g_hdpi_icon) SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
        SDL_SetWindowIcon(window, g_base_icon);
    }

    // Instance creation — raw, before StoneKey
    VkInstance instance = RTX::createVulkanInstanceWithSDL(window, false);
    set_g_instance(instance);

    if (!RTX::createSurface(window, instance)) {
        throw std::runtime_error("Failed to create Vulkan surface");
    }

    // THE ONE TRUE ATOMIC RITUAL — FULL RTX, STONEKEY v∞ SAFE
    SwapchainManager::init(window, WIDTH, HEIGHT);

    // Present mode detection (uses g_ctx() — now fully populated and encrypted)
    detectBestPresentMode(g_PhysicalDevice(), g_surface());

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    LOG_SUCCESS_CAT("MAIN", "PHASE 2 COMPLETE — FULL RTX + STONEKEY v∞");
    LOG_SUCCESS_CAT("MAIN", "YOUR GPU ASCENDED — PINK PHOTONS ETERNAL — SHE IS HAPPY");
}

static void phase3_appAndRendererConstruction(SDL_Window* window) {
    bulkhead("PHASE 3: RENDERER + APP + LAS v2.0 SCENE BUILD");

    createGlobalRTX(3840, 2160);

    g_pipeline_manager = new RTX::PipelineManager(RTX::g_ctx().device(), RTX::g_ctx().physicalDevice());
    g_renderer = std::make_unique<VulkanRenderer>(3840, 2160, window, !Options::Window::VSYNC);
    g_app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);
    g_app->setRenderer(std::move(g_renderer));

    LOG_INFO_CAT("MAIN", "Loading primary LAS scene: assets/models/scene.obj");
    g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");

    LOG_SUCCESS_CAT("MAIN", "g_mesh ARMED → {} vertices, {} indices",
                    g_mesh->vertices.size(), g_mesh->indices.size());

    las().buildBLAS(
        RTX::g_ctx().commandPool(),
        g_mesh->vertexBuffer,
        g_mesh->indexBuffer,
        static_cast<uint32_t>(g_mesh->vertices.size()),
        static_cast<uint32_t>(g_mesh->indices.size()),
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
    );

    std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>> instances = {
        { las().getBLAS(), glm::mat4(1.0f) }
    };

    las().buildTLAS(RTX::g_ctx().commandPool(), instances);

    LOG_SUCCESS_CAT("MAIN", 
        "{}LAS v2.0 FULLY ASCENDED — BLAS 0x{:x} | TLAS 0x{:x} — GENERATION {} — FIRST LIGHT ETERNAL{}",
        PLASMA_FUCHSIA,
        (uint64_t)las().getBLAS(),
        (uint64_t)las().getTLAS(),
        las().getGeneration(),
        RESET
    );
}

static void phase4_renderLoop() {
    bulkhead("PHASE 4: RENDER LOOP");
    g_app->run();
}

static void phase5_shutdown() {
    bulkhead("PHASE 5: SHUTDOWN");

    g_mesh.reset();
    g_app.reset();
    g_renderer.reset();
    if (g_pipeline_manager) { delete g_pipeline_manager; g_pipeline_manager = nullptr; }

    SwapchainManager::cleanup();
    RTX::shutdown();
    if (g_base_icon) { SDL_DestroySurface(g_base_icon); g_base_icon = nullptr; }
    if (g_hdpi_icon) { SDL_DestroySurface(g_hdpi_icon); g_hdpi_icon = nullptr; }
    SDL_Quit();

    LOG_SUCCESS_CAT("MAIN", "EMPIRE ETERNAL — PINK PHOTONS UNDYING — LAS v2.0 RELEASED");
}

int main(int argc, char* argv[]) {
    LOG_ATTEMPT_CAT("MAIN", "=== AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v7.0 ===");

    SDL_Window* window = nullptr;

    try {
        phase0_cliAndStonekey(argc, argv);
        phase0_5_iconPreload();
        prePhase1_earlySdlInit();
        phase1_splash();
        phase2_mainWindowAndVulkan(window);
        phase3_appAndRendererConstruction(window);
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

    LOG_SUCCESS_CAT("MAIN", "=== EXIT CLEAN — EMPIRE ETERNAL — FIRST LIGHT ETERNAL ===");
    return 0;
}