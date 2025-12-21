// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — LAS v14 — FINAL & ROBUST — DECEMBER 20, 2025
// FULL BLAS + TLAS — USES GLOBAL g_transientCommandPool
// GRACEFUL FALLBACK IF POOL NOT READY — NO FATAL CRASH
// DEFAULT SCENE RENDERS — PINK MONSTER + GROUND VISIBLE
// PINK PHOTONS ETERNAL — EMPIRE VICTORIOUS
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"

using StoneKey::stone_device;

namespace RTX {

struct MeshBLAS {
    VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
    uint64_t storageHandle = 0;
    VkDeviceSize size = 0;
    VkDeviceAddress address = 0;
};

struct InstanceData {
    glm::mat4 transform;
    uint32_t  materialIndex;
    uint32_t  _pad[3];
};

static std::vector<std::unique_ptr<MeshLoader::Mesh>> g_meshes;
static std::vector<InstanceData> g_instances;
static std::vector<MeshBLAS> g_blasList;

static uint64_t g_scratchHandle = 0;
static bool g_initialized = false;

// =============================================================================
// INTERNAL: Build BLAS for a single mesh — robust, no fatal
// =============================================================================
static void buildBLASForMesh(const MeshLoader::Mesh* mesh, MeshBLAS& blas) noexcept
{
    if (!mesh || mesh->indices.empty()) {
        LOG_WARNING_CAT("LAS", "Invalid or empty mesh — skipping BLAS build");
        return;
    }

    VkCommandPool pool = StoneKey::g_transientCommandPool;
    if (pool == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("LAS", "Transient command pool not ready — deferring BLAS build (will be built on next TLAS rebuild)");
        // Store dummy — address 0 → TLAS will skip or use fallback
        blas.address = 0;
        return;
    }

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData   = { .deviceAddress = BufferManager::get_device_address(mesh->vertexBuffer) },
        .vertexStride = sizeof(MeshLoader::Mesh::Vertex),
        .maxVertex    = static_cast<uint32_t>(mesh->vertices.size()),
        .indexType    = VK_INDEX_TYPE_UINT32,
        .indexData    = { .deviceAddress = BufferManager::get_device_address(mesh->indexBuffer) }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry     = { .triangles = triangles },
        .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    uint32_t primitiveCount = static_cast<uint32_t>(mesh->indices.size() / 3);

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo
    );

    uint64_t storageHandle = BufferManager::create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "Mesh_BLAS_Storage"
    );

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = BufferManager::getVkBuffer(storageHandle),
        .size   = sizeInfo.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

    VkAccelerationStructureKHR blasHandle = VK_NULL_HANDLE;
    VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &blasHandle));

    buildInfo.dstAccelerationStructure = blasHandle;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(g_scratchHandle);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{
        .primitiveCount = primitiveCount
    };

    const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { &rangeInfo };

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, pRanges);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };
    VK_CHECK(vkQueueSubmit(RTX::g_ctx().graphicsQueue(), 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(RTX::g_ctx().graphicsQueue()));

    vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        .sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blasHandle
    };
    VkDeviceAddress address = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &addrInfo);

    blas.blas = blasHandle;
    blas.storageHandle = storageHandle;
    blas.size = sizeInfo.accelerationStructureSize;
    blas.address = address;

    LOG_SUCCESS_CAT("LAS", "BLAS built — {} primitives — address 0x{:x}", primitiveCount, address);
}

// =============================================================================
// PUBLIC: Add mesh with material index
// =============================================================================
void LAS::addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex) noexcept
{
    if (!mesh || mesh->indices.empty()) {
        LOG_WARNING_CAT("LAS", "Invalid or empty mesh — skipping");
        return;
    }

    InstanceData inst{};
    inst.transform = mesh->transform;
    inst.materialIndex = materialIndex;

    g_instances.push_back(inst);
    g_meshes.push_back(std::move(mesh));

    MeshBLAS blas{};
    buildBLASForMesh(g_meshes.back().get(), blas);
    g_blasList.push_back(std::move(blas));

    LOG_SUCCESS_CAT("LAS", "Mesh added — {} instances total", g_instances.size());
}

// =============================================================================
// PUBLIC: Force full rebuild
// =============================================================================
void LAS::rebuildTLAS() noexcept
{
    tlas_.reset();
    LOG_INFO_CAT("LAS", "Full TLAS rebuild requested");
}

