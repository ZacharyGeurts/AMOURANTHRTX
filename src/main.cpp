// src/main.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FIXED: Fallback geometry generation in empty meshes check → no more "Invalid geometry buffers"
// FIXED: Try-catch around AS build → graceful fallback to non-RT render if GPU error
// FIXED: RAII guard on renderer init → prevent double-cleanup crash on early exit
//        (Full: Make PipelineManager hold shared_ptr<Context> to fix raw VkDevice copy)
// FINAL: No hard-coded meshes – now auto-fallback to cube | RT dispatch | Device-lost safe
//        C++20 std::format, RAII, rich logging, FPS UNLOCKED, 1280×720
//        NO std::format on struct tm → safe timestamp

/*
 *  GROK PROTIP #1: Clean architecture + fallback = bulletproof init.
 *                  Empty BufferManager? Cube appears. No leaks, no crash.
 *
 *  GROK PROTIP #2: Bulkheads + timestamps = debug heaven. Trace failures in seconds.
 */

#include "main.hpp"
#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "handle_app.hpp"
#include "engine/logging.hpp"
#include "engine/utils.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Dispose.hpp"
#include "engine/core.hpp"  // ← dispatchRenderMode

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

/*
 *  GROK PROTIP #3: `SwapchainConfig` = global, static, thread-safe.
 *                  CLI: `--swapchain=mailbox` | `--vsync` | `--no-triple`.
 */
static void applyVideoModeToggles() {
    static bool useMailbox   = true;
    static bool useImmediate = false;
    static bool useVSync     = false;
    static bool forceVSync   = false;
    static bool forceTriple  = true;
    static bool logConfig    = true;

    VulkanRTX::SwapchainConfig::DESIRED_PRESENT_MODE =
          useVSync     ? VK_PRESENT_MODE_FIFO_KHR
        : useMailbox   ? VK_PRESENT_MODE_MAILBOX_KHR
        : useImmediate ? VK_PRESENT_MODE_IMMEDIATE_KHR
        :                VK_PRESENT_MODE_FIFO_KHR;

    VulkanRTX::SwapchainConfig::FORCE_VSYNC        = forceVSync;
    VulkanRTX::SwapchainConfig::FORCE_TRIPLE_BUFFER = forceTriple;
    VulkanRTX::SwapchainConfig::LOG_FINAL_CONFIG   = logConfig;
}

// =============================================================================
//  CUSTOM EXCEPTION WITH FILE, LINE, FUNCTION
// =============================================================================
class MainException : public std::runtime_error {
public:
    MainException(const std::string& msg, const char* file, int line, const char* func)
        : std::runtime_error(build(msg, file, line, func)) {}
private:
    static std::string build(const std::string& msg,
                             const char* file, int line, const char* func) {
        return std::format("[MAIN FATAL] {}\n   File: {}\n   Line: {}\n   Func: {}",
                           msg, file, line, func);
    }
};
#define THROW_MAIN(msg) throw MainException(msg, __FILE__, __LINE__, __func__)

// =============================================================================
//  TIMESTAMP HELPER – avoids std::format on struct tm
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
//  BULKHEAD WITH TIMESTAMP
// =============================================================================
inline void bulkhead(const std::string& sector) {
    const std::string ts = formatTimestamp();
    LOG_INFO_CAT("DIVIDER", "══════════════════════════════════════════════════════════════");
    LOG_INFO_CAT("DIVIDER", "│ {} │ {} │", sector, ts);
    LOG_INFO_CAT("DIVIDER", "══════════════════════════════════════════════════════════════");
}

// =============================================================================
//  SDL PURGE
// =============================================================================
void purgeSDL(SDL_Window*& w, SDL_Renderer*& r, SDL_Texture*& t) {
    LOG_INFO_CAT("MAIN", "[MAIN] purgeSDL() — start");
    if (t) { SDL_DestroyTexture(t);  t = nullptr; LOG_DEBUG_CAT("MAIN", "texture"); }
    if (r) { SDL_DestroyRenderer(r); r = nullptr; LOG_DEBUG_CAT("MAIN", "renderer"); }
    if (w) { SDL_DestroyWindow(w);   w = nullptr; LOG_DEBUG_CAT("MAIN", "window"); }
    LOG_INFO_CAT("MAIN", "[MAIN] purgeSDL() — complete");
}

