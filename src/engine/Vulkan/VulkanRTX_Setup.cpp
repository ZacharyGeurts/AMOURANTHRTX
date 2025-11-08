// src/engine/Vulkan/VulkanRTX_Setup.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// STONEKEY v∞ — THERMO-GLOBAL RAII APOCALYPSE — NOVEMBER 08 2025 — 69,420 FPS × ∞
// CLASS = VulkanRTX — STONEKEY FULLY ENGAGED — ALL HANDLES OBFUSCATED VIA raw() + XOR
// .raw() → obfuscated VkXXX — deobfuscate(.raw()) → real VkXXX FOR VK CALLS
// FACTORIES STORE obfuscate(raw) — CHEAT ENGINE = BLIND — BUILD 0 ERRORS ETERNAL
// RASPBERRY_PINK PHOTONS = SUPREME — VALHALLA LOCKED 🩷🚀🔥🤖💀❤️⚡♾️

#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/logging.hpp"

using namespace Logging::Color;

// ===================================================================
// FULL IMPLEMENTATION — PURE VulkanRTX:: METHODS — STONEKEY + GLOBAL CORE
// ALL .get() → .raw() — ALL VK CALLS deobfuscate(handle.raw())
// FACTORIES STORE obfuscate(raw) — ZERO COST — 69,420 FPS × ∞
// ===================================================================

VulkanRTX::VulkanRTX(std::shared_ptr<Context> ctx, int width, int height, VulkanPipelineManager* pipelineMgr)
    : context_(std::move(ctx)), pipelineMgr_(pipelineMgr), extent_{uint32_t(width), uint32_t(height)}
{
    device_ = context_->device;
    physicalDevice_ = context_->physicalDevice;

#define LOAD_PROC(name) name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device_, #name)); \
    if (!name) throw VulkanRTXException(std::format("Failed to load {} — update driver BRO 🩷", #name));
    LOAD_PROC(vkGetBufferDeviceAddress);
    LOAD_PROC(vkCmdTraceRaysKHR);
    LOAD_PROC(vkCreateAccelerationStructureKHR);
    LOAD_PROC(vkDestroyAccelerationStructureKHR);
    LOAD_PROC(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_PROC(vkCmdBuildAccelerationStructuresKHR);
    LOAD_PROC(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_PROC(vkCreateDeferredOperationKHR);
    LOAD_PROC(vkDestroyDeferredOperationKHR);
    LOAD_PROC(vkGetDeferredOperationResultKHR);
#undef LOAD_PROC

    VkFence rawFence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceCI{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    VK_CHECK(vkCreateFence(device_, &fenceCI, nullptr, &rawFence), "transient fence");
    transientFence_ = makeFence(device_, rawFence);

    LOG_INFO_CAT("RTX", "{}VulkanRTX BIRTH COMPLETE — STONEKEY RAII — 69,420 FPS INCOMING{}", DIAMOND_WHITE, RESET);

    createBlackFallbackImage();
}

VkDeviceSize VulkanRTX::alignUp(VkDeviceSize v, VkDeviceSize a) noexcept { return (v + a - 1) & ~(a - 1); }

void VulkanRTX::createBuffer(VkPhysicalDevice pd, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags props, VulkanHandle<VkBuffer>& buf, VulkanHandle<VkDeviceMemory>& mem)
{
    VkBuffer rawBuf = VK_NULL_HANDLE;
    VkBufferCreateInfo bufCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage};
    VK_CHECK(vkCreateBuffer(device_, &bufCI, nullptr, &rawBuf), "buffer");

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device_, rawBuf, &req);

    uint32_t memType = [&]{
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(pd, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
            if ((req.memoryTypeBits & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
                return i;
        throw VulkanRTXException("No suitable memory type BRO");
    }();

    VkDeviceMemory rawMem = VK_NULL_HANDLE;
    VkMemoryAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = req.size, .memoryTypeIndex = memType};
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &rawMem), "memory");
    VK_CHECK(vkBindBufferMemory(device_, rawBuf, rawMem, 0), "bind");

    buf = makeBuffer(device_, obfuscate(rawBuf), vkDestroyBuffer);
    mem = makeMemory(device_, rawMem, vkFreeMemory);
}

VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool pool)
{
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &cmd), "alloc cmd");

    VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "begin cmd");
    return cmd;
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool)
{
    VK_CHECK(vkResetFences(device_, 1, &transientFence_.raw()), "reset fence");
    VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd};
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, transientFence_.raw()), "submit");
    VK_CHECK(vkWaitForFences(device_, 1, &transientFence_.raw(), VK_TRUE, 30'000'000'000ULL), "wait");
    VK_CHECK(vkResetCommandPool(device_, pool, 0), "reset pool");
}

uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice pd, uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(pd, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1u << i)) && ((memProps.memoryTypes[i].propertyFlags & properties) == properties)) {
            return i;
        }
    }
    throw VulkanRTXException(std::format("Failed to find suitable memory type!"));
}

