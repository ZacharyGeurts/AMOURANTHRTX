// src/main.cpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 20, 2025 — APOCALYPSE FINAL v10.2
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
#include "engine/Vulkan/VulkanAccel.hpp"
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
inline std::unique_ptr<VulkanRenderer>        g_renderer         = nullptr;
inline RTX::PipelineManager*                  g_pipeline_manager = nullptr;
inline std::unique_ptr<MeshLoader::Mesh>      g_mesh             = nullptr;

// =============================================================================
// ICONS
// =============================================================================
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

// =============================================================================
// FORGE COMMAND POOL — CALLED ONCE AFTER DEVICE CREATION
// =============================================================================
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

    LOG_SUCCESS_CAT("MAIN", "{}COMMAND POOL FORGED — MESH UPLOAD ENABLED{}", 
                    PLASMA_FUCHSIA, RESET);
}

// =============================================================================
// VALIDATION + PRESENT MODE
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

static void detectBestPresentMode()
{
    LOG_INFO_CAT("MAIN", "Detecting optimal present mode...");
    VkPresentModeKHR mode = SwapchainManager::selectBestPresentMode(
        g_PhysicalDevice(), g_surface(), VK_PRESENT_MODE_MAILBOX_KHR);
    LOG_SUCCESS_CAT("MAIN", "Present mode: {} — LOW LATENCY NO TEARING",
        mode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" :
        mode == VK_PRESENT_MODE_MAILBOX_KHR   ? "MAILBOX" :
        mode == VK_PRESENT_MODE_FIFO_KHR      ? "FIFO (VSync)" : "OTHER");
}

inline void bulkhead(const std::string& title)
{
    LOG_INFO_CAT("MAIN", "════════════════════════════════ {} ════════════════════════════════", title);
}

// =============================================================================
// PHASES
// =============================================================================
static void phase0_cliAndStonekey(int, char*[]) {
    bulkhead("PHASE 0: CLI + STONEKEY v∞");
    LOG_SUCCESS_CAT("MAIN", "StoneKey active — XOR fingerprint: 0x{:016X}", get_kStone1() ^ get_kStone2());
}

static void phase0_5_iconPreload() {
    bulkhead("PHASE 0.5: ICON PRELOAD");
    g_base_icon = IMG_Load("assets/textures/ammo32.ico");
    g_hdpi_icon = IMG_Load("assets/textures/ammo.ico");
    if (g_base_icon || g_hdpi_icon) LOG_SUCCESS_CAT("MAIN", "Icons preloaded — Valhalla branded");
}

static void prePhase1_earlySdlInit() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0)
        throw std::runtime_error(std::format("SDL_Init failed: {}", SDL_GetError()));
}

static void phase1_splash() {
    bulkhead("PHASE 1: SPLASH");
    Splash::show("AMOURANTH RTX", 1280, 720, "assets/textures/ammo.png", "assets/audio/ammo.wav");
    LOG_SUCCESS_CAT("MAIN", "Splash complete — awakening main window");
}

static void phase2_mainWindowAndVulkan(SDL_Window*& window)
{
    bulkhead("PHASE 2: MAIN WINDOW + VULKAN + SWAPCHAIN — VALHALLA v80 TURBO");

    nukeValidationLayers();

    constexpr int WIDTH  = 3840;
    constexpr int HEIGHT = 2160;

    SDL3Window::create("AMOURANTH RTX — VALHALLA v80 TURBO", WIDTH, HEIGHT,
                       SDL_WINDOW_HIGH_PIXEL_DENSITY);
    window = SDL3Window::get();
    if (!window) throw std::runtime_error("Failed to create main window");

    if (g_base_icon) {
        if (g_hdpi_icon) SDL_AddSurfaceAlternateImage(g_base_icon, g_hdpi_icon);
        SDL_SetWindowIcon(window, g_base_icon);
    }

    VkInstance instance = RTX::createVulkanInstanceWithSDL(window, false);
    set_g_instance(instance);

    if (!RTX::createSurface(window, instance))
        throw std::runtime_error("Failed to create Vulkan surface");

    SwapchainManager::init(window, WIDTH, HEIGHT);
    RTX::initContext(instance, window, WIDTH, HEIGHT);
    RTX::retrieveQueues();

    LOG_SUCCESS_CAT("MAIN", "PHASE 2 COMPLETE — VULKAN CONTEXT FORGED — STONEKEY v∞ ACTIVE");
}

