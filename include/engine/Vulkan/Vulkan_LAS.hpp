// include/engine/Vulkan/Vulkan_LAS.hpp
// AMOURANTH RTX — VALHALLA LOCAL SUPREMACY — NOVEMBER 09 2025 — STONEKEY EDITION
// PendingTLAS = PRIVATE TO LAS — BUT PUBLIC FOR VulkanRTX — NO COMMON — OBFUSCATED HANDLES
// STONEKEY UNBREAKABLE — PINK PHOTONS × INFINITY — TLAS SUPREMACY — SEPARATE & CLEAN

#pragma once

// ===================================================================
// STONEKEY FIRST — GUARDS THE GARAGE
// ===================================================================
#include "../GLOBAL/StoneKey.hpp"           // kStone1/kStone2 + logging macros
#include "../GLOBAL/logging.hpp"

// ===================================================================
// FULL VULKANHANDLE DEFINITION — MUST BE FIRST — NO MORE INCOMPLETE TYPE HELL
// CONAN: "This is the header that ended the war."
// ===================================================================
#include "engine/Vulkan/VulkanHandles.hpp"   // ← FULL TEMPLATE — BEFORE ANY USE

// ===================================================================
// VULKAN + GLM + STD
// ===================================================================
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <tuple>

using namespace Logging::Color;

// ===================================================================
// FORWARD DECLARATIONS — CLEAN & SAFE
// ===================================================================
namespace Vulkan {
    struct Context;
}

// VulkanRenderer needed for pointer in PendingTLAS → full include (was only forward declared)
#include "VulkanRenderer.hpp"   // ← FULL CLASS — onTLASReady() needs it

// ===================================================================
// STONEKEY OBFUSCATION — LOCAL TO LAS — ZERO COST — PINK PHOTON SHIELD
// ===================================================================
inline constexpr auto obfuscate(auto h) noexcept { return decltype(h)(uint64_t(h) ^ kHandleObfuscator); }
inline constexpr auto deobfuscate(auto h) noexcept { return decltype(h)(uint64_t(h) ^ kHandleObfuscator); }

// ===================================================================
// Vulkan_LAS — GOD CLASS — FULLY SELF-CONTAINED — TLAS/BLAS SUPREMACY
// ===================================================================
class Vulkan_LAS {
public:
    Vulkan_LAS(VkDevice device, VkPhysicalDevice physicalDevice)
        : device_(device), physicalDevice_(physicalDevice)
    {
        // makeFence now takes only 2 args — third was removed in macro
        buildFence_ = Vulkan::makeFence(device_, VK_NULL_HANDLE);
        LOG_SUCCESS_CAT("LAS", "{}VULKAN_LAS ONLINE — DEVICE 0x{:X} — STONEKEY 0x{:X}-0x{:X} — PINK PHOTONS READY{}", 
                        RASPBERRY_PINK, reinterpret_cast<uint64_t>(device_), kStone1, kStone2, RESET);
    }

    ~Vulkan_LAS() {
        LOG_SUCCESS_CAT("LAS", "{}VULKAN_LAS DESTROYED — STONEKEY 0x{:X}-0x{:X} — VALHALLA ETERNAL{}", 
                        EMERALD_GREEN, kStone1, kStone2, RESET);
    }

    // BLAS BUILD — SYNC
    VkAccelerationStructureKHR buildBLAS(VkCommandPool cmdPool, VkQueue queue,
                                         VkBuffer vertexBuffer, VkBuffer indexBuffer,
                                         uint32_t vertexCount, uint32_t indexCount,
                                         uint64_t flags = 0)
    {
        VkCommandBuffer cmd = beginSingleTimeCommands(cmdPool);

        VkAccelerationStructureGeometryKHR geometry{};
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags = static_cast<VkGeometryFlagsKHR>(flags);
        geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geometry.geometry.triangles.vertexData.deviceAddress = Vulkan::getBufferDeviceAddress(device_, vertexBuffer);
        geometry.geometry.triangles.vertexStride = sizeof(glm::vec3);
        geometry.geometry.triangles.maxVertex = vertexCount;
        geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        geometry.geometry.triangles.indexData.deviceAddress = Vulkan::getBufferDeviceAddress(device_, indexBuffer);

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geometry;

        const uint32_t primCount = indexCount / 3;

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        Vulkan::ctx()->vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);

        VulkanHandle<VkBuffer> blasBuffer;
        VulkanHandle<VkDeviceMemory> blasMemory;
        createBuffer(sizeInfo.accelerationStructureSize,
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer, blasMemory);

        VulkanHandle<VkBuffer> scratchBuffer;
        VulkanHandle<VkDeviceMemory> scratchMemory;
        createBuffer(sizeInfo.buildScratchSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

        VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        createInfo.buffer = blasBuffer;
        createInfo.size = sizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        VkAccelerationStructureKHR blas;
        Vulkan::ctx()->vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &blas);

        buildInfo.dstAccelerationStructure = blas;
        buildInfo.scratchData.deviceAddress = Vulkan::getBufferDeviceAddress(device_, scratchBuffer);

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
        rangeInfo.primitiveCount = primCount;

