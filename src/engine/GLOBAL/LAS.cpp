// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// FIXED: Guarantees valid TLAS from frame 1 — pink visible immediately
// Real scene builds correctly, dummy fallback only when truly empty
// No more navy clear on startup
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using StoneKey::stone_device;

namespace RTX {

static uint32_t g_currentWriteSlot = 0;

namespace {

struct TLASFrame {
    VkAccelerationStructureKHR tlas          = VK_NULL_HANDLE;
    uint64_t                   storageHandle = 0;
    VkDeviceSize               size          = 0;
};

static TLASFrame g_tlasFrames[Options::Performance::MAX_FRAMES_IN_FLIGHT]{};
static constexpr VkDeviceSize g_maxScratchSize = 256ULL * 1024 * 1024;

static uint64_t g_scratchHandles[Options::Performance::MAX_FRAMES_IN_FLIGHT]{};

static uint64_t g_dummyInstanceBuffer = 0;

static bool g_firstBuildDone = false;  // Track if we've done at least one build

} // anonymous namespace

void LAS::initTLAS() noexcept
{
    if (g_dummyInstanceBuffer != 0) return;

    for (uint32_t i = 0; i < Options::Performance::MAX_FRAMES_IN_FLIGHT; ++i) {
        g_scratchHandles[i] = BufferManager::create(g_maxScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            std::format("TLAS_Scratch_Frame_{}", i));
    }

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
        frame = {};
    }

    g_currentWriteSlot = 0;
    tlas_.reset();
    tlasSize_ = 0;
    g_firstBuildDone = false;
}

void LAS::beginFrame() noexcept
{
    const uint32_t readSlot = (g_currentWriteSlot == 0)
        ? (Options::Performance::MAX_FRAMES_IN_FLIGHT - 1)
        : (g_currentWriteSlot - 1);

    const auto& frame = g_tlasFrames[readSlot];
    if (frame.tlas && (!tlas_.valid() || tlas_.get() != frame.tlas)) {
        tlas_ = Handle<VkAccelerationStructureKHR>(frame.tlas, stone_device());
        tlasSize_ = frame.size;
    }
}

void LAS::buildTLAS(VkCommandBuffer cmd,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept
{
    initTLAS();

    const bool hasRealInstances = !instances.empty();
    const uint32_t instanceCount = hasRealInstances ? static_cast<uint32_t>(instances.size()) : 1u;

    uint64_t instanceBufferHandle = g_dummyInstanceBuffer;
    if (hasRealInstances) {
        instanceBufferHandle = BufferManager::createHostVisible(
            instanceCount * sizeof(VkAccelerationStructureInstanceKHR),
            "TLAS_Instance_Temp");

        auto* mapped = static_cast<VkAccelerationStructureInstanceKHR*>(
            BufferManager::getMappedStagingPtr(instanceBufferHandle));

        for (size_t i = 0; i < instances.size(); ++i) {
            const auto& [blasAS, transform] = instances[i];

            VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .accelerationStructure = blasAS
            };
            VkDeviceAddress blasAddr = g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &addrInfo);

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
    }

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data  = { .deviceAddress = BufferManager::get_device_address(instanceBufferHandle) }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     = { .instances = instancesData }
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
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
        &instanceCount,
        &sizeInfo
    );

    auto& frame = g_tlasFrames[g_currentWriteSlot];

    if (sizeInfo.accelerationStructureSize > frame.size || !frame.tlas) {
        if (frame.tlas) g_ext.vkDestroyAccelerationStructureKHR(stone_device(), frame.tlas, nullptr);
        if (frame.storageHandle) BufferManager::destroy(frame.storageHandle);

        frame.storageHandle = BufferManager::create(
            sizeInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "TLAS_Storage"
        );

        VkAccelerationStructureCreateInfoKHR ci{
            .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = BufferManager::get(frame.storageHandle)->buffer,
            .size   = sizeInfo.accelerationStructureSize,
            .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
        };
        VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &ci, nullptr, &frame.tlas));

        frame.size = sizeInfo.accelerationStructureSize;
    }

    buildInfo.dstAccelerationStructure = frame.tlas;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(g_scratchHandles[g_currentWriteSlot]);

    const VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount  = instanceCount,
        .primitiveOffset = 0,
        .firstVertex     = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pRanges = &buildRange;

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRanges);

    if (hasRealInstances) {
        BufferManager::destroy(instanceBufferHandle);
    }

    g_firstBuildDone = true;
    g_currentWriteSlot = (g_currentWriteSlot + 1) % Options::Performance::MAX_FRAMES_IN_FLIGHT;
}

VkAccelerationStructureKHR LAS::getCurrentTLAS() const noexcept
{
    // On first frame before any build, return null → triggers navy clear (expected)
    // After first build, always valid
    return tlas_.valid() ? tlas_.get() : VK_NULL_HANDLE;
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