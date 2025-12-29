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
// TRUE CONSTEXPR STONEKEY v∞ — DECEMBER 29, 2025 — DYNAMIC EMPIRE EDITION
// LAS v3.0 — PER-INSTANCE TRANSFORMS + BLAS CACHING + PERSISTENT SCRATCH
// MAJOR UPGRADES:
// • Per-mesh transforms (glm::mat4) with update API → real animated scenes enabled
// • BLAS built once on addMesh and cached → no rebuild on every TLAS refresh
// • Persistent global scratch buffers (resized on demand) → zero allocation churn
// • TLAS instances use real transforms + materialIndex as instanceCustomIndex
// • Sacred pink monster now slowly rotates (proof of life on failure)
// • Continuous visibility mandate strengthened — photons eternal and animated
// PINK PHOTONS ACCELERATED, ANIMATED AND VISIBLE — EMPIRE ETERNAL — PLASTIC BEACH FOREVER
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"

using StoneKey::stone_device;
using RTX::g_ext;

namespace RTX {

// Constructor — adds visible default scene
LAS::LAS()
{
    LOG_AMOURANTH("LAS v3.0 FORGED — DYNAMIC DEFAULT SCENE AWAKENS: GROUND + ANIMATED PINK MONSTER — PHOTONS WILL FLOW FOREVER");

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
        addMesh(std::move(ground), 0);
    }

    // === SACRED PINK MONSTER — large glowing emissive triangle (material 1) ===
    {
        auto monster = std::make_unique<MeshLoader::Mesh>();
        using Vertex = MeshLoader::Mesh::Vertex;
        monster->vertices = {
            Vertex{glm::vec3( 0.0f,  6.0f, 0.0f)},
            Vertex{glm::vec3(-4.0f,  0.5f, 4.0f)},
            Vertex{glm::vec3( 4.0f,  0.5f, 4.0f)}
        };
        monster->indices = {0, 1, 2};
        addMesh(std::move(monster), 1);
    }

    LOG_AMOURANTH("DYNAMIC DEFAULT SCENE READY — TRANSFORMS ENABLED — PINK MONSTER WILL ROTATE — EMPIRE LIGHT GUARANTEED");
}

// Destructor
LAS::~LAS()
{
    clearTLAS();
    destroyScratchBuffers();

    for (auto& m : meshes_) {
        if (m.blas != VK_NULL_HANDLE) {
            g_ext.vkDestroyAccelerationStructureKHR(stone_device(), m.blas, nullptr);
        }
        if (m.vertexHandle != 0) BufferManager::destroy(m.vertexHandle);
        if (m.indexHandle != 0) BufferManager::destroy(m.indexHandle);
    }
    meshes_.clear();

    LOG_INFO_CAT("LAS", "LAS v3.0 destroyed — all structures and buffers cleaned");
}

