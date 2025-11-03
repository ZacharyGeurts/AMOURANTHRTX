// src/main.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: Device-lost cleanup path – Dispose::cleanupAll(*core)
//        VulkanRTXException with file/line/function
//        C++20 std::format, RAII, rich logging, FPS UNLOCKED, 1280×720
//        NO std::format on struct tm → uses safe timestamp helper

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

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <algorithm>
#include <memory>
#include <format>
#include <cstdio>

using namespace Logging::Color;
using VulkanRTX::VulkanRTXException;

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
    localtime_r(&time_t, &local);  // POSIX thread-safe
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03ld",
              local.tm_hour, local.tm_min, local.tm_sec, ms.count());
    return std::string(buffer);
}

// =============================================================================
//  BULKHEAD WITH TIMESTAMP
// =============================================================================
inline void bulkhead(const std::string& sector) {
    const std::string ts = formatTimestamp();
    LOG_INFO_CAT("DIVIDER", "{}══════════════════════════════════════════════════════════════{}", DIAMOND_WHITE, RESET);
    LOG_INFO_CAT("DIVIDER", "{}│ {} │ {} │{}", DIAMOND_WHITE, sector, ts, RESET);
    LOG_INFO_CAT("DIVIDER", "{}══════════════════════════════════════════════════════════════{}", DIAMOND_WHITE, RESET);
}

// =============================================================================
//  SDL PURGE
// =============================================================================
void purgeSDL(SDL_Window*& w, SDL_Renderer*& r, SDL_Texture*& t) {
    LOG_INFO_CAT("MAIN", "{}[MAIN] purgeSDL() — start{}", EMERALD_GREEN, RESET);
    if (t) { SDL_DestroyTexture(t);  t = nullptr; LOG_DEBUG_CAT("MAIN", "{}texture{}", AMBER_YELLOW, RESET); }
    if (r) { SDL_DestroyRenderer(r); r = nullptr; LOG_DEBUG_CAT("MAIN", "{}renderer{}", AMBER_YELLOW, RESET); }
    if (w) { SDL_DestroyWindow(w);   w = nullptr; LOG_DEBUG_CAT("MAIN", "{}window{}",   AMBER_YELLOW, RESET); }
    LOG_INFO_CAT("MAIN", "{}[MAIN] purgeSDL() — complete{}", EMERALD_GREEN, RESET);
}

