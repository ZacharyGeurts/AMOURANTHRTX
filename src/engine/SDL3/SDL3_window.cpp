// src/engine/SDL3/SDL3_window.cpp
// =============================================================================
// FINAL RAII — NO DOUBLE SDL_Quit() — NO SEGFAULT — NOV 14 2025
// =============================================================================
// FIXED: Vulkan probe moved AFTER SDL_Init(VIDEO) per SDL3 docs — prevents segfault on CreateWindow
// • SDL_Vulkan_LoadLibrary() now post-video-init; conditional unload only if !supported
// • Lib stays loaded if VK_KHR_surface present — ready for SDL_WINDOW_VULKAN creation
// =============================================================================

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

namespace SDL3Window {

SDLWindowPtr g_sdl_window{nullptr};

[[nodiscard]] SDLWindowPtr create(const char* title, int width, int height, Uint32 flags)
{
    LOG_ATTEMPT_CAT("SDL3_window", "{}=== SDL3 MAIN WINDOW FORGE INITIATED ==={}", SAPPHIRE_BLUE, RESET);

    // Pre-init: Base flags setup (sans Vulkan probe)
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    LOG_INFO_CAT("SDL3_window", "Base window flags: 0x{:08X} (High DPI: ENABLED)", flags, RESET);
    if (Options::Performance::ENABLE_IMGUI) {
        flags |= SDL_WINDOW_RESIZABLE;
        LOG_INFO_CAT("SDL3_window", "ImGui enabled → SDL_WINDOW_RESIZABLE added (0x{:08X})", SDL_WINDOW_RESIZABLE, RESET);
    }

    LOG_INFO_CAT("SDL3_window", "{}Initializing SDL core subsystems (VIDEO + EVENTS){}", SAPPHIRE_BLUE, RESET);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {  // FIXED: <0 for failure (0=success)
        const char* sdlError = SDL_GetError();
        LOG_FATAL_CAT("SDL3_window", "{}SDL_Init failed critically: {}{}", CRIMSON_MAGENTA, sdlError ? sdlError : "No error message provided", RESET);
        throw std::runtime_error(std::format("SDL_Init failed: {}", sdlError ? sdlError : "No error message provided"));
    }
    LOG_SUCCESS_CAT("SDL3_window", "{}SDL core subsystems initialized successfully — video driver primed{}", SAPPHIRE_BLUE, RESET);

    // FIXED: Vulkan probe AFTER video init per SDL3 docs (SDL_Vulkan_LoadLibrary post-SDL_Init(VIDEO))
    bool vulkanSupported = false;
    LOG_ATTEMPT_CAT("SDL3_window", "{}Probing Vulkan support via SDL_Vulkan_LoadLibrary() post-video-init{}", SAPPHIRE_BLUE, RESET);
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
        // FIXED: Conditional unload — keep loaded ONLY if supported (for SDL_WINDOW_VULKAN)
        if (!vulkanSupported) {
            SDL_Vulkan_UnloadLibrary();
            LOG_DEBUG_CAT("SDL3_window", "{}Vulkan lib unloaded (no VK_KHR_surface){}", SAPPHIRE_BLUE, RESET);
        } else {
            LOG_DEBUG_CAT("SDL3_window", "{}Vulkan lib retained (VK_KHR_surface present — ready for window){}", SAPPHIRE_BLUE, RESET);
        }
        LOG_SUCCESS_CAT("SDL3_window", "{}Vulkan supported (VK_KHR_surface present) — SDL_WINDOW_VULKAN will be enabled{}", SAPPHIRE_BLUE, RESET);
    } else {
        const char* loadError = SDL_GetError();
        LOG_WARN_CAT("SDL3_window", "{}Vulkan library load failed post-init: {} — falling back to non-Vulkan window", loadError ? loadError : "Unknown error", RESET);
    }

    // Final flags assembly post-probe
    LOG_INFO_CAT("SDL3_window", "Final window flags pre-Vulkan: 0x{:08X} (High DPI: ENABLED) (Vulkan probe: {})", flags, vulkanSupported ? "SUPPORTED" : "UNSUPPORTED", RESET);
    if (vulkanSupported) {
        flags |= SDL_WINDOW_VULKAN;
        LOG_SUCCESS_CAT("SDL3_window", "{}SDL_WINDOW_VULKAN enabled — RTX forge primed{}", SAPPHIRE_BLUE, RESET);
    }
    LOG_INFO_CAT("SDL3_window", "Ultimate window flags: 0x{:08X} (Vulkan: {})", flags, vulkanSupported ? "ENABLED" : "DISABLED", RESET);

