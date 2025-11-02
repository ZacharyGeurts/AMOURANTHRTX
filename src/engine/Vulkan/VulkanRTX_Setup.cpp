// src/engine/Vulkan/VulkanRTX_Setup.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FULL: VulkanRTX implementation with RAII, OCEAN TEAL logging, FPS unlocked
// FIXED: All undefined references resolved
//        recordRayTracingCommands() implemented
//        SBT address caching
//        Black fallback image
//        Full VulkanRTX class body
//        rtPipeline_ / rtPipelineLayout_ → RAW Vk handles (non-owning)

#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Dispose.hpp"
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

using namespace Logging::Color;
using namespace Dispose;

namespace VulkanRTX {

/* -----------------------------------------------------------------
   Constructor – FULLY FIXED + MAX LOGGING
   CALLS: createDescriptorSetLayout() → createDescriptorPoolAndSet() → createBlackFallbackImage()
   rtPipeline_ / rtPipelineLayout_ → RAW Vk handles (non-owning)
   ----------------------------------------------------------------- */
VulkanRTX::VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height,
                     VulkanPipelineManager* pipelineMgr)
    : context_(std::move(ctx)),
      pipelineMgr_(pipelineMgr),
      device_(VK_NULL_HANDLE),
      physicalDevice_(VK_NULL_HANDLE),
      extent_{static_cast<uint32_t>(width), static_cast<uint32_t>(height)},

      // RAII Handles (must be initialized in order of declaration in header)
      dsLayout_(context_->device, nullptr, "RTX Desc Layout"),
      dsPool_(context_->device, nullptr, "RTX Desc Pool"),
      ds_(context_->device, VK_NULL_HANDLE, nullptr, "RTX Desc Set"),

      // NON-OWNING: rtPipeline_ and rtPipelineLayout_ are raw Vk handles
      rtPipeline_(VK_NULL_HANDLE),
      rtPipelineLayout_(VK_NULL_HANDLE),

      blas_(context_->device, nullptr, "BLAS"),
      tlas_(context_->device, nullptr, "TLAS"),
      blasBuffer_(context_->device, nullptr, "BLAS Buffer"),
      blasMemory_(context_->device, nullptr, "BLAS Memory"),
      tlasBuffer_(context_->device, nullptr, "TLAS Buffer"),
      tlasMemory_(context_->device, nullptr, "TLAS Memory"),

      sbtBuffer_(context_->device, nullptr, "SBT Buffer"),
      sbtMemory_(context_->device, nullptr, "SBT Memory"),

      blackFallbackImage_(context_->device, nullptr, "BLACK Fallback Image"),
      blackFallbackMemory_(context_->device, nullptr, "BLACK Fallback Memory"),
      blackFallbackView_(context_->device, nullptr, "BLACK Fallback View"),

      sbtBufferAddress_(0)
{
    LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX::VulkanRTX() — START [{}x{}]{}", 
                 OCEAN_TEAL, width, height, RESET);

    // --- Input validation ---
    if (!context_) throw VulkanRTXException("Null context");
    if (!pipelineMgr_) throw VulkanRTXException("Null pipeline manager");
    if (width <= 0 || height <= 0) throw VulkanRTXException("Invalid dimensions");

    // --- Device setup ---
    device_         = context_->device;
    physicalDevice_ = context_->physicalDevice;

    if (!device_) {
        LOG_ERROR_CAT("VulkanRTX", "NULL DEVICE IN CONSTRUCTOR!", CRIMSON_MAGENTA, RESET);
        throw VulkanRTXException("Null device");
    }

    /* ---------- LOAD ALL KHR EXTENSION FUNCTIONS ---------- */
    LOG_INFO_CAT("VulkanRTX", "Loading KHR function pointers...", OCEAN_TEAL, RESET);
