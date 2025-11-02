// src/engine/Vulkan/VulkanRTX_Setup.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts
// 100% COMPILE + LINK – ONLY IMPLEMENTATION
// FIXED: Call createDescriptorSetLayout() in ctor to populate dsLayout_ for renderer

#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <vector>
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdint>

using namespace Logging::Color;

namespace VulkanRTX {

/* -----------------------------------------------------------------
   VulkanResource – template implementation
   ----------------------------------------------------------------- */
template<typename T, typename DestroyFuncType>
VulkanResource<T, DestroyFuncType>::VulkanResource(VkDevice device, T resource, DestroyFuncType destroyFunc)
    : device_(device), resource_(resource), destroyFunc_(destroyFunc)
{
    if (!device || !resource || !destroyFunc)
        throw VulkanRTXException("VulkanResource initialized with null");
}
template<typename T, typename DestroyFuncType>
VulkanResource<T, DestroyFuncType>::~VulkanResource()
{
    if (resource_ != VK_NULL_HANDLE && destroyFunc_)
        destroyFunc_(device_, resource_, nullptr);
}
template<typename T, typename DestroyFuncType>
VulkanResource<T, DestroyFuncType>::VulkanResource(VulkanResource&& o) noexcept
    : device_(o.device_), resource_(o.resource_), destroyFunc_(o.destroyFunc_)
{
    o.resource_ = VK_NULL_HANDLE;
}
template<typename T, typename DestroyFuncType>
VulkanResource<T, DestroyFuncType>& VulkanResource<T, DestroyFuncType>::operator=(VulkanResource&& o) noexcept
{
    if (this != &o) {
        if (resource_ && destroyFunc_) destroyFunc_(device_, resource_, nullptr);
        device_ = o.device_; resource_ = o.resource_; destroyFunc_ = o.destroyFunc_;
        o.resource_ = VK_NULL_HANDLE;
    }
    return *this;
}
template<typename T, typename DestroyFuncType>
void VulkanResource<T, DestroyFuncType>::swap(VulkanResource& other) noexcept
{
    std::swap(device_, other.device_);
    std::swap(resource_, other.resource_);
    std::swap(destroyFunc_, other.destroyFunc_);
}

/* -----------------------------------------------------------------
   VulkanDescriptorSet – implementation
   ----------------------------------------------------------------- */
VulkanDescriptorSet::VulkanDescriptorSet(VkDevice device, VkDescriptorPool pool,
                                         VkDescriptorSet set, PFN_vkFreeDescriptorSets freeFunc)
    : device_(device), pool_(pool), set_(set), freeFunc_(freeFunc)
{
    if (!device || !pool || !set || !freeFunc)
        throw VulkanRTXException("VulkanDescriptorSet initialized with null");
}
VulkanDescriptorSet::~VulkanDescriptorSet()
{
    if (set_ != VK_NULL_HANDLE && freeFunc_)
        freeFunc_(device_, pool_, 1, &set_);
}
VulkanDescriptorSet::VulkanDescriptorSet(VulkanDescriptorSet&& o) noexcept
    : device_(o.device_), pool_(o.pool_), set_(o.set_), freeFunc_(o.freeFunc_)
{
    o.set_ = VK_NULL_HANDLE;
}
VulkanDescriptorSet& VulkanDescriptorSet::operator=(VulkanDescriptorSet&& o) noexcept
{
    if (this != &o) {
        if (set_ && freeFunc_) freeFunc_(device_, pool_, 1, &set_);
        device_ = o.device_; pool_ = o.pool_; set_ = o.set_; freeFunc_ = o.freeFunc_;
        o.set_ = VK_NULL_HANDLE;
    }
    return *this;
}

/* -----------------------------------------------------------------
   Constructor
   ----------------------------------------------------------------- */
VulkanRTX::VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height,
                     VulkanPipelineManager* pipelineMgr)
    : context_(std::move(ctx)),
      pipelineMgr_(pipelineMgr),
      device_(VK_NULL_HANDLE),
      physicalDevice_(VK_NULL_HANDLE),
      extent_{static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
{
    LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX::VulkanRTX() — START [{}x{}]{}", ARCTIC_CYAN, width, height, RESET);

    if (!context_) throw VulkanRTXException("Null context");
    if (!pipelineMgr_) throw VulkanRTXException("Null pipeline manager");
    if (width <= 0 || height <= 0) throw VulkanRTXException("Invalid dimensions");

    device_         = context_->device;
    physicalDevice_ = context_->physicalDevice;

#define LOAD_PROC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device_, #name)); \
    if (!name) throw VulkanRTXException("Failed to load " #name)

    LOAD_PROC(vkGetBufferDeviceAddress);
    LOAD_PROC(vkCmdTraceRaysKHR);
    LOAD_PROC(vkCreateAccelerationStructureKHR);
    LOAD_PROC(vkDestroyAccelerationStructureKHR);
    LOAD_PROC(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_PROC(vkCreateRayTracingPipelinesKHR);
    LOAD_PROC(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_PROC(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_PROC(vkCmdCopyAccelerationStructureKHR);
    LOAD_PROC(vkCreateDescriptorSetLayout);
    LOAD_PROC(vkAllocateDescriptorSets);
    LOAD_PROC(vkCreateDescriptorPool);
    LOAD_PROC(vkDestroyDescriptorSetLayout);
    LOAD_PROC(vkDestroyDescriptorPool);
    LOAD_PROC(vkFreeDescriptorSets);
    LOAD_PROC(vkDestroyPipelineLayout);
    LOAD_PROC(vkDestroyPipeline);
    LOAD_PROC(vkDestroyBuffer);
    LOAD_PROC(vkFreeMemory);
    LOAD_PROC(vkCreateQueryPool);
    LOAD_PROC(vkDestroyQueryPool);
    LOAD_PROC(vkGetQueryPoolResults);
    LOAD_PROC(vkCmdWriteAccelerationStructuresPropertiesKHR);
    LOAD_PROC(vkCreateBuffer);
    LOAD_PROC(vkAllocateMemory);
    LOAD_PROC(vkBindBufferMemory);
    LOAD_PROC(vkCmdBuildAccelerationStructuresKHR);
    LOAD_PROC(vkDestroyImage);
    LOAD_PROC(vkDestroyImageView);

#undef LOAD_PROC

    // FIXED: Create descriptor set layout in constructor to ensure it's available for renderer allocation
    createDescriptorSetLayout();

    LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX::VulkanRTX() — END{}", EMERALD_GREEN, RESET);
}

/* -----------------------------------------------------------------
   Destructor
   ----------------------------------------------------------------- */
VulkanRTX::~VulkanRTX()
{
    LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX::~VulkanRTX() — destroying resources{}", AMBER_YELLOW, RESET);
    LOG_INFO_CAT("VulkanRTX", "{}RAII cleanup complete{}", EMERALD_GREEN, RESET);
}

/* -----------------------------------------------------------------
   Private helpers (unchanged)
   ----------------------------------------------------------------- */
VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool)
{
    LOG_DEBUG_CAT("VulkanRTX", "{}Allocating transient command buffer{}", OCEAN_TEAL, RESET);
    VkCommandBufferAllocateInfo ai = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(device_, &ai, &cmd), "Allocate transient command buffer");
    return cmd;
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool)
{
    LOG_DEBUG_CAT("VulkanRTX", "{}Submitting transient command{}", ARCTIC_CYAN, RESET);
    VkSubmitInfo si = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };
    VK_CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE), "Submit transient");
    VK_CHECK(vkQueueWaitIdle(queue), "Wait for queue idle");
    vkFreeCommandBuffers(device_, pool, 1, &cmd);
}

