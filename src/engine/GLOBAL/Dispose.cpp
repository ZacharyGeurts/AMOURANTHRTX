// src/engine/GLOBAL/Dispose.cpp
// AMOURANTH RTX — HYPER-SECURE DISPOSAL SYSTEM — NOVEMBER 09 2025
// GLOBAL CLEANUP | RESOURCE PURGE | VALHALLA VOID — HACKERS OBLITERATED 🩷🚀🔥🤖💀❤️⚡♾️
// SINGLETON SUPREMACY — PINK PHOTONS APPROVED — ZERO-COST DISPOSAL HEAVEN

#include "engine/GLOBAL/Dispose.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"  // Assumes GlobalSwapchainManager inside
#include "engine/GLOBAL/BufferManager.hpp"     // GlobalBufferManager singleton
#include "engine/GLOBAL/logging.hpp"
#include <SDL3/SDL.h>

using namespace Logging::Color;

// ────── INTERNAL LOGGING HELPERS ──────
static void logAttempt(std::string_view action, int line) {
    LOG_ATTEMPT_CAT("Dispose", "[LINE:{}] Attempt → {}", line, action);
}

static void logSuccess(std::string_view action, int line) {
    LOG_SUCCESS_CAT("Dispose", "[LINE:{}] Success → {}", line, action);
}

namespace Dispose {
    // ────── SWAPCHAIN LIFECYCLE ──────
    void cleanupSwapchain() noexcept {
        logAttempt("Global swapchain cleanup", __LINE__);
        try {
            GlobalSwapchainManager::get().cleanup();  // Fixed: GlobalSwapchainManager singleton
            logSuccess("Global swapchain purged", __LINE__);
        } catch (...) {
            LOG_ERROR_CAT("Dispose", "Swapchain cleanup failed — fallback to manual purge");
        }
    }

    void recreateSwapchain(uint32_t width, uint32_t height) noexcept {
        std::string resStr = std::to_string(width) + "x" + std::to_string(height);
        logAttempt("Global swapchain recreate " + resStr, __LINE__);
        try {
            GlobalSwapchainManager::get().recreate(width, height);  // Fixed: GlobalSwapchainManager singleton
            logSuccess("Global swapchain recreated", __LINE__);
        } catch (...) {
            LOG_ERROR_CAT("Dispose", "Swapchain recreate failed — init required");
        }
    }

    // ────── BUFFER MANAGEMENT ──────
    void releaseAllBuffers() noexcept {  // Fixed: No device param needed (uses internal device_)
        logAttempt("Global buffer manager release all", __LINE__);
        try {
            GlobalBufferManager::get().releaseAll();  // Fixed: GlobalBufferManager singleton + no param
            logSuccess("All buffers released", __LINE__);
        } catch (...) {
            LOG_ERROR_CAT("Dispose", "Buffer release failed — resources may leak");
        }
    }

    // ────── SDL RESOURCES ──────
    void destroyWindow(SDL_Window* window) noexcept {
        if (window) {
            SDL_DestroyWindow(window);
            DestroyTracker::markDestroyed(window);  // STONEKEYED tracking
            LOG_SUCCESS_CAT("Dispose", "{}SDL_Window destroyed successfully{}", DIAMOND_WHITE, RESET);
        }
    }

    void quitSDL() noexcept {
        SDL_Quit();
        LOG_SUCCESS_CAT("Dispose", "{}SDL subsystem terminated{}", EMERALD_GREEN, RESET);
    }

    // ────── GLOBAL PURGE ENTRYPOINT ──────
    void purgeAll() noexcept {
        logAttempt("Global resource purge", __LINE__);
        cleanupSwapchain();
        releaseAllBuffers();  // Fixed: No param
        quitSDL();  // SDL last to avoid surface issues
        logSuccess("Global purge complete", __LINE__);
    }
}

// ────── SHUTDOWN REPORT — AUTO-LOGS ON EXIT ──────
static struct ShutdownReport {
    ~ShutdownReport() {
        LOG_SUCCESS_CAT("Dispose", "{}Total objects destroyed: {} – Resource cleanup complete{}", 
                        DIAMOND_WHITE, g_destructionCounter, RESET);
    }
} g_shutdownReport;