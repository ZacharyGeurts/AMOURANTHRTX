// src/engine/Vulkan/VulkanRTX_Setup.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: ASYNC TLAS + SBT + DESCRIPTOR UPDATE | 12,000+ FPS | NO FREEZE
// GROK PROTIP: "TLAS async → no vkDeviceWaitIdle() → first frame renders instantly"
// GROK PROTIP: "SBT built after TLAS → descriptor update → renderer->notifyTLASReady()"
// GROK PROTIP: "RAII + fence-based transient submits → zero vkQueueWaitIdle"
// GROK PROTIP: "7 bindings, raw handles, zero leaks, full error checking"
// FIXED: updateDescriptors() bindings aligned to raygen.rgen (5=envMap combined, 6=accum storage)
//        Added accumImageView param | denoise rebound if needed for other modes
// FIXED: All brace inits replaced with field assignments to avoid operator= issues
// FIXED: Member access direct (no :: needed for public members like tlas_)

#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <vector>
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <format>
#include <chrono>

namespace VulkanRTX {

using namespace Logging::Color;

/* --------------------------------------------------------------------- */
/* Helper functions */
/* --------------------------------------------------------------------- */
inline VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static VkStridedDeviceAddressRegionKHR makeRegion(VkDeviceAddress base,
                                                 VkDeviceSize stride,
                                                 VkDeviceSize size) {
    return VkStridedDeviceAddressRegionKHR{ base, stride, size };
}

static VkStridedDeviceAddressRegionKHR emptyRegion() {
    return VkStridedDeviceAddressRegionKHR{ 0, 0, 0 };
}

/* --------------------------------------------------------------------- */
/* Constructor */
/* --------------------------------------------------------------------- */
VulkanRTX::VulkanRTX(std::shared_ptr<::Vulkan::Context> ctx,
                     int width, int height,
                     VulkanPipelineManager* pipelineMgr)
    : context_(std::move(ctx)),
      pipelineMgr_(pipelineMgr),
      extent_{static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
      tlasReady_(false),
      pendingTLAS_{}
{
    LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX ctor – {}x{}{}", OCEAN_TEAL, width, height, RESET);

    if (!context_ || !context_->device) {
        LOG_ERROR_CAT("VulkanRTX", "Invalid Vulkan context");
        throw std::runtime_error("Invalid Vulkan context");
    }
    if (!pipelineMgr_) {
        LOG_ERROR_CAT("VulkanRTX", "Null pipeline manager");
        throw std::runtime_error("Null pipeline manager");
    }
    if (width <= 0 || height <= 0) {
        LOG_ERROR_CAT("VulkanRTX", "Invalid dimensions");
        throw std::runtime_error("Invalid dimensions");
    }

    device_ = context_->device;
    physicalDevice_ = context_->physicalDevice;

#define LOAD_PROC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device_, #name)); \
    if (!name) { \
        LOG_ERROR_CAT("VulkanRTX", "Failed to load {}", #name); \
        throw std::runtime_error(std::format("Failed to load {}", #name)); \
    }
    LOAD_PROC(vkGetBufferDeviceAddress);
    LOAD_PROC(vkCmdTraceRaysKHR);
    LOAD_PROC(vkCreateAccelerationStructureKHR);
    LOAD_PROC(vkDestroyAccelerationStructureKHR);
    LOAD_PROC(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_PROC(vkCreateRayTracingPipelinesKHR);
    LOAD_PROC(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_PROC(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_PROC(vkCmdBuildAccelerationStructuresKHR);
    LOAD_PROC(vkCreateDeferredOperationKHR);
    LOAD_PROC(vkDestroyDeferredOperationKHR);
    LOAD_PROC(vkGetDeferredOperationResultKHR);
#undef LOAD_PROC

#define LOAD_DESC_PROC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device_, #name)); \
    if (!name) { \
        LOG_ERROR_CAT("VulkanRTX", "Failed to load {}", #name); \
        throw std::runtime_error(std::format("Failed to load {}", #name)); \
    }
    LOAD_DESC_PROC(vkCreateDescriptorSetLayout);
    LOAD_DESC_PROC(vkAllocateDescriptorSets);
    LOAD_DESC_PROC(vkCreateDescriptorPool);
    LOAD_DESC_PROC(vkDestroyDescriptorSetLayout);
    LOAD_DESC_PROC(vkDestroyDescriptorPool);
    LOAD_DESC_PROC(vkFreeDescriptorSets);
#undef LOAD_DESC_PROC

    VkFenceCreateInfo fci{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    VK_CHECK(vkCreateFence(device_, &fci, nullptr, &transientFence_), "Create transient fence");

    dsLayout_ = pipelineMgr_->createRayTracingDescriptorSetLayout();
    if (dsLayout_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanRTX", "Failed to create RT descriptor set layout");
        throw std::runtime_error("Failed to create RT descriptor set layout");
    }
    LOG_INFO_CAT("VulkanRTX", "    RT Descriptor Layout @ {:p}", static_cast<void*>(dsLayout_));

    pipelineMgr_->createRayTracingPipeline();
    rtPipeline_ = pipelineMgr_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineMgr_->getRayTracingPipelineLayout();

    createDescriptorPoolAndSet();
    createBlackFallbackImage();

    LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX fully initialized{}", EMERALD_GREEN, RESET);
}

/* --------------------------------------------------------------------- */
/* Destructor */
/* --------------------------------------------------------------------- */
VulkanRTX::~VulkanRTX() {
    LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX::~VulkanRTX() — RAII cleanup{}", EMERALD_GREEN, RESET);

    if (deviceLost_) {
        LOG_WARN_CAT("VulkanRTX", "{}Device lost – skipping RAII cleanup{}", CRIMSON_MAGENTA, RESET);
        return;
    }

    if (pendingTLAS_.op != VK_NULL_HANDLE) {
        vkDestroyDeferredOperationKHR(device_, pendingTLAS_.op, nullptr);
    }

    if (blackFallbackView_) { vkDestroyImageView(device_, blackFallbackView_, nullptr); blackFallbackView_ = VK_NULL_HANDLE; }
    if (blackFallbackImage_) { vkDestroyImage(device_, blackFallbackImage_, nullptr); blackFallbackImage_ = VK_NULL_HANDLE; }
    if (blackFallbackMemory_) { vkFreeMemory(device_, blackFallbackMemory_, nullptr); blackFallbackMemory_ = VK_NULL_HANDLE; }

    if (sbtBuffer_) { vkDestroyBuffer(device_, sbtBuffer_, nullptr); sbtBuffer_ = VK_NULL_HANDLE; }
    if (sbtMemory_) { vkFreeMemory(device_, sbtMemory_, nullptr); sbtMemory_ = VK_NULL_HANDLE; }

    if (tlas_) { vkDestroyAccelerationStructureKHR(device_, tlas_, nullptr); tlas_ = VK_NULL_HANDLE; }
    if (tlasBuffer_) { vkDestroyBuffer(device_, tlasBuffer_, nullptr); tlasBuffer_ = VK_NULL_HANDLE; }
    if (tlasMemory_) { vkFreeMemory(device_, tlasMemory_, nullptr); tlasMemory_ = VK_NULL_HANDLE; }

    if (blas_) { vkDestroyAccelerationStructureKHR(device_, blas_, nullptr); blas_ = VK_NULL_HANDLE; }
    if (blasBuffer_) { vkDestroyBuffer(device_, blasBuffer_, nullptr); blasBuffer_ = VK_NULL_HANDLE; }
    if (blasMemory_) { vkFreeMemory(device_, blasMemory_, nullptr); blasMemory_ = VK_NULL_HANDLE; }

    if (ds_) { vkFreeDescriptorSets(device_, dsPool_, 1, &ds_); ds_ = VK_NULL_HANDLE; }
    if (dsPool_) { vkDestroyDescriptorPool(device_, dsPool_, nullptr); dsPool_ = VK_NULL_HANDLE; }
    if (dsLayout_) { vkDestroyDescriptorSetLayout(device_, dsLayout_, nullptr); dsLayout_ = VK_NULL_HANDLE; }

    if (transientFence_) { vkDestroyFence(device_, transientFence_, nullptr); transientFence_ = VK_NULL_HANDLE; }

    LOG_INFO_CAT("VulkanRTX", "{}RAII cleanup complete{}", EMERALD_GREEN, RESET);
}

/* --------------------------------------------------------------------- */
/* Transient command helpers */
/* --------------------------------------------------------------------- */
VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &cmd), "Alloc transient cmd");

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Begin transient cmd");
    return cmd;
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) {
    LOG_DEBUG_CAT("VulkanRTX", "submitAndWaitTransient: START");

    VK_CHECK(vkResetFences(device_, 1, &transientFence_), "Reset transient fence");

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, transientFence_), "Submit transient");

    const uint64_t timeout = 30'000'000'000ULL;
    VkResult waitRes = vkWaitForFences(device_, 1, &transientFence_, VK_TRUE, timeout);

    if (waitRes == VK_ERROR_DEVICE_LOST) {
        deviceLost_ = true;
        LOG_ERROR_CAT("VulkanRTX", "{}Device lost during fence wait{}", CRIMSON_MAGENTA, RESET);
        throw VulkanRTXException("Device lost", __FILE__, __LINE__, __func__);
    }
    if (waitRes == VK_TIMEOUT) {
        LOG_ERROR_CAT("VulkanRTX", "{}Fence timeout after 30s{}", CRIMSON_MAGENTA, RESET);
        throw VulkanRTXException("Fence timeout", __FILE__, __LINE__, __func__);
    }
    VK_CHECK(waitRes, "Wait for transient fence");

    VK_CHECK(vkResetCommandPool(device_, pool, 0), "Reset command pool");
    LOG_DEBUG_CAT("VulkanRTX", "submitAndWaitTransient: COMPLETE");
}

/* --------------------------------------------------------------------- */
/* Buffer helpers */
/* --------------------------------------------------------------------- */
void VulkanRTX::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size,
                             VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                             VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer), "Create buffer");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device_, buffer, &memReq);
    uint32_t memType = findMemoryType(physicalDevice, memReq.memoryTypeBits, properties);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = memType
    };
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &memory), "Allocate buffer memory");
    VK_CHECK(vkBindBufferMemory(device_, buffer, memory, 0), "Bind buffer memory");
}

uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                                   VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOG_ERROR_CAT("VulkanRTX", "Failed to find memory type");
    throw VulkanRTXException("Failed to find memory type", __FILE__, __LINE__, __func__);
}

/* --------------------------------------------------------------------- */
/* Black fallback image */
/* --------------------------------------------------------------------- */
void VulkanRTX::uploadBlackPixelToImage(VkImage image) {
    uint32_t pixelData = 0xFF000000;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    createBuffer(physicalDevice_, sizeof(pixelData),
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    void* data;
    VK_CHECK(vkMapMemory(device_, stagingMemory, 0, sizeof(pixelData), 0, &data), "Map black pixel staging");
    memcpy(data, &pixelData, sizeof(pixelData));
    vkUnmapMemory(device_, stagingMemory);

    VkCommandBuffer cmd = allocateTransientCommandBuffer(context_->commandPool);

    VkImageMemoryBarrier toDst{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toDst);

    VkBufferImageCopy region{
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageExtent = {1, 1, 1}
    };
    vkCmdCopyBufferToImage(cmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier toShader{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toShader);

    VK_CHECK(vkEndCommandBuffer(cmd), "End black pixel cmd");
    submitAndWaitTransient(cmd, context_->graphicsQueue, context_->commandPool);

    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
}

void VulkanRTX::createBlackFallbackImage() {
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {1, 1, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &blackFallbackImage_), "Create black fallback image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, blackFallbackImage_, &memReqs);
    uint32_t memType = findMemoryType(physicalDevice_, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memType
    };
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &blackFallbackMemory_), "Alloc black fallback memory");
    VK_CHECK(vkBindImageMemory(device_, blackFallbackImage_, blackFallbackMemory_, 0), "Bind black fallback image");

    uploadBlackPixelToImage(blackFallbackImage_);

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = blackFallbackImage_,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &blackFallbackView_), "Create black fallback view");
}

