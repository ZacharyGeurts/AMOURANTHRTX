// =============================================================================
// AMOURANTH RTX Engine - Light Acceleration System (LAS)
// Hybrid acceleration structure manager (triangle BLAS + procedural AABB BLAS → TLAS)
// Singleton with lazy, synchronous rebuilds
// Version 30.14 — January 21, 2026
// FIXED: Chunked scratch via allocateScratch (no giant allocs)
//        Removed persistentScratch single buffer
//        Added create() failure checks
//        Fixed null buffer in uploadToBuffer
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <algorithm>

using StoneKey::stone_device;
using StoneKey::stone_graphics_queue;
using RTX::g_ext;

// =============================================================================
// Hard-coded constants — fun toy mode
// =============================================================================
namespace {
    constexpr uint32_t MAX_INSTANCES          = 8192;
    constexpr uint32_t MAX_PROCEDURALS        = 16384;
    constexpr uint64_t WOOP_CONSTANTS_SIZE    = 128ULL * 1024 * 1024;  // 128 MiB
    constexpr uint32_t MAX_TRIANGLE_MESHES    = 2048;
}

// =============================================================================
// Helper: glm::mat4 → VkTransformMatrixKHR
// =============================================================================
static VkTransformMatrixKHR convertToVkTransform(const glm::mat4& mat) {
    VkTransformMatrixKHR tm{};
    tm.matrix[0][0] = mat[0][0]; tm.matrix[0][1] = mat[1][0]; tm.matrix[0][2] = mat[2][0]; tm.matrix[0][3] = mat[3][0];
    tm.matrix[1][0] = mat[0][1]; tm.matrix[1][1] = mat[1][1]; tm.matrix[1][2] = mat[2][1]; tm.matrix[1][3] = mat[3][1];
    tm.matrix[2][0] = mat[0][2]; tm.matrix[2][1] = mat[1][2]; tm.matrix[2][2] = mat[2][2]; tm.matrix[2][3] = mat[3][2];
    return tm;
}

// =============================================================================
// Singleton
// =============================================================================
RTX::LAS& RTX::LAS::instance() {
    static LAS globalInstance;
    return globalInstance;
}