#define LOAD_PROC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device_, #name)); \
    if (!name) { \
        LOG_ERROR_CAT("VulkanRTX", "Failed to load " #name, CRIMSON_MAGENTA, RESET); \
        throw VulkanRTXException("Failed to load " #name); \
    }

    LOAD_PROC(vkGetBufferDeviceAddress);
    LOAD_PROC(vkCmdTraceRaysKHR);
    LOAD_PROC(vkCreateAccelerationStructureKHR);
    LOAD_PROC(vkDestroyAccelerationStructureKHR);
    LOAD_PROC(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_PROC(vkCreateRayTracingPipelinesKHR);
    LOAD_PROC(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_PROC(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_PROC(vkCmdCopyAccelerationStructureKHR);
    LOAD_PROC(vkCmdBuildAccelerationStructuresKHR);
    
    // Descriptor KHR (safe to load even if not used directly)
    LOAD_PROC(vkCreateDescriptorSetLayout);
    LOAD_PROC(vkAllocateDescriptorSets);
    LOAD_PROC(vkCreateDescriptorPool);
    LOAD_PROC(vkDestroyDescriptorSetLayout);
    LOAD_PROC(vkDestroyDescriptorPool);
    LOAD_PROC(vkFreeDescriptorSets);

#undef LOAD_PROC
    /* ------------------------------------------------------ */

    // --- 1. CREATE DESCRIPTOR SET LAYOUT ---
    LOG_INFO_CAT("VulkanRTX", "{}Calling createDescriptorSetLayout()...{}", OCEAN_TEAL, RESET);
    createDescriptorSetLayout();
    LOG_INFO_CAT("VulkanRTX", "  → dsLayout_ @ {}", static_cast<void*>(dsLayout_.get()), RESET);

    // --- 2. CRITICAL: CREATE POOL + SET ---
    LOG_INFO_CAT("VulkanRTX", "{}CALLING createDescriptorPoolAndSet() NOW!{}", EMERALD_GREEN, RESET);
    createDescriptorPoolAndSet();
    LOG_INFO_CAT("VulkanRTX", "  → dsPool_ @ {}", static_cast<void*>(dsPool_.get()), RESET);
    LOG_INFO_CAT("VulkanRTX", "  → ds_ @ {}", static_cast<void*>(ds_.get()), RESET);

    if (ds_.get() == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanRTX", "ds_ STILL NULL AFTER createDescriptorPoolAndSet()!", CRIMSON_MAGENTA, RESET);
        throw VulkanRTXException("Descriptor set allocation failed");
    }

    // --- 3. BLACK FALLBACK IMAGE ---
    LOG_INFO_CAT("VulkanRTX", "{}Calling createBlackFallbackImage()...{}", OCEAN_TEAL, RESET);
    createBlackFallbackImage();
    LOG_INFO_CAT("VulkanRTX", "  → blackFallbackView_ @ {}", static_cast<void*>(blackFallbackView_.get()), RESET);

    LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX::VulkanRTX() — END{}", EMERALD_GREEN, RESET);
}

/* -----------------------------------------------------------------
   Destructor – RAII handles auto-clean
   ----------------------------------------------------------------- */
VulkanRTX::~VulkanRTX() {
    LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX::~VulkanRTX() — RAII cleanup complete{}", EMERALD_GREEN, RESET);
}

/* -----------------------------------------------------------------
   Private helpers
   ----------------------------------------------------------------- */
VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool) {
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

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) {
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

void VulkanRTX::uploadBlackPixelToImage(VkImage image) {
    auto cmd = allocateTransientCommandBuffer(context_->commandPool);
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
   createBlackFallbackImage – 1x1 black texture
   ----------------------------------------------------------------- */
void VulkanRTX::createBlackFallbackImage() {
    LOG_INFO_CAT("VulkanRTX", "{}createBlackFallbackImage() – START{}", OCEAN_TEAL, RESET);

    VkImageCreateInfo imgInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = {1, 1, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VkImage img;
    VK_CHECK(vkCreateImage(device_, &imgInfo, nullptr, &img), "Black fallback image");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device_, img, &memReq);
    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = VulkanInitializer::findMemoryType(
            physicalDevice_, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VkDeviceMemory mem;
    VK_CHECK(vkAllocateMemory(device_, &alloc, nullptr, &mem), "Black fallback mem");
    VK_CHECK(vkBindImageMemory(device_, img, mem, 0), "Bind black fallback");

    uploadBlackPixelToImage(img);

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VkImageView view;
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &view), "Black fallback view");

    blackFallbackImage_ = makeHandle(device_, img, "Black Fallback Image");
    blackFallbackMemory_ = makeHandle(device_, mem, "Black Fallback Memory");
    blackFallbackView_ = makeHandle(device_, view, "Black Fallback View");

    LOG_INFO_CAT("VulkanRTX", "{}Black fallback image created{}", EMERALD_GREEN, RESET);
}

/* -----------------------------------------------------------------
   createBottomLevelAS – FULL RAII
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

    /* ----- storage buffer ----- */
    Dispose::VulkanHandle<VkBuffer> asBuffer(device_, nullptr, "BLAS Storage Buffer");
    Dispose::VulkanHandle<VkDeviceMemory> asMemory(device_, nullptr, "BLAS Storage Memory");
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

    /* ----- scratch buffer ----- */
    Dispose::VulkanHandle<VkBuffer> scratchBuffer(device_, nullptr, "BLAS Scratch Buffer");
    Dispose::VulkanHandle<VkDeviceMemory> scratchMemory(device_, nullptr, "BLAS Scratch Memory");
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

    /* ----- store RAII handles ----- */
    blas_ = makeHandle(device_, blas, "BLAS");
    blas_.setDestroyFunc(vkDestroyAccelerationStructureKHR);
    blasBuffer_ = std::move(asBuffer);
    blasMemory_ = std::move(asMemory);

    LOG_INFO_CAT("VulkanRTX", "{}BLAS created @ 0x{:x}{}", EMERALD_GREEN,
                 reinterpret_cast<uintptr_t>(blas), RESET);
}

/* -----------------------------------------------------------------
   createTopLevelAS – FULL RAII
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

    /* ----- instance buffer (host-visible) ----- */
    Dispose::VulkanHandle<VkBuffer> instanceBuffer(device_, nullptr, "TLAS Instance Buffer");
    Dispose::VulkanHandle<VkDeviceMemory> instanceMemory(device_, nullptr, "TLAS Instance Memory");
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

    /* ----- TLAS storage buffer ----- */
    Dispose::VulkanHandle<VkBuffer> asBuffer(device_, nullptr, "TLAS Storage Buffer");
    Dispose::VulkanHandle<VkDeviceMemory> asMemory(device_, nullptr, "TLAS Storage Memory");
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

    /* ----- scratch buffer ----- */
    Dispose::VulkanHandle<VkBuffer> scratchBuffer(device_, nullptr, "TLAS Scratch Buffer");
    Dispose::VulkanHandle<VkDeviceMemory> scratchMemory(device_, nullptr, "TLAS Scratch Memory");
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

    /* ----- store RAII handles ----- */
    tlas_ = makeHandle(device_, tlas, "TLAS");
    tlas_.setDestroyFunc(vkDestroyAccelerationStructureKHR);
    tlasBuffer_ = std::move(asBuffer);
    tlasMemory_ = std::move(asMemory);

    LOG_INFO_CAT("VulkanRTX", "{}TLAS created @ 0x{:x}{}", EMERALD_GREEN,
                 reinterpret_cast<uintptr_t>(tlas), RESET);
}

/* -----------------------------------------------------------------
   createDescriptorSetLayout – ALL 11 BINDINGS
   ----------------------------------------------------------------- */
void VulkanRTX::createDescriptorSetLayout() {
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

    dsLayout_ = makeHandle(device_, layout, "RTX Desc Layout");
    LOG_INFO_CAT("VulkanRTX", "{}Descriptor set layout created{}", EMERALD_GREEN, RESET);
}

/* -----------------------------------------------------------------
   createDescriptorPoolAndSet – RAII
   ----------------------------------------------------------------- */
void VulkanRTX::createDescriptorPoolAndSet() {
    LOG_INFO_CAT("VulkanRTX", "{}createDescriptorPoolAndSet() – START{}", OCEAN_TEAL, RESET);
    LOG_INFO_CAT("VulkanRTX", "  → device_ @ {}", static_cast<void*>(device_), RESET);
    LOG_INFO_CAT("VulkanRTX", "  → dsLayout_ @ {}", static_cast<void*>(dsLayout_.get()), RESET);

    if (dsLayout_.get() == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanRTX", "dsLayout_ is NULL! Cannot create pool!", CRIMSON_MAGENTA, RESET);
        throw VulkanRTXException("Descriptor set layout missing");
    }

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

    VkDescriptorPool pool = VK_NULL_HANDLE;
    LOG_INFO_CAT("VulkanRTX", "vkCreateDescriptorPool() → CALL", OCEAN_TEAL, RESET);
    VkResult result = vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool);
    LOG_INFO_CAT("VulkanRTX", "vkCreateDescriptorPool() → result = {}", result, RESET);

    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRTX", "vkCreateDescriptorPool FAILED: {}", result, CRIMSON_MAGENTA, RESET);
        throw VulkanRTXException("Failed to create RTX descriptor pool");
    }

    dsPool_ = makeHandle(device_, pool, "RTX Desc Pool");
    LOG_INFO_CAT("VulkanRTX", "  → dsPool_ @ {}", static_cast<void*>(dsPool_.get()), RESET);

    VkDescriptorSetLayout layout = dsLayout_.get();
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };

    VkDescriptorSet set = VK_NULL_HANDLE;
    LOG_INFO_CAT("VulkanRTX", "vkAllocateDescriptorSets() → CALL", OCEAN_TEAL, RESET);
    result = vkAllocateDescriptorSets(device_, &allocInfo, &set);
    LOG_INFO_CAT("VulkanRTX", "vkAllocateDescriptorSets() → result = {}", result, RESET);

    if (result != VK_SUCCESS || set == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanRTX", "vkAllocateDescriptorSets FAILED: {} | set = {}", 
                      result, static_cast<void*>(set), CRIMSON_MAGENTA, RESET);
        throw VulkanRTXException("Failed to allocate RTX descriptor set");
    }

    ds_ = Dispose::VulkanHandle<VkDescriptorSet>(
        device_, set,
        [pool = dsPool_.get()](VkDevice d, VkDescriptorSet s, const VkAllocationCallbacks*) {
            if (s != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
                ::vkFreeDescriptorSets(d, pool, 1, &s);
            }
        },
        "RTX Desc Set"
    );

    LOG_INFO_CAT("VulkanRTX", "{}createDescriptorPoolAndSet() – SUCCESS @ {}{}", 
                 EMERALD_GREEN, static_cast<void*>(set), RESET);
}

