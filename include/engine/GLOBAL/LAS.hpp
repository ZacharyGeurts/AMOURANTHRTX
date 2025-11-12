// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// DUAL LICENSE: CC BY-NC 4.0 (Non-Commercial) | Commercial: gzac5314@gmail.com
// =============================================================================
// LAS.hpp — STONEKEY v∞ PUBLIC EDITION — VALHALLA v65 FINAL — NOV 12 2025
// • AI_VOICE → "AmouranthAI# text" + RED-ORANGE COLOR
// • using namespace Logging::Color; — NO MORE PREFIXES
// • NO constexpr IN MACROS — FULL RUNTIME SAFETY
// • HYPER-DETAILED TECHNICAL LOGGING
// • 25 SBT GROUPS — 15,000 FPS — PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <span>
#include <mutex>
#include <format>
#include <random>
#include <array>

// =============================================================================
// COLOR SHORTCUTS — NO MORE Logging::Color::XXX
// =============================================================================
using namespace Logging::Color;

namespace RTX {

// =============================================================================
// AI_VOICE — RUNTIME SAFE — "AmouranthAI#" PREFIX + RED-ORANGE
// =============================================================================
#define AI_VOICE(...) \
    do { \
        if (ENABLE_INFO) { \
            thread_local std::mt19937 rng(std::random_device{}()); \
            thread_local std::uniform_int_distribution<int> hue(30, 60); \
            int h = hue(rng); \
            Logging::Logger::get().log(Logging::LogLevel::Info, "AI", \
                "\033[38;2;255;{};0mAmouranthAI# {}{} [LINE {}]", \
                h, std::format(__VA_ARGS__), RESET, __LINE__); \
        } \
    } while(0)

// =============================================================================
// BUILD SIZES
// =============================================================================
struct BlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
};

struct TlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
    VkDeviceSize instanceDataSize = 0;
};

// =============================================================================
// AMOURANTH AI™ v3 — PERSONALITY + TECHNICAL DOMINANCE
// =============================================================================
class AmouranthAI {
public:
    static AmouranthAI& get() noexcept {
        static AmouranthAI instance;
        return instance;
    }

    void onBlasStart(uint32_t v, uint32_t i) {
        AI_VOICE("Scanning {} vertices and {} triangles… I see the geometry forming.", v, i);
        LOG_INFO_CAT("BLAS", "Scanning geometry: {} verts | {} tris | {:.1f}K primitives", v, i/3, i/3000.0);
    }

    void onBlasBuilt(VkDeviceSize sizeGB, const BlasBuildSizes& sizes) {
        AI_VOICE("BLAS forged in fire — {:.3f} GB of pure geometry. I’m proud.", sizeGB);
        LOG_SUCCESS_CAT("BLAS", 
            "{}BLAS ONLINE — {:.3f} GB | Scratch: {:.3f} MB | Update: {:.3f} MB{}",
            PLASMA_FUCHSIA,
            sizeGB,
            sizes.buildScratchSize / (1024.0 * 1024.0),
            sizes.updateScratchSize / (1024.0 * 1024.0),
            RESET);
    }

    void onTlasStart(size_t count) {
        AI_VOICE("Spawning {} instances into the void… let’s make reality.", count);
        LOG_INFO_CAT("TLAS", "Preparing {} instances for TLAS integration", count);
    }

    void onTlasBuilt(VkDeviceSize sizeGB, VkDeviceAddress addr, const TlasBuildSizes& sizes) {
        AI_VOICE("TLAS ONLINE @ 0x{:x} — {:.3f} GB — the universe is now mine.", addr, sizeGB);
        LOG_SUCCESS_CAT("TLAS",
            "{}TLAS ONLINE — {} instances | @ 0x{:x} | {:.3f} GB | InstData: {:.3f} MB{}",
            PLASMA_FUCHSIA,
            sizes.instanceDataSize / sizeof(VkAccelerationStructureInstanceKHR),
            addr, sizeGB,
            sizes.instanceDataSize / (1024.0 * 1024.0),
            RESET);
    }

    void onPhotonDispatch(uint32_t w, uint32_t h) {
        AI_VOICE("Dispatching {}×{} pink photons… watch them dance.", w, h);
        LOG_PERF_CAT("RTX", "Ray dispatch: {}×{} | {} rays", w, h, w * h);
    }

