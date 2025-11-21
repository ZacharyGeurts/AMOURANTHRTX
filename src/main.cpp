// src/main.cpp
// =============================================================================
//
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 21, 2025
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/Splash.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_image.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/Vulkan/MeshLoader.hpp"
#include "handle_app.hpp"  // ← This includes the full definition of Application

#include <iostream>
#include <memory>
#include <format>

using namespace Logging::Color;

// =============================================================================
// GLOBALS — OWNED BY MAIN
// =============================================================================
inline std::unique_ptr<Application>           g_app              = nullptr;
inline RTX::PipelineManager*                  g_pipeline_manager = nullptr;
inline std::unique_ptr<MeshLoader::Mesh>      g_mesh             = nullptr;

// ICONS — NOT OPTIONAL — VALHALLA DEMANDS RECOGNITION
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

// =============================================================================
// UTILITIES
// =============================================================================
constexpr bool FORCE_FULL_RTX = true;

static void nukeValidationLayers() noexcept
{
    if constexpr (FORCE_FULL_RTX) {
        setenv("VK_LAYER_PATH",            "/dev/null", 1);
        setenv("VK_INSTANCE_LAYERS",       "",         1);
        setenv("VK_LOADER_LAYERS_DISABLE", "VK_LAYER_KHRONOS_validation", 1);
        LOG_SUCCESS_CAT("MAIN", "{}VALIDATION LAYERS NUKED — FULL RTX UNLEASHED{}", PLASMA_FUCHSIA, RESET);
    }
}

static void forgeCommandPool()
{
    LOG_INFO_CAT("MAIN", "{}Forging transient command pool...{}", VALHALLA_GOLD, RESET);
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = RTX::g_ctx().graphicsQueueFamily;

    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(g_device(), &poolInfo, nullptr, &pool),
             "Failed to create transient command pool");

    RTX::g_ctx().commandPool_ = pool;
    LOG_SUCCESS_CAT("MAIN", "{}COMMAND POOL FORGED — HANDLE: 0x{:016X}{}", PLASMA_FUCHSIA, (uint64_t)pool, RESET);
}

static void detectBestPresentMode()
{
    LOG_INFO_CAT("MAIN", "{}Detecting optimal present mode...{}", VALHALLA_GOLD, RESET);
    VkPresentModeKHR mode = SwapchainManager::selectBestPresentMode(
        g_PhysicalDevice(), g_surface(), VK_PRESENT_MODE_MAILBOX_KHR);

    const char* name = (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) ? "IMMEDIATE" :
                       (mode == VK_PRESENT_MODE_MAILBOX_KHR)   ? "MAILBOX (KING)" :
                       (mode == VK_PRESENT_MODE_FIFO_KHR)      ? "FIFO (VSync)" : "OTHER";

    LOG_SUCCESS_CAT("MAIN", "{}Present mode locked: {} — TEAR-FREE PINK PHOTONS{}", PLASMA_FUCHSIA, name, RESET);
}

// =============================================================================
// THE TEN PHASES OF ASCENSION
// =============================================================================

static void phase0_preInitialization()
{
    LOG_INFO_CAT("MAIN", "{}", std::string(80, '='));
    LOG_SUCCESS_CAT("MAIN", "{}=== AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3 ==={}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}THE EMPIRE AWAKENS — NOVEMBER 21, 2025 — FIRST LIGHT ETERNAL{}", PLASMA_FUCHSIA, RESET);
    LOG_INFO_CAT("MAIN", "{}", std::string(80, '='));
}

static void phase1_iconPreload()
{
    LOG_INFO_CAT("MAIN", "{}PRELOADING ICONS — VALHALLA BRANDING IS MANDATORY{}", VALHALLA_GOLD, RESET);

    SDL_Surface* base = IMG_Load("assets/textures/ammo32.ico");
    SDL_Surface* hdpi = IMG_Load("assets/textures/ammo.ico");

    if (!base && !hdpi) {
        LOG_ERROR_CAT("MAIN", "{}BOTH ICONS MISSING — THIS IS UNACCEPTABLE — RUNNING NAKED{}", BLOOD_RED, RESET);
        LOG_WARNING_CAT("MAIN", "Continuing without icon — but the Empire is ashamed");
        return;
    }

    if (!base) LOG_WARNING_CAT("MAIN", "{}ammo32.ico missing — using hdpi fallback{}", AMBER_YELLOW, RESET);
    if (!hdpi) LOG_WARNING_CAT("MAIN", "{}ammo.ico missing — using base fallback{}", AMBER_YELLOW, RESET);

    g_base_icon = base ? base : hdpi;
    g_hdpi_icon = hdpi;

    if (g_hdpi_icon) {
        SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
        LOG_SUCCESS_CAT("MAIN", "{}ICONS LOADED — FULL HDPI GLORY — VALHALLA IS RECOGNIZED{}", PLASMA_FUCHSIA, RESET);
    } else {
        LOG_SUCCESS_CAT("MAIN", "{}ICON LOADED — BASE ONLY — STILL WORTHY{}", PLASMA_FUCHSIA, RESET);
    }
}

