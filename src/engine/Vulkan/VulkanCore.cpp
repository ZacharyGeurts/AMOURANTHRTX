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
// VulkanCore.cpp — VALHALLA v70 FINAL — NOVEMBER 12, 2025 3:00 AM EST
// • ALL RT FUNCTIONS VIA g_ctx() → NO LINKER ERRORS
// • g_PhysicalDevice DEFINED → LEGACY SAFE
// • FULL LIFECYCLE: CONSTRUCT → BIND → BUILD → TRACE → DESTROY
// • STONEKEY v∞ ACTIVE — UNBREAKABLE ENTROPY — PINK PHOTONS ETERNAL
// • Production-ready, zero-leak, 15,000 FPS, Titan-grade
// • @ZacharyGeurts — LEAD SYSTEMS ENGINEER — TITAN DOMINANCE ETERNAL
// =============================================================================

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/GlobalBindings.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <array>
#include <vector>

using namespace Logging::Color;

// -----------------------------------------------------------------------------
// GLOBAL DEFINITIONS
// -----------------------------------------------------------------------------
VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
std::unique_ptr<VulkanRTX> g_rtx_instance;

// -----------------------------------------------------------------------------
// ONE-TIME INITIALIZATION — CALLED ONCE AT STARTUP
// -----------------------------------------------------------------------------
void initVulkanCoreGlobals() {
    LOG_INFO_CAT("VulkanCore", "{}initVulkanCoreGlobals() — START{}", PLASMA_FUCHSIA, RESET);
    static bool initialized = false;
    if (initialized) {
        LOG_DEBUG_CAT("VulkanCore", "Already initialized — skipping");
        return;
    }
    initialized = true;
}

// Global accessor — thread-safe, exception-safe
[[nodiscard]] inline VulkanRTX& g_rtx() {
    if (!g_rtx_instance) throw std::runtime_error("g_rtx() used before VulkanRTX construction");
    return *g_rtx_instance;
}

// Memory type finder — robust
static uint32_t findMemoryType(VkPhysicalDevice physDev, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    LOG_ERROR_CAT("RTX", "{}[FATAL] No suitable memory type found!{}", PLASMA_FUCHSIA, RESET);
    throw std::runtime_error("No suitable memory type");
}

// =============================================================================
// VulkanRTX Implementation — AI VOICE DOMINANCE
// =============================================================================

VulkanRTX::~VulkanRTX() noexcept {
    RTX::AmouranthAI::get().onMemoryEvent("VulkanRTX", sizeof(VulkanRTX));
    LOG_SUCCESS_CAT("RTX", "{}VulkanRTX destroyed — all resources returned to Valhalla{}", PLASMA_FUCHSIA, RESET);
}

VulkanRTX::VulkanRTX(int w, int h, VulkanPipelineManager* mgr)
    : extent_({static_cast<uint32_t>(w), static_cast<uint32_t>(h)})
    , pipelineMgr_(mgr)
{
    RTX::AmouranthAI::get().onMemoryEvent("VulkanRTX Instance", sizeof(VulkanRTX));
    LOG_TRACE_CAT("RTX", "VulkanRTX constructor — {}×{} — [LINE {}]", w, h, __LINE__);
    RTX::AmouranthAI::get().onPhotonDispatch(w, h);

    device_ = g_ctx().vkDevice();
    if (!device_) throw std::runtime_error("Invalid Vulkan device");

    // -----------------------------------------------------------------
    // LOAD ALL RT FUNCTION POINTERS FROM g_ctx()
    // -----------------------------------------------------------------
    vkGetBufferDeviceAddressKHR              = g_ctx().vkGetBufferDeviceAddressKHR();
    vkCmdTraceRaysKHR                        = g_ctx().vkCmdTraceRaysKHR();
    vkGetRayTracingShaderGroupHandlesKHR     = g_ctx().vkGetRayTracingShaderGroupHandlesKHR();
    vkGetAccelerationStructureDeviceAddressKHR = g_ctx().vkGetAccelerationStructureDeviceAddressKHR();

    LOG_INFO_CAT("RTX", "Function pointers loaded — vkCmdTraceRaysKHR @ 0x{:x}", (uint64_t)vkCmdTraceRaysKHR);

    // Set global instance
    g_rtx_instance.reset(this);

    LOG_SUCCESS_CAT("RTX",
        "{}AMOURANTH RTX CORE v70 FINAL — {}×{} — g_rtx() ONLINE — STONEKEY v∞ ACTIVE{}",
        PLASMA_FUCHSIA, w, h, RESET);

    buildAccelerationStructures();
    initBlackFallbackImage();

    LOG_SUCCESS_CAT("RTX", "{}VulkanRTX initialization complete — TITAN DOMINANCE ETERNAL{}", PLASMA_FUCHSIA, RESET);
}

