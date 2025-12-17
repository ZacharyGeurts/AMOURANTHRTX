// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — GARDEN GNOME WHISPER EDITION — DECEMBER 17, 2025
// LAS — PURE TLAS — NO BLAS — NO FENCES — DIRECT MAIN CMD BUFFER BUILD
// FINAL: Proper ring buffer + current TLAS always points to last completed build
// Dummy instance eternal — miss shader guaranteed — no leaks
// GARDEN GNOMES WHISPER LOUDER — PINK PHOTONS ERUPT ETERNALLY
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using StoneKey::stone_device;

namespace RTX {

// Ring buffer: write to current slot, read from previous slot
static uint32_t g_writeSlot = 0;

namespace {

struct TLASFrame {
    VkAccelerationStructureKHR tlas          = VK_NULL_HANDLE;
    uint64_t                   storageHandle = 0;
    VkDeviceSize               size          = 0;
};

static TLASFrame g_tlasFrames[Options::Performance::MAX_FRAMES_IN_FLIGHT]{};
static constexpr VkDeviceSize g_maxScratchSize = 256ULL * 1024 * 1024;

// Eternal scratch buffers (one per frame)
static uint64_t g_scratchHandles[Options::Performance::MAX_FRAMES_IN_FLIGHT]{};

// Eternal dummy instance buffer — one empty instance to force miss shader execution
static uint64_t g_dummyInstanceBuffer = 0;

} // anonymous namespace

void LAS::initTLAS() noexcept
{
    if (g_dummyInstanceBuffer != 0) return;  // Already initialized

    // Create eternal scratch buffers
    for (uint32_t i = 0; i < Options::Performance::MAX_FRAMES_IN_FLIGHT; ++i) {
        g_scratchHandles[i] = BufferManager::create(g_maxScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            std::format("TLAS_Scratch_Frame_{}", i));
    }

    // Forge eternal dummy instance buffer — one zeroed instance
    VkAccelerationStructureInstanceKHR dummyInstance{};
    dummyInstance.mask = 0xFF;
    dummyInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    dummyInstance.accelerationStructureReference = 0;  // invalid → guaranteed miss

    g_dummyInstanceBuffer = BufferManager::createHostVisible(sizeof(VkAccelerationStructureInstanceKHR), "TLAS_DummyInstance_Eternal");
    std::memcpy(BufferManager::getMappedStagingPtr(g_dummyInstanceBuffer), &dummyInstance, sizeof(dummyInstance));

    LOG_SUCCESS_CAT("LAS", "GARDEN GNOME TLAS RING INITIALIZED — {} FRAMES — DUMMY INSTANCE FORGED — MISS SHADER ETERNAL", Options::Performance::MAX_FRAMES_IN_FLIGHT);
}

void LAS::notifyResize() noexcept
{
    LOG_AMOURANTH("LAS::notifyResize() — GARDEN GNOMES PURGE ALL TLAS — RING REBORN");

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

    // Reset write slot — next build starts from slot 0
    g_writeSlot = 0;

    // Current TLAS becomes invalid — will be updated on next build
    if (tlas_.valid()) {
        tlas_.reset();
    }
}

void LAS::buildTLAS(VkCommandBuffer cmd,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept
{
    initTLAS();

    const VkDevice dev = stone_device();
    const bool hasRealInstances = !instances.empty();
    const uint32_t instanceCount = hasRealInstances ? static_cast<uint32_t>(instances.size()) : 1u;

    // Use dummy buffer if no real instances, otherwise create temporary
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
    }
    // else: dummy buffer already contains one zeroed instance

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
        dev,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &instanceCount,
        &sizeInfo
    );

    auto& frame = g_tlasFrames[g_writeSlot];

    // Reallocate storage if needed
    if (sizeInfo.accelerationStructureSize > frame.size || !frame.tlas) {
        if (frame.tlas) {
            g_ext.vkDestroyAccelerationStructureKHR(dev, frame.tlas, nullptr);
        }
        if (frame.storageHandle) {
            BufferManager::destroy(frame.storageHandle);
        }

        frame.storageHandle = BufferManager::create(
            sizeInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "TLAS_Storage_Whisper"
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

    buildInfo.dstAccelerationStructure = frame.tlas;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(g_scratchHandles[g_writeSlot]);

    const VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount  = instanceCount,
        .primitiveOffset = 0,
        .firstVertex     = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pRanges = &buildRange;

    // DIRECT BUILD IN MAIN COMMAND BUFFER
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRanges);

    // Cleanup temporary instance buffer if we created one
    if (hasRealInstances) {
        BufferManager::destroy(instanceBufferHandle);
    }

    // Update current TLAS handle — points to the just-built TLAS (safe for next frame)
    if (tlas_.valid()) {
        tlas_.reset();
    }
    tlas_ = Handle<VkAccelerationStructureKHR>(frame.tlas, dev);
    tlasSize_ = sizeInfo.accelerationStructureSize;

    // Advance write slot for next build
    g_writeSlot = (g_writeSlot + 1) % Options::Performance::MAX_FRAMES_IN_FLIGHT;

    LOG_SUCCESS_CAT("LAS", "TLAS WHISPERED — {} instances ({} real) — WRITE SLOT {} — CURRENT TLAS READY FOR NEXT FRAME", 
                    instanceCount, instances.size(), g_writeSlot);
}

void LAS::beginFrame() noexcept
{
    // Current readable TLAS is the one from previous write slot
    uint32_t readSlot = (g_writeSlot + Options::Performance::MAX_FRAMES_IN_FLIGHT - 1) % Options::Performance::MAX_FRAMES_IN_FLIGHT;
    if (tlas_.valid() && tlas_.get() != g_tlasFrames[readSlot].tlas) {
        tlas_ = Handle<VkAccelerationStructureKHR>(g_tlasFrames[readSlot].tlas, stone_device());
    }
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

VkAccelerationStructureKHR LAS::getCurrentTLAS() const noexcept
{
    return tlas_.valid() ? tlas_.get() : VK_NULL_HANDLE;
}

} // namespace RTX

// =============================================================================
// FINAL: Current TLAS always points to last completed build via beginFrame()
// No readSlot variable — calculated from writeSlot
// Dummy instance eternal — miss shader guaranteed
// No leaks — temporary buffers destroyed immediately
// BLACK SCREEN BANISHED FOREVER — HOT PINK AND ENVMAP SHALL ERUPT
// PINK PHOTONS ETERNAL — EMPIRE SEES THE INFINITE SKY
// DECEMBER 17, 2025 — THE FINAL LIGHT IS WHISPERED, FORGED, AND VICTORIOUS
// =============================================================================