void VulkanRTX::uploadBlackPixelToImage(VkImage image)
{
    VkCommandBuffer cmd = allocateTransientCommandBuffer(context_->commandPool);
    VkCommandBufferBeginInfo bi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(cmd, &bi);

    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkClearColorValue black = {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);
    submitAndWaitTransient(cmd, context_->graphicsQueue, context_->commandPool);
}

/* -----------------------------------------------------------------
   createBottomLevelAS
   ----------------------------------------------------------------- */
void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                                    VkQueue queue,
                                    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries)
{
    LOG_INFO_CAT("VulkanRTX", "{}=== createBottomLevelAS() START ({}) ==={}", ARCTIC_CYAN, geometries.size(), RESET);
    if (geometries.empty()) return;

    std::vector<VkAccelerationStructureGeometryKHR> asGeometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;
    std::vector<uint32_t> maxPrimitiveCounts;
    asGeometries.reserve(geometries.size());
    buildRanges.reserve(geometries.size());
    maxPrimitiveCounts.reserve(geometries.size());

    for (const auto& [vertexBuffer, indexBuffer, vertexCount, indexCount, vertexStride] : geometries) {
        VkBufferDeviceAddressInfo vertexAddrInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = vertexBuffer};
        VkBufferDeviceAddressInfo indexAddrInfo  = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = indexBuffer};

        VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData = {.deviceAddress = vkGetBufferDeviceAddress(device_, &vertexAddrInfo)},
            .vertexStride = vertexStride,
            .maxVertex = vertexCount,
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData = {.deviceAddress = vkGetBufferDeviceAddress(device_, &indexAddrInfo)}
        };

        asGeometries.push_back(VkAccelerationStructureGeometryKHR{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {.triangles = triangles},
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        });

        buildRanges.push_back(VkAccelerationStructureBuildRangeInfoKHR{
            .primitiveCount = indexCount / 3,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        });

        maxPrimitiveCounts.push_back(indexCount / 3);
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = static_cast<uint32_t>(asGeometries.size()),
        .pGeometries = asGeometries.data()
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfo, maxPrimitiveCounts.data(), &sizeInfo);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> asBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> asMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, asBuffer, asMemory);

    VkAccelerationStructureCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = asBuffer.get(),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VkAccelerationStructureKHR blas;
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &blas), "Create BLAS");

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    VkBufferDeviceAddressInfo scratchAddrInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = scratchBuffer.get()};
    buildInfo.dstAccelerationStructure = blas;
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchAddrInfo);

    auto cmd = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                     .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin), "Begin BLAS build");

    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRange = buildRanges.data();
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRange);

    VkMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                               .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                               .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    VK_CHECK(vkEndCommandBuffer(cmd), "End BLAS build");
    submitAndWaitTransient(cmd, queue, commandPool);

    if (blas_.get()) vkDestroyAccelerationStructureKHR(device_, blas_.get(), nullptr);
    blas_ = {device_, blas, vkDestroyAccelerationStructureKHR};
    blasBuffer_.swap(asBuffer);
    blasMemory_.swap(asMemory);

    LOG_INFO_CAT("VulkanRTX", "{}BLAS created @ 0x{:x}{}", EMERALD_GREEN,
                 reinterpret_cast<uintptr_t>(blas), RESET);
}