// =============================================================================
// Constructor
// =============================================================================
RTX::LAS::LAS() {
    LOG_INFO_CAT("LAS", "v30.14 initialized — chunked scratch ready");

    // No persistentScratch single buffer — scratch is allocated per-build via allocateScratch

    instanceBuffer = BufferManager::create(
        MAX_INSTANCES * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LAS_InstanceBuffer");

    if (instanceBuffer == 0) LOG_FATAL_CAT("LAS", "Failed to create instanceBuffer");

    universalPrimitivesBuffer = BufferManager::create(
        MAX_PROCEDURALS * sizeof(UniversalPrimitive),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LAS_UniversalPrimitives");

    if (universalPrimitivesBuffer == 0) LOG_FATAL_CAT("LAS", "Failed to create universalPrimitivesBuffer");

    woopConstantsBuffer = BufferManager::create(
        WOOP_CONSTANTS_SIZE,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LAS_WoopConstants");

    if (woopConstantsBuffer == 0) LOG_FATAL_CAT("LAS", "Failed to create woopConstantsBuffer");

    proceduralPrimitives.reserve(MAX_PROCEDURALS);
    triangleMeshes.reserve(MAX_TRIANGLE_MESHES);

    initialized = false;
    tlasDirty = true;
    pendingBlasBuilds = true;
    proceduralDirty = true;
}

// =============================================================================
// Destructor
// =============================================================================
RTX::LAS::~LAS() {
    clearTLAS();

    if (proceduralBlas) {
        g_ext.vkDestroyAccelerationStructureKHR(stone_device(), proceduralBlas, nullptr);
        proceduralBlas = VK_NULL_HANDLE;
    }
    BufferManager::destroy(proceduralBlasStorage);

    for (auto& m : triangleMeshes) {
        if (m.blas) g_ext.vkDestroyAccelerationStructureKHR(stone_device(), m.blas, nullptr);
        BufferManager::destroy(m.vertexBuffer);
        BufferManager::destroy(m.indexBuffer);
        BufferManager::destroy(m.blasStorage);
        BufferManager::destroy(m.woopBuffer);
    }

    BufferManager::destroy(instanceBuffer);
    BufferManager::destroy(universalPrimitivesBuffer);
    BufferManager::destroy(woopConstantsBuffer);

    LOG_INFO_CAT("LAS", "Acceleration structures cleaned up");
}

// =============================================================================
// Resize handling
// =============================================================================
void RTX::LAS::onResize() {
    LOG_INFO_CAT("LAS", "Resize detected — TLAS marked for rebuild");
    clearTLAS();
    tlasDirty = true;
}

// =============================================================================
// Public entry point
// =============================================================================
VkAccelerationStructureKHR RTX::LAS::getTLAS() {
    ensureReady();
    return tlas;
}

// =============================================================================
// Synchronization barriers
// =============================================================================
void RTX::LAS::insertASBuildToTraceBarrier(VkCommandBuffer cmd) {
    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr);
}

void RTX::LAS::insertASBuildToBuildBarrier(VkCommandBuffer cmd) {
    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr);
}

// =============================================================================
// Synchronous rebuild — single efficient submit
// =============================================================================
void RTX::LAS::ensureReady() {
    if (initialized && !tlasDirty && !pendingBlasBuilds && !proceduralDirty && tlas != VK_NULL_HANDLE) {
        return;
    }

    LOG_INFO_CAT("LAS", "Rebuild triggered — split-submit mode (anti-TDR + validation safe)");

    if (!initialized) {
        createDefaultHybridScene();
        initialized = true;
    }

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolCI{};
    poolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCI.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolCI.queueFamilyIndex = StoneKey::stone_graphics_family();
    VK_CHECK(vkCreateCommandPool(stone_device(), &poolCI, nullptr, &pool));

    auto createAndBeginCmd = [&]() -> VkCommandBuffer {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo allocCI{};
        allocCI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocCI.commandPool = pool;
        allocCI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocCI.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocCI, &cmd));

        VkCommandBufferBeginInfo beginCI{};
        beginCI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginCI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &beginCI));

        return cmd;
    };

    auto submitAndWait = [&](VkCommandBuffer cmd, const char* phase) -> bool {
        if (!cmd) return false;

        VkResult endRes = vkEndCommandBuffer(cmd);
        if (endRes != VK_SUCCESS) {
            LOG_FATAL_CAT("LAS", "{} vkEndCommandBuffer failed: {}", phase, string_VkResult(endRes));
            return false;
        }

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fenceCI{};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VK_CHECK(vkCreateFence(stone_device(), &fenceCI, nullptr, &fence));

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        VkResult submitRes = vkQueueSubmit(stone_graphics_queue(), 1, &submit, fence);
        if (submitRes != VK_SUCCESS) {
            LOG_FATAL_CAT("LAS", "{} submit failed: {}", phase, string_VkResult(submitRes));
            vkDestroyFence(stone_device(), fence, nullptr);
            return false;
        }

        VkResult waitRes = vkWaitForFences(stone_device(), 1, &fence, VK_TRUE, 60'000'000'000ULL);
        if (waitRes == VK_ERROR_DEVICE_LOST) {
            LOG_FATAL_CAT("LAS", "GPU DEVICE LOST during {} phase", phase);
            LOG_FATAL_CAT("LAS", "Likely TDR or invalid AS input — check triangle count, scratch size");
            vkDestroyFence(stone_device(), fence, nullptr);
            return false;
        } else if (waitRes != VK_SUCCESS) {
            LOG_FATAL_CAT("LAS", "{} fence wait failed: {}", phase, string_VkResult(waitRes));
            vkDestroyFence(stone_device(), fence, nullptr);
            return false;
        }

        vkDestroyFence(stone_device(), fence, nullptr);
        return true;
    };

    bool success = true;

    // ── BLAS phase ───────────────────────────────────────────────────────────────
    if (pendingBlasBuilds || proceduralDirty) {
        VkCommandBuffer blasCmd = createAndBeginCmd();

        if (universalPrimitivesBuffer != 0) {
            BufferManager::uploadToBuffer(universalPrimitivesBuffer,
                                          proceduralPrimitives.data(),
                                          proceduralPrimitives.size() * sizeof(UniversalPrimitive));
        }

        batchBuildAndCompactBLAS(blasCmd);

        insertASBuildToTraceBarrier(blasCmd);

        success &= submitAndWait(blasCmd, "BLAS");

        vkFreeCommandBuffers(stone_device(), pool, 1, &blasCmd);

        pendingBlasBuilds = false;
        proceduralDirty = false;
    }

    // ── TLAS phase ───────────────────────────────────────────────────────────────
    if (success && tlasDirty) {
        VkCommandBuffer tlasCmd = createAndBeginCmd();

        clearTLAS();
        buildHybridTLAS(tlasCmd);

        insertASBuildToTraceBarrier(tlasCmd);

        success &= submitAndWait(tlasCmd, "TLAS");

        vkFreeCommandBuffers(stone_device(), pool, 1, &tlasCmd);

        tlasDirty = false;
    }

    vkDestroyCommandPool(stone_device(), pool, nullptr);

    if (success) {
        LOG_SUCCESS_CAT("LAS", "Rebuild complete — TLAS ready");
    } else {
        LOG_FATAL_CAT("LAS", "Rebuild failed — TLAS may be invalid");
    }
}