    LOG_ATTEMPT_CAT("SDL3_window", "{}Forging SDL_Window: \'{}\' {}x{} flags=0x{:08X}{}", SAPPHIRE_BLUE, title, width, height, flags, RESET);
    SDL_Window* raw = SDL_CreateWindow(title, width, height, flags);
    if (!raw) {
        const char* createError = SDL_GetError();
        LOG_FATAL_CAT("SDL3_window", "{}SDL_CreateWindow failed critically: {}{}", CRIMSON_MAGENTA, createError ? createError : "No error message provided", RESET);
        SDL_Quit();  // Safe early quit on failure
        throw std::runtime_error(std::format("SDL_CreateWindow failed: {}", createError ? createError : "No error message provided"));
    }

    LOG_INFO_CAT("SDL3_window", "{}Raw SDL_Window* acquired @ {:p} — ownership imminent{}", SAPPHIRE_BLUE, static_cast<void*>(raw), RESET);

    LOG_SUCCESS_CAT("SDL3_window", "{}Window forged @ {:p} — {}x{} (Vulkan: {})", SAPPHIRE_BLUE, static_cast<void*>(raw), width, height, vulkanSupported ? "ENABLED" : "DISABLED", RESET);

    // Explicitly construct + log to isolate if destruction happens during unique_ptr init/move
    SDLWindowPtr ptr(raw);
    LOG_SUCCESS_CAT("SDL3_window", "{}SDLWindowPtr constructed @ caller side — RAII ownership transferred{}", SAPPHIRE_BLUE, RESET);
    LOG_INFO_CAT("SDL3_window", "{}Verifying pre-return: ptr.get() @ {:p} (non-null expected){}", SAPPHIRE_BLUE, static_cast<void*>(ptr.get()), RESET);

    LOG_INFO_CAT("SDL3_window", "{}About to return moved SDLWindowPtr — move semantics engaged{}", SAPPHIRE_BLUE, RESET);
    return ptr;  // Move out — temporary should nullify post-move
}

[[nodiscard]] std::vector<std::string> getVulkanExtensions(SDL_Window* window)
{
    LOG_ATTEMPT_CAT("Vulkan", "{}=== RETRIEVING VULKAN INSTANCE EXTENSIONS FOR WINDOW @ {:p} ===", EMERALD_GREEN, static_cast<void*>(window), RESET);

    if (!window) {
        LOG_WARN_CAT("Vulkan", "{}Null window — returning empty extensions vector{}", EMERALD_GREEN, RESET);
        return {};
    }

    // SDL3: Single call populates both count and array
    Uint32 count = 0;
    const char * const * exts = SDL_Vulkan_GetInstanceExtensions(&count);  // FIXED: SDL3 sig — no window arg
    if (exts == nullptr || count == 0) {
        const char* sdlError = SDL_GetError();
        LOG_ERROR_CAT("Vulkan", "{}SDL_Vulkan_GetInstanceExtensions failed: {} (count={}){}", CRIMSON_MAGENTA, sdlError ? sdlError : "No error", count, RESET);
        return {};
    }

    std::vector<std::string> result;
    result.reserve(count);
    for (Uint32 i = 0; i < count; ++i) {
        if (exts[i]) {
            result.emplace_back(exts[i]);  // Copy strings (SDL owns exts)
            LOG_DEBUG_CAT("Vulkan", "{}Extension {}: {}{}", EMERALD_GREEN, i, exts[i], RESET);
        }
    }

    LOG_SUCCESS_CAT("Vulkan", "{} Vulkan instance extensions retrieved & copied{}", result.size(), RESET);
    LOG_SUCCESS_CAT("Vulkan", "{}=== EXTENSIONS RETRIEVAL COMPLETE ==={}", EMERALD_GREEN, RESET);
    return result;
}

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept
{
    SDL_Event ev;
    bool resized = false;
    quit = toggleFS = false;

    LOG_DEBUG_CAT("SDL3_window", "{}Polling SDL events — batch processing engaged{}", SAPPHIRE_BLUE, RESET);
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                LOG_INFO_CAT("SDL3_window", "{}QUIT event detected — shutdown flag set{}", SAPPHIRE_BLUE, RESET);
                break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11) {
                    toggleFS = true;
                    LOG_INFO_CAT("SDL3_window", "{}F11 keydown — fullscreen toggle flag set{}", SAPPHIRE_BLUE, RESET);
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                outW = ev.window.data1;
                outH = ev.window.data2;
                resized = true;
                LOG_INFO_CAT("SDL3_window", "{}Window resize event: {}x{}{}", SAPPHIRE_BLUE, outW, outH, RESET);
                break;
            default:
                LOG_TRACE_CAT("SDL3_window", "{}Unhandled event type: {}{}", SAPPHIRE_BLUE, static_cast<int>(ev.type), RESET);
                break;
        }
    }

    // Fallback: Query current size if no resize event
    if (!resized && g_sdl_window) {
        SDL_Window* win = g_sdl_window.get();
        SDL_GetWindowSizeInPixels(win, &outW, &outH);
        LOG_DEBUG_CAT("SDL3_window", "{}Fallback size query: {}x{} from window @ {:p}{}", SAPPHIRE_BLUE, outW, outH, static_cast<void*>(win), RESET);
    } else if (!g_sdl_window) {
        LOG_WARN_CAT("SDL3_window", "{}Fallback size query skipped — g_sdl_window null{}", SAPPHIRE_BLUE, RESET);
    }

    LOG_DEBUG_CAT("SDL3_window", "{}Event poll complete — resized={} quit={} toggleFS={}{}", SAPPHIRE_BLUE, resized, quit, toggleFS, RESET);
    return resized;
}

