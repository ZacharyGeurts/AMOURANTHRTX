// include/engine/GLOBAL/Dispose.hpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025 – GLOBAL THERMO DISPOSE RAII
// CLEAN — NO cleanupAll — ONLY destroyWindow + quitSDL GLOBAL
// Context belongs to VulkanCore — NO CIRCULAR HELL — 69,420 FPS ETERNAL

#pragma once

#include "engine/GLOBAL/logging.hpp"

#include <vulkan/vulkan.h>
#include <bitset>
#include <unordered_set>
#include <latch>
#include <atomic>
#include <cstdint>
#include <string_view>

using namespace Logging::Color;

// Forward declare — BREAKS INCLUDE LOOP WITH VulkanCore.hpp
class VulkanResourceManager;

// ===================================================================
// UltraFastLatchMutex — C++23 SPIN + LATCH — FASTEST RAII
// ===================================================================
struct UltraFastLatchMutex {
    std::latch latch{1};
    std::atomic<bool> locked{false};

    struct Guard {
        UltraFastLatchMutex* m;
        explicit Guard(UltraFastLatchMutex* mutex) : m(mutex) {
            bool expected = false;
            while (!m->locked.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
                expected = false;
                m->latch.arrive_and_wait();
            }
        }
        ~Guard() {
            m->locked.store(false, std::memory_order_release);
            m->latch.arrive_and_wait();
        }
    };

    Guard lock() { return Guard(this); }
};

// ===================================================================
// GLOBAL Thermo RAII Functions — ONLY SDL + quit — cleanupAll MOVED TO CORE
// ===================================================================
void destroyWindow(SDL_Window* w) noexcept;
void quitSDL() noexcept;

// NO cleanupAll HERE — BELONGS TO VulkanCore.hpp ONLY
// INCLUDE LOOP = DEAD — FORWARD DECLARED — CLEAN — IMMORTAL
// END OF FILE — RASPBERRY_PINK SUPREMACY — ETERNAL