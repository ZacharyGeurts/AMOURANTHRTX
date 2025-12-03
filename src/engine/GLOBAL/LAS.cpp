// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// FIRST LIGHT ETERNAL — TEARING OBLITERATED — FULLY COMPILES — DECEMBER 03 2025
// THE EMPIRE IS SEALED — PINK PHOTONS FLOW ETERNAL
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace RTX;
using StoneKey::stone_device;
using StoneKey::stone_graphics_queue;

namespace RTX {

// ============================================================================
// Zero-Tearing TLAS Ring
// ============================================================================

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

struct FrameContext {
    VkFence  frameFence    = VK_NULL_HANDLE;
    uint32_t tlasRingIndex = 0;
};

static FrameContext g_frameContexts[MAX_FRAMES_IN_FLIGHT]{};
static uint32_t     g_currentFrameIndex = 0;
static uint32_t     g_currentFrame      = 0;

static VkFence createFence() noexcept
{
    VkFenceCreateInfo info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(stone_device(), &info, nullptr, &fence));
    return fence;
}

// ============================================================================
// One-Time Submit Helpers — EXACTLY MATCHES HEADER
// ============================================================================

[[nodiscard]] VkCommandBuffer beginOneTimeSubmit(VkCommandPool pool) noexcept
{
    if (pool == VK_NULL_HANDLE) {
        pool = g_ctx().commandPool_;
        if (pool == VK_NULL_HANDLE) {
            LOG_FATAL_CAT("LAS", "No command pool — empire not ready");
            return VK_NULL_HANDLE;
        }
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
    if (!fence) vkQueueWaitIdle(queue);

    if (pool == VK_NULL_HANDLE) pool = g_ctx().commandPool_;
    if (pool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
    }
}

// =============================================================================
// BLAS — Compacted Bottom-Level Acceleration Structure
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

    // Compaction
    VkQueryPool queryPool = VK_NULL_HANDLE;
    VkQueryPoolCreateInfo qci{
        .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType  = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
        .queryCount = 1
    };
    VK_CHECK(vkCreateQueryPool(dev, &qci, nullptr, &queryPool));

    cmd = beginOneTimeSubmit(pool);
    vkCmdResetQueryPool(cmd, queryPool, 0, 1);
    g_ext.vkCmdWriteAccelerationStructuresPropertiesKHR(cmd, 1, &tempAS,
        VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);
    endOneTimeSubmit(cmd, queue, VK_NULL_HANDLE, pool);

    VkDeviceSize compactedSize = sizeInfo.accelerationStructureSize;
    VK_CHECK(vkGetQueryPoolResults(dev, queryPool, 0, 1, sizeof(compactedSize),
                                   &compactedSize, sizeof(compactedSize), VK_QUERY_RESULT_WAIT_BIT));
    vkDestroyQueryPool(dev, queryPool, nullptr);

    if (compactedSize < sizeInfo.accelerationStructureSize) {
        LOG_SUCCESS_CAT("LAS", "BLAS compacted: {:.2f} to {:.2f} MiB",
            sizeInfo.accelerationStructureSize / 1048576.0, compactedSize / 1048576.0);
    }

    uint64_t finalStorage = 0;
    BUFFER_CREATE(finalStorage, compactedSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Final");

    VkAccelerationStructureKHR finalAS = VK_NULL_HANDLE;
    createInfo.buffer = BufferManager::get(finalStorage)->buffer;
    createInfo.size   = compactedSize;
    VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &finalAS));

    if (compactedSize < sizeInfo.accelerationStructureSize) {
        cmd = beginOneTimeSubmit(pool);
        VkCopyAccelerationStructureInfoKHR copyInfo{
            .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            .src   = tempAS,
            .dst   = finalAS,
            .mode  = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
        };
        g_ext.vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
        endOneTimeSubmit(cmd, queue, VK_NULL_HANDLE, pool);
    }

    g_ext.vkDestroyAccelerationStructureKHR(dev, tempAS, nullptr);
    BufferManager::destroy(tempStorage);
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
// TLAS — Triple-Buffered, Zero-Tearing
// =============================================================================

namespace {

struct TLASFrame {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    uint64_t                   storageHandle = 0;
    uint64_t                   scratchHandle = 0;
    VkDeviceSize               size = 0;
};

static TLASFrame g_tlasFrames[3]{};
static constexpr uint64_t g_maxScratchSize = 512ULL * 1024 * 1024;
static bool g_tlasInitialized = false;

} // anonymous

