// =============================================================================
// AMOURANTH RTX Engine - Light Acceleration System (LAS)
// Hybrid acceleration structure manager (triangle BLAS + procedural AABB BLAS → TLAS)
// Singleton with toggleable sync/async rebuilds via Options::LAS::SYNC_REBUILD
// Version 30.26 — January 30, 2026
// - Sync/Async toggle support
// - Consistent MAX_INSTANCES / MAX_PROCEDURALS (131072)
// - Added notifySwapchainRecreated() for swapchain recreation handling
// Note: Options::LAS lives in global namespace — qualify as ::Options::LAS if needed
// Empire stays predictable, debuggable — pink photons breathe free
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <cstdint>
#include <future>
#include <optional>

#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/BufferManager.hpp"

namespace RTX {

enum class GeometryType : uint32_t {
    TriangleMesh        = 0,
    ProceduralSphere    = 1,
    ProceduralBox       = 2,
    ProceduralCylinder  = 3,
    ProceduralPlane     = 4,
    ProceduralCone      = 5,
    ProceduralD4        = 6,
    ProceduralD6        = 7,
    ProceduralD8        = 8,
    ProceduralD10       = 9,
    ProceduralD12       = 10,
    ProceduralD20       = 11,
    ProceduralSDF       = 12,
    ProceduralVolume    = 13,
    ProceduralParticles = 14,
    ProceduralLine      = 15,
    CustomProcedural    = 16
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
    uint32_t primitiveCount   = 0;
    uint32_t vertexCount      = 0;
    uint32_t materialIndex    = 0;
    glm::mat4 transform       = glm::mat4(1.0f);

    VkAccelerationStructureKHR blas           = VK_NULL_HANDLE;
    uint64_t blasStorage      = 0;

    bool blasBuilt            = false;
};

class LAS {
public:
    static LAS& instance();

    VkAccelerationStructureKHR getTLAS();

    size_t addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex = 0);

    size_t addProceduralAABB(GeometryType type, const glm::vec3& center, float scale,
                             uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f));

    size_t addProceduralSphere(const glm::vec3& center, float radius,
                               uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f));

    size_t addProceduralCylinder(const glm::vec3& center, float radius, float height,
                                 uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f));

    size_t addProceduralCone(const glm::vec3& center, float radius, float height,
                             uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f));

    size_t addProceduralD4(const glm::vec3& center, float size,
                           uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f));

    size_t addProceduralD6(const glm::vec3& center, float size,
                           uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f));

    size_t addProceduralD8(const glm::vec3& center, float size,
                           uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f));

    size_t addProceduralD10(const glm::vec3& center, float size,
                            uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f));

    size_t addProceduralD12(const glm::vec3& center, float size,
                            uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f));

    size_t addProceduralD20(const glm::vec3& center, float size,
                            uint32_t materialIndex = 0, const glm::mat4& transform = glm::mat4(1.0f));

    void hotReloadMesh(size_t index, std::unique_ptr<MeshLoader::Mesh> newMesh);

    void setInstanceTransform(size_t index, const glm::mat4& transform);

    void destroyPrimitive(size_t index, float amount = 1.0f);

    void onResize();

    void notifySwapchainRecreated();

private:
    LAS();
    ~LAS();

    void ensureReady();
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

    std::optional<std::promise<std::tuple<VkAccelerationStructureKHR, uint64_t, bool>>> rebuildPromise;
    std::optional<std::future<std::tuple<VkAccelerationStructureKHR, uint64_t, bool>>> rebuildFuture;

    static constexpr uint32_t MAX_INSTANCES   = 131072;
    static constexpr uint32_t MAX_PROCEDURALS = 131072;
};

} // namespace RTX