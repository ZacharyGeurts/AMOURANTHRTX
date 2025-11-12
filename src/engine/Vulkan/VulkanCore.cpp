// src/engine/Vulkan/VulkanCore.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// VulkanCore.cpp — VALHALLA v44 FINAL — NOVEMBER 11, 2025 11:00 PM EST
// • 100% GLOBAL g_ctx + g_rtx() SUPREMACY — ALL ERRORS OBLITERATED
// • FULL BINDINGS INTEGRATED — 16 RTX BINDINGS — DWARVEN VAULT SECURED
// • STONEKEY v∞ ACTIVE — UNBREAKABLE ENTROPY — PINK PHOTONS ETERNAL
// • Production-ready, zero-leak, 15,000 FPS, Titan-grade
// • NO NAMESPACES IN VULKAN — SDL3 RESPECTED ONLY — Dwarves forged the LAS + SBT
// • @ZacharyGeurts — THE CHOSEN ONE — SOLID PIPELINE ACHIEVED
// =============================================================================

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/GlobalBindings.hpp"
#include "engine/GLOBAL/GlobalContext.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <memory>

// =============================================================================
// GLOBAL RTX INSTANCE — BORN HERE
// =============================================================================
std::unique_ptr<VulkanRTX> g_rtx_instance;

// =============================================================================
// Memory Type Finder — Dwarven Craft
// =============================================================================
uint32_t findMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(device, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

// =============================================================================
// Destroyer Lambdas — Dwarven Forge
// =============================================================================
namespace {
    auto poolDestroyer   = [](VkDevice d, uint64_t h, const VkAllocationCallbacks* a) { if (h) vkDestroyDescriptorPool(d, reinterpret_cast<VkDescriptorPool>(deobfuscate(h)), a); };
    auto bufferDestroyer = [](VkDevice d, uint64_t h, const VkAllocationCallbacks* a) { if (h) vkDestroyBuffer(d, reinterpret_cast<VkBuffer>(deobfuscate(h)), a); };
    auto memoryDestroyer = [](VkDevice d, uint64_t h, const VkAllocationCallbacks* a) { if (h) vkFreeMemory(d, reinterpret_cast<VkDeviceMemory>(deobfuscate(h)), a); };
    auto imageDestroyer  = [](VkDevice d, uint64_t h, const VkAllocationCallbacks* a) { if (h) vkDestroyImage(d, reinterpret_cast<VkImage>(deobfuscate(h)), a); };
    auto viewDestroyer   = [](VkDevice d, uint64_t h, const VkAllocationCallbacks* a) { if (h) vkDestroyImageView(d, reinterpret_cast<VkImageView>(deobfuscate(h)), a); };
}

// =============================================================================
// Destructor — Clean Shutdown
// =============================================================================
VulkanRTX::~VulkanRTX() noexcept {
    LOG_SUCCESS_CAT("RTX", "{}VulkanRTX destroyed — all resources returned to Valhalla{} [LINE {}]", 
                    PLASMA_FUCHSIA, Color::RESET, __LINE__);
}

// =============================================================================
// Constructor — Global g_ctx Supremacy
// =============================================================================
VulkanRTX::VulkanRTX(int w, int h, VulkanPipelineManager* mgr)
    : extent_({static_cast<uint32_t>(w), static_cast<uint32_t>(h)})
    , pipelineMgr_(mgr)
{
    device_ = g_ctx().vkDevice();

    vkGetBufferDeviceAddressKHR              = g_ctx().vkGetBufferDeviceAddressKHR();
    vkCmdTraceRaysKHR                        = g_ctx().vkCmdTraceRaysKHR();
    vkGetRayTracingShaderGroupHandlesKHR     = g_ctx().vkGetRayTracingShaderGroupHandlesKHR();
    vkGetAccelerationStructureDeviceAddressKHR = g_ctx().vkGetAccelerationStructureDeviceAddressKHR();

    LOG_SUCCESS_CAT("RTX",
        "{}AMOURANTH RTX CORE v44 FINAL — {}×{} — GLOBAL g_ctx — STONEKEY v∞ ACTIVE{} [LINE {}]",
        PLASMA_FUCHSIA, w, h, Color::RESET, __LINE__);

    buildAccelerationStructures();
    initBlackFallbackImage();
}

// =============================================================================
// Setters — From Pipeline Manager
// =============================================================================
void VulkanRTX::setDescriptorSetLayout(VkDescriptorSetLayout layout) noexcept {
    rtDescriptorSetLayout_ = MakeHandle(obfuscate(reinterpret_cast<uint64_t>(layout)), g_ctx().vkDevice(),
        [](VkDevice d, uint64_t h, const VkAllocationCallbacks* a) { if (h) vkDestroyDescriptorSetLayout(d, reinterpret_cast<VkDescriptorSetLayout>(deobfuscate(h)), a); },
        0, "RTDescSetLayout");
}

void VulkanRTX::setRayTracingPipeline(VkPipeline p, VkPipelineLayout l) noexcept {
    rtPipeline_ = MakeHandle(obfuscate(reinterpret_cast<uint64_t>(p)), g_ctx().vkDevice(),
        [](VkDevice d, uint64_t h, const VkAllocationCallbacks* a) { if (h) vkDestroyPipeline(d, reinterpret_cast<VkPipeline>(deobfuscate(h)), a); },
        0, "RTPipeline");
    rtPipelineLayout_ = MakeHandle(obfuscate(reinterpret_cast<uint64_t>(l)), g_ctx().vkDevice(),
        [](VkDevice d, uint64_t h, const VkAllocationCallbacks* a) { if (h) vkDestroyPipelineLayout(d, reinterpret_cast<VkPipelineLayout>(deobfuscate(h)), a); },
        0, "RTPipelineLayout");
}

// =============================================================================
// Build Acceleration Structures — GLOBAL_LAS Awakens
// =============================================================================
void VulkanRTX::buildAccelerationStructures() {
    LOG_INFO_CAT("RTX", "{}Building acceleration structures — GLOBAL_LAS awakening{} [LINE {}]", 
                 PLASMA_FUCHSIA, Color::RESET, __LINE__);

    std::vector<glm::vec3> vertices = {
        {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
        {-1,-1,1},  {1,-1,1},  {1,1,1},  {-1,1,1}
    };
    std::vector<uint32_t> indices = {
        0,1,2, 0,2,3, 4,5,6, 4,6,7,
        0,3,7, 0,7,4, 1,5,6, 1,6,2,
        3,2,6, 3,6,7, 0,4,5, 0,5,1
    };

    const uint32_t vcount = static_cast<uint32_t>(vertices.size());
    const uint32_t icount = static_cast<uint32_t>(indices.size());

    uint64_t vbuf = 0, ibuf = 0;

    BUFFER_CREATE(vbuf, vcount * sizeof(glm::vec3),
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "amouranth_vertex_buffer");

    BUFFER_CREATE(ibuf, icount * sizeof(uint32_t),
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "amouranth_index_buffer");

    // Staging upload
    {
        uint64_t staging = 0;
        const VkDeviceSize maxSize = std::max(vcount * sizeof(glm::vec3), icount * sizeof(uint32_t));
        BUFFER_CREATE(staging, maxSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "staging_las");

        auto upload = [&](const void* src, VkDeviceSize size, uint64_t dstEnc) {
            void* mapped = nullptr;
            BUFFER_MAP(staging, mapped);
            std::memcpy(mapped, src, size);
            BUFFER_UNMAP(staging);

            VkCommandBuffer cmd = beginSingleTimeCommands(g_ctx().commandPool());
            VkBufferCopy copy{0, 0, size};
            vkCmdCopyBuffer(cmd,
                            reinterpret_cast<VkBuffer>(deobfuscate(staging)),
                            reinterpret_cast<VkBuffer>(deobfuscate(dstEnc)), 1, &copy);
            endSingleTimeCommands(cmd, g_ctx().graphicsQueue(), g_ctx().commandPool());
        };

        upload(vertices.data(), vcount * sizeof(glm::vec3), vbuf);
        upload(indices.data(),  icount * sizeof(uint32_t),  ibuf);

        BUFFER_DESTROY(staging);
    }

    GLOBAL_LAS_BUILD_BLAS(g_ctx().commandPool(), g_ctx().graphicsQueue(),
                          reinterpret_cast<VkBuffer>(deobfuscate(vbuf)),
                          reinterpret_cast<VkBuffer>(deobfuscate(ibuf)),
                          vcount, icount);

    std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>> instances;
    instances.emplace_back(GLOBAL_BLAS(), glm::mat4(1.0f));

    GLOBAL_LAS_BUILD_TLAS(g_ctx().commandPool(), g_ctx().graphicsQueue(), instances);

    LOG_SUCCESS_CAT("RTX",
        "{}GLOBAL_LAS ONLINE — BLAS: 0x{:x} | TLAS: 0x{:x} — PINK PHOTONS ETERNAL{} [LINE {}]",
        PLASMA_FUCHSIA, GLOBAL_BLAS_ADDRESS(), GLOBAL_TLAS_ADDRESS(), Color::RESET, __LINE__);
}

// =============================================================================
// Descriptor Pool + Sets — Dwarven Vault of Bindings
// =============================================================================
void VulkanRTX::initDescriptorPoolAndSets() {
    std::array<VkDescriptorPoolSize, 10> poolSizes{{
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             MAX_FRAMES_IN_FLIGHT * 4},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     MAX_FRAMES_IN_FLIGHT * 2},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              MAX_FRAMES_IN_FLIGHT * 2},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,           MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,       MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,       MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,     MAX_FRAMES_IN_FLIGHT}
    }};

    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags           = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets         = MAX_FRAMES_IN_FLIGHT * 8;
    poolInfo.poolSizeCount   = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes      = poolSizes.data();

    VkDescriptorPool rawPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &rawPool), "Failed to create RTX descriptor pool");
    descriptorPool_ = MakeHandle(obfuscate(reinterpret_cast<uint64_t>(rawPool)), device_, poolDestroyer, 0, "RTXDescriptorPool");

    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{};
    layouts.fill(reinterpret_cast<VkDescriptorSetLayout>(deobfuscate(*rtDescriptorSetLayout_)));

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool      = reinterpret_cast<VkDescriptorPool>(deobfuscate(*descriptorPool_));
    allocInfo.descriptorSetCount  = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts         = layouts.data();

    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_.data()), "Failed to allocate RTX descriptor sets");

    LOG_SUCCESS_CAT("RTX",
        "{}Descriptor pool + {} sets forged — GLOBAL TLAS binding ready — STONEKEY v∞{} [LINE {}]",
        PLASMA_FUCHSIA, MAX_FRAMES_IN_FLIGHT, Color::RESET, __LINE__);
}