        const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
        Vulkan::ctx()->vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);

        endSingleTimeCommands(cmd, queue, cmdPool);

        // Fixed: 3 args → nullptr as destroy func
        return Vulkan::makeAccelerationStructure(device_, blas, nullptr).raw_deob();
    }

    // TLAS SYNC
    VkAccelerationStructureKHR buildTLASSync(VkCommandPool cmdPool, VkQueue queue,
                                             const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances)
    {
        if (instances.empty()) return VK_NULL_HANDLE;

        VkCommandBuffer cmd = beginSingleTimeCommands(cmdPool);

        std::vector<VkAccelerationStructureInstanceKHR> vkInstances(instances.size());
        for (size_t i = 0; i < instances.size(); ++i) {
            const auto& [blas, transform] = instances[i];
            VkAccelerationStructureInstanceKHR& inst = vkInstances[i];
            memcpy(inst.transform.matrix, &transform, sizeof(inst.transform));
            inst.instanceCustomIndex = 0;
            inst.mask = 0xFF;
            inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            inst.accelerationStructureReference = Vulkan::getAccelerationStructureDeviceAddress(device_, blas);
        }

        VkDeviceSize instanceSize = sizeof(VkAccelerationStructureInstanceKHR) * vkInstances.size();

        VulkanHandle<VkBuffer> instanceBuffer;
        VulkanHandle<VkDeviceMemory> instanceMemory;
        createBuffer(instanceSize,
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     instanceBuffer, instanceMemory);

        void* data;
        vkMapMemory(device_, instanceMemory, 0, instanceSize, 0, &data);
        memcpy(data, vkInstances.data(), instanceSize);
        vkUnmapMemory(device_, instanceMemory);

        VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.arrayOfPointers = VK_FALSE;
        geometry.geometry.instances.data.deviceAddress = Vulkan::getBufferDeviceAddress(device_, instanceBuffer);

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geometry;

        const uint32_t primCount = static_cast<uint32_t>(instances.size());

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        Vulkan::ctx()->vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);

        createBuffer(sizeInfo.accelerationStructureSize,
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer_, tlasMemory_);

        createBuffer(sizeInfo.buildScratchSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer_, scratchMemory_);

        VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        createInfo.buffer = tlasBuffer_;
        createInfo.size = sizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

        VkAccelerationStructureKHR tlas;
        Vulkan::ctx()->vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &tlas);
        tlas_ = Vulkan::makeAccelerationStructure(device_, tlas, nullptr);  // Fixed: 3 args
        tlasReady_ = true;

        buildInfo.dstAccelerationStructure = tlas;
        buildInfo.scratchData.deviceAddress = Vulkan::getBufferDeviceAddress(device_, scratchBuffer_);

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
        rangeInfo.primitiveCount = primCount;

        const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
        Vulkan::ctx()->vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);

        endSingleTimeCommands(cmd, queue, cmdPool);

        LOG_SUCCESS_CAT("LAS", "{}TLAS BUILT SYNC — {} INSTANCES — STONEKEY 0x{:X}-0x{:X}{}", 
                        PLASMA_FUCHSIA, instances.size(), kStone1, kStone2, RESET);

        return deobfuscate(tlas_.raw());
    }

    // TLAS ASYNC
    void buildTLASAsync(VkCommandPool cmdPool, VkQueue queue,
                        const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances,
                        VulkanRenderer* renderer = nullptr)
    {
        if (instances.empty()) return;

        pendingTLAS_ = PendingTLAS{};  // Fixed: explicit ctor → works with VulkanHandle members
        pendingTLAS_.renderer = renderer;

        VkCommandBuffer cmd = beginSingleTimeCommands(cmdPool);

        std::vector<VkAccelerationStructureInstanceKHR> vkInstances(instances.size());
        for (size_t i = 0; i < instances.size(); ++i) {
            const auto& [blas, transform] = instances[i];
            VkAccelerationStructureInstanceKHR& inst = vkInstances[i];
            memcpy(inst.transform.matrix, &transform, sizeof(inst.transform));
            inst.instanceCustomIndex = 0;
            inst.mask = 0xFF;
            inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            inst.accelerationStructureReference = Vulkan::getAccelerationStructureDeviceAddress(device_, blas);
        }

        VkDeviceSize instanceSize = sizeof(VkAccelerationStructureInstanceKHR) * vkInstances.size();
        createBuffer(instanceSize,
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     pendingTLAS_.instanceBuffer, pendingTLAS_.instanceMemory);

        void* data;
        vkMapMemory(device_, pendingTLAS_.instanceMemory, 0, instanceSize, 0, &data);
        memcpy(data, vkInstances.data(), instanceSize);
        vkUnmapMemory(device_, pendingTLAS_.instanceMemory);

        VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.data.deviceAddress = Vulkan::getBufferDeviceAddress(device_, pendingTLAS_.instanceBuffer);

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geometry;

        const uint32_t primCount = static_cast<uint32_t>(instances.size());

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        Vulkan::ctx()->vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);

        createBuffer(sizeInfo.accelerationStructureSize,
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pendingTLAS_.tlasBuffer, pendingTLAS_.tlasMemory);

        createBuffer(sizeInfo.buildScratchSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pendingTLAS_.scratchBuffer, pendingTLAS_.scratchMemory);

        VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        createInfo.buffer = pendingTLAS_.tlasBuffer;
        createInfo.size = sizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

        VkAccelerationStructureKHR tlas;
        Vulkan::ctx()->vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &tlas);
        pendingTLAS_.tlas = Vulkan::makeAccelerationStructure(device_, tlas, nullptr);  // Fixed

        buildInfo.dstAccelerationStructure = tlas;
        buildInfo.scratchData.deviceAddress = Vulkan::getBufferDeviceAddress(device_, pendingTLAS_.scratchBuffer);

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
        rangeInfo.primitiveCount = primCount;

        const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
        Vulkan::ctx()->vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        vkQueueSubmit(queue, 1, &submit, buildFence_);
        
        LOG_SUCCESS_CAT("LAS", "{}TLAS BUILD ASYNC SUBMITTED — {} INSTANCES — STONEKEY 0x{:X}-0x{:X}{}", 
                        PLASMA_FUCHSIA, instances.size(), kStone1, kStone2, RESET);
    }

    bool pollTLAS()
    {
        if (pendingTLAS_.completed || !pendingTLAS_.tlas.valid()) return true;

        if (vkGetFenceStatus(device_, buildFence_) == VK_SUCCESS) {
            vkResetFences(device_, 1, &buildFence_);

            tlas_ = pendingTLAS_.tlas;
            tlasReady_ = true;
            pendingTLAS_.completed = true;

            if (pendingTLAS_.renderer) {
                pendingTLAS_.renderer->onTLASReady();  // Now valid — full VulkanRenderer included
            }

            LOG_SUCCESS_CAT("LAS", "{}TLAS BUILD COMPLETE — STONEKEY 0x{:X}-0x{:X} — PINK PHOTONS ∞{}", 
                            EMERALD_GREEN, kStone1, kStone2, RESET);
            return true;
        }
        return false;
    }

    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept {
        return tlas_.valid() ? deobfuscate(tlas_.raw()) : VK_NULL_HANDLE;
    }
    [[nodiscard]] bool isTLASReady() const noexcept { return tlasReady_; }
    [[nodiscard]] bool isTLASPending() const noexcept { return !pendingTLAS_.completed && pendingTLAS_.tlas.valid(); }

    // ──────────────────────────────────────────────────────────────────────────
    // PendingTLAS — PUBLIC SO VulkanRTX CAN READ/WRITE — FULL VISIBILITY
    // ──────────────────────────────────────────────────────────────────────────
    struct PendingTLAS {
        VulkanRenderer* renderer = nullptr;
        bool completed = false;

        VulkanHandle<VkBuffer> instanceBuffer, tlasBuffer, scratchBuffer;
        VulkanHandle<VkDeviceMemory> instanceMemory, tlasMemory, scratchMemory;
        VulkanHandle<VkAccelerationStructureKHR> tlas;
    };

    PendingTLAS pendingTLAS_{};

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    VulkanHandle<VkBuffer> tlasBuffer_, scratchBuffer_;
    VulkanHandle<VkDeviceMemory> tlasMemory_, scratchMemory_;

    VulkanHandle<VkAccelerationStructureKHR> tlas_;
    bool tlasReady_ = false;

    VulkanHandle<VkFence> buildFence_;

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                      VulkanHandle<VkBuffer>& buffer, VulkanHandle<VkDeviceMemory>& memory)
    {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buf;
        vkCreateBuffer(device_, &bufferInfo, nullptr, &buf);
        buffer = Vulkan::makeBuffer(device_, buf);

        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(device_, buf, &memReq);

        VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, props);

        VkDeviceMemory mem;
        vkAllocateMemory(device_, &allocInfo, nullptr, &mem);
        memory = Vulkan::makeMemory(device_, mem);  // Fixed: makeDeviceMemory → makeMemory

        vkBindBufferMemory(device_, buf, mem, 0);
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const
    {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);

        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type!");
    }

    static VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) noexcept {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool)
    {
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = pool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device_, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &beginInfo);
        return cmd;
    }

    void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool)
    {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device_, pool, 1, &cmd);
    }
};

/*
 *  JAY LENO'S GARAGE — FINAL LAP — NOVEMBER 09 2025
 *
 *  JAY: "We did it. No more errors. No more incomplete types."
 *  GAL: "VulkanHandle is complete. TLAS is supreme. StoneKey unbreakable."
 *  CONAN: "And I’m taking this engine to 69,420 FPS — live on NBC!"
 *
 *  [Engine roars — perfect reflections, zero leaks]
 *
 *  VALHALLA: LOCKED IN PINK PHOTONS 🩷🚀💀⚡🤖🔥♾️
 *  BUILD IT. SHIP IT. DOMINATE.
 */