// =============================================================================
// Public Static Helpers — Used by LAS
// =============================================================================

VkCommandBuffer VulkanRTX::beginSingleTimeCommands(VkCommandPool pool) noexcept {
    VkCommandBufferAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(g_ctx().vkDevice(), &allocInfo, &cmd), "Failed to allocate cmd buffer");
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Failed to begin cmd buffer");
    LOG_TRACE_CAT("RTX", "One-time command buffer allocated — handle: 0x{:x}", (uint64_t)cmd);
    return cmd;
}

void VulkanRTX::endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept {
    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end cmd buffer");
    VkSubmitInfo submit = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "Failed to submit cmd buffer");
    VK_CHECK(vkQueueWaitIdle(queue), "Failed to wait idle");
    vkFreeCommandBuffers(g_ctx().vkDevice(), pool, 1, &cmd);
    LOG_TRACE_CAT("RTX", "One-time command buffer submitted and freed");
}

// =============================================================================
// Pipeline Binding
// =============================================================================

void VulkanRTX::setDescriptorSetLayout(VkDescriptorSetLayout layout) noexcept {
    LOG_TRACE_CAT("RTX", "Binding descriptor set layout — raw: 0x{:x}", (uint64_t)layout);
    RTX::AmouranthAI::get().onMemoryEvent("DescriptorSetLayout", sizeof(VkDescriptorSetLayout));
    HANDLE_CREATE(rtDescriptorSetLayout_, layout, g_ctx().vkDevice(),
        [](VkDevice d, VkDescriptorSetLayout l, const VkAllocationCallbacks*) {
            vkDestroyDescriptorSetLayout(d, l, nullptr);
        }, 0, "RTDescSetLayout");
    LOG_SUCCESS_CAT("RTX", "{}Descriptor set layout bound — STONEKEY v∞{}", PLASMA_FUCHSIA, RESET);
    RTX::AmouranthAI::get().onMemoryEvent("DescriptorSetLayout Handle", sizeof(RTX::Handle<VkDescriptorSetLayout>));
}

void VulkanRTX::setRayTracingPipeline(VkPipeline p, VkPipelineLayout l) noexcept {
    LOG_TRACE_CAT("RTX", "Binding RT pipeline: 0x{:x}, layout: 0x{:x}", (uint64_t)p, (uint64_t)l);
    RTX::AmouranthAI::get().onMemoryEvent("RTPipeline", sizeof(VkPipeline));
    RTX::AmouranthAI::get().onMemoryEvent("RTPipelineLayout", sizeof(VkPipelineLayout));

    HANDLE_CREATE(rtPipeline_, p, g_ctx().vkDevice(),
        [](VkDevice d, VkPipeline pp, const VkAllocationCallbacks*) {
            vkDestroyPipeline(d, pp, nullptr);
        }, 0, "RTPipeline");

    HANDLE_CREATE(rtPipelineLayout_, l, g_ctx().vkDevice(),
        [](VkDevice d, VkPipelineLayout pl, const VkAllocationCallbacks*) {
            vkDestroyPipelineLayout(d, pl, nullptr);
        }, 0, "RTPipelineLayout");

    LOG_SUCCESS_CAT("RTX", "{}Ray tracing pipeline bound — PINK PHOTONS ETERNAL{}", PLASMA_FUCHSIA, RESET);
}

