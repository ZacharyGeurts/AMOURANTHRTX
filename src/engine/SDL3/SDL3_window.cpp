// =============================================================================
// SDL3_window.cpp — FINAL SDL3 FIX — NOV 13 2025
// • Fixed SDL_Vulkan_GetInstanceExtensions (SDL3 signature is different!)
// • No centering, raw OS position
// • PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <format>
#include <vector>

using namespace Logging::Color;

namespace SDL3Initializer {

SDLWindowPtr g_sdl3_window = nullptr;

void SDLWindowDeleter::operator()(SDL_Window* w) const noexcept {
    if (w) {
        LOG_INFO_CAT("Dispose", "{}Destroying SDL_Window @ {:p}{}", 
                     RASPBERRY_PINK, static_cast<void*>(w), RESET);
        SDL_DestroyWindow(w);
    }
    SDL_Quit();
    LOG_SUCCESS_CAT("Dispose", "{}SDL3 subsystem shutdown complete{}", EMERALD_GREEN, RESET);
}

SDLWindowPtr createWindow(const char* title, int w, int h, Uint32 flags) {
    LOG_ATTEMPT_CAT("Window", "Creating SDL window: '{}' ({}x{}) — NO CENTERING", title, w, h);

    flags |= SDL_WINDOW_VULKAN;

    if (Options::Performance::ENABLE_IMGUI) {
        flags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SDL", "{}SDL_Init failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("SDL_Init failed");
    }

    SDL_Window* raw = SDL_CreateWindow(title, w, h, flags);
    if (!raw) {
        LOG_FATAL_CAT("SDL", "{}SDL_CreateWindow failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    int x, y;
    SDL_GetWindowPosition(raw, &x, &y);
    LOG_INFO_CAT("Window", "Window created @ OS default position: ({}, {})", x, y);

    LOG_SUCCESS_CAT("Window", "{}SDL_Window created — RAW OS POSITION{}", PLASMA_FUCHSIA, RESET);
    LOG_INFO_CAT("Window", "  Handle: {:p}", static_cast<void*>(raw));
    LOG_INFO_CAT("Window", "  Size:   {}x{}", w, h);
    LOG_INFO_CAT("Window", "  Flags:  0x{:08x}", SDL_GetWindowFlags(raw));

    int pw, ph;
    SDL_GetWindowSizeInPixels(raw, &pw, &ph);
    if (pw != w || ph != h) {
        LOG_INFO_CAT("Window", "  High DPI: {}x{} → {}x{} (scale: {:.2f})", w, h, pw, ph, static_cast<float>(pw)/w);
    }

    setSDL3Window(SDLWindowPtr(raw));
    LOG_SUCCESS_CAT("SDL3", "{}g_sdl3_window assigned — GLOBAL READY{}", COSMIC_GOLD, RESET);
    return SDLWindowPtr(raw);
}

// =============================================================================
// FIXED: SDL3's Vulkan extension function has a DIFFERENT signature!
// =============================================================================
std::vector<std::string> getWindowExtensions(const SDLWindowPtr&) {
    Uint32 count = 0;

    // First call: get count only
    if (!SDL_Vulkan_GetInstanceExtensions(&count)) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_GetInstanceExtensions failed (count): {}", SDL_GetError());
        return {};
    }

    LOG_INFO_CAT("Vulkan", "SDL requires {} Vulkan instance extensions", count);

    // Second call: get actual names
    std::vector<const char*> names(count);
    if (SDL_Vulkan_GetInstanceExtensions(&count) == 0) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_GetInstanceExtensions failed (names): {}", SDL_GetError());
        return {};
    }

    std::vector<std::string> result;
    result.reserve(count);
    for (Uint32 i = 0; i < count; ++i) {
        result.emplace_back(names[i]);
        LOG_TRACE_CAT("Vulkan", "  [{}] {}", i, names[i]);
    }

    LOG_SUCCESS_CAT("Vulkan", "{} Vulkan instance extensions retrieved{}", result.size(), RESET);
    return result;
}

// pollEventsForResize and toggleFullscreen unchanged — already correct
bool pollEventsForResize(const SDLWindowPtr& window,
                         int& newWidth, int& newHeight,
                         bool& shouldQuit, bool& toggleFullscreenKey) noexcept {
    SDL_Event event;
    bool resized = false;
    shouldQuit = false;
    toggleFullscreenKey = false;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) shouldQuit = true;
        else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F11) toggleFullscreenKey = true;
        else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
            newWidth = event.window.data1;
            newHeight = event.window.data2;
            resized = true;
            LOG_INFO_CAT("APP", "Window resized: {}x{}", newWidth, newHeight);
        }
    }
    return resized;
}

void toggleFullscreen(SDLWindowPtr& window) noexcept {
    bool isFullscreen = (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_FULLSCREEN);
    SDL_SetWindowFullscreen(window.get(), !isFullscreen);

    int w, h;
    SDL_GetWindowSizeInPixels(window.get(), &w, &h);
    if (auto* r = &SDL3Vulkan::getRenderer()) r->handleResize(w, h);
}

} // namespace SDL3Initializer