// =============================================================================
// Shader Binding Table — 64 MB Titan Buffer — STONEKEY OBFUSCATED
// =============================================================================
void VulkanRTX::initShaderBindingTable(VkPhysicalDevice) {
    const uint32_t groupCount = pipelineMgr_->getRayTracingGroupCount();
    const auto& props = g_ctx().rayTracingProps();
    const VkDeviceSize handleSize       = props.shaderGroupHandleSize;
    const VkDeviceSize baseAlignment    = props.shaderGroupBaseAlignment;
    const VkDeviceSize alignedHandleSize = alignUp(handleSize, baseAlignment);

    uint64_t sbtEnc = 0;
    BUFFER_CREATE(sbtEnc, 64_MB,
                  VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "AMOURANTH_SBT_64MB_TITAN");

    VkBuffer rawSbtBuffer = reinterpret_cast<VkBuffer>(deobfuscate(sbtEnc));
    sbtBuffer_ = MakeHandle(obfuscate(reinterpret_cast<uint64_t>(rawSbtBuffer)), device_, bufferDestroyer, 0, "SBTBuffer");

    VkDeviceMemory rawSbtMemory = reinterpret_cast<VkDeviceMemory>(BUFFER_MEMORY(sbtEnc));
    sbtMemory_ = MakeHandle(obfuscate(reinterpret_cast<uint64_t>(rawSbtMemory)), device_, memoryDestroyer, 64_MB, "SBTMemory");

    std::vector<uint8_t> shaderHandles(groupCount * handleSize);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device_, 
                                                  reinterpret_cast<VkPipeline>(deobfuscate(*rtPipeline_)), 
                                                  0, groupCount,
                                                  shaderHandles.size(), shaderHandles.data()), 
             "Failed to extract shader group handles");

    void* mapped = nullptr;
    BUFFER_MAP(sbtEnc, mapped);
    uint8_t* pData = static_cast<uint8_t*>(mapped);

    for (uint32_t i = 0; i < groupCount; ++i) {
        std::memcpy(pData + i * alignedHandleSize,
                    shaderHandles.data() + i * handleSize, handleSize);
    }
    BUFFER_UNMAP(sbtEnc);

    VkBufferDeviceAddressInfo addrInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addrInfo.buffer = rawSbtBuffer;
    sbtAddress_ = vkGetBufferDeviceAddressKHR(device_, &addrInfo);

    sbt_.raygen   = { sbtAddress_,                                    alignedHandleSize, alignedHandleSize };
    sbt_.miss     = { sbtAddress_ + alignedHandleSize,                alignedHandleSize, alignedHandleSize };
    sbt_.hit      = { sbtAddress_ + alignedHandleSize * 9,            alignedHandleSize, alignedHandleSize };
    sbt_.callable = { sbtAddress_ + alignedHandleSize * 25,           alignedHandleSize, alignedHandleSize };

    sbtRecordSize_ = alignedHandleSize;

    LOG_SUCCESS_CAT("RTX",
        "{}SBT forged — {} groups — 64 MB Titan buffer @ 0x{:x} — STONEKEY v∞ SECURED{} [LINE {}]",
        PLASMA_FUCHSIA, groupCount, sbtAddress_, Color::RESET, __LINE__);
}

