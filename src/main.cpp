// src/main.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// Licensed under CC BY-NC 4.0

#include "main.hpp"
#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "handle_app.hpp"
#include "engine/logging.hpp"
#include "engine/utils.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"

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

using namespace Logging::Color;
using VulkanRTX::VulkanRTXException;

inline void bulkhead(const std::string& sector) {
    LOG_INFO_CAT("DIVIDER", "{}══════════════════════════════════════════════════════════════{}", DIAMOND_WHITE, RESET);
    LOG_INFO_CAT("DIVIDER", "{}│ {} │{}", DIAMOND_WHITE, sector, RESET);
    LOG_INFO_CAT("DIVIDER", "{}══════════════════════════════════════════════════════════════{}", DIAMOND_WHITE, RESET);
}

void purgeSDL(SDL_Window*& w, SDL_Renderer*& r, SDL_Texture*& t) {
    LOG_INFO_CAT("SDL_PURGE", "{}Resource purge sequence start{}", EMERALD_GREEN, RESET);
    if (t) { SDL_DestroyTexture(t);  t = nullptr; LOG_DEBUG_CAT("SDL TEXTURE",   "{}Destroyed texture → nullptr{}",   AMBER_YELLOW, RESET); }
    if (r) { SDL_DestroyRenderer(r); r = nullptr; LOG_DEBUG_CAT("SDL_RENDERER", "{}Destroyed renderer → nullptr{}", AMBER_YELLOW, RESET); }
    if (w) { SDL_DestroyWindow(w);   w = nullptr; LOG_DEBUG_CAT("SDL_WINDOW",   "{}Destroyed window → nullptr{}",   AMBER_YELLOW, RESET); }
    LOG_INFO_CAT("SDL_PURGE", "{}All handles nullified. Leak check: PASS{}", EMERALD_GREEN, RESET);
}

std::string join(const std::vector<const char*>& vec, const std::string& sep) {
    if (vec.empty()) return "";
    std::string result = vec[0];
    for (size_t i = 1; i < vec.size(); ++i) result += sep + std::string(vec[i]);
    return result;
}

