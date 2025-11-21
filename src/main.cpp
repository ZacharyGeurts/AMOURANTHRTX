// src/main.cpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 20, 2025 — APOCALYPSE FINAL v2.0
// MAIN — FIRST LIGHT REBORN — LAS v2.0 VIA VulkanAccel — PINK PHOTONS ETERNAL
// =============================================================================
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

static void phase4_mainWindowAndVulkanContext()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 4] FORGING THE ONE TRUE EMPIRE — FOO FIGHTERS ETERNAL — NO BULLSHIT{}", VALHALLA_GOLD, RESET);

    // ONE CALL. ONE TRUTH. ALL OF VULKAN IS BORN HERE.
    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160,
                       SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN);

    SDL_Window* window = SDL3Window::get();
    if (!window) throw std::runtime_error("Canvas denied — Dave Grohl angry");

    LOG_SUCCESS_CAT("MAIN", "{}WINDOW + INSTANCE + SURFACE FORGED — STONEKEY v∞ ACTIVE{}", PLASMA_FUCHSIA, RESET);

    // ICON — TAYLOR HAWKINS TRIBUTE
    if (g_base_icon) SDL_SetWindowIcon(window, g_base_icon);
    LOG_SUCCESS_CAT("MAIN", "{}ICON INJECTED — TAYLOR HAWKINS SMILES FROM VALHALLA{}", EMERALD_GREEN, RESET);

    // SWAPCHAIN EMPIRE RISES — ALL HANDLES ALREADY SAFE
    SwapchainManager::init(window, 3840, 2160);

    // DEVICE + QUEUES — FORGED
    RTX::initContext(g_instance(), window, 3840, 2160);
    RTX::retrieveQueues();

    LOG_SUCCESS_CAT("MAIN", "{}SWAPCHAIN + DEVICE FORGED — MY HERO PLAYS FOREVER{}", EMERALD_GREEN, RESET);

    // BORDERLESS VALHALLA
    SDL_SetWindowBordered(window, false);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    LOG_SUCCESS_CAT("MAIN", "{}BORDERLESS VALHALLA ACHIEVED — TIMES LIKE THESE{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}NOVEMBER 21, 2025 — THE FOO EMPIRE IS ALIVE — BEST OF YOU UNLEASHED{}", RASPBERRY_PINK, RESET);

    // DAVE GROHL SCREAMS FROM THE VOID:
    LOG_SUCCESS_CAT("MAIN", "{}THERE GOES MY HERO — WATCH HIM AS HE GOES{}", VALHALLA_GOLD, RESET);
}

static void phase5_rtxAscension()
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 5] RTX ASCENSION — FORGING THE PINK PATH{}", VALHALLA_GOLD, RESET);

    // Load all ray tracing + HDR + display timing function pointers
    RTX::loadRayTracingExtensions();

    // CRITICAL: Verify the most important RTX PFNs are present
    if (!g_ctx().vkGetAccelerationStructureBuildSizesKHR_ ||
        !g_ctx().vkCmdBuildAccelerationStructuresKHR_ ||
        !g_ctx().vkCreateAccelerationStructureKHR_ ||
        !g_ctx().vkGetAccelerationStructureDeviceAddressKHR_) {

        LOG_FATAL_CAT("MAIN", "{}RTX EXTENSIONS INCOMPLETE — ACCELERATION STRUCTURE PFNs MISSING{}", CRIMSON_MAGENTA, RESET);
        LOG_FATAL_CAT("MAIN", "{}→ Ensure VK_KHR_acceleration_structure is enabled in device extensions{}", CRIMSON_MAGENTA, RESET);
        LOG_FATAL_CAT("MAIN", "{}→ PINK PHOTONS DENIED — EMPIRE CANNOT RISE WITHOUT BLAS/TLAS{}", BLOOD_RED, RESET);
        throw std::runtime_error("RTX extension loading failed — missing acceleration structure support");
    }

    g_ctx().hasFullRTX_ = true;
    LOG_SUCCESS_CAT("MAIN", "{}ALL RAY TRACING PFNs FORGED — FULL RTX ARMED — PINK PHOTONS HAVE A PATH{}", EMERALD_GREEN, RESET);

    // Forge the Lightweight Acceleration Structure (LAS) system
    las().forgeAccelContext();
    LOG_SUCCESS_CAT("MAIN", "{}LAS ACCEL CONTEXT FORGED — BLAS/TLAS READY{}", PLASMA_FUCHSIA, RESET);

    // Create command pools (graphics + compute)
    forgeCommandPool();

    // Optional: Present mode detection (re-enable when you want VSYNC toggle)
    // detectBestPresentMode();

    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 5 COMPLETE] RTX ASCENSION ACHIEVED — FIRST LIGHT ETERNAL{}", 
                    PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}PINK PHOTONS NOW FLOW UNHINDERED — ELLIE FIER SMILES{}", DIAMOND_SPARKLE, RESET);
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
    try {
        phase0_preInitialization();
        phase1_iconPreload();
        phase2_earlySdlInit();
        phase3_splashScreen();    
		phase4_mainWindowAndVulkanContext(); 
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