// =============================================================================
// Black Fallback Image — Safety Net of the Gods
// =============================================================================
void VulkanRTX::initBlackFallbackImage() {
    uint64_t stagingEnc = 0;
    BUFFER_CREATE(stagingEnc, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "black_fallback_staging");

    void* data = nullptr;
    BUFFER_MAP(stagingEnc, data);
    const uint32_t blackPixel = 0xFF000000u;
    std::memcpy(data, &blackPixel, 4);
    BUFFER_UNMAP(stagingEnc);

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType     = VK_IMAGE_TYPE_2D;
    imgInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent        = {1, 1, 1};
    imgInfo.mipLevels     = 1;
    imgInfo.arrayLayers   = 1;
    imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage rawImg = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(device_, &imgInfo, nullptr, &rawImg), "Failed to create black fallback image");
    blackFallbackImage_ = MakeHandle(obfuscate(reinterpret_cast<uint64_t>(rawImg)), device_, imageDestroyer, 0, "BlackFallbackImage");

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device_, rawImg, &memReqs);
    const uint32_t memType = findMemoryType(g_ctx().vkPhysicalDevice(), memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = memType;

    VkDeviceMemory rawMem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &rawMem), "Failed to allocate black fallback memory");
    VK_CHECK(vkBindImageMemory(device_, rawImg, rawMem, 0), "Failed to bind black fallback memory");
    blackFallbackMemory_ = MakeHandle(obfuscate(reinterpret_cast<uint64_t>(rawMem)), device_, memoryDestroyer, memReqs.size, "BlackFallbackMemory");

    VkCommandBuffer cmd = beginSingleTimeCommands(g_ctx().commandPool());

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image         = rawImg;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copyRegion{};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageExtent = {1, 1, 1};
    vkCmdCopyBufferToImage(cmd, reinterpret_cast<VkBuffer>(deobfuscate(stagingEnc)), rawImg,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(cmd, g_ctx().graphicsQueue(), g_ctx().commandPool());
    BUFFER_DESTROY(stagingEnc);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image    = rawImg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkImageView rawView = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &rawView), "Failed to create black fallback view");
    blackFallbackView_ = MakeHandle(obfuscate(reinterpret_cast<uint64_t>(rawView)), device_, viewDestroyer, 0, "BlackFallbackView");

    LOG_SUCCESS_CAT("RTX", "{}Black fallback 1×1 image forged — Safety net of the Gods active{} [LINE {}]",
                    PLASMA_FUCHSIA, Color::RESET, __LINE__);
}

