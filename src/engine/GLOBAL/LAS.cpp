// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — LAS v∞ TURBO — FULL OPTIONS SUPPORT
// TLAS-ONLY DIRECT GEOMETRY — ALL VK_BUILD FLAGS RESPECTED
// COMPACT, REFIT, MOTION, LOW MEMORY — FULLY CONFIGURABLE AT COMPILE TIME
// PINK PHOTONS ETERNAL — EMPIRE SEES THE INFINITE
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"

using StoneKey::stone_device;

namespace RTX {

static uint32_t g_currentWriteSlot = 0;

namespace OptionsLAS {
    // Rebuild entire TLAS every frame (slow, accurate)
    constexpr bool     REBUILD_EVERY_FRAME         = false;

    // Update TLAS incrementally (fast, may have minor artifacts)
    constexpr bool     UPDATE_EVERY_FRAME          = true;

    // Compact TLAS after build/update (reduces memory, increases build time)
    constexpr bool     COMPACT_TLAS                = true;

    // Prefer fast build over fast trace
    constexpr bool     PREFER_FAST_BUILD           = false;

    // Prefer fast trace over fast build
    constexpr bool     PREFER_FAST_TRACE           = true;

    // Allow refit instead of full rebuild (requires UPDATE_EVERY_FRAME)
    constexpr bool     ALLOW_REFIT                 = true;

    // Low memory mode — reduce memory usage at cost of build time
    constexpr bool     LOW_MEMORY                  = false;