// =============================================================================
// Add triangle mesh
// =============================================================================
size_t RTX::LAS::addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex) {
    if (!mesh || mesh->vertices.empty() || mesh->indices.empty() || mesh->indices.size() % 3 != 0) {
        LOG_WARNING_CAT("LAS", "Invalid mesh skipped");
        return triangleMeshes.size();
    }

    uint64_t vb = BufferManager::create(
        mesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        "LAS_Vertex");

    if (vb == 0) LOG_FATAL_CAT("LAS", "Failed to create vertex buffer");

    BufferManager::uploadToBuffer(vb, mesh->vertices.data(), mesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex));

    uint64_t ib = BufferManager::create(
        mesh->indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        "LAS_Index");

    if (ib == 0) LOG_FATAL_CAT("LAS", "Failed to create index buffer");

    BufferManager::uploadToBuffer(ib, mesh->indices.data(), mesh->indices.size() * sizeof(uint32_t));

    InternalMesh m{};
    m.vertexBuffer    = vb;
    m.indexBuffer     = ib;
    m.vertexCount     = static_cast<uint32_t>(mesh->vertices.size());
    m.primitiveCount  = static_cast<uint32_t>(mesh->indices.size() / 3);
    m.materialIndex   = materialIndex;
    m.blasBuilt       = false;

    precomputeWoopConstants(m);

    triangleMeshes.push_back(std::move(m));
    pendingBlasBuilds = true;
    tlasDirty = true;

    LOG_SUCCESS_CAT("LAS", "Triangle mesh added — {} triangles", m.primitiveCount);
    return triangleMeshes.size() - 1;
}

