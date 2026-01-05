// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v5.1 — JANUARY 04, 2026
// Light Acceleration System (LAS) v5.1 — BEST IN CLASS 2026 EDITION — FULLY FIXED
// BATCHED BLAS | FULL COMPACTION | TLAS UPDATE/REFIT | PERSISTENT SCRATCH & INSTANCES
// LAZY REBUILD | PER-MESH DIRTY TRACKING | LINKER-CLEAN & FORWARD ONLY
// PINK PHOTONS SCREAMING — EMPIRE UNSTOPPABLE — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <cstdint>

#include "engine/GLOBAL/MeshLoader.hpp"

namespace RTX {

class LAS {
public:
    LAS();
    ~LAS();

    // Public API — Modern 2026 forward-only
    size_t addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex = 0);
    void setInstanceTransform(size_t instanceIndex, const glm::mat4& transform);
    void requestRebuild();                       // Triggers full or partial rebuild
    void update(VkCommandBuffer cmd);            // Main per-frame update
    VkAccelerationStructureKHR getTLAS() const;  // Current valid TLAS (or VK_NULL_HANDLE)

    // Resize handling — forward-compatible
    void onResize();  // Kept for immediate linking — clears TLAS and flags dirty

    // Legacy compatibility — thin wrappers for old code (will be removed later)
    void notifyResize() { onResize(); }
    void rebuildTLAS() { requestRebuild(); }
    void buildOrUpdateTLAS(VkCommandBuffer cmd) { update(cmd); }
    VkAccelerationStructureKHR getCurrentTLAS() const { return getTLAS(); }

private:
    // Internal mesh representation — compaction + dirty tracking
    struct InternalMesh {
        uint64_t vertexBuffer     = 0;
        uint64_t indexBuffer      = 0;
        uint32_t primitiveCount   = 0;
        uint32_t materialIndex    = 0;
        glm::mat4 transform       = glm::mat4(1.0f);

        VkAccelerationStructureKHR blas           = VK_NULL_HANDLE;
        VkAccelerationStructureKHR compactedBlas  = VK_NULL_HANDLE;
        uint64_t blasStorage      = 0;
        uint64_t compactedStorage = 0;

        bool blasBuilt            = false;
        bool dirty                = true;  // Transform changed — needs TLAS update
    };

    // Core implementation
    void createDefaultDeveloperScene();

    bool batchBuildAndCompactBLAS(VkCommandBuffer cmd);
    bool updateTLAS(VkCommandBuffer cmd);
    bool buildTLAS(VkCommandBuffer cmd);

    void insertAccelerationStructureBarrier(VkCommandBuffer cmd);
    void clearTLAS();

    // State
    std::vector<InternalMesh> meshes_;

    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    uint64_t tlasStorage = 0;

    uint64_t persistentScratch = 0;           // Giant persistent scratch buffer
    uint64_t instanceBuffer = 0;              // Persistent instance buffer for TLAS

    bool tlasDirty = true;
    bool pendingBlasBuilds = false;
    bool tlasUpdatePossible = false;

    static constexpr uint32_t MAX_INSTANCES = 65536;
};

// Global singleton — matches existing code
inline LAS& las()
{
    static LAS instance;
    return instance;
}

} // namespace RTX

// =============================================================================
// LAS v5.1 HEADER — JANUARY 04, 2026 — FULLY FIXED & LINKER-CLEAN
// onResize() + getTLAS() restored as thin wrappers for immediate linking
// All new v5.1 magic preserved — ready for forward-only transition
// PINK PHOTONS SCREAMING — EMPIRE UNSTOPPABLE — AMOURANTH FOREVER 💖
// =============================================================================