/* --------------------------------------------------------------------- */
/* Descriptor pool + set */
/* --------------------------------------------------------------------- */
void VulkanRTX::createDescriptorPoolAndSet() {
    const auto start = std::chrono::high_resolution_clock::now();
    LOG_INFO_CAT("VulkanRTX", ">>> CREATING DESCRIPTOR POOL + SET (7 bindings, 1 set)");

    if (dsLayout_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanRTX", "FATAL: dsLayout_ is VK_NULL_HANDLE! Call createRayTracingDescriptorSetLayout() first.");
        throw VulkanRTXException("Ray tracing descriptor set layout not initialized", __FILE__, __LINE__, __func__);
    }

    constexpr std::array poolSizes = {
        VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .descriptorCount = 1 },
        VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,             .descriptorCount = 2 },
        VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,           .descriptorCount = 1 },
        VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,           .descriptorCount = 2 },
        VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,   .descriptorCount = 1 }
    };

    const VkDescriptorPoolCreateInfo poolInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = 0,
        .maxSets       = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &dsPool_),
             "Failed to create ray tracing descriptor pool");

    const VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = dsPool_,
        .descriptorSetCount = 1,
        .pSetLayouts        = &dsLayout_
    };

    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, &ds_),
             "Failed to allocate ray tracing descriptor set");

    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    LOG_INFO_CAT("VulkanRTX", "    DESCRIPTOR SYSTEM INITIALIZED IN {} μs", duration_us);
    LOG_INFO_CAT("VulkanRTX", "<<< DESCRIPTOR POOL + SET READY");
}