/* -----------------------------------------------------------------
   createTopLevelAS
   ----------------------------------------------------------------- */
void VulkanRTX::createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                                 VkQueue queue,
                                 const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances)
{
    LOG_INFO_CAT("VulkanRTX", "{}=== createTopLevelAS() START ({}) ==={}", ARCTIC_CYAN, instances.size(), RESET);
    if (instances.empty() || !blas_.get())
        throw VulkanRTXException("No BLAS or instances");

    std::vector<VkAccelerationStructureInstanceKHR> asInstances;
    asInstances.reserve(instances.size());

    for (const auto& [blas, transform] : instances) {
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blas
        };
        VkDeviceAddress blasAddr = vkGetAccelerationStructureDeviceAddressKHR(device_, &addrInfo);

        glm::mat3x4 trans = glm::transpose(transform);
        asInstances.push_back(VkAccelerationStructureInstanceKHR{
            .transform = {
                .matrix = {
                    {trans[0][0], trans[1][0], trans[2][0], trans[0][3]},
                    {trans[0][1], trans[1][1], trans[2][1], trans[1][3]},
                    {trans[0][2], trans[1][2], trans[2][2], trans[2][3]}
                }
            },
            .instanceCustomIndex = 0,
            .mask = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blasAddr
        });
    }

    VkDeviceSize instanceDataSize = asInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> instanceBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> instanceMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, instanceDataSize,
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 instanceBuffer, instanceMemory);

    void* data = nullptr;
    VK_CHECK(vkMapMemory(device_, instanceMemory.get(), 0, instanceDataSize, 0, &data), "Map instance buffer");
    std::memcpy(data, asInstances.data(), instanceDataSize);
    vkUnmapMemory(device_, instanceMemory.get());

    VkBufferDeviceAddressInfo instanceAddrInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = instanceBuffer.get()};

    VkAccelerationStructureGeometryInstancesDataKHR instancesData = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data = {.deviceAddress = vkGetBufferDeviceAddress(device_, &instanceAddrInfo)}
    };

    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {.instances = instancesData}
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    uint32_t primitiveCount = static_cast<uint32_t>(instances.size());
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfo, &primitiveCount, &sizeInfo);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> asBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> asMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, asBuffer, asMemory);

    VkAccelerationStructureCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = asBuffer.get(),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    VkAccelerationStructureKHR tlas;
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &tlas), "Create TLAS");

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    VkBufferDeviceAddressInfo scratchAddrInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = scratchBuffer.get()};
    buildInfo.dstAccelerationStructure = tlas;
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchAddrInfo);

    auto cmd = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                     .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin), "Begin TLAS build");

    VkAccelerationStructureBuildRangeInfoKHR buildRange = {.primitiveCount = primitiveCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRange = &buildRange;
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRange);

    VkMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                               .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                               .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    VK_CHECK(vkEndCommandBuffer(cmd), "End TLAS build");
    submitAndWaitTransient(cmd, queue, commandPool);

    if (tlas_.get()) vkDestroyAccelerationStructureKHR(device_, tlas_.get(), nullptr);
    tlas_ = {device_, tlas, vkDestroyAccelerationStructureKHR};
    tlasBuffer_.swap(asBuffer);
    tlasMemory_.swap(asMemory);

    LOG_INFO_CAT("VulkanRTX", "{}TLAS created @ 0x{:x}{}", EMERALD_GREEN,
                 reinterpret_cast<uintptr_t>(tlas), RESET);
}

