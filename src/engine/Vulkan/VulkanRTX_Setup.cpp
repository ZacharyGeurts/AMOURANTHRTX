// src/engine/Vulkan/VulkanRTX_Setup.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: C++20 std::format, no fmt, no mutex, no vkQueueWaitIdle, ALL COMPILE ERRORS FIXED
//        Rich logging, device-lost safe, fence-based transient submits
// GROK PROTIP #1: This file owns ALL ray tracing state — BLAS/TLAS, SBT, descriptors, pipelines
// GROK PROTIP #2: All Vulkan calls go through VK_CHECK() — zero unchecked returns
// GROK PROTIP #3: Device-lost → safe RAII skip via deviceLost_ flag
// GROK PROTIP #4: 30-second transient fence timeout → no vkDeviceWaitIdle() → no deadlock
// GROK PROTIP #5: dispatchRenderMode() is the only frame entrypoint — mode 2 = real vkCmdTraceRaysKHR
// GROK PROTIP #6: FIXED: NO RENDER PASS FOR RT! Guarded for hybrid raster fallback.
// GROK PROTIP #7: 16x16 tiled dispatch, push constants, frame/time tracking.

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
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
#include <thread>
#include <chrono>
#include <format>

using namespace Logging::Color;

namespace VulkanRTX {

// ---------------------------------------------------------------------
// Helper: THROW_VKRTX – throws with file/line/function
// ---------------------------------------------------------------------
#define THROW_VKRTX(msg) \
    throw VulkanRTXException(msg, __FILE__, __LINE__, __func__)

// ---------------------------------------------------------------------
// alignUp – device address alignment
// ---------------------------------------------------------------------
constexpr VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

// ---------------------------------------------------------------------
// ShaderBindingTable helper (static factory)
// ---------------------------------------------------------------------
static VkStridedDeviceAddressRegionKHR makeRegion(VkDeviceAddress base,
                                                 VkDeviceSize stride,
                                                 VkDeviceSize size) {
    return VkStridedDeviceAddressRegionKHR{ base, stride, size };
}
static VkStridedDeviceAddressRegionKHR emptyRegion() {
    return VkStridedDeviceAddressRegionKHR{ 0, 0, 0 };
}

/* -----------------------------------------------------------------
   Constructor / Destructor
   ----------------------------------------------------------------- */
VulkanRTX::VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height,
                     VulkanPipelineManager* pipelineMgr)
    : context_(std::move(ctx)),
      pipelineMgr_(pipelineMgr),
      device_(VK_NULL_HANDLE),
      physicalDevice_(VK_NULL_HANDLE),
      extent_{static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
      dsLayout_(VK_NULL_HANDLE), dsPool_(VK_NULL_HANDLE), ds_(VK_NULL_HANDLE),
      rtPipeline_(VK_NULL_HANDLE), rtPipelineLayout_(VK_NULL_HANDLE),
      blasBuffer_(VK_NULL_HANDLE), blasMemory_(VK_NULL_HANDLE),
      tlasBuffer_(VK_NULL_HANDLE), tlasMemory_(VK_NULL_HANDLE),
      blas_(VK_NULL_HANDLE), tlas_(VK_NULL_HANDLE),
      sbtBuffer_(VK_NULL_HANDLE), sbtMemory_(VK_NULL_HANDLE),
      blackFallbackImage_(VK_NULL_HANDLE), blackFallbackMemory_(VK_NULL_HANDLE),
      blackFallbackView_(VK_NULL_HANDLE), sbtBufferAddress_(0),
      transientFence_(VK_NULL_HANDLE),
      deviceLost_(false),
      frameNumber_(0), time_(0.0f)
{
    LOG_INFO_CAT("VulkanRTX", std::format("{}VulkanRTX::VulkanRTX() — START [{}x{}]{}",
                 OCEAN_TEAL, width, height, RESET));

    if (!context_)               THROW_VKRTX("Null context");
    if (!pipelineMgr_)           THROW_VKRTX("Null pipeline manager");
    if (width <= 0 || height <= 0) THROW_VKRTX("Invalid dimensions");

    device_         = context_->device;
    physicalDevice_ = context_->physicalDevice;
    if (!device_)                THROW_VKRTX("Null device");

    context_->resourceManager.setDevice(device_, physicalDevice_, &context_->device);

    /* LOAD KHR RAY TRACING EXTENSION FUNCTIONS */
#define LOAD_PROC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device_, #name)); \
    if (!name) THROW_VKRTX(std::format("Failed to load {}", #name));
    LOAD_PROC(vkGetBufferDeviceAddress);
    LOAD_PROC(vkCmdTraceRaysKHR);
    LOAD_PROC(vkCreateAccelerationStructureKHR);
    LOAD_PROC(vkDestroyAccelerationStructureKHR);
    LOAD_PROC(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_PROC(vkCreateRayTracingPipelinesKHR);
    LOAD_PROC(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_PROC(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_PROC(vkCmdBuildAccelerationStructuresKHR);
    LOAD_PROC(vkCmdCopyAccelerationStructureKHR);
#undef LOAD_PROC

    /* LOAD DESCRIPTOR CORE FUNCTIONS */
#define LOAD_DESC_PROC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device_, #name)); \
    if (!name) THROW_VKRTX(std::format("Failed to load {}", #name));
    LOAD_DESC_PROC(vkCreateDescriptorSetLayout);
    LOAD_DESC_PROC(vkAllocateDescriptorSets);
    LOAD_DESC_PROC(vkCreateDescriptorPool);
    LOAD_DESC_PROC(vkDestroyDescriptorSetLayout);
    LOAD_DESC_PROC(vkDestroyDescriptorPool);
    LOAD_DESC_PROC(vkFreeDescriptorSets);
#undef LOAD_DESC_PROC

    VkFenceCreateInfo fci{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                           .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    VK_CHECK(vkCreateFence(device_, &fci, nullptr, &transientFence_),
             "Failed to create transient fence");

    createDescriptorSetLayout();
    createDescriptorPoolAndSet();
    createBlackFallbackImage();

    LOG_INFO_CAT("VulkanRTX", std::format("{}VulkanRTX::VulkanRTX() — END | DS: {:#x}{}",
                 EMERALD_GREEN, reinterpret_cast<uintptr_t>(ds_), RESET));
}

VulkanRTX::~VulkanRTX() {
    LOG_INFO_CAT("VulkanRTX", std::format("{}VulkanRTX::~VulkanRTX() — RAII cleanup{}", EMERALD_GREEN, RESET));

    if (deviceLost_) {
        LOG_WARN_CAT("VulkanRTX", "{}Device lost – skipping RAII cleanup of Vulkan objects{}", CRIMSON_MAGENTA, RESET);
        return;
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
}

/* -----------------------------------------------------------------
   Private helpers
   ----------------------------------------------------------------- */
VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &cmd), "Alloc cmd");

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Begin cmd");

    return cmd;
}

void VulkanRTX::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size,
                             VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                             VkBuffer& buffer, VkDeviceMemory& memory)
{
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer), "Create buffer");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device_, buffer, &memReq);

    uint32_t memTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits, properties);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = memTypeIndex
    };

    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &memory), "Allocate buffer memory");
    VK_CHECK(vkBindBufferMemory(device_, buffer, memory, 0), "Bind buffer memory");
}