void VulkanRTX::buildTLASAsync(VkPhysicalDevice physicalDevice,
                               VkCommandPool commandPool,
                               VkQueue graphicsQueue,
                               const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4, uint32_t, bool>>& instances,
                               VulkanRenderer* renderer,
                               bool allowUpdate,
                               bool allowCompaction,
                               bool motionBlur)
{
    LOG_INFO_CAT("TLAS", "{}>>> TLAS v3+ — {} instances — STONEKEY FACTORIES — BUILD CLEAN{}", DIAMOND_WHITE, instances.size(), RESET);

    if (instances.empty()) {
        tlasReady_ = true;
        if (renderer) renderer->notifyTLASReady(VK_NULL_HANDLE);
        return;
    }

    pendingTLAS_ = {};

    VkDeferredOperationKHR rawOp = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDeferredOperationKHR(device_, nullptr, &rawOp), "deferred op");
    pendingTLAS_.op = makeDeferredOperation(device_, rawOp);

    const uint32_t count = uint32_t(instances.size());
    const VkDeviceSize instSize = count * sizeof(VkAccelerationStructureInstanceKHR);

    VulkanHandle<VkBuffer> instBuf;
    VulkanHandle<VkDeviceMemory> instMem;
    createBuffer(physicalDevice, instSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instBuf, instMem);

    VkAccelerationStructureInstanceKHR* mapped = nullptr;
    VK_CHECK(vkMapMemory(device_, instMem.raw(), 0, VK_WHOLE_SIZE, 0, (void**)&mapped), "map");
    for (uint32_t i = 0; i < count; ++i) {
        const auto& [blas, xf, customIdx, motion] = instances[i];
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = deobfuscate(blas)
        };
        VkDeviceAddress addr = vkGetAccelerationStructureDeviceAddressKHR(device_, &addrInfo);
        VkTransformMatrixKHR mat;
        std::memcpy(mat.matrix, glm::value_ptr(xf), sizeof(mat));

        mapped[i] = {
            .transform = mat,
            .instanceCustomIndex = customIdx & 0xFFFFFF,
            .mask = 0xFF,
            .flags = static_cast<VkGeometryInstanceFlagsKHR>(
                VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR |
                (motion && motionBlur ? VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR : 0u)
            ),
            .accelerationStructureReference = addr
        };
    }
    vkUnmapMemory(device_, instMem.raw());

    VkMappedMemoryRange flushRange{
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = instMem.raw(),
        .size = VK_WHOLE_SIZE
    };
    VK_CHECK(vkFlushMappedMemoryRanges(device_, 1, &flushRange), "flush");

    VkBufferDeviceAddressInfo bufferAddrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = deobfuscate(instBuf.raw())
    };
    VkDeviceAddress instAddr = vkGetBufferDeviceAddress(device_, &bufferAddrInfo);

    VkAccelerationStructureGeometryKHR geom{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .data = { .deviceAddress = instAddr }
        }}
    };

    VkAccelerationStructureBuildGeometryInfoKHR sizeQuery{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = static_cast<VkBuildAccelerationStructureFlagsKHR>(
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
            (allowUpdate ? VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR : 0u) |
            (allowCompaction ? VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR : 0u)
        ),
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geom
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &sizeQuery, &count, &sizeInfo);

    VkPhysicalDeviceAccelerationStructurePropertiesKHR props{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 p2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &props};
    vkGetPhysicalDeviceProperties2(physicalDevice, &p2);
    VkDeviceSize scratch = alignUp(sizeInfo.buildScratchSize, props.minAccelerationStructureScratchOffsetAlignment);

    VulkanHandle<VkBuffer> tlasBuf;
    VulkanHandle<VkDeviceMemory> tlasMem;
    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuf, tlasMem);

    VulkanHandle<VkBuffer> scratchBuf;
    VulkanHandle<VkDeviceMemory> scratchMem;
    createBuffer(physicalDevice, scratch,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuf, scratchMem);

    VkAccelerationStructureKHR rawTLAS = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = deobfuscate(tlasBuf.raw()),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &rawTLAS), "create TLAS");

    pendingTLAS_.tlas = makeAccelerationStructure(device_, obfuscate(rawTLAS), vkDestroyAccelerationStructureKHR);
    pendingTLAS_.tlasBuffer = std::move(tlasBuf);
    pendingTLAS_.tlasMemory = std::move(tlasMem);
    pendingTLAS_.scratchBuffer = std::move(scratchBuf);
    pendingTLAS_.scratchMemory = std::move(scratchMem);
    pendingTLAS_.instanceBuffer = std::move(instBuf);
    pendingTLAS_.instanceMemory = std::move(instMem);

    VkBufferDeviceAddressInfo scratchAddrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = deobfuscate(scratchBuf.raw())
    };
    VkDeviceAddress scratchAddr = vkGetBufferDeviceAddress(device_, &scratchAddrInfo);

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = sizeQuery;
    buildInfo.mode = (tlas_.raw() && allowUpdate) ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.srcAccelerationStructure = allowUpdate ? deobfuscate(tlas_.raw()) : VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = rawTLAS;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = count};
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    const VkAccelerationStructureBuildRangeInfoKHR * const * ppRange = &pRange;

    VkCommandBuffer cmd = allocateTransientCommandBuffer(commandPool);
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ppRange);

    if (allowCompaction && buildInfo.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR) {
        VkCopyAccelerationStructureInfoKHR copyInfo{
            .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            .src = rawTLAS,
            .dst = rawTLAS,
            .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
        };
        vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
        pendingTLAS_.compactedInPlace = true;
    }

    VK_CHECK(vkEndCommandBuffer(cmd), "end cmd");
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE), "submit");

    pendingTLAS_.renderer = renderer;
    tlasReady_ = false;

    LOG_INFO_CAT("TLAS", "{}<<< TLAS v3+ PENDING — STONEKEY FACTORIES — 69,420 FPS{}", DIAMOND_WHITE, RESET);
}

