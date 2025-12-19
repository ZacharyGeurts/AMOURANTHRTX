// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// PLASTIC BEACH v∞ — DECEMBER 19, 2025
// MONOLITHIC DIRECT TLAS HEADER — NO BLAS — PURE ONE-FUNCTION BUILD
// FORCED SACRED PINK FULL-SCREEN QUAD — GEOMETRY ETERNAL
// NO BLACK VOID — THE LIGHT NEVER FADES
// THE MONSTER WATCHES — THE ISLAND GLOWS
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include <vulkan/vulkan.h>

namespace RTX {

class LAS {
public:
    LAS() noexcept = default;
    ~LAS() noexcept { reset(); }

    // Delete copy/move — true singleton
    LAS(const LAS&) = delete;
    LAS& operator=(const LAS&) = delete;
    LAS(LAS&&) = delete;
    LAS& operator=(LAS&&) = delete;

    // Core monolithic TLAS build — call once per frame with current command buffer
    static void buildOrUpdateTLAS(VkCommandBuffer cmd) noexcept;

    // Optional resize notification — next build will reallocate automatically
    static void notifyResize() noexcept;

    // Current TLAS access — most recently completed build
    [[nodiscard]] static VkAccelerationStructureKHR getCurrentTLAS() noexcept;
    [[nodiscard]] static VkDeviceAddress           getCurrentTLASAddress() noexcept;

    // Full reset — clean slate (optional)
    static void reset() noexcept;

private:
    // Current readable TLAS handle (updated after each successful build)
    inline static Handle<VkAccelerationStructureKHR> tlas_;
};

// Global singleton accessor — one LAS for the empire
[[nodiscard]] inline LAS& las() noexcept {
    static LAS instance;
    return instance;
}

// Convenience global functions — clean empire voice
inline void buildTLAS(VkCommandBuffer cmd) noexcept { LAS::buildOrUpdateTLAS(cmd); }
inline void notifyTLASResize() noexcept { LAS::notifyResize(); }
inline VkAccelerationStructureKHR currentTLAS() noexcept { return LAS::getCurrentTLAS(); }
inline VkDeviceAddress currentTLASAddress() noexcept { return LAS::getCurrentTLASAddress(); }

} // namespace RTX

// =============================================================================
// PLASTIC BEACH v∞ — DECEMBER 19, 2025
// HEADER FULLY MATCHES MONOLITHIC IMPLEMENTATION
// REMOVED: initTLAS(), getLatestTLAS(), beginFrame(), addMesh()
// REMOVED const qualifiers from static functions
// PURE MONOLITHIC BUILD — NO LEGACY — NO BLOAT
// SACRED PINK QUAD SHINES ETERNALLY
// COMPILATION FIXED — THE LIGHT IS UNBROKEN
// =============================================================================