/* -----------------------------------------------------------------
   createDescriptorSetLayout
   ----------------------------------------------------------------- */
void VulkanRTX::createDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 11> bindings = {};

    bindings[static_cast<uint32_t>(DescriptorBindings::TLAS)] = {
        .binding = static_cast<uint32_t>(DescriptorBindings::TLAS),
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::StorageImage)] = {
        .binding = static_cast<uint32_t>(DescriptorBindings::StorageImage),
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::CameraUBO)] = {
        .binding = static_cast<uint32_t>(DescriptorBindings::CameraUBO),
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::MaterialSSBO)] = {
        .binding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO),
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO)] = {
        .binding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO),
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::DenoiseImage)] = {
        .binding = static_cast<uint32_t>(DescriptorBindings::DenoiseImage),
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::EnvMap)] = {
        .binding = static_cast<uint32_t>(DescriptorBindings::EnvMap),
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::DensityVolume)] = {
        .binding = static_cast<uint32_t>(DescriptorBindings::DensityVolume),
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::GDepth)] = {
        .binding = static_cast<uint32_t>(DescriptorBindings::GDepth),
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::GNormal)] = {
        .binding = static_cast<uint32_t>(DescriptorBindings::GNormal),
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::AlphaTex)] = {
        .binding = static_cast<uint32_t>(DescriptorBindings::AlphaTex),
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &layout), "Create RTX descriptor set layout");
    setDescriptorSetLayout(layout);

    LOG_INFO_CAT("VulkanRTX", "{}Descriptor set layout created{}", EMERALD_GREEN, RESET);
}

/* -----------------------------------------------------------------
   createDescriptorPoolAndSet – fixed address-of
   ----------------------------------------------------------------- */
