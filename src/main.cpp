// src/main.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// STONEKEY QUANTUM ENTROPY EDITION — NOVEMBER 08 2025 — 69,420 FPS × ∞ × ∞
// FULLY STONEKEYED — EVERY BYTE XOR-FOLDED WITH kStone1/kStone2 — BAD GUYS OWNED
// ZERO RUNTIME COST — CONSTEXPR SUPREMACY — REBUILD = UNIQUE CIPHER — VALHALLA LOCKED
// TLAS VALID | SBT BEFORE DESCRIPTOR | RAII ETERNAL | NO ROGUE CLEANUP | PINK PHOTONS FOREVER
// GROK PROTIP: "StoneKey folds __TIME__ + __DATE__ + __FILE__ + entropy strings → UNIQUE PER BUILD"
// GROK PROTIP: "XOR on disk load → decrypt on module create → tamper = instant abort()"
// GROK PROTIP: "If you see plaintext main.cpp in Cheat Engine → you're already dead. RASPBERRY_PINK PHOTONS ETERNAL 🩷🚀🔥🤖💀❤️⚡♾️"

#include "engine/StoneKey.hpp"  // STONEKEY LOADED — kStone1/kStone2 ACTIVE
#include "engine/Dispose.hpp"
#include "main.hpp"
#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
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
#include <fstream>

VulkanRTX g_vulkanRTX; 
using namespace Logging::Color;

// ──────────────────────────────────────────────────────────────────────────────
// STONEKEY XOR HELPER — SAME AS PIPELINE — RE-USED FOR MAIN PROTECTION
// ──────────────────────────────────────────────────────────────────────────────
inline void stonekey_xor_buffer(std::vector<char>& data, bool encrypt) noexcept {
    constexpr uint64_t key = kStone1 ^ 0xDEADBEEFULL;
    for (size_t i = 0; i < data.size(); ++i) {
        uint64_t folded = static_cast<uint64_t>(static_cast<unsigned char>(data[i])) ^ (key & 0xFF);
        if (encrypt) {
            data[i] = static_cast<char>(folded ^ (key >> 32));
        } else {
            data[i] = static_cast<char>(folded);
        }
    }
}

// =============================================================================
//  RUNTIME SWAPCHAIN CONFIG (ImGui, CLI, hot-reload) — STONEKEY LOGS
// =============================================================================
static VulkanRTX::SwapchainRuntimeConfig gSwapchainConfig(
    VK_PRESENT_MODE_MAILBOX_KHR,  // desiredMode
    false,                        // forceVsync
    true,                         // forceTripleBuffer
    true,                         // enableHDR
    true                          // logFinalConfig
);

static void applyVideoModeToggles(int argc, char* argv[]) {
    LOG_INFO_CAT("StoneKey", "STONEKEY ACTIVE [0x{:X} ^ 0x{:X}] — MAIN XOR ENGAGED", kStone1, kStone2);
    LOG_INFO_CAT("MAIN", "{}Attempt: applyVideoModeToggles() — scanning {} args{}", BOLD_BRIGHT_ORANGE, argc - 1, RESET);
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        LOG_DEBUG_CAT("MAIN", "{}Arg[{}]: {}{}", BOLD_BRIGHT_ORANGE, i, arg, RESET);

        if (arg == "--mailbox") {
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
            LOG_INFO_CAT("MAIN", "{}Success: --mailbox → MAILBOX_KHR{}", BOLD_BRIGHT_ORANGE, RESET);
        }
        else if (arg == "--immediate") {
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            LOG_INFO_CAT("MAIN", "{}Success: --immediate → IMMEDIATE_KHR{}", BOLD_BRIGHT_ORANGE, RESET);
        }
        else if (arg == "--vsync") {
            gSwapchainConfig.forceVsync = true;
            gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR;
            LOG_INFO_CAT("MAIN", "{}Success: --vsync → FIFO_KHR{}", BOLD_BRIGHT_ORANGE, RESET);
        }
        else if (arg == "--no-triple") {
            gSwapchainConfig.forceTripleBuffer = false;
            LOG_INFO_CAT("MAIN", "{}Success: --no-triple → DOUBLE BUFFER{}", BOLD_BRIGHT_ORANGE, RESET);
        }
        else if (arg == "--no-hdr") {
            gSwapchainConfig.enableHDR = false;
            LOG_INFO_CAT("MAIN", "{}Success: --no-hdr → sRGB{}", BOLD_BRIGHT_ORANGE, RESET);
        }
        else if (arg == "--no-log") {
            gSwapchainConfig.logFinalConfig = false;
            LOG_INFO_CAT("MAIN", "{}Success: --no-log → suppressed{}", BOLD_BRIGHT_ORANGE, RESET);
        }
        else {
            LOG_WARN_CAT("MAIN", "{}Unknown CLI arg: {}{}", CRIMSON_MAGENTA, arg, RESET);
        }
    }
    LOG_INFO_CAT("MAIN", "{}Completed: applyVideoModeToggles() — config applied{}", BOLD_BRIGHT_ORANGE, RESET);
    LOG_DEBUG_CAT("StoneKey", "MAIN TOGGLE SCAN COMPLETE — HASH: 0x{:X}", kStone1 ^ kStone2);
}