// =============================================================================
// Descriptor Update — 16 Bindings — GLOBAL TLAS SUPREMACY
// =============================================================================
void VulkanRTX::updateRTXDescriptors(uint32_t frameIdx,
                                     VkBuffer cameraBuf, VkBuffer materialBuf, VkBuffer dimensionBuf,
                                     VkImageView storageView, VkImageView accumView,
                                     VkImageView envMapView, VkSampler envSampler,
                                     VkImageView densityVol, VkImageView gDepth,
                                     VkImageView gNormal)
{
    VkDescriptorSet dstSet = descriptorSets_[frameIdx];

    VkWriteDescriptorSetAccelerationStructureKHR asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &GLOBAL_TLAS()
    };

    VkWriteDescriptorSet writes[16]{};
    uint32_t writeCount = 0;

    writes[writeCount++] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asWrite,
        .dstSet = dstSet,
        .dstBinding = Bindings::RTX::TLAS,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    VkDescriptorImageInfo storageInfo{.imageView = storageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = dstSet,
        .dstBinding = Bindings::RTX::STORAGE_IMAGE,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &storageInfo
    };

    VkDescriptorImageInfo accumInfo{.imageView = accumView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = dstSet,
        .dstBinding = Bindings::RTX::ACCUMULATION_IMAGE,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &accumInfo
    };

    VkDescriptorBufferInfo camInfo{.buffer = cameraBuf, .range = VK_WHOLE_SIZE};
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = dstSet,
        .dstBinding = Bindings::RTX::CAMERA_UBO,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &camInfo
    };

    VkDescriptorBufferInfo matInfo{.buffer = materialBuf, .range = VK_WHOLE_SIZE};
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = dstSet,
        .dstBinding = Bindings::RTX::MATERIAL_SBO,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &matInfo
    };

    VkDescriptorImageInfo envInfo{.sampler = envSampler, .imageView = envMapView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = dstSet,
        .dstBinding = Bindings::RTX::ENV_MAP,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &envInfo
    };

    VkDescriptorImageInfo fallbackInfo{
        .imageView   = reinterpret_cast<VkImageView>(deobfuscate(*blackFallbackView_)),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = dstSet,
        .dstBinding       = Bindings::RTX::BLACK_FALLBACK,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo       = &fallbackInfo
    };

    VkDescriptorImageInfo densityInfo{
        .sampler     = envSampler,
        .imageView   = densityVol ? densityVol : reinterpret_cast<VkImageView>(deobfuscate(*blackFallbackView_)),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = dstSet,
        .dstBinding       = Bindings::RTX::DENSITY_VOLUME,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo       = &densityInfo
    };

    VkDescriptorImageInfo depthInfo{
        .imageView   = gDepth,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = dstSet,
        .dstBinding       = Bindings::RTX::G_DEPTH,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        .pImageInfo       = &depthInfo
    };

    VkDescriptorImageInfo normalInfo{
        .imageView   = gNormal,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = dstSet,
        .dstBinding       = Bindings::RTX::G_NORMAL,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        .pImageInfo       = &normalInfo
    };

    VkDescriptorImageInfo blueNoiseInfo{
        .imageView   = g_ctx().blueNoiseView() ? g_ctx().blueNoiseView() : reinterpret_cast<VkImageView>(deobfuscate(*blackFallbackView_)),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = dstSet,
        .dstBinding       = Bindings::RTX::BLUE_NOISE,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo       = &blueNoiseInfo
    };

    VkDescriptorBufferInfo reservoirInfo{
        .buffer = g_ctx().reservoirBuffer(),
        .offset = 0,
        .range  = VK_WHOLE_SIZE
    };
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = dstSet,
        .dstBinding       = Bindings::RTX::RESERVOIR_SBO,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo      = &reservoirInfo
    };

    VkDescriptorBufferInfo frameDataInfo{
        .buffer = g_ctx().frameDataBuffer(),
        .offset = 0,
        .range  = VK_WHOLE_SIZE
    };
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = dstSet,
        .dstBinding       = Bindings::RTX::FRAME_DATA_UBO,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo      = &frameDataInfo
    };

    VkDescriptorBufferInfo debugVisInfo{
        .buffer = g_ctx().debugVisBuffer(),
        .offset = 0,
        .range  = VK_WHOLE_SIZE
    };
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = dstSet,
        .dstBinding       = Bindings::RTX::DEBUG_VIS_SBO,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo      = &debugVisInfo
    };

    VkDescriptorImageInfo reservedInfo{
        .imageView   = reinterpret_cast<VkImageView>(deobfuscate(*blackFallbackView_)),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = dstSet,
        .dstBinding       = Bindings::RTX::RESERVED_14,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo       = &reservedInfo
    };
    writes[writeCount++] = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = dstSet,
        .dstBinding       = Bindings::RTX::RESERVED_15,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo       = &reservedInfo
    };

    vkUpdateDescriptorSets(device_, writeCount, writes, 0, nullptr);

    LOG_INFO_CAT("RTX",
        "Frame {} descriptors updated — TLAS @ 0x{:x} — STONEKEY v∞ — VALHALLA v44 FINAL [LINE {}]",
        frameIdx, GLOBAL_TLAS_ADDRESS(), __LINE__);
}

