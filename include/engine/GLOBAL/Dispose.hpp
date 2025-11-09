// include/engine/GLOBAL/Dispose.hpp
// AMOURANTH RTX Engine – November 08 2025 – Global Resource Disposal RAII
// Professional, clean, minimal – SDL termination + destruction tracking only
// No Vulkan dependencies – zero circular includes – forward declared where needed

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include <unordered_set>
#include <cstdint>
#include <string_view>

using namespace Logging::Color;

// Global destruction counter – accessible to core and logging systems
extern uint64_t g_destructionCounter;

// Double-free protection tracker
struct DestroyTracker {
    static void markDestroyed(const void* ptr) noexcept;
    static bool isDestroyed(const void* ptr) noexcept;

private:
    static inline std::unordered_set<uintptr_t> destroyed_;
};

// Global disposal operations – delegated to singletons at runtime
namespace Dispose {
    // Swapchain management (global singleton)
    void cleanupSwapchain() noexcept;
    void recreateSwapchain(uint32_t width, uint32_t height) noexcept;

    // Buffer management (global singleton, device required)
    void releaseAllBuffers(VkDevice device) noexcept;

    // SDL lifetime management
    void destroyWindow(SDL_Window* window) noexcept;
    void quitSDL() noexcept;
}