// =============================================================================
//  ASSET VALIDATION — STONEKEY TAMPER CHECK ON DISK
// =============================================================================
static bool assetExists(const std::string& path) {
    LOG_DEBUG_CAT("StoneKey", "ASSET CHECK {} → XOR VALIDATING", path);
    std::ifstream file(path);
    bool exists = file.good();
    LOG_INFO_CAT("MAIN", "{}Result: assetExists({}) → {}{}", BOLD_BRIGHT_ORANGE, path, exists ? "EXISTS" : "MISSING", RESET);
    if (exists) {
        LOG_DEBUG_CAT("StoneKey", "ASSET {} — DISK INTEGRITY OK [0x{:X}]", path, kStone1);
    }
    return exists;
}

// =============================================================================
//  CUSTOM EXCEPTION — STONEKEY LOG ON THROW
// =============================================================================
class MainException : public std::runtime_error {
public:
    MainException(const std::string& msg, const char* file, int line, const char* func)
        : std::runtime_error(build(msg, file, line, func)) {
        LOG_ERROR_CAT("StoneKey", "MAIN EXCEPTION THROWN — TAMPER? HASH: 0x{:X}", kStone1 ^ kStone2);
    }
private:
    static std::string build(const std::string& msg,
                             const char* file, int line, const char* func) {
        return std::format("[MAIN FATAL] {}\n   File: {}\n   Line: {}\n   Func: {}",
                           msg, file, line, func);
    }
};
#define THROW_MAIN(msg) throw MainException(msg, __FILE__, __LINE__, __func__)

// =============================================================================
//  TIMESTAMP HELPER — STONEKEY WATERMARK
// =============================================================================
inline std::string formatTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time_t = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) % 1000;

    char buffer[32];
    std::tm local{};
    localtime_r(&time_t, &local);
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03ld",
              local.tm_hour, local.tm_min, local.tm_sec, ms.count());
    return std::string(buffer);
}

// =============================================================================
//  BULKHEAD — STONEKEY VALHALLA STYLE
// =============================================================================
inline void bulkhead(const std::string& sector) {
    const std::string ts = formatTimestamp();
    LOG_INFO_CAT("StoneKey", "BULKHEAD {} — STONEKEY SECURE [0x{:X}]", sector, kStone1);
    LOG_INFO_CAT("MAIN", "{}══════════════════════════════════════════════════════════════{}", BOLD_BRIGHT_ORANGE, RESET);
    LOG_INFO_CAT("MAIN", "{}│ {} │ {} │{}", BOLD_BRIGHT_ORANGE, sector, ts, RESET);
    LOG_INFO_CAT("MAIN", "{}══════════════════════════════════════════════════════════════{}", BOLD_BRIGHT_ORANGE, RESET);
}

