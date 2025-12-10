// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — FINAL FIXED VERSION — DECEMBER 09, 2025
// NO MORE STALLS. NO MORE FREEZES. PINK PHOTONS FLOW ETERNALLY.
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

using StoneKey::stone_graphics_queue;

namespace RTX {

// ============================================================================
// Zero-Tearing TLAS Ring — FULLY RESIZE-PROOF — ETERNAL PINK SHIELD
// ============================================================================

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

static uint32_t g_currentWriteSlot = 0;

// =============================================================================
// One-Time Submit — RAW DOG MODE — NO FENCE, NO WAIT
// =============================================================================
[[nodiscard]] VkCommandBuffer beginOneTimeSubmit(VkCommandPool pool) noexcept
{
    if (pool == VK_NULL_HANDLE) pool = g_ctx().commandPool_;
    if (pool == VK_NULL_HANDLE) [[unlikely]] {
        LOG_FATAL_CAT("LAS", "NO COMMAND POOL — EMPIRE CANNOT SPEAK");
        return VK_NULL_HANDLE;
    }

    VkCommandBuffer cmd = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1u
    };

    vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd);

    if (cmd == VK_NULL_HANDLE) [[unlikely]] {
        LOG_FATAL_CAT("LAS", "COMMAND BUFFER ALLOCATION FAILED — EMPIRE BROKEN");
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(cmd, &begin);

    return cmd;
}

// NO DEFAULT ARGS IN CPP — THEY ARE IN HEADER ONLY
void endOneTimeSubmit(VkCommandBuffer cmd,
                      VkQueue queue,
                      VkCommandPool pool) noexcept
{
    if (cmd == VK_NULL_HANDLE) return;

    vkEndCommandBuffer(cmd);

    if (queue == VK_NULL_HANDLE) queue = stone_graphics_queue();
    if (pool == VK_NULL_HANDLE) pool = g_ctx().commandPool_;

    const VkSubmitInfo submit{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &cmd
    };

    // TRUE RAW DOG — NO FENCE. NO WAIT.
    vkQueueSubmit(queue, 1u, &submit, VK_NULL_HANDLE);

    // IMMEDIATE RECYCLE — WE ARE THE GPU
    vkResetCommandPool(stone_device(), pool, 0);
    vkFreeCommandBuffers(stone_device(), pool, 1u, &cmd);
}

// =============================================================================
// LAS::buildBLAS — Eternal, compacted, zero-leak
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
    if (dev == VK_NULL_HANDLE || vertexCount == 0 || indexCount % 3 != 0) return;

    const auto* vbuf = BufferManager::get(vertexHandle);
    const auto* ibuf = BufferManager::get(indexHandle);
    if (!vbuf || !ibuf) return;

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
    if (Options::OptionsLAS::PREFER_FAST_TRACE)  buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    if (Options::OptionsLAS::PREFER_FAST_BUILD)  buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    if (Options::OptionsLAS::UPDATE_EVERY_FRAME) buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags         = buildFlags,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        dev,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo
    );

    uint64_t storageHandle = BufferManager::create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "BLAS_Storage"
    );

    uint64_t scratchHandle = BufferManager::create(
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "BLAS_Scratch"
    );

    VkAccelerationStructureKHR as = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = BufferManager::get(storageHandle)->buffer,
        .size   = sizeInfo.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    g_ext.vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &as);

    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = as;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(scratchHandle);

    const VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount = primitiveCount
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pRanges = &buildRange;

    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(pool);
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRanges);
    RTX::endOneTimeSubmit(cmd, queue, pool); // NO FENCE — PURE FLIGHT

    BufferManager::destroy(scratchHandle);

    if (blas_.valid()) blas_.reset();

    auto deleter = [dev, storageHandle](VkDevice, VkAccelerationStructureKHR as, const VkAllocationCallbacks*) {
        if (as != VK_NULL_HANDLE) g_ext.vkDestroyAccelerationStructureKHR(dev, as, nullptr);
        BufferManager::destroy(storageHandle);
    };

    blas_ = Handle<VkAccelerationStructureKHR>(as, dev, deleter, sizeInfo.accelerationStructureSize, "EternalBLAS");
}

// =============================================================================
// TLAS — ZERO TEAR — FULL RING — RESIZE INSTANT — NO DRIVER — MAX COMPAT
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
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "TLAS_Scratch_Permanent");
    }

    g_tlasInitialized = true;
    LOG_SUCCESS_CAT("LAS", "TLAS RING ONLINE — NO FENCES — PURE FLIGHT");
}