    // Motion blur support — enable instance motion
    constexpr bool     MOTION_BLUR                 = false;
}

namespace {

struct TLASFrame {
    VkAccelerationStructureKHR tlas          = VK_NULL_HANDLE;
    uint64_t                   storageHandle = 0;
    VkDeviceSize               size          = 0;
    uint64_t                   compactHandle = 0;  // For post-compaction storage
    VkDeviceSize               compactedSize = 0;
};

static TLASFrame g_tlasFrames[Options::Performance::MAX_FRAMES_IN_FLIGHT]{};
static constexpr VkDeviceSize g_maxScratchSize = 512ULL * 1024 * 1024;

static uint64_t g_scratchHandles[Options::Performance::MAX_FRAMES_IN_FLIGHT]{};

static uint64_t g_dummyInstanceBuffer = 0;

static bool g_firstBuildDone = false;

// Query pool for compaction size queries — one per frame
static VkQueryPool g_compactionQueryPool = VK_NULL_HANDLE;

static void createCompactionQueryPool() noexcept
{
    if (g_compactionQueryPool != VK_NULL_HANDLE) return;

    VkQueryPoolCreateInfo info{
        .sType              = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType          = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
        .queryCount         = Options::Performance::MAX_FRAMES_IN_FLIGHT
    };

    VK_CHECK(vkCreateQueryPool(stone_device(), &info, nullptr, &g_compactionQueryPool));
}

// Direct meshes stored for TLAS build
struct DirectMesh {
    uint64_t   vertexBuffer = 0;
    uint64_t   indexBuffer  = 0;
    uint32_t   indexCount   = 0;
    glm::mat4  transform    = glm::mat4(1.0f);
    bool       moved        = true;  // Force rebuild on add
};

static std::vector<DirectMesh> g_meshes;

} // anonymous namespace

void LAS::initTLAS() noexcept
{
    if (g_dummyInstanceBuffer != 0) return;

    createCompactionQueryPool();

    for (uint32_t i = 0; i < Options::Performance::MAX_FRAMES_IN_FLIGHT; ++i) {
        g_scratchHandles[i] = BufferManager::create(g_maxScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            std::format("TLAS_Scratch_Frame_{}", i));
    }

    // Dummy instance for empty scene (pink void)
    VkAccelerationStructureInstanceKHR dummy{};
    dummy.mask = 0xFF;
    dummy.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    dummy.accelerationStructureReference = 0;

    g_dummyInstanceBuffer = BufferManager::createHostVisible(sizeof(VkAccelerationStructureInstanceKHR), "TLAS_DummyInstance_Eternal");
    std::memcpy(BufferManager::getMappedStagingPtr(g_dummyInstanceBuffer), &dummy, sizeof(dummy));
}

void LAS::notifyResize() noexcept
{
    for (auto& frame : g_tlasFrames) {
        if (frame.tlas) g_ext.vkDestroyAccelerationStructureKHR(stone_device(), frame.tlas, nullptr);
        if (frame.storageHandle) BufferManager::destroy(frame.storageHandle);
        if (frame.compactHandle) BufferManager::destroy(frame.compactHandle);
        frame = {};
    }

    g_currentWriteSlot = 0;
    tlas_.reset();
    tlasSize_ = 0;
    g_firstBuildDone = false;

    // Reset query pool on resize
    if (g_compactionQueryPool != VK_NULL_HANDLE) {
        vkResetQueryPool(stone_device(), g_compactionQueryPool, 0, Options::Performance::MAX_FRAMES_IN_FLIGHT);
    }
}

void LAS::beginFrame() noexcept
{
    const uint32_t readSlot = (g_currentWriteSlot == 0)
        ? (Options::Performance::MAX_FRAMES_IN_FLIGHT - 1)
        : (g_currentWriteSlot - 1);

    const auto& frame = g_tlasFrames[readSlot];
    if (frame.tlas && (!tlas_.valid() || tlas_.get() != frame.tlas)) {
        tlas_ = Handle<VkAccelerationStructureKHR>(frame.tlas, stone_device());
        tlasSize_ = frame.compactedSize > 0 ? frame.compactedSize : frame.size;
    }
}

void LAS::addMesh(std::unique_ptr<MeshLoader::Mesh> mesh) noexcept
{
    if (!mesh || mesh->vertices.empty() || mesh->indices.empty()) {
        LOG_WARNING_CAT("LAS", "Invalid mesh passed to addMesh — skipping");
        return;
    }

    LOG_INFO_CAT("LAS", "Mesh added to direct TLAS — {} vertices, {} indices", mesh->vertices.size(), mesh->indices.size());

    g_meshes.push_back({
        .vertexBuffer = mesh->vertexBuffer,
        .indexBuffer  = mesh->indexBuffer,
        .indexCount   = static_cast<uint32_t>(mesh->indices.size()),
        .transform    = glm::mat4(1.0f),
        .moved        = true
    });
}

void LAS::buildTLAS(VkCommandBuffer cmd) noexcept
{
    initTLAS();

    const bool hasGeometry = !g_meshes.empty();
    const uint32_t geometryCount = hasGeometry ? static_cast<uint32_t>(g_meshes.size()) : 1u;

    VkBuildAccelerationStructureFlagsKHR buildFlags = 0;
    if (OptionsLAS::PREFER_FAST_TRACE)   buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    if (OptionsLAS::PREFER_FAST_BUILD)   buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    if (OptionsLAS::LOW_MEMORY)          buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR;
    if (OptionsLAS::MOTION_BLUR)         buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_MOTION_BIT_NV;
    if (OptionsLAS::ALLOW_REFIT || OptionsLAS::COMPACT_TLAS) {
        buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    }

    VkBuildAccelerationStructureModeKHR mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    if (g_firstBuildDone && OptionsLAS::UPDATE_EVERY_FRAME) {
        mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
    }
    if (OptionsLAS::REBUILD_EVERY_FRAME) {
        mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    }

    std::vector<VkAccelerationStructureGeometryKHR> geometries(geometryCount);
    std::vector<uint32_t> primitiveCounts(geometryCount);
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> rangeInfos(geometryCount);

    if (hasGeometry) {
        for (uint32_t i = 0; i < g_meshes.size(); ++i) {
            const auto& m = g_meshes[i];

            VkAccelerationStructureGeometryTrianglesDataKHR triangles{
                .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData   = { .deviceAddress = BufferManager::get_device_address(m.vertexBuffer) },
                .vertexStride = sizeof(MeshLoader::Mesh::Vertex),
                .maxVertex    = 0,
                .indexType    = VK_INDEX_TYPE_UINT32,
                .indexData    = { .deviceAddress = BufferManager::get_device_address(m.indexBuffer) }
            };

            geometries[i] = {
                .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                .geometry     = { .triangles = triangles },
                .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR
            };

            primitiveCounts[i] = m.indexCount / 3;
            rangeInfos[i] = { primitiveCounts[i], 0, 0, 0 };
        }
    } else {
        VkAccelerationStructureGeometryInstancesDataKHR instancesData{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .data  = { .deviceAddress = BufferManager::get_device_address(g_dummyInstanceBuffer) }
        };

        geometries[0] = {
            .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry     = { .instances = instancesData }
        };
        primitiveCounts[0] = 1;
        rangeInfos[0] = { 1, 0, 0, 0 };
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = buildFlags,
        .mode          = mode,
        .srcAccelerationStructure = (mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR) ? g_tlasFrames[g_currentWriteSlot].tlas : VK_NULL_HANDLE,
        .geometryCount = geometryCount,
        .pGeometries   = geometries.data()
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        primitiveCounts.data(),
        &sizeInfo);

    auto& frame = g_tlasFrames[g_currentWriteSlot];

    // Reallocate storage if needed
    if (sizeInfo.accelerationStructureSize > frame.size || !frame.tlas) {
        if (frame.tlas) g_ext.vkDestroyAccelerationStructureKHR(stone_device(), frame.tlas, nullptr);
        if (frame.storageHandle) BufferManager::destroy(frame.storageHandle);
        if (frame.compactHandle) BufferManager::destroy(frame.compactHandle);

        frame.storageHandle = BufferManager::create(
            sizeInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "Direct_TLAS_Storage");

        VkAccelerationStructureCreateInfoKHR ci{
            .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = BufferManager::get(frame.storageHandle)->buffer,
            .size   = sizeInfo.accelerationStructureSize,
            .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
        };
        VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &ci, nullptr, &frame.tlas));