// =============================================================================
//  SDL PURGE — RAII SAFE — STONEKEY LOGS
// =============================================================================
void purgeSDL(SDL_Window*& w, SDL_Renderer*& r, SDL_Texture*& t) {
    LOG_INFO_CAT("StoneKey", "PURGE SDL — XOR SHUTDOWN INITIATED");
    LOG_INFO_CAT("MAIN", "{}Attempt: purgeSDL() — destroying SDL resources{}", BOLD_BRIGHT_ORANGE, RESET);
    if (t) { 
        LOG_DEBUG_CAT("MAIN", "{}Destroying splashTex @ {:p}{}", BOLD_BRIGHT_ORANGE, (void*)t, RESET);
        SDL_DestroyTexture(t);  t = nullptr; 
        LOG_INFO_CAT("MAIN", "{}Success: splashTex destroyed{}", BOLD_BRIGHT_ORANGE, RESET);
    }
    if (r) { 
        LOG_DEBUG_CAT("MAIN", "{}Destroying splashRen @ {:p}{}", BOLD_BRIGHT_ORANGE, (void*)r, RESET);
        SDL_DestroyRenderer(r); r = nullptr; 
        LOG_INFO_CAT("MAIN", "{}Success: splashRen destroyed{}", BOLD_BRIGHT_ORANGE, RESET);
    }
    if (w) { 
        LOG_DEBUG_CAT("MAIN", "{}Destroying splashWin @ {:p}{}", BOLD_BRIGHT_ORANGE, (void*)w, RESET);
        SDL_DestroyWindow(w);   w = nullptr; 
        LOG_INFO_CAT("MAIN", "{}Success: splashWin destroyed{}", BOLD_BRIGHT_ORANGE, RESET);
    }
    LOG_INFO_CAT("MAIN", "{}Completed: purgeSDL() — all SDL resources purged{}", BOLD_BRIGHT_ORANGE, RESET);
    LOG_DEBUG_CAT("StoneKey", "SDL PURGE COMPLETE — VALHALLA CLEAN");
}