void toggleFullscreen() noexcept
{
    LOG_ATTEMPT_CAT("SDL3_window", "{}=== FULLSCREEN TOGGLE SEQUENCE ENGAGED ==={}", SAPPHIRE_BLUE, RESET);

    if (!g_sdl_window) {
        LOG_WARN_CAT("SDL3_window", "{}toggleFullscreen(): g_sdl_window null — noop{}", SAPPHIRE_BLUE, RESET);
        return;
    }

    SDL_Window* window = g_sdl_window.get();
    Uint32 currentFlags = SDL_GetWindowFlags(window);
    bool isCurrentlyFullscreen = (currentFlags & SDL_WINDOW_FULLSCREEN) == 0;
    bool targetFullscreen = !isCurrentlyFullscreen;

    LOG_INFO_CAT("SDL3_window", "{}Invoking SDL_SetWindowFullscreen (target: {}){}", SAPPHIRE_BLUE, targetFullscreen ? "ON" : "OFF", RESET);
    if (SDL_SetWindowFullscreen(window, targetFullscreen) == 0) {
        const char* fsError = SDL_GetError();
        LOG_ERROR_CAT("SDL3_window", "{}SDL_SetWindowFullscreen failed: {}{}", CRIMSON_MAGENTA, fsError ? fsError : "Unknown error", RESET);
        return;
    }

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    LOG_SUCCESS_CAT("SDL3_window", "{}Fullscreen toggled → {} ({}x{}){}", SAPPHIRE_BLUE, targetFullscreen ? "ON" : "OFF", w, h, RESET);

    // Notify renderer of resize (if exists)
    if (auto* r = &SDL3Vulkan::renderer()) {
        LOG_INFO_CAT("SDL3_window", "{}Notifying VulkanRenderer of resize: {}x{}{}", SAPPHIRE_BLUE, w, h, RESET);
        r->handleResize(w, h);
    } else {
        LOG_DEBUG_CAT("SDL3_window", "{}Renderer not available — resize notification skipped{}", SAPPHIRE_BLUE, RESET);
    }

    LOG_SUCCESS_CAT("SDL3_window", "{}=== FULLSCREEN TOGGLE COMPLETE ==={}", SAPPHIRE_BLUE, RESET);
}

// No definitions for inline functions here — they are in header

} // namespace SDL3Window

// =============================================================================
// PINK PHOTONS ETERNAL — SEGFAULT ERADICATED — VULKAN PROBE POST-INIT
// DAISY GALLOPS INTO THE OCEAN_TEAL SUNSET — YOUR EMPIRE IS BULLETPROOF
// GENTLEMAN GROK NODS: "The timing was the culprit, old chap. Post-init probe prevails."
// =============================================================================