// =============================================================================
// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// FIRST LIGHT ETERNAL — RESIZE = INSTANT — TEARING = DEAD — DECEMBER 04 2025
// PINK PHOTONS PROTECT — THE EMPIRE IS ETERNAL — AMOURANTH ASCENDANT
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

using namespace RTX;
using StoneKey::stone_device;
using StoneKey::stone_graphics_queue;

namespace RTX {

// ============================================================================
// Zero-Tearing TLAS Ring — FULLY RESIZE-PROOF — ETERNAL PINK SHIELD
// ============================================================================

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

struct FrameContext {
    VkFence frameFence    = VK_NULL_HANDLE;
    bool    needsRebuild  = true;
};

static FrameContext g_frameContexts[MAX_FRAMES_IN_FLIGHT]{};
static uint32_t     g_currentFrameIndex = 0;
static uint32_t     g_currentWriteSlot  = 0;
static uint64_t     g_globalFrameCounter = 0;

static VkFence createFence() noexcept
{
    VkFenceCreateInfo info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(stone_device(), &info, nullptr, &fence));
    return fence;
}

// ============================================================================
// One-Time Submit Helpers
// ============================================================================

[[nodiscard]] inline VkCommandBuffer beginOneTimeSubmit(VkCommandPool pool) noexcept
{
    if (pool == VK_NULL_HANDLE) pool = g_ctx().commandPool_;
    if (pool == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("LAS", "No command pool — empire not ready");
        return VK_NULL_HANDLE;
    }

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    const VkCommandBufferAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1u
    };
    VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd));

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    return cmd;
}

inline void endOneTimeSubmit(VkCommandBuffer cmd,
                            VkQueue queue,
                            VkFence fence,
                            VkCommandPool pool) noexcept
{
    if (cmd == VK_NULL_HANDLE) return;

    VK_CHECK(vkEndCommandBuffer(cmd));

    if (queue == VK_NULL_HANDLE) queue = g_ctx().graphicsQueue_;

    const VkSubmitInfo submit{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &cmd
    };

    VK_CHECK(vkQueueSubmit(queue, 1u, &submit, fence));

    if (fence == VK_NULL_HANDLE) {
        vkQueueWaitIdle(queue);
    }

    if (pool == VK_NULL_HANDLE) pool = g_ctx().commandPool_;
    if (pool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(stone_device(), pool, 1u, &cmd);
    }
}

// =============================================================================
// BLAS — FULLY RESTORED FROM YOUR ORIGINAL PERFECT CODE
// =============================================================================

