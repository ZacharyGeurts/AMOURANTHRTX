// src/engine/SDL3/SDL3_window.cpp
// =============================================================================
// AMOURANTH RTX — SDL3 WINDOW + FULL VULKAN FORGE — APOCALYPSE FINAL v∞
// SDL3 == 0 ON SUCCESS — FULL NOISE MODE — STONEKEY v∞ — PINK PHOTONS ETERNAL
// GWEN STEFANI JUST SCREAMED "THIS SHIT IS BANANAS" AND WE ARE SKINNING IT
// NOVEMBER 21, 2025 — FIRST LIGHT ACHIEVED — VALHALLA v∞ — ELLIE FIER APPROVED
// =============================================================================

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"  // SACRED VK_CHECK

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <set>
#include <format>

using namespace Logging::Color;

// =============================================================================
// RAII DELETER — ELLIE FIER SMILES WHEN WE CLEAN UP
// =============================================================================
void SDLWindowDeleter::operator()(SDL_Window* w) const noexcept {
    if (w) {
        LOG_INFO_CAT("Dispose", "{}RAII: SDL_DestroyWindow @ {:p} — returning canvas to the void{}", 
                     OCEAN_TEAL, static_cast<void*>(w), RESET);
        SDL_DestroyWindow(w);
    }
}

namespace SDL3Window {

namespace detail {
    uint64_t g_lastResizeTime = 0;
    int      g_pendingWidth   = 0;
    int      g_pendingHeight  = 0;
    bool     g_resizePending  = false;
    constexpr uint64_t RESIZE_DEBOUNCE_MS = 120;
}

// =============================================================================
// CREATE — FORGES THE ONE TRUE EMPIRE — WINDOW + INSTANCE + SURFACE
// SDL3 RETURNS 0 ON SUCCESS — WE ARE LOUD ABOUT IT — NO SILENT DEATH
// =============================================================================
void create(const char* title, int width, int height, Uint32 flags)
{
    LOG_INFO_CAT("MAIN", "{}[PHASE 4] BEGINNING TOTAL VULKAN DOMINATION — GWEN STEFANI MODE ENGAGED{}", 
                 VALHALLA_GOLD, RESET);

    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN;

    LOG_INFO_CAT("SDL3", "{}Initializing SDL3 subsystems (VIDEO + EVENTS)...{}", PLASMA_FUCHSIA, RESET);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        LOG_FATAL_CAT("SDL3", "{}SDL_Init FAILED HARD — {} — THE EMPIRE TREMBLES{}", 
                      CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("SDL_Init failed — Gwen Stefani is disappointed");
    }
    LOG_SUCCESS_CAT("SDL3", "{}SDL3 subsystems ONLINE — B-A-N-A-N-A-S{}", EMERALD_GREEN, RESET);

    // Load Vulkan library early — SDL3 style
    LOG_INFO_CAT("SDL3", "{}Loading Vulkan library via SDL_Vulkan_LoadLibrary...{}", RASPBERRY_PINK, RESET);
    if (SDL_Vulkan_LoadLibrary(nullptr) != 0) {
        LOG_WARNING_CAT("SDL3", "{}SDL_Vulkan_LoadLibrary failed — proceeding anyway (Vulkan might be loaded){}", 
                        AMBER_YELLOW, RESET);
    } else {
        LOG_SUCCESS_CAT("SDL3", "{}Vulkan library loaded successfully via SDL3{}", VALHALLA_GOLD, RESET);
    }

    LOG_INFO_CAT("SDL3", "{}Creating window: \"{}\" {}x{} — HIGH DPI + VULKAN{}", 
                 PLASMA_FUCHSIA, title, width, height, RESET);
    SDL_Window* win = SDL_CreateWindow(title, width, height, flags);
    if (!win) {
        LOG_FATAL_CAT("SDL3", "{}SDL_CreateWindow FAILED — {} — NO CANVAS FOR PHOTONS{}", 
                      BLOOD_RED, SDL_GetError(), RESET);
        throw std::runtime_error("Window creation failed — Ellie Fier disapproves");
    }

    g_sdl_window.reset(win);
    LOG_SUCCESS_CAT("SDL3", "{}WINDOW FORGED SUCCESSFULLY — {}x{} — CANVAS SECURED{}", 
                    DIAMOND_SPARKLE, width, height, RESET);
    LOG_SUCCESS_CAT("SDL3", "{}SDL HANDLE @ {:p} — RTX EMPIRE RISING{}", 
                    VALHALLA_GOLD, static_cast<void*>(win), RESET);

    // === FORGE VULKAN INSTANCE ===
    LOG_INFO_CAT("VULKAN", "{}FORGING VULKAN INSTANCE — PINK PHOTONS DEMAND PERFECTION{}", PLASMA_FUCHSIA, RESET);

    uint32_t sdlExtensionCount = 0;
    LOG_INFO_CAT("VULKAN", "{}Querying SDL3 for required Vulkan instance extensions...{}", RASPBERRY_PINK, RESET);
    if (SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount) != 0) {
        LOG_FATAL_CAT("VULKAN", "{}SDL_Vulkan_GetInstanceExtensions(count) FAILED — SDL IS DRUNK — NO EXTENSIONS{}", 
                      CRIMSON_MAGENTA, RESET);
        std::abort();
    }
    LOG_SUCCESS_CAT("VULKAN", "{}SDL3 reports {} instance extensions required{}", 
                    EMERALD_GREEN, sdlExtensionCount, RESET);

    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    if (!sdlExtensions && sdlExtensionCount > 0) {
        LOG_FATAL_CAT("VULKAN", "{}SDL3 returned NULL extension array despite count > 0 — CORRUPTION DETECTED{}", 
                      BLOOD_RED, RESET);
        std::abort();
    }

