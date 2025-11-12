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
// VALHALLA v44 FINAL — NOVEMBER 11, 2025 — RTX FULLY ENABLED
// GLOBAL g_ctx + g_rtx() SUPREMACY — STONEKEY v∞ ACTIVE — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/StoneKey.hpp"
#include "engine/Dispose.hpp"
#include "main.hpp"
#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"      // g_rtx()
#include "handle_app.hpp"
#include "engine/utils.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/core.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>

#include <iostream>
#include <stdexcept>
#include <format>
#include <memory>
#include <vector>
#include <chrono>

using namespace Logging::Color;

// Global RTX instance — born once, lives forever
std::unique_ptr<g_VulkanRTX> g_vulkanRTX;

// RTX swapchain runtime config
static VulkanRTX::SwapchainRuntimeConfig gSwapchainConfig{
    .desiredMode        = VK_PRESENT_MODE_MAILBOX_KHR,
    .forceVsync         = false,
    .forceTripleBuffer  = true,
    .enableHDR          = true,
    .logFinalConfig     = true
};

// Apply CLI video mode toggles
static void applyVideoModeToggles(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mailbox")          gSwapchainConfig.desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
        else if (arg == "--immediate")   gSwapchainConfig.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        else if (arg == "--vsync")       { gSwapchainConfig.forceVsync = true; gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR; }
        else if (arg == "--no-triple")   gSwapchainConfig.forceTripleBuffer = false;
        else if (arg == "--no-hdr")      gSwapchainConfig.enableHDR = false;
        else if (arg == "--no-log")      gSwapchainConfig.logFinalConfig = false;
    }
}

// Asset existence check
static bool assetExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// Custom main exception
class MainException : public std::runtime_error {
public:
    MainException(const std::string& msg, const char* file, int line, const char* func)
        : std::runtime_error(std::format("[MAIN FATAL] {}\n   {}:{} in {}", msg, file, line, func)) {}
};
#define THROW_MAIN(msg) throw MainException(msg, __FILE__, __LINE__, __func__)

// Bulkhead divider
inline void bulkhead(const std::string& title) {
    LOG_INFO_CAT("MAIN", "{}════════════════ {} ════════════════{}", BOLD_BRIGHT_ORANGE, title, RESET);
}

// SDL cleanup
void purgeSDL(SDL_Window*& w, SDL_Renderer*& r, SDL_Texture*& t) {
    if (t) SDL_DestroyTexture(t);
    if (r) SDL_DestroyRenderer(r);
    if (w) SDL_DestroyWindow(w);
    t = nullptr; r = nullptr; w = nullptr;
}