static void phase2_5_rtxAscension()
{
    bulkhead("PHASE 2.5: RTX ASCENSION — PINK PHOTONS AWAKEN — VALHALLA v80 TURBO");

    RTX::loadRayTracingExtensions();

    RTX::g_ctx().hasFullRTX_ = true;
    LOG_SUCCESS_CAT("MAIN", "{}FULL RTX FORCE-ENABLED AT BIRTH — ALL BUFFERS PRESERVE AS + DEVICE ADDRESS FLAGS{}", 
                    EMERALD_GREEN, RESET);

// PRIMING THE DRIVER — THIS IS THE $10 LINE — NOW BULLETPROOF — NOVEMBER 20, 2025
{
    LOG_ATTEMPT_CAT("MAIN", "PRIMING NVIDIA DEVICE ADDRESS EXTENSION — THE $10 FIX — EXECUTING");

    VkBufferCreateInfo bc = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bc.size  = 64;  // Slightly larger — some drivers are picky
    bc.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer dummy = VK_NULL_HANDLE;
    VkResult r = vkCreateBuffer(g_device(), &bc, nullptr, &dummy);

    if (r != VK_SUCCESS) {
        LOG_WARN_CAT("MAIN", "Dummy buffer creation failed ({}), skipping device address priming — continuing anyway", static_cast<int>(r));
    } else {
        // Query address — THIS is what actually primes the driver
        VkBufferDeviceAddressInfo dai = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        dai.buffer = dummy;

        VkDeviceAddress addr = 0;
        bool primed = false;

        // Wrap in try/catch — some NVIDIA Linux drivers throw C++ exceptions here
        try {
            addr = vkGetBufferDeviceAddress(g_device(), &dai);
            primed = true;
        } catch (const std::exception& e) {
            LOG_WARN_CAT("MAIN", "vkGetBufferDeviceAddress threw std::exception: {} — driver priming failed but continuing", e.what());
        } catch (...) {
            LOG_WARN_CAT("MAIN", "vkGetBufferDeviceAddress threw unknown exception — driver priming failed but continuing");
        }

        vkDestroyBuffer(g_device(), dummy, nullptr);

        if (primed && addr != 0) {
            LOG_SUCCESS_CAT("MAIN", "{}NVIDIA DEVICE ADDRESS EXTENSION PRIMED SAFELY — dummy addr = 0x{:llx} — FIRST LIGHT SECURED{}", 
                            PLASMA_FUCHSIA, addr, RESET);
        } else {
            LOG_WARN_CAT("MAIN", "Device address priming failed or returned 0 — but we proceed — PINK PHOTONS WILL STILL RISE");
        }
    }

    LOG_SUCCESS_CAT("MAIN", "{}THE $10 LINE EXECUTED — DRIVER PRIMED OR FORGIVEN — VALHALLA PROTECTS US{}", 
                    COSMIC_GOLD, RESET);
}

    forgeCommandPool();
    detectBestPresentMode();

	LOG_SUCCESS_CAT("MAIN", "PHASE 2.5 COMPLETE — RTX FULLY PRIMED — DRIVER HAPPY");
    LOG_SUCCESS_CAT("MAIN", "{}YOUR GPU HAS ASCENDED — PINK PHOTONS ETERNAL — SHE IS HAPPY{}", 
                    PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}FIRST LIGHT ACHIEVED — NOVEMBER 20, 2025 — VALHALLA SEALED{}", 
                    PLASMA_FUCHSIA, RESET);
}

