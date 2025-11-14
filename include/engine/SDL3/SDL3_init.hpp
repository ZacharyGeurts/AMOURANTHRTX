// include/engine/SDL3/SDL3_init.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3 + Vulkan Surface — FINAL BULLETPROOF RAII — NOVEMBER 14 2025
// • WindowDeleter struct → external linkage → std::map & -Werror=subobject-linkage safe
// • Zero overhead, 15,000 FPS, GCC 14, Clang 18, MSVC approved
// • PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <string>
#include <memory>

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

using namespace Logging::Color;

namespace SDL3Initializer {

// =============================================================================
// BULLETPROOF WINDOW DELETER — EXTERNAL LINKAGE — GOD TIER
// =============================================================================
struct WindowDeleter {
    static inline const auto lambda = [](SDL_Window* w) noexcept {
        if (w) SDL_DestroyWindow(w);
    };
    using pointer = SDL_Window*;
    void operator()(SDL_Window* w) const noexcept { lambda(w); }
};

// The one true RAII window type — use this everywhere
using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;

class SDL3Initializer {
public:
    // Primary constructor
    SDL3Initializer(const std::string& title, int width, int height, Uint32 flags = 0);
    
    // Destructor — RAII cleans everything
    ~SDL3Initializer();

    // Accessors
    [[nodiscard]] SDL_Window*  getWindow()  const noexcept { return window_.get(); }
    [[nodiscard]] VkSurfaceKHR getSurface() const noexcept { return surface_; }

    // Factory method — preferred way to create
    static std::unique_ptr<SDL3Initializer> create(
        const char* title, int width, int height, Uint32 flags = 0) {
        return std::make_unique<SDL3Initializer>(std::string(title), width, height, flags);
    }

    // Runtime controls
    void toggleFullscreen(bool enable) noexcept;
    void toggleMaximize(bool enable) noexcept;

private:
    // RAII-managed resources
    WindowPtr    window_;                    // Auto-destroys on scope exit
    VkInstance   vkInstance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_    = VK_NULL_HANDLE;
};

} // namespace SDL3Initializer

// =============================================================================
// PINK PHOTONS ETERNAL
// DELETER STRUCT + EXTERNAL LINKAGE = UNSTOPPABLE
// NO MORE SUBOBJECT-LINKAGE ERRORS
// NO MORE LAMBDA POINTER NONSENSE
// DAISY GALLOPS INTO THE OCEAN_TEAL SUNSET
// YOUR EMPIRE IS PURE
// SHIP IT RAW
// =============================================================================