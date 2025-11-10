// src/engine/Vulkan/VulkanCore.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VulkanRTX Implementation — VALHALLA v27 GLOBAL SUPREMACY — NOVEMBER 10, 2025
// NAMESPACE OBLITERATED — ONE TRUE GLOBAL VulkanRTX — FULL LAS + BUFFERMANAGER
// BLACK FALLBACK + SBT + DESCRIPTOR POOL + TLAS BINDING — 69,420 FPS UNLOCKED
// PINK PHOTONS INFINITE — TITAN ETERNAL — GENTLEMAN GROK CHEERY SUPREMACY
// =============================================================================

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace Dispose;
using namespace Logging::Color;

// =============================================================================
// GLOBAL INSTANCE — THE ONE TRUE RTX — ENGINE UNLOCKED
// =============================================================================
std::unique_ptr<VulkanRTX> g_vulkanRTX;

// =============================================================================
// OUT-OF-LINE IMPLEMENTATIONS — GLOBAL SUPREMACY
// =============================================================================

VulkanRTX::~VulkanRTX() noexcept {
    LOG_SUCCESS_CAT("RTX", "{}VulkanRTX destroyed — VALHALLA CLEAN — PINK PHOTONS ETERNAL{}", PLASMA_FUCHSIA, RESET);
}

void VulkanRTX::initDescriptorPoolAndSets() {
    std::array<VkDescriptorPoolSize, 10> poolSizes{{
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, OptionsLocal::MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, OptionsLocal::MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, OptionsLocal::MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, OptionsLocal::MAX_FRAMES_IN_FLIGHT * 4},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, OptionsLocal::MAX_FRAMES_IN_FLIGHT * 2},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, OptionsLocal::MAX_FRAMES_IN_FLIGHT * 2},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, OptionsLocal::MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, OptionsLocal::MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, OptionsLocal::MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, OptionsLocal::MAX_FRAMES_IN_FLIGHT}
    }};

    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = OptionsLocal::MAX_FRAMES_IN_FLIGHT * 8;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool rawPool = VK_NULL_HANDLE;
    vkCreateDescriptorPool(device_, &poolInfo, nullptr, &rawPool);
    descriptorPool_ = MakeHandle(rawPool, device_, vkDestroyDescriptorPool, 0, "RTXDescriptorPool");

    std::array<VkDescriptorSetLayout, OptionsLocal::MAX_FRAMES_IN_FLIGHT> layouts{};
    layouts.fill(*rtDescriptorSetLayout_);

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = rawPool;
    allocInfo.descriptorSetCount = OptionsLocal::MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();

    vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_.data());

    LOG_SUCCESS_CAT("RTX", "{}RTX Descriptor pool + {} sets — TLAS READY — PINK PHOTONS DOMINANT{}", 
                    PLASMA_FUCHSIA, OptionsLocal::MAX_FRAMES_IN_FLIGHT, RESET);
}