/* -----------------------------------------------------------------
   createRayTracingPipeline – REMOVED (pipeline set externally)
   ----------------------------------------------------------------- */
// REMOVED: Pipeline is now set via setRayTracingPipeline() from VulkanRenderer
// No RAII ownership of pipeline/layout

/* -----------------------------------------------------------------
   updateDescriptorSetForTLAS
   ----------------------------------------------------------------- */
void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
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
   initializeRTX / updateRTX – RAII
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

    createShaderBindingTable(physicalDevice);
    updateDescriptorSetForTLAS(tlas_.get());
}

void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice) {
    LOG_INFO_CAT("SBT", "createShaderBindingTable() – START", OCEAN_TEAL, RESET);

    if (rtPipeline_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("SBT", "Ray tracing pipeline not set!", CRIMSON_MAGENTA, RESET);
        return;
    }

    const auto& props = context_->rtProperties;
    if (props.shaderGroupHandleSize == 0 || props.shaderGroupBaseAlignment == 0) {
        LOG_ERROR_CAT("SBT", "Invalid ray tracing properties!", CRIMSON_MAGENTA, RESET);
        return;
    }

    uint32_t handleSize = props.shaderGroupHandleSize;
    uint32_t handleAlign = props.shaderGroupBaseAlignment;
    uint32_t handleSizeAligned = (handleSize + handleAlign - 1) & ~(handleAlign - 1);

    uint32_t groupCount = 3;
    VkDeviceSize sbtSize = static_cast<VkDeviceSize>(groupCount) * handleSizeAligned;

    createBuffer(physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 sbtBuffer_, sbtMemory_);

    if (sbtBuffer_.get() == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("SBT", "Failed to create SBT buffer!", CRIMSON_MAGENTA, RESET);
        return;
    }

    VkBufferDeviceAddressInfo addrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = sbtBuffer_.get()
    };
    sbtBufferAddress_ = vkGetBufferDeviceAddress(device_, &addrInfo);
    if (sbtBufferAddress_ == 0) {
        LOG_ERROR_CAT("SBT", "Failed to get SBT device address!", CRIMSON_MAGENTA, RESET);
        return;
    }

    std::vector<uint8_t> shaderHandleStorage(groupCount * handleSize);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device_, rtPipeline_,  // RAW
                                                  0, groupCount, shaderHandleStorage.size(), shaderHandleStorage.data()),
             "Failed to get shader group handles");

    void* data = nullptr;
    VK_CHECK(vkMapMemory(device_, sbtMemory_.get(), 0, sbtSize, 0, &data), "Failed to map SBT memory");
    auto* pData = reinterpret_cast<uint8_t*>(data);

    for (uint32_t g = 0; g < groupCount; ++g) {
        memcpy(pData, shaderHandleStorage.data() + g * handleSize, handleSize);
        pData += handleSizeAligned;
    }

    vkUnmapMemory(device_, sbtMemory_.get());

    sbt_.raygen   = { sbtBufferAddress_ + 0 * handleSizeAligned, handleSizeAligned, handleSizeAligned };
    sbt_.miss     = { sbtBufferAddress_ + 1 * handleSizeAligned, handleSizeAligned, handleSizeAligned };
    sbt_.hit      = { sbtBufferAddress_ + 2 * handleSizeAligned, handleSizeAligned, handleSizeAligned };
    sbt_.callable = { 0, 0, 0 };

    LOG_INFO_CAT("SBT", "SBT READY: raygen=0x{:x}, miss=0x{:x}, hit=0x{:x}",
                 sbt_.raygen.deviceAddress, sbt_.miss.deviceAddress, sbt_.hit.deviceAddress, RESET);
}

