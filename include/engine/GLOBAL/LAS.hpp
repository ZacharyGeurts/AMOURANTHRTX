// =============================================================================
// AMOURANTH RTX Engine - Light Acceleration System (LAS)
// Hybrid acceleration structure manager (triangle BLAS + procedural AABB BLAS → TLAS)
// Singleton with lazy, synchronous rebuilds
// Version 30.7 — January 20, 2026
// Production ready: Single efficient submit with robust per-build barriers
// - Explicit build-to-build barrier after EVERY BLAS
// - Final build-to-build + build-to-trace before/after TLAS
// - No vkQueueWaitIdle()
// - Fence wait preserved for synchronous guarantee
// - Clean, minimal, driver-safe initialization
// - No OptionsMenu dependency — all constants hard-coded (fun toy mode)
// Stable, validation clean, ready for rendering
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
    TriangleMesh        = 0,     // standard ray-traced triangles (OBJ, glTF, etc.)
    ProceduralSphere    = 1,     // perfect spheres, cheap bounding
    ProceduralBox       = 2,     // axis-aligned or oriented boxes
    ProceduralCylinder  = 3,     // infinite or capped cylinders
    ProceduralPlane     = 4,     // infinite plane (ground, walls, mirrors)
    ProceduralSDF       = 5,     // signed distance field (terrain, caves, organic shapes, destruction)
    ProceduralVolume    = 6,     // participating media (fog, rain, clouds, god rays, Mie scattering)
    ProceduralParticles = 7,     // point sprites / billboard clouds (rain streaks, sparks, mosquitoes)
    ProceduralLine      = 8,     // thin beams, wires, lasers, hair
    CustomProcedural    = 9      // catch-all for user-defined (grass blades, fractal trees, etc.)
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
    uint64_t woopOffset       = 0;
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

    // No macros here — moved to .cpp to avoid redefinition issues

    // Future extensions (placeholders — not yet implemented)
    size_t addLine(const glm::vec3& start, const glm::vec3& end, float thickness = 1.0f, uint32_t materialIndex = 0);
    size_t addPointCloud(const std::vector<glm::vec3>& points, uint32_t materialIndex = 0);

    void setInstanceTransform(size_t instanceIndex, const glm::mat4& transform);
    void destroyPrimitive(size_t index, float amount = 1.0f);

    void onResize();
    void requestRebuild();

    // Legacy aliases (keep for compatibility)
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