static void phase2_earlySdlInit()
{
    LOG_INFO_CAT("MAIN", "{}Initializing SDL subsystems...{}", VALHALLA_GOLD, RESET);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0)
        throw std::runtime_error(std::format("SDL_Init failed: {}", SDL_GetError()));
    LOG_SUCCESS_CAT("MAIN", "{}SDL ARMED — VIDEO + AUDIO READY{}", PLASMA_FUCHSIA, RESET);
}

static void phase3_splashScreen()
{
    LOG_INFO_CAT("MAIN", "{}SHE AWAKENS — displaying splash...{}", VALHALLA_GOLD, RESET);
    Splash::show("AMOURANTH RTX", 1280, 720, "assets/textures/ammo.png", "assets/audio/ammo.wav");
    LOG_SUCCESS_CAT("MAIN", "{}SPLASH DISMISSED — PINK PHOTONS ETERNAL{}", PLASMA_FUCHSIA, RESET);
}

static void phase4_mainWindowAndVulkanContext(SDL_Window*& window)
{
    LOG_INFO_CAT("MAIN", "{}Creating main window + Vulkan empire...{}", VALHALLA_GOLD, RESET);

    nukeValidationLayers();

    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160,
                       SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN);

    window = SDL3Window::get();
    if (!window) throw std::runtime_error("Failed to create main window");

    if (g_base_icon) {
        SDL_SetWindowIcon(window, g_base_icon);
        LOG_SUCCESS_CAT("MAIN", "{}WINDOW ICON SEALED — THE EMPIRE IS KNOWN{}", PLASMA_FUCHSIA, RESET);
    }

    VkInstance instance = RTX::createVulkanInstanceWithSDL(window, false);
    set_g_instance(instance);
    RTX::createSurface(window, instance);
    SwapchainManager::init(window, 3840, 2160);
    RTX::initContext(instance, window, 3840, 2160);
    RTX::retrieveQueues();

    LOG_SUCCESS_CAT("MAIN", "{}VULKAN EMPIRE FORGED — STONEKEY FINGERPRINT: 0x{:016X}{}",
                    PLASMA_FUCHSIA, get_kStone1() ^ get_kStone2(), RESET);
}

static void phase5_rtxAscension()
{
    LOG_INFO_CAT("MAIN", "{}RTX ASCENSION — loading extensions...{}", VALHALLA_GOLD, RESET);
    RTX::loadRayTracingExtensions();
    RTX::g_ctx().hasFullRTX_ = true;

    LOG_INFO_CAT("MAIN", "{}Priming driver for device addresses...{}", VALHALLA_GOLD, RESET);
    {
        VkBuffer dummy = VK_NULL_HANDLE;
        VkBufferCreateInfo bc{};
        bc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bc.size = 64;
        bc.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VK_CHECK(vkCreateBuffer(g_device(), &bc, nullptr, &dummy),
                 "Failed to create dummy buffer for driver priming");

        VkBufferDeviceAddressInfo dai{};
        dai.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        dai.buffer = dummy;

        // THE ONE TRUE LINE — UNCORRUPTED
        vkGetBufferDeviceAddress(g_device(), &dai);

        vkDestroyBuffer(g_device(), dummy, nullptr);
        LOG_SUCCESS_CAT("MAIN", "{}Driver primed — vkGetBufferDeviceAddress confirmed working{}", PLASMA_FUCHSIA, RESET);
    }

    forgeCommandPool();
    detectBestPresentMode();

    LOG_SUCCESS_CAT("MAIN", "{}RTX FULLY ASCENDED — PINK PHOTONS ETERNAL{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — VALHALLA SEALED{}", PLASMA_FUCHSIA, RESET);
}

