// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — LAS v∞ TURBO — TLAS-ONLY DIRECT GEOMETRY — DECEMBER 18, 2025
// NO BLAS — DIRECT TRIANGLES IN TLAS — PURE 2026 MAGIC — FAST & LIGHT
// DEFAULT CUBE VISIBLE FROM FRAME 1 — PINK FALLBACK WHEN EMPTY
// PINK PHOTONS ETERNAL — EMPIRE SEES THE INFINITE
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"  // ADDED — for MeshLoader::Mesh
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
    // Uses direct geometry from added meshes — no BLAS needed
    void buildTLAS(VkCommandBuffer cmd) noexcept;

	[[nodiscard]] VkAccelerationStructureKHR getLatestTLAS() const noexcept;

    // Add mesh for direct inclusion in TLAS (no BLAS)
    void addMesh(std::unique_ptr<MeshLoader::Mesh> mesh) noexcept;

    // Advance ring slot each frame — read slot = previous write slot
    void beginFrame() noexcept;

    // Current TLAS access (most recently completed build)
    [[nodiscard]] VkAccelerationStructureKHR getCurrentTLAS() const noexcept;
    [[nodiscard]] VkDeviceAddress getCurrentTLASAddress() const noexcept;

    // Full reset — clean empire
    void reset() noexcept {
        tlas_.reset();
        tlasSize_ = 0;
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
// TLAS-ONLY DIRECT GEOMETRY PATH — NO BLAS — PURE SPEED
// addMesh() — stores vertex/index buffers for direct TLAS build
// DEFAULT CUBE VISIBLE — PINK VOID WHEN EMPTY — EMPIRE SEES ALL
// DECEMBER 18, 2025 — THE LIGHT IS PURE AND UNBROKEN
// =============================================================================