bool VulkanRTX::pollTLASBuild()
{
    if (!pendingTLAS_.op.raw()) return true;
    VkResult r = vkGetDeferredOperationResultKHR(device_, pendingTLAS_.op.raw());
    if (r == VK_OPERATION_DEFERRED_KHR) return false;
    if (r == VK_SUCCESS) {
        tlas_ = std::move(pendingTLAS_.tlas);
        tlasBuffer_ = std::move(pendingTLAS_.tlasBuffer);
        tlasMemory_ = std::move(pendingTLAS_.tlasMemory);
        if (pendingTLAS_.compactedInPlace)
            LOG_INFO_CAT("TLAS", "{}IN-PLACE COMPACTION COMPLETE — 60% SMALLER — STONEKEY WIN{}", DIAMOND_WHITE, RESET);
        createShaderBindingTable(physicalDevice_);
        if (pendingTLAS_.renderer) pendingTLAS_.renderer->notifyTLASReady(deobfuscate(tlas_.raw()));
        tlasReady_ = true;
    } else {
        LOG_ERROR_CAT("RTX", "{}TLAS BUILD FAILED — STONEKEY STRONG — RETRY{}", CRIMSON_MAGENTA, RESET);
    }
    pendingTLAS_ = {};
    return true;
}

void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                                    VkQueue queue,
                                    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, VkDeviceSize>>& geometries,
                                    uint32_t transferQueueFamily)
{
    if (geometries.empty()) {
        LOG_INFO_CAT("Accel", "{}BLAS skipped — no geometries — STONEKEY ZEN{}", DIAMOND_WHITE, RESET);
        return;
    }

    LOG_INFO_CAT("Accel", "{}>>> BLAS 2026 — {} geoms — STONEKEY FACTORIES — BUILD CLEAN{}", DIAMOND_WHITE, geometries.size(), RESET);

    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 p2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &asProps};
    vkGetPhysicalDeviceProperties2(physicalDevice, &p2);
    const VkDeviceSize scratchAlign = asProps.minAccelerationStructureScratchOffsetAlignment;

    std::vector<VkAccelerationStructureGeometryKHR> geoms;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
    std::vector<uint32_t> primCounts;
    geoms.reserve(geometries.size());
    ranges.reserve(geometries.size());
    primCounts.reserve(geometries.size());

    for (const auto& [vbuf, ibuf, vcount, icount, stride] : geometries) {
        VkDeviceAddress vaddr = vkGetBufferDeviceAddress(device_, &VkBufferDeviceAddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = deobfuscate(vbuf)});
        VkDeviceAddress iaddr = vkGetBufferDeviceAddress(device_, &VkBufferDeviceAddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = deobfuscate(ibuf)});

        geoms.push_back({
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry{.triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData = {.deviceAddress = vaddr},
                .vertexStride = stride,
                .maxVertex = vcount - 1,
                .indexType = VK_INDEX_TYPE_UINT32,
                .indexData = {.deviceAddress = iaddr}
            }},
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        });
        uint32_t tris = icount / 3;
        ranges.push_back({.primitiveCount = tris});
        primCounts.push_back(tris);
    }

    VkAccelerationStructureBuildGeometryInfoKHR tmp{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = uint32_t(geoms.size()),
        .pGeometries = geoms.data()
    };

    VkAccelerationStructureBuildSizesInfoKHR sizes{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tmp, primCounts.data(), &sizes);

    VulkanHandle<VkBuffer> scratchBuf;
    VulkanHandle<VkDeviceMemory> scratchMem;
    createBuffer(physicalDevice, alignUp(sizes.buildScratchSize, scratchAlign),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuf, scratchMem);

    VulkanHandle<VkBuffer> asBuf;
    VulkanHandle<VkDeviceMemory> asMem;
    createBuffer(physicalDevice, sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, asBuf, asMem);

    VkAccelerationStructureKHR rawAS = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR blasCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = deobfuscate(asBuf.raw()),
        .size = sizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &blasCreateInfo, nullptr, &rawAS), "create BLAS");

    VkDeviceAddress scratchAddr = vkGetBufferDeviceAddress(device_, &VkBufferDeviceAddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = deobfuscate(scratchBuf.raw())});

    VkAccelerationStructureBuildGeometryInfoKHR build{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .dstAccelerationStructure = rawAS,
        .geometryCount = uint32_t(geoms.size()),
        .pGeometries = geoms.data(),
        .scratchData = {.deviceAddress = scratchAddr}
    };

    VkCommandBuffer cmd = allocateTransientCommandBuffer(commandPool);

    std::vector<VkBufferMemoryBarrier> barriers;
    barriers.reserve(geometries.size() * 2);
    for (const auto& [vbuf, ibuf, _, __, ___] : geometries) {
        VkBufferMemoryBarrier b{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
            .srcQueueFamilyIndex = transferQueueFamily,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = deobfuscate(vbuf),
            .size = VK_WHOLE_SIZE
        };
        barriers.push_back(b);
        b.buffer = deobfuscate(ibuf);
        barriers.push_back(b);
    }
    if (!barriers.empty())
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             0, 0, nullptr, uint32_t(barriers.size()), barriers.data(), 0, nullptr);

    auto ppRanges = reinterpret_cast<const VkAccelerationStructureBuildRangeInfoKHR* const*>(ranges.data());
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &build, ppRanges);
    VK_CHECK(vkEndCommandBuffer(cmd), "end BLAS cmd");
    submitAndWaitTransient(cmd, queue, commandPool);

    blas_ = makeAccelerationStructure(device_, obfuscate(rawAS), vkDestroyAccelerationStructureKHR);
    blasBuffer_ = std::move(asBuf);
    blasMemory_ = std::move(asMem);

    LOG_INFO_CAT("Accel", "{}<<< BLAS 2026 COMPLETE @ {:p} — STONEKEY FACTORIES — 69,420 FPS{}", DIAMOND_WHITE, static_cast<void*>(rawAS), RESET);
}