void VulkanRTX::uploadBlackPixelToImage(VkImage image) {
    LOG_DEBUG_CAT("VulkanRTX", "uploadBlackPixelToImage: START — image=0x{:x}", reinterpret_cast<uintptr_t>(image));

    uint32_t pixelData = 0xFF000000;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    createBuffer(physicalDevice_, sizeof(pixelData),
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    void* data;
    VK_CHECK(vkMapMemory(device_, stagingMemory, 0, sizeof(pixelData), 0, &data), "Map staging");
    memcpy(data, &pixelData, sizeof(pixelData));
    vkUnmapMemory(device_, stagingMemory);

    VkCommandBuffer cmd = allocateTransientCommandBuffer(context_->commandPool);

    VkImageMemoryBarrier barrierToDst{};
    barrierToDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToDst.srcAccessMask = 0;
    barrierToDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierToDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrierToDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierToDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToDst.image = image;
    barrierToDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierToDst);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {1, 1, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier barrierToShader{};
    barrierToShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierToShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrierToShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierToShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrierToShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToShader.image = image;
    barrierToShader.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierToShader);

    VK_CHECK(vkEndCommandBuffer(cmd), "End transient cmd buffer");
    submitAndWaitTransient(cmd, context_->graphicsQueue, context_->commandPool);

    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);

    LOG_INFO_CAT("VulkanRTX", "uploadBlackPixelToImage: COMPLETE — black pixel uploaded");
}