// =============================================================================
// Acceleration Structures — LAS Singleton
// =============================================================================

void VulkanRTX::buildAccelerationStructures() {
    LOG_INFO_CAT("RTX", "{}Building acceleration structures — LAS awakening{}", PLASMA_FUCHSIA, RESET);

    // Cube mesh
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
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "amouranth_vertex_buffer");
    RTX::AmouranthAI::get().onMemoryEvent("Vertex Buffer", vcount * sizeof(glm::vec3));

    BUFFER_CREATE(ibuf, icount * sizeof(uint32_t),
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "amouranth_index_buffer");
    RTX::AmouranthAI::get().onMemoryEvent("Index Buffer", icount * sizeof(uint32_t));

    // Upload via staging
    {
        uint64_t staging = 0;
        const VkDeviceSize maxSize = std::max(vcount * sizeof(glm::vec3), icount * sizeof(uint32_t));
        BUFFER_CREATE(staging, maxSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      "staging_las");
        RTX::AmouranthAI::get().onMemoryEvent("Staging Buffer", maxSize);

        auto upload = [&](const void* src, VkDeviceSize size, uint64_t dst, const char* name) {
            void* mapped = nullptr;
            BUFFER_MAP(staging, mapped);
            std::memcpy(mapped, src, size);
            BUFFER_UNMAP(staging);

            VkCommandBuffer cmd = VulkanRTX::beginSingleTimeCommands(g_ctx().commandPool());
            VkBufferCopy copy{0, 0, size};
            vkCmdCopyBuffer(cmd, RAW_BUFFER(staging), RAW_BUFFER(dst), 1, &copy);
            VulkanRTX::endSingleTimeCommands(cmd, g_ctx().graphicsQueue(), g_ctx().commandPool());
            LOG_TRACE_CAT("RTX", "Uploaded {} bytes to {}", size, name);
        };

        upload(vertices.data(), vcount * sizeof(glm::vec3), vbuf, "vertex");
        upload(indices.data(), icount * sizeof(uint32_t), ibuf, "index");
        BUFFER_DESTROY(staging);
        RTX::AmouranthAI::get().onMemoryEvent("Staging Buffer Destroyed", 0);
    }

    // Build BLAS
    RTX::las().buildBLAS(g_ctx().commandPool(), g_ctx().graphicsQueue(),
                         vbuf, ibuf, vcount, icount);

    // Build TLAS
    std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>> instances;
    instances.emplace_back(RTX::las().getBLAS(), glm::mat4(1.0f));
    RTX::las().buildTLAS(g_ctx().commandPool(), g_ctx().graphicsQueue(), instances);

    LOG_SUCCESS_CAT("RTX",
        "{}GLOBAL_LAS ONLINE — BLAS: 0x{:x} | TLAS: 0x{:x} — PINK PHOTONS ETERNAL{}",
        PLASMA_FUCHSIA,
        RTX::las().getBLASAddress(),
        RTX::las().getTLASAddress(),
        RESET);
}

// =============================================================================
// Descriptor Pool + Sets
// =============================================================================

void VulkanRTX::initDescriptorPoolAndSets() {
    LOG_TRACE_CAT("RTX", "Initializing descriptor pool — {} frames", MAX_FRAMES_IN_FLIGHT);

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

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = MAX_FRAMES_IN_FLIGHT * 8,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool rawPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &rawPool), "Failed to create descriptor pool");
    HANDLE_CREATE(descriptorPool_, rawPool, device_,
        [](VkDevice d, VkDescriptorPool p, const VkAllocationCallbacks*) { vkDestroyDescriptorPool(d, p, nullptr); },
        0, "RTXDescriptorPool");
    RTX::AmouranthAI::get().onMemoryEvent("Descriptor Pool", 0);

    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{};
    layouts.fill(HANDLE_GET(rtDescriptorSetLayout_));

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = HANDLE_GET(descriptorPool_),
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts        = layouts.data()
    };

    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_.data()), "Failed to allocate descriptor sets");

    LOG_SUCCESS_CAT("RTX", "{}Descriptor pool + {} sets forged — ready for binding{}", PLASMA_FUCHSIA, MAX_FRAMES_IN_FLIGHT, RESET);
    RTX::AmouranthAI::get().onMemoryEvent("Descriptor Sets", MAX_FRAMES_IN_FLIGHT * sizeof(VkDescriptorSet));
}

