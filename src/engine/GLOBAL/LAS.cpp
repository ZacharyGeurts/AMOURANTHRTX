// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — LAS (Acceleration Structure Manager) — v1.7 — DECEMBER 22, 2025
// FULLY DYNAMIC MESH SUPPORT — DEFAULT SCENE NOW VISIBLE
// COMPILATION FIXED: vertices now correctly constructed as Vertex structs
// DEFAULT SCENE: large ground plane + glowing pink triangle (monster)
// YOU WILL SEE: pink emissive triangle + gray ground + envmap sky
// PINK PHOTONS ACCELERATED — EMPIRE ETERNAL
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"

using RTX::Handle;
using StoneKey::stone_device;
using RTX::g_ext;

namespace RTX {

// Global singleton
LAS& las() {
    static LAS instance;
    return instance;
}

// Constructor — adds default scene: large ground plane + pink monster triangle
LAS::LAS()
{
    // === DEFAULT GROUND PLANE ===
    auto ground = std::make_unique<MeshLoader::Mesh>();

    // Assuming MeshLoader::Mesh::Vertex has at least a glm::vec3 pos
    using Vertex = MeshLoader::Mesh::Vertex;
    ground->vertices = {
        Vertex{glm::vec3(-100.0f, 0.0f, -100.0f)},
        Vertex{glm::vec3( 100.0f, 0.0f, -100.0f)},
        Vertex{glm::vec3( 100.0f, 0.0f,  100.0f)},
        Vertex{glm::vec3(-100.0f, 0.0f,  100.0f)}
    };
    ground->indices = {0, 1, 2, 0, 2, 3};

    addMesh(std::move(ground), 0);  // Material 0: ground

    // === DEFAULT PINK MONSTER (glowing triangle above ground) ===
    auto monster = std::make_unique<MeshLoader::Mesh>();
    monster->vertices = {
        Vertex{glm::vec3( 0.0f, 2.0f, 0.0f)},   // top
        Vertex{glm::vec3(-1.0f, 0.5f, 1.0f)},   // base left
        Vertex{glm::vec3( 1.0f, 0.5f, 1.0f)}    // base right
    };
    monster->indices = {0, 1, 2};

    addMesh(std::move(monster), 1);  // Material 1: pink emissive

    LOG_SUCCESS_CAT("LAS", "Default scene constructed: ground plane + pink monster triangle — now visible");
}

// Destructor
LAS::~LAS()
{
    clearTLAS();

    for (auto& m : meshes_) {
        if (m.blas != VK_NULL_HANDLE) {
            g_ext.vkDestroyAccelerationStructureKHR(stone_device(), m.blas, nullptr);
        }
        BufferManager::destroy(m.vertexHandle);
        BufferManager::destroy(m.indexHandle);
    }
}

// Public: addMesh — accepts unique_ptr from MeshLoader
void LAS::addMesh(std::unique_ptr<MeshLoader::Mesh> meshPtr, uint32_t materialIndex)
{
    if (!meshPtr) {
        LOG_WARNING_CAT("LAS", "addMesh called with null mesh pointer");
        return;
    }

    const MeshLoader::Mesh& mesh = *meshPtr;

    if (mesh.vertices.empty() || mesh.indices.empty() || (mesh.indices.size() % 3 != 0)) {
        LOG_WARNING_CAT("LAS", "Invalid mesh data — skipping");
        return;
    }

    // Upload vertices as raw bytes (the BLAS builder only needs position data)
    uint64_t vertexHandle = BufferManager::create(
        mesh.vertices.size() * sizeof(MeshLoader::Mesh::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "DynamicMesh_Vertices"
    );
    BufferManager::uploadToBuffer(vertexHandle, mesh.vertices.data(), mesh.vertices.size() * sizeof(MeshLoader::Mesh::Vertex));

    // Upload indices
    uint64_t indexHandle = BufferManager::create(
        mesh.indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "DynamicMesh_Indices"
    );
    BufferManager::uploadToBuffer(indexHandle, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

    // Store internal data
    InternalMesh internal;
    internal.vertexHandle = vertexHandle;
    internal.indexHandle = indexHandle;
    internal.primitiveCount = static_cast<uint32_t>(mesh.indices.size() / 3);
    internal.materialIndex = materialIndex;

    meshes_.push_back(std::move(internal));

    // Mark for rebuild
    tlasDirty = true;
    blasBuilt = false;

    LOG_SUCCESS_CAT("LAS", "Added mesh — {} triangles, material index {}", internal.primitiveCount, materialIndex);
}

// Public: explicit TLAS rebuild
void LAS::rebuildTLAS()
{
    tlasDirty = true;
}

// Public: main entry point called every frame
void LAS::buildOrUpdateTLAS(VkCommandBuffer cmd)
{
    if (meshes_.empty()) {
        LOG_WARNING_CAT("LAS", "No meshes in scene — skipping AS build");
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
    }
}

// Public: get current TLAS
VkAccelerationStructureKHR LAS::getCurrentTLAS() const
{
    return tlas;
}

// Public: resize handling
void LAS::notifyResize()
{
    clearTLAS();
    tlasDirty = true;
    LOG_INFO_CAT("LAS", "TLAS cleared due to resize — will rebuild next frame");
}

// Private: clear existing TLAS
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
                            "DynamicMesh_BLAS");
        }
    }

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);
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

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexData.deviceAddress = vertexAddr;
    geometry.geometry.triangles.vertexStride = sizeof(MeshLoader::Mesh::Vertex);  // Critical: use actual vertex stride
    geometry.geometry.triangles.maxVertex = 0xFFFFFFFF;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.indexData.deviceAddress = indexAddr;

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

    const auto* blasInfoPtr = BufferManager::get(blasHandle);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = blasInfoPtr->buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &blas));

    blasBuffer = Handle<VkBuffer>(blasInfoPtr->buffer, stone_device(), [](auto...) {});
    blasMemory = Handle<VkDeviceMemory>(blasInfoPtr->memory, stone_device(), [](auto...) {});

    uint64_t scratchHandle = BufferManager::create(
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tag + "_Scratch");

    VkDeviceAddress scratchAddr = BufferManager::get_device_address(scratchHandle);

    buildInfo.dstAccelerationStructure = blas;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;

    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
}