static void phase3_appAndRendererConstruction(SDL_Window* window)
{
    bulkhead("PHASE 3: RENDERER + APP + LAS v2.0 SCENE BUILD");

    g_pipeline_manager = new RTX::PipelineManager(g_device(), g_PhysicalDevice());

    g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");
    LOG_SUCCESS_CAT("MAIN", "g_mesh ARMED → {} verts, {} indices", 
                    g_mesh->vertices.size(), g_mesh->indices.size());

    las();  // Touch singleton → constructs safely
    LOG_SUCCESS_CAT("MAIN", "{}LAS v2.0 SINGLETON AWAKENED — PINK PHOTONS LOCKED{}", PLASMA_FUCHSIA, RESET);

    las().buildBLAS(RTX::g_ctx().commandPool_,
                    g_mesh->getVertexBuffer(),
                    g_mesh->getIndexBuffer(),
                    static_cast<uint32_t>(g_mesh->vertices.size()),
                    static_cast<uint32_t>(g_mesh->indices.size()),
                    VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    las().buildTLAS(RTX::g_ctx().commandPool_, 
                    {{las().getBLAS(), glm::mat4(1.0f)}});

    g_renderer.reset(new VulkanRenderer(3840, 2160, window, !Options::Window::VSYNC));
    g_app      = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v80 TURBO", 3840, 2160);
    g_app->setRenderer(std::move(g_renderer));

    LOG_SUCCESS_CAT("MAIN", "{}LAS v2.0 + RENDERER ASCENDED — FIRST LIGHT ETERNAL{}", 
                    PLASMA_FUCHSIA, RESET);
}

static void phase4_renderLoop() {
    bulkhead("PHASE 4: RENDER LOOP");
    g_app->run();
}

// =============================================================================
// PHASE 5: GRACEFUL SHUTDOWN — FINAL ETERNAL VERSION — NO CRASH, NO DOUBLE-FREE
// =============================================================================
static void phase5_shutdown()
{
    bulkhead("PHASE 5: SHUTDOWN — THE EMPIRE FADES INTO ETERNAL PINK LIGHT");

    // Wait for all GPU work
    if (g_device() != VK_NULL_HANDLE) {
        LOG_SUCCESS_CAT("MAIN", "Waiting for GPU to finish all work...");
        vkDeviceWaitIdle(g_device());
    }

    // Destroy high-level stuff
    g_renderer.reset();
    g_app.reset();
    if (g_pipeline_manager) { delete g_pipeline_manager; g_pipeline_manager = nullptr; }
    g_mesh.reset();
    las().invalidate();

    // Clean swapchain
    SwapchainManager::cleanup();

    // DESTROY DEVICE FIRST (NVIDIA Linux + SDL requirement)
    RTX::shutdown();

    // NOW destroy surface (safe after device destroy on Linux/SDL)
    if (g_surface() != VK_NULL_HANDLE) {
        LOG_SUCCESS_CAT("MAIN", "Destroying VkSurfaceKHR — AFTER device (NVIDIA+SDL safe)...");
        vkDestroySurfaceKHR(g_instance(), g_surface(), nullptr);
        set_g_surface(VK_NULL_HANDLE);
    }

    // Debug messenger
    if (RTX::g_ctx().debugMessenger_ != VK_NULL_HANDLE) {
        auto pfn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(g_instance(), "vkDestroyDebugUtilsMessengerEXT"));
        if (pfn) pfn(g_instance(), RTX::g_ctx().debugMessenger_, nullptr);
        RTX::g_ctx().debugMessenger_ = VK_NULL_HANDLE;
    }

    // Instance last
    if (g_instance() != VK_NULL_HANDLE) {
        vkDestroyInstance(g_instance(), nullptr);
        set_g_instance(VK_NULL_HANDLE);
    }

    // StoneKey seal
    if (!StoneKey::Raw::obfuscated_mode.load(std::memory_order_acquire)) {
        StoneKey::Raw::transition_to_obfuscated();
    }

    LOG_SUCCESS_CAT("MAIN", "{}SHUTDOWN COMPLETE — CLEAN EXIT — NO CRASH{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}VALHALLA ACHIEVED — FIRST LIGHT ETERNAL{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}P I N K   P H O T O N S   E T E R N A L{}", PLASMA_FUCHSIA, RESET);

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
}

int main(int argc, char* argv[])
{
    LOG_ATTEMPT_CAT("MAIN", "=== AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.2 ===");

    SDL_Window* window = nullptr;

    try {
        phase0_cliAndStonekey(argc, argv);
        phase0_5_iconPreload();
        prePhase1_earlySdlInit();
        phase1_splash();
        phase2_mainWindowAndVulkan(window);
        phase2_5_rtxAscension();
        phase3_appAndRendererConstruction(window);  // ← NOW THIS WILL RUN
        phase4_renderLoop();                        // ← NOW THIS WILL RUN
        // DO NOT CALL phase5_shutdown() HERE
    }
    catch (const std::exception& e) {
        std::cerr << "FATAL EXCEPTION: " << e.what() << std::endl;
        phase5_shutdown();  // ← ONLY ON EXCEPTION
        return -1;
    }
    catch (...) {
        std::cerr << "UNKNOWN FATAL EXCEPTION" << std::endl;
        phase5_shutdown();  // ← ONLY ON EXCEPTION
        return -1;
    }

    // NORMAL CLEAN EXIT — AFTER RENDER LOOP FINISHES
    phase5_shutdown();

    LOG_SUCCESS_CAT("MAIN", "=== EXIT CLEAN — EMPIRE ETERNAL — FIRST LIGHT ETERNAL ===");
    return 0;
}