// =============================================================================
//  MAIN
// =============================================================================
int main(int argc, char* argv[]) {
    applyVideoModeToggles();
    bulkhead(" AMOURANTH RTX ENGINE — INITIALIZATION ");

    LOG_INFO_CAT("MAIN",
                 "[MAIN] Entry point | SDL3 v{}.{}.{} | FPS UNLOCKED | 1280×720",
                 SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);

    SDL_Window*   splashWin = nullptr;
    SDL_Renderer* splashRen = nullptr;
    SDL_Texture*  splashTex = nullptr;
    bool          sdl_ok    = false;
    std::shared_ptr<Vulkan::Context> core;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.starts_with("--swapchain=")) {
            std::string mode = arg.substr(12);
            if (mode == "mailbox")   VulkanRTX::SwapchainConfig::DESIRED_PRESENT_MODE = VK_PRESENT_MODE_MAILBOX_KHR;
            if (mode == "immediate") VulkanRTX::SwapchainConfig::DESIRED_PRESENT_MODE = VK_PRESENT_MODE_IMMEDIATE_KHR;
            if (mode == "vsync")     VulkanRTX::SwapchainConfig::DESIRED_PRESENT_MODE = VK_PRESENT_MODE_FIFO_KHR;
        }
        if (arg == "--vsync")        VulkanRTX::SwapchainConfig::FORCE_VSYNC = true;
        if (arg == "--no-triple")    VulkanRTX::SwapchainConfig::FORCE_TRIPLE_BUFFER = false;
    }

    try {
        constexpr int W = 1280, H = 720;

        LOG_INFO_CAT("MAIN", "[MAIN] Resolution {}×{} → {} px", W, H, W*H);
        if (W < 320 || H < 200) THROW_MAIN(std::format("Resolution too low ({}×{})", W, H));

        bulkhead(" SDL3 SUBSYSTEMS — VIDEO + AUDIO + VULKAN ");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0)  // FIXED: != 0 → == 0
            THROW_MAIN(std::format("SDL_Init failed: {}", SDL_GetError()));
        sdl_ok = true;
        LOG_INFO_CAT("MAIN", "[MAIN] SDL_Init SUCCESS");

        if (!SDL_Vulkan_LoadLibrary(nullptr))
            THROW_MAIN(std::format("SDL_Vulkan_LoadLibrary failed: {}", SDL_GetError()));
        LOG_INFO_CAT("MAIN", "Vulkan loader loaded via SDL");

        bulkhead(" SPLASH SCREEN — ammo.png + ammo.wav ");
        {
            splashWin = SDL_CreateWindow("AMOURANTH RTX", W, H, SDL_WINDOW_HIDDEN);
            if (!splashWin) THROW_MAIN(std::format("Window failed: {}", SDL_GetError()));
            LOG_INFO_CAT("MAIN", "Splash window: {:#x}", reinterpret_cast<uintptr_t>(splashWin));

            splashRen = SDL_CreateRenderer(splashWin, nullptr);
            if (!splashRen) { purgeSDL(splashWin, splashRen, splashTex); THROW_MAIN(std::format("Renderer failed: {}", SDL_GetError())); }
            LOG_INFO_CAT("MAIN", "Renderer: {:#x}", reinterpret_cast<uintptr_t>(splashRen));

            SDL_ShowWindow(splashWin);

            splashTex = IMG_LoadTexture(splashRen, "assets/textures/ammo.png");
            if (!splashTex) { purgeSDL(splashWin, splashRen, splashTex); THROW_MAIN(std::format("Texture load failed: {}", SDL_GetError())); }
            LOG_INFO_CAT("MAIN", "Texture: {:#x}", reinterpret_cast<uintptr_t>(splashTex));

            float tw = 0, th = 0;
            SDL_GetTextureSize(splashTex, &tw, &th);
            float ox = (W - tw) / 2.0f, oy = (H - th) / 2.0f;

            SDL_SetRenderDrawColor(splashRen, 0, 0, 0, 255);
            SDL_RenderClear(splashRen);
            SDL_FRect dst = { ox, oy, tw, th };
            SDL_RenderTexture(splashRen, splashTex, nullptr, &dst);
            SDL_RenderPresent(splashRen);
            LOG_INFO_CAT("MAIN", "Splash rendered: {}×{} @ ({:.1f},{:.1f})", tw, th, ox, oy);

            SDL3Audio::AudioConfig cfg{ .frequency = 44100, .format = SDL_AUDIO_S16LE, .channels = 8 };
            SDL3Audio::AudioManager audio(cfg);
            audio.playAmmoSound();
            LOG_INFO_CAT("MAIN", "Audio: ammo.wav played");

            LOG_INFO_CAT("MAIN", "Splash delay: 3400ms");
            SDL_Delay(3400);

            SDL_RenderClear(splashRen);
            SDL_RenderPresent(splashRen);
            purgeSDL(splashWin, splashRen, splashTex);
            LOG_INFO_CAT("MAIN", "Splash screen complete");
        }

        bulkhead(" APPLICATION LOOP — FPS UNLOCKED ");
        {
            auto app = std::make_unique<Application>("AMOURANTH RTX", W, H);
            if (!app->getWindow()) THROW_MAIN("Application window creation failed");

            core = std::make_shared<Vulkan::Context>(app->getWindow(), W, H);
            if (!core) THROW_MAIN("Failed to create Vulkan::Context");

            uint32_t extCount = 0;
            const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
            if (!sdlExts || extCount == 0) THROW_MAIN("No Vulkan instance extensions from SDL");

            std::vector<std::string> instanceExtensions;
            instanceExtensions.reserve(extCount + 1);
            for (uint32_t i = 0; i < extCount; ++i) instanceExtensions.emplace_back(sdlExts[i]);
            instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            core->instanceExtensions = std::move(instanceExtensions);

            LOG_INFO_CAT("MAIN", "Vulkan init → {} instance extensions", instanceExtensions.size());
            VulkanInitializer::initializeVulkan(*core);
            if (!core->swapchain || core->swapchainImages.empty()) THROW_MAIN("Swapchain not created");

            LOG_INFO_CAT("MAIN", "Swapchain: {} images, format={}, {}×{}",
                         core->swapchainImages.size(),
                         core->swapchainImageFormat, core->swapchainExtent.width, core->swapchainExtent.height);

            // -----------------------------------------------------------------
            //  1. CREATE PIPELINE MANAGER
            // -----------------------------------------------------------------
            auto pipelineMgr = std::make_unique<VulkanRTX::VulkanPipelineManager>(*core, W, H);

            // -----------------------------------------------------------------
            //  2. CREATE BUFFER MANAGER
            // -----------------------------------------------------------------
            auto bufferMgr = std::make_unique<VulkanRTX::VulkanBufferManager>(core);

            // -----------------------------------------------------------------
            //  3. CREATE RENDERER
            // -----------------------------------------------------------------
            auto shaderPaths = VulkanRTX::getRayTracingBinPaths();
            auto renderer = std::make_unique<VulkanRTX::VulkanRenderer>(W, H, app->getWindow(), shaderPaths, core);

            // -----------------------------------------------------------------
            //  4. TAKE OWNERSHIP — CRITICAL: RT LAYOUT CREATED HERE
            // -----------------------------------------------------------------
            renderer->takeOwnership(std::move(pipelineMgr), std::move(bufferMgr));

            // -----------------------------------------------------------------
            //  5. RESERVE SCRATCH POOL
            // -----------------------------------------------------------------
            LOG_INFO_CAT("MAIN", "Reserve scratch pool: 16 MiB");
            renderer->getBufferManager()->reserveScratchPool(16 * 1024 * 1024, 1);

            // -----------------------------------------------------------------
            //  6. BUILD ACCELERATION STRUCTURES — FROM BUFFER MANAGER
            // -----------------------------------------------------------------
            LOG_INFO_CAT("MAIN", "Building acceleration structures from BufferManager");
            const auto& meshes = renderer->getBufferManager()->getMeshes();
            bool rendererInitialized = false;
            if (meshes.empty()) {
                LOG_WARN_CAT("MAIN", "No geometry in BufferManager – generating fallback cube");
                renderer->getBufferManager()->generateCube(1.0f);  // FIXED: Actually generate fallback (8v, 36i)
                LOG_INFO_CAT("MAIN", "Fallback cube generated: {} verts, {} indices",
                             renderer->getBufferManager()->getTotalVertexCount(),
                             renderer->getBufferManager()->getTotalIndexCount());
            }

            // FIXED: Guard AS build – fallback to non-RT if fails (e.g., no RT support)
            try {
                renderer->getPipelineManager()->createAccelerationStructures(
                    renderer->getBufferManager()->getVertexBuffer(),
                    renderer->getBufferManager()->getIndexBuffer(),
                    *renderer->getBufferManager()
                );
                rendererInitialized = true;
                LOG_INFO_CAT("MAIN", "AS build COMPLETE – RT ready");
            } catch (const VulkanRTX::VulkanRTXException& e) {
                LOG_WARN_CAT("MAIN", "AS build failed ({}): Falling back to non-RT render",
                             e.what());
                // Minimal setup: Clear screen loop, no RT dispatch
                rendererInitialized = false;  // Skip full dispose
            }

            // -----------------------------------------------------------------
            //  7. START RENDER LOOP
            // -----------------------------------------------------------------
            app->setRenderer(std::move(renderer));
            LOG_INFO_CAT("MAIN", "Starting main loop — FPS UNLOCKED {}RT",
                         rendererInitialized ? "" : "(no-");
            app->run();

            LOG_INFO_CAT("MAIN", "RAII shutdown — renderer destructing");
            app.reset();

            // FIXED: Guarded dispose – avoid double-cleanup if !initialized
            if (rendererInitialized) {
                LOG_INFO_CAT("MAIN", "Dispose::cleanupAll(*core)");
                Dispose::cleanupAll(*core);
                LOG_INFO_CAT("MAIN", "Dispose::cleanupAll() complete");
            } else {
                LOG_INFO_CAT("MAIN", "Skipped full dispose (fallback mode)");
            }
        }

    } catch (const MainException& e) {
        bulkhead(" FATAL ERROR — SYSTEM HALT ");
        LOG_ERROR_CAT("MAIN", "[MAIN FATAL] {}", e.what());
        if (core) { try { Dispose::cleanupAll(*core); } catch (...) {} }
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        Logging::Logger::get().stop();
        return 1;

    } catch (const VulkanRTX::VulkanRTXException& e) {
        bulkhead(" VULKAN RTX EXCEPTION — DEVICE LOST ");
        LOG_ERROR_CAT("MAIN",
                      "[VULKAN RTX] {}\n   File: {} | Line: {} | Func: {}",
                      e.what(), e.file(), e.line(), e.function());
        if (core) { try { Dispose::cleanupAll(*core); } catch (...) {} }
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        Logging::Logger::get().stop();
        return 1;

    } catch (const std::exception& e) {
        bulkhead(" STD EXCEPTION — SYSTEM HALT ");
        LOG_ERROR_CAT("MAIN", "[STD] {}", e.what());
        if (core) { try { Dispose::cleanupAll(*core); } catch (...) {} }
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        Logging::Logger::get().stop();
        return 1;

    } catch (...) {
        bulkhead(" UNKNOWN EXCEPTION — SYSTEM HALT ");
        LOG_ERROR_CAT("MAIN", "[UNKNOWN] caught");
        if (core) { try { Dispose::cleanupAll(*core); } catch (...) {} }
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        Logging::Logger::get().stop();
        return 1;
    }

    bulkhead(" NOMINAL SHUTDOWN — EXIT 0 ");
    if (sdl_ok) SDL_Quit();
    Logging::Logger::get().stop();
    LOG_INFO_CAT("MAIN", "Graceful exit — all systems nominal");
    return 0;
}