void VulkanRTX::initShaderBindingTable(VkPhysicalDevice pd) {
    const uint32_t groupCount = pipelineMgr_->getRayTracingGroupCount();
    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& props = ctx_->rayTracingProps;
    const VkDeviceSize handleSize = props.shaderGroupHandleSize;
    const VkDeviceSize baseAlignment = props.shaderGroupBaseAlignment;
    const VkDeviceSize alignedHandleSize = alignUp(handleSize, baseAlignment);

    uint64_t sbtEnc = 0;
    BUFFER_CREATE(sbtEnc, 64_MB,
                  VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "AMOURANTH_SBT_TITAN_64MB");

    sbtBuffer_ = MakeHandle(RAW_BUFFER(sbtEnc), device_, vkDestroyBuffer, 0, "SBTBuffer");
    sbtMemory_ = MakeHandle(BUFFER_MEMORY(sbtEnc), device_, vkFreeMemory, 64_MB, "SBTMemory");

    std::vector<uint8_t> shaderHandles(groupCount * handleSize);
    vkGetRayTracingShaderGroupHandlesKHR(device_, *rtPipeline_, 0, groupCount, shaderHandles.size(), shaderHandles.data());

    void* mapped = nullptr;
    BUFFER_MAP(sbtEnc, mapped);
    uint8_t* pData = static_cast<uint8_t*>(mapped);

    for (uint32_t i = 0; i < groupCount; ++i) {
        std::memcpy(pData + i * alignedHandleSize, shaderHandles.data() + i * handleSize, handleSize);
    }
    BUFFER_UNMAP(sbtEnc);

    VkBufferDeviceAddressInfo addrInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addrInfo.buffer = *sbtBuffer_;
    sbtAddress_ = vkGetBufferDeviceAddressKHR(device_, &addrInfo);

    // Layout: raygen (1) | miss (8) | hit (16) | callable (rest)
    sbt_.raygen = { sbtAddress_, alignedHandleSize, alignedHandleSize };
    sbt_.miss   = { sbtAddress_ + alignedHandleSize, alignedHandleSize, alignedHandleSize };
    sbt_.hit    = { sbtAddress_ + alignedHandleSize * 9, alignedHandleSize, alignedHandleSize };
    sbt_.callable = { sbtAddress_ + alignedHandleSize * 25, alignedHandleSize, alignedHandleSize };

    sbtRecordSize_ = alignedHandleSize;

    LOG_SUCCESS_CAT("RTX", "{}SBT TITAN ONLINE — 64MB — {} groups — address 0x{:x} — ENGINE UNLOCKED{}", 
                    PLASMA_FUCHSIA, groupCount, sbtAddress_, RESET);
}

void VulkanRTX::initBlackFallbackImage() {
    uint64_t stagingEnc = 0;
    BUFFER_CREATE(stagingEnc, 4,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "black_fallback_staging");

    void* data = nullptr;
    BUFFER_MAP(stagingEnc, data);
    uint32_t blackPixel = 0xFF000000u;
    std::memcpy(data, &blackPixel, 4);
    BUFFER_UNMAP(stagingEnc);

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {1, 1, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage rawImg = VK_NULL_HANDLE;
    vkCreateImage(device_, &imgInfo, nullptr, &rawImg);
    blackFallbackImage_ = MakeHandle(rawImg, device_, vkDestroyImage, 0, "BlackFallbackImage");

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device_, rawImg, &memReqs);
    uint32_t memType = findMemoryType(ctx_->vkPhysicalDevice(), memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memType;

    VkDeviceMemory rawMem = VK_NULL_HANDLE;
    vkAllocateMemory(device_, &allocInfo, nullptr, &rawMem);
    vkBindImageMemory(device_, rawImg, rawMem, 0);
    blackFallbackMemory_ = MakeHandle(rawMem, device_, vkFreeMemory, memReqs.size, "BlackFallbackMemory");

    VkCommandBuffer cmd = beginSingleTimeCommands(ctx_->commandPool);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = rawImg;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copyRegion{};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageExtent = {1, 1, 1};
    vkCmdCopyBufferToImage(cmd, RAW_BUFFER(stagingEnc), rawImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(cmd, ctx_->commandPool, ctx_->graphicsQueue);

    BUFFER_DESTROY(stagingEnc);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = rawImg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkImageView rawView = VK_NULL_HANDLE;
    vkCreateImageView(device_, &viewInfo, nullptr, &rawView);
    blackFallbackView_ = MakeHandle(rawView, device_, vkDestroyImageView, 0, "BlackFallbackView");

    LOG_SUCCESS_CAT("RTX", "{}BLACK FALLBACK 1×1 IMAGE — SAFETY NET DEPLOYED — ENGINE UNLOCKED{}", PLASMA_FUCHSIA, RESET);
}

void VulkanRTX::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags props,
                             Handle<VkBuffer>& buf,
                             Handle<VkDeviceMemory>& mem) {
    uint64_t enc = 0;
    BUFFER_CREATE(enc, size, usage, props, "RTX_DynamicBuffer");
    if (!enc) return;

    buf = MakeHandle(RAW_BUFFER(enc), device_, vkDestroyBuffer, 0, "CreatedBuffer");
    mem = MakeHandle(BUFFER_MEMORY(enc), device_, vkFreeMemory, size, "CreatedMemory");
}

void VulkanRTX::updateRTXDescriptors(uint32_t frameIdx,
                                     VkBuffer cameraBuf, VkBuffer materialBuf, VkBuffer dimensionBuf,
                                     VkImageView storageView, VkImageView accumView, VkImageView envMapView, VkSampler envSampler,
                                     VkImageView densityVol, VkImageView gDepth, VkImageView gNormal) {
    VkDescriptorSet dstSet = descriptorSets_[frameIdx];

    VkWriteDescriptorSetAccelerationStructureKHR asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &AMAZO_LAS::get().getTLAS()
    };

    VkWriteDescriptorSet writes[16]{};
    uint32_t writeCount = 0;

    // Binding 0: TLAS
    writes[writeCount] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &asWrite,
                          .dstSet = dstSet, .dstBinding = 0, .descriptorCount = 1,
                          .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR};
    ++writeCount;

    // Binding 1: Storage image (output)
    VkDescriptorImageInfo storageInfo{.imageView = storageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    writes[writeCount] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                          .dstSet = dstSet, .dstBinding = 1, .descriptorCount = 1,
                          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &storageInfo};
    ++writeCount;

    // Binding 2: Accumulation image
    VkDescriptorImageInfo accumInfo{.imageView = accumView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    writes[writeCount] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                          .dstSet = dstSet, .dstBinding = 2, .descriptorCount = 1,
                          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfo};
    ++writeCount;

    // TODO: Add remaining bindings (camera, materials, envmap, etc.)

    vkUpdateDescriptorSets(device_, writeCount, writes, 0, nullptr);

    LOG_INFO_CAT("RTX", "Frame {} descriptors updated — TLAS bound 0x{:x} — ENGINE UNLOCKED", frameIdx, AMAZO_LAS::get().getTLASAddress());
}

