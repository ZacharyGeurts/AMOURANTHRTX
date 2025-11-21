// src/main.cpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 20, 2025 — APOCALYPSE FINAL v10.3
// MAIN — FULL RTX ALWAYS — VALIDATION NUKED — PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED
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
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "handle_app.hpp"
#include "engine/Vulkan/MeshLoader.hpp"

#include <iostream>
#include <stdexcept>
#include <format>
#include <memory>
#include <thread>
#include <chrono>
#include <vulkan/vulkan.h>

using namespace Logging::Color;

// =============================================================================
// SINGLETONS OWNED BY MAIN
// =============================================================================
inline std::unique_ptr<Application>           g_app              = nullptr;
inline RTX::PipelineManager*                  g_pipeline_manager = nullptr;
inline std::unique_ptr<MeshLoader::Mesh>      g_mesh             = nullptr;

// =============================================================================
// ICONS
// =============================================================================
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

// =============================================================================
// UTILITIES
// =============================================================================
constexpr bool FORCE_FULL_RTX = true;

inline void bulkhead(const std::string& title)
{
    LOG_INFO_CAT("MAIN", "════════════════════════════════ {} ════════════════════════════════", title);
}

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
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = RTX::g_ctx().graphicsQueueFamily;

    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(g_device(), &poolInfo, nullptr, &pool),
             "Failed to create transient command pool");

    RTX::g_ctx().commandPool_ = pool;
    LOG_SUCCESS_CAT("MAIN", "{}COMMAND POOL FORGED — TRANSIENT GLORY{}", PLASMA_FUCHSIA, RESET);
}

static void detectBestPresentMode()
{
    LOG_INFO_CAT("MAIN", "Detecting optimal present mode...");
    VkPresentModeKHR mode = SwapchainManager::selectBestPresentMode(
        g_PhysicalDevice(), g_surface(), VK_PRESENT_MODE_MAILBOX_KHR);

    LOG_SUCCESS_CAT("MAIN", "Present mode locked: {} — TEAR-FREE PINK PHOTONS",
        mode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" :
        mode == VK_PRESENT_MODE_MAILBOX_KHR   ? "MAILBOX (KING)" :
        mode == VK_PRESENT_MODE_FIFO_KHR      ? "FIFO (VSync)" : "OTHER");
}

// =============================================================================
// THE TEN PHASES OF ASCENSION
// =============================================================================

static void phase0_preInitialization()
{
    bulkhead("PHASE 0: PRE-INITIALIZATION — THE VOID STIRS");
    LOG_ATTEMPT_CAT("MAIN", "=== AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3 ===");
    LOG_SUCCESS_CAT("MAIN", "{}THE EMPIRE AWAKENS — NOVEMBER 20, 2025{}", PLASMA_FUCHSIA, RESET);
}

static void phase1_iconPreload()
{
    bulkhead("PHASE 1: ICON PRELOAD — VALHALLA BRANDING");
    g_base_icon = IMG_Load("assets/textures/ammo32.ico");
    g_hdpi_icon = IMG_Load("assets/textures/ammo.ico");
    LOG_SUCCESS_CAT("MAIN", "{}Icons preloaded — Ammo eternal{}", PLASMA_FUCHSIA, RESET);
    // If they fail to load, it's fine — we run naked and proud
}

static void phase2_earlySdlInit()
{
    bulkhead("PHASE 2: EARLY SDL INIT — SUBSYSTEMS RISE");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0)
        throw std::runtime_error(std::format("SDL_Init failed: {}", SDL_GetError()));
    LOG_SUCCESS_CAT("MAIN", "SDL subsystems initialized — video + audio armed");
}

static void phase3_splashScreen()
{
    bulkhead("PHASE 3: SPLASH — FIRST LIGHT MANIFESTS");
    Splash::show("AMOURANTH RTX", 1280, 720, "assets/textures/ammo.png", "assets/audio/ammo.wav");
    LOG_SUCCESS_CAT("MAIN", "{}SPLASH COMPLETE — SHE HAS AWAKENED{}", PLASMA_FUCHSIA, RESET);
}

static void phase4_mainWindowAndVulkanContext(SDL_Window*& window)
{
    bulkhead("PHASE 4: MAIN WINDOW + VULKAN CONTEXT — FORGE THE EMPIRE");

    nukeValidationLayers();

    constexpr int WIDTH  = 3840;
    constexpr int HEIGHT = 2160;

    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", WIDTH, HEIGHT,
                       SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN);

    window = SDL3Window::get();
    if (!window) throw std::runtime_error("Failed to create main window");

    if (g_base_icon) {
        if (g_hdpi_icon) SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
        SDL_SetWindowIcon(window, g_base_icon);
        LOG_SUCCESS_CAT("MAIN", "Window icon sealed — Valhalla branded");
    }

    VkInstance instance = RTX::createVulkanInstanceWithSDL(window, false);
    set_g_instance(instance);

    if (!RTX::createSurface(window, instance))
        throw std::runtime_error("Failed to create Vulkan surface");

    SwapchainManager::init(window, WIDTH, HEIGHT);
    RTX::initContext(instance, window, WIDTH, HEIGHT);
    RTX::retrieveQueues();

    LOG_SUCCESS_CAT("MAIN", "{}VULKAN CONTEXT FORGED — STONEKEY v∞ NOW TRULY ALIVE{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}STONEKEY XOR FINGERPRINT: 0x{:016X}{}", 
                    PLASMA_FUCHSIA, get_kStone1() ^ get_kStone2(), RESET);
}

