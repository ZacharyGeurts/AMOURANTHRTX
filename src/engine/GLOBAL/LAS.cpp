// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v29.6
// AUTOMAGIC LIGHT ACCELERATION SYSTEM — SINGLETON, LAZY, HYBRID (TRIANGLES + PROCEDURAL AABB BLAS)
// JANUARY 12, 2026 — FIXED sType in BuildSizesInfo + RT flag compatibility with BufferManager suballocation
// PINK PHOTONS ETERNAL — EMPIRE REFORGED — VALIDATION VANQUISHED
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
#include <mutex>

using StoneKey::stone_device;
using StoneKey::stone_graphics_queue;
using RTX::g_ext;

// =============================================================================
// Helper: glm::mat4 → VkTransformMatrixKHR (row-major 3×4)
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
// Constructor — Added RT-specific flags to persistentScratch for safe suballocation
// =============================================================================
RTX::LAS::LAS() {
    LOG_AMOURANTH("LAS v29.6 — AUTOMAGIC SUPER FREE HYBRID EMPIRE — VALIDATION OBLITERATED");

    // Persistent scratch now carries both RT flags — safe for suballocation of AS storage / input buffers
    persistentScratch = BufferManager::create(
        512ULL * 1024 * 1024,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        "LAS_Scratch");

    instanceBuffer = BufferManager::create(
        MAX_INSTANCES * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LAS_InstanceBuffer");

    // Removed unnecessary STORAGE_BUFFER_BIT — only needs build input + address
    universalPrimitivesBuffer = BufferManager::create(
        MAX_PROCEDURALS * sizeof(UniversalPrimitive),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LAS_UniversalPrimitives");

    woopConstantsBuffer = BufferManager::create(
        128ULL * 1024 * 1024,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LAS_WoopConstants");

    proceduralPrimitives.reserve(4096);
    triangleMeshes.reserve(1024);

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

    BufferManager::destroy(persistentScratch);
    BufferManager::destroy(instanceBuffer);
    BufferManager::destroy(universalPrimitivesBuffer);
    BufferManager::destroy(woopConstantsBuffer);

    LOG_SUCCESS_CAT("LAS", "Empire dissolved — light returns to infinity");
}

// =============================================================================
// onResize
// =============================================================================
void RTX::LAS::onResize() {
    LOG_INFO_CAT("LAS", "Resize event received — clearing TLAS and marking dirty");
    clearTLAS();
    tlasDirty = true;
}

// =============================================================================
// Main entry — lazy evaluation
// =============================================================================
VkAccelerationStructureKHR RTX::LAS::getTLAS() {
    ensureReady();
    return tlas;
}

// =============================================================================
// Lazy rebuild (protected by mutex)
// =============================================================================
void RTX::LAS::ensureReady() {
    if (initialized && !tlasDirty && !pendingBlasBuilds && !proceduralDirty && tlas != VK_NULL_HANDLE) {
        return;
    }

    static std::mutex buildMutex;
    std::lock_guard<std::mutex> lock(buildMutex);

    if (initialized && !tlasDirty && !pendingBlasBuilds && !proceduralDirty && tlas != VK_NULL_HANDLE) {
        return;
    }

    LOG_AMOURANTH("LAS awakening — synchronous rebuild");

    if (!initialized) {
        createDefaultHybridScene();
        initialized = true;
    }

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolCI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = StoneKey::stone_graphics_family()
    };
    VK_CHECK(vkCreateCommandPool(stone_device(), &poolCI, nullptr, &pool));

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocCI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocCI, &cmd));

    VkCommandBufferBeginInfo beginCI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginCI));

    if (pendingBlasBuilds || proceduralDirty) {
        BufferManager::uploadToBuffer(universalPrimitivesBuffer,
                                      proceduralPrimitives.data(),
                                      proceduralPrimitives.size() * sizeof(UniversalPrimitive));
        batchBuildAndCompactBLAS(cmd);
        pendingBlasBuilds = false;
        proceduralDirty = false;
    }

    if (tlasDirty) {
        clearTLAS();
        buildHybridTLAS(cmd);
        tlasDirty = false;
    }

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceCI{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VK_CHECK(vkCreateFence(stone_device(), &fenceCI, nullptr, &fence));

    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VK_CHECK(vkQueueSubmit(stone_graphics_queue(), 1, &submit, fence));
    vkWaitForFences(stone_device(), 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(stone_device(), fence, nullptr);
    vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
    vkDestroyCommandPool(stone_device(), pool, nullptr);

    LOG_SUCCESS_CAT("LAS", "Rebuild complete — TLAS ready");
}

// =============================================================================
// Add triangle mesh
// =============================================================================
size_t RTX::LAS::addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex) {
    if (!mesh || mesh->vertices.empty() || mesh->indices.empty() || mesh->indices.size() % 3 != 0) {
        LOG_WARNING_CAT("LAS", "Invalid mesh — skipped");
        return triangleMeshes.size();
    }

    uint64_t vb = BufferManager::create(
        mesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        "LAS_Vertex");

    BufferManager::uploadToBuffer(vb, mesh->vertices.data(), mesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex));

    uint64_t ib = BufferManager::create(
        mesh->indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        "LAS_Index");

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

    LOG_SUCCESS_CAT("LAS", "Triangle mesh added — {} tris", m.primitiveCount);
    return triangleMeshes.size() - 1;
}

