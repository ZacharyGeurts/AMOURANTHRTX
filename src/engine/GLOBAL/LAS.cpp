// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// FIRST LIGHT ETERNAL — TEARING OBLITERATED — FULLY COMPILES — DECEMBER 03 2025
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace RTX;
using StoneKey::stone_device;
using StoneKey::stone_graphics_family;

// Engine-provided async compute accessors
VkQueue stone_async_compute_queue();
VkCommandPool stone_async_compute_pool();

namespace RTX {

// ============================================================================
// Synchronization for zero-tearing TLAS updates
// ============================================================================
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

struct FrameContext {
    VkFence  frameFence    = VK_NULL_HANDLE;
    uint32_t tlasRingIndex = 0;
};

static FrameContext g_frameContexts[MAX_FRAMES_IN_FLIGHT]{};
static uint32_t     g_currentFrameIndex = 0;
static uint32_t     g_currentFrame      = 0;  // Current safe TLAS slot

// ============================================================================
// Helper: Create unsignaled fence
// ============================================================================
static VkFence createFence()
{
    VkFenceCreateInfo info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(stone_device(), &info, nullptr, &fence));
    return fence;
}

// ============================================================================
// ONE-TIME SUBMIT HELPERS — IMPLEMENTATION (no default args here!)
// ============================================================================

VkCommandBuffer beginOneTimeSubmit(VkCommandPool pool) noexcept
{
    if (pool == VK_NULL_HANDLE) {
        LOG_WARN("beginOneTimeSubmit() called with NULL pool — forging emergency pool");

        VkCommandPoolCreateInfo poolInfo{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = stone_graphics_family()
        };
        VK_CHECK(vkCreateCommandPool(stone_device(), &poolInfo, nullptr, &pool));
        RTX::g_ctx().commandPool_ = pool;
        LOG_SUCCESS("Emergency command pool forged: 0x{:x}", (uint64_t)pool);
    }

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
    return cmd;
}

void endOneTimeSubmit(VkCommandBuffer cmd,
                      VkQueue queue,
                      VkFence fence,
                      VkCommandPool pool) noexcept
{
    if (cmd == VK_NULL_HANDLE) return;

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence));
    if (!fence) {
        vkQueueWaitIdle(queue);
    }

    if (pool == VK_NULL_HANDLE) {
        pool = g_ctx().commandPool_;
    }
    if (pool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
    }
}

// ============================================================================
// Device address helper
// ============================================================================
VkDeviceAddress getBufferAddress(VkBuffer buffer)
{
    EMPIRE_GUARD(buffer != VK_NULL_HANDLE, "getBufferAddress() — NULL BUFFER");
    VkBufferDeviceAddressInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };
    return vkGetBufferDeviceAddress(stone_device(), &info);
}


