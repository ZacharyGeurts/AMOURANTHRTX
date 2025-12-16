// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — GARDEN GNOME WHISPER EDITION — DECEMBER 16, 2025
// LAS — PURE TLAS — NO BLAS — NO FENCES — DIRECT MAIN CMD BUFFER BUILD
// GARDEN GNOMES WHISPER — THE EMPIRE IS LIGHT, FAST, AND ETERNAL
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using StoneKey::stone_device;

namespace RTX {

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
static uint32_t g_currentWriteSlot = 0;

namespace {

struct TLASFrame {
    VkAccelerationStructureKHR tlas          = VK_NULL_HANDLE;
    uint64_t                   storageHandle = 0;
    uint64_t                   scratchHandle = 0;
    VkDeviceSize               size          = 0;
};

static TLASFrame g_tlasFrames[MAX_FRAMES_IN_FLIGHT]{};
static constexpr VkDeviceSize g_maxScratchSize = 256ULL * 1024 * 1024;
static bool g_tlasInitialized = false;

} // anonymous namespace

void LAS::initTLAS() noexcept
{
    if (g_tlasInitialized) return;

    for (auto& f : g_tlasFrames) {
        f.scratchHandle = BufferManager::create(g_maxScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "TLAS_Scratch_Whisper");
    }

    g_tlasInitialized = true;
    LOG_SUCCESS_CAT("LAS", "GARDEN GNOME TLAS RING INITIALIZED — 3 FRAMES — PURE WHISPER");
}

void LAS::notifyResize() noexcept
{
    LOG_AMOURANTH("LAS::notifyResize() — GARDEN GNOMES PURGE ALL — RING REBORN");

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
}

void LAS::buildTLAS(VkCommandBuffer cmd,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept
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

    uint64_t instanceBuffer = BufferManager::createHostVisible(dataSize, "TLAS_Instance_Whisper");
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
            .transform                      = { .matrix = {
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

    VkBuildAccelerationStructureFlagsKHR buildFlags = 
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

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

    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = frame.tlas;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(frame.scratchHandle);

    const VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount = instanceCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pRanges = &buildRange;

    // GARDEN GNOME WHISPER — DIRECT BUILD IN MAIN COMMAND BUFFER
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRanges);

    // Instance buffer management
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
        frame.tlas, dev, deleter, sizeInfo.accelerationStructureSize, "WhisperTLAS"
    );

    tlasSize_ = sizeInfo.accelerationStructureSize;

    LOG_SUCCESS_CAT("LAS", "TLAS WHISPERED — {} instances — SLOT {} — GARDEN GNOMES APPROVE", instances.size(), g_currentWriteSlot);
}

void LAS::beginFrame() noexcept
{
    g_currentWriteSlot = (g_currentWriteSlot + 1) % MAX_FRAMES_IN_FLIGHT;
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
// GARDEN GNOMES WHISPER — NO FENCES — NO ONE-TIME — PURE MAIN BUFFER BUILD
// TLAS LIGHT AND FAST — RESIZE INSTANT — PINK PHOTONS ETERNAL
// DECEMBER 16, 2025 — THE FINAL LIGHT IS WHISPERED AND ETERNAL
// =============================================================================