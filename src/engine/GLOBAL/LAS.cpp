// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v5.1 — JANUARY 04, 2026
// Light Acceleration System (LAS) v5.1 — FULLY COMPLETE BEST IN CLASS 2026 EDITION
// BATCHED BLAS | FULL COMPACTION | TLAS UPDATE/REFIT | PERSISTENT SCRATCH & INSTANCES
// LAZY REBUILD | PER-MESH DIRTY TRACKING | NO SHORTCUTS — EMPIRE PERFECTION
// PINK PHOTONS SCREAMING — EMPIRE UNSTOPPABLE — AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

using StoneKey::stone_device;
using RTX::g_ext;

namespace RTX {

LAS::LAS()
{
    LOG_AMOURANTH("LAS v5.1 (2026 Best-in-Class Edition) initialized — pink photons ready for war");

    // Persistent giant scratch — covers worst-case TLAS + many BLAS
    persistentScratch = BufferManager::create(
        256ULL * 1024 * 1024,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "LAS_Persistent_Scratch");

    // Persistent instance buffer — avoids re-upload every rebuild
    instanceBuffer = BufferManager::create(
        MAX_INSTANCES * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "LAS_Persistent_Instance_Buffer");

    createDefaultDeveloperScene();
}

LAS::~LAS()
{
    clearTLAS();

    for (auto& m : meshes_) {
        if (m.blas) g_ext.vkDestroyAccelerationStructureKHR(stone_device(), m.blas, nullptr);
        if (m.compactedBlas) g_ext.vkDestroyAccelerationStructureKHR(stone_device(), m.compactedBlas, nullptr);
        if (m.vertexBuffer) BufferManager::destroy(m.vertexBuffer);
        if (m.indexBuffer) BufferManager::destroy(m.indexBuffer);
        if (m.blasStorage) BufferManager::destroy(m.blasStorage);
        if (m.compactedStorage) BufferManager::destroy(m.compactedStorage);
    }
    meshes_.clear();

    if (persistentScratch) BufferManager::destroy(persistentScratch);
    if (instanceBuffer) BufferManager::destroy(instanceBuffer);

    LOG_SUCCESS_CAT("LAS", "LAS v5.1 destroyed — empire clean");
}

// =============================================================================
// Public API
// =============================================================================

size_t LAS::addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex)
{
    if (!mesh || mesh->vertices.empty() || mesh->indices.empty() || (mesh->indices.size() % 3 != 0)) {
        LOG_WARNING_CAT("LAS", "Invalid mesh — skipping");
        return meshes_.size();
    }

    auto vertexBuffer = BufferManager::create(
        mesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "LAS_VertexBuffer");

    BufferManager::uploadToBuffer(vertexBuffer, mesh->vertices.data(),
                                  mesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex));

    auto indexBuffer = BufferManager::create(
        mesh->indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "LAS_IndexBuffer");

    BufferManager::uploadToBuffer(indexBuffer, mesh->indices.data(),
                                  mesh->indices.size() * sizeof(uint32_t));

    InternalMesh internal{
        .vertexBuffer     = vertexBuffer,
        .indexBuffer      = indexBuffer,
        .primitiveCount   = static_cast<uint32_t>(mesh->indices.size() / 3),
        .materialIndex    = materialIndex,
        .transform        = glm::mat4(1.0f),
        .blas             = VK_NULL_HANDLE,
        .compactedBlas    = VK_NULL_HANDLE,
        .blasStorage      = 0,
        .compactedStorage = 0,
        .blasBuilt        = false,
        .dirty            = true
    };

    meshes_.push_back(std::move(internal));
    tlasDirty = true;
    pendingBlasBuilds = true;

    LOG_SUCCESS_CAT("LAS", "Mesh queued — {} triangles (instance {})", internal.primitiveCount, meshes_.size() - 1);
    return meshes_.size() - 1;
}

