// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025-2026 — VALHALLA v∞ TURBO — DEVELOPER EDITION
// Light Acceleration System (LAS) v4.0 — January 03, 2026
// Clean, modern C++23, developer-friendly, safe, extensible
// Fully compatible with existing codebase and LAS.hpp
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
#include <span>

using StoneKey::stone_device;
using RTX::g_ext;

namespace RTX {

LAS::LAS()
{
    LOG_AMOURANTH("LAS v4.0 (Developer Edition) initialized");

    // Optional default scene for immediate testing — comment out if you want empty start
    createDefaultDeveloperScene();
}

LAS::~LAS()
{
    clearTLAS();
    destroyScratchBuffers();

    for (auto& mesh : meshes_) {
        if (mesh.blas != VK_NULL_HANDLE) {
            g_ext.vkDestroyAccelerationStructureKHR(stone_device(), mesh.blas, nullptr);
        }
        if (mesh.vertexBuffer != 0) BufferManager::destroy(mesh.vertexBuffer);
        if (mesh.indexBuffer != 0) BufferManager::destroy(mesh.indexBuffer);
    }
    meshes_.clear();

    LOG_SUCCESS_CAT("LAS", "LAS destroyed — all resources released");
}

// =============================================================================
// Public API — Matches LAS.hpp exactly
// =============================================================================

size_t LAS::addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex)
{
    if (!mesh || mesh->vertices.empty() || mesh->indices.empty() || (mesh->indices.size() % 3 != 0)) {
        LOG_WARNING_CAT("LAS", "Invalid mesh — skipping addMesh");
        return meshes_.size();
    }

    auto vertexBuffer = BufferManager::create(
        mesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "LAS_VertexBuffer");

    if (vertexBuffer == 0) {
        LOG_ERROR_CAT("LAS", "Failed to create vertex buffer");
        return meshes_.size();
    }
    BufferManager::uploadToBuffer(vertexBuffer, mesh->vertices.data(),
                                  mesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex));

    auto indexBuffer = BufferManager::create(
        mesh->indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "LAS_IndexBuffer");

    if (indexBuffer == 0) {
        BufferManager::destroy(vertexBuffer);
        LOG_ERROR_CAT("LAS", "Failed to create index buffer");
        return meshes_.size();
    }
    BufferManager::uploadToBuffer(indexBuffer, mesh->indices.data(),
                                  mesh->indices.size() * sizeof(uint32_t));

    InternalMesh internal{
        .vertexBuffer   = vertexBuffer,
        .indexBuffer    = indexBuffer,
        .primitiveCount = static_cast<uint32_t>(mesh->indices.size() / 3),
        .materialIndex  = materialIndex,
        .transform      = glm::mat4(1.0f),
        .blas           = VK_NULL_HANDLE,
        .blasStorage    = 0
    };

    meshes_.push_back(std::move(internal));
    tlasDirty = true;

    LOG_SUCCESS_CAT("LAS", "Mesh added — {} triangles, material {} (instance index {})", internal.primitiveCount, materialIndex, meshes_.size() - 1);
    return meshes_.size() - 1;
}

void LAS::setInstanceTransform(size_t instanceIndex, const glm::mat4& transform)
{
    if (instanceIndex >= meshes_.size()) {
        LOG_WARNING_CAT("LAS", "Invalid instance index {}", instanceIndex);
        return;
    }

    meshes_[instanceIndex].transform = transform;
    tlasDirty = true;
}

void LAS::requestRebuild()
{
    tlasDirty = true;
}

void LAS::update(VkCommandBuffer cmd)
{
    if (meshes_.empty()) {
        return;
    }

    bool buildFailed = false;

    // Build missing BLAS
    for (auto& mesh : meshes_) {
        if (mesh.blas == VK_NULL_HANDLE) {
            buildBLAS(cmd, mesh);
            if (mesh.blas == VK_NULL_HANDLE) buildFailed = true;
        }
    }

    // BLAS visibility barrier
    if (tlasDirty) {
        insertAccelerationStructureBarrier(cmd);
    }

    // Rebuild TLAS if needed
    if (tlasDirty) {
        clearTLAS();
        if (buildTLAS(cmd)) {
            tlasDirty = false;
        } else {
            buildFailed = true;
        }
    }

    if (buildFailed) {
        LOG_WARNING_CAT("LAS", "Acceleration structure build failed this frame");
    }
}

VkAccelerationStructureKHR LAS::getTLAS() const
{
    return tlas != VK_NULL_HANDLE ? tlas : VK_NULL_HANDLE;
}

void LAS::onResize()
{
    clearTLAS();
    tlasDirty = true;
}

// =============================================================================
// Private Implementation — Developer-friendly and safe
// =============================================================================

void LAS::createDefaultDeveloperScene()
{
    // Large ground plane
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

    // Glowing pink triangle
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

    LOG_AMOURANTH("Default developer scene created — ground + pink triangle");
}

