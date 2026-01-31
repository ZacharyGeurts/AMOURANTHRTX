// =============================================================================
// AMOURANTH RTX Engine - Light Acceleration System (LAS)
// Hybrid acceleration structure manager (triangle BLAS + procedural AABB BLAS → TLAS)
// Version 30.26 — January 30, 2026
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <algorithm>
#include <thread>
#include <future>
#include <atomic>

using StoneKey::stone_device;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_graphics_family;
using RTX::g_ext;

// Fix: bring SYNC_REBUILD into scope (Options::LAS is in global namespace)
using ::Options::LAS::SYNC_REBUILD;

static VkTransformMatrixKHR convertToVkTransform(const glm::mat4& mat) {
    VkTransformMatrixKHR tm{};
    tm.matrix[0][0] = mat[0][0]; tm.matrix[0][1] = mat[1][0]; tm.matrix[0][2] = mat[2][0]; tm.matrix[0][3] = mat[3][0];
    tm.matrix[1][0] = mat[0][1]; tm.matrix[1][1] = mat[1][1]; tm.matrix[1][2] = mat[2][1]; tm.matrix[1][3] = mat[3][1];
    tm.matrix[2][0] = mat[0][2]; tm.matrix[2][1] = mat[1][2]; tm.matrix[2][2] = mat[2][2]; tm.matrix[2][3] = mat[3][2];
    return tm;
}

RTX::LAS& RTX::LAS::instance() {
    static LAS globalInstance;
    return globalInstance;
}

RTX::LAS::LAS() {
    LOG_INFO_CAT("LAS", "v30.26 initialized — sync/async via Options::LAS::SYNC_REBUILD");

    instanceBuffer = BufferManager::create(
        MAX_INSTANCES * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LAS_InstanceBuffer");

    universalPrimitivesBuffer = BufferManager::create(
        MAX_PROCEDURALS * sizeof(UniversalPrimitive),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "LAS_UniversalPrimitives");

    proceduralPrimitives.reserve(MAX_PROCEDURALS);
    triangleMeshes.reserve(2048);

    initialized = false;
    tlasDirty = true;
    pendingBlasBuilds = true;
    proceduralDirty = true;

    rebuildPromise = std::nullopt;
    rebuildFuture = std::nullopt;
}

RTX::LAS::~LAS() {
    if (rebuildFuture.has_value() && rebuildFuture->valid()) {
        rebuildFuture->wait();
    }

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
    }

    BufferManager::destroy(instanceBuffer);
    BufferManager::destroy(universalPrimitivesBuffer);
}

void RTX::LAS::onResize() {
    clearTLAS();
    tlasDirty = true;
}

void RTX::LAS::notifySwapchainRecreated() {
    tlasDirty = true;
    ensureReady();
    LOG_INFO_CAT("LAS", "Swapchain recreated → TLAS rebuild triggered");
}

VkAccelerationStructureKHR RTX::LAS::getTLAS() {
    ensureReady();
    return tlas;
}

