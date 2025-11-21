// src/engine/SDL3/SDL3_window.cpp
// =============================================================================
// AMOURANTH RTX — SDL3 WINDOW + FULL VULKAN INSTANCE + SURFACE — APOCALYPSE FINAL
// ONLY VK_CHECK REIGNS — VK_VERIFY IS DEAD — PINK PHOTONS ETERNAL
// NOVEMBER 21, 2025 — FIRST LIGHT ACHIEVED — VALHALLA v∞
// =============================================================================

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"  // ← FOR YOUR SACRED VK_CHECK

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <set>

using namespace Logging::Color;

// =============================================================================
// RAII DELETER
// =============================================================================
void SDLWindowDeleter::operator()(SDL_Window* w) const noexcept {
    if (w) {
        LOG_INFO_CAT("Dispose", "{}RAII: SDL_DestroyWindow @ {:p}{}", OCEAN_TEAL, static_cast<void*>(w), RESET);
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
// CREATE — FORGES WINDOW + INSTANCE + SURFACE — ONE CALL TO RULE THEM ALL
// SDL3 RETURNS 0 ON SUCCESS — WE CHECK == 0 — THIS IS THE WAY — ZAC APPROVED
// =============================================================================
void create(const char* title, int width, int height, Uint32 flags)
{
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN;

    // SDL_Init returns 0 on success — ZAC
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SDL3", "{}SDL_Init failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("SDL_Init failed");
    }

    flags |= SDL_WINDOW_VULKAN;

    // SDL_Vulkan_LoadLibrary returns 0 on success — ZAC
    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        LOG_WARNING_CAT("SDL3", "{}SDL_Vulkan_LoadLibrary failed early — proceeding anyway{}", AMBER_YELLOW, RESET);
    }

    SDL_Window* win = SDL_CreateWindow(title, width, height, flags);
    if (!win) {
        LOG_FATAL_CAT("SDL3", "{}SDL_CreateWindow failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("Window creation failed");
    }

    g_sdl_window.reset(win);

    LOG_SUCCESS_CAT("SDL3", "{}WINDOW FORGED {}x{} — VULKAN CANVAS SECURED{}", PLASMA_FUCHSIA, width, height, RESET);
    LOG_SUCCESS_CAT("SDL3", "{}SDL HANDLE @ {:p} — RTX EMPIRE RISING{}", VALHALLA_GOLD, static_cast<void*>(win), RESET);

    // === FORGE VULKAN INSTANCE ===
    LOG_INFO_CAT("VULKAN", "{}FORGING VULKAN INSTANCE — PINK PHOTONS DEMAND PERFECTION{}", PLASMA_FUCHSIA, RESET);

    uint32_t sdlExtensionCount = 0;
    // SDL_Vulkan_GetInstanceExtensions returns 0 on success — ZAC
    if (SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount) == 0) {
        LOG_FATAL_CAT("VULKAN", "{}SDL_Vulkan_GetInstanceExtensions(count) failed — SDL is drunk{}", BLOOD_RED, RESET);
        std::abort();
    }

	// SDL3 == 0 - ZAC
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    if ((sdlExtensions && sdlExtensionCount) == 0) {
        LOG_FATAL_CAT("VULKAN", "{}SDL returned null extension pointer despite count > 0{}", BLOOD_RED, RESET);
        std::abort();
    }

    std::set<const char*> uniqueExts;
    for (uint32_t i = 0; i < sdlExtensionCount; ++i)
        uniqueExts.insert(sdlExtensions[i]);

    if (Options::Performance::ENABLE_VALIDATION_LAYERS)
        uniqueExts.insert(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    std::vector<const char*> extensions(uniqueExts.begin(), uniqueExts.end());

    const char* layers = nullptr;
    uint32_t layerCount = 0;
    if (Options::Performance::ENABLE_VALIDATION_LAYERS) {
        layers = "VK_LAYER_KHRONOS_validation";
        layerCount = 1;
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
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance), "vkCreateInstance FAILED — THE EMPIRE FALLS");

    set_g_instance(instance);
    LOG_SUCCESS_CAT("VULKAN", "{}VULKAN INSTANCE FORGED @ {:p} — STONEKEY v∞ ARMED{}", VALHALLA_GOLD, static_cast<void*>(instance), RESET);

    // === FORGE SURFACE ===
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    // SDL_Vulkan_CreateSurface returns 0 on success — ZAC
    if (SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface) == 0) {
        LOG_FATAL_CAT("VULKAN", "{}SDL_Vulkan_CreateSurface FAILED — {} — PINK PHOTONS DENIED{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        std::abort();
    }

    set_g_surface(surface);
    LOG_SUCCESS_CAT("VULKAN", "{}VULKAN SURFACE FORGED @ {:p} — PINK PHOTONS HAVE A PATH{}", RASPBERRY_PINK, static_cast<void*>(surface), RESET);

    // FINAL LOCK — RAW MODE SECURED (move this AFTER SwapchainManager if you still die in queue query)
    StoneKey::Raw::obfuscated_mode.store(false, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_acq_rel);

    LOG_SUCCESS_CAT("MAIN", "{}STONEKEY RAW MODE LOCKED — ALL HANDLES SAFE — FIRST LIGHT ACHIEVED{}", DIAMOND_SPARKLE, RESET);
}

