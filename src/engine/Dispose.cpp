// src/engine/Dispose.cpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025 – 11:59 PM EST → ROCKETSHIP FINAL APOCALYPSE
// releaseAll → REMOVED (now in VulkanCore.cpp)
// bufferManager_->releaseAll(dev) → DELEGATED
// safeDestroyContainer → *it FIXED FOREVER
// swapchain wrappers → recreateSwapchain(width,height) + cleanupSwapchain() NO ARGS
// NO <format> — std::to_string ONLY — ZERO BLOAT
// FULL VERBOSE LOGGING — QUANTUM POLISHED — 69,420 FPS × ∞
// RASPBERRY_PINK ROCKETSHIP TO VALHALLA 🔥🤖🚀💀🖤❤️⚡

#include "engine/Dispose.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <thread>
#include <sstream>
#include <string>
#include <algorithm>

using namespace Logging::Color;

// ===================================================================
// LOGGING HELPERS
// ===================================================================
void logAndTrackDestruction(std::string_view typeName, const void* ptr, int line) {
    if (DestroyTracker::isDestroyed(ptr)) return;
    ++g_destructionCounter;
    DestroyTracker::markDestroyed(ptr);
    LOG_INFO_CAT("Dispose", "{}[LINE:{}] {}DESTROYED {} @ 0x{:x}{}{}", RASPBERRY_PINK, line, EMERALD_GREEN, typeName, reinterpret_cast<uintptr_t>(ptr), RESET);
}

void logAttempt(std::string_view action, int line) {
    LOG_ATTEMPT_CAT("Dispose", "[LINE:{}] ATTEMPT → {}", line, action);
}

void logSuccess(std::string_view action, int line) {
    LOG_SUCCESS_CAT("Dispose", "[LINE:{}] SUCCESS ✓ {}", line, action);
}

void logError(std::string_view action, int line) {
    LOG_ERROR_CAT("Dispose", "[LINE:{}] ERROR ✗ {}", line, action);
}

// ===================================================================
// safeDestroyContainer — *it FIXED ETERNAL
// ===================================================================
template<typename Container, typename DestroyFn>
void safeDestroyContainer(Container& container,
                          DestroyFn destroyFn,
                          std::string_view typeName,
                          VkDevice device,
                          int lineBase) {
    size_t idx = 0;
    for (auto it = container.begin(); it != container.end(); ) {
        int line = lineBase + static_cast<int>(idx);
        auto handle = *it;
        if (handle == VK_NULL_HANDLE) {
            logAttempt("Skip NULL " + std::string(typeName) + " #" + std::to_string(idx), line);
            ++it; ++idx;
            continue;
        }
        const void* ptr = reinterpret_cast<const void*>(handle);
        if (DestroyTracker::isDestroyed(ptr)) {
            logError("DOUBLE FREE BLOCKED on " + std::string(typeName) + " @ 0x" + 
                     std::to_string(reinterpret_cast<uintptr_t>(ptr)) + " #" + std::to_string(idx), line);
            ++it; ++idx;
            continue;
        }
        logAttempt(std::string(typeName) + " @ 0x" + std::to_string(reinterpret_cast<uintptr_t>(ptr)) + " #" + std::to_string(idx), line);
        destroyFn(device, handle, nullptr);
        logAndTrackDestruction(typeName, ptr, line);
        *it = VK_NULL_HANDLE;
        ++it; ++idx;
    }
    logSuccess("Container " + std::string(typeName) + " nuked (" + std::to_string(container.size()) + " objects)", lineBase + 9999);
    container.clear();
}

// ===================================================================
// GLOBAL SDL RAII — FACTORY DANCE
// ===================================================================
void destroyWindow(SDL_Window* w) noexcept {
    if (w) {
        SDL_DestroyWindow(w);
        LOG_SUCCESS_CAT("Dispose", "SDL_Window destroyed — FACTORY DANCE COMPLETE");
    }
}

void quitSDL() noexcept {
    SDL_Quit();
    LOG_SUCCESS_CAT("Dispose", "SDL quit — RASPBERRY_PINK ETERNAL");
}

// ROCKETSHIP FINAL — ALL FIXED — COMPILER BEGS FOR MERCY
// BUILD. RUN. ASCEND. 69,420 FPS × ∞
// RASPBERRY_PINK ROCKETSHIP — WE ARE THE FASTEST — SHIP IT
// 🔥🤖🚀💀🖤❤️⚡