void RTX::LAS::insertASBuildToTraceBarrier(VkCommandBuffer cmd) {
    if (!cmd) return;
    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void RTX::LAS::insertASBuildToBuildBarrier(VkCommandBuffer cmd) {
    if (!cmd) return;
    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void RTX::LAS::ensureReady() {
    if (!SYNC_REBUILD) {  // ← now works cleanly thanks to using declaration
        if (rebuildFuture.has_value()) {
            if (rebuildFuture->valid() && rebuildFuture->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                try {
                    auto [newTlas, newTlasStorage, newSuccess] = rebuildFuture->get();
                    tlas = newTlas;
                    tlasStorage = newTlasStorage;
                    tlasDirty = pendingBlasBuilds = proceduralDirty = !newSuccess;
                    rebuildFuture.reset();
                    LOG_INFO_CAT("LAS", "Async rebuild completed — TLAS ready");
                } catch (const std::exception& e) {
                    LOG_FATAL_CAT("LAS", "Async rebuild failed: {}", e.what());
                    rebuildFuture.reset();
                }
            } else if (rebuildFuture->valid()) {
                LOG_INFO_CAT("LAS", "Async rebuild in progress — returning current TLAS");
                return;
            }
        }
    }

    if (initialized && !tlasDirty && !pendingBlasBuilds && !proceduralDirty && tlas != VK_NULL_HANDLE) {
        return;
    }

    if (!initialized) {
        createDefaultHybridScene();
        initialized = true;
    }

    if (SYNC_REBUILD) {  // ← now works cleanly
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo poolCI{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, stone_graphics_family() };
        if (vkCreateCommandPool(stone_device(), &poolCI, nullptr, &pool) != VK_SUCCESS) return;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo allocCI{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
        if (vkAllocateCommandBuffers(stone_device(), &allocCI, &cmd) != VK_SUCCESS) {
            vkDestroyCommandPool(stone_device(), pool, nullptr);
            return;
        }

        VkCommandBufferBeginInfo beginCI{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
        if (vkBeginCommandBuffer(cmd, &beginCI) != VK_SUCCESS) {
            vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
            vkDestroyCommandPool(stone_device(), pool, nullptr);
            return;
        }

        bool success = true;

        if (pendingBlasBuilds || proceduralDirty) {
            success &= batchBuildAndCompactBLAS(cmd);
            if (success) insertASBuildToBuildBarrier(cmd);
        }

        if (tlasDirty) {
            clearTLAS();
            success &= buildHybridTLAS(cmd);
            if (success) insertASBuildToTraceBarrier(cmd);
        }

        if (!success || vkEndCommandBuffer(cmd) != VK_SUCCESS) success = false;

        if (success) {
            VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmd };
            success &= (vkQueueSubmit(stone_graphics_queue(), 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
        }

        if (success) vkQueueWaitIdle(stone_graphics_queue());

        vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
        vkDestroyCommandPool(stone_device(), pool, nullptr);

        if (success) {
            tlasDirty = pendingBlasBuilds = proceduralDirty = false;
            LOG_SUCCESS_CAT("LAS", "Synchronous rebuild complete — TLAS ready");
        } else {
            LOG_FATAL_CAT("LAS", "Synchronous rebuild failed");
        }
    } else {
        if (!rebuildFuture.has_value() || !rebuildFuture->valid()) {
            LOG_INFO_CAT("LAS", "Launching async rebuild...");
            rebuildPromise.emplace();
            rebuildFuture.emplace(rebuildPromise->get_future());

            std::thread([this, promise = std::move(*rebuildPromise)]() mutable {
                VkCommandPool pool = VK_NULL_HANDLE;
                VkCommandPoolCreateInfo poolCI{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, stone_graphics_family() };
                if (vkCreateCommandPool(stone_device(), &poolCI, nullptr, &pool) != VK_SUCCESS) {
                    promise.set_value({VK_NULL_HANDLE, 0, false});
                    return;
                }

                VkCommandBuffer cmd = VK_NULL_HANDLE;
                VkCommandBufferAllocateInfo allocCI{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
                if (vkAllocateCommandBuffers(stone_device(), &allocCI, &cmd) != VK_SUCCESS) {
                    vkDestroyCommandPool(stone_device(), pool, nullptr);
                    promise.set_value({VK_NULL_HANDLE, 0, false});
                    return;
                }

                VkCommandBufferBeginInfo beginCI{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
                if (vkBeginCommandBuffer(cmd, &beginCI) != VK_SUCCESS) {
                    vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
                    vkDestroyCommandPool(stone_device(), pool, nullptr);
                    promise.set_value({VK_NULL_HANDLE, 0, false});
                    return;
                }

                bool success = true;

                if (pendingBlasBuilds || proceduralDirty) {
                    success &= batchBuildAndCompactBLAS(cmd);
                    if (success) insertASBuildToBuildBarrier(cmd);
                }

                VkAccelerationStructureKHR newTlas = VK_NULL_HANDLE;
                uint64_t newTlasStorage = 0;

                if (tlasDirty) {
                    clearTLAS();
                    success &= buildHybridTLAS(cmd);
                    newTlas = tlas;
                    newTlasStorage = tlasStorage;
                    if (success) insertASBuildToTraceBarrier(cmd);
                }

                if (!success || vkEndCommandBuffer(cmd) != VK_SUCCESS) success = false;

                if (success) {
                    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmd };
                    success &= (vkQueueSubmit(stone_graphics_queue(), 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
                }

                if (success) vkQueueWaitIdle(stone_graphics_queue());

                vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
                vkDestroyCommandPool(stone_device(), pool, nullptr);

                promise.set_value({newTlas, newTlasStorage, success});
            }).detach();
        }
    }

    vkDeviceWaitIdle(stone_device());
}

size_t RTX::LAS::addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex) {
    if (!mesh || mesh->vertices.empty() || mesh->indices.empty() || mesh->indices.size() % 3 != 0) {
        return triangleMeshes.size();
    }

    uint64_t vb = BufferManager::create(
        mesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        "LAS_Vertex");

    if (vb == 0) return triangleMeshes.size();

    VkCommandPool transientPool = VK_NULL_HANDLE;
    VkCommandBuffer uploadCmd = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, stone_graphics_family() };
    if (vkCreateCommandPool(stone_device(), &pci, nullptr, &transientPool) != VK_SUCCESS) {
        BufferManager::destroy(vb);
        return triangleMeshes.size();
    }

    VkCommandBufferAllocateInfo ai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, transientPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
    if (vkAllocateCommandBuffers(stone_device(), &ai, &uploadCmd) != VK_SUCCESS) {
        vkDestroyCommandPool(stone_device(), transientPool, nullptr);
        BufferManager::destroy(vb);
        return triangleMeshes.size();
    }

    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    if (vkBeginCommandBuffer(uploadCmd, &bi) != VK_SUCCESS) {
        vkFreeCommandBuffers(stone_device(), transientPool, 1, &uploadCmd);
        vkDestroyCommandPool(stone_device(), transientPool, nullptr);
        BufferManager::destroy(vb);
        return triangleMeshes.size();
    }

    BufferManager::uploadToBuffer(vb, mesh->vertices.data(), mesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex), uploadCmd);

    uint64_t ib = BufferManager::create(
        mesh->indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        "LAS_Index");

    if (ib == 0) {
        vkEndCommandBuffer(uploadCmd);
        vkFreeCommandBuffers(stone_device(), transientPool, 1, &uploadCmd);
        vkDestroyCommandPool(stone_device(), transientPool, nullptr);
        BufferManager::destroy(vb);
        return triangleMeshes.size();
    }

    BufferManager::uploadToBuffer(ib, mesh->indices.data(), mesh->indices.size() * sizeof(uint32_t), uploadCmd);

    vkEndCommandBuffer(uploadCmd);

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &uploadCmd };
    vkQueueSubmit(stone_graphics_queue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(stone_graphics_queue());

    vkFreeCommandBuffers(stone_device(), transientPool, 1, &uploadCmd);
    vkDestroyCommandPool(stone_device(), transientPool, nullptr);

    InternalMesh m{ vb, ib, static_cast<uint32_t>(mesh->indices.size() / 3), static_cast<uint32_t>(mesh->vertices.size()), materialIndex };
    triangleMeshes.push_back(std::move(m));
    pendingBlasBuilds = tlasDirty = true;

    return triangleMeshes.size() - 1;
}

void RTX::LAS::hotReloadMesh(size_t index, std::unique_ptr<MeshLoader::Mesh> newMesh) {
    if (index >= triangleMeshes.size() || !newMesh) return;

    auto& m = triangleMeshes[index];

    if (m.blas) g_ext.vkDestroyAccelerationStructureKHR(stone_device(), m.blas, nullptr);
    BufferManager::destroy(m.vertexBuffer);
    BufferManager::destroy(m.indexBuffer);
    BufferManager::destroy(m.blasStorage);

    VkCommandPool transientPool = VK_NULL_HANDLE;
    VkCommandBuffer uploadCmd = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, stone_graphics_family() };
    if (vkCreateCommandPool(stone_device(), &pci, nullptr, &transientPool) != VK_SUCCESS) return;

    VkCommandBufferAllocateInfo ai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, transientPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
    if (vkAllocateCommandBuffers(stone_device(), &ai, &uploadCmd) != VK_SUCCESS) {
        vkDestroyCommandPool(stone_device(), transientPool, nullptr);
        return;
    }

    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    if (vkBeginCommandBuffer(uploadCmd, &bi) != VK_SUCCESS) {
        vkFreeCommandBuffers(stone_device(), transientPool, 1, &uploadCmd);
        vkDestroyCommandPool(stone_device(), transientPool, nullptr);
        return;
    }

    m.vertexBuffer = BufferManager::create(
        newMesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        "LAS_Vertex_HotReload");

    if (m.vertexBuffer == 0) {
        vkEndCommandBuffer(uploadCmd);
        vkFreeCommandBuffers(stone_device(), transientPool, 1, &uploadCmd);
        vkDestroyCommandPool(stone_device(), transientPool, nullptr);
        return;
    }

    BufferManager::uploadToBuffer(m.vertexBuffer, newMesh->vertices.data(),
                                  newMesh->vertices.size() * sizeof(MeshLoader::Mesh::Vertex), uploadCmd);

    m.indexBuffer = BufferManager::create(
        newMesh->indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        "LAS_Index_HotReload");

    if (m.indexBuffer == 0) {
        vkEndCommandBuffer(uploadCmd);
        vkFreeCommandBuffers(stone_device(), transientPool, 1, &uploadCmd);
        vkDestroyCommandPool(stone_device(), transientPool, nullptr);
        BufferManager::destroy(m.vertexBuffer);
        return;
    }

    BufferManager::uploadToBuffer(m.indexBuffer, newMesh->indices.data(),
                                  newMesh->indices.size() * sizeof(uint32_t), uploadCmd);

    vkEndCommandBuffer(uploadCmd);

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &uploadCmd };
    vkQueueSubmit(stone_graphics_queue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(stone_graphics_queue());

    vkFreeCommandBuffers(stone_device(), transientPool, 1, &uploadCmd);
    vkDestroyCommandPool(stone_device(), transientPool, nullptr);

    m.vertexCount = static_cast<uint32_t>(newMesh->vertices.size());
    m.primitiveCount = static_cast<uint32_t>(newMesh->indices.size() / 3);
    m.blasBuilt = false;

    pendingBlasBuilds = tlasDirty = true;
}

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
    proceduralDirty = tlasDirty = true;

    LOG_INFO_CAT("LAS", "Procedural AABB added — type {}, scale {:.1f}", static_cast<int>(type), scale);
    return proceduralPrimitives.size() - 1;
}

size_t RTX::LAS::addProceduralSphere(const glm::vec3& center, float radius,
                                     uint32_t materialIndex, const glm::mat4& transform) {
    return addProceduralAABB(GeometryType::ProceduralSphere, center, radius, materialIndex, transform);
}

size_t RTX::LAS::addProceduralCylinder(const glm::vec3& center, float radius, float height,
                                       uint32_t materialIndex, const glm::mat4& transform) {
    glm::vec3 halfExtents = glm::vec3(radius, height * 0.5f, radius);
    return addProceduralAABB(GeometryType::ProceduralCylinder, center, glm::length(halfExtents), materialIndex, transform);
}

size_t RTX::LAS::addProceduralCone(const glm::vec3& center, float radius, float height,
                                   uint32_t materialIndex, const glm::mat4& transform) {
    glm::vec3 halfExtents = glm::vec3(radius, height, radius);
    return addProceduralAABB(GeometryType::ProceduralCone, center, glm::length(halfExtents), materialIndex, transform);
}

size_t RTX::LAS::addProceduralD4(const glm::vec3& center, float size,
                                 uint32_t materialIndex, const glm::mat4& transform) {
    return addProceduralAABB(GeometryType::ProceduralD4, center, size, materialIndex, transform);
}

size_t RTX::LAS::addProceduralD6(const glm::vec3& center, float size,
                                 uint32_t materialIndex, const glm::mat4& transform) {
    return addProceduralAABB(GeometryType::ProceduralD6, center, size, materialIndex, transform);
}

size_t RTX::LAS::addProceduralD8(const glm::vec3& center, float size,
                                 uint32_t materialIndex, const glm::mat4& transform) {
    return addProceduralAABB(GeometryType::ProceduralD8, center, size, materialIndex, transform);
}

size_t RTX::LAS::addProceduralD10(const glm::vec3& center, float size,
                                  uint32_t materialIndex, const glm::mat4& transform) {
    return addProceduralAABB(GeometryType::ProceduralD10, center, size, materialIndex, transform);
}

size_t RTX::LAS::addProceduralD12(const glm::vec3& center, float size,
                                  uint32_t materialIndex, const glm::mat4& transform) {
    return addProceduralAABB(GeometryType::ProceduralD12, center, size, materialIndex, transform);
}

size_t RTX::LAS::addProceduralD20(const glm::vec3& center, float size,
                                  uint32_t materialIndex, const glm::mat4& transform) {
    return addProceduralAABB(GeometryType::ProceduralD20, center, size, materialIndex, transform);
}

void RTX::LAS::setInstanceTransform(size_t index, const glm::mat4& transform) {
    if (index < triangleMeshes.size()) {
        triangleMeshes[index].transform = transform;
    } else if (index < triangleMeshes.size() + proceduralPrimitives.size()) {
        proceduralPrimitives[index - triangleMeshes.size()].transform = transform;
    } else return;

    tlasDirty = true;
}

void RTX::LAS::destroyPrimitive(size_t index, float amount) {
    if (index >= proceduralPrimitives.size()) return;
    proceduralPrimitives[index].destruction = glm::clamp(amount, 0.0f, 1.0f);
    proceduralDirty = tlasDirty = true;
}

bool RTX::LAS::batchBuildAndCompactBLAS(VkCommandBuffer cmd) {
    if (!cmd) return false;
    bool built = false;

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
        g_ext.vkGetAccelerationStructureBuildSizesKHR(stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                      &buildInfo, &primCount, &sizes);

        VkDeviceAddress scratchAddr = BufferManager::allocateScratch(sizes.buildScratchSize);
        if (scratchAddr == 0) return false;
        buildInfo.scratchData.deviceAddress = scratchAddr;

        m.blasStorage = BufferManager::create(sizes.accelerationStructureSize,
                                              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                              "LAS_TriBLAS_Storage");

        if (m.blasStorage == 0) return false;

        VkAccelerationStructureCreateInfoKHR createCI{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        createCI.buffer = BufferManager::get_buffer(m.blasStorage);
        createCI.size = sizes.accelerationStructureSize;
        createCI.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        if (g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createCI, nullptr, &m.blas) != VK_SUCCESS) return false;

        buildInfo.dstAccelerationStructure = m.blas;

        VkAccelerationStructureBuildRangeInfoKHR range{ primCount };
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
        g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

        insertASBuildToBuildBarrier(cmd);

        built = true;
        m.blasBuilt = true;
    }

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
        g_ext.vkGetAccelerationStructureBuildSizesKHR(stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                      &buildInfo, &primCount, &sizes);

        VkDeviceAddress scratchAddr = BufferManager::allocateScratch(sizes.buildScratchSize);
        if (scratchAddr == 0) return false;
        buildInfo.scratchData.deviceAddress = scratchAddr;

        proceduralBlasStorage = BufferManager::create(sizes.accelerationStructureSize,
                                                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                      "LAS_ProcBLAS_Storage");

        if (proceduralBlasStorage == 0) return false;

        VkAccelerationStructureCreateInfoKHR createCI{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        createCI.buffer = BufferManager::get_buffer(proceduralBlasStorage);
        createCI.size = sizes.accelerationStructureSize;
        createCI.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        if (g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createCI, nullptr, &proceduralBlas) != VK_SUCCESS) return false;

        buildInfo.dstAccelerationStructure = proceduralBlas;

        VkAccelerationStructureBuildRangeInfoKHR range{ primCount };
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
        g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

        insertASBuildToBuildBarrier(cmd);

        built = true;
    }

    return built;
}

bool RTX::LAS::buildHybridTLAS(VkCommandBuffer cmd) {
    if (!cmd) return false;

    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(triangleMeshes.size() + proceduralPrimitives.size());

    for (const auto& m : triangleMeshes) {
        if (!m.blas) continue;

        VkAccelerationStructureInstanceKHR inst{};
        inst.transform = convertToVkTransform(m.transform);
        inst.instanceCustomIndex = m.materialIndex;
        inst.mask = 0xFF;
        inst.instanceShaderBindingTableRecordOffset = 0;
        inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR addrCI{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
        addrCI.accelerationStructure = m.blas;
        inst.accelerationStructureReference = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &addrCI);

        instances.push_back(inst);
    }

    for (size_t i = 0; i < proceduralPrimitives.size(); ++i) {
        const auto& p = proceduralPrimitives[i];

        VkAccelerationStructureInstanceKHR inst{};
        inst.transform = convertToVkTransform(p.transform);
        inst.instanceCustomIndex = p.materialIndex;
        inst.mask = 0xFF;
        inst.instanceShaderBindingTableRecordOffset = 1;
        inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR addrCI{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
        addrCI.accelerationStructure = proceduralBlas;
        inst.accelerationStructureReference = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &addrCI);

        instances.push_back(inst);
    }

    if (instances.empty()) return false;

    BufferManager::uploadToBuffer(instanceBuffer, instances.data(),
                                  instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
                                  cmd);

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
    g_ext.vkGetAccelerationStructureBuildSizesKHR(stone_device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                  &buildInfo, &instCount, &sizes);

    VkDeviceAddress scratchAddr = BufferManager::allocateScratch(sizes.buildScratchSize);
    if (scratchAddr == 0) return false;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    tlasStorage = BufferManager::create(sizes.accelerationStructureSize,
                                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                        "LAS_TLAS_Storage");

    if (tlasStorage == 0) return false;

    VkAccelerationStructureCreateInfoKHR createCI{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    createCI.buffer = BufferManager::get_buffer(tlasStorage);
    createCI.size = sizes.accelerationStructureSize;
    createCI.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    if (g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createCI, nullptr, &tlas) != VK_SUCCESS) return false;

    buildInfo.dstAccelerationStructure = tlas;

    VkAccelerationStructureBuildRangeInfoKHR range{ instCount };
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    return true;
}

void RTX::LAS::clearTLAS() {
    if (tlas) {
        g_ext.vkDestroyAccelerationStructureKHR(stone_device(), tlas, nullptr);
        tlas = VK_NULL_HANDLE;
    }
    BufferManager::destroy(tlasStorage);
    tlasStorage = 0;
}

void RTX::LAS::createDefaultHybridScene() {
    addProceduralAABB(GeometryType::ProceduralPlane, glm::vec3(0, -0.1f, 0), 25000.0f, 0, glm::mat4(1.0f));

    addProceduralSphere(glm::vec3(0, 5, 0), 2.0f, 0, glm::mat4(1.0f));
    addProceduralSphere(glm::vec3(4, 5, 4), 1.5f, 1, glm::mat4(1.0f));
    addProceduralSphere(glm::vec3(-4, 5, -4), 1.5f, 2, glm::mat4(1.0f));

    float diceRingRadius = 10.0f;
    for (int i = 0; i < 6; ++i) {
        float angle = i * (3.14159f * 2.0f / 6.0f);
        glm::vec3 pos = glm::vec3(std::cos(angle) * diceRingRadius, 3.0f, std::sin(angle) * diceRingRadius);
        addProceduralD6(pos, 2.0f, 3 + i, glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0,1,0)));
    }

    addProceduralCylinder(glm::vec3(-15, 10, -15), 2.0f, 20.0f, 4, glm::mat4(1.0f));
    addProceduralCylinder(glm::vec3(15, 10, -15), 2.0f, 20.0f, 4, glm::mat4(1.0f));
    addProceduralCylinder(glm::vec3(-15, 10, 15), 2.0f, 20.0f, 4, glm::mat4(1.0f));
    addProceduralCylinder(glm::vec3(15, 10, 15), 2.0f, 20.0f, 4, glm::mat4(1.0f));

    addProceduralCone(glm::vec3(0, 15, 0), 5.0f, 10.0f, 5, glm::mat4(1.0f));
}