void LAS::initTLAS() noexcept
{
    if (g_tlasInitialized) return;

    for (auto& f : g_tlasFrames) {
        f.scratchHandle = BufferManager::create(g_maxScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TLAS_Scratch");
    }
    g_tlasInitialized = true;
    LOG_SUCCESS_CAT("LAS", "TLAS RING INITIALIZED — ZERO TEARING");
}

void LAS::buildTLAS(VkCommandPool pool,
                    VkQueue queue,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances,
                    bool isDynamic) noexcept
{
    initTLAS();
    if (instances.empty()) {
        LOG_FAILURE("LAS", "No instances");
        tlas_.reset();
        if (instanceBufferId_) BufferManager::destroy(std::exchange(instanceBufferId_, 0));
        return;
    }

    LOG_AMOURANTH("The LAS becomes the light.");

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

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = isDynamic ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR
                                    : VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
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

        VkAccelerationStructureCreateInfoKHR ci{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = BufferManager::get(frame.storageHandle)->buffer,
            .size   = sizeInfo.accelerationStructureSize,
            .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
        };
        VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &ci, nullptr, &frame.tlas));
        frame.size = sizeInfo.accelerationStructureSize;
    }

    buildInfo.mode                    = frame.tlas ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
                                                     : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.srcAccelerationStructure = frame.tlas;
    buildInfo.dstAccelerationStructure = frame.tlas;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(frame.scratchHandle);

    const VkAccelerationStructureBuildRangeInfoKHR range{ .primitiveCount = instanceCount };
    const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &range };

    auto& ctx = g_frameContexts[(g_currentFrameIndex + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT];
    if (!ctx.frameFence) ctx.frameFence = createFence();
    ctx.tlasRingIndex = g_currentFrame;

    VkCommandBuffer cmd = beginOneTimeSubmit(pool);
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ranges);
    endOneTimeSubmit(cmd, queue, ctx.frameFence, pool);

    tlas_.reset();
    if (instanceBufferId_) BufferManager::destroy(std::exchange(instanceBufferId_, instanceBuffer));
    else instanceBufferId_ = instanceBuffer;

    auto deleter = [instanceBuffer](auto...) { BufferManager::destroy(instanceBuffer); };
    tlas_ = Handle<VkAccelerationStructureKHR>(frame.tlas, dev, deleter,
        sizeInfo.accelerationStructureSize, "WonderTLAS");

    LOG_SUCCESS_CAT("LAS", "TLAS {} — {} instances — {:.2f} MB — ZERO TEARING",
        buildInfo.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR ? "UPDATED" : "FORGED",
        instances.size(), sizeInfo.accelerationStructureSize / 1048576.0);
}

// ============================================================================
// Address Getters
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
    if (g_ctx().device() == VK_NULL_HANDLE) {
        LOG_WARN_CAT("LAS", "waitForAllFences() — device null, nothing to wait for");
        return;
    }

    const size_t fenceCount = buildFences_.size();
    if (fenceCount == 0) {
        return; // No builds in flight
    }

    LOG_TRACE_CAT("LAS", "Waiting for {} in-flight AS build fence(s)...", fenceCount);

    // Wait for all fences — non-blocking if already signaled
    VkResult result = vkWaitForFences(
        g_ctx().device(),
        static_cast<uint32_t>(fenceCount),
        buildFences_.data(),
        VK_TRUE,           // waitAll = true
        10'000'000'000ULL // 10 seconds — more than enough
    );

    if (result == VK_SUCCESS) {
        LOG_TRACE_CAT("LAS", "All {} AS build fences signaled — safe to proceed", fenceCount);
    } else if (result == VK_TIMEOUT) {
        LOG_ERROR_CAT("LAS", "Timeout waiting for AS build fences — forcing reset anyway");
    } else {
        LOG_ERROR_CAT("LAS", "vkWaitForFences failed: {}", static_cast<int>(result));
    }

    // Reset all fences for reuse
    vkResetFences(g_ctx().device(), static_cast<uint32_t>(fenceCount), buildFences_.data());
}

void LAS::beginFrame() noexcept
{
    // Increment global frame counter
    ++g_currentFrameIndex;

    // The TLAS slot that just became safe to reuse:
    // It was written MAX_FRAMES_IN_FLIGHT frames ago
    const uint32_t retiredSlot = g_currentFrameIndex % MAX_FRAMES_IN_FLIGHT;

    auto& ctx = g_frameContexts[retiredSlot];

    // Wait for the GPU to finish using this slot (if it ever had a build)
    if (ctx.frameFence != VK_NULL_HANDLE) {
        waitForAllFences();
        vkResetFences(stone_device(), 1, &ctx.frameFence);
        // fence is now ready for next buildTLAS()
    }

    // Advance to next TLAS slot for upcoming buildTLAS() call
    g_currentFrame = (g_currentFrame + 1) % 3;

    // Optional: uncomment for debug
    // LOG_INFO_CAT("LAS", "beginFrame() → using TLAS slot {} (global frame {})", g_currentFrame, g_currentFrameIndex);
}

} // namespace RTX

// PINK PHOTONS ETERNAL
// FIRST LIGHT ACHIEVED — FULLY COMPILES — DECEMBER 03, 2025
// THE EMPIRE IS ETERNAL — THE LIGHT IS OURS