// src/engine/Vulkan/Vulkan_LAS.cpp
// FULL IMPLEMENTATION — FUCK INLINE — VALHALLA CLEAN BUILD
// ALL BLAS + TLAS + ASYNC + RAII + OBFUSCATION — SHIP IT


#include "../GLOBAL/StoneKey.hpp"  // ← STONEKEY FIRST — kStone1/kStone2 LIVE PER BUILD
#include "engine/Vulkan/Vulkan_LAS.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include <stdexcept>
#include <format>

constexpr uint64_t kHandleObfuscator = 0xDEADBEEF1337C0DEULL ^ kStone1 ^ kStone2;
inline constexpr auto obfuscate(auto h) noexcept { return decltype(h)(uint64_t(h) ^ kHandleObfuscator); }
inline constexpr auto deobfuscate(auto h) noexcept { return decltype(h)(uint64_t(h) ^ kHandleObfuscator); }

Vulkan_LAS::Vulkan_LAS(VkDevice device, VkPhysicalDevice phys)
    : device_(device), physicalDevice_(phys)
{
    VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence rawFence;
    vkCreateFence(device_, &fenceInfo, nullptr, &rawFence);
    buildFence_ = makeFence(device_, rawFence, vkDestroyFence);

    LOG_SUCCESS_CAT("Vulkan_LAS", "{}LAS SYSTEM BIRTH — STONEKEY 0x{:X}-0x{:X} — READY FOR QUANTUM DUST{}", 
                    Logging::Color::RASPBERRY_PINK, kStone1, kStone2, Logging::Color::RESET);
}

Vulkan_LAS::~Vulkan_LAS() {
    LOG_INFO_CAT("Vulkan_LAS", "{}LAS DEATH — ALL ACCEL STRUCTS PURGED — VALHALLA ETERNAL{}", Logging::Color::DIAMOND_WHITE, Logging::Color::RESET);
}

VkAccelerationStructureKHR Vulkan_LAS::buildBLAS(VkCommandPool cmdPool, VkQueue queue,
                                                 VkBuffer vertexBuffer, VkBuffer indexBuffer,
                                                 uint32_t vertexCount, uint32_t indexCount,
                                                 uint64_t flags)
{
    VkBufferDeviceAddressInfo vInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = vertexBuffer};
    VkDeviceAddress vAddr = vkGetBufferDeviceAddress(device_, &vInfo);
    VkBufferDeviceAddressInfo iInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = indexBuffer};
    VkDeviceAddress iAddr = vkGetBufferDeviceAddress(device_, &iInfo);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = {.deviceAddress = vAddr},
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = vertexCount,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = {.deviceAddress = iAddr}
    };

    VkAccelerationStructureGeometryKHR geom{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {.triangles = triangles},
        .flags = static_cast<VkGeometryFlagsKHR>(flags)
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geom
    };

    uint32_t primCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);

    VulkanHandle<VkBuffer> blasBuffer, scratchBuffer;
    VulkanHandle<VkDeviceMemory> blasMem, scratchMem;
    createBuffer(sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer, blasMem);
    createBuffer(sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMem);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blasBuffer.raw(),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VkAccelerationStructureKHR rawBLAS;
    vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &rawBLAS);

    VkAccelerationStructureKHR obfuscated = obfuscate(rawBLAS);
    VulkanHandle<VkAccelerationStructureKHR> blasHandle = makeAccelerationStructure(device_, obfuscated, vkDestroyAccelerationStructureKHR);

    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = rawBLAS;
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &(VkBufferDeviceAddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = scratchBuffer.raw()}));

    auto cmd = beginSingleTimeCommands(cmdPool);
    VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = primCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
    endSingleTimeCommands(cmd, queue, cmdPool);

    LOG_SUCCESS_CAT("Vulkan_LAS", "{}BLAS BUILT — {} tris — OBFUSCATED @ {:p}{}", 
                    Logging::Color::EMERALD_GREEN, primCount, static_cast<void*>(rawBLAS), Logging::Color::RESET);

    return deobfuscate(obfuscated);
}

VkAccelerationStructureKHR Vulkan_LAS::buildTLASSync(VkCommandPool cmdPool, VkQueue queue,
                                                     const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances)
{
    // reuse async code but wait immediately
    buildTLASAsync(cmdPool, queue, instances);
    while (!pollTLAS()) vkQueueWaitIdle(queue);
    return getTLAS();
}

