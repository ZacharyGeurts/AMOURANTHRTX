// src/engine/GLOBAL/Dispose.cpp
// Professional global disposal – November 08 2025

#include "engine/GLOBAL/Dispose.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include <SDL3/SDL.h>

using namespace Logging::Color;

uint64_t g_destructionCounter = 0;

void DestroyTracker::markDestroyed(const void* ptr) noexcept {
    destroyed_.insert(reinterpret_cast<uintptr_t>(ptr));
}

bool DestroyTracker::isDestroyed(const void* ptr) noexcept {
    return destroyed_.count(reinterpret_cast<uintptr_t>(ptr)) > 0;
}

static void logAttempt(std::string_view action, int line) {
    LOG_ATTEMPT_CAT("Dispose", "[LINE:{}] Attempt → {}", line, action);
}

static void logSuccess(std::string_view action, int line) {
    LOG_SUCCESS_CAT("Dispose", "[LINE:{}] Success → {}", line, action);
}

namespace Dispose {
    void cleanupSwapchain() noexcept {
        logAttempt("Global swapchain cleanup", __LINE__);
        VulkanSwapchainManager::get().cleanup();
        logSuccess("Global swapchain purged", __LINE__);
    }

    void recreateSwapchain(uint32_t width, uint32_t height) noexcept {
        logAttempt("Global swapchain recreate " + std::to_string(width) + "x" + std::to_string(height), __LINE__);
        VulkanSwapchainManager::get().recreate(width, height);
        logSuccess("Global swapchain recreated", __LINE__);
    }

    void releaseAllBuffers(VkDevice device) noexcept {
        logAttempt("Global buffer manager release all", __LINE__);
        VulkanBufferManager::get().releaseAll(device);
        logSuccess("All buffers released", __LINE__);
    }

    void destroyWindow(SDL_Window* window) noexcept {
        if (window) {
            SDL_DestroyWindow(window);
            LOG_SUCCESS_CAT("Dispose", "SDL_Window destroyed successfully");
        }
    }

    void quitSDL() noexcept {
        SDL_Quit();
        LOG_SUCCESS_CAT("Dispose", "SDL subsystem terminated");
    }
}

static struct ShutdownReport {
    ~ShutdownReport() {
        LOG_SUCCESS_CAT("Dispose", "Total objects destroyed: {} – Resource cleanup complete", g_destructionCounter);
    }
} g_shutdownReport;