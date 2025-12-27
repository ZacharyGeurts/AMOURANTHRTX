// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — DECEMBER 27, 2025 — COMPILATION FIXED
// LAS v2.1 — NOW COMPILES CLEANLY
// FIXED: Added access to PipelineManager singleton via pipeline()
// getCurrentTLAS() now correctly returns pipeline().dummyTLAS() when needed
// Default scene visible — pink monster + ground + envmap sky
// PINK PHOTONS ACCELERATED AND VISIBLE — EMPIRE ETERNAL
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"  // Required for pipeline() accessor

using RTX::Handle;
using StoneKey::stone_device;
using RTX::g_ext;

namespace RTX {

// Global singleton
LAS& las() {
    static LAS instance;
    return instance;
}

// Constructor — adds visible default scene
LAS::LAS()
{
    LOG_INFO_CAT("LAS", "Constructing default scene — ground plane + sacred pink monster");

    // === LARGE GROUND PLANE (material 0) ===
    {
        auto ground = std::make_unique<MeshLoader::Mesh>();

        using Vertex = MeshLoader::Mesh::Vertex;
        ground->vertices = {
            Vertex{glm::vec3(-100.0f, 0.0f, -100.0f)},
            Vertex{glm::vec3( 100.0f, 0.0f, -100.0f)},
            Vertex{glm::vec3( 100.0f, 0.0f,  100.0f)},
            Vertex{glm::vec3(-100.0f, 0.0f,  100.0f)}
        };
        ground->indices = {0, 1, 2, 0, 2, 3};

        addMesh(std::move(ground), 0);  // Ground material
    }

    // === SACRED PINK MONSTER — glowing emissive triangle (material 1) ===
    {
        auto monster = std::make_unique<MeshLoader::Mesh>();

        using Vertex = MeshLoader::Mesh::Vertex;
        monster->vertices = {
            Vertex{glm::vec3( 0.0f,  3.0f, 0.0f)},   // Apex — high up for visibility
            Vertex{glm::vec3(-2.0f,  0.5f, 2.0f)},   // Base left
            Vertex{glm::vec3( 2.0f,  0.5f, 2.0f)}    // Base right
        };
        monster->indices = {0, 1, 2};

        addMesh(std::move(monster), 1);  // Pink emissive material
    }

    LOG_SUCCESS_CAT("LAS", "Default scene ready: large ground + glowing pink triangle — photons will be visible");
}

// Destructor — robust cleanup
LAS::~LAS()
{
    clearTLAS();

    for (auto& m : meshes_) {
        if (m.blas != VK_NULL_HANDLE) {
            g_ext.vkDestroyAccelerationStructureKHR(stone_device(), m.blas, nullptr);
            m.blas = VK_NULL_HANDLE;
        }
        if (m.vertexHandle != 0) BufferManager::destroy(m.vertexHandle);
        if (m.indexHandle != 0) BufferManager::destroy(m.indexHandle);
    }
    meshes_.clear();

    LOG_INFO_CAT("LAS", "LAS destroyed — all acceleration structures and buffers cleaned");
}

// Public: add mesh from loader
void LAS::addMesh(std::unique_ptr<MeshLoader::Mesh> meshPtr, uint32_t materialIndex)
{
    if (!meshPtr || meshPtr->vertices.empty() || meshPtr->indices.empty() || (meshPtr->indices.size() % 3 != 0)) {
        LOG_WARNING_CAT("LAS", "Invalid or empty mesh provided — skipping addMesh");
        return;
    }

    const MeshLoader::Mesh& mesh = *meshPtr;

    // Upload vertices
    uint64_t vertexHandle = BufferManager::create(
        mesh.vertices.size() * sizeof(MeshLoader::Mesh::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "Mesh_Vertices"
    );
    if (vertexHandle == 0) {
        LOG_ERROR_CAT("LAS", "Failed to create vertex buffer");
        return;
    }
    BufferManager::uploadToBuffer(vertexHandle, mesh.vertices.data(), mesh.vertices.size() * sizeof(MeshLoader::Mesh::Vertex));

    // Upload indices
    uint64_t indexHandle = BufferManager::create(
        mesh.indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "Mesh_Indices"
    );
    if (indexHandle == 0) {
        BufferManager::destroy(vertexHandle);
        LOG_ERROR_CAT("LAS", "Failed to create index buffer");
        return;
    }
    BufferManager::uploadToBuffer(indexHandle, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

    // Store internal representation
    InternalMesh internal;
    internal.vertexHandle = vertexHandle;
    internal.indexHandle = indexHandle;
    internal.primitiveCount = static_cast<uint32_t>(mesh.indices.size() / 3);
    internal.materialIndex = materialIndex;
    internal.blas = VK_NULL_HANDLE;

    meshes_.push_back(std::move(internal));

    // Mark for rebuild
    tlasDirty = true;
    blasBuilt = false;

    LOG_SUCCESS_CAT("LAS", "Mesh added — {} triangles, material {}", internal.primitiveCount, materialIndex);
}

// Public: force TLAS rebuild
void LAS::rebuildTLAS()
{
    tlasDirty = true;
    LOG_INFO_CAT("LAS", "TLAS rebuild requested");
}

// Public: called every frame
void LAS::buildOrUpdateTLAS(VkCommandBuffer cmd)
{
    if (meshes_.empty()) {
        LOG_TRACE_CAT("LAS", "No meshes — skipping AS build");
        return;
    }

    if (!blasBuilt) {
        buildBLAS(cmd);
        blasBuilt = true;
    }

    if (tlasDirty) {
        clearTLAS();
        buildTLAS(cmd);
        tlasDirty = false;
        LOG_SUCCESS_CAT("LAS", "TLAS rebuilt — {} instances", meshes_.size());
    }
}

// Public: get current TLAS — FIXED: now uses pipeline() singleton
VkAccelerationStructureKHR LAS::getCurrentTLAS() const
{
    return tlas != VK_NULL_HANDLE ? tlas : pipeline().dummyTLAS();
}

// Public: resize notification
void LAS::notifyResize()
{
    clearTLAS();
    tlasDirty = true;
    LOG_INFO_CAT("LAS", "Resize detected — TLAS cleared, will rebuild next frame");
}

// Private: clear current TLAS
void LAS::clearTLAS()
{
    if (tlas != VK_NULL_HANDLE) {
        g_ext.vkDestroyAccelerationStructureKHR(stone_device(), tlas, nullptr);
        tlas = VK_NULL_HANDLE;
    }
    tlasBuffer.reset();
    tlasMemory.reset();
    scratchBuffer.reset();
    scratchMemory.reset();
    instanceBuffer.reset();
    instanceMemory.reset();
}

// Private: build all BLAS
void LAS::buildBLAS(VkCommandBuffer cmd)
{
    for (auto& m : meshes_) {
        if (m.blas == VK_NULL_HANDLE) {
            buildSingleBLAS(cmd,
                            m.blas,
                            m.blasBuffer,
                            m.blasMemory,
                            m.vertexHandle,
                            m.indexHandle,
                            m.primitiveCount,
                            "Mesh_BLAS");
        }
    }

    // Barrier to ensure BLAS completion before TLAS build
    VkMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    g_ext.vkCmdPipelineBarrier2(cmd, &dep);
}

// Private: build single BLAS
void LAS::buildSingleBLAS(VkCommandBuffer cmd,
                          VkAccelerationStructureKHR& blas,
                          Handle<VkBuffer>& blasBuffer,
                          Handle<VkDeviceMemory>& blasMemory,
                          uint64_t vertexHandle,
                          uint64_t indexHandle,
                          uint32_t primitiveCount,
                          const std::string& tag)
{
    VkDeviceAddress vertexAddr = BufferManager::get_device_address(vertexHandle);
    VkDeviceAddress indexAddr  = BufferManager::get_device_address(indexHandle);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexAddr;
    triangles.vertexStride = sizeof(MeshLoader::Mesh::Vertex);
    triangles.maxVertex = 0xFFFFFFFF;
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexAddr;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles = triangles;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo);

    uint64_t blasHandle = BufferManager::create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tag + "_Buffer");

    if (blasHandle == 0) {
        LOG_FATAL_CAT("LAS", "Failed to create BLAS storage buffer");
        return;
    }

    const auto* blasInfoPtr = BufferManager::get(blasHandle);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = blasInfoPtr->buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkResult res = g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &blas);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("LAS", "Failed to create BLAS: {}", string_VkResult(res));
        BufferManager::destroy(blasHandle);
        return;
    }

    blasBuffer = Handle<VkBuffer>(blasInfoPtr->buffer, stone_device(), vkDestroyBuffer);
    blasMemory = Handle<VkDeviceMemory>(blasInfoPtr->memory, stone_device(), vkFreeMemory);

    // Scratch buffer
    uint64_t scratchHandle = BufferManager::create(
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tag + "_Scratch");

    if (scratchHandle == 0) {
        LOG_FATAL_CAT("LAS", "Failed to create BLAS scratch buffer");
        return;
    }

    VkDeviceAddress scratchAddr = BufferManager::get_device_address(scratchHandle);

    buildInfo.dstAccelerationStructure = blas;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;

    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    // Cleanup scratch after build
    BufferManager::destroy(scratchHandle);
}