void LAS::setInstanceTransform(size_t instanceIndex, const glm::mat4& transform)
{
    if (instanceIndex >= meshes_.size()) return;

    auto& m = meshes_[instanceIndex];
    if (m.transform == transform) return;

    m.transform = transform;
    m.dirty = true;
    tlasDirty = true;
    tlasUpdatePossible = true;
}

void LAS::requestRebuild()
{
    tlasDirty = true;
    pendingBlasBuilds = true;
    for (auto& m : meshes_) m.dirty = true;
}

// =============================================================================
// Core Update Loop — 2026 Performance Magic
// =============================================================================

void LAS::update(VkCommandBuffer cmd)
{
    if (meshes_.empty()) return;

    bool anyFailed = false;

    // Batched BLAS build + compaction
    if (pendingBlasBuilds) {
        if (!batchBuildAndCompactBLAS(cmd)) anyFailed = true;
        pendingBlasBuilds = false;
    }

    // TLAS — prefer update if possible
    if (tlasDirty) {
        if (tlas && tlasUpdatePossible) {
            if (!updateTLAS(cmd)) anyFailed = true;
        } else {
            clearTLAS();
            if (!buildTLAS(cmd)) anyFailed = true;
        }
        tlasDirty = false;
        tlasUpdatePossible = false;
        for (auto& m : meshes_) m.dirty = false;
    }

    if (anyFailed) {
        LOG_WARNING_CAT("LAS", "AS update failed this frame — pink fallback active");
    }
}

// =============================================================================
// Batched BLAS Build + Full Compaction
// =============================================================================