// =============================================================================
// Woop (placeholder)
// =============================================================================
void RTX::LAS::precomputeWoopConstants(InternalMesh& m) {
    LOG_WARNING_CAT("LAS", "Woop precompute skipped — using fallback intersection");
}

// =============================================================================
// Add procedural AABB
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

    LOG_AMOURANTH("Procedural AABB added — type {}, scale {:.1f}", static_cast<int>(p.type), scale);
    return proceduralPrimitives.size() - 1;
}

// =============================================================================
// Default test scene
// =============================================================================
void RTX::LAS::createDefaultHybridScene() {
    addProceduralAABB(GeometryType::ProceduralAABB, {0, -20, 0},    30000.0f, 0, glm::mat4(1.0f));
    addProceduralAABB(GeometryType::ProceduralAABB, {200, 0, 200},  100.0f,   1, glm::mat4(1.0f));
    addProceduralAABB(GeometryType::ProceduralAABB, {-300, 0, 400}, 150.0f,   1, glm::mat4(1.0f));
    addProceduralAABB(GeometryType::ProceduralAABB, {0, 300, 0},    50.0f,    2, glm::mat4(1.0f));
    addProceduralAABB(GeometryType::ProceduralAABB, {0, 10, 0},     20000.0f, 3, glm::mat4(1.0f));

    LOG_AMOURANTH("Default hybrid scene forged — infinite terrain + test primitives");
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
        buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(persistentScratch);

        uint32_t primCount = m.primitiveCount;

        VkAccelerationStructureBuildSizesInfoKHR sizes{};
        sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        g_ext.vkGetAccelerationStructureBuildSizesKHR(
            stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo, &primCount, &sizes);

        m.blasStorage = BufferManager::create(
            sizes.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            "LAS_TriBLAS_Storage");

        VkAccelerationStructureCreateInfoKHR createCI{};
        createCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createCI.buffer = BufferManager::getVkBuffer(m.blasStorage);
        createCI.size = sizes.accelerationStructureSize;
        createCI.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createCI, nullptr, &m.blas));

        buildInfo.dstAccelerationStructure = m.blas;

        VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = primCount};
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
        g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

        m.blasBuilt = true;
        built = true;
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
        buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(persistentScratch);

        uint32_t primCount = static_cast<uint32_t>(proceduralPrimitives.size());

        VkAccelerationStructureBuildSizesInfoKHR sizes{};
        sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        g_ext.vkGetAccelerationStructureBuildSizesKHR(
            stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo, &primCount, &sizes);

        proceduralBlasStorage = BufferManager::create(
            sizes.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            "LAS_ProcBLAS_Storage");

        VkAccelerationStructureCreateInfoKHR createCI{};
        createCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createCI.buffer = BufferManager::getVkBuffer(proceduralBlasStorage);
        createCI.size = sizes.accelerationStructureSize;
        createCI.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createCI, nullptr, &proceduralBlas));

        buildInfo.dstAccelerationStructure = proceduralBlas;

        VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = primCount};
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
        g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

        built = true;
    }

    if (built) LOG_SUCCESS_CAT("LAS", "BLAS batch complete");
    return true;
}

// =============================================================================
// TLAS build
// =============================================================================
bool RTX::LAS::buildHybridTLAS(VkCommandBuffer cmd) {
    LOG_AMOURANTH("Building hybrid TLAS");

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

        VkAccelerationStructureDeviceAddressInfoKHR addrCI{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = m.blas
        };
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

        VkAccelerationStructureDeviceAddressInfoKHR addrCI{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = proceduralBlas
        };
        inst.accelerationStructureReference = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &addrCI);

        instances.push_back(inst);
    }

    if (instances.empty()) return false;

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
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(persistentScratch);

    uint32_t instCount = static_cast<uint32_t>(instances.size());

    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &instCount, &sizes);

    tlasStorage = BufferManager::create(
        sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LAS_TLAS_Storage");

    VkAccelerationStructureCreateInfoKHR createCI{};
    createCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createCI.buffer = BufferManager::getVkBuffer(tlasStorage);
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
// Cleanup & utilities
// =============================================================================
void RTX::LAS::clearTLAS() {
    if (tlas) {
        g_ext.vkDestroyAccelerationStructureKHR(stone_device(), tlas, nullptr);
        tlas = VK_NULL_HANDLE;
    }
    BufferManager::destroy(tlasStorage);
    tlasStorage = 0;
}

void RTX::LAS::insertAccelerationStructureBarrier(VkCommandBuffer cmd) {
    VkMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    VkDependencyInfo dep{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &barrier
    };
    g_ext.vkCmdPipelineBarrier2(cmd, &dep);
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
// END — v29.6 — ALL VALIDATION ERRORS RESOLVED — EMPIRE ETERNAL 💖
// =============================================================================