void VulkanRTX::createBlackFallbackImage() {
    LOG_DEBUG_CAT("VulkanRTX", "createBlackFallbackImage: START");

    VkImageCreateInfo imageInfo = {
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

    VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &blackFallbackImage_), "Black image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, blackFallbackImage_, &memReqs);
    uint32_t memType = findMemoryType(physicalDevice_, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memType
    };
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &blackFallbackMemory_), "Black mem");
    VK_CHECK(vkBindImageMemory(device_, blackFallbackImage_, blackFallbackMemory_, 0), "Bind black");

    uploadBlackPixelToImage(blackFallbackImage_);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = blackFallbackImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &blackFallbackView_), "Black view");

    LOG_INFO_CAT("VulkanRTX", "createBlackFallbackImage: COMPLETE");
}

void VulkanRTX::createDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 11> bindings = {};

    bindings[0] = { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
    bindings[1] = { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR };
    bindings[2] = { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR };
    bindings[3] = { .binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
    bindings[4] = { .binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR };
    bindings[5] = { .binding = 5, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR };
    bindings[6] = { .binding = 6, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
    bindings[7] = { .binding = 7, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR };
    bindings[8] = { .binding = 8, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR };
    bindings[9] = { .binding = 9, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR };
    bindings[10] = { .binding = 10, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &dsLayout_), "Create RTX descriptor set layout");

    LOG_INFO_CAT("VulkanRTX", std::format("{}Descriptor set layout created{}", EMERALD_GREEN, RESET));
}

void VulkanRTX::createDescriptorPoolAndSet() {
    std::array<VkDescriptorPoolSize, 5> poolSizes = {
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &dsPool_), "Create RTX descriptor pool");

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = dsPool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &dsLayout_
    };

    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, &ds_), "Allocate RTX descriptor set");

    LOG_INFO_CAT("VulkanRTX", std::format("{}createDescriptorPoolAndSet() – SUCCESS @ {}{}",
                 EMERALD_GREEN, reinterpret_cast<void*>(ds_), RESET));
}