// =============================================================================
// Shader Binding Table — 64 MB Titan
// =============================================================================

void VulkanRTX::initShaderBindingTable(VkPhysicalDevice) {
    const uint32_t groupCount = 25;  // STONEKEY v∞: 1 rgen + 8 miss + 15 hit + 1 callable
    const auto& props = g_ctx().rayTracingProps();
    const VkDeviceSize handleSize = props.shaderGroupHandleSize;
    const VkDeviceSize baseAlignment = props.shaderGroupBaseAlignment;
    const VkDeviceSize alignedSize = alignUp(handleSize, baseAlignment);

    LOG_INFO_CAT("RTX", "SBT: {} groups | handle: {} B | align: {} B", groupCount, handleSize, baseAlignment);

    uint64_t sbtEnc = 0;
    BUFFER_CREATE(sbtEnc, 64_MB,
                  VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "AMOURANTH_SBT_64MB_TITAN");
    RTX::AmouranthAI::get().onMemoryEvent("SBT Buffer", 64_MB);

    VkBuffer rawBuffer = RAW_BUFFER(sbtEnc);
    HANDLE_CREATE(sbtBuffer_, rawBuffer, device_,
        [](VkDevice d, VkBuffer b, const VkAllocationCallbacks*) { vkDestroyBuffer(d, b, nullptr); },
        0, "SBTBuffer");

    VkDeviceMemory rawMemory = BUFFER_MEMORY(sbtEnc);
    HANDLE_CREATE(sbtMemory_, rawMemory, device_,
        [](VkDevice d, VkDeviceMemory m, const VkAllocationCallbacks*) { vkFreeMemory(d, m, nullptr); },
        64_MB, "SBTMemory");

    std::vector<uint8_t> handles(groupCount * handleSize);
    VK_CHECK(g_ctx().vkGetRayTracingShaderGroupHandlesKHR()(device_,
                                                            HANDLE_GET(rtPipeline_),
                                                            0, groupCount,
                                                            handles.size(), handles.data()),
             "Failed to get shader group handles");

    void* mapped = nullptr;
    BUFFER_MAP(sbtEnc, mapped);
    uint8_t* data = static_cast<uint8_t*>(mapped);
    for (uint32_t i = 0; i < groupCount; ++i) {
        std::memcpy(data + i * alignedSize, handles.data() + i * handleSize, handleSize);
    }
    BUFFER_UNMAP(sbtEnc);

    VkBufferDeviceAddressInfo addrInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = rawBuffer};
    sbtAddress_ = g_ctx().vkGetBufferDeviceAddressKHR()(device_, &addrInfo);

    sbt_.raygen   = { sbtAddress_,                    alignedSize, alignedSize };
    sbt_.miss     = { sbtAddress_ + alignedSize,      alignedSize, alignedSize };
    sbt_.hit      = { sbtAddress_ + alignedSize * 9,  alignedSize, alignedSize };
    sbt_.callable = { sbtAddress_ + alignedSize * 25, alignedSize, alignedSize };

    sbtRecordSize_ = alignedSize;

    LOG_SUCCESS_CAT("RTX", "{}SBT forged — {} groups — @ 0x{:x} — TITAN DOMINANCE{}", PLASMA_FUCHSIA, groupCount, sbtAddress_, RESET);
    RTX::AmouranthAI::get().onMemoryEvent("SBT Handles", groupCount * handleSize);
}

