// =============================================================================
// src/engine/SDL3/SDL3_window.cpp
// STONEKEY v∞ FULLY ACTIVE — PINK PHOTONS ETERNAL — APOCALYPSE FINAL 2025 AAAA
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 20, 2025 — APOCALYPSE FINAL v7.0
// MAIN — FULL RTX ALWAYS — VALIDATION LAYERS FORCE-DISABLED — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace Logging::Color;

// =============================================================================
// STONEKEY v∞ RAII DELETER + TYPE — VISIBLE ONLY IN THIS TU
// =============================================================================

struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept {
        if (w) {
            LOG_INFO_CAT("Dispose", "RAII: SDL_DestroyWindow @ {:p} — STONEKEY v∞", static_cast<void*>(w));
            SDL_DestroyWindow(w);
        }
    }
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

// =============================================================================
// STONEKEY v∞ ENCRYPTED GLOBAL — THREAD_LOCAL + ZERO HEADER LEAK
// =============================================================================

thread_local SDLWindowPtr g_stonekey_window;

namespace SDL3Window {

// =============================================================================
// PUBLIC ACCESSORS
// =============================================================================

[[nodiscard]] SDL_Window* get() noexcept {
    return g_stonekey_window ? g_stonekey_window.get() : nullptr;
}

void create(const char* title, int width, int height, Uint32 flags)
{
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (Options::Performance::ENABLE_IMGUI) flags |= SDL_WINDOW_RESIZABLE;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SDL3", "SDL_Init failed: {}", SDL_GetError());
        throw std::runtime_error("SDL_Init failed");
    }

    bool vulkanReady = false;
    if (SDL_Vulkan_LoadLibrary(nullptr)) {
        uint32_t count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> exts(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data());
        for (const auto& e : exts)
            if (strcmp(e.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0)
                vulkanReady = true;
        if (vulkanReady) flags |= SDL_WINDOW_VULKAN;
    }

    SDL_Window* raw = SDL_CreateWindow(title, width, height, flags);
    if (!raw) {
        LOG_FATAL_CAT("SDL3", "SDL_CreateWindow failed: {}", SDL_GetError());
        throw std::runtime_error("Window creation failed");
    }

    g_stonekey_window.reset(raw);

    LOG_SUCCESS_CAT("SDL3", "Window forged {}x{} — Vulkan: {} — STONEKEY v∞ SEALED",
                    width, height, vulkanReady ? "ENABLED" : "FALLBACK");
    LOG_SUCCESS_CAT("SDL3", "{}SDL3 WINDOW ASCENDED — PINK PHOTONS PROTECTED — FIRST LIGHT ETERNAL{}",
                    PLASMA_FUCHSIA, RESET);
}

[[nodiscard]] std::vector<std::string> getVulkanExtensions(SDL_Window* window)
{
    if (!window) window = get();
    if (!window) return {};

    Uint32 count = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!exts || count == 0) return {};

    return {exts, exts + count};
}

// =============================================================================
// RESIZE + EVENT POLLING — UNCHANGED PERFECTION
// =============================================================================

namespace detail {
    uint64_t g_lastResizeTime   = 0;
    int      g_pendingWidth     = 0;
    int      g_pendingHeight    = 0;
    bool     g_resizePending    = false;
    constexpr uint64_t RESIZE_DEBOUNCE_MS = 120;
}

std::atomic<int>  g_resizeWidth{0};
std::atomic<int>  g_resizeHeight{0};
std::atomic<bool> g_resizeRequested{false};

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept
{
    using namespace detail;

    SDL_Event ev;
    quit = toggleFS = false;
    bool resized = false;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11)
                    toggleFS = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED: {
                g_pendingWidth   = ev.window.data1;
                g_pendingHeight  = ev.window.data2;
                g_resizePending  = true;
                g_lastResizeTime = SDL_GetTicks();
                resized = true;
                break;
            }
        }
    }

    if (g_stonekey_window) {
        SDL_GetWindowSizeInPixels(g_stonekey_window.get(), &outW, &outH);
    }

    if (g_resizePending && (SDL_GetTicks() - g_lastResizeTime >= RESIZE_DEBOUNCE_MS)) {
        LOG_INFO_CAT("Window", "DEFERRED RESIZE → {}x{}", g_pendingWidth, g_pendingHeight);
        g_resizeWidth.store(g_pendingWidth);
        g_resizeHeight.store(g_pendingHeight);
        g_resizeRequested.store(true);
        g_resizePending = false;
    }

    return resized;
}

void toggleFullscreen() noexcept
{
    if (!g_stonekey_window) return;
    Uint32 f = SDL_GetWindowFlags(g_stonekey_window.get());
    SDL_SetWindowFullscreen(g_stonekey_window.get(), !(f & SDL_WINDOW_FULLSCREEN));
}

void destroy() noexcept
{
    g_stonekey_window.reset();
    g_resizeRequested.store(false);
    SDL_Quit();
}

} // namespace SDL3Window