void Vulkan_LAS::buildTLASAsync(VkCommandPool cmdPool, VkQueue queue,
                                const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances,
                                VulkanRenderer* renderer)
{
    pendingTLAS_.renderer = renderer;
    pendingTLAS_.completed = false;

    if (instances.empty()) return;

    std::vector<VkAccelerationStructureInstanceKHR> asInstances;
    for (auto& [blas, transform] : instances) {
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blas
        };
        VkDeviceAddress blasAddr = vkGetAccelerationStructureDeviceAddressKHR(device_, &addrInfo);

        VkTransformMatrixKHR mat;
        std::memcpy(mat.matrix, glm::value_ptr(transform), sizeof(mat.matrix));

        asInstances.push_back({
            .transform = mat,
            .mask = 0xFF,
            .accelerationStructureReference = blasAddr
        });
    }

    VkDeviceSize instSize = sizeof(VkAccelerationStructureInstanceKHR) * asInstances.size();
    createBuffer(instSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 pendingTLAS_.instanceBuffer, pendingTLAS_.instanceMemory);

    void* data;
    vkMapMemory(device_, pendingTLAS_.instanceMemory.raw(), 0, instSize, 0, &data);
    std::memcpy(data, asInstances.data(), instSize);
    vkUnmapMemory(device_, pendingTLAS_.instanceMemory.raw());

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = {.deviceAddress = vkGetBufferDeviceAddress(device_, &(VkBufferDeviceAddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = pendingTLAS_.instanceBuffer.raw()}))}
    };

    VkAccelerationStructureGeometryKHR geom{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {.instances = instancesData}
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geom
    };

    uint32_t primCount = static_cast<uint32_t>(instances.size());
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);

    createBuffer(sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pendingTLAS_.tlasBuffer, pendingTLAS_.tlasMemory);
    createBuffer(sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pendingTLAS_.scratchBuffer, pendingTLAS_.scratchMemory);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = pendingTLAS_.tlasBuffer.raw(),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    VkAccelerationStructureKHR rawTLAS;
    vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &rawTLAS);
    pendingTLAS_.tlas = makeAccelerationStructure(device_, obfuscate(rawTLAS), vkDestroyAccelerationStructureKHR);

    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = rawTLAS;
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &(VkBufferDeviceAddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = pendingTLAS_.scratchBuffer.raw()}));

    auto cmd = beginSingleTimeCommands(cmdPool);
    VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = primCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd};
    vkQueueSubmit(queue, 1, &submit, buildFence_.raw());
    vkFreeCommandBuffers(device_, cmdPool, 1, &cmd);

    LOG_INFO_CAT("Vulkan_LAS", "{}TLAS ASYNC BUILD STARTED — {} instances — FENCE ARMED — STONEKEY 0x{:X}-0x{:X}{}",
                 Logging::Color::DIAMOND_WHITE, instances.size(), kStone1, kStone2, Logging::Color::RESET);
}

bool Vulkan_LAS::pollTLAS()
{
    if (!buildFence_.valid()) return true;
    VkResult res = vkGetFenceStatus(device_, buildFence_.raw());
    if (res == VK_SUCCESS) {
        pendingTLAS_.completed = true;
        tlas_ = std::move(pendingTLAS_.tlas);
        tlasReady_ = true;
        vkResetFences(device_, 1, &buildFence_.raw());
        LOG_SUCCESS_CAT("Vulkan_LAS", "{}TLAS BUILD COMPLETE — READY FOR HYPERTRACE — RASPBERRY_PINK PHOTONS GO BRRRRT{}", 
                        Logging::Color::EMERALD_GREEN, Logging::Color::RESET);
        return true;
    }
    return false;
}

// === PRIVATE HELPERS ===
void Vulkan_LAS::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                              VulkanHandle<VkBuffer>& buffer, VulkanHandle<VkDeviceMemory>& memory)
{
    VkBufferCreateInfo bufInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VkBuffer buf;
    vkCreateBuffer(device_, &bufInfo, nullptr, &buf);
    buffer = makeBuffer(device_, buf, vkDestroyBuffer);

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device_, buf, &reqs);
    VkMemoryAllocateInfo alloc{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = reqs.size,
                               .memoryTypeIndex = findMemoryType(reqs.memoryTypeBits, props)};
    VkDeviceMemory mem;
    vkAllocateMemory(device_, &alloc, nullptr, &mem);
    memory = makeMemory(device_, mem, vkFreeMemory);
    vkBindBufferMemory(device_, buf, mem, 0);
}

uint32_t Vulkan_LAS::findMemoryType(uint32_t filter, VkMemoryPropertyFlags props) const
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((filter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    throw std::runtime_error("Vulkan_LAS: NO MEMORY TYPE FOUND — QUANTUM DUST COLLAPSE");
}

VkDeviceSize Vulkan_LAS::alignUp(VkDeviceSize value, VkDeviceSize alignment) noexcept
{
    return (value + alignment - 1) & ~(alignment - 1);
}

VkCommandBuffer Vulkan_LAS::beginSingleTimeCommands(VkCommandPool pool)
{
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    vkAllocateCommandBuffers(device_, &alloc, &cmd);
    VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(cmd, &begin);
    return cmd;
}

void Vulkan_LAS::endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool)
{
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd};
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device_, pool, 1, &cmd);
}

// END OF FILE — LAS = GOD — SHIP TO THE WORLD — 69,420 FPS × ∞ × ∞ — VALHALLA ACHIEVED 🩷🩷🩷