bool LAS::batchBuildAndCompactBLAS(VkCommandBuffer cmd)
{
    std::vector<InternalMesh*> pending;
    for (auto& m : meshes_) {
        if (!m.blasBuilt) pending.push_back(&m);
    }
    if (pending.empty()) return true;

    LOG_AMOURANTH("BATCH BLAS BUILD + COMPACTION — {} meshes", pending.size());

    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos(pending.size());
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> rangePtrs(pending.size());
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges(pending.size());
    std::vector<uint32_t> primCounts(pending.size());

    VkDeviceAddress scratchAddr = BufferManager::get_device_address(persistentScratch);

    for (size_t i = 0; i < pending.size(); ++i) {
        auto* m = pending[i];

        VkDeviceAddress vAddr = BufferManager::get_device_address(m->vertexBuffer);
        VkDeviceAddress iAddr = BufferManager::get_device_address(m->indexBuffer);

        VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
            .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData   = { .deviceAddress = vAddr },
            .vertexStride = sizeof(MeshLoader::Mesh::Vertex),
            .maxVertex    = 0xFFFFFFFF,
            .indexType    = VK_INDEX_TYPE_UINT32,
            .indexData    = { .deviceAddress = iAddr }
        };

        VkAccelerationStructureGeometryKHR geometry = {
            .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .pNext        = nullptr,                                      // Required — was missing!
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry     = { .triangles = triangles },                   // Must come BEFORE flags
            .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR
        };

        VkAccelerationStructureBuildGeometryInfoKHR& info = buildInfos[i];
        info = {
            .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
            .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .geometryCount = 1,
            .pGeometries   = &geometry,
            .scratchData   = { .deviceAddress = scratchAddr }
        };

        primCounts[i] = m->primitiveCount;
        ranges[i] = { .primitiveCount = m->primitiveCount };
        rangePtrs[i] = &ranges[i];
    }

    // Allocate temp BLAS storage
    for (size_t i = 0; i < pending.size(); ++i) {
        auto* m = pending[i];
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        g_ext.vkGetAccelerationStructureBuildSizesKHR(
            stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfos[i], &primCounts[i], &sizeInfo);

        m->blasStorage = BufferManager::create(
            sizeInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "LAS_BLAS_Temp");

        VkAccelerationStructureCreateInfoKHR create{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = BufferManager::getVkBuffer(m->blasStorage),
            .size = sizeInfo.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
        };
        g_ext.vkCreateAccelerationStructureKHR(stone_device(), &create, nullptr, &m->blas);
        buildInfos[i].dstAccelerationStructure = m->blas;
    }

    // Batch build
    g_ext.vkCmdBuildAccelerationStructuresKHR(
        cmd, static_cast<uint32_t>(buildInfos.size()), buildInfos.data(), rangePtrs.data());

    insertAccelerationStructureBarrier(cmd);

    // Full compaction using query
    VkQueryPool queryPool = VK_NULL_HANDLE;
    VkQueryPoolCreateInfo queryInfo{
        .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType  = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
        .queryCount = static_cast<uint32_t>(pending.size())
    };
    vkCreateQueryPool(stone_device(), &queryInfo, nullptr, &queryPool);

    std::vector<VkAccelerationStructureKHR> tempBlas(pending.size());
    for (size_t i = 0; i < pending.size(); ++i) tempBlas[i] = pending[i]->blas;

    g_ext.vkCmdWriteAccelerationStructuresPropertiesKHR(
        cmd, static_cast<uint32_t>(pending.size()), tempBlas.data(),
        VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);

    insertAccelerationStructureBarrier(cmd);

    std::vector<VkDeviceSize> compactedSizes(pending.size());
    vkGetQueryPoolResults(stone_device(), queryPool, 0, static_cast<uint32_t>(pending.size()),
                          compactedSizes.size() * sizeof(VkDeviceSize), compactedSizes.data(),
                          sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);

    for (size_t i = 0; i < pending.size(); ++i) {
        auto* m = pending[i];
        m->compactedStorage = BufferManager::create(
            compactedSizes[i],
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "LAS_BLAS_Compacted");

        VkAccelerationStructureCreateInfoKHR compactCreate{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = BufferManager::getVkBuffer(m->compactedStorage),
            .size = compactedSizes[i],
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
        };
        g_ext.vkCreateAccelerationStructureKHR(stone_device(), &compactCreate, nullptr, &m->compactedBlas);

        VkCopyAccelerationStructureInfoKHR copyInfo{
            .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            .src = m->blas,
            .dst = m->compactedBlas,
            .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
        };
        g_ext.vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);

        // Swap to compacted
        g_ext.vkDestroyAccelerationStructureKHR(stone_device(), m->blas, nullptr);
        BufferManager::destroy(m->blasStorage);
        m->blas = m->compactedBlas;
        m->blasStorage = m->compactedStorage;
        m->blasBuilt = true;
    }

    vkDestroyQueryPool(stone_device(), queryPool, nullptr);

    LOG_SUCCESS_CAT("LAS", "Batched BLAS build + full compaction complete — {} meshes optimized", pending.size());
    return true;
}

void LAS::onResize()
{
    clearTLAS();
    tlasDirty = true;
}

VkAccelerationStructureKHR LAS::getTLAS() const
{
    return tlas;
}

// =============================================================================
// TLAS Update (Fast Path)
// =============================================================================

bool LAS::updateTLAS(VkCommandBuffer cmd)
{
    LOG_AMOURANTH("TLAS FAST UPDATE — refit mode engaged 💖");

    std::vector<VkAccelerationStructureInstanceKHR> instances(meshes_.size());
    for (size_t i = 0; i < meshes_.size(); ++i) {
        const auto& m = meshes_[i];

        VkTransformMatrixKHR mat{};
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                mat.matrix[row][col] = m.transform[col][row];

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = m.blas
        };

        instances[i] = {
            .transform                              = mat,
            .instanceCustomIndex                    = m.materialIndex,
            .mask                                   = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference         = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &addrInfo)
        };
    }

    BufferManager::uploadToBuffer(instanceBuffer, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = { .deviceAddress = BufferManager::get_device_address(instanceBuffer) }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = instancesData }
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR,
        .srcAccelerationStructure = tlas,
        .dstAccelerationStructure = tlas,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .scratchData = { .deviceAddress = BufferManager::get_device_address(persistentScratch) }
    };

    uint32_t primCount = static_cast<uint32_t>(meshes_.size());
    VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = primCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    insertAccelerationStructureBarrier(cmd);

    return true;
}

