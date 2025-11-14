// src/engine/SDL3/SDL3_window.cpp
// =============================================================================
// FINAL RAII — NO DOUBLE SDL_Quit() — NO SEGFAULT — NOV 14 2025
// =============================================================================

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

using namespace Logging::Color;

// One true global RAII window
SDLWindowPtr g_sdl_window{nullptr};

namespace SDL3Window {

[[nodiscard]] SDLWindowPtr create(const char* title, int width, int height, Uint32 flags)
{
    LOG_ATTEMPT_CAT("Window", "Creating main window: '{}' {}x{}", title, width, height);

    flags |= SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (Options::Performance::ENABLE_IMGUI) {
        flags |= SDL_WINDOW_RESIZABLE;
        LOG_INFO_CAT("Window", "ImGui enabled → SDL_WINDOW_RESIZABLE added");
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SDL", "SDL_Init failed: {}", SDL_GetError());
        throw std::runtime_error("SDL_Init failed");
    }

    SDL_Window* raw = SDL_CreateWindow(title, width, height, flags);
    if (!raw) {
        LOG_FATAL_CAT("SDL", "SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();  // Clean up partial init
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    LOG_SUCCESS_CAT("Window", "Window created @ {:p} — {}x{}", static_cast<void*>(raw), width, height);

    // Return ownership to caller (e.g., g_sdl_window = create(...)); global managed externally
    return SDLWindowPtr(raw);
}

[[nodiscard]] std::vector<std::string> getVulkanExtensions(SDL_Window* window)
{
    if (!window) return {};

    // SDL3: Single call populates both count and array
    Uint32 count = 0;
    const char * const * exts = SDL_Vulkan_GetInstanceExtensions(&count);
    if (exts == nullptr) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        return {};
    }

    std::vector<std::string> result;
    result.reserve(count);
    for (Uint32 i = 0; i < count; ++i) {
        if (exts[i]) result.emplace_back(exts[i]);  // Copy strings (SDL owns exts)
    }

    LOG_SUCCESS_CAT("Vulkan", "{} Vulkan instance extensions retrieved", result.size());
    return result;
}

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept
{
    SDL_Event ev;
    bool resized = false;
    quit = toggleFS = false;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11) {
                    toggleFS = true;
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                outW = ev.window.data1;
                outH = ev.window.data2;
                resized = true;
                LOG_INFO_CAT("Window", "Resize: {}x{}", outW, outH);
                break;
        }
    }

    // Fallback: Query current size if no resize event
    if (!resized && g_sdl_window) {
        SDL_GetWindowSizeInPixels(g_sdl_window.get(), &outW, &outH);
    }

    return resized;
}

void toggleFullscreen() noexcept
{
    if (!g_sdl_window) {
        LOG_WARN_CAT("Window", "toggleFullscreen(): no window");
        return;
    }

    SDL_Window* window = g_sdl_window.get();
    bool is_fs = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    SDL_SetWindowFullscreen(window, !is_fs);  // SDL3: bool (true=fullscreen, false=windowed)

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    LOG_INFO_CAT("Window", "Fullscreen → {} ({}x{})", !is_fs ? "ON" : "OFF", w, h);

    if (auto* r = &SDL3Vulkan::renderer()) {
        r->handleResize(w, h);
    }
}

} // namespace SDL3Window