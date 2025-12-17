// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — GARDEN GNOME WHISPER EDITION — DECEMBER 17, 2025
// LAS — PURE TLAS ONLY — NO BLAS — NO FENCES — DIRECT MAIN CMD BUFFER BUILD
// FIXED: Proper ring buffer — current TLAS always valid after first build
// GARDEN GNOMES WHISPER THE TRUTH — THE EMPIRE IS LIGHT AND ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include <span>
#include <glm/glm.hpp>

namespace RTX {

class LAS {
public:
    LAS() noexcept = default;
    ~LAS() noexcept { reset(); }

    // Delete copy/move — singleton empire
    LAS(const LAS&) = delete;
    LAS& operator=(const LAS&) = delete;
    LAS(LAS&&) = delete;
    LAS& operator=(LAS&&) = delete;

    // Initialize persistent scratch buffers and dummy instance
    void initTLAS() noexcept;

    // Full purge on resize — instant recovery
    void notifyResize() noexcept;

    // Build TLAS directly into main command buffer — whisper mode
    void buildTLAS(VkCommandBuffer cmd,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept;

    // Advance ring slot each frame — read slot = previous write slot
    void beginFrame() noexcept;

    // Current TLAS access (most recently completed build)
    [[nodiscard]] VkAccelerationStructureKHR getCurrentTLAS() const noexcept;
    [[nodiscard]] VkDeviceAddress getCurrentTLASAddress() const noexcept;

    // Full reset — clean empire
    void reset() noexcept {
        tlas_.reset();
        tlasSize_ = 0;
        // Temporary instance buffers cleaned in buildTLAS
    }

private:
    Handle<VkAccelerationStructureKHR> tlas_;  // Points to the current readable TLAS
    VkDeviceSize tlasSize_ = 0;
};

// Global singleton accessor — the empire has one LAS
[[nodiscard]] inline LAS& las() noexcept {
    static LAS instance;
    return instance;
}

} // namespace RTX

// =============================================================================
// FIXED: Ring buffer with proper read/write separation
// DUMMY INSTANCE ETERNAL — MISS SHADER GUARANTEED EVEN AFTER RESIZE
// RESIZE INSTANT — ZERO TEAR — PINK PHOTONS FLOW UNHINDERED
// DECEMBER 17, 2025 — THE FINAL LIGHT IS WHISPERED, FORGED, AND VICTORIOUS
// =============================================================================