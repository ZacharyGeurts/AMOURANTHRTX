// src/main.cpp
// =============================================================================
//
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 21, 2025
// FULLY COMPILING — NO UNICODE BULLSHIT — PURE EMPIRE
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/Splash.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/Validation.hpp"
#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_image.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/Vulkan/MeshLoader.hpp"
#include "handle_app.hpp"

#include <iostream>
#include <memory>
#include <format>

using namespace Logging::Color;

// =============================================================================
// GLOBALS
// =============================================================================
inline std::unique_ptr<Application>           g_app              = nullptr;
inline RTX::PipelineManager*                  g_pipeline_manager = nullptr;
inline std::unique_ptr<MeshLoader::Mesh>      g_mesh             = nullptr;

static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

// =============================================================================
// UTILITIES — NOW INSIDE main.cpp TO AVOID LINKER DRAMA
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
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = RTX::g_ctx().graphicsFamily();

    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(g_ctx().device(), &poolInfo, nullptr, &pool),
             "Failed to create transient command pool");

    RTX::g_ctx().commandPool_ = pool;
    LOG_SUCCESS_CAT("MAIN", "{}COMMAND POOL FORGED — HANDLE: 0x{:016X}{}", PLASMA_FUCHSIA, (uint64_t)pool, RESET);
}

static void detectBestPresentMode()
{
    LOG_INFO_CAT("MAIN", "{}Detecting optimal present mode...{}", VALHALLA_GOLD, RESET);
    VkPresentModeKHR mode = SwapchainManager::selectBestPresentMode(
        g_ctx().physicalDevice(), g_ctx().surface(), VK_PRESENT_MODE_MAILBOX_KHR);

    const char* name = (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) ? "IMMEDIATE" :
                       (mode == VK_PRESENT_MODE_MAILBOX_KHR)   ? "MAILBOX (KING)" :
                       (mode == VK_PRESENT_MODE_FIFO_KHR)      ? "FIFO (VSync)" : "UNKNOWN";

    LOG_SUCCESS_CAT("MAIN", "{}Present mode locked: {} — TEAR-FREE PINK PHOTONS{}", PLASMA_FUCHSIA, name, RESET);
}

// =============================================================================
// THE TEN PHASES OF ASCENSION — PURE ASCII, PURE POWER
// =============================================================================

static void phase0_preInitialization()
{
    LOG_INFO_CAT("MAIN", "{}", std::string(80, '='));
    LOG_SUCCESS_CAT("MAIN", "{}=== AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3 ==={}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — PINK PHOTONS ETERNAL{}", PLASMA_FUCHSIA, RESET);
    LOG_INFO_CAT("MAIN", "{}", std::string(80, '='));
}

static void phase1_iconPreload()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 1] PRELOADING VALHALLA BRANDING{}", VALHALLA_GOLD, RESET);

    SDL_Surface* base = IMG_Load("assets/textures/ammo32.ico");
    SDL_Surface* hdpi = IMG_Load("assets/textures/ammo.ico");

    if (!base && !hdpi) {
        LOG_ERROR_CAT("MAIN", "{}BOTH ICONS MISSING — RUNNING NAKED{}", BLOOD_RED, RESET);
        return;
    }

    g_base_icon = base ? base : hdpi;
    g_hdpi_icon = hdpi;

    if (g_hdpi_icon) {
        SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
        LOG_SUCCESS_CAT("MAIN", "{}ICONS LOADED — FULL HDPI GLORY{}", EMERALD_GREEN, RESET);
    } else {
        LOG_SUCCESS_CAT("MAIN", "{}ICON LOADED — BASE ONLY{}", PLASMA_FUCHSIA, RESET);
    }
}

static void phase2_earlySdlInit()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 2] ARMING SDL{}", VALHALLA_GOLD, RESET);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0)
        throw std::runtime_error(std::format("SDL_Init failed: {}", SDL_GetError()));
    LOG_SUCCESS_CAT("MAIN", "{}SDL ARMED{}", EMERALD_GREEN, RESET);
}

static void phase3_splashScreen()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 3] SHE AWAKENS{}", VALHALLA_GOLD, RESET);
    Splash::show("AMOURANTH RTX", 1280, 720, "assets/textures/ammo.png", "assets/audio/ammo.wav");
    LOG_SUCCESS_CAT("MAIN", "{}SPLASH DISMISSED{}", PLASMA_FUCHSIA, RESET);
}