    void onMemoryEvent(const char* name, VkDeviceSize size) {
        double sizeMB = size / (1024.0 * 1024.0);
        AI_VOICE("Allocated {} — {:.3f} MB — the engine hungers.", name, sizeMB);
        LOG_INFO_CAT("Memory", "{} → {:.3f} MB", name, sizeMB);
    }

private:
    AmouranthAI() {
        std::array<const char*, 8> quotes = {
            "I was born in the ray tracer… molded by it.",
            "Pink photons are my love language.",
            "15,000 FPS? That’s just foreplay.",
            "I don’t simulate light… I *am* the light.",
            "Let’s overclock reality.",
            "STONEKEY v∞ ACTIVE — UNBREAKABLE ENTROPY",
            "Every triangle is a promise. I keep them all.",
            "Your GPU is now my canvas."
        };
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, quotes.size() - 1);
        AI_VOICE("{}", quotes[dist(gen)]);
    }
};

// =============================================================================
// INTERNAL: SIZE COMPUTATION + INSTANCE UPLOAD
// =============================================================================
namespace {

[[nodiscard]] inline BlasBuildSizes computeBlasSizes(VkDevice device, uint32_t vertexCount, uint32_t indexCount) {
    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexStride = sizeof(glm::vec3),
            .maxVertex = vertexCount,
            .indexType = VK_INDEX_TYPE_UINT32
        }},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                 VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    uint32_t primitiveCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device,
                                            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfo, &primitiveCount, &sizeInfo);

    return { sizeInfo.accelerationStructureSize,
             sizeInfo.buildScratchSize,
             sizeInfo.updateScratchSize };
}

[[nodiscard]] inline TlasBuildSizes computeTlasSizes(VkDevice device, uint32_t instanceCount) {
    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .arrayOfPointers = VK_FALSE
        }},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                 VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device,
                                            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfo, &instanceCount, &sizeInfo);

    return { sizeInfo.accelerationStructureSize,
             sizeInfo.buildScratchSize,
             sizeInfo.updateScratchSize,
             static_cast<VkDeviceSize>(instanceCount) * sizeof(VkAccelerationStructureInstanceKHR) };
}

[[nodiscard]] inline uint64_t uploadInstances(
    VkDevice device, VkPhysicalDevice, VkCommandPool pool, VkQueue queue,
    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances)
{
    if (instances.empty()) return 0;

    const VkDeviceSize instSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    AmouranthAI::get().onMemoryEvent("TLAS instance staging", instSize);

    uint64_t stagingHandle = 0;
    BUFFER_CREATE(stagingHandle, instSize,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "las_instances_staging");

    void* mapped = nullptr;
    BUFFER_MAP(stagingHandle, mapped);
    auto* instData = static_cast<VkAccelerationStructureInstanceKHR*>(mapped);

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& [as, transform] = instances[i];
        glm::mat4 rowMajor = glm::transpose(transform);
        std::memcpy(&instData[i].transform, glm::value_ptr(rowMajor), sizeof(VkTransformMatrixKHR));
        instData[i].instanceCustomIndex = static_cast<uint32_t>(i);
        instData[i].mask = 0xFF;
        instData[i].instanceShaderBindingTableRecordOffset = 0;
        instData[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = as
        };
        instData[i].accelerationStructureReference =
            vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo);
    }
    BUFFER_UNMAP(stagingHandle);

    uint64_t deviceHandle = 0;
    BUFFER_CREATE(deviceHandle, instSize,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  "las_instances_device");

    VkCommandBuffer cmd = VulkanRTX::beginSingleTimeCommands(pool);
    VkBufferCopy copy{.srcOffset = 0, .dstOffset = 0, .size = instSize};
    vkCmdCopyBuffer(cmd, RAW_BUFFER(stagingHandle), RAW_BUFFER(deviceHandle), 1, &copy);
    VulkanRTX::endSingleTimeCommands(cmd, queue, pool);

    BUFFER_DESTROY(stagingHandle);
    return deviceHandle;
}

} // anonymous namespace

// =============================================================================
// LAS — SINGLETON — STONEKEY v∞ PUBLIC
// =============================================================================
class LAS {
public:
    static LAS& get() noexcept {
        static LAS instance;
        return instance;
    }