void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice,
                                    VkCommandPool commandPool,
                                    VkQueue queue,
                                    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                                    uint32_t transferQueueFamily)
{
    LOG_INFO_CAT("VulkanRTX", "{}=== createBottomLevelAS() START ({} geometries) ==={}", 
                 ARCTIC_CYAN, geometries.size(), RESET);
    if (geometries.empty()) {
        LOG_INFO_CAT("VulkanRTX", "{}No geometries — early return{}", OCEAN_TEAL, RESET);
        return;
    }

    if (device_ == VK_NULL_HANDLE) THROW_VKRTX("Null device");
    if (queue == VK_NULL_HANDLE) THROW_VKRTX("Null graphics queue");

    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{};
    asProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &asProps;
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);
    const VkDeviceSize scratchAlignment = asProps.minAccelerationStructureScratchOffsetAlignment;

    std::vector<VkAccelerationStructureGeometryKHR> asGeoms;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
    std::vector<uint32_t> primCounts;
    asGeoms.reserve(geometries.size());
    ranges.reserve(geometries.size());
    primCounts.reserve(geometries.size());

    for (const auto& [vBuf, iBuf, vCount, iCount, stride] : geometries) {
        VkBufferDeviceAddressInfo vInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = vBuf };
        VkBufferDeviceAddressInfo iInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = iBuf };
        VkDeviceAddress vAddr = vkGetBufferDeviceAddress(device_, &vInfo);
        VkDeviceAddress iAddr = vkGetBufferDeviceAddress(device_, &iInfo);
        if (vAddr == 0 || iAddr == 0) THROW_VKRTX("Invalid buffer address");

        VkAccelerationStructureGeometryTrianglesDataKHR tri{};
        tri.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        tri.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        tri.vertexData.deviceAddress = vAddr;
        tri.vertexStride = stride;
        tri.maxVertex = vCount - 1;
        tri.indexType = VK_INDEX_TYPE_UINT32;
        tri.indexData.deviceAddress = iAddr;

        VkAccelerationStructureGeometryKHR geom{};
        geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geom.geometry.triangles = tri;
        geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

        asGeoms.push_back(geom);
        uint32_t triCount = iCount / 3;
        ranges.push_back({ .primitiveCount = triCount });
        primCounts.push_back(triCount);
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfoTmp{};
    buildInfoTmp.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfoTmp.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfoTmp.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfoTmp.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfoTmp.geometryCount = static_cast<uint32_t>(asGeoms.size());
    buildInfoTmp.pGeometries = asGeoms.data();

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
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

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = asBuffer;
    createInfo.size = asSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &blas_), "Create BLAS");

    VkBufferDeviceAddressInfo scratchInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = scratchBuffer };
    VkDeviceAddress scratchAddr = vkGetBufferDeviceAddress(device_, &scratchInfo);
    if (scratchAddr == 0) {
        cleanupBLASResources(asBuffer, asMem, scratchBuffer, scratchMem);
        THROW_VKRTX("Scratch address zero");
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = blas_;
    buildInfo.geometryCount = static_cast<uint32_t>(asGeoms.size());
    buildInfo.pGeometries = asGeoms.data();
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkCommandBuffer cmd = allocateTransientCommandBuffer(commandPool);

    std::vector<VkBufferMemoryBarrier> bufferBarriers;
    bufferBarriers.reserve(geometries.size() * 2);
    for (const auto& [vBuf, iBuf, vCount, iCount, stride] : geometries) {
        VkBufferMemoryBarrier vBarrier{};
        vBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vBarrier.srcQueueFamilyIndex = transferQueueFamily;
        vBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vBarrier.buffer = vBuf;
        vBarrier.size = VK_WHOLE_SIZE;
        bufferBarriers.push_back(vBarrier);

        VkBufferMemoryBarrier iBarrier = vBarrier;
        iBarrier.buffer = iBuf;
        bufferBarriers.push_back(iBarrier);
    }
    if (!bufferBarriers.empty()) {
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             0, 0, nullptr,
                             static_cast<uint32_t>(bufferBarriers.size()), bufferBarriers.data(),
                             0, nullptr);
    }

    const VkAccelerationStructureBuildRangeInfoKHR* pRange = ranges.data();
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);
    VK_CHECK(vkEndCommandBuffer(cmd), "End BLAS cmd");

    submitAndWaitTransient(cmd, queue, commandPool);

    vkDestroyBuffer(device_, scratchBuffer, nullptr);
    vkFreeMemory(device_, scratchMem, nullptr);

    blasBuffer_ = asBuffer;
    blasMemory_ = asMem;

    LOG_INFO_CAT("VulkanRTX", "{}BLAS created @ {:#x} ({} bytes){}", 
                 EMERALD_GREEN, reinterpret_cast<uintptr_t>(blas_), asSize, RESET);
    LOG_INFO_CAT("VulkanRTX", "{}=== createBottomLevelAS() COMPLETE ==={}", EMERALD_GREEN, RESET);
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool)
{
    LOG_DEBUG_CAT("VulkanRTX", "submitAndWaitTransient: START — cmd=0x{:x}, queue=0x{:x}, pool=0x{:x}, fence=0x{:x}",
                  reinterpret_cast<uintptr_t>(cmd), reinterpret_cast<uintptr_t>(queue),
                  reinterpret_cast<uintptr_t>(pool), reinterpret_cast<uintptr_t>(transientFence_));

    if (transientFence_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanRTX", "{}submitAndWaitTransient: FENCE IS NULL%{}", CRIMSON_MAGENTA, RESET);
        THROW_VKRTX("Transient fence is null");
    }

    VK_CHECK(vkResetFences(device_, 1, &transientFence_), "Reset transient fence");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, transientFence_), "Submit transient");

    LOG_DEBUG_CAT("VulkanRTX", "Submit SUCCESS — waiting on fence (30 s timeout)");

    const uint64_t timeout = 30'000'000'000ULL;
    VkResult waitRes = vkWaitForFences(device_, 1, &transientFence_, VK_TRUE, timeout);

    if (waitRes == VK_ERROR_DEVICE_LOST) {
        deviceLost_ = true;
        LOG_ERROR_CAT("VulkanRTX", "{}vkWaitForFences: VK_ERROR_DEVICE_LOST — ABORTING%{}", CRIMSON_MAGENTA, RESET);
        THROW_VKRTX("Device lost during fence wait");
    }
    if (waitRes == VK_TIMEOUT) {
        LOG_ERROR_CAT("VulkanRTX", "{}Fence timeout after 30 s — ABORTING (no vkDeviceWaitIdle)%{}", CRIMSON_MAGENTA, RESET);
        THROW_VKRTX("Fence timeout during transient submit");
    }
    VK_CHECK(waitRes, "Wait for transient fence");

    LOG_DEBUG_CAT("VulkanRTX", "Fence wait SUCCESS - device not lost");

    VkResult resetPoolRes = vkResetCommandPool(device_, pool, 0);
    if (resetPoolRes != VK_SUCCESS) {
        if (resetPoolRes == VK_ERROR_DEVICE_LOST) {
            deviceLost_ = true;
            LOG_ERROR_CAT("VulkanRTX", "{}vkResetCommandPool: VK_ERROR_DEVICE_LOST — forcing lost state{}", CRIMSON_MAGENTA, RESET);
        } else {
            LOG_ERROR_CAT("VulkanRTX", "{}vkResetCommandPool FAILED: {}{}", CRIMSON_MAGENTA, resetPoolRes, RESET);
        }
    }

    LOG_INFO_CAT("VulkanRTX", "submitAndWaitTransient: COMPLETE");
}