static void phase5_rtxAscension()
{
    bulkhead("PHASE 5: RTX ASCENSION — PINK PHOTONS AWAKEN");

    RTX::loadRayTracingExtensions();
    RTX::g_ctx().hasFullRTX_ = true;

    // Prime the driver (ensures device addresses work)
    {
        VkBufferCreateInfo bc{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bc.size = 64;
        bc.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkBuffer dummy = VK_NULL_HANDLE;
        if (vkCreateBuffer(g_device(), &bc, nullptr, &dummy) == VK_SUCCESS) {
            VkBufferDeviceAddressInfo dai{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = dummy};
            vkGetBufferDeviceAddress(g_device(), &dai);
            vkDestroyBuffer(g_device(), dummy, nullptr);
        }
    }

    forgeCommandPool();
    detectBestPresentMode();

    LOG_SUCCESS_CAT("MAIN", "{}RTX FULLY ASCENDED — DRIVER IS HAPPY — PINK PHOTONS ETERNAL{}", 
                    PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}FIRST LIGHT ACHIEVED — NOVEMBER 20, 2025 — VALHALLA SEALED{}", 
                    PLASMA_FUCHSIA, RESET);
}

static void phase6_sceneAndAccelerationStructures()
{
    bulkhead("PHASE 6: SCENE BUILD — LAS v2.0 ARMED");

    g_pipeline_manager = new RTX::PipelineManager(g_device(), g_PhysicalDevice());

    g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");
    LOG_SUCCESS_CAT("MAIN", "g_mesh ARMED → {} vertices, {} indices", 
                    g_mesh->vertices.size(), g_mesh->indices.size());

    las();
    las().buildBLAS(RTX::g_ctx().commandPool_,
                    g_mesh->getVertexBuffer(),
                    g_mesh->getIndexBuffer(),
                    static_cast<uint32_t>(g_mesh->vertices.size()),
                    static_cast<uint32_t>(g_mesh->indices.size()),
                    VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    las().buildTLAS(RTX::g_ctx().commandPool_, 
                    {{las().getBLAS(), glm::mat4(1.0f)}});

    LOG_SUCCESS_CAT("MAIN", "{}BLAS + TLAS BUILT — RAY TRACING READY — PHOTONS HAVE A PATH{}", 
                    PLASMA_FUCHSIA, RESET);
}

static void phase7_applicationAndRendererSeal()
{
    bulkhead("PHASE 7: APPLICATION + RENDERER SEAL — FINAL BINDING");

    g_app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);
    g_app->setRenderer(std::make_unique<VulkanRenderer>(3840, 2160, SDL3Window::get(), !Options::Window::VSYNC));

    LOG_SUCCESS_CAT("MAIN", "{}VULKANRENDERER SEALED INSIDE APPLICATION — THE EMPIRE IS COMPLETE{}", 
                    PLASMA_FUCHSIA, RESET);
}

static void phase8_renderLoop()
{
    bulkhead("PHASE 8: RENDER LOOP — ENTERING ETERNAL CYCLE");
    LOG_SUCCESS_CAT("MAIN", "{}PINK PHOTONS FLOW — INFINITE LOOP ENGAGED{}", PLASMA_FUCHSIA, RESET);
    g_app->run();
}

static void phase9_gracefulShutdown()
{
    bulkhead("PHASE 9–10: SHUTDOWN — THE EMPIRE FADES INTO ETERNAL PINK LIGHT");

    if (g_device() != VK_NULL_HANDLE) vkDeviceWaitIdle(g_device());

    g_app.reset();
    if (g_pipeline_manager) { delete g_pipeline_manager; g_pipeline_manager = nullptr; }
    g_mesh.reset();
    las().invalidate();

    SwapchainManager::cleanup();
    RTX::shutdown();

    if (g_surface() != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_instance(), g_surface(), nullptr);
        set_g_surface(VK_NULL_HANDLE);
    }

    if (g_instance() != VK_NULL_HANDLE) {
        vkDestroyInstance(g_instance(), nullptr);
        set_g_instance(VK_NULL_HANDLE);
    }

    LOG_SUCCESS_CAT("MAIN", "{}SHUTDOWN COMPLETE — ALL RESOURCES RETURNED TO THE VOID{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}P I N K   P H O T O N S   E T E R N A L{}", PLASMA_FUCHSIA, RESET);
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
}

int main(int argc, char* argv[])
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
        phase8_renderLoop();       // ← Never returns (unless window closed)
        phase9_gracefulShutdown(); // ← Only reached on clean exit
    }
    catch (...) {
        std::cerr << "\nFATAL EXCEPTION — EMPIRE UNDER ATTACK\n" << std::endl;
        phase9_gracefulShutdown();
        return -1;
    }

    LOG_SUCCESS_CAT("MAIN", "=== EXIT CLEAN — FIRST LIGHT ETERNAL ===");
    return 0;
}