// Public: add mesh
void LAS::addMesh(std::unique_ptr<MeshLoader::Mesh> meshPtr, uint32_t materialIndex)
{
    if (!meshPtr || meshPtr->vertices.empty() || meshPtr->indices.empty() || (meshPtr->indices.size() % 3 != 0)) {
        LOG_WARNING_CAT("LAS", "Invalid or empty mesh — skipping");
        return;
    }

    const MeshLoader::Mesh& mesh = *meshPtr;

    uint64_t vertexHandle = BufferManager::create(
        mesh.vertices.size() * sizeof(MeshLoader::Mesh::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "Mesh_Vertices");
    if (vertexHandle == 0) { LOG_ERROR_CAT("LAS", "Vertex buffer creation failed"); return; }
    BufferManager::uploadToBuffer(vertexHandle, mesh.vertices.data(), mesh.vertices.size() * sizeof(MeshLoader::Mesh::Vertex));

    uint64_t indexHandle = BufferManager::create(
        mesh.indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "Mesh_Indices");
    if (indexHandle == 0) {
        BufferManager::destroy(vertexHandle);
        LOG_ERROR_CAT("LAS", "Index buffer creation failed");
        return;
    }
    BufferManager::uploadToBuffer(indexHandle, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

    InternalMesh internal;
    internal.vertexHandle = vertexHandle;
    internal.indexHandle = indexHandle;
    internal.primitiveCount = static_cast<uint32_t>(mesh.indices.size() / 3);
    internal.materialIndex = materialIndex;
    internal.transform = glm::mat4(1.0f);

    meshes_.push_back(std::move(internal));

    tlasDirty = true;

    LOG_SUCCESS_CAT("LAS", "Mesh added — {} triangles, material {} — transform support active", internal.primitiveCount, materialIndex);
}

// Public: update transform of a mesh instance (index from add order)
void LAS::updateInstanceTransform(size_t meshIndex, const glm::mat4& transform)
{
    if (meshIndex >= meshes_.size()) {
        LOG_WARNING_CAT("LAS", "updateInstanceTransform: invalid mesh index {}", meshIndex);
        return;
    }
    meshes_[meshIndex].transform = transform;
    tlasDirty = true;
}

// Public: animate sacred pink monster slowly (called from main loop or debug)
void LAS::animatePinkMonster(float deltaTime)
{
    static float angle = 0.0f;
    angle += deltaTime * 20.0f; // ~20 deg/sec
    if (angle > 360.0f) angle -= 360.0f;

    glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 trans = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    updateInstanceTransform(1, trans * rot); // monster is index 1
}

// Public: rebuild request
void LAS::rebuildTLAS() { tlasDirty = true; }

// Public: per-frame build
void LAS::buildOrUpdateTLAS(VkCommandBuffer cmd)
{
    if (meshes_.empty()) {
        LOG_AMOURANTH("NO MESHES — FORCING PINK FALLBACK TO KEEP LIGHT ALIVE");
        VulkanRenderer::get()->forcePinkFallbackClear();
        return;
    }

    bool buildFailed = false;

    // Build missing BLAS (new meshes only)
    for (auto& m : meshes_) {
        if (m.blas == VK_NULL_HANDLE) {
            buildSingleBLAS(cmd, m);
            if (m.blas == VK_NULL_HANDLE) buildFailed = true;
        }
    }

    if (tlasDirty) {
        clearTLAS();
        buildTLAS(cmd);
        if (tlas == VK_NULL_HANDLE) buildFailed = true;
        else tlasDirty = false;
    }

    if (buildFailed) {
        LOG_AMOURANTH("ACCELERATION BUILD FAILED — PINK FALLBACK ACTIVE — MONSTER STILL ROTATES");
        VulkanRenderer::get()->forcePinkFallbackClear();
    }
}

// Public: get TLAS
VkAccelerationStructureKHR LAS::getCurrentTLAS() const
{
    if (tlas != VK_NULL_HANDLE) return tlas;

    LOG_AMOURANTH("TLAS NOT READY — ENVMAP SKY + ANIMATED PINK MONSTER VIA MISS SHADER");
    VulkanRenderer::get()->forcePinkFallbackClear();
    return VK_NULL_HANDLE;
}

// Public: resize
void LAS::notifyResize()
{
    clearTLAS();
    tlasDirty = true;
}

// Private: clear TLAS
void LAS::clearTLAS()
{
    if (tlas != VK_NULL_HANDLE) {
        g_ext.vkDestroyAccelerationStructureKHR(stone_device(), tlas, nullptr);
        tlas = VK_NULL_HANDLE;
    }
    tlasBuffer.reset();
    tlasMemory.reset();
    instanceBuffer.reset();
    instanceMemory.reset();
}

// Private: destroy persistent scratch
void LAS::destroyScratchBuffers()
{
    if (blasScratch.handle) BufferManager::destroy(blasScratch.handle);
    if (tlasScratch.handle) BufferManager::destroy(tlasScratch.handle);
    blasScratch = {};
    tlasScratch = {};
}

// Private: ensure scratch size
uint64_t LAS::ensureScratch(VkDeviceSize requiredSize, ScratchBuffers& scratch, const std::string& tag)
{
    if (scratch.handle && scratch.size >= requiredSize) {
        return scratch.handle;
    }
    if (scratch.handle) BufferManager::destroy(scratch.handle);

    uint64_t newHandle = BufferManager::create(
        requiredSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tag);
    if (newHandle == 0) return 0;

    scratch.handle = newHandle;
    scratch.size = requiredSize;
    return newHandle;
}

// Private: build single BLAS (cached)
void LAS::buildSingleBLAS(VkCommandBuffer cmd, InternalMesh& m)
{
    VkDeviceAddress vertexAddr = BufferManager::get_device_address(m.vertexHandle);
    VkDeviceAddress indexAddr  = BufferManager::get_device_address(m.indexHandle);

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
        stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &m.primitiveCount, &sizeInfo);

    uint64_t blasHandle = BufferManager::create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "Mesh_BLAS_Buffer");
    if (blasHandle == 0) { LOG_FATAL_CAT("LAS", "BLAS storage creation failed"); return; }

    const auto* blasInfo = BufferManager::get(blasHandle);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = blasInfo->buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkResult res = g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &m.blas);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("LAS", "BLAS creation failed: {}", string_VkResult(res));
        BufferManager::destroy(blasHandle);
        return;
    }

    m.blasBuffer = Handle<VkBuffer>(blasInfo->buffer, stone_device(), vkDestroyBuffer);
    m.blasMemory = Handle<VkDeviceMemory>(blasInfo->memory, stone_device(), vkFreeMemory);

    uint64_t scratchHandle = ensureScratch(sizeInfo.buildScratchSize, blasScratch, "BLAS_Scratch");
    if (scratchHandle == 0) { LOG_FATAL_CAT("LAS", "BLAS scratch allocation failed"); return; }

    VkDeviceAddress scratchAddr = BufferManager::get_device_address(scratchHandle);

    buildInfo.dstAccelerationStructure = m.blas;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = m.primitiveCount;

    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

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