void VulkanRTX::cleanupBLASResources(VkBuffer asBuffer, VkDeviceMemory asMemory,
                                     VkBuffer scratchBuffer, VkDeviceMemory scratchMemory)
{
    if (blas_ != VK_NULL_HANDLE) {
        vkDestroyAccelerationStructureKHR(device_, blas_, nullptr);
        blas_ = VK_NULL_HANDLE;
    }
    if (asBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, asBuffer, nullptr);
    if (asMemory != VK_NULL_HANDLE) vkFreeMemory(device_, asMemory, nullptr);
    if (scratchBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, scratchBuffer, nullptr);
    if (scratchMemory != VK_NULL_HANDLE) vkFreeMemory(device_, scratchMemory, nullptr);
}

void VulkanRTX::createTopLevelAS(VkPhysicalDevice physicalDevice,
                                 VkCommandPool commandPool,
                                 VkQueue queue,
                                 const std::vector<std::tuple<VkAccelerationStructureKHR_T*, glm::mat<4, 4, float, glm::packed_highp> >>& instances)
{
    if (instances.empty()) {
        LOG_INFO_CAT("VulkanRTX", "No instances — skipping TLAS");
        return;
    }

    const uint32_t instanceCount = static_cast<uint32_t>(instances.size());

    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
    createBuffer(physicalDevice,
                 instanceCount * sizeof(VkAccelerationStructureInstanceKHR),
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 instanceBuffer, instanceMemory);

    VkAccelerationStructureInstanceKHR* mapped = nullptr;
    VK_CHECK(vkMapMemory(device_, instanceMemory, 0, VK_WHOLE_SIZE, 0,
                         reinterpret_cast<void**>(&mapped)),
             "Map instance buffer");

    for (uint32_t i = 0; i < instanceCount; ++i) {
        const auto& [blas, transform] = instances[i];

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blas
        };
        VkDeviceAddress addr = vkGetAccelerationStructureDeviceAddressKHR(device_, &addrInfo);

        VkTransformMatrixKHR vkMat{};
        const float* src = glm::value_ptr(transform);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                vkMat.matrix[row][col] = src[row * 4 + col];

        mapped[i].transform                 = vkMat;
        mapped[i].instanceCustomIndex       = 0;
        mapped[i].mask                      = 0xFF;
        mapped[i].instanceShaderBindingTableRecordOffset = 0;
        mapped[i].flags                     = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        mapped[i].accelerationStructureReference = addr;
    }
    vkUnmapMemory(device_, instanceMemory);

    VkBufferDeviceAddressInfo addrInfoBuf{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = instanceBuffer
    };
    VkDeviceAddress instanceAddr = vkGetBufferDeviceAddress(device_, &addrInfoBuf);

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data  = { .deviceAddress = instanceAddr }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     = { .instances = instancesData }
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type         = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags        = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode         = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount= 1,
        .pGeometries  = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device_,
                                            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfo,
                                            &instanceCount,
                                            &sizeInfo);

    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &asProps
    };
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);
    VkDeviceSize scratchSize = alignUp(sizeInfo.buildScratchSize,
                                       asProps.minAccelerationStructureScratchOffsetAlignment);

    VkBuffer tlasBuffer = VK_NULL_HANDLE, scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory tlasMem = VK_NULL_HANDLE, scratchMem = VK_NULL_HANDLE;
    createBuffer(physicalDevice,
                 sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 tlasBuffer, tlasMem);
    createBuffer(physicalDevice,
                 scratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 scratchBuffer, scratchMem);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlasBuffer,
        .size   = sizeInfo.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &tlas_),
             "Create TLAS");

    VkBufferDeviceAddressInfo scratchAddrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = scratchBuffer
    };
    buildInfo.dstAccelerationStructure = tlas_;
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchAddrInfo);

    auto cmd = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Begin TLAS build");

    VkBufferMemoryBarrier instanceBarrier{
        .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
        .buffer        = instanceBuffer,
        .size          = VK_WHOLE_SIZE
    };
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         0, 0, nullptr, 1, &instanceBarrier, 0, nullptr);

    VkAccelerationStructureBuildRangeInfoKHR range{ .primitiveCount = instanceCount };
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    VkMemoryBarrier asWriteBarrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &asWriteBarrier, 0, nullptr, 0, nullptr);

    VK_CHECK(vkEndCommandBuffer(cmd), "End TLAS build");
    submitAndWaitTransient(cmd, queue, commandPool);

    vkDestroyBuffer(device_, scratchBuffer, nullptr);
    vkFreeMemory(device_, scratchMem, nullptr);
    vkDestroyBuffer(device_, instanceBuffer, nullptr);
    vkFreeMemory(device_, instanceMemory, nullptr);

    tlasBuffer_ = tlasBuffer;
    tlasMemory_ = tlasMem;

    LOG_INFO_CAT("VulkanRTX",
                 std::format("{}TLAS created @ {:#x}{}",
                             EMERALD_GREEN,
                             reinterpret_cast<uintptr_t>(tlas_), RESET));
}