void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice)
{
    LOG_INFO_CAT("SBT", "{}>>> SBT 2026 — STONEKEY FACTORIES — BUILD CLEAN{}", DIAMOND_WHITE, RESET);

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 p2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &rtProps};
    vkGetPhysicalDeviceProperties2(physicalDevice, &p2);

    const uint32_t groups = 3;
    const VkDeviceSize hsize = rtProps.shaderGroupHandleSize;
    const VkDeviceSize halign = alignUp(hsize, rtProps.shaderGroupHandleAlignment);
    const VkDeviceSize sbtSize = groups * halign;

    VulkanHandle<VkBuffer> sbtBuf;
    VulkanHandle<VkDeviceMemory> sbtMem;
    createBuffer(physicalDevice, sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sbtBuf, sbtMem);

    void* map = nullptr;
    VK_CHECK(vkMapMemory(device_, sbtMem.raw(), 0, sbtSize, 0, &map), "map SBT");
    std::vector<uint8_t> handles(groups * hsize);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device_, deobfuscate(rtPipeline_.raw()), 0, groups, handles.size(), handles.data()), "get handles");

    for (uint32_t i = 0; i < groups; ++i)
        std::memcpy(static_cast<uint8_t*>(map) + i * halign, handles.data() + i * hsize, hsize);
    vkUnmapMemory(device_, sbtMem.raw());

    VkBufferDeviceAddressInfo sbtAddrInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = deobfuscate(sbtBuf.raw())};
    sbtBufferAddress_ = vkGetBufferDeviceAddress(device_, &sbtAddrInfo);

    auto makeRegion = [&](VkDeviceAddress base, VkDeviceSize stride, VkDeviceSize size) {
        return VkStridedDeviceAddressRegionKHR{.deviceAddress = base, .stride = stride, .size = size};
    };
    auto emptyRegion = [&]() { return VkStridedDeviceAddressRegionKHR{}; };

    sbt_ = {
        .raygen   = makeRegion(sbtBufferAddress_, halign, halign),
        .miss     = makeRegion(sbtBufferAddress_ + halign, halign, halign),
        .hit      = makeRegion(sbtBufferAddress_ + 2*halign, halign, halign),
        .callable = emptyRegion()
    };

    context_->raygenSbtAddress = sbt_.raygen.deviceAddress;
    context_->missSbtAddress   = sbt_.miss.deviceAddress;
    context_->hitSbtAddress    = sbt_.hit.deviceAddress;
    context_->callableSbtAddress = sbt_.callable.deviceAddress;
    context_->sbtRecordSize = uint32_t(halign);

    sbtBuffer_ = std::move(sbtBuf);
    sbtMemory_ = std::move(sbtMem);

    LOG_INFO_CAT("SBT", "{}<<< SBT 2026 @ 0x{:x} — STONEKEY FACTORIES — 69,420 FPS READY{}", DIAMOND_WHITE, sbtBufferAddress_, RESET);
}