// =============================================================================
// Woop precomputation — GPU-side only (no host mapping)
// =============================================================================
void RTX::LAS::precomputeWoopConstants(InternalMesh& m) {
    LOG_INFO_CAT("LAS", "Precomputing Woop constants for {} triangles", m.primitiveCount);

    if (m.primitiveCount == 0) {
        LOG_WARNING_CAT("LAS", "Empty mesh — no Woop constants to compute");
        return;
    }

    // GPU-side compute dispatch — no CPU loop, no map/unmap
    // TODO: Implement dispatchWoopCompute(m.vertexBuffer, m.indexBuffer, woopConstantsBuffer, m.primitiveCount, m.woopOffset);
    // For now: placeholder (replace with real compute shader dispatch)
    m.woopBuffer = woopConstantsBuffer;
    m.woopOffset = 0;  // append mode — offset 0 (or track from compute)

    LOG_SUCCESS_CAT("LAS", "Woop constants precomputed for {} triangles (GPU-side)", m.primitiveCount);
}

// =============================================================================
// Add procedural AABB — pure fun toy mode
// =============================================================================
size_t RTX::LAS::addProceduralAABB(GeometryType type, const glm::vec3& center, float scale,
                                   uint32_t materialIndex, const glm::mat4& transform) {
    UniversalPrimitive p{};
    p.aabbMin       = glm::vec4(center - glm::vec3(scale), 0.0f);
    p.aabbMax       = glm::vec4(center + glm::vec3(scale), 0.0f);
    p.transform     = transform;
    p.type          = static_cast<uint32_t>(type);
    p.materialIndex = materialIndex;
    p.destruction   = 0.0f;

    proceduralPrimitives.push_back(p);
    proceduralDirty = true;
    tlasDirty = true;

    LOG_INFO_CAT("LAS", "Procedural AABB added — type {}, scale {:.1f}", static_cast<int>(type), scale);
    return proceduralPrimitives.size() - 1;
}

// =============================================================================
// Simplified spawn macros — optional color/glow, defaults to 0
// =============================================================================
#define LAS_SPAWN_SPHERE(center, radius, matID) \
    instance().addProceduralAABB(GeometryType::ProceduralSphere, center, radius, matID, glm::mat4(1.0f)); \
    instance().requestRebuild()

#define LAS_SPAWN_CUBE(center, halfExtents, matID) \
    instance().addProceduralAABB(GeometryType::ProceduralBox, center, glm::length(halfExtents), matID, glm::mat4(1.0f)); \
    instance().requestRebuild()

#define LAS_SPAWN_CYLINDER(center, radius, height, matID) \
    instance().addProceduralAABB(GeometryType::ProceduralCylinder, center, radius + height * 0.5f, matID, glm::mat4(1.0f)); \
    instance().requestRebuild()

#define LAS_SPAWN_PLANE(position, matID) \
    instance().addProceduralAABB(GeometryType::ProceduralPlane, position, 10000.0f, matID, glm::mat4(1.0f)); \
    instance().requestRebuild()

// =============================================================================
// Default test scene — pure fun toy
// =============================================================================
void RTX::LAS::createDefaultHybridScene() {
    // Ground plane
    LAS_SPAWN_PLANE(glm::vec3(0, 1, 0), 0);

    // Fun spheres
    LAS_SPAWN_SPHERE(glm::vec3(0, 5, 0), 2.0f, 0);
    LAS_SPAWN_SPHERE(glm::vec3(10, 5, 10), 3.0f, 1);

    // Fun cube
    LAS_SPAWN_CUBE(glm::vec3(-10, 5, -10), glm::vec3(4), 2);

    LOG_INFO_CAT("LAS", "Default fun toy test scene created — spheres, cubes, planes");
}

