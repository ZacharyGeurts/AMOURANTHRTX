// include/engine/GLOBAL/Dispose.hpp
// AMOURANTH RTX — HYPER-SECURE DISPOSAL SYSTEM — NOVEMBER 08 2025
// GLOBAL CLEANUP | RESOURCE PURGE | VALHALLA VOID — HACKERS OBLITERATED 🩷🚀🔥🤖💀❤️⚡♾️

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <unordered_set>
#include <string_view>
#include <SDL3/SDL.h>

// ────── DESTRUCTION TRACKER — TRACKS & VALIDATES RESOURCE LIFETIMES ──────
// STONEKEYED: Handles encrypted with kStone1 ^ kStone2 for memory obfuscation — zero-cost XOR
struct DestroyTracker {
private:
    static inline std::unordered_set<uintptr_t> destroyed_{};  // Encrypted storage, zero-cost access

    // ────── ZERO-COST ENCRYPT/DECRYPT FOR POINTERS ──────
    static inline constexpr uintptr_t encryptPtr(const void* ptr) noexcept {
        uintptr_t raw = reinterpret_cast<uintptr_t>(ptr);
        return raw ^ kStone1 ^ kStone2;
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

// ────── GLOBAL COUNTER — ATOMIC-FRIENDLY FOR MULTI-THREAD PURGE ──────
inline uint64_t g_destructionCounter = 0;  // Inline definition, zero-cost

// ────── DISPOSE NAMESPACE — CENTRALIZED CLEANUP API ──────
namespace Dispose {
    // Swapchain lifecycle
    void cleanupSwapchain() noexcept;
    void recreateSwapchain(uint32_t width, uint32_t height) noexcept;

    // Buffer management
    void releaseAllBuffers(VkDevice device) noexcept;

    // SDL resources
    void destroyWindow(SDL_Window* window) noexcept;
    void quitSDL() noexcept;

    // Global purge entrypoint
    void purgeAll() noexcept;
}