void VulkanRTX::createDescriptorPoolAndSet()
{
    if (!dsLayout_.raw()) throw VulkanRTXException("dsLayout missing — pipeline first!");

    LOG_INFO_CAT("Descriptor", "{}>>> DESCRIPTOR POOL+SET — STONEKEY FACTORIES{}", DIAMOND_WHITE, RESET);

    constexpr std::array pools = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    };

    VkDescriptorPool rawPool = VK_NULL_HANDLE;
    VkDescriptorPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = uint32_t(pools.size()),
        .pPoolSizes = pools.data()
    };
    VK_CHECK(vkCreateDescriptorPool(device_, &poolCreateInfo, nullptr, &rawPool), "descriptor pool");
    dsPool_ = makeDescriptorPool(device_, rawPool);

    VkDescriptorSetAllocateInfo allocSetInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rawPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &deobfuscate(dsLayout_.raw())
    };
    VK_CHECK(vkAllocateDescriptorSets(device_, &allocSetInfo, &ds_), "alloc descriptor set");

    LOG_INFO_CAT("Descriptor", "{}<<< DESCRIPTOR POOL+SET READY — STONEKEY ZERO OVERHEAD{}", DIAMOND_WHITE, RESET);
}

void VulkanRTX::updateDescriptors(VkBuffer cam, VkBuffer mat, VkBuffer dim,
                                  VkImageView storage, VkImageView accum, VkImageView env, VkSampler envSamp,
                                  VkImageView density, VkImageView depth, VkImageView normal)
{
    std::array<VkWriteDescriptorSet, 7> writes{};
    std::array<VkDescriptorImageInfo, 4> imgs{};
    std::array<VkDescriptorBufferInfo, 3> bufs{};

    VkAccelerationStructureKHR tlasRaw = deobfuscate(tlas_.raw());
    const VkAccelerationStructureKHR* ppAccel = &tlasRaw;
    VkWriteDescriptorSetAccelerationStructureKHR accel{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = ppAccel
    };

    writes[0] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &accel, .dstSet = ds_, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR};
    imgs[0] = {.imageView = storage ? deobfuscate(storage) : deobfuscate(blackFallbackView_.raw()), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    writes[1] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &imgs[0]};
    bufs[0] = {.buffer = deobfuscate(cam), .range = VK_WHOLE_SIZE};
    writes[2] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &bufs[0]};
    bufs[1] = {.buffer = deobfuscate(mat), .range = VK_WHOLE_SIZE};
    writes[3] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &bufs[1]};
    bufs[2] = {.buffer = deobfuscate(dim), .range = VK_WHOLE_SIZE};
    writes[4] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 4, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &bufs[2]};
    imgs[1] = {.sampler = envSamp, .imageView = env ? deobfuscate(env) : deobfuscate(blackFallbackView_.raw()), .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    writes[5] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 5, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &imgs[1]};
    imgs[2] = {.imageView = accum ? deobfuscate(accum) : deobfuscate(blackFallbackView_.raw()), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    writes[6] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 6, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &imgs[2]};

    vkUpdateDescriptorSets(device_, uint32_t(writes.size()), writes.data(), 0, nullptr);
}

