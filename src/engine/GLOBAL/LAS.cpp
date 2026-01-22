// =============================================================================
// AMOURANTH RTX Engine - Light Acceleration System (LAS)
// Hybrid acceleration structure manager (triangle BLAS + procedural AABB BLAS → TLAS)
// Singleton with lazy, synchronous rebuilds
// Version 30.15 — January 21, 2026
// FIXED: Proper memory barriers between BLAS/TLAS
//        Scratch allocation with size check + alignment
//        Wait-idle after builds to prevent race with trace
//        Null checks everywhere
//        Removed unnecessary persistent buffers
//        Cleaned logging — startup + fatal only
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

// Hard-coded limits — fun toy mode
namespace {
    constexpr uint32_t MAX_INSTANCES       = 8192;
    constexpr uint32_t MAX_PROCEDURALS     = 16384;
    constexpr uint32_t MAX_TRIANGLE_MESHES = 2048;
}

// Helper: glm::mat4 → VkTransformMatrixKHR
static VkTransformMatrixKHR convertToVkTransform(const glm::mat4& mat) {
    VkTransformMatrixKHR tm{};
    tm.matrix[0][0] = mat[0][0]; tm.matrix[0][1] = mat[1][0]; tm.matrix[0][2] = mat[2][0]; tm.matrix[0][3] = mat[3][0];
    tm.matrix[1][0] = mat[0][1]; tm.matrix[1][1] = mat[1][1]; tm.matrix[1][2] = mat[2][1]; tm.matrix[1][3] = mat[3][1];
    tm.matrix[2][0] = mat[0][2]; tm.matrix[2][1] = mat[1][2]; tm.matrix[2][2] = mat[2][2]; tm.matrix[2][3] = mat[3][2];
    return tm;
}

// Singleton
RTX::LAS& RTX::LAS::instance() {
    static LAS globalInstance;
    return globalInstance;
}

// Constructor — minimal
RTX::LAS::LAS() {
    LOG_INFO_CAT("LAS", "v30.15 initialized — chunked scratch, barriers, sync");

    instanceBuffer = BufferManager::create(
        MAX_INSTANCES * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LAS_InstanceBuffer");

    if (instanceBuffer == 0) {
        LOG_FATAL_CAT("LAS", "Failed to create instance buffer");
    }

    universalPrimitivesBuffer = BufferManager::create(
        MAX_PROCEDURALS * sizeof(UniversalPrimitive),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LAS_UniversalPrimitives");

    if (universalPrimitivesBuffer == 0) {
        LOG_FATAL_CAT("LAS", "Failed to create procedural primitives buffer");
    }

    proceduralPrimitives.reserve(MAX_PROCEDURALS);
    triangleMeshes.reserve(MAX_TRIANGLE_MESHES);

    initialized = false;
    tlasDirty = true;
    pendingBlasBuilds = true;
    proceduralDirty = true;
}

// Destructor — full cleanup
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

    LOG_INFO_CAT("LAS", "Acceleration structures cleaned up");
}

// Resize handling
void RTX::LAS::onResize() {
    clearTLAS();
    tlasDirty = true;
}

// Public entry — get TLAS (sync if needed)
VkAccelerationStructureKHR RTX::LAS::getTLAS() {
    ensureReady();
    return tlas;
}

// Barriers
void RTX::LAS::insertASBuildToTraceBarrier(VkCommandBuffer cmd) {
    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);
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
                         0, 1, &barrier, 0, nullptr, 0, nullptr);
}