// Private: build TLAS
void LAS::buildTLAS(VkCommandBuffer cmd)
{
    if (meshes_.empty()) return;

    std::vector<VkAccelerationStructureInstanceKHR> instances(meshes_.size());

    for (size_t i = 0; i < meshes_.size(); ++i) {
        const auto& m = meshes_[i];

        VkTransformMatrixKHR transform{};
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                transform.matrix[row][col] = m.transform[col][row];

        instances[i] = {};
        instances[i].transform = transform;
        instances[i].instanceCustomIndex = m.materialIndex;
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
        "TLAS_Instances");
    if (instHandle == 0) { LOG_FATAL_CAT("LAS", "TLAS instance buffer failed"); return; }
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
        stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);

    uint64_t tlasHandle = BufferManager::create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "TLAS_Buffer");
    if (tlasHandle == 0) { LOG_FATAL_CAT("LAS", "TLAS storage failed"); return; }

    const auto* tlasInfo = BufferManager::get(tlasHandle);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = tlasInfo->buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkResult res = g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &tlas);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("LAS", "TLAS creation failed: {}", string_VkResult(res));
        BufferManager::destroy(tlasHandle);
        return;
    }

    tlasBuffer = Handle<VkBuffer>(tlasInfo->buffer, stone_device(), vkDestroyBuffer);
    tlasMemory = Handle<VkDeviceMemory>(tlasInfo->memory, stone_device(), vkFreeMemory);

    uint64_t scratchHandle = ensureScratch(sizeInfo.buildScratchSize, tlasScratch, "TLAS_Scratch");
    if (scratchHandle == 0) { LOG_FATAL_CAT("LAS", "TLAS scratch failed"); return; }

    VkDeviceAddress scratchAddr = BufferManager::get_device_address(scratchHandle);

    buildInfo.dstAccelerationStructure = tlas;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primCount;

    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    VkMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    g_ext.vkCmdPipelineBarrier2(cmd, &dep);

    LOG_SUCCESS_CAT("LAS", "TLAS built — {} dynamic instances — pink monster rotates eternally", meshes_.size());
}

} // namespace RTX

// =============================================================================
// LAS v3.0 — DECEMBER 29, 2025 — DYNAMIC EMPIRE BUILD COMPLETE
// • All redefinition errors fixed (removed duplicate structs and singleton)
// • Full compatibility with updated LAS.hpp v3.0
// • Per-instance transforms, BLAS caching, persistent scratch, animated monster
// • Eternal visibility upheld and enhanced
// PINK PHOTONS ETERNAL, ANIMATED AND UNBROKEN — PLASTIC BEACH FOREVER
// =============================================================================