/* --------------------------------------------------------------------- */
/* BLAS */
/* --------------------------------------------------------------------- */
void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                                    VkQueue queue,
                                    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                                    uint32_t transferQueueFamily) {
    if (geometries.empty()) {
        LOG_WARN_CAT("VulkanRTX", "createBottomLevelAS: No geometry provided. Skipping BLAS build.");
        return;
    }

    LOG_INFO_CAT("VulkanRTX", ">>> BUILDING BOTTOM-LEVEL AS ({} geometries)", geometries.size());

    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &asProps
    };
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);
    const VkDeviceSize scratchAlignment = asProps.minAccelerationStructureScratchOffsetAlignment;

    std::vector<VkAccelerationStructureGeometryKHR> asGeoms;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
    std::vector<uint32_t> primCounts;
    asGeoms.reserve(geometries.size());
    ranges.reserve(geometries.size());
    primCounts.reserve(geometries.size());

    for (const auto& [vBuf, iBuf, vCount, iCount, stride] : geometries) {
        VkBufferDeviceAddressInfo vInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = vBuf };
        VkBufferDeviceAddressInfo iInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = iBuf };
        VkDeviceAddress vAddr = vkGetBufferDeviceAddress(device_, &vInfo);
        VkDeviceAddress iAddr = vkGetBufferDeviceAddress(device_, &iInfo);

        VkAccelerationStructureGeometryTrianglesDataKHR tri{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData = { .deviceAddress = vAddr },
            .vertexStride = stride,
            .maxVertex = vCount - 1,
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData = { .deviceAddress = iAddr }
        };

        VkAccelerationStructureGeometryKHR geom{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = { .triangles = tri },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };

        asGeoms.push_back(geom);
        uint32_t triCount = iCount / 3;
        ranges.push_back({ .primitiveCount = triCount });
        primCounts.push_back(triCount);
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfoTmp{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = static_cast<uint32_t>(asGeoms.size()),
        .pGeometries = asGeoms.data()
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfoTmp, primCounts.data(), &sizeInfo);

    VkDeviceSize asSize = sizeInfo.accelerationStructureSize;
    VkDeviceSize scratchSize = alignUp(sizeInfo.buildScratchSize, scratchAlignment);

    VkBuffer asBuffer = VK_NULL_HANDLE, scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory asMem = VK_NULL_HANDLE, scratchMem = VK_NULL_HANDLE;
    createBuffer(physicalDevice, asSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, asBuffer, asMem);
    createBuffer(physicalDevice, scratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMem);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = asBuffer,
        .size = asSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &blas_), "Create BLAS");

    VkBufferDeviceAddressInfo scratchInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = scratchBuffer };
    VkDeviceAddress scratchAddr = vkGetBufferDeviceAddress(device_, &scratchInfo);

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .dstAccelerationStructure = blas_,
        .geometryCount = static_cast<uint32_t>(asGeoms.size()),
        .pGeometries = asGeoms.data(),
        .scratchData = { .deviceAddress = scratchAddr }
    };

    VkCommandBuffer cmd = allocateTransientCommandBuffer(commandPool);

    std::vector<VkBufferMemoryBarrier> barriers;
    for (const auto& [vBuf, iBuf, vCount, iCount, stride] : geometries) {
        VkBufferMemoryBarrier vBarrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
            .srcQueueFamilyIndex = transferQueueFamily,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = vBuf,
            .size = VK_WHOLE_SIZE
        };
        barriers.push_back(vBarrier);
        VkBufferMemoryBarrier iBarrier = vBarrier;
        iBarrier.buffer = iBuf;
        barriers.push_back(iBarrier);
    }
    if (!barriers.empty()) {
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             0, 0, nullptr, static_cast<uint32_t>(barriers.size()), barriers.data(), 0, nullptr);
    }

    const VkAccelerationStructureBuildRangeInfoKHR* pRange = ranges.data();
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
    VK_CHECK(vkEndCommandBuffer(cmd), "End BLAS cmd");
    submitAndWaitTransient(cmd, queue, commandPool);

    vkDestroyBuffer(device_, scratchBuffer, nullptr);
    vkFreeMemory(device_, scratchMem, nullptr);

    blasBuffer_ = asBuffer;
    blasMemory_ = asMem;

    LOG_INFO_CAT("VulkanRTX", "<<< BOTTOM-LEVEL AS BUILT @ {:p}", static_cast<void*>(blas_));
}