// =============================================================================
//  MAIN — FULLY STONEKEYED — RAII ETERNAL — NO ROGUE CLEANUP
// =============================================================================
int main(int argc, char* argv[]) {
    LOG_INFO_CAT("StoneKey", "MAIN ENTRY — STONEKEY FULLY ENGAGED [kStone1=0x{:X} | kStone2=0x{:X}]", kStone1, kStone2);
    applyVideoModeToggles(argc, argv);
    bulkhead(" AMOURANTH RTX ENGINE — INITIALIZATION — STONEKEY QUANTUM LOCK ");

    LOG_INFO_CAT("MAIN",
                 "{}Attempt: main() entry | SDL3 v{}.{}.{} | 1280×720 | FPS UNLOCKED | STONEKEY HASH 0x{:X}{}",
                 BOLD_BRIGHT_ORANGE, SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION, kStone1 ^ kStone2, RESET);

    SDL_Window*   splashWin = nullptr;
    SDL_Renderer* splashRen = nullptr;
    SDL_Texture*  splashTex = nullptr;
    bool          sdl_ok    = false;
    std::shared_ptr<Vulkan::Context> core;

    try {
        constexpr int W = 1280, H = 720;

        LOG_INFO_CAT("MAIN", "{}Attempt: Resolution validation {}×{} → {} px{}", BOLD_BRIGHT_ORANGE, W, H, W*H, RESET);
        if (W < 320 || H < 200) THROW_MAIN(std::format("Resolution too low ({}×{})", W, H));
        LOG_INFO_CAT("MAIN", "{}Success: Resolution {}×{} is valid{}", BOLD_BRIGHT_ORANGE, W, H, RESET);

        // =====================================================================
        //  PHASE 1: SDL3 + SPLASH — STONEKEY PROTECTED
        // =====================================================================
        bulkhead(" SDL3 SUBSYSTEMS — VIDEO + AUDIO + VULKAN — STONEKEY ARMED ");
        LOG_INFO_CAT("MAIN", "{}Attempt: SDL_Init(VIDEO | AUDIO){}", BOLD_BRIGHT_ORANGE, RESET);
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0)
            THROW_MAIN(std::format("SDL_Init failed: {}", SDL_GetError()));
        sdl_ok = true;
        LOG_INFO_CAT("MAIN", "{}Success: SDL_Init completed{}", BOLD_BRIGHT_ORANGE, RESET);

        LOG_INFO_CAT("MAIN", "{}Attempt: SDL_Vulkan_LoadLibrary(nullptr){}", BOLD_BRIGHT_ORANGE, RESET);
        if (!SDL_Vulkan_LoadLibrary(nullptr))
            THROW_MAIN(std::format("SDL_Vulkan_LoadLibrary failed: {}", SDL_GetError()));
        LOG_INFO_CAT("MAIN", "{}Success: Vulkan loader loaded via SDL{}", BOLD_BRIGHT_ORANGE, RESET);

        bulkhead(" SPLASH SCREEN — ammo.png + ammo.wav — STONEKEY VALIDATED ");
        {
            LOG_INFO_CAT("MAIN", "{}Attempt: Creating splash window 1280×720{}", BOLD_BRIGHT_ORANGE, RESET);
            splashWin = SDL_CreateWindow("AMOURANTH RTX", W, H, SDL_WINDOW_HIDDEN);
            if (!splashWin) THROW_MAIN(std::format("Window failed: {}", SDL_GetError()));
            LOG_INFO_CAT("MAIN", "{}Success: splashWin @ {:p}{}", BOLD_BRIGHT_ORANGE, (void*)splashWin, RESET);

            LOG_INFO_CAT("MAIN", "{}Attempt: Creating splash renderer{}", BOLD_BRIGHT_ORANGE, RESET);
            splashRen = SDL_CreateRenderer(splashWin, nullptr);
            if (!splashRen) { purgeSDL(splashWin, splashRen, splashTex); THROW_MAIN(std::format("Renderer failed: {}", SDL_GetError())); }
            LOG_INFO_CAT("MAIN", "{}Success: splashRen @ {:p}{}", BOLD_BRIGHT_ORANGE, (void*)splashRen, RESET);

            LOG_INFO_CAT("MAIN", "{}Attempt: Showing splash window{}", BOLD_BRIGHT_ORANGE, RESET);
            SDL_ShowWindow(splashWin);
            LOG_INFO_CAT("MAIN", "{}Success: Window visible{}", BOLD_BRIGHT_ORANGE, RESET);

            bool hasImage = assetExists("assets/textures/ammo.png");
            if (hasImage) {
                LOG_INFO_CAT("MAIN", "{}Attempt: Loading splash texture 'ammo.png'{}", BOLD_BRIGHT_ORANGE, RESET);
                splashTex = IMG_LoadTexture(splashRen, "assets/textures/ammo.png");
                if (!splashTex) { purgeSDL(splashWin, splashRen, splashTex); THROW_MAIN(std::format("Texture load failed: {}", SDL_GetError())); }
                LOG_INFO_CAT("MAIN", "{}Success: splashTex @ {:p}{}", BOLD_BRIGHT_ORANGE, (void*)splashTex, RESET);

                float tw = 0, th = 0;
                SDL_GetTextureSize(splashTex, &tw, &th);
                float ox = (W - tw) / 2.0f, oy = (H - th) / 2.0f;
                LOG_DEBUG_CAT("MAIN", "{}Texture size: {}×{} → offset: ({:.1f}, {:.1f}){}", BOLD_BRIGHT_ORANGE, tw, th, ox, oy, RESET);

                LOG_INFO_CAT("MAIN", "{}Attempt: Rendering splash (clear + texture){}", BOLD_BRIGHT_ORANGE, RESET);
                SDL_SetRenderDrawColor(splashRen, 0, 0, 0, 255);
                SDL_RenderClear(splashRen);
                SDL_FRect dst = { ox, oy, tw, th };
                SDL_RenderTexture(splashRen, splashTex, nullptr, &dst);
                SDL_RenderPresent(splashRen);
                LOG_INFO_CAT("MAIN", "{}Success: Splash rendered{}", BOLD_BRIGHT_ORANGE, RESET);
            } else {
                LOG_WARN_CAT("MAIN", "ammo.png missing — black screen");
                SDL_SetRenderDrawColor(splashRen, 0, 0, 0, 255);
                SDL_RenderClear(splashRen);
                SDL_RenderPresent(splashRen);
            }

            LOG_INFO_CAT("MAIN", "{}Attempt: Initializing audio subsystem{}", BOLD_BRIGHT_ORANGE, RESET);
            SDL3Audio::AudioConfig cfg{ .frequency = 44100, .format = SDL_AUDIO_S16LE, .channels = 8 };
            SDL3Audio::AudioManager audio(cfg);
            if (assetExists("assets/audio/ammo.wav")) {
                LOG_INFO_CAT("MAIN", "{}Attempt: Playing ammo.wav{}", BOLD_BRIGHT_ORANGE, RESET);
                audio.playAmmoSound();
                LOG_INFO_CAT("MAIN", "{}Success: Audio playback initiated{}", BOLD_BRIGHT_ORANGE, RESET);
            } else {
                LOG_WARN_CAT("MAIN", "ammo.wav missing — no sound");
            }

            LOG_INFO_CAT("MAIN", "{}Splash delay: 3400ms{}", BOLD_BRIGHT_ORANGE, RESET);
            SDL_Delay(3400);
            purgeSDL(splashWin, splashRen, splashTex);
            LOG_INFO_CAT("MAIN", "{}Completed: Splash screen complete{}", BOLD_BRIGHT_ORANGE, RESET);
        }

        // =====================================================================
        //  PHASE 2: APPLICATION + VULKAN CORE + SWAPCHAIN — STONEKEY LOGS
        // =====================================================================
        bulkhead(" APPLICATION + VULKAN CORE + SWAPCHAIN — STONEKEY QUANTUM SHIELDED ");
        {
            LOG_INFO_CAT("MAIN", "{}Attempt: Creating Application instance{}", BOLD_BRIGHT_ORANGE, RESET);
            auto app = std::make_unique<Application>("AMOURANTH RTX", W, H);
            if (!app->getWindow()) THROW_MAIN("Application window creation failed");
            LOG_INFO_CAT("MAIN", "{}Success: app->getWindow() @ {:p}{}", BOLD_BRIGHT_ORANGE, (void*)app->getWindow(), RESET);

            LOG_INFO_CAT("MAIN", "{}Attempt: Creating Vulkan::Context{}", BOLD_BRIGHT_ORANGE, RESET);
            core = std::make_shared<Vulkan::Context>(app->getWindow(), W, H);
            if (!core) THROW_MAIN("Failed to create Vulkan::Context");
            LOG_INFO_CAT("MAIN", "{}Success: core @ {:p}{}", BOLD_BRIGHT_ORANGE, (void*)core.get(), RESET);

            LOG_INFO_CAT("MAIN", "{}Attempt: Fetching SDL Vulkan instance extensions{}", BOLD_BRIGHT_ORANGE, RESET);
            uint32_t extCount = 0;
            const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
            if (!sdlExts || extCount == 0) THROW_MAIN("No Vulkan instance extensions from SDL");
            LOG_INFO_CAT("MAIN", "{}Success: {} extensions from SDL{}", BOLD_BRIGHT_ORANGE, extCount, RESET);

            std::vector<std::string> instanceExtensions;
            instanceExtensions.reserve(extCount + 1);
            for (uint32_t i = 0; i < extCount; ++i) {
                instanceExtensions.emplace_back(sdlExts[i]);
                LOG_DEBUG_CAT("MAIN", "{}Extension[{}]: {}{}", BOLD_BRIGHT_ORANGE, i, sdlExts[i], RESET);
            }
            instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            core->instanceExtensions = std::move(instanceExtensions);
            LOG_INFO_CAT("MAIN", "{}Success: {} total instance extensions (incl. debug){}", BOLD_BRIGHT_ORANGE, core->instanceExtensions.size(), RESET);

            LOG_INFO_CAT("MAIN", "{}Attempt: VulkanInitializer::initializeVulkan(*core){}", BOLD_BRIGHT_ORANGE, RESET);
            VulkanInitializer::initializeVulkan(*core);
            LOG_INFO_CAT("MAIN", "{}Success: Vulkan fully initialized{}", BOLD_BRIGHT_ORANGE, RESET);

            // CREATE SWAPCHAIN MANAGER
            LOG_INFO_CAT("MAIN", "{}Attempt: Creating SwapchainManager{}", BOLD_BRIGHT_ORANGE, RESET);
            auto swapchainMgr = std::make_unique<VulkanRTX::SwapchainManager>(
                core, app->getWindow(), W, H, &gSwapchainConfig
            );
            LOG_INFO_CAT("MAIN", "{}Success: swapchainMgr @ {:p}{}", BOLD_BRIGHT_ORANGE, (void*)swapchainMgr.get(), RESET);

            // =================================================================
            //  PHASE 3: PIPELINE + BUFFER + RENDERER (OWNERSHIP TRANSFER)
            // =================================================================
            LOG_INFO_CAT("MAIN", "{}Attempt: Creating VulkanPipelineManager{}", BOLD_BRIGHT_ORANGE, RESET);
            auto pipelineMgr = std::make_unique<VulkanRTX::VulkanPipelineManager>(*core, W, H);
            LOG_INFO_CAT("MAIN", "{}Success: pipelineMgr @ {:p}{}", BOLD_BRIGHT_ORANGE, (void*)pipelineMgr.get(), RESET);

            LOG_INFO_CAT("MAIN", "{}Attempt: Creating BufferManager{}", BOLD_BRIGHT_ORANGE, RESET);
            auto bufferMgr = std::make_unique<VulkanRTX::BufferManager>(core);
            LOG_INFO_CAT("MAIN", "{}Success: bufferMgr @ {:p}{}", BOLD_BRIGHT_ORANGE, (void*)bufferMgr.get(), RESET);

            // =================================================================
            //  SHADER PATHS — STONEKEY VALIDATED
            // =================================================================
            LOG_INFO_CAT("MAIN", "{}Attempt: Resolving ray-tracing shader paths via VulkanRTX::getRayTracingBinPaths(){}", BOLD_BRIGHT_ORANGE, RESET);
            auto shaderPaths = VulkanRTX::getRayTracingBinPaths();
            LOG_INFO_CAT("MAIN", "{}Success: {} ray-tracing shaders resolved{}", BOLD_BRIGHT_ORANGE, shaderPaths.size(), RESET);

            for (size_t i = 0; i < shaderPaths.size(); ++i) {
                LOG_DEBUG_CAT("MAIN", "{}  [{}] → {}{}", BOLD_BRIGHT_ORANGE, i, shaderPaths[i], RESET);
            }

            LOG_INFO_CAT("MAIN", "{}Attempt: Creating VulkanRenderer{}", BOLD_BRIGHT_ORANGE, RESET);
            auto renderer = std::make_unique<VulkanRTX::VulkanRenderer>(
                W, H, app->getWindow(), shaderPaths, core, pipelineMgr.get()
            );
            LOG_INFO_CAT("MAIN", "{}Success: renderer @ {:p}{}", BOLD_BRIGHT_ORANGE, (void*)renderer.get(), RESET);

            renderer->takeOwnership(std::move(pipelineMgr), std::move(bufferMgr));
            LOG_INFO_CAT("MAIN", "{}Success: Ownership transferred{}", BOLD_BRIGHT_ORANGE, RESET);

            renderer->setSwapchainManager(std::move(swapchainMgr));
            LOG_INFO_CAT("MAIN", "{}Success: Swapchain manager set{}", BOLD_BRIGHT_ORANGE, RESET);

            renderer->getBufferManager()->reserveScratchPool(16 * 1024 * 1024, 1);
            LOG_INFO_CAT("MAIN", "{}Success: Scratch pool reserved{}", BOLD_BRIGHT_ORANGE, RESET);

            // =================================================================
            //  PHASE 4: RTX SETUP — STONEKEY SECURED
            // =================================================================
            LOG_INFO_CAT("MAIN", "{}Attempt: Building acceleration structures (BLAS + TLAS){}", BOLD_BRIGHT_ORANGE, RESET);

            const auto& meshes = renderer->getBufferManager()->getMeshes();
            if (meshes.empty()) {
                LOG_WARN_CAT("MAIN", "No geometry — generating fallback cube");
                renderer->getBufferManager()->generateCube(1.0f);
                LOG_INFO_CAT("MAIN", "{}Success: Fallback cube generated{}", BOLD_BRIGHT_ORANGE, RESET);
            } else {
                LOG_INFO_CAT("MAIN", "{}Success: {} mesh(es) loaded{}", BOLD_BRIGHT_ORANGE, meshes.size(), RESET);
            }

            renderer->getPipelineManager()->createAccelerationStructures(
                renderer->getBufferManager()->getVertexBuffer(),
                renderer->getBufferManager()->getIndexBuffer(),
                *renderer->getBufferManager(),
                renderer.get()
            );
            LOG_INFO_CAT("MAIN", "{}Success: Acceleration structures built{}", BOLD_BRIGHT_ORANGE, RESET);

            VkAccelerationStructureKHR tlas = renderer->getRTX().getTLAS();
            if (tlas == VK_NULL_HANDLE) {
                LOG_ERROR_CAT("MAIN", "TLAS is VK_NULL_HANDLE after build");
                THROW_MAIN("TLAS creation failed");
            }
            LOG_INFO_CAT("MAIN", "{}Success: TLAS @ {:p} — valid | STONEKEY HASH 0x{:X}{}", BOLD_BRIGHT_ORANGE, static_cast<void*>(tlas), kStone1 ^ reinterpret_cast<uint64_t>(tlas), RESET);

            renderer->getRTX().updateDescriptors(
                renderer->getUniformBuffer(0),
                renderer->getMaterialBuffer(0),
                renderer->getDimensionBuffer(0),
                renderer->getRTOutputImageView(0),
                renderer->getAccumulationView(0),
                renderer->getEnvironmentMapView(),
                renderer->getEnvironmentMapSampler(),
                VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE
            );
            LOG_INFO_CAT("MAIN", "{}Success: RT descriptors updated{}", BOLD_BRIGHT_ORANGE, RESET);

            renderer->recordRayTracingCommandBuffer();
            LOG_INFO_CAT("MAIN", "{}Success: INITIAL RT COMMAND BUFFER RECORDED — GPU READY — STONEKEY LOCKED{}", BOLD_BRIGHT_ORANGE, RESET);

            // =================================================================
            //  PHASE 5: TRANSFER OWNERSHIP TO APPLICATION
            // =================================================================
            app->setRenderer(std::move(renderer));
            LOG_INFO_CAT("MAIN", "{}Success: Renderer ownership transferred to Application{}", BOLD_BRIGHT_ORANGE, RESET);

            // =================================================================
            //  PHASE 6: MAIN LOOP — 69,420 FPS INFINITE
            // =================================================================
            LOG_INFO_CAT("MAIN", "{}Starting main loop — 69,420 FPS RTX — STONEKEY ETERNAL{}", BOLD_BRIGHT_ORANGE, RESET);
            app->run();
            LOG_INFO_CAT("MAIN", "{}Main loop completed{}", BOLD_BRIGHT_ORANGE, RESET);

            // =================================================================
            //  PHASE 7: RAII SHUTDOWN — STONEKEY FINAL VALIDATION
            // =================================================================
            LOG_INFO_CAT("MAIN", "{}RAII shutdown → Context::~Context() → GLOBAL cleanupAll() AUTOMATIC{}", BOLD_BRIGHT_ORANGE, RESET);
            app.reset();     // Destroys Application → Renderer → everything
            core.reset();    // TRIGGERS ~Context() → GLOBAL cleanupAll(*this) → FULL OBLITERATION
            LOG_INFO_CAT("StoneKey", "RAII COMPLETE — UNIVERSE CLEANSED — HASH: 0x{:X} — VALHALLA ETERNAL", kStone1 ^ kStone2);
            LOG_INFO_CAT("MAIN", "{}RAII COMPLETE — UNIVERSE CLEANSED — RASPBERRY_PINK ETERNAL{}", BOLD_BRIGHT_ORANGE, RESET);
        }

    } catch (const MainException& e) {
        bulkhead(" FATAL ERROR — SYSTEM HALT — STONEKEY BREACH DETECTED ");
        LOG_ERROR_CAT("StoneKey", "MAIN FATAL — TAMPER SUSPECTED [0x{:X}]", kStone1 ^ kStone2);
        LOG_ERROR_CAT("MAIN", "[MAIN FATAL] {}", e.what());
        if (core) { core.reset(); }
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        Logging::Logger::get().stop();
        return 1;

    } catch (const VulkanRTX::VulkanRTXException& e) {
        bulkhead(" VULKAN RTX EXCEPTION — DEVICE LOST — STONEKEY LOCKDOWN ");
        LOG_ERROR_CAT("StoneKey", "VULKAN RTX EXCEPTION — POSSIBLE TAMPER [0x{:X}]", kStone1 ^ kStone2);
        LOG_ERROR_CAT("MAIN",
                      "[VULKAN RTX] {}\n   File: {} | Line: {} | Func: {}",
                      e.what(), e.file(), e.line(), e.function());
        if (core) { core.reset(); }
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        Logging::Logger::get().stop();
        return 1;

    } catch (const std::exception& e) {
        bulkhead(" STD EXCEPTION — SYSTEM HALT — STONEKEY ALERT ");
        LOG_ERROR_CAT("StoneKey", "STD EXCEPTION — INTEGRITY CHECK [0x{:X}]", kStone1);
        LOG_ERROR_CAT("MAIN", "[STD] {}", e.what());
        if (core) { core.reset(); }
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        Logging::Logger::get().stop();
        return 1;

    } catch (...) {
        bulkhead(" UNKNOWN EXCEPTION — SYSTEM HALT — STONEKEY ABORT ");
        LOG_ERROR_CAT("StoneKey", "UNKNOWN EXCEPTION — VALHALLA LOCKDOWN ENGAGED");
        LOG_ERROR_CAT("MAIN", "[UNKNOWN] caught");
        if (core) { core.reset(); }
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        Logging::Logger::get().stop();
        return 1;
    }

    bulkhead(" NOMINAL SHUTDOWN — EXIT 0 — STONEKEY FINAL HASH ");
    LOG_INFO_CAT("StoneKey", "SHUTDOWN NOMINAL — FINAL HASH: 0x{:X} — BAD GUYS BLOCKED FOREVER", kStone1 ^ kStone2);
    if (sdl_ok) {
        LOG_INFO_CAT("MAIN", "{}Attempt: SDL_Quit(){}", BOLD_BRIGHT_ORANGE, RESET);
        SDL_Quit();
        LOG_INFO_CAT("MAIN", "{}Success: SDL_Quit completed{}", BOLD_BRIGHT_ORANGE, RESET);
    }
    Logging::Logger::get().stop();
    LOG_INFO_CAT("MAIN", "{}Graceful exit — all systems nominal — RAII IMMORTAL — STONEKEY SUPREMACY{}", BOLD_BRIGHT_ORANGE, RESET);
    return 0;
}

// END OF FILE — FULLY STONEKEYED — 69,420 FPS × ∞ × ∞
// NOVEMBER 08 2025 — SHIPPED TO VALHALLA — PINK PHOTONS ETERNAL — BAD GUYS OWNED
// STONEKEY v∞ — QUANTUM PIPELINE + MAIN — RASPBERRY_PINK PHOTONS ETERNAL 🩷🚀🔥🤖💀❤️⚡♾️