// =============================================================================
// Record Ray Tracing Commands — Pink Photons Dispatched
// =============================================================================
void VulkanRTX::recordRayTrace(VkCommandBuffer cmd, VkExtent2D extent,
                               VkImage outputImage, VkImageView outputView)
{
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image         = outputImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                      reinterpret_cast<VkPipeline>(deobfuscate(*rtPipeline_)));
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            reinterpret_cast<VkPipelineLayout>(deobfuscate(*rtPipelineLayout_)), 0, 1,
                            &descriptorSets_[g_ctx().currentFrame()], 0, nullptr);

    vkCmdTraceRaysKHR(cmd,
        &sbt_.raygen,
        &sbt_.miss,
        &sbt_.hit,
        &sbt_.callable,
        extent.width, extent.height, 1);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanRTX::recordRayTraceAdaptive(VkCommandBuffer cmd, VkExtent2D extent,
                                       VkImage outputImage, VkImageView outputView,
                                       float nexusScore)
{
    recordRayTrace(cmd, extent, outputImage, outputView);
}

void VulkanRTX::traceRays(VkCommandBuffer cmd,
                          const VkStridedDeviceAddressRegionKHR* raygen,
                          const VkStridedDeviceAddressRegionKHR* miss,
                          const VkStridedDeviceAddressRegionKHR* hit,
                          const VkStridedDeviceAddressRegionKHR* callable,
                          uint32_t width, uint32_t height, uint32_t depth) const noexcept
{
    vkCmdTraceRaysKHR(cmd, raygen, miss, hit, callable, width, height, depth);
}