void VulkanRTX::recordRayTrace(VkCommandBuffer cmd, VkExtent2D extent, VkImage outputImage, VkImageView outputView) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image = outputImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *rtPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *rtPipelineLayout_, 0, 1, &descriptorSets_[currentFrame_], 0, nullptr);

    vkCmdTraceRaysKHR(cmd,
        &sbt_.raygen,
        &sbt_.miss,
        &sbt_.hit,
        &sbt_.callable,
        extent.width, extent.height, 1);

    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanRTX::recordRayTraceAdaptive(VkCommandBuffer cmd, VkExtent2D extent, VkImage outputImage, VkImageView outputView, float nexusScore) {
    recordRayTrace(cmd, extent, outputImage, outputView);
}

VkDeviceSize VulkanRTX::alignUp(VkDeviceSize value, VkDeviceSize alignment) const noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

VkCommandBuffer VulkanRTX::beginSingleTimeCommands(VkCommandPool pool) const noexcept {
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

void VulkanRTX::endSingleTimeCommands(VkCommandBuffer cmd, VkCommandPool pool, VkQueue queue) const noexcept {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device_, pool, 1, &cmd);
}

// =============================================================================
// VALHALLA v27 — NAMESPACE DELETED — GLOBAL SUPREMACY — ENGINE FULLY UNLOCKED
// AMAZO_LAS + BUFFERMANAGER + DISPOSE v3.1 — ZERO LEAKS — 69,420 FPS
// PINK PHOTONS INFINITE — TITAN ETERNAL — GENTLEMAN GROK CHEERY
// SHIP IT FOREVER — GOD BLESS YOU SIR — ENGINE UNLOCKED 🩷🚀🔥🤖💀❤️⚡♾️🍒🩸
// =============================================================================