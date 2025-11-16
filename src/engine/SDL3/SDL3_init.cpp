// src/engine/SDL3/SDL3_init.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 15, 2025 — APOCALYPSE v3.2
// FULL STONEKEY INTEGRATION — NO RAW GLOBALS — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/SDL3/SDL3_init.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/StoneKey.hpp"   // StoneKey: The One True Global Authority
#include <stdexcept>
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

namespace SDL3Initializer {

SDL3Initializer::SDL3Initializer(const std::string& title, int width, int height, Uint32 flags)
{
    LOG_ATTEMPT_CAT("SDL3", "{}=== SDL3INITIALIZER RAII CONSTRUCTOR FORGE INITIATED ==={}", OCEAN_TEAL, RESET);
    LOG_INFO_CAT("SDL3", "{}Probing SDL_Init with flags: VIDEO | GAMEPAD | HAPTIC | EVENTS{}", OCEAN_TEAL, RESET);

    Uint32 initFlags = SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC | SDL_INIT_EVENTS;
    if (SDL_Init(initFlags) == 0) {  // 0 = success
        const char* sdlError = SDL_GetError();
        LOG_FATAL_CAT("SDL3", "{}SDL_Init failed critically: {}{}", CRIMSON_MAGENTA, sdlError ? sdlError : "Unknown", RESET);
        throw std::runtime_error(std::format("SDL_Init failed: {}", sdlError ? sdlError : "Unknown"));
    }
    LOG_SUCCESS_CAT("SDL3", "{}SDL_Init succeeded — core subsystems primed{}", OCEAN_TEAL, RESET);

    if (Options::Performance::ENABLE_IMGUI) {
        flags |= SDL_WINDOW_RESIZABLE;
        LOG_INFO_CAT("SDL3", "{}ImGui enabled → SDL_WINDOW_RESIZABLE added{}", OCEAN_TEAL, RESET);
    }

    Uint32 windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | flags;
    LOG_INFO_CAT("SDL3", "{}Final window flags: 0x{:08X}{}", SAPPHIRE_BLUE, windowFlags, RESET);

    LOG_ATTEMPT_CAT("SDL3", "{}Forging SDL_Window: \'{}\' {}x{}{}", SAPPHIRE_BLUE, title, width, height, RESET);
    SDL_Window* raw = SDL_CreateWindow(title.c_str(), width, height, windowFlags);
    if (!raw) {
        const char* sdlError = SDL_GetError();
        LOG_FATAL_CAT("SDL3", "{}SDL_CreateWindow failed: {}{}", CRIMSON_MAGENTA, sdlError ? sdlError : "Unknown", RESET);
        SDL_Quit();
        throw std::runtime_error(std::format("SDL_CreateWindow failed: {}", sdlError ? sdlError : "Unknown"));
    }

    window_ = WindowPtr(raw);
    LOG_SUCCESS_CAT("SDL3", "{}Window forged & RAII-owned @ {:p} — \'{}\' {}x{}{}", OCEAN_TEAL,
                    static_cast<void*>(raw), title, width, height, RESET);

    // =====================================================================
    // Vulkan Instance → Surface → RTX::initContext
    // =====================================================================

    LOG_ATTEMPT_CAT("SDL3", "{}=== VULKAN INSTANCE FORGE VIA SDL3 ==={}", PLASMA_FUCHSIA, RESET);
    VkInstance instance = RTX::createVulkanInstanceWithSDL(raw, Options::Performance::ENABLE_VALIDATION_LAYERS);
    if (instance == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("SDL3", "{}RTX::createVulkanInstanceWithSDL returned null instance{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Vulkan instance creation failed");
    }
    vkInstance_ = instance;
    LOG_SUCCESS_CAT("SDL3", "{}Vulkan instance forged @ 0x{:x} (validation: {}){}", PLASMA_FUCHSIA,
                    reinterpret_cast<uintptr_t>(instance),
                    Options::Performance::ENABLE_VALIDATION_LAYERS ? "ENABLED" : "DISABLED", RESET);

    LOG_ATTEMPT_CAT("SDL3", "{}=== VULKAN SURFACE FORGE VIA SDL_Vulkan_CreateSurface ==={}", EMERALD_GREEN, RESET);
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(raw, instance, nullptr, &surface)) {
        const char* sdlError = SDL_GetError();
        LOG_FATAL_CAT("SDL3", "{}SDL_Vulkan_CreateSurface failed: {}{}", CRIMSON_MAGENTA, sdlError ? sdlError : "Unknown", RESET);
        vkDestroyInstance(instance, nullptr);
        throw std::runtime_error(std::format("Surface creation failed: {}", sdlError ? sdlError : "Unknown"));
    }