void VulkanRTX::setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) {
    LOG_INFO_CAT("VulkanRTX", "setRayTracingPipeline() – pipeline @ {}", 
                 static_cast<void*>(pipeline), RESET);

    if (pipeline == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanRTX", "Invalid pipeline passed!", CRIMSON_MAGENTA, RESET);
        return;
    }

    rtPipeline_ = pipeline;        // RAW
    rtPipelineLayout_ = layout;    // RAW

    LOG_INFO_CAT("VulkanRTX", "Ray tracing pipeline assigned (non-owning)", EMERALD_GREEN, RESET);
}

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
   createBuffer – RAII
   ----------------------------------------------------------------- */
void VulkanRTX::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size,
                             VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                             VulkanHandle<VkBuffer>& buffer,
                             VulkanHandle<VkDeviceMemory>& memory)
{
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buf;
    VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &buf), "Create buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, buf, &memReqs);

    uint32_t memType = VulkanInitializer::findMemoryType(physicalDevice, memReqs.memoryTypeBits, properties);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory mem;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &mem), "Allocate buffer memory");
    VK_CHECK(vkBindBufferMemory(device_, buf, mem, 0), "Bind buffer memory");

    buffer = makeHandle(device_, buf, "Buffer");
    memory = makeHandle(device_, mem, "Memory");
}