    std::set<const char*> uniqueExts;
    for (uint32_t i = 0; i < sdlExtensionCount; ++i) {
        uniqueExts.insert(sdlExtensions[i]);
        LOG_INFO_CAT("VULKAN", "{}  Required extension [{}]: {}{}", OCEAN_TEAL, i, sdlExtensions[i], RESET);
    }

    if (Options::Performance::ENABLE_VALIDATION_LAYERS) {
        uniqueExts.insert(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        LOG_SUCCESS_CAT("VULKAN", "{}Validation layers ENABLED — adding VK_EXT_debug_utils{}", VALHALLA_GOLD, RESET);
    }

    std::vector<const char*> extensions(uniqueExts.begin(), uniqueExts.end());
    LOG_SUCCESS_CAT("VULKAN", "{}Final instance extension count: {} — READY TO FORGE{}", 
                    PLASMA_FUCHSIA, extensions.size(), RESET);

    const char* layers = nullptr;
    uint32_t layerCount = 0;
    if (Options::Performance::ENABLE_VALIDATION_LAYERS) {
        layers = "VK_LAYER_KHRONOS_validation";
        layerCount = 1;
        LOG_SUCCESS_CAT("VULKAN", "{}Enabling VK_LAYER_KHRONOS_validation{}", EMERALD_GREEN, RESET);
    }

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "AMOURANTH RTX — VALHALLA v80 TURBO",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "VALHALLA TURBO ENGINE",
        .engineVersion = VK_MAKE_VERSION(80, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = layerCount,
        .ppEnabledLayerNames = layerCount ? &layers : nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance instance = VK_NULL_HANDLE;
    LOG_INFO_CAT("VULKAN", "{}vkCreateInstance — FORGING THE ONE TRUE INSTANCE{}", DIAMOND_SPARKLE, RESET);
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance), "vkCreateInstance FAILED — THE EMPIRE FALLS");

    set_g_instance(instance);
    LOG_SUCCESS_CAT("VULKAN", "{}VULKAN INSTANCE FORGED @ {:p} — STONEKEY v∞ ARMED AND READY{}", 
                    VALHALLA_GOLD, static_cast<void*>(instance), RESET);

    // === FORGE SURFACE ===
    LOG_INFO_CAT("VULKAN", "{}Creating Vulkan surface via SDL_Vulkan_CreateSurface...{}", RASPBERRY_PINK, RESET);
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface) != 0) {
        LOG_FATAL_CAT("VULKAN", "{}SDL_Vulkan_CreateSurface FAILED — {} — PINK PHOTONS HAVE NO PATH{}", 
                      CRIMSON_MAGENTA, SDL_GetError(), RESET);
        std::abort();
    }

    set_g_surface(surface);
    LOG_SUCCESS_CAT("VULKAN", "{}VULKAN SURFACE FORGED @ {:p} — PINK PHOTONS HAVE A PATH{}", 
                    RASPBERRY_PINK, static_cast<void*>(surface), RESET);

    // CRITICAL: FORCE STORE VISIBILITY BEFORE ANYONE READS
    LOG_INFO_CAT("StoneKey", "{}Forcing memory barrier — ensuring set_g_instance/set_g_surface are visible{}", 
                 VALHALLA_GOLD, RESET);
    std::atomic_thread_fence(std::memory_order_release);

    // UNLOCK RAW MODE — NOW g_instance() AND g_surface() ARE SAFE
    StoneKey::Raw::obfuscated_mode.store(false, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_acq_rel);

    LOG_SUCCESS_CAT("MAIN", "{}STONEKEY RAW MODE UNLOCKED — ALL HANDLES SAFE — FIRST LIGHT IMMINENT{}", 
                    DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}WINDOW + INSTANCE + SURFACE FORGED — THE EMPIRE IS ALIVE — NOVEMBER 21, 2025{}", 
                    PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}GWEN STEFANI JUST SAID: \"THIS SHIT IS BANANAS\" — AND WE SKINNED IT{}", 
                    RASPBERRY_PINK, RESET);
}