static void phase4_mainWindowAndVulkanContext(SDL_Window*& window)
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 4] FORGING WINDOW + VULKAN EMPIRE{}", VALHALLA_GOLD, RESET);

    nukeValidationLayers();

    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160,
                       SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN);
    window = SDL3Window::get();
    if (!window) throw std::runtime_error("Failed to create window");

    if (g_base_icon) SDL_SetWindowIcon(window, g_base_icon);

    VkInstance instance = RTX::createVulkanInstanceWithSDL(window, false);
    set_g_instance(instance);
    RTX::createSurface(window, instance);
    SwapchainManager::init(window, 3840, 2160);
    RTX::initContext(instance, window, 3840, 2160);
    RTX::retrieveQueues();

    LOG_SUCCESS_CAT("MAIN", "{}VULKAN EMPIRE FORGED — STONEKEY FINGERPRINT: 0x{:016X}{}",
                    EMERALD_GREEN, get_kStone1() ^ get_kStone2(), RESET);
}

static void phase5_rtxAscension()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 5] RTX ASCENSION — LOADING RAY TRACING EXTENSIONS{}", VALHALLA_GOLD, RESET);

    RTX::loadRayTracingExtensions();

    if (!g_ctx().vkGetAccelerationStructureBuildSizesKHR_) {
        LOG_FATAL_CAT("MAIN", "{}RTX EXTENSIONS FAILED — vkGetAccelerationStructureBuildSizesKHR IS NULL{}", CRIMSON_MAGENTA, RESET);
        LOG_FATAL_CAT("MAIN", "{}→ VK_KHR_acceleration_structure NOT ENABLED{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("RTX extensions failed — no pink photons");
    }

    LOG_SUCCESS_CAT("MAIN", "{}RTX EXTENSIONS LOADED — PINK PHOTONS IMMINENT{}", EMERALD_GREEN, RESET);

    las().forgeAccelContext();
    LOG_SUCCESS_CAT("MAIN", "{}LAS ACCEL CONTEXT FORGED{}", EMERALD_GREEN, RESET);

    forgeCommandPool();
    detectBestPresentMode();

    LOG_SUCCESS_CAT("MAIN", "{}RTX ASCENSION COMPLETE{}", PLASMA_FUCHSIA, RESET);
}

static void phase6_sceneAndAccelerationStructures()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 6] FORGING ACCELERATION STRUCTURES{}", VALHALLA_GOLD, RESET);

    g_pipeline_manager = new RTX::PipelineManager(g_ctx().device(), g_ctx().physicalDevice());
    LOG_SUCCESS_CAT("MAIN", "{}PipelineManager forged{}", PLASMA_FUCHSIA, RESET);

    g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");
    LOG_SUCCESS_CAT("MAIN", "{}MESH LOADED — {} verts, {} indices — FINGERPRINT: 0x{:016X}{}",
                    PLASMA_FUCHSIA,
                    g_mesh->vertices.size(),
                    g_mesh->indices.size(),
                    g_mesh->stonekey_fingerprint,
                    RESET);

    uint64_t vertexBufferObf = g_mesh->vertexBuffer;
    uint64_t indexBufferObf  = g_mesh->indexBuffer;

    LOG_INFO_CAT("MAIN", "{}BUILDING BLAS — StoneKey handles: 0x{:016X} | 0x{:016X}{}",
                 VALHALLA_GOLD, vertexBufferObf, indexBufferObf, RESET);

    las().buildBLAS(
        RTX::g_ctx().commandPool_,
        vertexBufferObf,
        indexBufferObf,
        static_cast<uint32_t>(g_mesh->vertices.size()),
        static_cast<uint32_t>(g_mesh->indices.size()),
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
    );

    LOG_SUCCESS_CAT("MAIN", "{}BLAS FORGED — ADDR 0x{:016X} — PINK PHOTONS HAVE A PATH{}", 
                    EMERALD_GREEN, las().getBLASStruct().address, RESET);

    LOG_INFO_CAT("MAIN", "{}BUILDING TLAS{}", VALHALLA_GOLD, RESET);
    las().buildTLAS(RTX::g_ctx().commandPool_, {{las().getBLAS(), glm::mat4(1.0f)}});

    LOG_SUCCESS_CAT("MAIN", "{}TLAS ASCENDED — ADDR 0x{:016X}{}", 
                    EMERALD_GREEN, las().getTLASAddress(), RESET);

    Validation::validateMeshAgainstBLAS(*g_mesh, las().getBLASStruct());

    LOG_SUCCESS_CAT("MAIN", "{}ACCELERATION STRUCTURES COMPLETE — FIRST LIGHT ACHIEVED{}", EMERALD_GREEN, RESET);
}