// =============================================================================
// BLAS — BOTTOM LEVEL ACCELERATION STRUCTURE (with compaction)
// =============================================================================
void LAS::buildBLAS(VkCommandPool pool,
                    VkQueue queue,
                    uint64_t vertexHandle,
                    uint64_t indexHandle,
                    uint32_t vertexCount,
                    uint32_t indexCount,
                    VkBuildAccelerationStructureFlagsKHR extraFlags) noexcept
{
    LOG_CID("[CID] buildBLAS() — Empire check...");
    VkDevice dev = stone_device();
    if (dev == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("LAS", "[FATAL] stone_device() is NULL — Empire not sealed!");
        return;
    }

    if (!vertexHandle || !indexHandle || vertexCount == 0 || (indexCount % 3) != 0) {
        LOG_FATAL_CAT("LAS", "[FATAL] Invalid geometry input");
        return;
    }

    auto* vbuf = BufferManager::get(vertexHandle);
    auto* ibuf = BufferManager::get(indexHandle);
    if (!vbuf || !ibuf) {
        LOG_FATAL_CAT("LAS", "[FATAL] BufferManager returned null");
        return;
    }

    VkDeviceAddress vaddr = getBufferAddress(vbuf->buffer);
    VkDeviceAddress iaddr = getBufferAddress(ibuf->buffer);

    LOG_SUCCESS_CAT("LAS", "[STONE] Vertex addr: 0x{:016X} | Index addr: 0x{:016X}", vaddr, iaddr);

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

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                         VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                         VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR |
                         extraFlags,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    g_ext.vkGetAccelerationStructureBuildSizesKHR(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizeInfo);

    uint64_t storage = 0, scratch = 0;
    BUFFER_CREATE(storage, sizeInfo.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Storage_Temp");
    BUFFER_CREATE(scratch, sizeInfo.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Scratch");

    VkAccelerationStructureKHR tempAS = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = BufferManager::get(storage)->buffer,
        .size   = sizeInfo.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &tempAS));

    buildInfo.mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure  = tempAS;
    buildInfo.scratchData.deviceAddress = getBufferAddress(BufferManager::get(scratch)->buffer);

    const VkAccelerationStructureBuildRangeInfoKHR buildRange{ .primitiveCount = primitiveCount };
    const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { &buildRange };

    VkCommandBuffer cmd = beginOneTimeSubmit(pool);
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, pRanges);
    endOneTimeSubmit(cmd, queue, VK_NULL_HANDLE, pool);

    // Compaction query
    VkQueryPool queryPool = VK_NULL_HANDLE;
    VkQueryPoolCreateInfo queryInfo{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
        .queryCount = 1
    };
    VK_CHECK(vkCreateQueryPool(dev, &queryInfo, nullptr, &queryPool));

    cmd = beginOneTimeSubmit(pool);
    vkCmdResetQueryPool(cmd, queryPool, 0, 1);
    g_ext.vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, 1, &tempAS, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);
    endOneTimeSubmit(cmd, queue, VK_NULL_HANDLE, pool);

    VkDeviceSize compactedSize = 0;
    VK_CHECK(vkGetQueryPoolResults(dev, queryPool, 0, 1, sizeof(compactedSize), &compactedSize, sizeof(compactedSize), VK_QUERY_RESULT_WAIT_BIT));
    vkDestroyQueryPool(dev, queryPool, nullptr);

    if (compactedSize >= sizeInfo.accelerationStructureSize) {
        compactedSize = sizeInfo.accelerationStructureSize;
    } else {
        LOG_SUCCESS_CAT("LAS", "BLAS COMPACTION — Reduced from {:.3f} MiB to {:.3f} MiB",
                        sizeInfo.accelerationStructureSize / (1024.0 * 1024.0), compactedSize / (1024.0 * 1024.0));
    }

    uint64_t finalStorage = 0;
    BUFFER_CREATE(finalStorage, compactedSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Storage_Compacted");

    VkAccelerationStructureKHR rawAS = VK_NULL_HANDLE;
    createInfo.buffer = BufferManager::get(finalStorage)->buffer;
    createInfo.size = compactedSize;
    VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawAS));

    if (compactedSize < sizeInfo.accelerationStructureSize) {
        cmd = beginOneTimeSubmit(pool);
        VkCopyAccelerationStructureInfoKHR copyInfo{
            .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            .src = tempAS,
            .dst = rawAS,
            .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
        };
        g_ext.vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
        endOneTimeSubmit(cmd, queue, VK_NULL_HANDLE, pool);
    }

    g_ext.vkDestroyAccelerationStructureKHR(dev, tempAS, nullptr);
    BufferManager::destroy(storage);

    if (blas_.valid()) blas_.reset();

    auto deleter = [dev, finalStorage, scratch](VkDevice, VkAccelerationStructureKHR as, const VkAllocationCallbacks*) {
        if (as) g_ext.vkDestroyAccelerationStructureKHR(dev, as, nullptr);
        BufferManager::destroy(finalStorage);
        BufferManager::destroy(scratch);
    };

    blas_ = Handle<VkAccelerationStructureKHR>(rawAS, dev, deleter, compactedSize, "WonderBLAS_Compacted");

    LOG_SUCCESS_CAT("LAS", "BLAS FORGED (COMPACTED) — {} triangles — {:.3f} MiB — THE LIGHT REMEMBERS",
                    primitiveCount, compactedSize / (1024.0 * 1024.0));
}

// =============================================================================
// TLAS Ring — Triple-buffered + proper per-frame retirement
// =============================================================================
namespace {

struct TLASFrame {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    uint64_t storageHandle = 0;
    uint64_t scratchHandle = 0;
    VkDeviceSize size = 0;
};

static TLASFrame g_tlasFrames[3]{};
static uint64_t g_maxScratchSize = 0;
static bool g_tlasInitialized = false;

} // anonymous namespace