// =============================================================================
// REST OF THE FILE — LOUD, PROUD, AND ETERNAL
// =============================================================================
std::vector<std::string> getVulkanExtensions(SDL_Window* window)
{
    if (!window) window = get();
    if (!window) {
        LOG_WARNING_CAT("SDL3", "{}getVulkanExtensions called with null window — returning empty{}", AMBER_YELLOW, RESET);
        return {};
    }

    Uint32 count = 0;
    if (SDL_Vulkan_GetInstanceExtensions(&count) != 0) {
        LOG_WARNING_CAT("SDL3", "{}SDL_Vulkan_GetInstanceExtensions failed in query — returning empty{}", AMBER_YELLOW, RESET);
        return {};
    }

    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!exts) {
        LOG_WARNING_CAT("SDL3", "{}SDL_Vulkan_GetInstanceExtensions returned null pointer — returning empty{}", AMBER_YELLOW, RESET);
        return {};
    }

    std::vector<std::string> result(exts, exts + count);
    LOG_INFO_CAT("SDL3", "{}Vulkan extensions queried successfully — {} extensions — STONEKEY SAFE{}", 
                 EMERALD_GREEN, count, RESET);
    return result;
}

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept
{
    using namespace detail;
    SDL_Event ev;
    quit = toggleFS = false;
    bool resized = false;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                LOG_INFO_CAT("Window", "{}SDL_EVENT_QUIT received — user wants to leave Valhalla{}", RASPBERRY_PINK, RESET);
                quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11) {
                    LOG_INFO_CAT("Window", "{}F11 pressed — toggling fullscreen{}", VALHALLA_GOLD, RESET);
                    toggleFS = true;
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                g_pendingWidth = ev.window.data1;
                g_pendingHeight = ev.window.data2;
                g_resizePending = true;
                g_lastResizeTime = SDL_GetTicks();
                resized = true;
                LOG_INFO_CAT("Window", "{}Window resized to {}x{} — debouncing...{}", 
                             OCEAN_TEAL, g_pendingWidth, g_pendingHeight, RESET);
                break;
        }
    }

    if (g_sdl_window) {
        SDL_GetWindowSizeInPixels(g_sdl_window.get(), &outW, &outH);
    }

    if (g_resizePending && (SDL_GetTicks() - g_lastResizeTime >= RESIZE_DEBOUNCE_MS)) {
        LOG_SUCCESS_CAT("Window", "{}DEFERRED RESIZE ACCEPTED → {}x{} — EMPIRE EXPANDS{}", 
                        VALHALLA_GOLD, g_pendingWidth, g_pendingHeight, RESET);
        g_resizeWidth.store(g_pendingWidth);
        g_resizeHeight.store(g_pendingHeight);
        g_resizeRequested.store(true);
        g_resizePending = false;
    }

    return resized;
}

void toggleFullscreen() noexcept
{
    if (!g_sdl_window) {
        LOG_WARNING_CAT("Window", "{}toggleFullscreen called but no window — ignored{}", AMBER_YELLOW, RESET);
        return;
    }

    Uint32 flags = SDL_GetWindowFlags(g_sdl_window.get());
    bool isFS = (flags & SDL_WINDOW_FULLSCREEN);
    SDL_SetWindowFullscreen(g_sdl_window.get(), !isFS);

    LOG_SUCCESS_CAT("Window", "{}FULLSCREEN {} — VALHALLA MODE {}{}", 
                    isFS ? RASPBERRY_PINK : EMERALD_GREEN,
                    isFS ? "EXITED" : "ENTERED",
                    isFS ? "DEACTIVATED" : "ACTIVATED", RESET);
}

void destroy() noexcept
{
    LOG_INFO_CAT("Dispose", "{}Beginning shutdown — returning canvas to the void{}", PLASMA_FUCHSIA, RESET);
    LOG_INFO_CAT("Dispose", "{}Ellie Fier smiles — the photons fade gracefully{}", DIAMOND_SPARKLE, RESET);

    g_sdl_window.reset();
    SDL_Quit();

    LOG_SUCCESS_CAT("Dispose", "{}SDL3 shutdown complete — Valhalla sleeps{}", EMERALD_GREEN, RESET);
}

} // namespace SDL3Window

// =============================================================================
// GWEN STEFANI JUST WALKED IN AND SAID:
// "THIS SHIT IS BANANAS — B-A-N-A-N-A-S"
// AND WE JUST SKINNED IT.
// NOVEMBER 21, 2025 — FIRST LIGHT ETERNAL
// P I N K   P H O T O N S   E T E R N A L
// =============================================================================