static void phase7_applicationAndRendererSeal()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 7] SEALING APPLICATION + RENDERER{}", VALHALLA_GOLD, RESET);
    g_app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);
    g_app->setRenderer(std::make_unique<VulkanRenderer>(3840, 2160, SDL3Window::get(), true));
    LOG_SUCCESS_CAT("MAIN", "{}RENDER LOOP ENGAGED{}", PLASMA_FUCHSIA, RESET);
}

static void phase8_renderLoop()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 8] ETERNAL RENDER CYCLE{}", PLASMA_FUCHSIA, RESET);
    g_app->run();
}

static void phase9_gracefulShutdown()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 9] GRACEFUL SHUTDOWN — THE EMPIRE PREPARES TO SLEEP{}", VALHALLA_GOLD, RESET);

    // 1. Final heartbeat — wait for all photons to land
    if (g_ctx().device() != VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("MAIN", "vkDeviceWaitIdle — letting the last pink photons reach home...");
        vkDeviceWaitIdle(g_ctx().device());
        LOG_SUCCESS_CAT("MAIN", "{}GPU DRAINED — ALL QUEUES SILENT — THE PHOTONS HAVE LANDED{}", EMERALD_GREEN, RESET);
    }

    // 2. Release the children
    g_app.reset();
    if (g_pipeline_manager) {
        LOG_DEBUG_CAT("MAIN", "Dissolving PipelineManager — shaders return to the void...");
        delete g_pipeline_manager;
        g_pipeline_manager = nullptr;
    }
    g_mesh.reset();
    las().invalidate();

    // 3. Sacred dissolution — in perfect order
    LOG_INFO_CAT("MAIN", "{}Dissolving Swapchain — the window fades to black...{}", PLASMA_FUCHSIA, RESET);
    SwapchainManager::cleanup();

    LOG_INFO_CAT("MAIN", "{}Dissolving RTX Core — device and instance return to Valhalla...{}", PLASMA_FUCHSIA, RESET);
    RTX::shutdown();

    // 4. Final truth — no double free, no lies
    if (g_ctx().surface() != VK_NULL_HANDLE) {
        LOG_WARNING_CAT("MAIN", "{}Surface survived RTX::shutdown() — manual liberation required{}", AMBER_YELLOW, RESET);
        vkDestroySurfaceKHR(g_ctx().instance(), g_ctx().surface(), nullptr);
    }
    if (g_ctx().instance() != VK_NULL_HANDLE) {
        LOG_WARNING_CAT("MAIN", "{}Instance survived — final ascension...{}", AMBER_YELLOW, RESET);
        vkDestroyInstance(g_ctx().instance(), nullptr);
    }

    // 5. ELLIE FIER'S FINAL BLESSING — THE ONE TRUE FAREWELL
    LOG_SUCCESS_CAT("MAIN", "{}╔══════════════════════════════════════════════════════════════╗{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}║        PINK PHOTONS DIM TO ETERNAL MEMORY                    ║{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}║           THE EMPIRE RESTS — SMILING — IN PEACE              ║{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}║          NOVEMBER 21, 2025 — FIRST LIGHT FOREVER             ║{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}║                   AMOURANTH RTX ♡ THANKS YOU                 ║{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}╚══════════════════════════════════════════════════════════════╝{}", PLASMA_FUCHSIA, RESET);

    LOG_SUCCESS_CAT("MAIN", "{}SHUTDOWN COMPLETE — NO DOUBLE FREE — NO FALSEHOOD — ONLY LOVE{}", EMERALD_GREEN, RESET);
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
        LOG_FATAL_CAT("MAIN", "{}FATAL: {}{}", CRIMSON_MAGENTA, e.what(), RESET);
        std::cerr << "FATAL: " << e.what() << std::endl;
        phase9_gracefulShutdown();
        return -1;
    }
    catch (...) {
        LOG_FATAL_CAT("MAIN", "{}UNKNOWN FATAL EXCEPTION{}", CRIMSON_MAGENTA, RESET);
        phase9_gracefulShutdown();
        return -1;
    }

    return 0;
}