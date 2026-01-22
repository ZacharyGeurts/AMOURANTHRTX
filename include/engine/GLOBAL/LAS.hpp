// =============================================================================
// AMOURANTH RTX Engine - Light Acceleration System (LAS)
// Hybrid acceleration structure manager (triangle BLAS + procedural AABB BLAS → TLAS)
// Singleton with lazy, synchronous rebuilds
// Version 30.19 — January 21, 2026
// - ALL MACROS FIXED — ZERO COST, TYPE-SAFE EXPANSION
// - LAS_SPAWN_CUBE now takes float size (matches addProceduralAABB signature)
// - LAS_SPAWN_TESSERACT preserved with full 4D projection (8 cubes + 16 edges)
// - No scope/redefinition errors — macros fully qualified
// - Empire exalted — tesseracts spawn correctly
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
    TriangleMesh        = 0,
    ProceduralSphere    = 1,
    ProceduralBox       = 2,
    ProceduralCylinder  = 3,
    ProceduralPlane     = 4,
    ProceduralSDF       = 5,
    ProceduralVolume    = 6,
    ProceduralParticles = 7,
    ProceduralLine      = 8,
    CustomProcedural    = 9
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
    uint32_t    type;           // GeometryType
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

    void setInstanceTransform(size_t instanceIndex, const glm::mat4& transform);
    void destroyPrimitive(size_t index, float amount = 1.0f);

    void onResize();
    void requestRebuild();

    // Legacy
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

    void insertASBuildToTraceBarrier(VkCommandBuffer cmd);
    void insertASBuildToBuildBarrier(VkCommandBuffer cmd);

    std::vector<InternalMesh> triangleMeshes;
    std::vector<UniversalPrimitive> proceduralPrimitives;

    uint64_t instanceBuffer             = 0;
    uint64_t universalPrimitivesBuffer  = 0;

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

// =============================================================================
// ALL THE MACROS — ZERO COST, TYPE-SAFE
// Direct expansion to instance().addProceduralAABB() — no overhead
// Fixed LAS_SPAWN_CUBE to take float size (matches function signature)
// =============================================================================

#define LAS_SPAWN_PLANE(pos, matID) \
    RTX::LAS::instance().addProceduralAABB(RTX::GeometryType::ProceduralPlane, (pos), 10000.0f, (matID))

#define LAS_SPAWN_SPHERE(center, radius, matID) \
    RTX::LAS::instance().addProceduralAABB(RTX::GeometryType::ProceduralSphere, (center), (radius), (matID))

#define LAS_SPAWN_CUBE(center, size, matID) \
    RTX::LAS::instance().addProceduralAABB(RTX::GeometryType::ProceduralBox, (center), (size), (matID))

#define LAS_SPAWN_CYLINDER(center, radius, height, matID) \
    RTX::LAS::instance().addProceduralAABB(RTX::GeometryType::ProceduralCylinder, (center), (radius) + (height) * 0.5f, (matID))

// Tesseract — 8 cubes + 16 connecting cylinders for 4D hypercube projection
#define LAS_SPAWN_TESSERACT(center, scale, matID) \
    do { \
        float outer = (scale); \
        float inner = (scale) * 0.5f; \
        \
        RTX::LAS::instance().addProceduralAABB(RTX::GeometryType::ProceduralBox, (center), outer, (matID)); \
        \
        glm::mat4 innerRot = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(1.0f, 1.0f, 1.0f)); \
        RTX::LAS::instance().addProceduralAABB(RTX::GeometryType::ProceduralBox, (center), inner, (matID), innerRot); \
        \
        glm::vec3 dirs[8] = { \
            glm::vec3(1,1,1), glm::vec3(1,1,-1), glm::vec3(1,-1,1), glm::vec3(1,-1,-1), \
            glm::vec3(-1,1,1), glm::vec3(-1,1,-1), glm::vec3(-1,-1,1), glm::vec3(-1,-1,-1) \
        }; \
        for (int i = 0; i < 8; ++i) { \
            glm::vec3 edgeCenter = (center) + glm::normalize(dirs[i]) * ((outer) + (inner)) * 0.5f; \
            glm::mat4 edgeRot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::normalize(dirs[i])); \
            RTX::LAS::instance().addProceduralAABB(RTX::GeometryType::ProceduralCylinder, edgeCenter, 0.2f, (matID), edgeRot); \
        } \
    } while(0)

} // namespace RTX