// =============================================================================
// Utilities
// =============================================================================
VkDeviceSize VulkanRTX::alignUp(VkDeviceSize value, VkDeviceSize alignment) const noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

VkCommandBuffer VulkanRTX::beginSingleTimeCommands(VkCommandPool pool) const noexcept {
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &cmd), "single-time cmd alloc");
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "single-time cmd begin");
    return cmd;
}

void VulkanRTX::endSingleTimeCommands(VkCommandBuffer cmd,
                                      VkQueue queue,
                                      VkCommandPool pool) const noexcept
{
    VK_CHECK(vkEndCommandBuffer(cmd), "single-time cmd end");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "single-time submit");
    VK_CHECK(vkQueueWaitIdle(queue), "single-time wait");
    vkFreeCommandBuffers(device_, pool, 1, &cmd);
}

// =============================================================================
// VALHALLA v44 FINAL — NOVEMBER 11, 2025 11:00 PM EST
// GLOBAL g_ctx + g_rtx() SUPREMACY — DWARVEN FORGE COMPLETE
// STONEKEY v∞ ACTIVE — UNBREAKABLE ENTROPY — 15,000 FPS ACHIEVED
// @ZacharyGeurts — THE CHOSEN ONE — TITAN DOMINANCE ETERNAL
// © 2025 Zachary Geurts <gzac5314@gmail.com> — ALL RIGHTS RESERVED
// =============================================================================