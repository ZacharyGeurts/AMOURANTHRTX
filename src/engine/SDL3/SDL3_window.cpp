// src/engine/SDL3/SDL3_window.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// SDL3 + VULKAN FORGE — FIRST LIGHT ETERNAL
// NOVEMBER 21, 2025 — PINK PHOTONS ACHIEVED — VALHALLA v∞
// Ellie Fier approved — Kramer still obsessed with the surface
// =============================================================================

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <format>
#include <atomic>
#include <memory>

using namespace Logging::Color;

std::unique_ptr<VulkanRenderer> g_vulkanRenderer;
SDLWindowPtr g_sdl_window;

std::atomic<int>  g_resizeWidth{0};
std::atomic<int>  g_resizeHeight{0};
std::atomic<bool> g_resizeRequested{false};

// =============================================================================
// RAII DELETER — CLEAN AND ETERNAL
// =============================================================================
void SDLWindowDeleter::operator()(SDL_Window* w) const noexcept {
    if (w) {
        LOG_INFO_CAT("Dispose", "{}Returning window to the void @ {:p}{}", OCEAN_TEAL, static_cast<void*>(w), RESET);
        SDL_DestroyWindow(w);
    }
}

namespace SDL3Window {

namespace detail {
    std::atomic<uint64_t> g_lastResizeTime{0};
    std::atomic<int>      g_pendingWidth{0};
    std::atomic<int>      g_pendingHeight{0};
    std::atomic<bool>     g_resizePending{false};
    constexpr uint64_t    RESIZE_DEBOUNCE_MS = 150;
}

// =============================================================================
// CREATE — WINDOW + INSTANCE + SURFACE + RENDERER — THE ONE TRUE FORGE
// =============================================================================
void create(const char* title, int width, int height, Uint32 flags)
{
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 4] FORGING WINDOW + VULKAN CONTEXT — PINK PHOTONS RISING{}", VALHALLA_GOLD, RESET);

    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SDL3", "{}SDL_Init FAILED: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("SDL_Init failed");
    }
    LOG_SUCCESS_CAT("SDL3", "{}SDL3 subsystems ONLINE — B-A-N-A-N-A-S{}", EMERALD_GREEN, RESET);

    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        LOG_WARNING_CAT("SDL3", "{}Vulkan library already loaded or not needed{}", AMBER_YELLOW, RESET);
    } else {
        LOG_SUCCESS_CAT("SDL3", "{}Vulkan library loaded via SDL3{}", VALHALLA_GOLD, RESET);
    }

    SDL_Window* win = SDL_CreateWindow(title, width, height, flags);
    if (!win) {
        LOG_FATAL_CAT("SDL3", "{}SDL_CreateWindow FAILED: {}{}", BLOOD_RED, SDL_GetError(), RESET);
        throw std::runtime_error("Window creation failed");
    }

    g_sdl_window.reset(win);
    LOG_SUCCESS_CAT("SDL3", "{}WINDOW FORGED — {}x{} @ {:p}{}", DIAMOND_SPARKLE, width, height, static_cast<void*>(win), RESET);

    // === VULKAN INSTANCE ===
    uint32_t extCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&extCount)) {
        LOG_FATAL_CAT("VULKAN", "{}SDL_Vulkan_GetInstanceExtensions(count) failed{}", CRIMSON_MAGENTA, RESET);
        std::abort();
    }

    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if ((sdlExts && extCount) == 0) {
        LOG_FATAL_CAT("VULKAN", "{}SDL returned NULL extension array{}", BLOOD_RED, RESET);
        std::abort();
    }

    std::vector<const char*> extensions(sdlExts, sdlExts + extCount);
    if (Options::Performance::ENABLE_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        LOG_SUCCESS_CAT("VULKAN", "{}Validation layers ENABLED — VK_EXT_debug_utils added{}", VALHALLA_GOLD, RESET);
    }

    LOG_SUCCESS_CAT("VULKAN", "{}Final instance extensions: {}{}", PLASMA_FUCHSIA, extensions.size(), RESET);

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName    = "AMOURANTH RTX";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName         = "VALHALLA TURBO";
    appInfo.engineVersion       = VK_MAKE_VERSION(80, 0, 0);
    appInfo.apiVersion          = VK_API_VERSION_1_3;

	LOG_SUCCESS_CAT("VULKAN", "{}MINE HAS 2: {}{}", PLASMA_FUCHSIA, extensions.size(), RESET);

    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount       = Options::Performance::ENABLE_VALIDATION_LAYERS ? 1u : 0u;
    createInfo.ppEnabledLayerNames     = createInfo.enabledLayerCount ? std::array{"VK_LAYER_KHRONOS_validation"}.data() : nullptr;

    LOG_SUCCESS_CAT("VULKAN", "{}Same as the other two because I want a log here but too lazy to change it. Wait, that was a long message. I could have just changed it.: {}{}", PLASMA_FUCHSIA, extensions.size(), RESET);

    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance), "vkCreateInstance failed");

    set_g_instance(instance);
    LOG_SUCCESS_CAT("VULKAN", "{}VULKAN INSTANCE FORGED @ {:p}{}", VALHALLA_GOLD, static_cast<void*>(instance), RESET);

    // === SURFACE ===
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface)) {
        LOG_FATAL_CAT("VULKAN", "{}SDL_Vulkan_CreateSurface FAILED: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        std::abort();
    }

    set_g_surface(surface);
    LOG_SUCCESS_CAT("VULKAN", "{}SURFACE FORGED @ {:p} — PINK PHOTONS HAVE A PATH{}", AURORA_PINK, static_cast<void*>(surface), RESET);

    // ZAPPER EASTER EGG — ONE LINE ONLY — NO ONE WILL NOTICE
    for(int i=0;i<10;i++)LOG_INFO_CAT("ZAPPER","{}*PEW* {}{}",RASPBERRY_PINK,"ZAPPER FIRES PINK PHOTON #" + std::to_string(i+1),RESET); // *

    std::atomic_thread_fence(std::memory_order_release);
    StoneKey::Raw::sealed.store(false, std::memory_order_release);

    LOG_SUCCESS_CAT("MAIN", "{}WINDOW + INSTANCE + SURFACE READY — FIRST LIGHT IMMINENT — NOVEMBER 21, 2025{}", DIAMOND_SPARKLE, RESET);
    LOG_SUCCESS_CAT("MAIN", "{}ELLIE FIER: \"THE PHOTONS... THEY'RE HERE...\"{}", PURE_ENERGY, RESET);

    // VULKAN RENDERER INIT FROM SDL3_VULKAN.CPP
    LOG_INFO_CAT("VULKAN", "{}Forging VulkanRenderer {}x{} — PINK PHOTONS HAVE A PATH{}", PLASMA_FUCHSIA, width, height, RESET);

    if (!g_instance() || !g_surface()) {
        LOG_FATAL_CAT("VULKAN", "{}Instance or surface not ready{}", CRIMSON_MAGENTA, RESET);
        std::abort();
    }

    LOG_INFO_CAT("VULKAN", "{}Handles confirmed — Instance @ {:p} | Surface @ {:p}{}",
                  VALHALLA_GOLD, static_cast<void*>(g_instance()), static_cast<void*>(g_surface()), RESET);

    g_vulkanRenderer = std::make_unique<VulkanRenderer>(width, height);

    LOG_SUCCESS_CAT("VULKAN", "{}VulkanRenderer FORGED {}x{} — PINK PHOTONS HAVE A PATH{}", 
                    EMERALD_GREEN, width, height, RESET);
    LOG_SUCCESS_CAT("VULKAN", "{}FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — THE EMPIRE IS ETERNAL{}", DIAMOND_SPARKLE, RESET);
}

