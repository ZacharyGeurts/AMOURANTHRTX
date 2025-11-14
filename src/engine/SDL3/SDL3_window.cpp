// src/engine/SDL3/SDL3_window.cpp
// =============================================================================
// FINAL RAII — NO DOUBLE SDL_Quit() — NO SEGFAULT — NOV 14 2025
// =============================================================================

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

using namespace Logging::Color;

namespace SDL3Window {

SDLWindowPtr g_sdl_window{nullptr};

[[nodiscard]] SDLWindowPtr create(const char* title, int width, int height, Uint32 flags)
{
    LOG_INFO_CAT("SDL3_window", "{}=== SDL3 MAIN WINDOW FORGE INITIATED ==={}", SAPPHIRE_BLUE, RESET);

    bool vulkanSupported = false;
    LOG_INFO_CAT("SDL3_window", "{}Probing Vulkan support via SDL_Vulkan_LoadLibrary(){}", SAPPHIRE_BLUE, RESET);
    bool libLoaded = SDL_Vulkan_LoadLibrary(nullptr);
    if (libLoaded) {
        uint32_t extCount = 0;
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr) == VK_SUCCESS) {
            std::vector<VkExtensionProperties> exts(extCount);
            if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, exts.data()) == VK_SUCCESS) {
                for (const auto& ext : exts) {
                    if (strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
                        vulkanSupported = true;
                        break;
                    }
                }
            }
        }
        SDL_Vulkan_UnloadLibrary();
        LOG_SUCCESS_CAT("SDL3_window", "{}Vulkan supported (VK_KHR_surface present) — enabling SDL_WINDOW_VULKAN{}", SAPPHIRE_BLUE, RESET);
    } else {
        LOG_WARN_CAT("SDL3_window", "Vulkan library load failed: {} — falling back to non-Vulkan window", SDL_GetError());
    }

    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    LOG_INFO_CAT("SDL3_window", "Final window flags: 0x{:08X} (High DPI: {}) (Vulkan: {})", flags, "ENABLED", vulkanSupported ? "ENABLED" : "DISABLED");
    if (vulkanSupported) {
        flags |= SDL_WINDOW_VULKAN;
        LOG_INFO_CAT("SDL3_window", "SDL_WINDOW_VULKAN enabled — ready for RTX forge");
    }
    if (Options::Performance::ENABLE_IMGUI) {
        flags |= SDL_WINDOW_RESIZABLE;
        LOG_INFO_CAT("SDL3_window", "ImGui enabled → SDL_WINDOW_RESIZABLE added");
    }

    LOG_INFO_CAT("SDL3_window", "{}Initializing SDL core subsystems (VIDEO + EVENTS){}", SAPPHIRE_BLUE, RESET);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SDL3_window", "SDL_Init failed: {}", SDL_GetError());
        throw std::runtime_error("SDL_Init failed");
    }
    LOG_SUCCESS_CAT("SDL3_window", "SDL core subsystems initialized successfully");

    LOG_ATTEMPT_CAT("SDL3_window", "{}Forging SDL_Window: \'{}\' {}x{} flags=0x{:08X}{}", SAPPHIRE_BLUE, title, width, height, flags, RESET);
    SDL_Window* raw = SDL_CreateWindow(title, width, height, flags);
    if (!raw) {
        LOG_FATAL_CAT("SDL3_window", "SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();  // Safe early quit on failure
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    LOG_INFO_CAT("SDL3_window", "{}Raw SDL_Window* acquired @ {:p} — ownership imminent{}", SAPPHIRE_BLUE, static_cast<void*>(raw), RESET);

    LOG_SUCCESS_CAT("SDL3_window", "{}Window forged @ {:p} — {}x{} (Vulkan: {})", SAPPHIRE_BLUE, static_cast<void*>(raw), width, height, vulkanSupported ? "YES" : "NO");

    // Explicitly construct + log to isolate if destruction happens during unique_ptr init/move
    SDLWindowPtr ptr(raw);
    LOG_SUCCESS_CAT("SDL3_window", "{}SDLWindowPtr constructed @ caller side — RAII ownership transferred{}", SAPPHIRE_BLUE, RESET);
    LOG_INFO_CAT("SDL3_window", "{}Verifying pre-return: ptr.get() @ {:p} (non-null expected){}", SAPPHIRE_BLUE, static_cast<void*>(ptr.get()), RESET);

    LOG_INFO_CAT("SDL3_window", "{}About to return moved SDLWindowPtr — move semantics engaged{}", SAPPHIRE_BLUE, RESET);
    return ptr;  // Move out — temporary should nullify post-move
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
                LOG_INFO_CAT("SDL3_window", "Resize: {}x{}", outW, outH);
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
        LOG_WARN_CAT("SDL3_window", "toggleFullscreen(): no window");
        return;
    }

    SDL_Window* window = g_sdl_window.get();
    bool is_fs = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0;
    SDL_SetWindowFullscreen(window, !is_fs);  // SDL3: bool (true=fullscreen, false=windowed)

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    LOG_INFO_CAT("SDL3_window", "Fullscreen → {} ({}x{})", !is_fs ? "ON" : "OFF", w, h);

    if (auto* r = &SDL3Vulkan::renderer()) {
        r->handleResize(w, h);
    }
}

// No definitions for inline functions here — they are in header

} // namespace SDL3Window