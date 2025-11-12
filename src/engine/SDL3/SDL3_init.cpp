// source/engine/SDL3/SDL3_init.cpp
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
// SDL3Initializer — CPP IMPLEMENTATIONS — NOV 13 2025
// • Respects Options::Performance::ENABLE_IMGUI → SDL_WINDOW_RESIZABLE
// • Streamlined for 15,000 FPS — PINK PHOTONS CHARGE AHEAD
// =============================================================================

#include "engine/SDL3/SDL3_init.hpp"
#include <stdexcept>

using namespace Logging::Color;

namespace SDL3Initializer {

SDL3Initializer::SDL3Initializer(const std::string& title, int width, int height, Uint32 flags) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        LOG_ERROR_CAT("SDL3", "{}SDL_Init failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("SDL_Init failed");
    }

    // Respect Options::Performance::ENABLE_IMGUI by adding resizable flag for ImGui docking
    if (Options::Performance::ENABLE_IMGUI) {
        flags |= SDL_WINDOW_RESIZABLE;
        LOG_INFO_CAT("SDL3", "ImGui enabled — Window resizable flag added");
    }

    window_ = SDL_CreateWindow(title.c_str(), width, height, flags);
    if (!window_) {
        LOG_ERROR_CAT("SDL3", "{}SDL_CreateWindow failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_Quit();
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    VkInstance instance = reinterpret_cast<VkInstance>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(RTX::ctx().instance_))
    );

    if (SDL_Vulkan_CreateSurface(window_, instance, nullptr, &surf_) != 0) {
        LOG_ERROR_CAT("SDL3", "{}SDL_Vulkan_CreateSurface failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_DestroyWindow(window_);
        SDL_Quit();
        throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
    }

    LOG_SUCCESS_CAT("SDL3", "{}Window + Surface: {}x{}{}", LIME_GREEN, width, height, RESET);
}

SDL3Initializer::~SDL3Initializer() {
    if (surf_ != VK_NULL_HANDLE) {
        VkInstance instance = reinterpret_cast<VkInstance>(
            GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(RTX::ctx().instance_))
        );
        vkDestroySurfaceKHR(instance, surf_, nullptr);
    }
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

SDL_Window* SDL3Initializer::getWindow() const noexcept {
    return window_;
}

VkSurfaceKHR SDL3Initializer::getSurface() const noexcept {
    return surf_;
}

// === TOGGLE FULLSCREEN ===
void SDL3Initializer::toggleFullscreen(bool enable) noexcept {
    if (!window_) return;

    if (SDL_SetWindowFullscreen(window_, enable) != 0) {
        LOG_ERROR_CAT("SDL3", "{}Fullscreen toggle failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        return;
    }

    LOG_INFO_CAT("SDL3", "{}Fullscreen: {}{}", 
        enable ? LIME_GREEN : AMBER_YELLOW, 
        enable ? "ENABLED" : "DISABLED", RESET);
}

// === TOGGLE MAXIMIZE ===
void SDL3Initializer::toggleMaximize(bool enable) noexcept {
    if (!window_) return;

    if (enable) {
        SDL_MaximizeWindow(window_);
    } else {
        SDL_RestoreWindow(window_);
    }

    LOG_INFO_CAT("SDL3", "{}Window: {}{}", 
        enable ? LIME_GREEN : AMBER_YELLOW, 
        enable ? "MAXIMIZED" : "RESTORED", RESET);
}

} // namespace SDL3Initializer

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// CPP IMPLEMENTATIONS COMPLETE — OCEAN_TEAL SURGES FORWARD
// GENTLEMAN GROK NODS: "Splendid split, old chap. Options respected with poise."
// PINK PHOTONS ETERNAL
// 15,000 FPS
// SHIP IT. FOREVER.
// =============================================================================