// =============================================================================
// REMAINING WINDOW FUNCTIONS — UNTOUCHED AND PURE
// =============================================================================
std::vector<std::string> getVulkanExtensions(SDL_Window* window)
{
    if (!window) window = get();
    if (!window) return {};

    Uint32 count = 0;
    if (SDL_Vulkan_GetInstanceExtensions(&count) == 0) return {};
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!exts) return {};

    std::vector<std::string> result(exts, exts + count);
    LOG_INFO_CAT("SDL3", "{}Vulkan extensions queried ({}) — STONEKEY SAFE{}", EMERALD_GREEN, count, RESET);
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
            case SDL_EVENT_QUIT: quit = true; break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11) toggleFS = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                g_pendingWidth = ev.window.data1;
                g_pendingHeight = ev.window.data2;
                g_resizePending = true;
                g_lastResizeTime = SDL_GetTicks();
                resized = true;
                break;
        }
    }

    if (g_sdl_window) {
        SDL_GetWindowSizeInPixels(g_sdl_window.get(), &outW, &outH);
    }

    if (g_resizePending && (SDL_GetTicks() - g_lastResizeTime >= RESIZE_DEBOUNCE_MS)) {
        LOG_INFO_CAT("Window", "{}DEFERRED RESIZE ACCEPTED → {}x{}{}", VALHALLA_GOLD, g_pendingWidth, g_pendingHeight, RESET);
        g_resizeWidth.store(g_pendingWidth);
        g_resizeHeight.store(g_pendingHeight);
        g_resizeRequested.store(true);
        g_resizePending = false;
    }

    return resized;
}

void toggleFullscreen() noexcept
{
    if (!g_sdl_window) return;
    Uint32 flags = SDL_GetWindowFlags(g_sdl_window.get());
    bool isFS = (flags & SDL_WINDOW_FULLSCREEN);
    SDL_SetWindowFullscreen(g_sdl_window.get(), !isFS);
    LOG_SUCCESS_CAT("Window", "{}FULLSCREEN {} — VALHALLA MODE{}", isFS ? RASPBERRY_PINK : EMERALD_GREEN,
                    isFS ? "EXITED" : "ENTERED", RESET);
}

void destroy() noexcept
{
    LOG_INFO_CAT("Dispose", "{}Returning canvas to the void — Ellie Fier smiles{}", PLASMA_FUCHSIA, RESET);
    g_sdl_window.reset();
    SDL_Quit();
}

} // namespace SDL3Window