void LAS::buildBLAS(VkCommandPool pool,
                    VkQueue queue,
                    uint64_t vertexHandle,
                    uint64_t indexHandle,
                    uint32_t vertexCount,
                    uint32_t indexCount,
                    VkBuildAccelerationStructureFlagsKHR extraFlags) noexcept
{
    const VkDevice dev = stone_device();
    if (dev == VK_NULL_HANDLE) return;

    const auto* vbuf = BufferManager::get(vertexHandle);
    const auto* ibuf = BufferManager::get(indexHandle);
    if (!vbuf || !ibuf || vertexCount == 0 || indexCount % 3 != 0) {
        LOG_FATAL_CAT("LAS", "Invalid BLAS input");
        return;
    }

    const VkDeviceAddress vaddr = BufferManager::get_device_address(vertexHandle);
    const VkDeviceAddress iaddr = BufferManager::get_device_address(indexHandle);
    const uint32_t primitiveCount = indexCount / 3;

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData   = { .deviceAddress = vaddr },
        .vertexStride = sizeof(glm::vec3),
        .maxVertex    = vertexCount - 1,
        .indexType    = VK_INDEX_TYPE_UINT32,
        .indexData    = { .deviceAddress = iaddr }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry     = { .triangles = triangles },
        .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkBuildAccelerationStructureFlagsKHR buildFlags = extraFlags;
    if (Options::OptionsLAS::PREFER_FAST_TRACE) {
        buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    } else if (Options::OptionsLAS::PREFER_FAST_BUILD) {
        buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    }
    if (Options::OptionsLAS::UPDATE_EVERY_FRAME) {
        buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    }
    if (Options::OptionsLAS::COMPACT_TLAS) {
        buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags         = buildFlags,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    g_ext.vkGetAccelerationStructureBuildSizesKHR(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                   &buildInfo, &primitiveCount, &sizeInfo);

    uint64_t tempStorage = 0;
    uint64_t scratch     = 0;

    BUFFER_CREATE(tempStorage, sizeInfo.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Temp");
    BUFFER_CREATE(scratch, sizeInfo.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Scratch");

    VkAccelerationStructureKHR tempAS = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = BufferManager::get(tempStorage)->buffer,
        .size   = sizeInfo.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &tempAS));

    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = tempAS;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(scratch);

    const VkAccelerationStructureBuildRangeInfoKHR range{ .primitiveCount = primitiveCount };
    const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &range };

    VkCommandBuffer cmd = beginOneTimeSubmit(pool);
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ranges);
    endOneTimeSubmit(cmd, queue, VK_NULL_HANDLE, pool);

    uint64_t finalStorage = tempStorage;
    VkAccelerationStructureKHR finalAS = tempAS;
    VkDeviceSize compactedSize = sizeInfo.accelerationStructureSize;
    tempStorage = 0;
    tempAS = VK_NULL_HANDLE;

    if (Options::OptionsLAS::COMPACT_TLAS) {
        // Compaction
        VkQueryPool queryPool = VK_NULL_HANDLE;
        VkQueryPoolCreateInfo qci{
            .sType     = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            .queryCount = 1
        };
        VK_CHECK(vkCreateQueryPool(dev, &qci, nullptr, &queryPool));

        cmd = beginOneTimeSubmit(pool);
        vkCmdResetQueryPool(cmd, queryPool, 0, 1);
        g_ext.vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, 1, &tempAS,
            VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);
        endOneTimeSubmit(cmd, queue, VK_NULL_HANDLE, pool);

        VK_CHECK(vkGetQueryPoolResults(dev, queryPool, 0, 1, sizeof(compactedSize),
                                       &compactedSize, sizeof(compactedSize), VK_QUERY_RESULT_WAIT_BIT));
        vkDestroyQueryPool(dev, queryPool, nullptr);

        if (compactedSize < sizeInfo.accelerationStructureSize) {
            LOG_SUCCESS_CAT("LAS", "BLAS compacted: {:.2f} → {:.2f} MiB",
                sizeInfo.accelerationStructureSize / 1048576.0, compactedSize / 1048576.0);

            BUFFER_CREATE(finalStorage, compactedSize,
                          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Final");

            createInfo.buffer = BufferManager::get(finalStorage)->buffer;
            createInfo.size   = compactedSize;
            VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &finalAS));

            cmd = beginOneTimeSubmit(pool);
            VkCopyAccelerationStructureInfoKHR copyInfo{
                .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
                .src   = tempAS,
                .dst   = finalAS,
                .mode  = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
            };
            g_ext.vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
            endOneTimeSubmit(cmd, queue, VK_NULL_HANDLE, pool);

            g_ext.vkDestroyAccelerationStructureKHR(dev, tempAS, nullptr);
            BufferManager::destroy(tempStorage);
            tempStorage = 0;
            tempAS = VK_NULL_HANDLE;
        }
    }

    BufferManager::destroy(scratch);

    if (blas_.valid()) blas_.reset();

    auto deleter = [dev, finalStorage](VkDevice, VkAccelerationStructureKHR as, const VkAllocationCallbacks*) {
        if (as) g_ext.vkDestroyAccelerationStructureKHR(dev, as, nullptr);
        BufferManager::destroy(finalStorage);
    };

    blas_ = Handle<VkAccelerationStructureKHR>(finalAS, dev, deleter, compactedSize, "WonderBLAS");

    LOG_SUCCESS_CAT("LAS", "BLAS FORGED — {} triangles — {:.2f} MiB", primitiveCount, compactedSize / 1048576.0);
}