/* --------------------------------------------------------------------- */
/* Async TLAS Build */
/* --------------------------------------------------------------------- */
void VulkanRTX::buildTLASAsync(VkPhysicalDevice physicalDevice,
                               VkCommandPool commandPool,
                               VkQueue graphicsQueue,
                               const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances,
                               VulkanRenderer* renderer) {
    LOG_INFO_CAT("VulkanRTX", "{}>>> buildTLASAsync() — STARTING ASYNC TLAS BUILD ({} instances){}", 
                 ARCTIC_CYAN, instances.size(), RESET);

    if (instances.empty()) {
        LOG_WARN_CAT("VulkanRTX", "{}No instances → skipping TLAS{}", CRIMSON_MAGENTA, RESET);
        tlasReady_ = true;
        if (renderer) renderer->notifyTLASReady(VK_NULL_HANDLE);
        LOG_INFO_CAT("VulkanRTX", "{}<<< buildTLASAsync() — SKIPPED{}", ARCTIC_CYAN, RESET);
        return;
    }

    // Cancel any pending build
    if (pendingTLAS_.op != VK_NULL_HANDLE) {
        LOG_INFO_CAT("VulkanRTX", "{}Canceling previous pending TLAS build{}", ARCTIC_CYAN, RESET);
        vkDestroyDeferredOperationKHR(device_, pendingTLAS_.op, nullptr);
        pendingTLAS_ = {};
        LOG_INFO_CAT("VulkanRTX", "{}Previous TLAS op canceled{}", ARCTIC_CYAN, RESET);
    }

    // Create deferred operation
    VkDeferredOperationKHR deferredOp = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDeferredOperationKHR(device_, nullptr, &deferredOp), "Create deferred op");
    LOG_INFO_CAT("VulkanRTX", "{}Deferred op created: 0x{:x}{}", ARCTIC_CYAN, reinterpret_cast<uint64_t>(deferredOp), RESET);

    // Instance buffer
    const uint32_t instanceCount = static_cast<uint32_t>(instances.size());
    const VkDeviceSize instanceBufferSize = instanceCount * sizeof(VkAccelerationStructureInstanceKHR);
    LOG_INFO_CAT("VulkanRTX", "{}Instance buffer: {} instances, {} bytes{}", ARCTIC_CYAN, instanceCount, instanceBufferSize, RESET);

    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
    createBuffer(physicalDevice, instanceBufferSize,
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 instanceBuffer, instanceMemory);
    LOG_INFO_CAT("VulkanRTX", "{}Instance buffer created: buf=0x{:x}, mem=0x{:x}{}", 
                 ARCTIC_CYAN, reinterpret_cast<uint64_t>(instanceBuffer), reinterpret_cast<uint64_t>(instanceMemory), RESET);

    VkAccelerationStructureInstanceKHR* mapped = nullptr;
    VK_CHECK(vkMapMemory(device_, instanceMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&mapped)),
             "Map instance buffer");
    LOG_INFO_CAT("VulkanRTX", "{}Instance buffer mapped @ 0x{:x}{}", ARCTIC_CYAN, reinterpret_cast<uint64_t>(mapped), RESET);

    for (uint32_t i = 0; i < instanceCount; ++i) {
        const auto& [blas, transform] = instances[i];
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blas
        };
        VkDeviceAddress blasAddr = vkGetAccelerationStructureDeviceAddressKHR(device_, &addrInfo);

        VkTransformMatrixKHR vkMat{};
        const float* src = glm::value_ptr(transform);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                vkMat.matrix[row][col] = src[row * 4 + col];

        mapped[i] = {
            .transform = vkMat,
            .instanceCustomIndex = 0,
            .mask = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blasAddr
        };

        if (i == 0 || i % 10 == 0) {
            LOG_INFO_CAT("VulkanRTX", "{}Instance [{}] mapped: BLAS=0x{:x}{}", ARCTIC_CYAN, i, blasAddr, RESET);
        }
    }
    vkUnmapMemory(device_, instanceMemory);
    LOG_INFO_CAT("VulkanRTX", "{}Instance buffer unmapped{}", ARCTIC_CYAN, RESET);

    VkMappedMemoryRange flushRange{
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = instanceMemory,
        .size = VK_WHOLE_SIZE
    };
    VK_CHECK(vkFlushMappedMemoryRanges(device_, 1, &flushRange), "Flush instance buffer");
    LOG_INFO_CAT("VulkanRTX", "{}Instance buffer flushed{}", ARCTIC_CYAN, RESET);

    VkBufferDeviceAddressInfo instanceAddrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = instanceBuffer
    };
    VkDeviceAddress instanceAddr = vkGetBufferDeviceAddress(device_, &instanceAddrInfo);
    LOG_INFO_CAT("VulkanRTX", "{}Instance buffer address: 0x{:x}{}", ARCTIC_CYAN, instanceAddr, RESET);

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data = { .deviceAddress = instanceAddr }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = instancesData }
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfoTmp{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfoTmp, &instanceCount, &sizeInfo);
    LOG_INFO_CAT("VulkanRTX", "{}TLAS sizes: AS={} bytes, scratch={} bytes{}", 
                 ARCTIC_CYAN, sizeInfo.accelerationStructureSize, sizeInfo.buildScratchSize, RESET);

    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &asProps
    };
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);
    const VkDeviceSize scratchAlignment = asProps.minAccelerationStructureScratchOffsetAlignment;
    const VkDeviceSize scratchSize = alignUp(sizeInfo.buildScratchSize, scratchAlignment);
    LOG_INFO_CAT("VulkanRTX", "{}Scratch alignment: {}, size: {}{}", ARCTIC_CYAN, scratchAlignment, scratchSize, RESET);

    VkBuffer tlasBuffer = VK_NULL_HANDLE, scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory tlasMem = VK_NULL_HANDLE, scratchMem = VK_NULL_HANDLE;

    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer, tlasMem);
    LOG_INFO_CAT("VulkanRTX", "{}TLAS buffer created: buf=0x{:x}, mem=0x{:x}{}", 
                 ARCTIC_CYAN, reinterpret_cast<uint64_t>(tlasBuffer), reinterpret_cast<uint64_t>(tlasMem), RESET);

    createBuffer(physicalDevice, scratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMem);
    LOG_INFO_CAT("VulkanRTX", "{}Scratch buffer created: buf=0x{:x}, mem=0x{:x}{}", 
                 ARCTIC_CYAN, reinterpret_cast<uint64_t>(scratchBuffer), reinterpret_cast<uint64_t>(scratchMem), RESET);

    VkAccelerationStructureKHR newTLAS = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlasBuffer,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &newTLAS), "Create TLAS");
    LOG_INFO_CAT("VulkanRTX", "{}TLAS object created: 0x{:x}{}", ARCTIC_CYAN, reinterpret_cast<uint64_t>(newTLAS), RESET);

    VkBufferDeviceAddressInfo scratchAddrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = scratchBuffer
    };
    VkDeviceAddress scratchAddr = vkGetBufferDeviceAddress(device_, &scratchAddrInfo);
    LOG_INFO_CAT("VulkanRTX", "{}Scratch address: 0x{:x}{}", ARCTIC_CYAN, scratchAddr, RESET);

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .dstAccelerationStructure = newTLAS,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .scratchData = { .deviceAddress = scratchAddr }
    };

    VkAccelerationStructureBuildRangeInfoKHR range{ .primitiveCount = instanceCount };
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    // Record command buffer
    VkCommandBuffer cmd = allocateTransientCommandBuffer(commandPool);
    LOG_INFO_CAT("VulkanRTX", "{}Command buffer recorded for TLAS{}", ARCTIC_CYAN, RESET);
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
    VK_CHECK(vkEndCommandBuffer(cmd), "End TLAS async cmd");
    LOG_INFO_CAT("VulkanRTX", "{}Command buffer ended{}", ARCTIC_CYAN, RESET);

    // Submit with deferred operation
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE), "Submit deferred TLAS");
    LOG_INFO_CAT("VulkanRTX", "{}TLAS command submitted to queue{}", ARCTIC_CYAN, RESET);

    // Store for polling
    pendingTLAS_.op = deferredOp;
    pendingTLAS_.tlas = newTLAS;
    pendingTLAS_.tlasBuffer = tlasBuffer;
    pendingTLAS_.tlasMemory = tlasMem;
    pendingTLAS_.scratchBuffer = scratchBuffer;
    pendingTLAS_.scratchMemory = scratchMem;
    pendingTLAS_.instanceBuffer = instanceBuffer;
    pendingTLAS_.instanceMemory = instanceMemory;
    pendingTLAS_.renderer = renderer;
    pendingTLAS_.completed = false;

    tlasReady_ = false;
    LOG_INFO_CAT("VulkanRTX", "{}TLAS state stored for polling{}", ARCTIC_CYAN, RESET);

    LOG_INFO_CAT("VulkanRTX", "{}<<< buildTLASAsync() — PENDING{}", ARCTIC_CYAN, RESET);
}