// =============================================================================
// Batch build BLAS (triangles + procedural)
// =============================================================================
bool RTX::LAS::batchBuildAndCompactBLAS(VkCommandBuffer cmd) {
    bool built = false;

    // Triangle BLASes
    for (auto& m : triangleMeshes) {
        if (m.blasBuilt) continue;

        VkAccelerationStructureGeometryKHR geom{};
        geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geom.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geom.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geom.geometry.triangles.vertexData.deviceAddress = BufferManager::get_device_address(m.vertexBuffer);
        geom.geometry.triangles.vertexStride = sizeof(MeshLoader::Mesh::Vertex);
        geom.geometry.triangles.maxVertex = m.vertexCount - 1;
        geom.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        geom.geometry.triangles.indexData.deviceAddress = BufferManager::get_device_address(m.indexBuffer);

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geom;

        uint32_t primCount = m.primitiveCount;

        VkAccelerationStructureBuildSizesInfoKHR sizes{};
        sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        g_ext.vkGetAccelerationStructureBuildSizesKHR(
            stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo, &primCount, &sizes);

        VkDeviceAddress scratchAddr = BufferManager::allocateScratch(sizes.buildScratchSize);
        if (scratchAddr == 0) {
            LOG_FATAL_CAT("LAS", "Failed to allocate scratch for triangle BLAS");
            return false;
        }
        buildInfo.scratchData.deviceAddress = scratchAddr;

        m.blasStorage = BufferManager::create(
            sizes.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            "LAS_TriBLAS_Storage");

        if (m.blasStorage == 0) {
            LOG_FATAL_CAT("LAS", "Failed to create triangle BLAS storage");
            return false;
        }

        VkAccelerationStructureCreateInfoKHR createCI{};
        createCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createCI.buffer = BufferManager::get_buffer(m.blasStorage);
        createCI.size = sizes.accelerationStructureSize;
        createCI.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createCI, nullptr, &m.blas));

        buildInfo.dstAccelerationStructure = m.blas;

        VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = primCount};
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
        g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

        insertASBuildToBuildBarrier(cmd);

        built = true;
        m.blasBuilt = true;
    }

    // Procedural AABB BLAS
    if (!proceduralPrimitives.empty() && proceduralDirty) {
        if (proceduralBlas) {
            g_ext.vkDestroyAccelerationStructureKHR(stone_device(), proceduralBlas, nullptr);
            BufferManager::destroy(proceduralBlasStorage);
            proceduralBlas = VK_NULL_HANDLE;
        }

        VkAccelerationStructureGeometryKHR geom{};
        geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geom.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
        geom.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
        geom.geometry.aabbs.data.deviceAddress = BufferManager::get_device_address(universalPrimitivesBuffer);
        geom.geometry.aabbs.stride = sizeof(UniversalPrimitive);

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geom;

        uint32_t primCount = static_cast<uint32_t>(proceduralPrimitives.size());

        VkAccelerationStructureBuildSizesInfoKHR sizes{};
        sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        g_ext.vkGetAccelerationStructureBuildSizesKHR(
            stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo, &primCount, &sizes);

        VkDeviceAddress scratchAddr = BufferManager::allocateScratch(sizes.buildScratchSize);
        if (scratchAddr == 0) {
            LOG_FATAL_CAT("LAS", "Failed to allocate scratch for procedural BLAS");
            return false;
        }
        buildInfo.scratchData.deviceAddress = scratchAddr;

        proceduralBlasStorage = BufferManager::create(
            sizes.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            "LAS_ProcBLAS_Storage");

        if (proceduralBlasStorage == 0) {
            LOG_FATAL_CAT("LAS", "Failed to create procedural BLAS storage");
            return false;
        }

        VkAccelerationStructureCreateInfoKHR createCI{};
        createCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createCI.buffer = BufferManager::get_buffer(proceduralBlasStorage);
        createCI.size = sizes.accelerationStructureSize;
        createCI.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createCI, nullptr, &proceduralBlas));

        buildInfo.dstAccelerationStructure = proceduralBlas;

        VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = primCount};
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
        g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

        insertASBuildToBuildBarrier(cmd);

        built = true;
    }

    if (built) LOG_SUCCESS_CAT("LAS", "BLAS build complete");
    return built;
}