// =============================================================================
// TLAS — ETERNAL ZERO-TEAR + RESIZE-PROOF — FINAL VERSION
// =============================================================================

namespace {

struct TLASFrame {
    VkAccelerationStructureKHR tlas          = VK_NULL_HANDLE;
    uint64_t                   storageHandle = 0;
    uint64_t                   scratchHandle = 0;
    VkDeviceSize               size          = 0;
};

static TLASFrame g_tlasFrames[MAX_FRAMES_IN_FLIGHT]{};
static constexpr uint64_t g_maxScratchSize = 512ULL * 1024 * 1024;
static bool g_tlasInitialized = false;

} // anonymous namespace

void LAS::initTLAS() noexcept
{
    if (g_tlasInitialized) return;

    for (auto& f : g_tlasFrames) {
        f.scratchHandle = BufferManager::create(g_maxScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TLAS_Scratch");
    }
    for (auto& ctx : g_frameContexts) {
        if (!ctx.frameFence) ctx.frameFence = createFence();
    }
    g_tlasInitialized = true;
    LOG_SUCCESS_CAT("LAS", "TLAS RING INITIALIZED — ETERNAL PINK SHIELD ACTIVE");
}

void LAS::notifyResize() noexcept
{
    LOG_AMOURANTH("LAS::notifyResize() — FULL TLAS PURGE — RING RESET — PHOTONS REBORN");

    // Mark all slots as needing full rebuild
    for (auto& ctx : g_frameContexts) ctx.needsRebuild = true;

    // Destroy all TLAS objects and storage
    for (auto& frame : g_tlasFrames) {
        if (frame.tlas) {
            g_ext.vkDestroyAccelerationStructureKHR(stone_device(), frame.tlas, nullptr);
            frame.tlas = VK_NULL_HANDLE;
        }
        if (frame.storageHandle) {
            BufferManager::destroy(frame.storageHandle);
            frame.storageHandle = 0;
        }
        frame.size = 0;
    }

    // FULL RING RESET — THIS IS THE KEY TO INSTANT RESIZE
    g_currentFrameIndex = 0;
    g_currentWriteSlot  = 0;
    g_globalFrameCounter = 0;

    LOG_AMOURANTH("TLAS RING PURGED — ALL SLOTS FRESH — RESIZE RECOVERY COMPLETE");
}

void LAS::buildTLAS(VkCommandPool pool,
                    VkQueue queue,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances,
                    bool isDynamic) noexcept
{
    initTLAS();
    if (instances.empty()) {
        LOG_FAILURE("LAS", "No instances — clearing TLAS");
        tlas_.reset();
        if (instanceBufferId_) BufferManager::destroy(std::exchange(instanceBufferId_, 0));
        return;
    }

    const VkDevice dev = stone_device();
    const VkDeviceSize dataSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

    uint64_t instanceBuffer = BufferManager::createHostVisible(dataSize, "TLAS_InstanceBuffer");
    auto* mapped = static_cast<VkAccelerationStructureInstanceKHR*>(
        BufferManager::getMappedStagingPtr(instanceBuffer));

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& [blasAS, transform] = instances[i];

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blasAS
        };
        VkDeviceAddress blasAddr = g_ext.vkGetAccelerationStructureDeviceAddressKHR(dev, &addrInfo);

        const glm::mat3x4 trans = glm::transpose(transform);

        mapped[i] = VkAccelerationStructureInstanceKHR{
            .transform = VkTransformMatrixKHR{ .matrix = {
                { trans[0][0], trans[0][1], trans[0][2], trans[0][3] },
                { trans[1][0], trans[1][1], trans[1][2], trans[1][3] },
                { trans[2][0], trans[2][1], trans[2][2], trans[2][3] }
            }},
            .instanceCustomIndex            = static_cast<uint32_t>(i),
            .mask                           = 0xFF,
            .flags                          = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blasAddr
        };
    }

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data  = { .deviceAddress = BufferManager::get_device_address(instanceBuffer) }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     = { .instances = instancesData }
    };

    VkBuildAccelerationStructureFlagsKHR buildFlags = 0;
    if (Options::OptionsLAS::PREFER_FAST_TRACE) {
        buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    } else if (Options::OptionsLAS::PREFER_FAST_BUILD) {
        buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    }
    if (Options::OptionsLAS::UPDATE_EVERY_FRAME && !Options::OptionsLAS::REBUILD_EVERY_FRAME) {
        buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    }
    if (Options::OptionsLAS::COMPACT_TLAS) {
        buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = buildFlags,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    const uint32_t instanceCount = static_cast<uint32_t>(instances.size());

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    g_ext.vkGetAccelerationStructureBuildSizesKHR(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                   &buildInfo, &instanceCount, &sizeInfo);

    auto& frame = g_tlasFrames[g_currentWriteSlot];
    bool forceRebuild = g_frameContexts[g_currentWriteSlot].needsRebuild || Options::OptionsLAS::REBUILD_EVERY_FRAME;

    if (forceRebuild || sizeInfo.accelerationStructureSize > frame.size) {
        if (frame.tlas) g_ext.vkDestroyAccelerationStructureKHR(dev, frame.tlas, nullptr);
        if (frame.storageHandle) BufferManager::destroy(frame.storageHandle);

        BUFFER_CREATE(frame.storageHandle, sizeInfo.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TLAS_Storage");

        VkAccelerationStructureCreateInfoKHR ci{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = BufferManager::get(frame.storageHandle)->buffer,
            .size   = sizeInfo.accelerationStructureSize,
            .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
        };
        VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &ci, nullptr, &frame.tlas));
        frame.size = sizeInfo.accelerationStructureSize;
    }

    buildInfo.mode                    = (frame.tlas && !forceRebuild)
        ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
        : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.srcAccelerationStructure = (buildInfo.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR) ? frame.tlas : VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = frame.tlas;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(frame.scratchHandle);

    const VkAccelerationStructureBuildRangeInfoKHR range{ .primitiveCount = instanceCount };
    const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &range };

    VkCommandBuffer cmd = beginOneTimeSubmit(pool);
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ranges);

    VkFence buildFence = Options::OptionsLAS::COMPACT_TLAS ? VK_NULL_HANDLE : g_frameContexts[g_currentWriteSlot].frameFence;
    endOneTimeSubmit(cmd, queue, buildFence, pool);

    if (Options::OptionsLAS::COMPACT_TLAS) {
        // Compaction - since we waited idle, proceed
        VkQueryPool queryPool = VK_NULL_HANDLE;
        VkQueryPoolCreateInfo qci{
            .sType     = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            .queryCount = 1
        };
        VK_CHECK(vkCreateQueryPool(dev, &qci, nullptr, &queryPool));

        cmd = beginOneTimeSubmit(pool);
        vkCmdResetQueryPool(cmd, queryPool, 0, 1);
        g_ext.vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, 1, &buildInfo.dstAccelerationStructure,
            VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);
        endOneTimeSubmit(cmd, queue, VK_NULL_HANDLE, pool);

        VkDeviceSize compactedSize = 0;
        VK_CHECK(vkGetQueryPoolResults(dev, queryPool, 0, 1, sizeof(compactedSize),
                                       &compactedSize, sizeof(compactedSize), VK_QUERY_RESULT_WAIT_BIT));
        vkDestroyQueryPool(dev, queryPool, nullptr);

        if (compactedSize < frame.size && compactedSize > 0) {
            LOG_SUCCESS_CAT("LAS", "TLAS compacted: {:.2f} → {:.2f} MiB",
                frame.size / 1048576.0, compactedSize / 1048576.0);

            uint64_t newStorage = 0;
            BUFFER_CREATE(newStorage, compactedSize,
                          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TLAS_Storage_Compact");

            VkAccelerationStructureKHR newAS = VK_NULL_HANDLE;
            VkAccelerationStructureCreateInfoKHR ci{
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                .buffer = BufferManager::get(newStorage)->buffer,
                .size   = compactedSize,
                .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
            };
            VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &ci, nullptr, &newAS));

            cmd = beginOneTimeSubmit(pool);
            VkCopyAccelerationStructureInfoKHR copyInfo{
                .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
                .src   = frame.tlas,
                .dst   = newAS,
                .mode  = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
            };
            g_ext.vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
            endOneTimeSubmit(cmd, queue, VK_NULL_HANDLE, pool);

            g_ext.vkDestroyAccelerationStructureKHR(dev, frame.tlas, nullptr);
            BufferManager::destroy(frame.storageHandle);

            frame.tlas = newAS;
            frame.storageHandle = newStorage;
            frame.size = compactedSize;
        }
    }

    tlas_.reset();
    uint64_t oldBuffer = instanceBufferId_;
    instanceBufferId_ = instanceBuffer;

    auto deleter = [oldBuffer](auto...) {
        if (oldBuffer) BufferManager::destroy(oldBuffer);
    };

    tlas_ = Handle<VkAccelerationStructureKHR>(frame.tlas, dev, deleter,
        sizeInfo.accelerationStructureSize, "WonderTLAS");
    tlasSize_ = sizeInfo.accelerationStructureSize;

    g_frameContexts[g_currentWriteSlot].needsRebuild = false;

    LOG_SUCCESS_CAT("LAS", "TLAS {} — {} instances — SLOT {} — ZERO TEARING",
        buildInfo.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR ? "UPDATED" : "FORGED",
        instances.size(), g_currentWriteSlot);
}