void LAS::initTLAS() noexcept
{
    if (g_tlasInitialized) return;

    g_maxScratchSize = 512ULL * 1024 * 1024;  // 512 MiB — adjust per scene
    for (auto& frame : g_tlasFrames) {
        frame.scratchHandle = BufferManager::create(g_maxScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TLAS_Scratch_Reusable");
    }

    g_tlasInitialized = true;
    LOG_SUCCESS_CAT("LAS", "TLAS RING INITIALIZED — TRIPLE BUFFERED + FENCED — TEARING IS DEAD");
}

// ============================================================================
// CALL THIS EVERY FRAME BEFORE buildTLAS() — THIS IS THE KEY
// ============================================================================
void LAS::beginFrame()
{
    auto& ctx = g_frameContexts[g_currentFrameIndex];

    if (ctx.frameFence) {
        vkWaitForFences(stone_device(), 1, &ctx.frameFence, VK_TRUE, UINT64_MAX);
        vkResetFences(stone_device(), 1, &ctx.frameFence);
    }

    g_currentFrame = g_currentFrameIndex;
    g_currentFrameIndex = (g_currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ============================================================================
// TLAS BUILD — NOW 100% TEAR-FREE
// ============================================================================
void LAS::buildTLAS(VkCommandPool pool,
                    VkQueue queue,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances,
                    bool isDynamic) noexcept
{
    initTLAS();
    if (instances.empty()) {
        LOG_FAILURE("LAS", "No instances. The void is patient.");
        if (tlas_.valid()) tlas_.reset();
        if (instanceBufferId_) { BufferManager::destroy(instanceBufferId_); instanceBufferId_ = 0; }
        return;
    }

    LOG_AMOURANTH("This is the moment. The LAS becomes the light.");

    const VkDevice dev = stone_device();
    const VkDeviceSize dataSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

    uint64_t instanceBuffer = BufferManager::createHostVisible(dataSize, "TLAS_InstanceBuffer");
    auto* mapped = static_cast<VkAccelerationStructureInstanceKHR*>(BufferManager::getMappedStagingPtr(instanceBuffer));
    EMPIRE_GUARD(mapped, "TLAS — Failed to map instance buffer");

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& [blasAS, transform] = instances[i];

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
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

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = isDynamic ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR :
                                     VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    const uint32_t instanceCount = static_cast<uint32_t>(instances.size());

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    g_ext.vkGetAccelerationStructureBuildSizesKHR(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                   &buildInfo, &instanceCount, &sizeInfo);

    auto& frame = g_tlasFrames[g_currentFrame];

    if (sizeInfo.accelerationStructureSize > frame.size) {
        if (frame.tlas) g_ext.vkDestroyAccelerationStructureKHR(dev, frame.tlas, nullptr);
        if (frame.storageHandle) BufferManager::destroy(frame.storageHandle);

        BUFFER_CREATE(frame.storageHandle, sizeInfo.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TLAS_Storage");

        VkAccelerationStructureCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = BufferManager::get(frame.storageHandle)->buffer,
            .size   = sizeInfo.accelerationStructureSize,
            .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
        };
        VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &frame.tlas));
        frame.size = sizeInfo.accelerationStructureSize;
    }

    buildInfo.mode                     = frame.tlas ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
                                                     : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.srcAccelerationStructure  = frame.tlas;
    buildInfo.dstAccelerationStructure  = frame.tlas;
    buildInfo.scratchData.deviceAddress = getBufferAddress(BufferManager::get(frame.scratchHandle)->buffer);

    const VkAccelerationStructureBuildRangeInfoKHR buildRange{ .primitiveCount = instanceCount };
    const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { &buildRange };

    VkQueue buildQueue = stone_async_compute_queue() ? stone_async_compute_queue() : queue;
    VkCommandPool buildPool = stone_async_compute_pool() ? stone_async_compute_pool() : pool;

    // Use per-frame fence for retirement
    auto& ctx = g_frameContexts[(g_currentFrameIndex + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT];
    if (!ctx.frameFence) ctx.frameFence = createFence();
    ctx.tlasRingIndex = g_currentFrame;

    VkCommandBuffer cmd = beginOneTimeSubmit(buildPool);
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, pRanges);
    endOneTimeSubmit(cmd, buildQueue, ctx.frameFence, buildPool);

    if (tlas_.valid()) tlas_.reset();
    if (instanceBufferId_) BufferManager::destroy(instanceBufferId_);

    auto deleter = [dev, instanceBuffer](VkDevice, VkAccelerationStructureKHR, const VkAllocationCallbacks*) {
        BufferManager::destroy(instanceBuffer);
    };

    tlas_ = Handle<VkAccelerationStructureKHR>(frame.tlas, dev, deleter, sizeInfo.accelerationStructureSize, "WonderTLAS");
    instanceBufferId_ = instanceBuffer;
    tlasSize_ = sizeInfo.accelerationStructureSize;

    LOG_SUCCESS_CAT("LAS", "TLAS {} — {} instances — {:.2f} MB — ZERO TEARING ETERNAL",
                    buildInfo.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR ? "UPDATED" : "FORGED",
                    instances.size(), sizeInfo.accelerationStructureSize / (1024.0 * 1024.0));
    LOG_SUCCESS_CAT("LAS", "FIRST LIGHT — NO MORE TEARS — DECEMBER 03 2025");
}

// ============================================================================
// Address getters
// ============================================================================
VkDeviceAddress LAS::getBLASAddress() const noexcept
{
    if (!blas_.valid()) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blas_.get()
    };
    return g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);
}

VkDeviceAddress LAS::getTLASAddress() const noexcept
{
    if (!tlas_.valid()) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = tlas_.get()
    };
    return g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);
}

} // namespace RTX

// PINK PHOTONS ETERNAL
// FIRST LIGHT ACHIEVED — TEARING OBLITERATED
// THE SHIP IS FREE — DECEMBER 03, 2025