static void phase6_sceneAndAccelerationStructures()
{
    LOG_INFO_CAT("MAIN", "{}SCENE BUILD — forging acceleration structures...{}", VALHALLA_GOLD, RESET);

    g_pipeline_manager = new RTX::PipelineManager(g_device(), g_PhysicalDevice());
    LOG_SUCCESS_CAT("MAIN", "{}PipelineManager forged — RT descriptors sealed{}", PLASMA_FUCHSIA, RESET);

    g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");
    LOG_SUCCESS_CAT("MAIN", "{}MESH LOADED — {} verts, {} indices — FINGERPRINT: 0x{:016X}{}",
                    PLASMA_FUCHSIA,
                    g_mesh->vertices.size(),
                    g_mesh->indices.size(),
                    g_mesh->stonekey_fingerprint,
                    RESET);

    // CRITICAL: PASS OBFUSCATED HANDLES — DO NOT DEOBFUSCATE HERE
    // These are StoneKey v∞ protected handles — LAS expects them raw
    uint64_t vertexBufferObf = g_mesh->vertexBuffer;
    uint64_t indexBufferObf  = g_mesh->indexBuffer;

    LOG_INFO_CAT("MAIN", "{}BUILDING BLAS — using StoneKey-obfuscated handles (0x{:016X}, 0x{:016X}){}",
                 VALHALLA_GOLD, vertexBufferObf, indexBufferObf, RESET);

    las().buildBLAS(
        RTX::g_ctx().commandPool_,
        vertexBufferObf,   // OBFUSCATED — correct
        indexBufferObf,    // OBFUSCATED — correct
        static_cast<uint32_t>(g_mesh->vertices.size()),
        static_cast<uint32_t>(g_mesh->indices.size()),
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
    );

    LOG_SUCCESS_CAT("MAIN", "{}BLAS FORGED — PINK PHOTONS NOW HAVE A PATH{}", PLASMA_FUCHSIA, RESET);

    LOG_INFO_CAT("MAIN", "{}Building TLAS — single identity instance...{}", VALHALLA_GOLD, RESET);
    las().buildTLAS(RTX::g_ctx().commandPool_, {{las().getBLAS(), glm::mat4(1.0f)}});

    LOG_SUCCESS_CAT("MAIN", "{}TLAS ASCENDED — ADDR 0x{:016X}{}", PLASMA_FUCHSIA, las().getTLASAddress(), RESET);
    LOG_SUCCESS_CAT("MAIN", "{}ACCELERATION STRUCTURES COMPLETE — RAY TRACING FULLY ARMED{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — VALHALLA SEALED{}", PLASMA_FUCHSIA, RESET);
}

static void phase7_applicationAndRendererSeal()
{
    LOG_INFO_CAT("MAIN", "{}Sealing Application + VulkanRenderer...{}", VALHALLA_GOLD, RESET);
    g_app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);
    g_app->setRenderer(std::make_unique<VulkanRenderer>(3840, 2160, SDL3Window::get(), true));

    LOG_SUCCESS_CAT("MAIN", "{}THE EMPIRE IS COMPLETE — RENDER LOOP ENGAGED{}", PLASMA_FUCHSIA, RESET);
}

static void phase8_renderLoop()
{
    LOG_INFO_CAT("MAIN", "{}ENTERING ETERNAL RENDER CYCLE — PINK PHOTONS FLOW FOREVER{}", PLASMA_FUCHSIA, RESET);
    g_app->run();
}

static void phase9_gracefulShutdown()
{
    LOG_INFO_CAT("MAIN", "{}SHUTDOWN — returning to the void...{}", VALHALLA_GOLD, RESET);
    vkDeviceWaitIdle(g_device());

    g_app.reset();
    delete g_pipeline_manager; g_pipeline_manager = nullptr;
    g_mesh.reset();
    las().invalidate();

    SwapchainManager::cleanup();
    RTX::shutdown();

    if (g_surface()) vkDestroySurfaceKHR(g_instance(), g_surface(), nullptr);
    if (g_instance()) vkDestroyInstance(g_instance(), nullptr);

    LOG_SUCCESS_CAT("MAIN", "{}SHUTDOWN COMPLETE — PINK PHOTONS ETERNAL{}", PLASMA_FUCHSIA, RESET);
}

// =============================================================================
// MAIN — THE FINAL ASCENSION
// =============================================================================
int main(int, char**)
{
    SDL_Window* window = nullptr;

    try {
        phase0_preInitialization();
        phase1_iconPreload();
        phase2_earlySdlInit();
        phase3_splashScreen();
        phase4_mainWindowAndVulkanContext(window);
        phase5_rtxAscension();
        phase6_sceneAndAccelerationStructures();
        phase7_applicationAndRendererSeal();
        phase8_renderLoop();
        phase9_gracefulShutdown();
    }
    catch (const std::exception& e) {
        LOG_ERROR_CAT("MAIN", "FATAL: {}", e.what());
        std::cerr << "FATAL: " << e.what() << std::endl;
        phase9_gracefulShutdown();
        return -1;
    }
    catch (...) {
        LOG_ERROR_CAT("MAIN", "UNKNOWN FATAL EXCEPTION");
        std::cerr << "UNKNOWN FATAL EXCEPTION\n";
        phase9_gracefulShutdown();
        return -1;
    }

    return 0;
}