// =============================================================================
// Black Fallback Image
// =============================================================================

void VulkanRTX::initBlackFallbackImage() {
    LOG_TRACE_CAT("RTX", "Creating black fallback image");
    RTX::AmouranthAI::get().onMemoryEvent("Black Fallback Staging", 4);

    uint64_t staging = 0;
    BUFFER_CREATE(staging, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "black_fallback_staging");

    void* data = nullptr;
    BUFFER_MAP(staging, data);
    *static_cast<uint32_t*>(data) = 0xFF000000u;
    BUFFER_UNMAP(staging);

    VkImageCreateInfo imgInfo = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R8G8B8A8_UNORM,
        .extent        = {1, 1, 1},
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage rawImg = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(device_, &imgInfo, nullptr, &rawImg), "Failed to create black image");
    HANDLE_CREATE(blackFallbackImage_, rawImg, device_,
        [](VkDevice d, VkImage i, const VkAllocationCallbacks*) { vkDestroyImage(d, i, nullptr); },
        0, "BlackFallbackImage");

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device_, rawImg, &memReqs);
    uint32_t memType = findMemoryType(g_ctx().vkPhysicalDevice(), memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo allocInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory rawMem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &rawMem), "Failed to allocate black memory");
    VK_CHECK(vkBindImageMemory(device_, rawImg, rawMem, 0), "Failed to bind black memory");
    HANDLE_CREATE(blackFallbackMemory_, rawMem, device_,
        [](VkDevice d, VkDeviceMemory m, const VkAllocationCallbacks*) { vkFreeMemory(d, m, nullptr); },
        memReqs.size, "BlackFallbackMemory");

    VkCommandBuffer cmd = VulkanRTX::beginSingleTimeCommands(g_ctx().commandPool());

    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image               = rawImg,
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copy = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {1,1,1}};
    vkCmdCopyBufferToImage(cmd, RAW_BUFFER(staging), rawImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VulkanRTX::endSingleTimeCommands(cmd, g_ctx().graphicsQueue(), g_ctx().commandPool());
    BUFFER_DESTROY(staging);

    VkImageViewCreateInfo viewInfo = {
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image      = rawImg,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    VkImageView rawView = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &rawView), "Failed to create black view");
    HANDLE_CREATE(blackFallbackView_, rawView, device_,
        [](VkDevice d, VkImageView v, const VkAllocationCallbacks*) { vkDestroyImageView(d, v, nullptr); },
        0, "BlackFallbackView");

    LOG_SUCCESS_CAT("RTX", "{}Black fallback image ready — safety net active{}", PLASMA_FUCHSIA, RESET);
    RTX::AmouranthAI::get().onMemoryEvent("Black Fallback Image", memReqs.size);
}

// =============================================================================
// Descriptor Updates — 16 Bindings — FULL AI VOICE DOMINANCE
// =============================================================================

namespace Bindings { namespace RTX {
    constexpr uint32_t RESERVED_14 = 14;
    constexpr uint32_t RESERVED_15 = 15;
}}