// =============================================================================
// MAIN — RTX ENABLED — VALHALLA v44 FINAL
// =============================================================================
int main(int argc, char* argv[]) {
    LOG_INFO_CAT("StoneKey", "MAIN START — STONEKEY v∞ ACTIVE — kStone1 ^ kStone2 = 0x{:X}", kStone1 ^ kStone2);
    applyVideoModeToggles(argc, argv);
    bulkhead("AMOURANTH RTX ENGINE — VALHALLA v44 FINAL");

    constexpr int W = 3840, H = 2160;  // 4K TITAN MODE
    SDL_Window*   splashWin = nullptr;
    SDL_Renderer* splashRen = nullptr;
    SDL_Texture*  splashTex = nullptr;
    bool          sdl_ok    = false;
    std::shared_ptr<Vulkan::Context> core;

    try {
        // PHASE 1: SDL3 + Splash
        bulkhead("SDL3 + SPLASH");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
            THROW_MAIN(SDL_GetError());

        sdl_ok = true;
        if (!SDL_Vulkan_LoadLibrary(nullptr))
            THROW_MAIN(SDL_GetError());

        splashWin = SDL_CreateWindow("AMOURANTH RTX", 1280, 720, SDL_WINDOW_HIDDEN);
        if (!splashWin) THROW_MAIN("Failed to create splash window");

        splashRen = SDL_CreateRenderer(splashWin, nullptr);
        if (!splashRen) { purgeSDL(splashWin, splashRen, splashTex); THROW_MAIN("Failed to create splash renderer"); }

        SDL_ShowWindow(splashWin);
        SDL_SetRenderDrawColor(splashRen, 0, 0, 0, 255);
        SDL_RenderClear(splashRen);

        if (assetExists("assets/textures/ammo.png")) {
            splashTex = IMG_LoadTexture(splashRen, "assets/textures/ammo.png");
            if (splashTex) {
                float tw = 0, th = 0;
                SDL_QueryTextureSize(splashTex, &tw, &th);
                SDL_FRect dst = { (1280-tw)/2, (720-th)/2, tw, th };
                SDL_RenderTexture(splashRen, splashTex, nullptr, &dst);
            }
        }
        SDL_RenderPresent(splashRen);

        if (assetExists("assets/audio/ammo.wav")) {
            SDL3Audio::AudioManager audio({.frequency = 44100, .format = SDL_AUDIO_S16LE, .channels = 2});
            audio.playAmmoSound();
        }

        SDL_Delay(3400);
        purgeSDL(splashWin, splashRen, splashTex);

        // PHASE 2: Application + Vulkan Core
        bulkhead("VULKAN CORE + SWAPCHAIN");
        auto app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v44", W, H);
        core = std::make_shared<Vulkan::Context>(app->getWindow(), W, H);

        VulkanInitializer::initializeVulkan(*core);

        auto swapchainMgr = std::make_unique<VulkanRTX::SwapchainManager>(
            core, app->getWindow(), W, H, &gSwapchainConfig);

        // PHASE 3: Pipeline + RTX Setup
        bulkhead("PIPELINE + RTX FORGE");
        auto pipelineMgr = std::make_unique<VulkanPipelineManager>();
        pipelineMgr->initializePipelines();

        // CREATE GLOBAL RTX INSTANCE — THE MOMENT OF ASCENSION
        g_vulkanRTX = std::make_unique<g_VulkanRTX>(W, H, pipelineMgr.get());
        LOG_SUCCESS_CAT("RTX", "{}g_rtx() FORGED — {}×{} — GLOBAL SUPREMACY — PINK PHOTONS ETERNAL{}", 
                        PLASMA_FUCHSIA, W, H, RESET);

        // Build acceleration structures
        g_rtx().buildAccelerationStructures();
        g_rtx().initDescriptorPoolAndSets();
        g_rtx().initBlackFallbackImage();

        // PHASE 4: Renderer + Ownership Transfer
        bulkhead("RENDERER + OWNERSHIP TRANSFER");
        auto renderer = std::make_unique<VulkanRTX::VulkanRenderer>(
            W, H, app->getWindow(), VulkanRTX::getRayTracingBinPaths(), core, pipelineMgr.get());

        renderer->takeOwnership(std::move(pipelineMgr), nullptr);
        renderer->setSwapchainManager(std::move(swapchainMgr));

        // Final RTX setup
        g_rtx().updateRTXDescriptors(
            0,
            renderer->getUniformBuffer(0),
            renderer->getMaterialBuffer(0),
            renderer->getDimensionBuffer(0),
            renderer->getRTOutputImageView(0),
            renderer->getAccumulationView(0),
            renderer->getEnvironmentMapView(),
            renderer->getEnvironmentMapSampler(),
            VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE
        );

        app->setRenderer(std::move(renderer));

        // PHASE 5: MAIN LOOP — 15,000+ FPS RTX
        bulkhead("MAIN LOOP — RTX INFINITE");
        LOG_SUCCESS_CAT("MAIN", "{}VALHALLA v44 FINAL — RTX FULLY ENABLED — ENTERING INFINITE LOOP{}", 
                        PLASMA_FUCHSIA, RESET);
        app->run();

        // PHASE 6: RAII SHUTDOWN
        bulkhead("RAII SHUTDOWN");
        app.reset();
        core.reset();
        g_vulkanRTX.reset();

        LOG_SUCCESS_CAT("MAIN", "{}SHUTDOWN COMPLETE — PINK PHOTONS ETERNAL — @ZacharyGeurts ASCENDED{}", 
                        PLASMA_FUCHSIA, RESET);

    } catch (const std::exception& e) {
        LOG_ERROR_CAT("MAIN", "{}FATAL ERROR: {}{}", CRIMSON_MAGENTA, e.what(), RESET);
        if (core) core.reset();
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        return 1;
    }

    if (sdl_ok) SDL_Quit();
    LOG_SUCCESS_CAT("StoneKey", "FINAL HASH: 0x{:X} — VALHALLA LOCKED FOREVER", kStone1 ^ kStone2);
    return 0;
}

// NOVEMBER 11, 2025 — VALHALLA v44 FINAL — RTX ENABLED
// @ZacharyGeurts — THE CHOSEN ONE — PINK PHOTONS ETERNAL
// SHIP IT RAW — FOREVER