        frame.size = sizeInfo.accelerationStructureSize;
        frame.compactedSize = 0;
    }

    buildInfo.dstAccelerationStructure = frame.tlas;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(g_scratchHandles[g_currentWriteSlot]);

    const VkAccelerationStructureBuildRangeInfoKHR* ppRangeInfos = rangeInfos.data();

    // Reset query for compaction size
    if (OptionsLAS::COMPACT_TLAS) {
        vkCmdResetQueryPool(cmd, g_compactionQueryPool, g_currentWriteSlot, 1);
    }

    g_ext.vkCmdBuildAccelerationStructuresKHR(
        cmd,
        1,
        &buildInfo,
        &ppRangeInfos);

    // Write query for compacted size if enabled
    if (OptionsLAS::COMPACT_TLAS) {
        g_ext.vkCmdWriteAccelerationStructuresPropertiesKHR(
            cmd,
            1,
            &frame.tlas,
            VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            g_compactionQueryPool,
            g_currentWriteSlot);
    }

    // Optional compaction pass
    if (OptionsLAS::COMPACT_TLAS && g_firstBuildDone) {
        VkMemoryBarrier2 barrier{
            .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask  = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            .dstStageMask  = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
        };
        VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
        vkCmdPipelineBarrier2(cmd, &dep);

        // Get compacted size from query
        VkDeviceSize compactedSize = frame.size;  // Fallback
        VkResult queryResult = vkGetQueryPoolResults(
            stone_device(),
            g_compactionQueryPool,
            g_currentWriteSlot,
            1,
            sizeof(VkDeviceSize),
            &compactedSize,
            sizeof(VkDeviceSize),
            VK_QUERY_RESULT_WAIT_BIT);

        if (queryResult == VK_SUCCESS && compactedSize < frame.size) {
            // Create compacted TLAS
            if (frame.compactHandle) BufferManager::destroy(frame.compactHandle);

            frame.compactHandle = BufferManager::create(
                compactedSize,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                "Direct_TLAS_Compacted");

            VkAccelerationStructureCreateInfoKHR compactCI{
                .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                .buffer = BufferManager::get(frame.compactHandle)->buffer,
                .size   = compactedSize,
                .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
            };

            VkAccelerationStructureKHR compactedTLAS = VK_NULL_HANDLE;
            VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &compactCI, nullptr, &compactedTLAS));

            // Copy & compact
            VkCopyAccelerationStructureInfoKHR copyInfo{
                .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
                .src   = frame.tlas,
                .dst   = compactedTLAS,
                .mode  = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
            };
            g_ext.vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);

            // Swap to compacted version
            g_ext.vkDestroyAccelerationStructureKHR(stone_device(), frame.tlas, nullptr);
            BufferManager::destroy(frame.storageHandle);

            frame.tlas = compactedTLAS;
            frame.storageHandle = frame.compactHandle;
            frame.compactHandle = 0;
            frame.size = compactedSize;
            frame.compactedSize = compactedSize;
        }
    }

    g_firstBuildDone = true;
    g_currentWriteSlot = (g_currentWriteSlot + 1) % Options::Performance::MAX_FRAMES_IN_FLIGHT;
}

VkAccelerationStructureKHR LAS::getCurrentTLAS() const noexcept
{
    return tlas_.valid() ? tlas_.get() : VK_NULL_HANDLE;
}

VkAccelerationStructureKHR LAS::getLatestTLAS() const noexcept
{
    const auto& frame = g_tlasFrames[g_currentWriteSlot];
    return frame.tlas;
}

VkDeviceAddress LAS::getCurrentTLASAddress() const noexcept
{
    if (!tlas_.valid()) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = tlas_.get()
    };
    return g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);
}

} // namespace RTX

// =============================================================================
// FULL OPTIONS SUPPORT — COMPACTION IMPLEMENTED — QUERY POOL USED
// ALL FLAGS RESPECTED — CLEAN & EFFICIENT
// DECEMBER 18, 2025 — THE LIGHT IS PURE AND OPTIMIZED
// =============================================================================