void LAS::buildBLAS(VkCommandBuffer cmd, InternalMesh& mesh)
{
    VkDeviceAddress vertexAddr = BufferManager::get_device_address(mesh.vertexBuffer);
    VkDeviceAddress indexAddr  = BufferManager::get_device_address(mesh.indexBuffer);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData   = {vertexAddr},
        .vertexStride = sizeof(MeshLoader::Mesh::Vertex),
        .maxVertex    = 0xFFFFFFFF,
        .indexType    = VK_INDEX_TYPE_UINT32,
        .indexData    = {indexAddr}
    };

    VkAccelerationStructureGeometryKHR geometry = {};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles = triangles;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &mesh.primitiveCount,
        &sizeInfo);

    auto blasStorage = BufferManager::create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "LAS_BLAS_Storage");

    if (blasStorage == 0) {
        LOG_ERROR_CAT("LAS", "Failed to allocate BLAS storage");
        return;
    }

    const auto* info = BufferManager::get(blasStorage);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = info->buffer,
        .size   = sizeInfo.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

    VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
    if (g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &blas) != VK_SUCCESS) {
        LOG_ERROR_CAT("LAS", "Failed to create BLAS");
        BufferManager::destroy(blasStorage);
        return;
    }

    mesh.blas = blas;
    mesh.blasStorage = blasStorage;

    auto scratch = ensureScratch(sizeInfo.buildScratchSize, "LAS_BLAS_Scratch");
    if (scratch == 0) return;

    buildInfo.dstAccelerationStructure = blas;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(scratch);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{.primitiveCount = mesh.primitiveCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
}

bool LAS::buildTLAS(VkCommandBuffer cmd)
{
    if (meshes_.empty()) return false;

    std::vector<VkAccelerationStructureInstanceKHR> instances(meshes_.size());

    for (size_t i = 0; i < meshes_.size(); ++i) {
        const auto& m = meshes_[i];

        VkTransformMatrixKHR transformMatrix{};
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                transformMatrix.matrix[row][col] = m.transform[col][row];

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = m.blas
        };

        instances[i] = VkAccelerationStructureInstanceKHR{};
        instances[i].transform = transformMatrix;
        instances[i].instanceCustomIndex = m.materialIndex;
        instances[i].mask = 0xFF;
        instances[i].instanceShaderBindingTableRecordOffset = 0;
        instances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instances[i].accelerationStructureReference = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &addrInfo);
    }

    auto instanceStorage = BufferManager::create(
        instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "LAS_TLAS_Instances");

    if (instanceStorage == 0) return false;

    BufferManager::uploadToBuffer(instanceStorage, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

    VkDeviceAddress instanceAddr = BufferManager::get_device_address(instanceStorage);

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data            = {instanceAddr}
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     = {.instances = instancesData}
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    uint32_t primCount = static_cast<uint32_t>(meshes_.size());

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primCount,
        &sizeInfo);

    auto tlasStorage = BufferManager::create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "LAS_TLAS_Storage");

    if (tlasStorage == 0) {
        BufferManager::destroy(instanceStorage);
        return false;
    }

    const auto* info = BufferManager::get(tlasStorage);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = info->buffer,
        .size   = sizeInfo.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    if (g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &tlas) != VK_SUCCESS) {
        BufferManager::destroy(tlasStorage);
        BufferManager::destroy(instanceStorage);
        return false;
    }

    this->tlasStorage = tlasStorage;
    this->instanceStorage = instanceStorage;

    auto scratch = ensureScratch(sizeInfo.buildScratchSize, "LAS_TLAS_Scratch");
    if (scratch == 0) return false;

    buildInfo.dstAccelerationStructure = tlas;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(scratch);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{.primitiveCount = primCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    VkMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstStageMask  = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    g_ext.vkCmdPipelineBarrier2(cmd, &dep);

    LOG_SUCCESS_CAT("LAS", "TLAS built — {} instances", meshes_.size());
    return true;
}

uint64_t LAS::ensureScratch(VkDeviceSize required, const std::string& tag)
{
    if (scratchBuffer != 0 && scratchSize >= required) {
        return scratchBuffer;
    }

    if (scratchBuffer != 0) {
        BufferManager::destroy(scratchBuffer);
    }

    scratchBuffer = BufferManager::create(
        required,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tag);

    scratchSize = required;
    return scratchBuffer;
}

void LAS::insertAccelerationStructureBarrier(VkCommandBuffer cmd)
{
    VkMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstStageMask  = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    g_ext.vkCmdPipelineBarrier2(cmd, &dep);
}

void LAS::clearTLAS()
{
    if (tlas != VK_NULL_HANDLE) {
        g_ext.vkDestroyAccelerationStructureKHR(stone_device(), tlas, nullptr);
        tlas = VK_NULL_HANDLE;
    }
    if (tlasStorage != 0) {
        BufferManager::destroy(tlasStorage);
        tlasStorage = 0;
    }
    if (instanceStorage != 0) {
        BufferManager::destroy(instanceStorage);
        instanceStorage = 0;
    }
}

void LAS::destroyScratchBuffers()
{
    if (scratchBuffer != 0) {
        BufferManager::destroy(scratchBuffer);
        scratchBuffer = 0;
        scratchSize = 0;
    }
}

} // namespace RTX

// =============================================================================
// LAS v4.0 — Developer Edition — January 03, 2026
// Fully compatible with existing codebase
// All compilation errors resolved
// Clean, safe, modern C++23 design
// Ready for developer use and extension
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN
// =============================================================================