void LAS::beginFrame() noexcept
{
    ++g_globalFrameCounter;
    uint32_t retiredSlot = g_currentFrameIndex % MAX_FRAMES_IN_FLIGHT;

    auto& ctx = g_frameContexts[retiredSlot];
    if (ctx.frameFence) {
        vkWaitForFences(stone_device(), 1, &ctx.frameFence, VK_TRUE, 5'000'000'000ULL);
        vkResetFences(stone_device(), 1, &ctx.frameFence);
    }

    ++g_currentFrameIndex;
    g_currentWriteSlot = g_currentFrameIndex % MAX_FRAMES_IN_FLIGHT;

    LOG_TRACE_CAT("LAS", "beginFrame() → global#{} | retired#{} | nextWrite#{}",
                  g_globalFrameCounter, retiredSlot, g_currentWriteSlot);
}

// ============================================================================
// Address Getters & Fence Management
// ============================================================================

VkDeviceAddress LAS::getBLASAddress() const noexcept
{
    if (!blas_.valid()) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blas_.get()
    };
    return g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);
}

VkDeviceAddress LAS::getTLASAddress() const noexcept
{
    if (!tlas_.valid()) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = tlas_.get()
    };
    return g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);
}

void RTX::LAS::waitForAllFences()
{
    if (stone_device() == VK_NULL_HANDLE) return;

    const size_t fenceCount = buildFences_.size();
    if (fenceCount == 0) return;

    vkWaitForFences(stone_device(), static_cast<uint32_t>(fenceCount), buildFences_.data(), VK_TRUE, 10'000'000'000ULL);
    vkResetFences(stone_device(), static_cast<uint32_t>(fenceCount), buildFences_.data());
    buildFences_.clear();

    LOG_SUCCESS_CAT("LAS", "All {} in-flight AS builds completed", fenceCount);
}

} // namespace RTX

// =============================================================================
// FIRST LIGHT ETERNAL — DECEMBER 04 2025
// RESIZE = INSTANT — BLACK SCREEN = DEAD — TLAS = ALWAYS FRESH
// PINK PHOTONS PROTECT — THE EMPIRE IS ETERNAL — AMOURANTH ASCENDANT
// =============================================================================