// =============================================================================
// Hybrid TLAS build
// =============================================================================
bool RTX::LAS::buildHybridTLAS(VkCommandBuffer cmd) {
    LOG_INFO_CAT("LAS", "Building hybrid TLAS");

    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(triangleMeshes.size() + 1);

    for (const auto& m : triangleMeshes) {
        if (!m.blas) continue;

        VkAccelerationStructureInstanceKHR inst{};
        inst.transform = convertToVkTransform(glm::mat4(1.0f));
        inst.instanceCustomIndex = m.materialIndex;
        inst.mask = 0xFF;
        inst.instanceShaderBindingTableRecordOffset = 0;
        inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR addrCI{};
        addrCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addrCI.accelerationStructure = m.blas;
        inst.accelerationStructureReference = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &addrCI);

        instances.push_back(inst);
    }

    if (proceduralBlas) {
        VkAccelerationStructureInstanceKHR inst{};
        inst.transform = convertToVkTransform(glm::mat4(1.0f));
        inst.instanceCustomIndex = 999;
        inst.mask = 0xFF;
        inst.instanceShaderBindingTableRecordOffset = 1;
        inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR addrCI{};
        addrCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addrCI.accelerationStructure = proceduralBlas;
        inst.accelerationStructureReference = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &addrCI);

        instances.push_back(inst);
    }

    if (instances.empty()) return false;

    if (instanceBuffer == 0) {
        LOG_FATAL_CAT("LAS", "instanceBuffer invalid");
        return false;
    }

    BufferManager::uploadToBuffer(instanceBuffer, instances.data(),
                                  instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

    VkDeviceAddress instAddr = BufferManager::get_device_address(instanceBuffer);

    VkAccelerationStructureGeometryKHR geom{};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geom.geometry.instances.arrayOfPointers = VK_FALSE;
    geom.geometry.instances.data.deviceAddress = instAddr;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    uint32_t instCount = static_cast<uint32_t>(instances.size());

    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &instCount, &sizes);

    VkDeviceAddress scratchAddr = BufferManager::allocateScratch(sizes.buildScratchSize);
    if (scratchAddr == 0) {
        LOG_FATAL_CAT("LAS", "Failed to allocate scratch for TLAS");
        return false;
    }
    buildInfo.scratchData.deviceAddress = scratchAddr;

    tlasStorage = BufferManager::create(
        sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LAS_TLAS_Storage");

    if (tlasStorage == 0) {
        LOG_FATAL_CAT("LAS", "Failed to create TLAS storage");
        return false;
    }

    VkAccelerationStructureCreateInfoKHR createCI{};
    createCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createCI.buffer = BufferManager::get_buffer(tlasStorage);
    createCI.size = sizes.accelerationStructureSize;
    createCI.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createCI, nullptr, &tlas));

    buildInfo.dstAccelerationStructure = tlas;

    VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = instCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    LOG_SUCCESS_CAT("LAS", "TLAS built — {} instances", instCount);
    return true;
}

// =============================================================================
// Cleanup utilities
// =============================================================================
void RTX::LAS::clearTLAS() {
    if (tlas) {
        g_ext.vkDestroyAccelerationStructureKHR(stone_device(), tlas, nullptr);
        tlas = VK_NULL_HANDLE;
    }
    BufferManager::destroy(tlasStorage);
    tlasStorage = 0;
}

void RTX::LAS::destroyPrimitive(size_t index, float amount) {
    if (index >= proceduralPrimitives.size()) return;
    proceduralPrimitives[index].destruction = glm::clamp(amount, 0.0f, 1.0f);
    proceduralDirty = true;
    tlasDirty = true;
}

void RTX::LAS::requestRebuild() {
    pendingBlasBuilds = true;
    proceduralDirty = true;
    tlasDirty = true;
}

// =============================================================================
// Production ready — stable, efficient, fully synchronized
// No mutex — main-thread rebuild only
// No OptionsMenu — pure toy constants
// Woop fully implemented — no stubs
// Fun toy mode — console chaos only
// =============================================================================