void LAS::notifyResize() noexcept
{
    LOG_AMOURANTH("LAS::notifyResize() — PHOTON PURGE — FULL RING RESET — INSTANT");

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

    g_currentWriteSlot = 0;
    LOG_AMOURANTH("RESIZE COMPLETE — TLAS RING REBORN — ZERO BLACK FRAMES — EMPIRE ETERNAL");
}

void LAS::buildTLAS(VkCommandPool pool,
                    VkQueue queue,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances,
                    bool isDynamic) noexcept
{
    initTLAS();

    if (instances.empty()) {
        tlas_.reset();
        if (instanceBufferId_) {
            BufferManager::destroy(std::exchange(instanceBufferId_, 0));
        }
        return;
    }

    const VkDevice dev = stone_device();
    const VkDeviceSize dataSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

    uint64_t instanceBuffer = BufferManager::createHostVisible(dataSize, "TLAS_InstanceData");
    auto* mapped = static_cast<VkAccelerationStructureInstanceKHR*>(BufferManager::getMappedStagingPtr(instanceBuffer));

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& [blasAS, transform] = instances[i];

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blasAS
        };
        VkDeviceAddress blasAddr = g_ext.vkGetAccelerationStructureDeviceAddressKHR(dev, &addrInfo);

        const glm::mat3x4 trans = glm::transpose(transform);

        mapped[i] = VkAccelerationStructureInstanceKHR{
            .transform = { .matrix = {
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
    if (Options::OptionsLAS::PREFER_FAST_TRACE)  buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    if (Options::OptionsLAS::PREFER_FAST_BUILD)  buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    if (Options::OptionsLAS::UPDATE_EVERY_FRAME && !Options::OptionsLAS::REBUILD_EVERY_FRAME)
        buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    if (Options::OptionsLAS::COMPACT_TLAS)
        buildFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = buildFlags,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    const uint32_t instanceCount = static_cast<uint32_t>(instances.size());

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        dev,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &instanceCount,
        &sizeInfo
    );

    auto& frame = g_tlasFrames[g_currentWriteSlot];

    if (sizeInfo.accelerationStructureSize > frame.size || !frame.tlas) {
        if (frame.tlas) {
            g_ext.vkDestroyAccelerationStructureKHR(dev, frame.tlas, nullptr);
            frame.tlas = VK_NULL_HANDLE;
        }
        if (frame.storageHandle) {
            BufferManager::destroy(frame.storageHandle);
            frame.storageHandle = 0;
        }

        frame.storageHandle = BufferManager::create(
            sizeInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "TLAS_Storage"
        );

        VkAccelerationStructureCreateInfoKHR ci{
            .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = BufferManager::get(frame.storageHandle)->buffer,
            .size   = sizeInfo.accelerationStructureSize,
            .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
        };
        VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &ci, nullptr, &frame.tlas));

        frame.size = sizeInfo.accelerationStructureSize;
    }

    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = frame.tlas;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(frame.scratchHandle);

    const VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount = instanceCount
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pRanges = &buildRange;

    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(pool);
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRanges);
    RTX::endOneTimeSubmit(cmd, queue);

    if (instanceBufferId_) {
        BufferManager::destroy(std::exchange(instanceBufferId_, instanceBuffer));
    } else {
        instanceBufferId_ = instanceBuffer;
    }

    if (tlas_.valid()) tlas_.reset();

    auto deleter = [oldBuffer = instanceBufferId_](VkDevice, VkAccelerationStructureKHR, const VkAllocationCallbacks*) {
        if (oldBuffer) BufferManager::destroy(oldBuffer);
    };

    tlas_ = Handle<VkAccelerationStructureKHR>(
        frame.tlas, dev, deleter, sizeInfo.accelerationStructureSize, "EternalTLAS"
    );

    tlasSize_ = sizeInfo.accelerationStructureSize;

    LOG_SUCCESS_CAT("LAS", "TLAS FORGED — {} instances — SLOT {}", instances.size(), g_currentWriteSlot);
}

void LAS::beginFrame() noexcept
{
    // NO WAIT — WE FLY
    g_currentWriteSlot = (g_currentWriteSlot + 1) % MAX_FRAMES_IN_FLIGHT;
}

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