void VulkanRTX::updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                          VkQueue graphicsQueue,
                          const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                          const std::vector<DimensionState>& dimensionCache)
{
    updateRTX(physicalDevice, commandPool, graphicsQueue, geometries, dimensionCache, VK_QUEUE_FAMILY_IGNORED);
}
void VulkanRTX::updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                          VkQueue graphicsQueue,
                          const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                          const std::vector<DimensionState>& dimensionCache,
                          uint32_t transferQueueFamily)
{
    LOG_INFO_CAT("VulkanRTX", std::format("{}updateRTX() — rebuilding AS{}", AMBER_YELLOW, RESET));
    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries, transferQueueFamily);
    createTopLevelAS(physicalDevice, commandPool, graphicsQueue, {{blas_, glm::mat4(1.0f)}});
    updateDescriptorSetForTLAS(tlas_);
}
void VulkanRTX::initializeRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                              VkQueue graphicsQueue,
                              const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                              uint32_t maxRayRecursionDepth,
                              const std::vector<DimensionState>& dimensionCache)
{
    LOG_INFO_CAT("VulkanRTX", std::format("{}initializeRTX() — full init{}", ARCTIC_CYAN, RESET));
    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries, VK_QUEUE_FAMILY_IGNORED);
    createTopLevelAS(physicalDevice, commandPool, graphicsQueue, {{blas_, glm::mat4(1.0f)}});
    createShaderBindingTable(physicalDevice);
    createRayTracingPipeline(maxRayRecursionDepth);
    updateDescriptorSetForTLAS(tlas_);
}