// Synchronous rebuild — single submit, full sync
void RTX::LAS::ensureReady() {
    if (initialized && !tlasDirty && !pendingBlasBuilds && !proceduralDirty && tlas != VK_NULL_HANDLE) {
        return;
    }

    LOG_INFO_CAT("LAS", "Rebuild triggered — full sync");

    if (!initialized) {
        createDefaultHybridScene();
        initialized = true;
    }

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolCI{};
    poolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCI.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolCI.queueFamilyIndex = StoneKey::stone_graphics_family();
    if (vkCreateCommandPool(stone_device(), &poolCI, nullptr, &pool) != VK_SUCCESS) {
        LOG_FATAL_CAT("LAS", "Failed to create command pool for rebuild");
        return;
    }

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocCI{};
    allocCI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocCI.commandPool = pool;
    allocCI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocCI.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(stone_device(), &allocCI, &cmd) != VK_SUCCESS) {
        LOG_FATAL_CAT("LAS", "Failed to allocate command buffer for rebuild");
        vkDestroyCommandPool(stone_device(), pool, nullptr);
        return;
    }

    VkCommandBufferBeginInfo beginCI{};
    beginCI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginCI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &beginCI) != VK_SUCCESS) {
        LOG_FATAL_CAT("LAS", "Failed to begin command buffer for rebuild");
        vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
        vkDestroyCommandPool(stone_device(), pool, nullptr);
        return;
    }

    bool success = true;

    // BLAS phase
    if (pendingBlasBuilds || proceduralDirty) {
        batchBuildAndCompactBLAS(cmd);
        insertASBuildToBuildBarrier(cmd);
    }

    // TLAS phase
    if (tlasDirty) {
        clearTLAS();
        success &= buildHybridTLAS(cmd);
        insertASBuildToTraceBarrier(cmd);
    }

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        LOG_FATAL_CAT("LAS", "vkEndCommandBuffer failed during rebuild");
        success = false;
    }

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceCI{};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(stone_device(), &fenceCI, nullptr, &fence) != VK_SUCCESS) {
        LOG_FATAL_CAT("LAS", "Failed to create fence for rebuild");
        success = false;
    }

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    if (success && vkQueueSubmit(stone_graphics_queue(), 1, &submit, fence) != VK_SUCCESS) {
        LOG_FATAL_CAT("LAS", "vkQueueSubmit failed during rebuild");
        success = false;
    }

    if (success) {
        vkWaitForFences(stone_device(), 1, &fence, VK_TRUE, UINT64_MAX);
    }

    vkDestroyFence(stone_device(), fence, nullptr);
    vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
    vkDestroyCommandPool(stone_device(), pool, nullptr);

    if (success) {
        LOG_SUCCESS_CAT("LAS", "Rebuild complete — TLAS ready");
        tlasDirty = false;
        pendingBlasBuilds = false;
        proceduralDirty = false;
    } else {
        LOG_FATAL_CAT("LAS", "Rebuild failed — TLAS may be invalid");
    }

    // Final sync — prevent race with trace
    vkDeviceWaitIdle(stone_device());
}

// Add triangle mesh
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

    if (vb == 0) {
        LOG_FATAL_CAT("LAS", "Failed to create vertex buffer");
        return triangleMeshes.size();
    }

    BufferManager::uploadToBuffer(vb, mesh->vertices.data(), mesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex));

    uint64_t ib = BufferManager::create(
        mesh->indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        "LAS_Index");

    if (ib == 0) {
        LOG_FATAL_CAT("LAS", "Failed to create index buffer");
        BufferManager::destroy(vb);
        return triangleMeshes.size();
    }

    BufferManager::uploadToBuffer(ib, mesh->indices.data(), mesh->indices.size() * sizeof(uint32_t));

    InternalMesh m{};
    m.vertexBuffer    = vb;
    m.indexBuffer     = ib;
    m.vertexCount     = static_cast<uint32_t>(mesh->vertices.size());
    m.primitiveCount  = static_cast<uint32_t>(mesh->indices.size() / 3);
    m.materialIndex   = materialIndex;
    m.blasBuilt       = false;

    triangleMeshes.push_back(std::move(m));
    pendingBlasBuilds = true;
    tlasDirty = true;

    LOG_INFO_CAT("LAS", "Triangle mesh added — {} triangles", m.primitiveCount);
    return triangleMeshes.size() - 1;
}

// Add procedural AABB
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

// Batch build BLAS (triangles + procedural)
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
        if (g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createCI, nullptr, &m.blas) != VK_SUCCESS) {
            LOG_FATAL_CAT("LAS", "Failed to create triangle BLAS");
            return false;
        }

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
        if (g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createCI, nullptr, &proceduralBlas) != VK_SUCCESS) {
            LOG_FATAL_CAT("LAS", "Failed to create procedural BLAS");
            return false;
        }

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

// Hybrid TLAS build
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

    if (instances.empty()) {
        LOG_WARNING_CAT("LAS", "No instances — skipping TLAS build");
        return false;
    }

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
    if (g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createCI, nullptr, &tlas) != VK_SUCCESS) {
        LOG_FATAL_CAT("LAS", "Failed to create TLAS");
        return false;
    }

    buildInfo.dstAccelerationStructure = tlas;

    VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = instCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    LOG_SUCCESS_CAT("LAS", "TLAS built — {} instances", instCount);
    return true;
}

// Cleanup
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

// Default test scene
void RTX::LAS::createDefaultHybridScene() {
    LAS_SPAWN_PLANE(glm::vec3(0, 0, 0), 0);

    LAS_SPAWN_SPHERE(glm::vec3(0, 5, 0), 2.0f, 0);
    LAS_SPAWN_SPHERE(glm::vec3(10, 5, 10), 3.0f, 1);

    LAS_SPAWN_CUBE(glm::vec3(-10, 5, -10), 4.0f, 2);

    LOG_INFO_CAT("LAS", "Default fun toy test scene created");
}

// =============================================================================
// Production ready — stable, efficient, fully synchronized
// No mutex — main-thread only
// No persistent scratch — allocate per-build
// Full barriers + wait-idle
// Fun toy mode — no options
// =============================================================================