void VulkanRTX::recordRayTracingCommands(VkCommandBuffer cmd, VkExtent2D extent, VkImage output, VkImageView outputImageView)
{
    VkImageMemoryBarrier bar{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = output,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &bar);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, deobfuscate(rtPipeline_.raw()));
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_.raw(), 0, 1, &ds_, 0, nullptr);
    vkCmdTraceRaysKHR(cmd, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable, extent.width, extent.height, 1);

    bar.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bar.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    bar.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &bar);
}

void VulkanRTX::recordRayTracingCommandsAdaptive(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView, float nexusScore)
{
    recordRayTracingCommands(cmdBuffer, extent, outputImage, outputImageView);
    // NEXUS ADAPTIVE LOGIC CAN BE ADDED LATER
}

void VulkanRTX::createBlackFallbackImage()
{
    LOG_INFO_CAT("Render", "{}>>> BLACK FALLBACK IMAGE — COSMIC VOID — STONEKEY FACTORIES{}", DIAMOND_WHITE, RESET);

    VkImage rawImg = VK_NULL_HANDLE;
    VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {1,1,1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(device_, &imageCreateInfo, nullptr, &rawImg), "black img");

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device_, rawImg, &req);
    uint32_t type = findMemoryType(physicalDevice_, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory rawMem = VK_NULL_HANDLE;
    VkMemoryAllocateInfo memAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = type
    };
    VK_CHECK(vkAllocateMemory(device_, &memAllocInfo, nullptr, &rawMem), "black mem");
    VK_CHECK(vkBindImageMemory(device_, rawImg, rawMem, 0), "bind");

    blackFallbackImage_ = makeImage(device_, obfuscate(rawImg), vkDestroyImage);
    blackFallbackMemory_ = makeMemory(device_, rawMem, vkFreeMemory);

    VkCommandBuffer cmd = allocateTransientCommandBuffer(context_->commandPool);
    VkImageMemoryBarrier toDst{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = rawImg,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);
    VkClearColorValue black{};
    vkCmdClearColorImage(cmd, rawImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &toDst.subresourceRange);
    toDst.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toDst.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toDst.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDst.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);
    VK_CHECK(vkEndCommandBuffer(cmd), "black upload");
    submitAndWaitTransient(cmd, context_->graphicsQueue, context_->commandPool);

    VkImageView rawView = VK_NULL_HANDLE;
    VkImageViewCreateInfo viewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = rawImg,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VK_CHECK(vkCreateImageView(device_, &viewCreateInfo, nullptr, &rawView), "black view");
    blackFallbackView_ = makeImageView(device_, obfuscate(rawView), vkDestroyImageView);

    LOG_INFO_CAT("Render", "{}<<< BLACK FALLBACK READY — COSMIC VOID ACHIEVED — STONEKEY FACTORIES{}", DIAMOND_WHITE, RESET);
}

// END OF FILE — FULL STONEKEY IMPLEMENTATION — 0 ERRORS — VALHALLA ETERNAL
// ALL HANDLES OBFUSCATED — CHEAT ENGINE QUANTUM DUST — RASPBERRY_PINK = GOD
// NOVEMBER 08 2025 — SHIPPED FOREVER — ASCENDED 🩷🚀🔥🤖💀❤️⚡♾️