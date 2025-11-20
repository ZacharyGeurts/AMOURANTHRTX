// src/engine/SDL3/SDL3_window.cpp
// AMOURANTH RTX — PINK PHOTONS ETERNAL — NOVEMBER 20, 2025 — SIMPLIFIED & CONQUERED
#include "engine/SDL3/SDL3_window.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include <SDL3/SDL.h>

using namespace Logging::Color;

// RAII — raw pointer never leaks
struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept { if (w) SDL_DestroyWindow(w); }
};
using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;
thread_local SDLWindowPtr g_window;

namespace SDL3Window {
std::atomic<int>  g_resizeWidth{0};
std::atomic<int>  g_resizeHeight{0};
std::atomic<bool> g_resizeRequested{false};
}

namespace {
    uint64_t g_last_resize_time = 0;
    constexpr uint64_t RESIZE_DEBOUNCE_MS = 100;
}

namespace SDL3Window {

SDL_Window* get() noexcept { return g_window.get(); }

void create(const char* title, int width, int height, Uint32 flags)
{
    flags |= SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        LOG_FATAL("SDL_Init failed: {}", SDL_GetError());
        std::abort();
    }

    SDL_Window* raw = SDL_CreateWindow(title, width, height, flags);
    if (!raw) {
        LOG_FATAL("SDL_CreateWindow failed: {}", SDL_GetError());
        std::abort();
    }
    g_window.reset(raw);
    LOG_SUCCESS_CAT("SDL3", "{}WINDOW FORGED — {}×{} — PINK PHOTONS IMMINENT{}", LIME_GREEN, width, height, RESET);
}

std::vector<std::string> getVulkanExtensions(SDL_Window* window) noexcept
{
    if (!window) window = get();
    if (!window) return {};

    unsigned int count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&count)) return {};
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
    return {exts, exts + count};
}

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept
{
    SDL_Event ev;
    quit = toggleFS = false;
    bool hadEvent = false;

    while (SDL_PollEvent(&ev)) {
        hadEvent = true;
        switch (ev.type) {
            case SDL_EVENT_QUIT: quit = true; break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11) toggleFS = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                int w = ev.window.data1, h = ev.window.data2;
                if (w <= 0 || h <= 0) break;
                g_resizeWidth.store(w);
                g_resizeHeight.store(h);
                g_resizeRequested.store(true);
                g_last_resize_time = SDL_GetTicks();
                break;
            }
        }
    }

    if (get()) SDL_GetWindowSizeInPixels(get(), &outW, &outH);

    if (g_resizeRequested.load() && (SDL_GetTicks() - g_last_resize_time >= RESIZE_DEBOUNCE_MS)) {
        int w = g_resizeWidth.load(), h = g_resizeHeight.load();
        SwapchainManager::get().recreate(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        SwapchainManager::get().updateWindowTitle(get(), 0.0f);
        g_resizeRequested.store(false);
    }

    return hadEvent;
}

void toggleFullscreen() noexcept
{
    if (!get()) return;
    Uint32 flags = SDL_GetWindowFlags(get());
    bool isFS = (flags & SDL_WINDOW_FULLSCREEN);
    SDL_SetWindowFullscreen(get(), !isFS);
    g_last_resize_time = SDL_GetTicks() - RESIZE_DEBOUNCE_MS - 1;
    g_resizeRequested.store(true);
}

void destroy() noexcept
{
    g_window.reset();
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

} // namespace SDL3Window