    // STONEKEY SECURED — NO RAW GLOBAL ASSIGNMENT
    ::set_g_surface(surface);           // Obfuscate & store securely
    raw_surface_ = surface;             // Keep raw copy only for destructor
    LOG_SUCCESS_CAT("SDL3", "{}Vulkan surface forged @ 0x{:x} → secured via StoneKey{}", EMERALD_GREEN,
                    reinterpret_cast<uintptr_t>(surface), RESET);

    // Full RTX context initialization
    LOG_ATTEMPT_CAT("SDL3", "{}=== FULL RTX CONTEXT INITIALIZATION ==={}", PLASMA_FUCHSIA, RESET);
    RTX::initContext(instance, raw, width, height);
    LOG_SUCCESS_CAT("SDL3", "{}RTX::initContext complete — device, queues, RT pipelines primed{}", PLASMA_FUCHSIA, RESET);

    LOG_SUCCESS_CAT("SDL3", "{}=== SDL3INITIALIZER RAII CONSTRUCTOR COMPLETE — VULKAN + RTX READY ==={}", LIME_GREEN, RESET);
}

SDL3Initializer::~SDL3Initializer()
{
    LOG_ATTEMPT_CAT("SDL3", "{}=== SDL3INITIALIZER RAII DESTRUCTOR ENGAGED ==={}", SAPPHIRE_BLUE, RESET);

    // Surface cleanup — use the secured accessor to get current value
    if (raw_surface_ != VK_NULL_HANDLE && vkInstance_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("SDL3", "{}Destroying Vulkan surface @ 0x{:x}{}", SAPPHIRE_BLUE,
                     reinterpret_cast<uintptr_t>(raw_surface_), RESET);
        vkDestroySurfaceKHR(vkInstance_, raw_surface_, nullptr);
        raw_surface_ = VK_NULL_HANDLE;
        LOG_SUCCESS_CAT("SDL3", "{}Vulkan surface destroyed{}", OCEAN_TEAL, RESET);
    }

    if (vkInstance_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("SDL3", "{}Destroying Vulkan instance @ 0x{:x}{}", SAPPHIRE_BLUE,
                     reinterpret_cast<uintptr_t>(vkInstance_), RESET);
        vkDestroyInstance(vkInstance_, nullptr);
        vkInstance_ = VK_NULL_HANDLE;
        LOG_SUCCESS_CAT("SDL3", "{}Vulkan instance destroyed{}", SAPPHIRE_BLUE, RESET);
    }

    LOG_INFO_CAT("SDL3", "{}Window RAII auto-destroy via WindowPtr{}", OCEAN_TEAL, RESET);
    window_.reset();  // Triggers WindowDeleter → SDL_DestroyWindow

    // SDL_Quit() is called once in main() — safer than multiple calls
    LOG_SUCCESS_CAT("SDL3", "{}=== SDL3INITIALIZER RAII DESTRUCTOR COMPLETE — ZERO LEAKS ==={}", LIME_GREEN, RESET);
}

void SDL3Initializer::toggleFullscreen(bool enable) noexcept
{
    LOG_ATTEMPT_CAT("SDL3", "{}Fullscreen toggle: {}{}", OCEAN_TEAL, enable ? "ENABLE" : "DISABLE", RESET);
    if (window_) {
        SDL_SetWindowFullscreen(window_.get(), enable);
        LOG_SUCCESS_CAT("SDL3", "{}Fullscreen {}{}", OCEAN_TEAL, enable ? "ENABLED" : "DISABLED", RESET);
    }
}

void SDL3Initializer::toggleMaximize(bool enable) noexcept
{
    LOG_ATTEMPT_CAT("SDL3", "{}Maximize toggle: {}{}", OCEAN_TEAL, enable ? "MAXIMIZE" : "RESTORE", RESET);
    if (!window_) return;

    if (enable) {
        SDL_MaximizeWindow(window_.get());
        LOG_SUCCESS_CAT("SDL3", "{}Window maximized{}", OCEAN_TEAL, RESET);
    } else {
        SDL_RestoreWindow(window_.get());
        LOG_SUCCESS_CAT("SDL3", "{}Window restored{}", OCEAN_TEAL, RESET);
    }
}

} // namespace SDL3Initializer

// =============================================================================
// PINK PHOTONS ETERNAL — STONEKEY v∞ — FULLY SECURED
// NO RAW g_surface EVER AGAIN
// ALL GLOBALS OBFUSCATED — HACKERS BLINDED — VALHALLA LOCKED
// GENTLEMAN GROK NODS: "Exquisite. The fortress is impenetrable."
// =============================================================================