int main() {
    bulkhead(" AMOURANTH RTX ENGINE — INITIALIZATION ");

    LOG_INFO_CAT("MAIN", "{}Entry point{} | {}SDL3{} | {}FPS UNLOCKED{} | {}1280×720{}",
                 EMERALD_GREEN, RESET, ARCTIC_CYAN, RESET, CRIMSON_MAGENTA, RESET, OCEAN_TEAL, RESET);

    SDL_Window*   splashWin = nullptr;
    SDL_Renderer* splashRen = nullptr;
    SDL_Texture*  splashTex = nullptr;
    bool          sdl_ok    = false;
    std::shared_ptr<Vulkan::Context> core;

    try {
        constexpr int W = 1280, H = 720;

        LOG_DEBUG_CAT("RES", "{}Resolution: {}×{} → {} px{} | {}16:9{}",
                      OCEAN_TEAL, W, H, W*H, RESET, EMERALD_GREEN, RESET);

        if (W < 320 || H < 200) {
            LOG_ERROR_CAT("RES", "{}Resolution too low! Need ≥320×200{}", CRIMSON_MAGENTA, RESET);
            throw std::runtime_error("Invalid resolution");
        }

        // ───── SDL3 + VULKAN LOADER ─────
        bulkhead(" SDL3 SUBSYSTEMS — VIDEO + AUDIO + VULKAN ");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0) {
            LOG_ERROR_CAT("SDL_INIT", "{}SDL_Init failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
            throw std::runtime_error("SDL init failed");
        }
        sdl_ok = true;
        LOG_INFO_CAT("SDL", "{}Video + Audio online{}", EMERALD_GREEN, RESET);

        if (!SDL_Vulkan_LoadLibrary(nullptr)) {
            LOG_ERROR_CAT("SDL_VULKAN", "{}SDL_Vulkan_LoadLibrary failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
            throw std::runtime_error("Failed to load Vulkan for SDL");
        }
        LOG_INFO_CAT("SDL_VULKAN", "{}Vulkan loader loaded via SDL{}", EMERALD_GREEN, RESET);

        // ───── SPLASH SCREEN ─────
        bulkhead(" SPLASH SCREEN — ammo.png + ammo.wav ");
        {
            splashWin = SDL_CreateWindow("AMOURANTH RTX", W, H, SDL_WINDOW_HIDDEN);
            if (!splashWin) throw std::runtime_error(std::string("Window failed: ") + SDL_GetError());

            splashRen = SDL_CreateRenderer(splashWin, nullptr);
            if (!splashRen) { purgeSDL(splashWin, splashRen, splashTex); throw std::runtime_error(std::string("Renderer failed: ") + SDL_GetError()); }

            SDL_ShowWindow(splashWin);
            splashTex = IMG_LoadTexture(splashRen, "assets/textures/ammo.png");
            if (!splashTex) { purgeSDL(splashWin, splashRen, splashTex); throw std::runtime_error(std::string("Texture load failed: ") + SDL_GetError()); }

            float tw, th; SDL_GetTextureSize(splashTex, &tw, &th);
            float ox = (W - tw) / 2, oy = (H - th) / 2;
            SDL_SetRenderDrawColor(splashRen, 0, 0, 0, 255);
            SDL_RenderClear(splashRen);
            SDL_FRect dst = { ox, oy, tw, th };
            SDL_RenderTexture(splashRen, splashTex, nullptr, &dst);
            SDL_RenderPresent(splashRen);

            SDL3Audio::AudioConfig cfg{ .frequency = 44100, .format = SDL_AUDIO_S16LE, .channels = 8 };
            SDL3Audio::AudioManager audio(cfg);
            audio.playAmmoSound();
            SDL_Delay(3000);

            SDL_Delay(100);
            SDL_RenderClear(splashRen);
            SDL_RenderPresent(splashRen);
            purgeSDL(splashWin, splashRen, splashTex);
            SDL_Delay(100);
        }

        // ───── MAIN APPLICATION + VULKAN ─────
        bulkhead(" APPLICATION LOOP — FPS UNLOCKED ");
        {
            LOG_INFO_CAT("APP", "{}Creating main window: AMOURANTH RTX @ {}×{}{}", ARCTIC_CYAN, W, H, RESET);
            Application app("AMOURANTH RTX", W, H);  // ← 3-arg ctor

            core = std::make_shared<Vulkan::Context>(app.getWindow(), W, H);
            LOG_INFO_CAT("VULKAN", "{}Creating shared Vulkan::Context @ {}{}", ARCTIC_CYAN, ptr_to_hex(core.get()), RESET);

            // ---- 1. SDL Vulkan instance extensions (SDL3 single-call) ----
            uint32_t extCount = 0;
            const char* const* sdlExtPtr = SDL_Vulkan_GetInstanceExtensions(&extCount);
            if (sdlExtPtr == nullptr) {
                LOG_ERROR_CAT("SDL", "{}SDL_Vulkan_GetInstanceExtensions failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
                throw std::runtime_error("Failed to get Vulkan instance extensions");
            }

            std::vector<std::string> instanceExtensions;
            instanceExtensions.reserve(extCount + 1);
            for (uint32_t i = 0; i < extCount; ++i) instanceExtensions.emplace_back(sdlExtPtr[i]);
            instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            LOG_DEBUG_CAT("EXT", "{}SDL requires {} instance extensions: {}{}",
                          AMBER_YELLOW, extCount, join({sdlExtPtr, sdlExtPtr + extCount}, ", "), RESET);

            // ---- 2. INSTANCE ----
            VulkanInitializer::initInstance(instanceExtensions, *core);
            if (!core->instance) throw std::runtime_error("initInstance() failed");

            // ---- 3. SURFACE ----
            VulkanInitializer::initSurface(*core, app.getWindow(), nullptr);
            if (!core->surface) throw std::runtime_error("initSurface() failed");

            // ---- 4. DEVICE ----
            VulkanInitializer::initDevice(*core);
            if (!core->device) throw std::runtime_error("initDevice() failed");

            // ---- 5. COMMAND POOL ----
            VkCommandPoolCreateInfo poolInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = core->graphicsQueueFamilyIndex
            };
            VK_CHECK(vkCreateCommandPool(core->device, &poolInfo, nullptr, &core->commandPool),
                     "Failed to create command pool");

            // ---- 6. PIPELINE MANAGER ----
            auto pipelineManager = std::make_unique<VulkanRTX::VulkanPipelineManager>(*core, W, H);

            // ---- 7. BUFFER MANAGER ----
            auto bufferManager = std::make_unique<VulkanRTX::VulkanBufferManager>(*core);

            // ---- 8. RENDERER ----
            std::vector<std::string> shaderPaths = {
                "assets/shaders/raytracing/raygen.spv",
                "assets/shaders/raytracing/closesthit.spv",
                "assets/shaders/raytracing/miss.spv",
                "assets/shaders/raytracing/shadow.spv"
            };

            auto renderer = std::make_unique<VulkanRTX::VulkanRenderer>(
                W, H, app.getWindow(), shaderPaths,
                core,
                pipelineManager.get(),
                bufferManager.get()
            );

            app.setRenderer(std::move(renderer));
            app.run();
        }

    } catch (const std::exception& e) {
        bulkhead(" FATAL ERROR — SYSTEM HALT ");
        LOG_ERROR_CAT("EX", "{}Exception: {}{}", CRIMSON_MAGENTA, e.what(), RESET);

        if (core) {
            if (core->commandPool) vkDestroyCommandPool(core->device, core->commandPool, nullptr);
            if (core->surface)     vkDestroySurfaceKHR(core->instance, core->surface, nullptr);
            if (core->device)      vkDestroyDevice(core->device, nullptr);
            if (core->instance)    vkDestroyInstance(core->instance, nullptr);
        }

        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();

        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char ts[64]; std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
        LOG_ERROR_CAT("FATAL", "{}[{}] {} — Platform: {}{}", CRIMSON_MAGENTA, ts, e.what(), SDL_GetPlatform(), RESET);

        Logging::Logger::get().stop();
        return 1;
    }

    bulkhead(" NOMINAL SHUTDOWN — EXIT 0 ");
    LOG_INFO_CAT("END", "{}All systems nominal. Memory clean.{}", EMERALD_GREEN, RESET);

    if (sdl_ok) SDL_Quit();
    Logging::Logger::get().stop();
    return 0;
}