// =============================================================================
// AMOURANTH RTX Engine - Light Acceleration System (LAS)
// Hybrid acceleration structure manager (triangle BLAS + procedural AABB BLAS → TLAS)
// Singleton with lazy, synchronous rebuilds
// Supports triangle meshes and procedural AABBs
// Version 30.5 — January 20, 2026
// Production ready: Proper AS build synchronization (build-to-build + build-to-trace)
// Stable initial build, no device lost
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
    uint32_t    type;           // GeometryType cast to uint32_t
    uint32_t    materialIndex;
    uint32_t    customDataIndex = 0;
    float       destruction     = 0.0f;
};

struct InternalMesh {
    uint64_t vertexBuffer     = 0;
    uint64_t indexBuffer      = 0;
    uint64_t woopBuffer       = 0;
    uint32_t primitiveCount   = 0;
    uint32_t vertexCount      = 0;
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
    static LAS& instance();

    VkAccelerationStructureKHR getTLAS();

    size_t addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex = 0);

    size_t addProceduralAABB(GeometryType type, const glm::vec3& center, float scale,
                             uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f));

    // Future extensions (not yet implemented)
    size_t addLine(const glm::vec3& start, const glm::vec3& end, float thickness = 1.0f, uint32_t materialIndex = 0);
    size_t addPointCloud(const std::vector<glm::vec3>& points, uint32_t materialIndex = 0);

    void setInstanceTransform(size_t instanceIndex, const glm::mat4& transform);
    void destroyPrimitive(size_t index, float amount = 1.0f);

    void onResize();
    void requestRebuild();

    // Legacy aliases
    void notifyResize() { onResize(); }
    void rebuildTLAS() { requestRebuild(); }

private:
    LAS();
    ~LAS();

    void ensureReady();

    void precomputeWoopConstants(InternalMesh& m);
    bool batchBuildAndCompactBLAS(VkCommandBuffer cmd);
    bool buildHybridTLAS(VkCommandBuffer cmd);
    void clearTLAS();
    void createDefaultHybridScene();

    // Synchronization barriers
    void insertASBuildToTraceBarrier(VkCommandBuffer cmd);
    void insertASBuildToBuildBarrier(VkCommandBuffer cmd);

    // State
    std::vector<InternalMesh> triangleMeshes;
    std::vector<UniversalPrimitive> proceduralPrimitives;

    uint64_t persistentScratch          = 0;
    uint64_t instanceBuffer             = 0;
    uint64_t universalPrimitivesBuffer  = 0;
    uint64_t woopConstantsBuffer        = 0;

    VkAccelerationStructureKHR tlas                = VK_NULL_HANDLE;
    VkAccelerationStructureKHR proceduralBlas      = VK_NULL_HANDLE;
    uint64_t                   tlasStorage         = 0;
    uint64_t                   proceduralBlasStorage = 0;

    bool tlasDirty         = true;
    bool pendingBlasBuilds = true;
    bool proceduralDirty   = true;
    bool initialized       = false;

    static constexpr uint32_t MAX_INSTANCES   = 131072;
    static constexpr uint32_t MAX_PROCEDURALS = 131072;
};

} // namespace RTX

// =============================================================================
// LAS header v30.5 — synchronized with production-ready LAS.cpp
// Cleaned: removed excessive commentary, focused on clarity
// Added declarations for new barrier functions
// Ready for clean compilation
// =============================================================================