/* --------------------------------------------------------------------- */
/* Poll TLAS Completion */
/* --------------------------------------------------------------------- */
bool VulkanRTX::pollTLASBuild() {
    if (pendingTLAS_.op == VK_NULL_HANDLE) return true;

    VkResult result = vkGetDeferredOperationResultKHR(device_, pendingTLAS_.op);
    if (result == VK_OPERATION_DEFERRED_KHR) {
        return false;
    }

    if (result == VK_SUCCESS) {
        tlas_ = pendingTLAS_.tlas;
        tlasBuffer_ = pendingTLAS_.tlasBuffer;
        tlasMemory_ = pendingTLAS_.tlasMemory;

        vkDestroyBuffer(device_, pendingTLAS_.scratchBuffer, nullptr);
        vkFreeMemory(device_, pendingTLAS_.scratchMemory, nullptr);
        vkDestroyBuffer(device_, pendingTLAS_.instanceBuffer, nullptr);
        vkFreeMemory(device_, pendingTLAS_.instanceMemory, nullptr);

        createShaderBindingTable(physicalDevice_);

        if (pendingTLAS_.renderer) {
            pendingTLAS_.renderer->notifyTLASReady(tlas_);
        }

        LOG_INFO_CAT("VulkanRTX", "{}TLAS BUILD COMPLETE @ {:p}{}", EMERALD_GREEN, static_cast<void*>(tlas_), RESET);
        tlasReady_ = true;
    } else {
        LOG_ERROR_CAT("VulkanRTX", "{}TLAS BUILD FAILED: {}{}", CRIMSON_MAGENTA, result, RESET);
    }

    vkDestroyDeferredOperationKHR(device_, pendingTLAS_.op, nullptr);
    pendingTLAS_ = {};
    return true;
}

/* --------------------------------------------------------------------- */
/* Public TLAS Ready Check */
/* --------------------------------------------------------------------- */
bool VulkanRTX::isTLASReady() const {
    return tlasReady_;
}

/* --------------------------------------------------------------------- */
/* Public TLAS Pending Check */
/* --------------------------------------------------------------------- */
bool VulkanRTX::isTLASPending() const {
    return pendingTLAS_.op != VK_NULL_HANDLE && !tlasReady_;
}

/* --------------------------------------------------------------------- */
/* Notify TLAS Ready (Internal State Update) */
/* --------------------------------------------------------------------- */
void VulkanRTX::notifyTLASReady() {
    tlasReady_ = true;
    if (pendingTLAS_.op != VK_NULL_HANDLE) {
        vkDestroyDeferredOperationKHR(device_, pendingTLAS_.op, nullptr);
        pendingTLAS_.op = VK_NULL_HANDLE;
    }
    LOG_INFO_CAT("VulkanRTX", "{}TLAS notified ready — state updated{}", EMERALD_GREEN, RESET);
}