void VulkanRTX::createDescriptorPoolAndSet()
{
    std::array<VkDescriptorPoolSize, 5> poolSizes = {};
    poolSizes[0] = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 };
    poolSizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 };
    poolSizes[2] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };
    poolSizes[3] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 };
    poolSizes[4] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VkDescriptorPool pool;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool), "Create RTX descriptor pool");
    setDescriptorPool(pool);

    VkDescriptorSetLayout layout = dsLayout_.get();

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };

    VkDescriptorSet set;
    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, &set), "Allocate RTX descriptor set");
    setDescriptorSet(set);

    LOG_INFO_CAT("VulkanRTX", "{}Descriptor pool + set created{}", EMERALD_GREEN, RESET);
}

/* -----------------------------------------------------------------
   createRayTracingPipeline
   ----------------------------------------------------------------- */
void VulkanRTX::createRayTracingPipeline(uint32_t maxRayRecursionDepth)
{
    if (!pipelineMgr_) throw VulkanRTXException("Pipeline manager null");

    VkPipelineLayout layout = pipelineMgr_->getRayTracingPipelineLayout();
    if (layout == VK_NULL_HANDLE) throw VulkanRTXException("Pipeline layout missing");

    VkPipeline pipeline = pipelineMgr_->getRayTracingPipeline();
    if (pipeline == VK_NULL_HANDLE) throw VulkanRTXException("Ray tracing pipeline missing");

    setPipelineLayout(layout);
    setPipeline(pipeline);

    LOG_INFO_CAT("VulkanRTX", "{}Ray tracing pipeline bound{}", EMERALD_GREEN, RESET);
}

/* -----------------------------------------------------------------
   createShaderBindingTable – removed unused variable
   ----------------------------------------------------------------- */
void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice)
{
    if (!rtPipeline_.get()) throw VulkanRTXException("Pipeline not created");

    const uint32_t groupCount = 4;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

    const uint32_t handleSize    = rtProps.shaderGroupHandleSize;
    const uint32_t baseAlignment = rtProps.shaderGroupBaseAlignment;
    uint32_t sbtSize = groupCount * handleSize;
    sbtSize = (sbtSize + baseAlignment - 1) & ~(baseAlignment - 1);

    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device_, rtPipeline_.get(), 0, groupCount,
                                                  sbtSize, shaderHandleStorage.data()),
             "Get shader group handles");

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> sbtBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> sbtMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 sbtBuffer, sbtMemory);

    void* data;
    VK_CHECK(vkMapMemory(device_, sbtMemory.get(), 0, sbtSize, 0, &data), "Map SBT");
    std::memcpy(data, shaderHandleStorage.data(), sbtSize);
    vkUnmapMemory(device_, sbtMemory.get());

    VkBufferDeviceAddressInfo addrInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = sbtBuffer.get()
    };
    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device_, &addrInfo);

    sbt_ = ShaderBindingTable{
        .raygen   = { sbtAddress,                     handleSize, handleSize },
        .miss     = { sbtAddress + handleSize,        handleSize, handleSize },
        .hit      = { sbtAddress + 2*handleSize,      handleSize, handleSize },
        .callable = { sbtAddress + 3*handleSize,      handleSize, handleSize }
    };

    LOG_INFO_CAT("VulkanRTX", "{}SBT created @ 0x{:x}{}", EMERALD_GREEN, sbtAddress, RESET);
}

/* -----------------------------------------------------------------
   updateDescriptorSetForTLAS
   ----------------------------------------------------------------- */
void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas)
{
    if (tlas == VK_NULL_HANDLE || !ds_.get()) return;

    VkWriteDescriptorSetAccelerationStructureKHR asWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asWrite,
        .dstSet = ds_.get(),
        .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    LOG_INFO_CAT("VulkanRTX", "{}TLAS descriptor updated{}", EMERALD_GREEN, RESET);
}

/* -----------------------------------------------------------------
   initializeRTX
   ----------------------------------------------------------------- */