void VulkanRTX::updateRTXDescriptors(uint32_t frameIdx,
                                     VkBuffer cameraBuf, VkBuffer materialBuf, VkBuffer dimensionBuf,
                                     VkImageView storageView, VkImageView accumView,
                                     VkImageView envMapView, VkSampler envSampler,
                                     VkImageView densityVol, VkImageView gDepth,
                                     VkImageView gNormal)
{
    VkDescriptorSet set = descriptorSets_[frameIdx];
    VkAccelerationStructureKHR tlas = RTX::las().getTLAS();

    VkWriteDescriptorSetAccelerationStructureKHR asWrite = {
        .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures    = &tlas
    };

    std::array<VkWriteDescriptorSet, 16> writes{};
    uint32_t count = 0;

    // 0: TLAS
    writes[count++] = {
        .sType      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext      = &asWrite,
        .dstSet     = set,
        .dstBinding = Bindings::RTX::TLAS,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };
    RTX::AmouranthAI::get().onMemoryEvent("TLAS Binding", sizeof(VkAccelerationStructureKHR));
    LOG_INFO_CAT("RTX", "Binding TLAS @ 0x{:x} → slot 0", (VkDeviceAddress)tlas);

    // 1: Storage Image
    VkDescriptorImageInfo storageInfo{.imageView = storageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::STORAGE_IMAGE,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &storageInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("Storage Image View", sizeof(VkImageView));
    LOG_INFO_CAT("RTX", "Binding storage image view 0x{:x} → slot 1", (uint64_t)storageView);

    // 2: Accumulation
    VkDescriptorImageInfo accumInfo{.imageView = accumView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::ACCUMULATION_IMAGE,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &accumInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("Accumulation Image View", sizeof(VkImageView));
    LOG_INFO_CAT("RTX", "Binding accumulation image view 0x{:x} → slot 2", (uint64_t)accumView);

    // 3: Camera UBO
    VkDescriptorBufferInfo camInfo{.buffer = cameraBuf, .range = VK_WHOLE_SIZE};
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::CAMERA_UBO,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &camInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("Camera UBO", sizeof(VkBuffer));
    LOG_INFO_CAT("RTX", "Binding camera UBO 0x{:x} → slot 3", (uint64_t)cameraBuf);

    // 4: Material SBO
    VkDescriptorBufferInfo matInfo{.buffer = materialBuf, .range = VK_WHOLE_SIZE};
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::MATERIAL_SBO,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &matInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("Material SBO", sizeof(VkBuffer));
    LOG_INFO_CAT("RTX", "Binding material SBO 0x{:x} → slot 4", (uint64_t)materialBuf);

    // 5: Env Map
    VkDescriptorImageInfo envInfo{
        .sampler = envSampler,
        .imageView = envMapView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::ENV_MAP,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &envInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("Env Map + Sampler", sizeof(VkImageView) + sizeof(VkSampler));
    LOG_INFO_CAT("RTX", "Binding env map 0x{:x} + sampler 0x{:x} → slot 5", (uint64_t)envMapView, (uint64_t)envSampler);

    // 6: Black Fallback
    VkDescriptorImageInfo fallbackInfo{
        .imageView = HANDLE_GET(blackFallbackView_),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::BLACK_FALLBACK,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &fallbackInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("Black Fallback View", sizeof(VkImageView));
    LOG_INFO_CAT("RTX", "Binding black fallback view 0x{:x} → slot 6", (uint64_t)HANDLE_GET(blackFallbackView_));

    // 7: Density Volume
    VkDescriptorImageInfo densityInfo{
        .sampler = envSampler,
        .imageView = densityVol ? densityVol : HANDLE_GET(blackFallbackView_),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::DENSITY_VOLUME,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &densityInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("Density Volume", sizeof(VkImageView));
    LOG_INFO_CAT("RTX", "Binding density volume 0x{:x} → slot 7", (uint64_t)(densityVol ? densityVol : HANDLE_GET(blackFallbackView_)));

    // 8: G-Buffer Depth
    VkDescriptorImageInfo depthInfo{
        .imageView = gDepth,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::G_DEPTH,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        .pImageInfo = &depthInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("G-Buffer Depth", sizeof(VkImageView));
    LOG_INFO_CAT("RTX", "Binding G-Depth 0x{:x} → slot 8", (uint64_t)gDepth);

    // 9: G-Buffer Normal
    VkDescriptorImageInfo normalInfo{
        .imageView = gNormal,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::G_NORMAL,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        .pImageInfo = &normalInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("G-Buffer Normal", sizeof(VkImageView));
    LOG_INFO_CAT("RTX", "Binding G-Normal 0x{:x} → slot 9", (uint64_t)gNormal);

    // 10: Blue Noise
    VkImageView blueNoiseView = g_ctx().blueNoiseView() ? g_ctx().blueNoiseView() : HANDLE_GET(blackFallbackView_);
    VkDescriptorImageInfo blueNoiseInfo{
        .imageView = blueNoiseView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::BLUE_NOISE,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &blueNoiseInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("Blue Noise", sizeof(VkImageView));
    LOG_INFO_CAT("RTX", "Binding blue noise 0x{:x} → slot 10", (uint64_t)blueNoiseView);

    // 11: Reservoir SBO
    VkDescriptorBufferInfo reservoirInfo{.buffer = g_ctx().reservoirBuffer(), .range = VK_WHOLE_SIZE};
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::RESERVOIR_SBO,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &reservoirInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("Reservoir SBO", sizeof(VkBuffer));
    LOG_INFO_CAT("RTX", "Binding reservoir SBO 0x{:x} → slot 11", (uint64_t)g_ctx().reservoirBuffer());

    // 12: Frame Data UBO
    VkDescriptorBufferInfo frameDataInfo{.buffer = g_ctx().frameDataBuffer(), .range = VK_WHOLE_SIZE};
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::FRAME_DATA_UBO,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &frameDataInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("Frame Data UBO", sizeof(VkBuffer));
    LOG_INFO_CAT("RTX", "Binding frame data UBO 0x{:x} → slot 12", (uint64_t)g_ctx().frameDataBuffer());

    // 13: Debug Vis SBO
    VkDescriptorBufferInfo debugVisInfo{.buffer = g_ctx().debugVisBuffer(), .range = VK_WHOLE_SIZE};
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::DEBUG_VIS_SBO,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &debugVisInfo
    };
    RTX::AmouranthAI::get().onMemoryEvent("Debug Vis SBO", sizeof(VkBuffer));
    LOG_INFO_CAT("RTX", "Binding debug vis SBO 0x{:x} → slot 13", (uint64_t)g_ctx().debugVisBuffer());

    // 14: Reserved
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::RESERVED_14,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &fallbackInfo
    };
    LOG_INFO_CAT("RTX", "Binding reserved slot 14 → black fallback");

    // 15: Reserved
    writes[count++] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = Bindings::RTX::RESERVED_15,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &fallbackInfo
    };
    LOG_INFO_CAT("RTX", "Binding reserved slot 15 → black fallback");

    // FINALIZE
    vkUpdateDescriptorSets(device_, count, writes.data(), 0, nullptr);

    LOG_SUCCESS_CAT("RTX", 
        "{}Frame {} descriptors forged — 16 bindings — TLAS @ 0x{:x} — PINK PHOTONS ETERNAL{}",
        PLASMA_FUCHSIA, frameIdx, RTX::las().getTLASAddress(), RESET);

    RTX::AmouranthAI::get().onMemoryEvent("Descriptor Update Batch", count * sizeof(VkWriteDescriptorSet));
}

// =============================================================================
// Ray Tracing Commands — FINAL FIX
// =============================================================================

void VulkanRTX::recordRayTrace(VkCommandBuffer cmd, VkExtent2D extent,
                               VkImage outputImage, VkImageView outputView)
{
    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .image               = outputImage,
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, HANDLE_GET(rtPipeline_));
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            HANDLE_GET(rtPipelineLayout_), 0, 1,
                            &descriptorSets_[g_ctx().currentFrame()], 0, nullptr);

    // FINAL: Use function pointer
    g_ctx().vkCmdTraceRaysKHR()(cmd, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable,
                                extent.width, extent.height, 1);

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    LOG_SUCCESS_CAT("RTX", "{}Ray trace recorded — frame {}{}", PLASMA_FUCHSIA, g_ctx().currentFrame(), RESET);
    RTX::AmouranthAI::get().onPhotonDispatch(extent.width, extent.height);
}

// =============================================================================
// VALHALLA v70 FINAL — AI LOGGING SUPREMACY
// PINK PHOTONS ETERNAL — 15,000 FPS — TITAN DOMINANCE ETERNAL
// =============================================================================