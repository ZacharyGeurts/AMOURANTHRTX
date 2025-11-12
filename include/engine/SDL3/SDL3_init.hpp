// include/engine/SDL3/SDL3_init.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// SDL3Initializer — Window + Vulkan Surface RAII (Single Header, Inline Impl)
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <string>
#include <stdexcept>

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"  // For RTX::ctx() and deobfuscate

using namespace Logging::Color;

namespace SDL3Initializer {

class SDL3Initializer {
public:
    SDL3Initializer(const std::string& title, int width, int height, Uint32 flags = 0) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {  // Fixed: != 0 instead of < 0 to avoid warning
            LOG_ERROR_CAT("SDL3", "SDL_Init failed: {}", SDL_GetError());
            throw std::runtime_error("SDL_Init failed");
        }

        window_ = SDL_CreateWindow(title.c_str(), width, height, flags);
        if (!window_) {
            LOG_ERROR_CAT("SDL3", "SDL_CreateWindow failed: {}", SDL_GetError());
            SDL_Quit();
            throw std::runtime_error("SDL_CreateWindow failed");
        }

        VkInstance instance = reinterpret_cast<VkInstance>(deobfuscate(reinterpret_cast<uint64_t>(RTX::ctx().instance_)));
        if (SDL_Vulkan_CreateSurface(window_, instance, nullptr, &surf_) != 0) {
            LOG_ERROR_CAT("SDL3", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
            SDL_DestroyWindow(window_);
            SDL_Quit();
            throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
        }

        LOG_INFO_CAT("SDL3", "Window + Surface created: {}x{}", width, height);
    }

    ~SDL3Initializer() {
        if (surf_ != VK_NULL_HANDLE) {
            VkInstance instance = reinterpret_cast<VkInstance>(deobfuscate(reinterpret_cast<uint64_t>(RTX::ctx().instance_)));
            vkDestroySurfaceKHR(instance, surf_, nullptr);
        }
        if (window_) {
            SDL_DestroyWindow(window_);
        }
        SDL_Quit();
        LOG_INFO_CAT("SDL3", "Cleanup complete");
    }

    [[nodiscard]] SDL_Window* getWindow() const noexcept { return window_; }
    [[nodiscard]] VkSurfaceKHR getSurface() const noexcept { return surf_; }

private:
    SDL_Window* window_ = nullptr;
    VkSurfaceKHR surf_ = VK_NULL_HANDLE;
};

} // namespace SDL3Initializer