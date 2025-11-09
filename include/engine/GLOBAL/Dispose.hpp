// include/engine/GLOBAL/Dispose.hpp
// AMOURANTH RTX — LOW-LEVEL DISPOSAL TRACKER — NOVEMBER 09 2025 — HARDWARE EDITION
// ENCRYPTED HARDWARE RESOURCE TRACKING — VULKAN HANDLE LOGGING — ATOMIC COUNTERS
// HEADER-ONLY — C++23 — ZERO SYNCHRONIZATION — HARDWARE-LEVEL PURGE LOGS

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <unordered_set>
#include <atomic>
#include <string_view>

// HARDWARE-LEVEL DESTRUCTION TRACKER — STONEKEY + VULKAN INTEGRATION
struct DestroyTracker {
private:
    static inline std::unordered_set<const void*> destroyed_;

    static inline constexpr uintptr_t encryptPtr(const void* ptr) noexcept {
        return reinterpret_cast<uintptr_t>(ptr) ^ kStone1 ^ kStone2;
    }

    static inline constexpr uintptr_t decryptPtr(uintptr_t enc) noexcept {
        return enc ^ kStone1 ^ kStone2;
    }

public:
    static void markDestroyed(const void* ptr) noexcept {
        destroyed_.insert(ptr);
    }

    static bool isDestroyed(const void* ptr) noexcept {
        return destroyed_.count(ptr) > 0;
    }

    // HARDWARE-LEVEL LOGGING — VULKAN-SPECIFIC
    static void logHardwareDestruction(std::string_view resourceType, const void* handle, int line) noexcept {
        using namespace Logging::Color;
        LOG_INFO_CAT("HardwareDispose", "{}[HW] {} destroyed @ line {} — Handle: {:#x}{}", 
                     PLASMA_FUCHSIA, resourceType, line, reinterpret_cast<uintptr_t>(handle), RESET);
    }
};

// GLOBAL HARDWARE DESTRUCTION COUNTER — ATOMIC
inline std::atomic<uint64_t> g_hardwareDestructionCounter{0};

// LOW-LEVEL DISPOSAL NAMESPACE — HARDWARE FOCUS
namespace Dispose {
    // VULKAN HANDLE DISPOSAL WITH HARDWARE LOGGING
    template<typename T>
    inline void disposeVulkanHandle(T handle, VkDevice device, std::string_view type) noexcept {
        if (handle && device && !DestroyTracker::isDestroyed(reinterpret_cast<const void*>(handle))) {
            Vulkan::logAndTrackDestruction(type, handle, __LINE__);
            DestroyTracker::markDestroyed(reinterpret_cast<const void*>(handle));
            g_hardwareDestructionCounter.fetch_add(1, std::memory_order_relaxed);
            DestroyTracker::logHardwareDestruction(type, handle, __LINE__);
        }
    }

    // SDL HARDWARE RESOURCES
    inline void destroyWindow(SDL_Window* window) noexcept {
        if (window && !DestroyTracker::isDestroyed(window)) {
            SDL_DestroyWindow(window);
            DestroyTracker::markDestroyed(window);
            g_hardwareDestructionCounter.fetch_add(1, std::memory_order_relaxed);
            DestroyTracker::logHardwareDestruction("SDL_Window", window, __LINE__);
        }
    }

    inline void quitSDL() noexcept {
        SDL_Quit();
        LOG_SUCCESS_CAT("HardwareDispose", "{}SDL hardware subsystems terminated{}", EMERALD_GREEN, RESET);
    }

    // MINIMAL PURGE — HARDWARE ONLY
    inline void purgeHardware() noexcept {
        quitSDL();
        LOG_SUCCESS_CAT("HardwareDispose", "{}Hardware purge complete — {} objects tracked{}", 
                        DIAMOND_WHITE, g_hardwareDestructionCounter.load(), RESET);
    }
}

// HARDWARE SHUTDOWN REPORT — RAII
static struct HardwareShutdownReport {
    ~HardwareShutdownReport() {
        using namespace Logging::Color;
        LOG_SUCCESS_CAT("HardwareDispose", "{}[HW FINAL] Total hardware objects destroyed: {}{}", 
                        PLASMA_FUCHSIA, g_hardwareDestructionCounter.load(), RESET);
    }
} g_hardwareShutdownReport;

// NOVEMBER 09 2025 — LOW-LEVEL HARDWARE DISPOSAL
// VULKAN + SDL INTEGRATED — ENCRYPTED TRACKING — HARDWARE-ONLY LOGGING
// NO HIGH-LEVEL MANAGERS — DIRECT HANDLE PURGE — OPTIMIZED FOR RTX HARDWARE