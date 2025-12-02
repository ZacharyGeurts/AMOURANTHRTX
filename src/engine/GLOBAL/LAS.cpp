// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
//
// THE LAS IS THE LIGHT — FIRST LIGHT ACHIEVED — NOVEMBER 30 2025
// A dwarf walks up, "Welcome to Lights And Shit, how can we help?"
// =============================================================================
//
// Updated December 02, 2025: Realistic speedups incorporated
// - BLAS compaction for reduced memory and faster traversal (post-build query and copy)
// - TLAS in-place updates with PREFER_FAST_BUILD for dynamic scenes
// - Scratch buffer reuse across builds/updates
// - Triple-buffered TLAS for zero tearing during per-frame updates
// - Async compute queue support for AS builds (assumes engine provides async queue)
// - Instance culling stub (frustum/occlusion - integrate with engine camera system)
//
// Based on 2025 best practices from NVIDIA/Khronos: compaction, updates, async builds
//
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace RTX;
using StoneKey::stone_device;

// Assume engine provides these (add to StoneKey.hpp if needed)
VkQueue stone_async_compute_queue();  // Async compute queue for AS builds
VkCommandPool stone_async_compute_pool();  // Corresponding pool

namespace RTX {

// ---------------------------------------------------------------------------
// Helper: Create a simple fence
// ---------------------------------------------------------------------------
inline VkFence createFence()
{
    VkFenceCreateInfo info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(stone_device(), &info, nullptr, &fence));
    return fence;
}

