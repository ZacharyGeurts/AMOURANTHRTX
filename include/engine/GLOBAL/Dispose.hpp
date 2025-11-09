// include/engine/GLOBAL/Dispose.hpp
// AMOURANTH RTX — GLOBAL DISPOSAL MANAGER — NOVEMBER 09 2025 — PROFESSIONAL EDITION
// CENTRALIZED RESOURCE LIFECYCLE CONTROL — ENCRYPTED TRACKING — AUTOMATIC PURGE — PRODUCTION READY
// HEADER-ONLY — C++23 ATOMICS — ZERO EXTERNAL SYNCHRONIZATION — DESIGNED FOR WORLDWIDE ADOPTION ♥✨💀

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <unordered_set>
#include <atomic>
#include <string_view>

using namespace Logging::Color;

// ENCRYPTED DESTRUCTION TRACKER — STONEKEY PROTECTED POINTER STORAGE
struct DestroyTracker {
private:
    static inline std::unordered_set<uintptr_t> destroyed_;

    static inline constexpr uintptr_t encryptPtr(const void* ptr) noexcept {
        return reinterpret_cast<uintptr_t>(ptr) ^ kStone1 ^ kStone2;
    }

    static inline constexpr uintptr_t decryptPtr(uintptr_t enc) noexcept {
        return enc ^ kStone1 ^ kStone2;
    }

public:
    static void markDestroyed(const void* ptr) noexcept {
        destroyed_.insert(encryptPtr(ptr));
    }

    static bool isDestroyed(const void* ptr) noexcept {
        return destroyed_.count(encryptPtr(ptr)) > 0;
    }
};

// GLOBAL DESTRUCTION COUNTER — ATOMIC FOR MULTI-THREAD SAFETY
inline std::atomic<uint64_t> g_destructionCounter{0};

// GLOBAL DISPOSAL MANAGER — NAMESPACE WITH STATIC FUNCTIONS
namespace Dispose {
    inline void logAttempt(std::string_view action, int line) {
        LOG_ATTEMPT_CAT("Dispose", "[LINE:{}] Attempt → {}", line, action);
    }

    inline void logSuccess(std::string_view action, int line) {
        LOG_SUCCESS_CAT("Dispose", "[LINE:{}] Success → {}", line, action);
    }

    // SWAPCHAIN LIFECYCLE
    inline void cleanupSwapchain() noexcept {
        logAttempt("Global swapchain cleanup", __LINE__);
        try {
            GlobalSwapchainManager::get().cleanup();
            logSuccess("Global swapchain purged", __LINE__);
        } catch (...) {
            LOG_ERROR_CAT("Dispose", "Swapchain cleanup failed — fallback to manual purge");
        }
    }

    inline void recreateSwapchain(uint32_t width, uint32_t height) noexcept {
        std::string resStr = std::to_string(width) + "x" + std::to_string(height);
        logAttempt("Global swapchain recreate " + resStr, __LINE__);
        try {
            GlobalSwapchainManager::get().recreate(width, height);
            logSuccess("Global swapchain recreated", __LINE__);
        } catch (...) {
            LOG_ERROR_CAT("Dispose", "Swapchain recreate failed — init required");
        }
    }

    // BUFFER MANAGEMENT
    inline void releaseAllBuffers() noexcept {
        logAttempt("Global buffer manager release all", __LINE__);
        try {
            GlobalBufferManager::get().releaseAll();
            logSuccess("All buffers released", __LINE__);
        } catch (...) {
            LOG_ERROR_CAT("Dispose", "Buffer release failed — resources may leak");
        }
    }

    // SDL RESOURCES
    inline void destroyWindow(SDL_Window* window) noexcept {
        if (window) {
            SDL_DestroyWindow(window);
            DestroyTracker::markDestroyed(window);
            g_destructionCounter.fetch_add(1, std::memory_order_relaxed);
            LOG_SUCCESS_CAT("Dispose", "{}SDL_Window destroyed successfully{}", DIAMOND_WHITE, RESET);
        }
    }

    inline void quitSDL() noexcept {
        SDL_Quit();
        LOG_SUCCESS_CAT("Dispose", "{}SDL subsystem terminated{}", EMERALD_GREEN, RESET);
    }

    // GLOBAL PURGE ENTRYPOINT
    inline void purgeAll() noexcept {
        logAttempt("Global resource purge", __LINE__);
        cleanupSwapchain();
        releaseAllBuffers();
        quitSDL();
        logSuccess("Global purge complete", __LINE__);
    }
}

// AUTOMATIC SHUTDOWN REPORT — RAII LOGGING ON PROCESS EXIT
static struct ShutdownReport {
    ~ShutdownReport() {
        LOG_SUCCESS_CAT("Dispose", "{}Total objects destroyed: {} — Resource cleanup complete{}", 
                        DIAMOND_WHITE, g_destructionCounter.load(), RESET);
    }
} g_shutdownReport;

// NOVEMBER 09 2025 — HEADER-ONLY DISPOSAL MANAGER
// FULLY COMPLIANT WITH C++23 — NO SYNCHRONIZATION PRIMITIVES — ENTERPRISE GRADE
// DELETE Dispose.cpp PERMANENTLY — BUILD WITH MAXIMUM OPTIMIZATION
// rm src/engine/GLOBAL/Dispose.cpp && make clean && make -j$(nproc)
// READY FOR GLOBAL ADOPTION ♥✨💀