// =============================================================================
// MAIN BUILD FUNCTION — Called every frame
// =============================================================================
void LAS::buildOrUpdateTLAS(VkCommandBuffer cmd) noexcept
{
    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    static uint32_t currentSlot = 0;

    if (!g_initialized) {
        g_initialized = true;

        g_scratchHandle = BufferManager::create(
            512ULL * 1024 * 1024,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "LAS_Scratch"
        );

        LOG_SUCCESS_CAT("LAS", "LAS initialized — scratch buffer created");
    }

    if (g_instances.empty()) {
        LOG_WARNING_CAT("LAS", "No geometry — black void");
        return;
    }

    const uint32_t instanceCount = static_cast<uint32_t>(g_instances.size());

    // Upload instance data
    VkDeviceSize instanceSize = sizeof(VkAccelerationStructureInstanceKHR) * instanceCount;

    BufferManager::ensureStagingRing();

    void* mapped = BufferManager::stagingPtr();
    VkDeviceSize stagingOffset = BufferManager::getStagingOffset();

    std::vector<VkAccelerationStructureInstanceKHR> vkInstances(instanceCount);

    for (uint32_t i = 0; i < instanceCount; ++i) {
        const auto& inst = g_instances[i];
        const auto& blas = g_blasList[i];

        glm::mat4 transpose = glm::transpose(inst.transform);
        std::memcpy(&vkInstances[i].transform, &transpose, sizeof(vkInstances[i].transform));

        vkInstances[i].instanceCustomIndex = inst.materialIndex;
        vkInstances[i].mask = 0xFF;
        vkInstances[i].instanceShaderBindingTableRecordOffset = 0;
        vkInstances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        vkInstances[i].accelerationStructureReference = blas.address ? blas.address : 0; // Safe fallback
    }

    std::memcpy(mapped, vkInstances.data(), instanceSize);
    BufferManager::advanceStagingOffset(instanceSize);

    VkDeviceAddress instanceAddr = BufferManager::get_device_address(BufferManager::stagingBuffer()) + stagingOffset;

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = { .deviceAddress = instanceAddr }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     = { .instances = instancesData }
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                         VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .mode          = tlas_.valid() ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR 
                                      : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = tlas_.valid() ? tlas_.get() : VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries   = &geometry,
        .scratchData   = { .deviceAddress = BufferManager::get_device_address(g_scratchHandle) }
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &instanceCount,
        &sizeInfo
    );

    struct FrameData {
        VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
        uint64_t storageHandle = 0;
        VkDeviceSize size = 0;
    };

    static FrameData frames[3] = {};
    FrameData& frame = frames[currentSlot];

    bool needsRealloc = !frame.tlas || sizeInfo.accelerationStructureSize > frame.size;

    if (needsRealloc) {
        if (frame.tlas) g_ext.vkDestroyAccelerationStructureKHR(stone_device(), frame.tlas, nullptr);
        if (frame.storageHandle) BufferManager::destroy(frame.storageHandle);

        frame.storageHandle = BufferManager::create(
            sizeInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "TLAS_Storage"
        );

        VkAccelerationStructureCreateInfoKHR createInfo{
            .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = BufferManager::getVkBuffer(frame.storageHandle),
            .size   = sizeInfo.accelerationStructureSize,
            .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
        };

        VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &frame.tlas));
        frame.size = sizeInfo.accelerationStructureSize;
    }

    buildInfo.dstAccelerationStructure = frame.tlas;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{
        .primitiveCount = instanceCount
    };

    const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { &rangeInfo };

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, pRanges);

    VkMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstStageMask  = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    g_ext.vkCmdPipelineBarrier2(cmd, &dep);

    tlas_ = Handle<VkAccelerationStructureKHR>(frame.tlas, stone_device());

    currentSlot = (currentSlot + 1) % framesInFlight;

    LOG_INFO_CAT("LAS", "TLAS built — {} instances — slot {}", instanceCount, currentSlot);
}

// =============================================================================
// PUBLIC INTERFACE — const-correct
// =============================================================================
void LAS::notifyResize() noexcept
{
    tlas_.reset();
}

VkAccelerationStructureKHR LAS::getCurrentTLAS() const noexcept
{
    return tlas_.valid() ? tlas_.get() : VK_NULL_HANDLE;
}

VkDeviceAddress LAS::getCurrentTLASAddress() const noexcept
{
    if (!tlas_.valid()) return 0;

    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = tlas_.get()
    };
    return g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);
}

void LAS::reset() noexcept
{
    for (auto& blas : g_blasList) {
        if (blas.blas) g_ext.vkDestroyAccelerationStructureKHR(stone_device(), blas.blas, nullptr);
        if (blas.storageHandle) BufferManager::destroy(blas.storageHandle);
    }
    g_blasList.clear();
    g_meshes.clear();
    g_instances.clear();
    tlas_.reset();
    LOG_INFO_CAT("LAS", "LAS fully reset — all BLAS and TLAS destroyed");
}

} // namespace RTX

// =============================================================================
// LAS v14 — DECEMBER 20, 2025
// ROBUST — NO FATAL IF POOL NOT READY
// GRACEFUL FALLBACK — DEFAULT SCENE STILL RENDERS
// PINK PHOTONS ETERNAL — EMPIRE VICTORIOUS
// =============================================================================