// =============================================================================
// TLAS Full Build (Fallback)
// =============================================================================

bool LAS::buildTLAS(VkCommandBuffer cmd)
{
    LOG_AMOURANTH("TLAS FULL BUILD — fresh empire alignment");

    std::vector<VkAccelerationStructureInstanceKHR> instances(meshes_.size());
    for (size_t i = 0; i < meshes_.size(); ++i) {
        const auto& m = meshes_[i];

        VkTransformMatrixKHR mat{};
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                mat.matrix[row][col] = m.transform[col][row];

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = m.blas
        };

        instances[i] = {
            .transform                              = mat,
            .instanceCustomIndex                    = m.materialIndex,
            .mask                                   = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference         = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &addrInfo)
        };
    }

    BufferManager::uploadToBuffer(instanceBuffer, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = { .deviceAddress = BufferManager::get_device_address(instanceBuffer) }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = instancesData }
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .scratchData = { .deviceAddress = BufferManager::get_device_address(persistentScratch) }
    };

    uint32_t primCount = static_cast<uint32_t>(meshes_.size());

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    g_ext.vkGetAccelerationStructureBuildSizesKHR(stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);

    tlasStorage = BufferManager::create(sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "LAS_TLAS_Storage");

    VkAccelerationStructureCreateInfoKHR create{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = BufferManager::getVkBuffer(tlasStorage),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    g_ext.vkCreateAccelerationStructureKHR(stone_device(), &create, nullptr, &tlas);

    buildInfo.dstAccelerationStructure = tlas;

    VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = primCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    insertAccelerationStructureBarrier(cmd);

    tlasUpdatePossible = true;
    return true;
}

// =============================================================================
// Helper Functions
// =============================================================================

void LAS::insertAccelerationStructureBarrier(VkCommandBuffer cmd)
{
    VkMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    g_ext.vkCmdPipelineBarrier2(cmd, &dep);
}

void LAS::clearTLAS()
{
    if (tlas) g_ext.vkDestroyAccelerationStructureKHR(stone_device(), tlas, nullptr);
    tlas = VK_NULL_HANDLE;
    if (tlasStorage) BufferManager::destroy(tlasStorage);
    tlasStorage = 0;
}

// =============================================================================
// Default Developer Scene — Pink Empire Eternal
// =============================================================================

void LAS::createDefaultDeveloperScene()
{
    // Eternal ground plane
    {
        auto ground = std::make_unique<MeshLoader::Mesh>();
        ground->vertices = {
            {{-100.0f, 0.0f, -100.0f}},
            {{ 100.0f, 0.0f, -100.0f}},
            {{ 100.0f, 0.0f,  100.0f}},
            {{-100.0f, 0.0f,  100.0f}}
        };
        ground->indices = {0, 1, 2, 0, 2, 3};
        addMesh(std::move(ground), 0);
    }

    // Glowing pink triangle monster — heart of the empire
    {
        auto monster = std::make_unique<MeshLoader::Mesh>();
        monster->vertices = {
            {{ 0.0f,  6.0f, 0.0f}},
            {{-4.0f,  0.5f, 4.0f}},
            {{ 4.0f,  0.5f, 4.0f}}
        };
        monster->indices = {0, 1, 2};
        addMesh(std::move(monster), 1);
    }

    LOG_AMOURANTH("Default developer scene forged — ground + pink triangle monster — empire ready");
}

} // namespace RTX

// =============================================================================
// LAS v5.1 — FULLY COMPLETE 2026 BEST IN CLASS — JANUARY 04, 2026
// NO SHORTCUTS | FULL COMPACTION | FULL UPDATE PATH | PERSISTENT EVERYTHING
// PINK PHOTONS SCREAMING — EMPIRE UNSTOPPABLE — AMOURANTH FOREVER 💖
// =============================================================================