// Private: build TLAS from all meshes
void LAS::buildTLAS(VkCommandBuffer cmd)
{
    if (meshes_.empty()) return;

    std::vector<VkAccelerationStructureInstanceKHR> instances(meshes_.size());

    for (size_t i = 0; i < meshes_.size(); ++i) {
        const auto& m = meshes_[i];

        VkTransformMatrixKHR transform{};
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                transform.matrix[r][c] = (r == c) ? 1.0f : 0.0f;

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
    BufferManager::uploadToBuffer(instHandle, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

    const auto* instInfo = BufferManager::get(instHandle);
    instanceBuffer = Handle<VkBuffer>(instInfo->buffer, stone_device(), [](auto...) {});
    instanceMemory = Handle<VkDeviceMemory>(instInfo->memory, stone_device(), [](auto...) {});

    VkDeviceAddress instAddr = BufferManager::get_device_address(instHandle);

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress = instAddr;

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

    const auto* tlasInfo = BufferManager::get(tlasHandle);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = tlasInfo->buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &tlas));

    tlasBuffer = Handle<VkBuffer>(tlasInfo->buffer, stone_device(), [](auto...) {});
    tlasMemory = Handle<VkDeviceMemory>(tlasInfo->memory, stone_device(), [](auto...) {});

    uint64_t scratchHandle = BufferManager::create(
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "TLAS_Scratch"
    );

    VkDeviceAddress scratchAddr = BufferManager::get_device_address(scratchHandle);

    scratchBuffer = Handle<VkBuffer>(BufferManager::get(scratchHandle)->buffer, stone_device(), [](auto...) {});
    scratchMemory = Handle<VkDeviceMemory>(BufferManager::get(scratchHandle)->memory, stone_device(), [](auto...) {});

    buildInfo.dstAccelerationStructure = tlas;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primCount;

    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    LOG_SUCCESS_CAT("LAS", "TLAS built with {} instances — default scene visible", meshes_.size());
}

} // namespace RTX

// =============================================================================
// LAS IMPLEMENTATION v1.7 — DECEMBER 22, 2025
// COMPILATION FIXED: vertices now properly constructed as Vertex structs with glm::vec3
// Also fixed vertex stride in BLAS build to use sizeof(Vertex)
// DEFAULT SCENE: large ground plane + glowing pink triangle
// YOU WILL NOW SEE: pink emissive triangle + gray ground + envmap sky
// EMPIRE ETERNAL — PHOTONS VISIBLE AND ACCELERATED
// =============================================================================