/* -----------------------------------------------------------------
   recordRayTracingCommands – FULLY FIXED (no .get() on raw handles)
   ----------------------------------------------------------------- */
void VulkanRTX::recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent,
                                         VkImage outputImage, VkImageView outputImageView) {
    LOG_DEBUG_CAT("RTX", "recordRayTracingCommands() – START", OCEAN_TEAL, RESET);

    if (!rtPipeline_ || rtPipeline_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RTX", "Ray tracing pipeline not created!", CRIMSON_MAGENTA, RESET);
        return;
    }
    if (ds_.get() == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RTX", "Descriptor set not allocated!", CRIMSON_MAGENTA, RESET);
        return;
    }
    if (sbtBufferAddress_ == 0) {
        LOG_ERROR_CAT("RTX", "SBT has no device address!", CRIMSON_MAGENTA, RESET);
        return;
    }

    // --- TRANSITION OUTPUT IMAGE ---
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = outputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // --- BIND PIPELINE ---
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);

    // --- BIND DESCRIPTOR SET (FIXED) ---
    VkDescriptorSet ds = ds_.get();  // ← TEMPORARY LVALUE
    vkCmdBindDescriptorSets(cmdBuffer,
                            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipelineLayout_,
                            0, 1, &ds,  // ← VALID ADDRESS
                            0, nullptr);

    // --- SBT REGIONS ---
    const uint32_t handleSizeAligned = 32;
    const VkStridedDeviceAddressRegionKHR raygenRegion{
        .deviceAddress = sbt_.raygen.deviceAddress,
        .stride = handleSizeAligned,
        .size = handleSizeAligned
    };
    const VkStridedDeviceAddressRegionKHR missRegion{
        .deviceAddress = sbt_.miss.deviceAddress,
        .stride = handleSizeAligned,
        .size = handleSizeAligned
    };
    const VkStridedDeviceAddressRegionKHR hitRegion{
        .deviceAddress = sbt_.hit.deviceAddress,
        .stride = handleSizeAligned,
        .size = handleSizeAligned
    };
    const VkStridedDeviceAddressRegionKHR callableRegion{0, 0, 0};

    // --- DISPATCH RAYS ---
    vkCmdTraceRaysKHR(cmdBuffer,
                      &raygenRegion, &missRegion, &hitRegion, &callableRegion,
                      extent.width, extent.height, 1);

    // --- TRANSITION BACK ---
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    LOG_DEBUG_CAT("RTX", "recordRayTracingCommands() – COMPLETE", EMERALD_GREEN, RESET);
}
} // namespace VulkanRTX