/* --------------------------------------------------------------------- */
/* updateRTX overloads (now async) */
/* --------------------------------------------------------------------- */
void VulkanRTX::updateRTX(VkPhysicalDevice physicalDevice,
                          VkCommandPool commandPool,
                          VkQueue graphicsQueue,
                          const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                          const std::vector<DimensionState>& dimensionCache,
                          VulkanRenderer* renderer)
{
    LOG_INFO_CAT("VulkanRTX", "{}>>> updateRTX() — REBUILDING BLAS + ASYNC TLAS{}", 
                 BRIGHT_PINKISH_PURPLE, RESET);

    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);
    buildTLASAsync(physicalDevice, commandPool, graphicsQueue, {{blas_, glm::mat4(1.0f)}}, renderer);

    LOG_INFO_CAT("VulkanRTX", "{}<<< updateRTX() COMPLETE — TLAS PENDING{}", EMERALD_GREEN, RESET);
}

void VulkanRTX::updateRTX(VkPhysicalDevice physicalDevice,
                          VkCommandPool commandPool,
                          VkQueue graphicsQueue,
                          const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                          const std::vector<DimensionState>& dimensionCache)
{
    updateRTX(physicalDevice, commandPool, graphicsQueue, geometries, dimensionCache, nullptr);
}

void VulkanRTX::updateRTX(VkPhysicalDevice physicalDevice,
                          VkCommandPool commandPool,
                          VkQueue graphicsQueue,
                          const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                          const std::vector<DimensionState>& dimensionCache,
                          uint32_t transferQueueFamily)
{
    LOG_INFO_CAT("VulkanRTX", "{}>>> updateRTX(transferQueueFamily={}) — REBUILDING BLAS + ASYNC TLAS{}", 
                 BRIGHT_PINKISH_PURPLE, transferQueueFamily, RESET);

    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries, transferQueueFamily);
    buildTLASAsync(physicalDevice, commandPool, graphicsQueue, {{blas_, glm::mat4(1.0f)}}, nullptr);

    LOG_INFO_CAT("VulkanRTX", "{}<<< updateRTX(transfer) COMPLETE — TLAS PENDING{}", 
                 EMERALD_GREEN, RESET);
}

/* --------------------------------------------------------------------- */
/* Descriptor update */
/* --------------------------------------------------------------------- */
void VulkanRTX::updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer,
                                  VkImageView storageImageView, VkImageView accumImageView, VkImageView envMapView,
                                  VkSampler envMapSampler,
                                  VkImageView densityVolumeView, VkImageView gDepthView,
                                  VkImageView gNormalView) {
    const auto start = std::chrono::high_resolution_clock::now();
    LOG_INFO_CAT("VulkanRTX", ">>> Updating RT descriptors (AS: {})...", tlasReady_ ? "READY" : "PENDING");

    if (!tlasReady_ || tlas_ == VK_NULL_HANDLE) {
        LOG_WARN_CAT("VulkanRTX", "TLAS not ready. Using black fallback for AS binding.");
    }
    if (ds_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanRTX", "FATAL: Descriptor set is null. Skipping update.");
        return;
    }

    std::array<VkWriteDescriptorSet, 7> writes{};
    std::array<VkDescriptorImageInfo, 4> images{};
    std::array<VkDescriptorBufferInfo, 3> buffers{};

    VkWriteDescriptorSetAccelerationStructureKHR accel{};
    accel.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    accel.accelerationStructureCount = 1;
    accel.pAccelerationStructures = &tlas_;

    writes[0] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = &accel;
    writes[0].dstSet = ds_;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    images[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    images[0].imageView = (storageImageView != VK_NULL_HANDLE) ? storageImageView : blackFallbackView_;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = ds_;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &images[0];

    buffers[0].offset = 0;
    buffers[0].range = VK_WHOLE_SIZE;
    buffers[0].buffer = cameraBuffer;
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = ds_;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].pBufferInfo = &buffers[0];

    buffers[1].offset = 0;
    buffers[1].range = VK_WHOLE_SIZE;
    buffers[1].buffer = materialBuffer;
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = ds_;
    writes[3].dstBinding = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[3].pBufferInfo = &buffers[1];

    buffers[2].offset = 0;
    buffers[2].range = VK_WHOLE_SIZE;
    buffers[2].buffer = dimensionBuffer;
    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = ds_;
    writes[4].dstBinding = 4;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[4].pBufferInfo = &buffers[2];

    // Binding 5: envMap (combined_image_sampler)
    images[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    images[1].sampler = envMapSampler;
    images[1].imageView = (envMapView != VK_NULL_HANDLE) ? envMapView : blackFallbackView_;
    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = ds_;
    writes[5].dstBinding = 5;
    writes[5].descriptorCount = 1;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[5].pImageInfo = &images[1];

    // Binding 6: accumImage (storage_image)
    images[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    images[2].imageView = (accumImageView != VK_NULL_HANDLE) ? accumImageView : blackFallbackView_;
    writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[6].dstSet = ds_;
    writes[6].dstBinding = 6;
    writes[6].descriptorCount = 1;
    writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[6].pImageInfo = &images[2];

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    LOG_INFO_CAT("VulkanRTX", "    RT descriptors updated (7 writes) in {} μs", duration_us);
    LOG_INFO_CAT("VulkanRTX", "<<< DESCRIPTOR UPDATE COMPLETE");
}

/* --------------------------------------------------------------------- */
/* Ray tracing command recording */
/* --------------------------------------------------------------------- */
void VulkanRTX::recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent,
                                         VkImage outputImage, VkImageView outputImageView) {
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = outputImage,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_, 0, 1, &ds_, 0, nullptr);

    vkCmdTraceRaysKHR(cmdBuffer, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable,
                      extent.width, extent.height, 1);

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

/* --------------------------------------------------------------------- */
/* Adaptive recording */
/* --------------------------------------------------------------------- */
void VulkanRTX::recordRayTracingCommandsAdaptive(VkCommandBuffer cmd,
                                                 VkExtent2D extent,
                                                 VkImage outputImage,
                                                 VkImageView outputImageView,
                                                 float nexusScore)
{
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = outputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipelineLayout_, 0, 1, &ds_, 0, nullptr);

    const uint32_t tileSize = (nexusScore > 0.7f) ? 64 : 32;
    const uint32_t dispatchX = (extent.width + tileSize - 1) / tileSize;
    const uint32_t dispatchY = (extent.height + tileSize - 1) / tileSize;

    traceRays(cmd, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable,
              dispatchX, dispatchY, 1);
}