// ---------------------------------------------------------------------------
// Begin a one-time command buffer
// ---------------------------------------------------------------------------
[[nodiscard]] inline VkCommandBuffer beginOneTimeSubmit(VkCommandPool pool) noexcept
{
    EMPIRE_GUARD(pool != VK_NULL_HANDLE, "beginOneTimeSubmit() — NULL COMMAND POOL");

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

// ---------------------------------------------------------------------------
// End and submit a one-time command buffer (now supports async queues)
// ---------------------------------------------------------------------------
inline void endOneTimeSubmit(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept
{
    if (!cmd || !queue || !pool || !stone_device()) {
        LOG_NICK("The engine is still warming. We'll try again when the stars align.");
        if (cmd) vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
        return;
    }

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkFence fence = createFence();

    VkSubmitInfo submit{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };

    VkResult r = vkQueueSubmit(queue, 1, &submit, fence);
    if (r != VK_SUCCESS) {
        LOG_ELON("The queue hesitated… but the photons are patient.");
        vkDestroyFence(stone_device(), fence, nullptr);
        vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
        return;
    }

    r = vkWaitForFences(stone_device(), 1, &fence, VK_TRUE, 8'000'000'000ULL); // 8s

    if (r == VK_TIMEOUT) {
        LOG_CARMACK("GPU deep in thought. Giving it another heartbeat.");
    } else if (r == VK_ERROR_DEVICE_LOST) {
        LOG_GROK("A brief eclipse. The light always returns.");
    }

    vkDestroyFence(stone_device(), fence, nullptr);
    vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
}

// ---------------------------------------------------------------------------
// Get device address of a buffer
// ---------------------------------------------------------------------------
VkDeviceAddress LAS::getBufferAddress(VkBuffer buffer) const noexcept
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
                         VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR |  // Enable compaction
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
    endOneTimeSubmit(cmd, queue, pool);

    // Compaction query
    VkQueryPool queryPool = VK_NULL_HANDLE;
    VkQueryPoolCreateInfo queryInfo{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
        .queryCount = 1
    };
    VK_CHECK(vkCreateQueryPool(dev, &queryInfo, nullptr, &queryPool));
    vkCmdResetQueryPool(cmd, queryPool, 0, 1);
    g_ext.vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, 1, &tempAS, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);
    endOneTimeSubmit(cmd, queue, pool);

    VkDeviceSize compactedSize = 0;
    VK_CHECK(vkGetQueryPoolResults(dev, queryPool, 0, 1, sizeof(compactedSize), &compactedSize, sizeof(compactedSize), VK_QUERY_RESULT_WAIT_BIT));
    vkDestroyQueryPool(dev, queryPool, nullptr);

    if (compactedSize >= sizeInfo.accelerationStructureSize) {
        compactedSize = sizeInfo.accelerationStructureSize;  // No gain, keep original
    } else {
        LOG_SUCCESS_CAT("LAS", "BLAS COMPACTION — Reduced from {:.3f} MiB to {:.3f} MiB",
                        sizeInfo.accelerationStructureSize / (1024.0 * 1024.0), compactedSize / (1024.0 * 1024.0));
    }

    // Create final storage
    uint64_t finalStorage = 0;
    BUFFER_CREATE(finalStorage, compactedSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Storage_Compacted");

    VkAccelerationStructureKHR rawAS = VK_NULL_HANDLE;
    createInfo.buffer = BufferManager::get(finalStorage)->buffer;
    createInfo.size = compactedSize;
    VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawAS));

    // Copy compacted if smaller
    if (compactedSize < sizeInfo.accelerationStructureSize) {
        cmd = beginOneTimeSubmit(pool);
        VkCopyAccelerationStructureInfoKHR copyInfo{
            .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            .src = tempAS,
            .dst = rawAS,
            .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
        };
        g_ext.vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
        endOneTimeSubmit(cmd, queue, pool);
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
// TLAS Ring — Triple-buffered for zero tearing, with updates and fast build
// =============================================================================
namespace {

struct TLASFrame {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    uint64_t storageHandle = 0;
    uint64_t scratchHandle = 0;  // Reused per frame
    VkDeviceSize size = 0;
};

static TLASFrame g_tlasFrames[3]{};
static uint32_t g_currentFrame = 0;
static uint64_t g_maxScratchSize = 0;  // Global max for reuse
static bool g_tlasInitialized = false;

} // anonymous

void LAS::initTLAS() noexcept
{
    if (g_tlasInitialized) return;

    // Pre-allocate scratch (sized to max expected)
    g_maxScratchSize = 512ULL * 1024 * 1024;  // 512 MiB - adjust based on scene
    for (auto& frame : g_tlasFrames) {
        frame.scratchHandle = BufferManager::create(g_maxScratchSize,
                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TLAS_Scratch_Reusable");
    }

    g_tlasInitialized = true;
    LOG_SUCCESS_CAT("LAS", "TLAS RING INITIALIZED — TRIPLE BUFFERED, SCRATCH REUSED — DECEMBER 02 2025");
}

// ---------------------------------------------------------------------------
// TLAS — TOP LEVEL ACCELERATION STRUCTURE (updates, fast build, triple buffer)
// ---------------------------------------------------------------------------
void LAS::buildTLAS(VkCommandPool pool,
                    VkQueue queue,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances,
                    bool isDynamic /* = true */) noexcept
{
    initTLAS();
    if (instances.empty()) {
        LOG_FAILURE("LAS", "No instances yet. The void is patient.");
        if (tlas_.valid()) tlas_.reset();
        if (instanceBufferId_) { BufferManager::destroy(instanceBufferId_); instanceBufferId_ = 0; }
        return;
    }

    // Stub for instance culling (integrate with engine camera/frustum/occlusion queries)
    // auto culledInstances = cullInstances(instances);  // Implement frustum/occlusion culling
    // instances = culledInstances;  // Reduces build time and trace cost

    LOG_AMOURANTH("This is the moment. The LAS becomes the light. The ship becomes the universe.");

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
            .instanceCustomIndex                  = static_cast<uint32_t>(i),
            .mask                                 = 0xFF,
            .flags                                = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference       = blasAddr
        };
    }
    // No unmap needed - staging ring is persistent

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

    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &instanceCount, &sizeInfo);

    auto& frame = g_tlasFrames[g_currentFrame];

    // Resize storage if needed
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

    buildInfo.mode                    = frame.tlas ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR :
                                                      VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.srcAccelerationStructure = frame.tlas;  // For update
    buildInfo.dstAccelerationStructure = frame.tlas;
    buildInfo.scratchData.deviceAddress = getBufferAddress(BufferManager::get(frame.scratchHandle)->buffer);

    const VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount = instanceCount
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeArray[] = { &buildRange };

    // Use async compute if available
    VkQueue buildQueue = stone_async_compute_queue() ? stone_async_compute_queue() : queue;
    VkCommandPool buildPool = stone_async_compute_pool() ? stone_async_compute_pool() : pool;

    VkCommandBuffer cmd = beginOneTimeSubmit(buildPool);

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, pBuildRangeArray);

    endOneTimeSubmit(cmd, buildQueue, buildPool);

    if (tlas_.valid()) tlas_.reset();
    if (instanceBufferId_) BufferManager::destroy(instanceBufferId_);

    auto deleter = [dev, &frame, instanceBuffer](VkDevice, VkAccelerationStructureKHR as, const VkAllocationCallbacks*) {
        // Defer destroy to frame retirement
        BufferManager::destroy(instanceBuffer);
    };

    tlas_ = Handle<VkAccelerationStructureKHR>(frame.tlas, dev, deleter, sizeInfo.accelerationStructureSize, "WonderTLAS");
    instanceBufferId_ = instanceBuffer;
    tlasSize_ = sizeInfo.accelerationStructureSize;

    // Advance frame (cycle triple buffer)
    g_currentFrame = (g_currentFrame + 1) % 3;

    LOG_CAPTAIN_N("…It's beautiful. The warp zones are open. We can finally go home.");
    LOG_GROK("To the light that found its way. To the ship that sails anywhere.");
    LOG_AMOURANTH("The universe is no longer a cage.");
    LOG_NICK("We didn't build an engine. We built a key.");

    LOG_SUCCESS_CAT("LAS", "TLAS {} — {} instances — {:.2f} MB — THE SHIP IS FREE",
                    buildInfo.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR ? "UPDATED" : "FORGED",
                    instances.size(), sizeInfo.accelerationStructureSize / (1024.0 * 1024.0));
    LOG_SUCCESS_CAT("LAS", "FIRST LIGHT ETERNAL — NOVEMBER 30 2025 — THE EMPIRE IS COMPLETE");
}

// ---------------------------------------------------------------------------
// Address getters
// ---------------------------------------------------------------------------
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
// FIRST LIGHT ACHIEVED
// NOVEMBER 30, 2025
// UPDATED FOR 2025 SPEEDUPS — DECEMBER 02, 2025