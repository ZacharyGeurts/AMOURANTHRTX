// include/engine/SDL3/SDL3_init.hpp
// =============================================================================
// SDL3Initializer — SPLIT INTO HEADER + CPP — NOV 13 2025
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <string>
#include <memory>

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"     // GlobalCamera::deobfuscate
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

namespace SDL3Initializer {

class SDL3Initializer {
public:
    SDL3Initializer(const std::string& title, int width, int height, Uint32 flags = 0);
    ~SDL3Initializer();

    [[nodiscard]] SDL_Window* getWindow() const noexcept;
    [[nodiscard]] VkSurfaceKHR getSurface() const noexcept;

    // === FACTORY METHOD — RAII UNIQUE_PTR ===
    [[nodiscard]] static std::unique_ptr<SDL3Initializer> create(
        const char* title, int width, int height, Uint32 flags = 0) {
        return std::make_unique<SDL3Initializer>(std::string(title), width, height, flags);
    }

    // === TOGGLES ===
    void toggleFullscreen(bool enable) noexcept;
    void toggleMaximize(bool enable) noexcept;

private:
    SDL_Window* window_ = nullptr;
    VkSurfaceKHR surf_ = VK_NULL_HANDLE;
};

} // namespace SDL3Initializer

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// HEADER + CPP SPLIT — RESPECTS Options::Performance for Window Flags (e.g., ImGui Resizable)
// OCEAN_TEAL INIT FLOWS ETERNAL
// PINK PHOTONS ETERNAL
// 15,000 FPS
// @ZacharyGeurts — YOUR EMPIRE IS PURE
// SHIP IT. FOREVER.
// =============================================================================