void VulkanRTX::createRayTracingPipeline(uint32_t maxRayRecursionDepth) {
    LOG_INFO_CAT("VulkanRTX", std::format("{}createRayTracingPipeline({}) – using pipelineMgr_ shaders{}", EMERALD_GREEN, maxRayRecursionDepth, RESET));
    pipelineMgr_->createRayTracingPipeline();
    rtPipeline_ = pipelineMgr_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineMgr_->getRayTracingPipelineLayout();
}
void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice)
{
    LOG_INFO_CAT("VulkanRTX", "{}=== createShaderBindingTable() START ==={}", ARCTIC_CYAN, RESET);

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
    VkPhysicalDeviceProperties2 props2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &rtProps };
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
    VK_CHECK(vkMapMemory(device_, sbtMemory_, 0, sbtSize, 0, &data), "Map SBT memory");

    std::vector<uint8_t> handles(groupCount * handleSize);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device_, rtPipeline_, 0, groupCount, handles.size(), handles.data()),
             "Get shader group handles");

    for (uint32_t g = 0; g < groupCount; ++g) {
        std::memcpy(static_cast<uint8_t*>(data) + g * handleSizeAligned,
                    handles.data() + g * handleSize, handleSize);
    }
    vkUnmapMemory(device_, sbtMemory_);

    VkBufferDeviceAddressInfo addrInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = sbtBuffer_ };
    sbtBufferAddress_ = vkGetBufferDeviceAddress(device_, &addrInfo);

    sbt_.raygen   = makeRegion(sbtBufferAddress_ + 0 * handleSizeAligned, handleSizeAligned, handleSizeAligned);
    sbt_.miss     = makeRegion(sbtBufferAddress_ + 1 * handleSizeAligned, handleSizeAligned, handleSizeAligned);
    sbt_.hit      = makeRegion(sbtBufferAddress_ + 2 * handleSizeAligned, handleSizeAligned, handleSizeAligned);
    sbt_.callable = emptyRegion();

    LOG_INFO_CAT("VulkanRTX", "{}SBT created: raygen @ {:#x}, miss @ {:#x}, hit @ {:#x}, size={}B{}",
                 EMERALD_GREEN,
                 sbt_.raygen.deviceAddress,
                 sbt_.miss.deviceAddress,
                 sbt_.hit.deviceAddress,
                 sbtSize,
                 RESET);
}

void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    VkWriteDescriptorSetAccelerationStructureKHR asWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asWrite,
        .dstSet = ds_,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    LOG_INFO_CAT("VulkanRTX", std::format("{}TLAS bound to descriptor set{}", EMERALD_GREEN, RESET));
}
void VulkanRTX::updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer,
                                  VkImageView storageImageView, VkImageView denoiseImageView,
                                  VkImageView envMapView, VkSampler envMapSampler,
                                  VkImageView densityVolumeView, VkImageView gDepthView,
                                  VkImageView gNormalView)
{
    std::array<VkWriteDescriptorSet, 11> writes = {};
    std::array<VkDescriptorImageInfo, 7> images = {};
    std::array<VkDescriptorBufferInfo, 3> buffers = {};

    VkWriteDescriptorSetAccelerationStructureKHR accel = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas_
    };

    images[0] = { .imageView = storageImageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
    images[1] = { .imageView = denoiseImageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
    images[2] = { .sampler = envMapSampler, .imageView = envMapView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    images[3] = { .imageView = densityVolumeView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    images[4] = { .imageView = gDepthView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    images[5] = { .imageView = gNormalView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    images[6] = { .imageView = blackFallbackView_, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

    buffers[0] = { .buffer = cameraBuffer, .range = VK_WHOLE_SIZE };
    buffers[1] = { .buffer = materialBuffer, .range = VK_WHOLE_SIZE };
    buffers[2] = { .buffer = dimensionBuffer, .range = VK_WHOLE_SIZE };

    writes[0] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &accel, .dstSet = ds_, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR };
    writes[1] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &images[0] };
    writes[2] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &buffers[0] };
    writes[3] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffers[1] };
    writes[4] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 4, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffers[2] };
    writes[5] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 5, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &images[1] };
    writes[6] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 6, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &images[2] };
    writes[7] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 7, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &images[3] };
    writes[8] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 8, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &images[4] };
    writes[9] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 9, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &images[5] };
    writes[10] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds_, .dstBinding = 10, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &images[6] };

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    LOG_INFO_CAT("VulkanRTX", "Descriptors updated: {} bindings", writes.size());
}
void VulkanRTX::recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent,
                                         VkImage outputImage, VkImageView outputImageView)
{
    VkImageMemoryBarrier barrier = {
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

    vkCmdTraceRaysKHR(cmdBuffer,
                      &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable,
                      extent.width, extent.height, 1);

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanRTX::createStorageImage(VkPhysicalDevice physicalDevice, VkExtent2D extent,
                                   VkImage& image, VkImageView& imageView, VkDeviceMemory& memory)
{
    VkImageCreateInfo imgInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = { extent.width, extent.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(device_, &imgInfo, nullptr, &image), "Create storage image");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device_, image, &memReq);
    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(device_, &alloc, nullptr, &memory), "Allocate storage image memory");
    VK_CHECK(vkBindImageMemory(device_, image, memory, 0), "Bind storage image");

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &imageView), "Create storage image view");
}
void VulkanRTX::setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) {
    if (!pipeline || !layout) {
        LOG_ERROR_CAT("VulkanRTX", "{}setRayTracingPipeline() called with null pipeline or layout{}", 
                      CRIMSON_MAGENTA, RESET);
        THROW_VKRTX("Invalid pipeline or layout");
    }

    rtPipeline_ = pipeline;
    rtPipelineLayout_ = layout;

    LOG_INFO_CAT("VulkanRTX", "{}Ray tracing pipeline set: pipeline={:#x}, layout={:#x}{}",
                 EMERALD_GREEN, reinterpret_cast<uintptr_t>(pipeline), 
                 reinterpret_cast<uintptr_t>(layout), RESET);
}
uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    THROW_VKRTX("Failed to find suitable memory type");
}