// =============================================================================
//  CUBE MESH DATA
// =============================================================================
static const glm::vec3 cubeVertices[] = {
    {-0.5f,-0.5f, 0.5f},{ 0.5f,-0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{-0.5f, 0.5f, 0.5f},
    {-0.5f,-0.5f,-0.5f},{ 0.5f,-0.5f,-0.5f},{ 0.5f, 0.5f,-0.5f},{-0.5f, 0.5f,-0.5f}
};
static const uint32_t cubeIndices[] = {
    0,1,2,2,3,0,4,5,6,6,7,4,0,4,7,7,3,0,
    1,5,6,6,2,1,0,1,5,5,4,0,3,2,6,6,7,3
};

// =============================================================================
//  MAIN
// =============================================================================
int main() {
    bulkhead(" AMOURANTH RTX ENGINE — INITIALIZATION ");

    LOG_INFO_CAT("MAIN",
                 "{}[MAIN] Entry point{} | {}SDL3 v{}.{}.{}{} | {}FPS UNLOCKED{} | {}1280×720{}",
                 EMERALD_GREEN, RESET,
                 ARCTIC_CYAN, SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION, RESET,
                 CRIMSON_MAGENTA, RESET,
                 OCEAN_TEAL, RESET);

    SDL_Window*   splashWin = nullptr;
    SDL_Renderer* splashRen = nullptr;
    SDL_Texture*  splashTex = nullptr;
    bool          sdl_ok    = false;
    std::shared_ptr<Vulkan::Context> core;

    try {
        constexpr int W = 1280, H = 720;

        LOG_INFO_CAT("MAIN", "{}[MAIN] Resolution {}×{} → {} px{}", OCEAN_TEAL, W, H, W*H, RESET);
        if (W < 320 || H < 200) THROW_MAIN(std::format("Resolution too low ({}×{})", W, H));

        bulkhead(" SDL3 SUBSYSTEMS — VIDEO + AUDIO + VULKAN ");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0)
            THROW_MAIN(std::format("SDL_Init failed: {}", SDL_GetError()));
        sdl_ok = true;
        LOG_INFO_CAT("MAIN", "{}SDL_Init SUCCESS{}", EMERALD_GREEN, RESET);

        if (!SDL_Vulkan_LoadLibrary(nullptr))
            THROW_MAIN(std::format("SDL_Vulkan_LoadLibrary failed: {}", SDL_GetError()));
        LOG_INFO_CAT("MAIN", "{}Vulkan loader loaded via SDL{}", EMERALD_GREEN, RESET);

        bulkhead(" SPLASH SCREEN — ammo.png + ammo.wav ");
        {
            splashWin = SDL_CreateWindow("AMOURANTH RTX", W, H, SDL_WINDOW_HIDDEN);
            if (!splashWin) THROW_MAIN(std::format("Window failed: {}", SDL_GetError()));
            LOG_INFO_CAT("MAIN", "{}Splash window: {:#x}{}", EMERALD_GREEN, reinterpret_cast<uintptr_t>(splashWin), RESET);

            splashRen = SDL_CreateRenderer(splashWin, nullptr);
            if (!splashRen) { purgeSDL(splashWin, splashRen, splashTex); THROW_MAIN(std::format("Renderer failed: {}", SDL_GetError())); }
            LOG_INFO_CAT("MAIN", "{}Renderer: {:#x}{}", EMERALD_GREEN, reinterpret_cast<uintptr_t>(splashRen), RESET);

            SDL_ShowWindow(splashWin);

            splashTex = IMG_LoadTexture(splashRen, "assets/textures/ammo.png");
            if (!splashTex) { purgeSDL(splashWin, splashRen, splashTex); THROW_MAIN(std::format("Texture load failed: {}", SDL_GetError())); }
            LOG_INFO_CAT("MAIN", "{}Texture: {:#x}{}", EMERALD_GREEN, reinterpret_cast<uintptr_t>(splashTex), RESET);

            float tw = 0, th = 0;
            SDL_GetTextureSize(splashTex, &tw, &th);
            float ox = (W - tw) / 2.0f, oy = (H - th) / 2.0f;

            SDL_SetRenderDrawColor(splashRen, 0, 0, 0, 255);
            SDL_RenderClear(splashRen);
            SDL_FRect dst = { ox, oy, tw, th };
            SDL_RenderTexture(splashRen, splashTex, nullptr, &dst);
            SDL_RenderPresent(splashRen);
            LOG_INFO_CAT("MAIN", "{}Splash rendered: {}×{} @ ({:.1f},{:.1f}){}", EMERALD_GREEN, tw, th, ox, oy, RESET);

            SDL3Audio::AudioConfig cfg{ .frequency = 44100, .format = SDL_AUDIO_S16LE, .channels = 8 };
            SDL3Audio::AudioManager audio(cfg);
            audio.playAmmoSound();
            LOG_INFO_CAT("MAIN", "{}Audio: ammo.wav played{}", EMERALD_GREEN, RESET);

            LOG_INFO_CAT("MAIN", "{}Splash delay: 3400ms{}", OCEAN_TEAL, RESET);
            SDL_Delay(3400);

            SDL_RenderClear(splashRen);
            SDL_RenderPresent(splashRen);
            purgeSDL(splashWin, splashRen, splashTex);
            LOG_INFO_CAT("MAIN", "{}Splash screen complete{}", EMERALD_GREEN, RESET);
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

            LOG_INFO_CAT("MAIN", "{}Vulkan init → {} instance extensions{}", ARCTIC_CYAN, instanceExtensions.size(), RESET);
            VulkanInitializer::initializeVulkan(*core);
            if (!core->swapchain || core->swapchainImages.empty()) THROW_MAIN("Swapchain not created");

            LOG_INFO_CAT("MAIN", "{}Swapchain: {} images, format={}, {}×{}{}",
                         EMERALD_GREEN, core->swapchainImages.size(),
                         core->swapchainImageFormat, core->swapchainExtent.width, core->swapchainExtent.height, RESET);

            auto pipelineMgr = std::make_unique<VulkanRTX::VulkanPipelineManager>(*core, W, H);
            auto bufferMgr   = std::make_unique<VulkanRTX::VulkanBufferManager>(*core);
            auto shaderPaths = VulkanRTX::getRayTracingBinPaths();

            auto renderer = std::make_unique<VulkanRTX::VulkanRenderer>(W, H, app->getWindow(), shaderPaths, core);
            renderer->takeOwnership(std::move(pipelineMgr), std::move(bufferMgr));

            LOG_INFO_CAT("MAIN", "{}Uploading cube: {} verts, {} indices{}", OCEAN_TEAL,
                         std::size(cubeVertices), std::size(cubeIndices), RESET);
            renderer->getBufferManager()->uploadMesh(cubeVertices, std::size(cubeVertices), cubeIndices, std::size(cubeIndices));
            renderer->getBufferManager()->reserveScratchPool(16 * 1024 * 1024, 1);

            LOG_INFO_CAT("MAIN", "{}Building acceleration structures{}", ARCTIC_CYAN, RESET);
            renderer->getPipelineManager()->createAccelerationStructures(
                renderer->getBufferManager()->getVertexBuffer(),
                renderer->getBufferManager()->getIndexBuffer(),
                *renderer->getBufferManager());

            // Pipeline and SBT creation moved to VulkanRenderer init to avoid duplication

            app->setRenderer(std::move(renderer));
            LOG_INFO_CAT("MAIN", "{}Starting main loop — FPS UNLOCKED{}", EMERALD_GREEN, RESET);
            app->run();

            LOG_INFO_CAT("MAIN", "{}RAII shutdown — renderer destructing{}", ARCTIC_CYAN, RESET);
            app.reset();

            LOG_INFO_CAT("MAIN", "{}Dispose::cleanupAll(*core){}", ARCTIC_CYAN, RESET);
            Dispose::cleanupAll(*core);  // normal path
            LOG_INFO_CAT("MAIN", "{}Dispose::cleanupAll() complete{}", EMERALD_GREEN, RESET);
        }

    } catch (const MainException& e) {
        bulkhead(" FATAL ERROR — SYSTEM HALT ");
        LOG_ERROR_CAT("MAIN", "{}[MAIN FATAL] {}{}", CRIMSON_MAGENTA, e.what(), RESET);
        if (core) { try { Dispose::cleanupAll(*core); } catch (...) {} }
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        Logging::Logger::get().stop();
        return 1;

    } catch (const VulkanRTXException& e) {
        bulkhead(" VULKAN RTX EXCEPTION — DEVICE LOST ");
        LOG_ERROR_CAT("MAIN",
                      "{}[VULKAN RTX] {}\n   File: {} | Line: {} | Func: {}{}",
                      CRIMSON_MAGENTA, e.what(), e.file(), e.line(), e.function(), RESET);
        if (core) { try { Dispose::cleanupAll(*core); } catch (...) {} }
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        Logging::Logger::get().stop();
        return 1;

    } catch (const std::exception& e) {
        bulkhead(" STD EXCEPTION — SYSTEM HALT ");
        LOG_ERROR_CAT("MAIN", "{}[STD] {}{}", CRIMSON_MAGENTA, e.what(), RESET);
        if (core) { try { Dispose::cleanupAll(*core); } catch (...) {} }
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        Logging::Logger::get().stop();
        return 1;

    } catch (...) {
        bulkhead(" UNKNOWN EXCEPTION — SYSTEM HALT ");
        LOG_ERROR_CAT("MAIN", "{}[UNKNOWN] caught{}", CRIMSON_MAGENTA, RESET);
        if (core) { try { Dispose::cleanupAll(*core); } catch (...) {} }
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        Logging::Logger::get().stop();
        return 1;
    }

    bulkhead(" NOMINAL SHUTDOWN — EXIT 0 ");
    if (sdl_ok) SDL_Quit();
    Logging::Logger::get().stop();
    LOG_INFO_CAT("MAIN", "{}Graceful exit — all systems nominal{}", EMERALD_GREEN, RESET);
    return 0;
}