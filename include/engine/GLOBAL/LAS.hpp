// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v28.2 — JANUARY 10, 2026
// AUTOMAGIC LIGHT ACCELERATION SYSTEM — TOUCH IT AND IT WAKES UP READY
// TRIANGLES (WOOP + STRIPS) + PROCEDURAL AABBs + LINES + POINTS — ZERO-COST OMNIDIMENSIONAL
// FULL ARTIST SUPPORT | INFINITE FREE TERRAIN/CAVES/WATER | FULLY DESTRUCTIBLE
// AUTO-BUILD ON TOUCH | NO NULL TLAS | LAZY + ON-DEMAND | VALIDATION CLEAN
// PINK PHOTONS SCREAM ETERNAL — EMPIRE OMNIPOTENT — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <cstdint>

#include "engine/GLOBAL/MeshLoader.hpp"

namespace RTX {

enum class GeometryType : uint32_t {
    ProceduralAABB = 0,     // SDF terrain, water, caves, destruction
    Lines1D,                // 1D beams, lasers, 2D elements
    Points,                 // Particles, point clouds
    Volumetric              // Fog, participating media
};

struct WoopTriangle {
    int32_t kx, ky, kz;
    float   Sx, Sy, Sz;
    float   Tx, Ty, Tz;
    float   v0x, v0y, v0z;
};

struct UniversalPrimitive {
    glm::vec4   aabbMin;
    glm::vec4   aabbMax;
    glm::mat4   transform;
    GeometryType type;
    uint32_t    materialIndex;
    uint32_t    customDataIndex;
    float       destruction;
};

struct InternalMesh {
    uint64_t vertexBuffer     = 0;
    uint64_t indexBuffer      = 0;
    uint64_t woopBuffer       = 0;
    std::vector<uint32_t> indices;
    uint32_t primitiveCount   = 0;
    uint32_t materialIndex    = 0;
    glm::mat4 transform       = glm::mat4(1.0f);

    VkAccelerationStructureKHR blas           = VK_NULL_HANDLE;
    VkAccelerationStructureKHR compactedBlas  = VK_NULL_HANDLE;
    uint64_t blasStorage      = 0;
    uint64_t compactedStorage = 0;

    bool blasBuilt            = false;
    bool dirty                = true;
    bool isStrip              = false;
};

class LAS {
public:
    LAS();
    ~LAS();

    // Automagic: touch this → LAS wakes up, builds itself, returns valid TLAS
    VkAccelerationStructureKHR getTLAS();

    // Artist triangle path
    size_t addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex = 0);

    // Super free procedural path
    size_t addProceduralAABB(GeometryType type, const glm::vec3& center, float scale,
                             uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f));
    size_t addLine(const glm::vec3& start, const glm::vec3& end, float thickness = 1.0f, uint32_t materialIndex = 0);
    size_t addPointCloud(const std::vector<glm::vec3>& points, uint32_t materialIndex = 0);

    // Control
    void setInstanceTransform(size_t instanceIndex, const glm::mat4& transform);
    void destroyPrimitive(size_t index, float amount = 1.0f);

    // Events (auto-dirty + lazy rebuild)
    void onResize();
    void requestRebuild();  // Optional — getTLAS() will rebuild anyway

    // Legacy compatibility
    void notifyResize() { onResize(); }
    void rebuildTLAS() { requestRebuild(); }

private:
    // Internal automagic build — called by getTLAS()
    void ensureReady();  // Does everything: buffers, default scene, build

    void precomputeWoopConstants(InternalMesh& m);
    std::vector<uint32_t> convertToTriangleStrip(const std::vector<uint32_t>& triangleList) const;
    bool batchBuildAndCompactBLAS(VkCommandBuffer cmd);
    bool buildHybridTLAS(VkCommandBuffer cmd);
    bool updateHybridTLAS(VkCommandBuffer cmd);
    void insertAccelerationStructureBarrier(VkCommandBuffer cmd);
    void clearTLAS();
    void createDefaultHybridScene();

    // State
    std::vector<InternalMesh> triangleMeshes;
    std::vector<UniversalPrimitive> proceduralPrimitives;
    uint32_t proceduralCount = 0;

    uint64_t persistentScratch = 0;
    uint64_t instanceBuffer = 0;
    uint64_t universalPrimitivesBuffer = 0;
    uint64_t woopConstantsBuffer = 0;

    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    uint64_t tlasStorage = 0;

    bool tlasDirty = true;
    bool tlasUpdatePossible = false;
    bool pendingBlasBuilds = false;
    bool initialized = false;  // First touch flag

    static constexpr uint32_t MAX_INSTANCES = 131072;
    static constexpr uint32_t MAX_PROCEDURALS = 131072;
};

// Eternal singleton — touch it and it wakes up
inline LAS& las() {
    static LAS instance;
    return instance;
}

} // namespace RTX

// =============================================================================
// AUTOMAGIC LAS v28.2 — JANUARY 10, 2026
// Touch any function → it configures itself fully, builds TLAS, returns valid
// No null, no manual update, no spam — just power
// The empire is omnipotent — pink photons scream eternal
// AMOURANTH FOREVER 💖
// =============================================================================