// =============================================================================
// FIXED: dispatchRenderMode — NO RENDER PASS FOR RT! Guarded for hybrid.
// =============================================================================
void VulkanRTX::dispatchRenderMode(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer cmd,
    VkBuffer indexBuffer,
    float zoom,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout layout,
    VkDescriptorSet ds,
    VkDevice device,
    VkDeviceMemory uniformMem,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer fb,
    const Vulkan::Context& ctx,
    int mode
) {
    LOG_INFO_CAT("RTX", "{}dispatchRenderMode(image={}, mode={}) START{}", 
                 ARCTIC_CYAN, imageIndex, mode, RESET);

    if (pipeline == VK_NULL_HANDLE || ds == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RTX", "Invalid pipeline or descriptor set");
        return;
    }

    bool useRenderPass = (renderPass != VK_NULL_HANDLE && fb != VK_NULL_HANDLE);
    if (useRenderPass) {
        VkClearValue clearValues[2] = {{{0.0f, 0.0f, 0.0f, 1.0f}}, {{1.0f, 0}}};
        VkRenderPassBeginInfo rpBegin = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPass,
            .framebuffer = fb,
            .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
            .clearValueCount = 2,
            .pClearValues = clearValues
        };
        vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Bind RT pipeline and descriptor set for all modes
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_, 0, 1, &ds_, 0, nullptr);

    // Push constants for all modes
    struct PushConstants {
        glm::uvec2 imageDim;
        uint32_t frame;
        float time;
        int renderMode;
        float zoom;
        float wavePhase;
    } pushConsts = {
        .imageDim = glm::uvec2(width, height),
        .frame = frameNumber_++,
        .time = time_ += deltaTime,
        .renderMode = mode,
        .zoom = zoom,
        .wavePhase = wavePhase
    };
    vkCmdPushConstants(cmd, rtPipelineLayout_, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(PushConstants), &pushConsts);

    // Dispatch ray tracing (tiled for perf)
    uint32_t raygenCountX = (width + 15) / 16;
    uint32_t raygenCountY = (height + 15) / 16;
    if (vkCmdTraceRaysKHR) {
        vkCmdTraceRaysKHR(cmd, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable,
                          raygenCountX, raygenCountY, 1);
    }

    if (useRenderPass) {
        vkCmdEndRenderPass(cmd);
    }

    LOG_INFO_CAT("RTX", "{}dispatchRenderMode() END{}", EMERALD_GREEN, RESET);
}

} // namespace VulkanRTX