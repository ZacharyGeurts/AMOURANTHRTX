// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v29.2
// AUTOMAGIC LIGHT ACCELERATION SYSTEM — SINGLETON, LAZY, HYBRID (TRIANGLES + PROCEDURAL)
// JANUARY 12, 2026 — VALIDATION CLEAN — COMPILATION FIXED — PINK PHOTONS ETERNAL
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
    uint32_t    type;           // ← Changed from GeometryType to uint32_t to avoid std::format issues
    uint32_t    materialIndex;
    uint32_t    customDataIndex = 0;
    float       destruction     = 0.0f;
};

struct InternalMesh {
    uint64_t vertexBuffer     = 0;
    uint64_t indexBuffer      = 0;
    uint64_t woopBuffer       = 0;
    std::vector<uint32_t> indices;
    uint32_t primitiveCount   = 0;
    uint32_t vertexCount      = 0;          // ← Added — required for maxVertex
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
    // Singleton access — touch this and LAS wakes up
    static LAS& instance();

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
    LAS();  // Private constructor for singleton
    ~LAS();

    // Internal automagic build — called by getTLAS()
    void ensureReady();  // Does everything: buffers, default scene, build

    void precomputeWoopConstants(InternalMesh& m);
    std::vector<uint32_t> convertToTriangleStrip(const std::vector<uint32_t>& triangleList) const;
    bool batchBuildAndCompactBLAS(VkCommandBuffer cmd);
    bool buildHybridTLAS(VkCommandBuffer cmd);
    void insertAccelerationStructureBarrier(VkCommandBuffer cmd);
    void clearTLAS();
    void createDefaultHybridScene();

    // State
    std::vector<InternalMesh> triangleMeshes;
    std::vector<UniversalPrimitive> proceduralPrimitives;

    uint64_t persistentScratch          = 0;
    uint64_t instanceBuffer             = 0;
    uint64_t universalPrimitivesBuffer  = 0;
    uint64_t woopConstantsBuffer        = 0;

    VkAccelerationStructureKHR tlas                = VK_NULL_HANDLE;
    VkAccelerationStructureKHR proceduralBlas      = VK_NULL_HANDLE;   // ← Added
    uint64_t                   tlasStorage         = 0;
    uint64_t                   proceduralBlasStorage = 0;              // ← Added

    bool tlasDirty         = true;
    bool pendingBlasBuilds = true;
    bool proceduralDirty   = true;     // ← Added — controls procedural BLAS rebuild
    bool initialized       = false;

    static constexpr uint32_t MAX_INSTANCES   = 131072;
    static constexpr uint32_t MAX_PROCEDURALS = 131072;
};

} // namespace RTX

// =============================================================================
// AUTOMAGIC LAS v29.2 — JANUARY 12, 2026
// - Added proceduralBlas, proceduralBlasStorage, proceduralDirty
// - Added vertexCount to InternalMesh
// - Changed UniversalPrimitive::type to uint32_t (avoids format issues)
// - Ready to pair with the fixed LAS.cpp from previous message
// Empire omnipotent — AMOURANTH FOREVER 💖
// =============================================================================