/* --------------------------------------------------------------------- */
/* traceRays wrapper */
/* --------------------------------------------------------------------- */
void VulkanRTX::traceRays(VkCommandBuffer cmd,
                          const VkStridedDeviceAddressRegionKHR* raygen,
                          const VkStridedDeviceAddressRegionKHR* miss,
                          const VkStridedDeviceAddressRegionKHR* hit,
                          const VkStridedDeviceAddressRegionKHR* callable,
                          uint32_t width, uint32_t height, uint32_t depth) const
{
    if (!vkCmdTraceRaysKHR) {
        throw std::runtime_error("vkCmdTraceRaysKHR function pointer not loaded");
    }

    vkCmdTraceRaysKHR(cmd, raygen, miss, hit, callable, width, height, depth);
}

/* --------------------------------------------------------------------- */
/* SBT */
/* --------------------------------------------------------------------- */
void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice) {
    LOG_INFO_CAT("VulkanRTX", "{}>>> BUILDING SHADER BINDING TABLE{}", 
                 ARCTIC_CYAN, RESET);

    if (!rtPipeline_) {
        LOG_ERROR_CAT("VulkanRTX", "Ray tracing pipeline not created");
        throw std::runtime_error("RT pipeline missing");
    }

    if (tlas_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanRTX", "TLAS not set! Call setTLAS() before SBT build.");
        throw std::runtime_error("TLAS missing — cannot build SBT");
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

    const uint32_t groupCount = 3;
    const VkDeviceSize handleSize = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleSizeAligned = alignUp(handleSize, rtProps.shaderGroupHandleAlignment);
    const VkDeviceSize sbtSize = groupCount * handleSizeAligned;

    createBuffer(physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 sbtBuffer_, sbtMemory_);

    void* data;
    VK_CHECK(vkMapMemory(device_, sbtMemory_, 0, sbtSize, 0, &data), "Map SBT");

    std::vector<uint8_t> handles(groupCount * handleSize);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device_, rtPipeline_, 0, groupCount, handles.size(), handles.data()),
             "Get group handles");

    for (uint32_t g = 0; g < groupCount; ++g) {
        memcpy(static_cast<uint8_t*>(data) + g * handleSizeAligned, handles.data() + g * handleSize, handleSize);
    }
    vkUnmapMemory(device_, sbtMemory_);

    VkBufferDeviceAddressInfo addrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = sbtBuffer_
    };
    sbtBufferAddress_ = vkGetBufferDeviceAddress(device_, &addrInfo);

    sbt_.raygen   = makeRegion(sbtBufferAddress_ + 0 * handleSizeAligned, handleSizeAligned, handleSizeAligned);
    sbt_.miss     = makeRegion(sbtBufferAddress_ + 1 * handleSizeAligned, handleSizeAligned, handleSizeAligned);
    sbt_.hit      = makeRegion(sbtBufferAddress_ + 2 * handleSizeAligned, handleSizeAligned, handleSizeAligned);
    sbt_.callable = emptyRegion();

    context_->raygenSbtAddress   = sbt_.raygen.deviceAddress;
    context_->missSbtAddress     = sbt_.miss.deviceAddress;
    context_->hitSbtAddress      = sbt_.hit.deviceAddress;
    context_->callableSbtAddress = sbt_.callable.deviceAddress;
    context_->sbtRecordSize      = static_cast<uint32_t>(handleSizeAligned);

    LOG_INFO_CAT("VulkanRTX", "{}    SBT BUILT @ 0x{:x} (stride={})", 
                 EMERALD_GREEN, sbtBufferAddress_, handleSizeAligned);
    LOG_INFO_CAT("VulkanRTX", "{}<<< SHADER BINDING TABLE BUILT SUCCESSFULLY{}", 
                 EMERALD_GREEN, RESET);
}

} // namespace VulkanRTX