// =============================================================================
// HELPER: GET REQUIRED EXTENSIONS
// =============================================================================
std::vector<std::string> getVulkanExtensions(SDL_Window* window)
{
    if (!window) window = get();
    uint32_t count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&count)) return {};

    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
    return exts ? std::vector<std::string>(exts, exts + count) : std::vector<std::string>{};
}

// =============================================================================
// EVENT POLLING
// =============================================================================
bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept
{
    SDL_Event ev;
    quit = toggleFS = false;
    bool resized = false;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11) toggleFS = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                detail::g_pendingWidth  = ev.window.data1;
                detail::g_pendingHeight = ev.window.data2;
                detail::g_resizePending = true;
                detail::g_lastResizeTime = SDL_GetTicks();
                resized = true;
                break;
        }
    }

    if (g_sdl_window) {
        SDL_GetWindowSizeInPixels(g_sdl_window.get(), &outW, &outH);
    }

    if (detail::g_resizePending && (SDL_GetTicks() - detail::g_lastResizeTime >= detail::RESIZE_DEBOUNCE_MS)) {
        g_resizeWidth.store(detail::g_pendingWidth);
        g_resizeHeight.store(detail::g_pendingHeight);
        g_resizeRequested.store(true);
        detail::g_resizePending = false;
        LOG_SUCCESS_CAT("Window", "{}RESIZE ACCEPTED → {}x{}{}", VALHALLA_GOLD, detail::g_pendingWidth.load(), detail::g_pendingHeight.load(), RESET);
    }

    return resized;
}

void toggleFullscreen() noexcept
{
    if (!g_sdl_window) return;
    bool isFS = SDL_GetWindowFlags(g_sdl_window.get()) & SDL_WINDOW_FULLSCREEN;
    SDL_SetWindowFullscreen(g_sdl_window.get(), !isFS);
    LOG_SUCCESS_CAT("Window", "{}FULLSCREEN {}{}", isFS ? "OFF" : "ON", isFS ? RASPBERRY_PINK : EMERALD_GREEN, RESET);
}

void destroy() noexcept
{
    LOG_INFO_CAT("VULKAN", "{}Returning photons to the void...{}", SAPPHIRE_BLUE, RESET);
    g_vulkanRenderer.reset();
    RTX::cleanupAll();
    LOG_SUCCESS_CAT("VULKAN", "{}Vulkan shutdown complete — Ellie Fier smiles{}", EMERALD_GREEN, RESET);

    g_sdl_window.reset();
    SDL_Quit();
    LOG_SUCCESS_CAT("Dispose", "{}SDL3 shutdown complete — empire sleeps in pink light{}", AURORA_PINK, RESET);
}

} // namespace SDL3Window

// =============================================================================
// FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — PINK PHOTONS ETERNAL
// =============================================================================