void VulkanRTX::initializeRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                              VkQueue graphicsQueue,
                              const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                              uint32_t maxRayRecursionDepth,
                              const std::vector<DimensionState>& dimensionCache)
{
    LOG_INFO_CAT("VulkanRTX", "{}initializeRTX() — full init{}", ARCTIC_CYAN, RESET);

    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);
    createTopLevelAS(physicalDevice, commandPool, graphicsQueue,
                     {{blas_.get(), glm::mat4(1.0f)}});

    createDescriptorPoolAndSet();

    createRayTracingPipeline(maxRayRecursionDepth);
    createShaderBindingTable(physicalDevice);
    updateDescriptorSetForTLAS(tlas_.get());
}

/* -----------------------------------------------------------------
   updateRTX
   ----------------------------------------------------------------- */
void VulkanRTX::updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                          VkQueue graphicsQueue,
                          const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                          const std::vector<DimensionState>& dimensionCache)
{
    LOG_INFO_CAT("VulkanRTX", "{}updateRTX() — rebuilding AS{}", AMBER_YELLOW, RESET);

    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);
    createTopLevelAS(physicalDevice, commandPool, graphicsQueue,
                     {{blas_.get(), glm::mat4(1.0f)}});
    updateDescriptorSetForTLAS(tlas_.get());
}

/* -----------------------------------------------------------------
   -----------------------------------------------------------------
   IMPLEMENTATIONS OF THE MISSING PRIVATE FUNCTIONS
   -----------------------------------------------------------------
   ----------------------------------------------------------------- */

/* -----------------------------------------------------------------
   setDescriptorSetLayout / setDescriptorPool / setDescriptorSet
   ----------------------------------------------------------------- */
void VulkanRTX::setDescriptorSetLayout(VkDescriptorSetLayout layout)
{
    dsLayout_ = {device_, layout, vkDestroyDescriptorSetLayout};
}
void VulkanRTX::setDescriptorPool(VkDescriptorPool pool)
{
    dsPool_ = {device_, pool, vkDestroyDescriptorPool};
}
void VulkanRTX::setDescriptorSet(VkDescriptorSet set)
{
    ds_ = VulkanDescriptorSet(device_, dsPool_.get(), set, vkFreeDescriptorSets);
}

/* -----------------------------------------------------------------
   setPipelineLayout / setPipeline
   ----------------------------------------------------------------- */
void VulkanRTX::setPipelineLayout(VkPipelineLayout layout)
{
    rtPipelineLayout_ = {device_, layout, vkDestroyPipelineLayout};
}
void VulkanRTX::setPipeline(VkPipeline pipeline)
{
    rtPipeline_ = {device_, pipeline, vkDestroyPipeline};
}

/* -----------------------------------------------------------------
   createBuffer – generic helper used everywhere
   ----------------------------------------------------------------- */
void VulkanRTX::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size,
                             VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                             VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
                             VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory)
{
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buf;
    VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &buf), "Create buffer");
    buffer = {device_, buf, vkDestroyBuffer};

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, buf, &memReqs);

    uint32_t memType = findMemoryType(physicalDevice, memReqs.memoryTypeBits, properties);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory mem;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &mem), "Allocate buffer memory");
    memory = {device_, mem, vkFreeMemory};

    VK_CHECK(vkBindBufferMemory(device_, buf, mem, 0), "Bind buffer memory");
}

/* -----------------------------------------------------------------
   findMemoryType – helper for createBuffer
   ----------------------------------------------------------------- */
uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                                   VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw VulkanRTXException("Failed to find suitable memory type");
}

/* -----------------------------------------------------------------
   Explicit template instantiations
   ----------------------------------------------------------------- */
template class VulkanResource<VkBuffer, PFN_vkDestroyBuffer>;
template class VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>;
template class VulkanResource<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout>;
template class VulkanResource<VkDescriptorPool, PFN_vkDestroyDescriptorPool>;
template class VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>;
template class VulkanResource<VkPipeline, PFN_vkDestroyPipeline>;
template class VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>;
template class VulkanResource<VkImage, PFN_vkDestroyImage>;
template class VulkanResource<VkImageView, PFN_vkDestroyImageView>;

} // namespace VulkanRTX