// Private: build TLAS
void LAS::buildTLAS(VkCommandBuffer cmd)
{
    if (meshes_.empty()) return;

    std::vector<VkAccelerationStructureInstanceKHR> instances(meshes_.size());

    for (size_t i = 0; i < meshes_.size(); ++i) {
        const auto& m = meshes_[i];

        VkTransformMatrixKHR transform = {};
        std::memset(&transform, 0, sizeof(transform));
        transform.matrix[0][0] = transform.matrix[1][1] = transform.matrix[2][2] = 1.0f;

        instances[i] = {};
        instances[i].transform = transform;
        instances[i].instanceCustomIndex = static_cast<uint32_t>(i);
        instances[i].mask = 0xFF;
        instances[i].instanceShaderBindingTableRecordOffset = 0;
        instances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addrInfo.accelerationStructure = m.blas;
        instances[i].accelerationStructureReference = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &addrInfo);
    }

    uint64_t instHandle = BufferManager::create(
        instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "TLAS_Instances"
    );
    if (instHandle == 0) {
        LOG_FATAL_CAT("LAS", "Failed to create TLAS instance buffer");
        return;
    }
    BufferManager::uploadToBuffer(instHandle, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

    const auto* instInfo = BufferManager::get(instHandle);
    instanceBuffer = Handle<VkBuffer>(instInfo->buffer, stone_device(), vkDestroyBuffer);
    instanceMemory = Handle<VkDeviceMemory>(instInfo->memory, stone_device(), vkFreeMemory);

    VkDeviceAddress instAddr = BufferManager::get_device_address(instHandle);

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.arrayOfPointers = VK_FALSE;
    instancesData.data.deviceAddress = instAddr;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = instancesData;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    uint32_t primCount = static_cast<uint32_t>(meshes_.size());

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primCount,
        &sizeInfo);

    uint64_t tlasHandle = BufferManager::create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "TLAS_Buffer"
    );
    if (tlasHandle == 0) {
        LOG_FATAL_CAT("LAS", "Failed to create TLAS storage buffer");
        return;
    }

    const auto* tlasInfo = BufferManager::get(tlasHandle);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = tlasInfo->buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkResult res = g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &tlas);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("LAS", "Failed to create TLAS: {}", string_VkResult(res));
        BufferManager::destroy(tlasHandle);
        return;
    }

    tlasBuffer = Handle<VkBuffer>(tlasInfo->buffer, stone_device(), vkDestroyBuffer);
    tlasMemory = Handle<VkDeviceMemory>(tlasInfo->memory, stone_device(), vkFreeMemory);

    uint64_t scratchHandle = BufferManager::create(
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "TLAS_Scratch"
    );
    if (scratchHandle == 0) {
        LOG_FATAL_CAT("LAS", "Failed to create TLAS scratch buffer");
        return;
    }

    VkDeviceAddress scratchAddr = BufferManager::get_device_address(scratchHandle);

    buildInfo.dstAccelerationStructure = tlas;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primCount;

    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    BufferManager::destroy(scratchHandle);

    VkMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    g_ext.vkCmdPipelineBarrier2(cmd, &dep);

    LOG_SUCCESS_CAT("LAS", "TLAS built successfully — {} mesh instances — default scene now visible", meshes_.size());
}

} // namespace RTX

// =============================================================================
// LAS v2.1 — DECEMBER 27, 2025
// COMPILATION FIXED: #include "engine/GLOBAL/PipelineManager.hpp" added
// getCurrentTLAS() now correctly uses pipeline().dummyTLAS()
// Default scene visible — pink monster + ground + envmap sky
// Empire compiles and runs — photons eternal
// =============================================================================