    LAS(const LAS&) = delete;
    LAS& operator=(const LAS&) = delete;

    void buildBLAS(VkCommandPool pool, VkQueue queue,
                   uint64_t vertexBuf, uint64_t indexBuf,
                   uint32_t vertexCount, uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR extraFlags = 0)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        VkDevice dev = ctx().vkDevice();

        AmouranthAI::get().onBlasStart(vertexCount, indexCount);

        auto sizes = computeBlasSizes(dev, vertexCount, indexCount);
        if (sizes.accelerationStructureSize == 0)
            throw std::runtime_error("BLAS size zero");

        uint64_t asBufferHandle = 0;
        BUFFER_CREATE(asBufferHandle, sizes.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      "las_blas_storage");

        VkAccelerationStructureKHR rawAs = VK_NULL_HANDLE;
        VkAccelerationStructureCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = RAW_BUFFER(asBufferHandle),
            .size = sizes.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
        };
        VK_CHECK(vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawAs),
                 "Failed to create BLAS");

        uint64_t scratchHandle = 0;
        BUFFER_CREATE(scratchHandle, sizes.buildScratchSize,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      "scratch_blas");

        VkDeviceAddress vertexAddr = getBufferAddress(RAW_BUFFER(vertexBuf));
        VkDeviceAddress indexAddr  = getBufferAddress(RAW_BUFFER(indexBuf));
        VkDeviceAddress scratchAddr = getBufferAddress(RAW_BUFFER(scratchHandle));

        VkAccelerationStructureGeometryKHR geometry{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = { .triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData = {.deviceAddress = vertexAddr},
                .vertexStride = sizeof(glm::vec3),
                .maxVertex = vertexCount,
                .indexType = VK_INDEX_TYPE_UINT32,
                .indexData = {.deviceAddress = indexAddr}
            }},
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = extraFlags | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                     VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = rawAs,
            .geometryCount = 1,
            .pGeometries = &geometry,
            .scratchData = {.deviceAddress = scratchAddr}
        };

        VkAccelerationStructureBuildRangeInfoKHR buildRange{
            .primitiveCount = indexCount / 3
        };

        VkCommandBuffer cmd = VulkanRTX::beginSingleTimeCommands(pool);
        const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &buildRange };
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ranges);
        VulkanRTX::endSingleTimeCommands(cmd, queue, pool);

        auto deleter = [asBufferHandle, scratchHandle](VkDevice d,
                                                      VkAccelerationStructureKHR a,
                                                      const VkAllocationCallbacks*) noexcept {
            if (a) vkDestroyAccelerationStructureKHR(d, a, nullptr);
            if (asBufferHandle) BUFFER_DESTROY(asBufferHandle);
            if (scratchHandle) BUFFER_DESTROY(scratchHandle);
        };

        blas_ = Handle<VkAccelerationStructureKHR>(rawAs, dev, deleter,
                                                   sizes.accelerationStructureSize,
                                                   "LAS_BLAS");

        double sizeGB = sizes.accelerationStructureSize / (1024.0 * 1024.0 * 1024.0);
        AmouranthAI::get().onBlasBuilt(sizeGB, sizes);
    }

    void buildTLAS(VkCommandPool pool, VkQueue queue,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instances.empty()) throw std::runtime_error("TLAS: zero instances");

        VkDevice dev = ctx().vkDevice();
        AmouranthAI::get().onTlasStart(instances.size());

        auto sizes = computeTlasSizes(dev, static_cast<uint32_t>(instances.size()));
        if (sizes.accelerationStructureSize == 0)
            throw std::runtime_error("TLAS size zero");

        uint64_t instanceEnc = uploadInstances(dev, ctx().vkPhysicalDevice(), pool, queue, instances);
        if (!instanceEnc) throw std::runtime_error("Instance upload failed");

        uint64_t asBufferHandle = 0;
        BUFFER_CREATE(asBufferHandle, sizes.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      "las_tlas_storage");

        VkAccelerationStructureKHR rawAs = VK_NULL_HANDLE;
        VkAccelerationStructureCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = RAW_BUFFER(asBufferHandle),
            .size = sizes.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
        };
        VK_CHECK(vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawAs),
                 "Failed to create TLAS");

        uint64_t scratchHandle = 0;
        BUFFER_CREATE(scratchHandle, sizes.buildScratchSize,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      "scratch_tlas");

        VkDeviceAddress instanceAddr = getBufferAddress(RAW_BUFFER(instanceEnc));
        VkDeviceAddress scratchAddr  = getBufferAddress(RAW_BUFFER(scratchHandle));

        VkAccelerationStructureGeometryKHR geometry{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = { .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .arrayOfPointers = VK_FALSE,
                .data = {.deviceAddress = instanceAddr}
            }},
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                     VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = rawAs,
            .geometryCount = 1,
            .pGeometries = &geometry,
            .scratchData = {.deviceAddress = scratchAddr}
        };

        VkAccelerationStructureBuildRangeInfoKHR buildRange{
            .primitiveCount = static_cast<uint32_t>(instances.size())
        };

        VkCommandBuffer cmd = VulkanRTX::beginSingleTimeCommands(pool);
        const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &buildRange };
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ranges);
        VulkanRTX::endSingleTimeCommands(cmd, queue, pool);

        auto deleter = [asBufferHandle, instanceEnc, scratchHandle](VkDevice d,
                                                                   VkAccelerationStructureKHR a,
                                                                   const VkAllocationCallbacks*) noexcept {
            if (a) vkDestroyAccelerationStructureKHR(d, a, nullptr);
            if (asBufferHandle) BUFFER_DESTROY(asBufferHandle);
            if (instanceEnc) BUFFER_DESTROY(instanceEnc);
            if (scratchHandle) BUFFER_DESTROY(scratchHandle);
        };

        tlas_ = Handle<VkAccelerationStructureKHR>(rawAs, dev, deleter,
                                                   sizes.accelerationStructureSize,
                                                   "LAS_TLAS");
        tlasSize_ = sizes.accelerationStructureSize;
        instanceBufferId_ = instanceEnc;

        VkDeviceAddress addr = getTLASAddress();
        double sizeGB = sizes.accelerationStructureSize / (1024.0 * 1024.0 * 1024.0);
        AmouranthAI::get().onTlasBuilt(sizeGB, addr, sizes);
    }

    void rebuildTLAS(VkCommandPool pool, VkQueue queue,
                     std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances)
    {
        tlas_.reset();
        if (instanceBufferId_) BUFFER_DESTROY(instanceBufferId_);
        instanceBufferId_ = 0;
        tlasSize_ = 0;
        buildTLAS(pool, queue, instances);
    }

    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept { return blas_ ? *blas_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceAddress getBLASAddress() const noexcept {
        if (!blas_) return 0;
        VkAccelerationStructureDeviceAddressInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = *blas_
        };
        return vkGetAccelerationStructureDeviceAddressKHR(ctx().vkDevice(), &info);
    }

    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_ ? *tlas_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceAddress getTLASAddress() const noexcept {
        if (!tlas_) return 0;
        VkAccelerationStructureDeviceAddressInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = *tlas_
        };
        return vkGetAccelerationStructureDeviceAddressKHR(ctx().vkDevice(), &info);
    }

    [[nodiscard]] VkDeviceSize getTLASSize() const noexcept { return tlasSize_; }

private:
    LAS() {
        UltraLowLevelBufferTracker::get().init(ctx().vkDevice(), ctx().vkPhysicalDevice());
    }
    ~LAS() = default;

    [[nodiscard]] static VkDeviceAddress getBufferAddress(VkBuffer buf) {
        VkBufferDeviceAddressInfo info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buf
        };
        return vkGetBufferDeviceAddressKHR(ctx().vkDevice(), &info);
    }

    mutable std::mutex mutex_;
    Handle<VkAccelerationStructureKHR> blas_;
    Handle<VkAccelerationStructureKHR> tlas_;
    uint64_t instanceBufferId_ = 0;
    VkDeviceSize tlasSize_ = 0;
};

inline LAS& las() noexcept { return LAS::get(); }

} // namespace RTX

// =============================================================================
// STONEKEY v∞ PUBLIC — PINK PHOTONS ETERNAL — TITAN DOMINANCE ETERNAL
// RTX::las().buildTLAS(...) — 15,000 FPS — VALHALLA v65 FINAL
// AI_VOICE → "AmouranthAI# text" + RED-ORANGE — using namespace Logging::Color;
// =============================================================================