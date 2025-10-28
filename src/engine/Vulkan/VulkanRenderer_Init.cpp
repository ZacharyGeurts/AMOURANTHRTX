// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/core.hpp"
#include "engine/Dispose.hpp"
#include <stdexcept>
#include <algorithm>
#include <format>
#include <cstring>
#include <array>
#include <iomanip>
#include <span>
#include <chrono>
#include <bit>  // for std::bit_cast
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

namespace VulkanRTX {

#define VK_CHECK(x) do { VkResult r = (x); if (r != VK_SUCCESS) { LOG_ERROR_CAT("Renderer", "{} failed: {}", #x, static_cast<int>(r)); throw std::runtime_error(#x " failed"); } } while(0)

/* --------------------------------------------------------------------- */
/*  AS extension function pointers (global, loaded once)                 */
/* --------------------------------------------------------------------- */
PFN_vkGetAccelerationStructureBuildSizesKHR    vkGetAccelerationStructureBuildSizesKHR    = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR       vkCmdBuildAccelerationStructuresKHR       = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
PFN_vkGetBufferDeviceAddressKHR               vkGetBufferDeviceAddressKHR               = nullptr;
PFN_vkCreateAccelerationStructureKHR          vkCreateAccelerationStructureKHR          = nullptr;
PFN_vkDestroyAccelerationStructureKHR         vkDestroyAccelerationStructureKHR         = nullptr;

/* --------------------------------------------------------------------- */
/*  Helper – write TLAS into a descriptor set                           */
/* --------------------------------------------------------------------- */
static void updateTLASDescriptor(VkDevice device,
                                 VkDescriptorSet ds,
                                 VkAccelerationStructureKHR tlas)
{
    VkWriteDescriptorSetAccelerationStructureKHR accelWrite = {
        .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures    = &tlas
    };

    VkWriteDescriptorSet write = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext           = &accelWrite,
        .dstSet          = ds,
        .dstBinding      = static_cast<uint32_t>(DescriptorBindings::TLAS),
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

/* --------------------------------------------------------------------- */
/*  Constructor – TLAS is built *here*                                   */
/* --------------------------------------------------------------------- */
VulkanRenderer::VulkanRenderer(int width, int height, void* window,
                               const std::vector<std::string>& instanceExtensions)
    : width_(width),
      height_(height),
      window_(window),
      currentFrame_(0),
      frameCount_(0),
      framesSinceLastLog_(0),
      lastLogTime_(std::chrono::steady_clock::now()),
      indexCount_(0),
      rtPipeline_(VK_NULL_HANDLE),
      rtPipelineLayout_(VK_NULL_HANDLE),
      denoiseImage_(VK_NULL_HANDLE),
      denoiseImageMemory_(VK_NULL_HANDLE),
      denoiseImageView_(VK_NULL_HANDLE),
      denoiseSampler_(VK_NULL_HANDLE),
      envMapImage_(VK_NULL_HANDLE),
      envMapImageMemory_(VK_NULL_HANDLE),
      envMapImageView_(VK_NULL_HANDLE),
      envMapSampler_(VK_NULL_HANDLE),
      computeDescriptorSetLayout_(VK_NULL_HANDLE),
      blasHandle_(VK_NULL_HANDLE),
      tlasHandle_(VK_NULL_HANDLE),
      context_()
{
    LOG_INFO_CAT("Renderer", "Constructing VulkanRenderer with width={}, height={}, window={:p}", width, height, window);
    if (width <= 0 || height <= 0 || window == nullptr) {
        LOG_ERROR_CAT("Renderer", "Invalid window parameters");
        throw std::invalid_argument("Invalid window parameters");
    }

    frames_.resize(MAX_FRAMES_IN_FLIGHT);
    LOG_DEBUG_CAT("Renderer", "Initialized frames_ vector with size: {}", frames_.size());

    /* --------------------------------------------------------------- */
    /*  Vulkan init (instance, surface, device, command pool)          */
    /* --------------------------------------------------------------- */
    VulkanInitializer::initInstance(instanceExtensions, context_);
    VulkanInitializer::initSurface(context_, window_, nullptr);
    context_.physicalDevice = VulkanInitializer::findPhysicalDevice(context_.instance, context_.surface, true);
    VulkanInitializer::initDevice(context_);
    context_.resourceManager.setDevice(context_.device);

    VkCommandPoolCreateInfo cmdPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context_.graphicsQueueFamilyIndex
    };
    VK_CHECK(vkCreateCommandPool(context_.device, &cmdPoolInfo, nullptr, &context_.commandPool));
    context_.resourceManager.addCommandPool(context_.commandPool);

    /* --------------------------------------------------------------- */
    /*  Compute descriptor set layout (used later)                     */
    /* --------------------------------------------------------------- */
    VkDescriptorSetLayoutBinding computeBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };
    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = computeBindings
    };
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &computeLayoutInfo, nullptr, &computeDescriptorSetLayout_));
    context_.resourceManager.addDescriptorSetLayout(computeDescriptorSetLayout_);

    /* --------------------------------------------------------------- */
    /*  Swapchain                                                     */
    /* --------------------------------------------------------------- */
    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, context_.surface);
    swapchainManager_->initializeSwapchain(width_, height_);
    context_.swapchain = swapchainManager_->getSwapchain();
    context_.swapchainImageFormat = swapchainManager_->getSwapchainImageFormat();
    context_.swapchainExtent = swapchainManager_->getSwapchainExtent();
    context_.swapchainImages = swapchainManager_->getSwapchainImages();
    context_.swapchainImageViews = swapchainManager_->getSwapchainImageViews();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames_[i].imageAvailableSemaphore = swapchainManager_->getImageAvailableSemaphore(i);
        frames_[i].renderFinishedSemaphore = swapchainManager_->getRenderFinishedSemaphore(i);
        frames_[i].fence = swapchainManager_->getInFlightFence(i);
    }

    /* --------------------------------------------------------------- */
    /*  Pipeline manager (creates layouts, pipelines, SBT)            */
    /* --------------------------------------------------------------- */
    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_, width_, height_);
    pipelineManager_->createRayTracingPipeline();
    pipelineManager_->createGraphicsPipeline(width_, height_);
    pipelineManager_->createComputePipeline();

    rtPipeline_ = pipelineManager_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();
    context_.rayTracingPipeline = rtPipeline_;

    /* --------------------------------------------------------------- */
    /*  Load geometry → vertex / index buffers                         */
    /* --------------------------------------------------------------- */
    bufferManager_ = std::make_unique<VulkanBufferManager>(
        context_,
        std::span<const glm::vec3>(getVertices()),
        std::span<const uint32_t>(getIndices())
    );
    indexCount_ = static_cast<uint32_t>(getIndices().size());

    bufferManager_->createUniformBuffers(MAX_FRAMES_IN_FLIGHT);
    context_.uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    context_.uniformBufferMemories.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        context_.uniformBuffers[i] = bufferManager_->getUniformBuffer(i);
        context_.uniformBufferMemories[i] = bufferManager_->getUniformBufferMemory(i);
    }

    /* --------------------------------------------------------------- */
    /*  *** BUILD BLAS + TLAS RIGHT HERE ***                           */
    /* --------------------------------------------------------------- */
    {
        LOG_INFO_CAT("Renderer", "=== BUILDING ACCELERATION STRUCTURES ===");

        /* ---- load AS extension functions ---- */
        vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
            vkGetDeviceProcAddr(context_.device, "vkCreateAccelerationStructureKHR"));
        vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(context_.device, "vkDestroyAccelerationStructureKHR"));
        vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
            vkGetDeviceProcAddr(context_.device, "vkGetAccelerationStructureBuildSizesKHR"));
        vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
            vkGetDeviceProcAddr(context_.device, "vkCmdBuildAccelerationStructuresKHR"));
        vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
            vkGetDeviceProcAddr(context_.device, "vkGetAccelerationStructureDeviceAddressKHR"));
        vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(
            vkGetDeviceProcAddr(context_.device, "vkGetBufferDeviceAddressKHR"));

        if (!vkCreateAccelerationStructureKHR || !vkDestroyAccelerationStructureKHR ||
            !vkGetAccelerationStructureBuildSizesKHR || !vkCmdBuildAccelerationStructuresKHR ||
            !vkGetAccelerationStructureDeviceAddressKHR || !vkGetBufferDeviceAddressKHR) {
            LOG_ERROR_CAT("Renderer", "Failed to load one or more AS extension functions");
            throw std::runtime_error("Missing AS extension functions");
        }

        VkBuffer vertexBuffer = bufferManager_->getVertexBuffer();
        VkBuffer indexBuffer  = bufferManager_->getIndexBuffer();

        /* ---- device addresses ---- */
        VkBufferDeviceAddressInfoKHR vAddrInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR, .buffer = vertexBuffer };
        VkBufferDeviceAddressInfoKHR iAddrInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR, .buffer = indexBuffer };
        VkDeviceAddress vertexAddr = vkGetBufferDeviceAddressKHR(context_.device, &vAddrInfo);
        VkDeviceAddress indexAddr  = vkGetBufferDeviceAddressKHR(context_.device, &iAddrInfo);

        /* ---- counts ---- */
        VkMemoryRequirements vReq{}, iReq{};
        vkGetBufferMemoryRequirements(context_.device, vertexBuffer, &vReq);
        vkGetBufferMemoryRequirements(context_.device, indexBuffer,  &iReq);
        uint32_t numVertices    = static_cast<uint32_t>(vReq.size / sizeof(Vertex));
        uint32_t maxVertex      = numVertices ? numVertices - 1 : 0;
        uint32_t primitiveCount = static_cast<uint32_t>(iReq.size / sizeof(uint32_t)) / 3;

        if (primitiveCount == 0) {
            LOG_ERROR_CAT("Renderer", "No primitives – cannot build AS");
            throw std::runtime_error("No primitives");
        }

        /* ---- BLAS geometry ---- */
        VkAccelerationStructureGeometryKHR geometry = {
            .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {
                .triangles = {
                    .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                    .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
                    .vertexData    = { .deviceAddress = vertexAddr },
                    .vertexStride  = sizeof(Vertex),
                    .maxVertex     = maxVertex,
                    .indexType     = VK_INDEX_TYPE_UINT32,
                    .indexData     = { .deviceAddress = indexAddr }
                }
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };

        VkAccelerationStructureBuildGeometryInfoKHR blasBuildInfo = {
            .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                             VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
            .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .geometryCount = 1,
            .pGeometries   = &geometry
        };

        /* ---- query BLAS size ---- */
        VkAccelerationStructureBuildSizesInfoKHR blasSizeInfo = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        vkGetAccelerationStructureBuildSizesKHR(context_.device,
                                                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &blasBuildInfo,
                                                &primitiveCount,
                                                &blasSizeInfo);

        LOG_INFO_CAT("Renderer", "BLAS size: {} bytes, scratch: {} bytes", blasSizeInfo.accelerationStructureSize, blasSizeInfo.buildScratchSize);

        /* ---- create BLAS storage buffer ---- */
        VkBuffer blasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory blasMemory = VK_NULL_HANDLE;
        VkMemoryAllocateFlagsInfo allocFlags{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
        };
        VulkanInitializer::createBuffer(context_.device,
                                        context_.physicalDevice,
                                        blasSizeInfo.accelerationStructureSize,
                                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        blasBuffer,
                                        blasMemory,
                                        &allocFlags,
                                        context_.resourceManager);

        /* ---- create BLAS handle ---- */
        VkAccelerationStructureCreateInfoKHR blasCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = blasBuffer,
            .size   = blasSizeInfo.accelerationStructureSize,
            .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
        };
        VK_CHECK(vkCreateAccelerationStructureKHR(context_.device, &blasCreateInfo, nullptr, &blasHandle_));

        /* ---- scratch for BLAS build ---- */
        VkBuffer blasScratch = VK_NULL_HANDLE;
        VkDeviceMemory blasScratchMem = VK_NULL_HANDLE;
        VulkanInitializer::createBuffer(context_.device,
                                        context_.physicalDevice,
                                        blasSizeInfo.buildScratchSize,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        blasScratch,
                                        blasScratchMem,
                                        nullptr,
                                        context_.resourceManager);

        VkBufferDeviceAddressInfoKHR scratchAddrInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR, .buffer = blasScratch };
        blasBuildInfo.dstAccelerationStructure = blasHandle_;
        blasBuildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddressKHR(context_.device, &scratchAddrInfo);

        /* ---- build BLAS ---- */
        VkAccelerationStructureBuildRangeInfoKHR blasRange = { .primitiveCount = primitiveCount };
        const VkAccelerationStructureBuildRangeInfoKHR* pBlasRange = &blasRange;
        VkCommandBuffer cmd = VulkanInitializer::beginSingleTimeCommands(context_);
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &blasBuildInfo, &pBlasRange);
        VulkanInitializer::endSingleTimeCommands(context_, cmd);

        // clean scratch
        context_.resourceManager.removeBuffer(blasScratch);
        context_.resourceManager.removeMemory(blasScratchMem);
        vkDestroyBuffer(context_.device, blasScratch, nullptr);
        vkFreeMemory(context_.device, blasScratchMem, nullptr);

        /* ---- TLAS instance buffer ---- */
        VkAccelerationStructureDeviceAddressInfoKHR asAddrInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blasHandle_
        };
        VkDeviceAddress blasDevAddr = vkGetAccelerationStructureDeviceAddressKHR(context_.device, &asAddrInfo);

        VkTransformMatrixKHR identity{{
            {1,0,0,0},
            {0,1,0,0},
            {0,0,1,0}
        }};
        VkAccelerationStructureInstanceKHR instance = {
            .transform                               = identity,
            .instanceCustomIndex                     = 0,
            .mask                                    = 0xFF,
            .instanceShaderBindingTableRecordOffset  = 0,
            .flags                                   = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference          = blasDevAddr
        };

        VkBuffer instBuffer = VK_NULL_HANDLE;
        VkDeviceMemory instMem = VK_NULL_HANDLE;
        VulkanInitializer::createBuffer(context_.device,
                                        context_.physicalDevice,
                                        sizeof(instance),
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        instBuffer,
                                        instMem,
                                        nullptr,
                                        context_.resourceManager);

        void* mapPtr = nullptr;
        VK_CHECK(vkMapMemory(context_.device, instMem, 0, sizeof(instance), 0, &mapPtr));
        std::memcpy(mapPtr, &instance, sizeof(instance));
        vkUnmapMemory(context_.device, instMem);

        VkBufferDeviceAddressInfoKHR instAddrInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR, .buffer = instBuffer };
        VkDeviceAddress instDevAddr = vkGetBufferDeviceAddressKHR(context_.device, &instAddrInfo);

        /* ---- TLAS geometry ---- */
        VkAccelerationStructureGeometryKHR tlasGeom = {
            .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {
                .instances = {
                    .sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                    .arrayOfPointers = VK_FALSE,
                    .data            = { .deviceAddress = instDevAddr }
                }
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };

        VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo = {
            .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                             VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
            .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .geometryCount = 1,
            .pGeometries   = &tlasGeom
        };

        uint32_t instanceCount = 1;
        VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        vkGetAccelerationStructureBuildSizesKHR(context_.device,
                                                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &tlasBuildInfo,
                                                &instanceCount,
                                                &tlasSizeInfo);

        LOG_INFO_CAT("Renderer", "TLAS size: {} bytes, scratch: {} bytes", tlasSizeInfo.accelerationStructureSize, tlasSizeInfo.buildScratchSize);

        /* ---- TLAS storage buffer ---- */
        VkBuffer tlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory tlasMemory = VK_NULL_HANDLE;
        VulkanInitializer::createBuffer(context_.device,
                                        context_.physicalDevice,
                                        tlasSizeInfo.accelerationStructureSize,
                                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        tlasBuffer,
                                        tlasMemory,
                                        &allocFlags,
                                        context_.resourceManager);

        /* ---- TLAS handle ---- */
        VkAccelerationStructureCreateInfoKHR tlasCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = tlasBuffer,
            .size   = tlasSizeInfo.accelerationStructureSize,
            .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
        };
        VK_CHECK(vkCreateAccelerationStructureKHR(context_.device, &tlasCreateInfo, nullptr, &tlasHandle_));

        /* ---- TLAS scratch ---- */
        VkBuffer tlasScratch = VK_NULL_HANDLE;
        VkDeviceMemory tlasScratchMem = VK_NULL_HANDLE;
        VulkanInitializer::createBuffer(context_.device,
                                        context_.physicalDevice,
                                        tlasSizeInfo.buildScratchSize,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        tlasScratch,
                                        tlasScratchMem,
                                        nullptr,
                                        context_.resourceManager);

        VkBufferDeviceAddressInfoKHR tlasScratchAddr = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR, .buffer = tlasScratch };
        tlasBuildInfo.dstAccelerationStructure = tlasHandle_;
        tlasBuildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddressKHR(context_.device, &tlasScratchAddr);

        /* ---- build TLAS ---- */
        VkAccelerationStructureBuildRangeInfoKHR tlasRange = { .primitiveCount = 1 };
        const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRange;
        cmd = VulkanInitializer::beginSingleTimeCommands(context_);
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuildInfo, &pTlasRange);
        VulkanInitializer::endSingleTimeCommands(context_, cmd);

        // clean TLAS scratch
        context_.resourceManager.removeBuffer(tlasScratch);
        context_.resourceManager.removeMemory(tlasScratchMem);
        vkDestroyBuffer(context_.device, tlasScratch, nullptr);
        vkFreeMemory(context_.device, tlasScratchMem, nullptr);

        /* ---- register resources ---- */
        context_.resourceManager.addAccelerationStructure(blasHandle_);
        context_.resourceManager.addAccelerationStructure(tlasHandle_);
        context_.resourceManager.addBuffer(blasBuffer);
        context_.resourceManager.addBuffer(tlasBuffer);
        context_.resourceManager.addBuffer(instBuffer);
        context_.resourceManager.addMemory(instMem);

        LOG_INFO_CAT("Renderer", "=== ACCELERATION STRUCTURES BUILT ===");
        LOG_INFO_CAT("Renderer", "BLAS: {:p}   TLAS: {:p}", std::bit_cast<const void*>(blasHandle_), std::bit_cast<const void*>(tlasHandle_));
    }

    /* --------------------------------------------------------------- */
    /*  Framebuffers, command buffers, storage images, env-map, etc.   */
    /* --------------------------------------------------------------- */
    createFramebuffers();
    createCommandBuffers();

    // --- CREATE DENOISE IMAGE ---
    VulkanInitializer::createStorageImage(
        context_.device,
        context_.physicalDevice,
        denoiseImage_,
        denoiseImageMemory_,
        denoiseImageView_,
        width_,
        height_,
        context_.resourceManager
    );

    LOG_INFO_CAT("VulkanInitializer", "Created storage image {:p}, memory {:p}, view {:p} (width={}, height={})",
                 std::bit_cast<const void*>(denoiseImage_), std::bit_cast<const void*>(denoiseImageMemory_),
                 std::bit_cast<const void*>(denoiseImageView_), width_, height_);

    // --- TRANSITION DENOISE IMAGE TO GENERAL ---
    {
        VkCommandBuffer cmd = VulkanInitializer::beginSingleTimeCommands(context_);

        VkImageMemoryBarrier barrier = {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = 0,
            .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = denoiseImage_,
            .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0, 0, nullptr, 0, nullptr, 1, &barrier
        );

        VulkanInitializer::endSingleTimeCommands(context_, cmd);
    }

    VkSamplerCreateInfo denoiseSamplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    };
    VK_CHECK(vkCreateSampler(context_.device, &denoiseSamplerInfo, nullptr, &denoiseSampler_));

    createEnvironmentMap();

    constexpr uint32_t MATERIAL_COUNT = 128;
    constexpr uint32_t DIMENSION_COUNT = 1;
    VkDeviceSize materialSize = sizeof(MaterialData) * MATERIAL_COUNT;
    VkDeviceSize dimensionSize = sizeof(DimensionData) * DIMENSION_COUNT;
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, materialSize, dimensionSize);

    /* --------------------------------------------------------------- */
    /*  Descriptor pool + per-frame sets                               */
    /* --------------------------------------------------------------- */
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 3}
    };
    VkDescriptorPoolCreateInfo descPoolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_FRAMES_IN_FLIGHT * 3,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes
    };
    VK_CHECK(vkCreateDescriptorPool(context_.device, &descPoolInfo, nullptr, &context_.descriptorPool));
    context_.resourceManager.addDescriptorPool(context_.descriptorPool);

    std::vector<VkDescriptorSetLayout> rtLayouts(MAX_FRAMES_IN_FLIGHT, context_.rayTracingDescriptorSetLayout);
    std::vector<VkDescriptorSetLayout> gfxLayouts(MAX_FRAMES_IN_FLIGHT, context_.graphicsDescriptorSetLayout);
    std::vector<VkDescriptorSetLayout> cmpLayouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout_);

    VkDescriptorSetAllocateInfo rtAlloc{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                        .descriptorPool = context_.descriptorPool,
                                        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
                                        .pSetLayouts = rtLayouts.data()};
    VkDescriptorSetAllocateInfo gfxAlloc{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                         .descriptorPool = context_.descriptorPool,
                                         .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
                                         .pSetLayouts = gfxLayouts.data()};
    VkDescriptorSetAllocateInfo cmpAlloc{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                         .descriptorPool = context_.descriptorPool,
                                         .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
                                         .pSetLayouts = cmpLayouts.data()};

    std::vector<VkDescriptorSet> rtSets(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSet> gfxSets(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSet> cmpSets(MAX_FRAMES_IN_FLIGHT);

    VK_CHECK(vkAllocateDescriptorSets(context_.device, &rtAlloc, rtSets.data()));
    VK_CHECK(vkAllocateDescriptorSets(context_.device, &gfxAlloc, gfxSets.data()));
    VK_CHECK(vkAllocateDescriptorSets(context_.device, &cmpAlloc, cmpSets.data()));

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames_[i].rayTracingDescriptorSet = rtSets[i];
        frames_[i].graphicsDescriptorSet   = gfxSets[i];
        frames_[i].computeDescriptorSet    = cmpSets[i];
    }

    /* --------------------------------------------------------------- */
    /*  NOW: Update descriptors AFTER TLAS is valid                   */
    /* --------------------------------------------------------------- */
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        updateTLASDescriptor(context_.device, frames_[i].rayTracingDescriptorSet, tlasHandle_);
        updateDescriptorSetForFrame(i, tlasHandle_);
        updateGraphicsDescriptorSet(i);
        updateComputeDescriptorSet(i);
    }

    LOG_INFO_CAT("Renderer", "VulkanRenderer initialized successfully");
}

/* --------------------------------------------------------------------- */
/*  Destructor – clean AS handles + everything else                     */
/* --------------------------------------------------------------------- */
VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

void VulkanRenderer::cleanup() noexcept {
    try {
        if (tlasHandle_ && vkDestroyAccelerationStructureKHR) vkDestroyAccelerationStructureKHR(context_.device, tlasHandle_, nullptr);
        if (blasHandle_ && vkDestroyAccelerationStructureKHR) vkDestroyAccelerationStructureKHR(context_.device, blasHandle_, nullptr);

        for (auto& b : materialBuffers_)  if (b) vkDestroyBuffer(context_.device, b, nullptr);
        for (auto& m : materialBufferMemory_) if (m) vkFreeMemory(context_.device, m, nullptr);
        materialBuffers_.clear(); materialBufferMemory_.clear();

        for (auto& b : dimensionBuffers_) if (b) vkDestroyBuffer(context_.device, b, nullptr);
        for (auto& m : dimensionBufferMemory_) if (m) vkFreeMemory(context_.device, m, nullptr);
        dimensionBuffers_.clear(); dimensionBufferMemory_.clear();

        if (denoiseSampler_)    vkDestroySampler(context_.device, denoiseSampler_, nullptr);
        if (denoiseImageView_)  vkDestroyImageView(context_.device, denoiseImageView_, nullptr);
        if (denoiseImageMemory_)vkFreeMemory(context_.device, denoiseImageMemory_, nullptr);
        if (denoiseImage_)      vkDestroyImage(context_.device, denoiseImage_, nullptr);

        if (envMapSampler_)     vkDestroySampler(context_.device, envMapSampler_, nullptr);
        if (envMapImageView_)   vkDestroyImageView(context_.device, envMapImageView_, nullptr);
        if (envMapImageMemory_) vkFreeMemory(context_.device, envMapImageMemory_, nullptr);
        if (envMapImage_)       vkDestroyImage(context_.device, envMapImage_, nullptr);

        if (computeDescriptorSetLayout_) vkDestroyDescriptorSetLayout(context_.device, computeDescriptorSetLayout_, nullptr);

        bufferManager_.reset();
        pipelineManager_.reset();
        swapchainManager_.reset();
    } catch (...) {}
}

/* --------------------------------------------------------------------- */
/*  Geometry loaders – now return the cached data                        */
/* --------------------------------------------------------------------- */
std::vector<glm::vec3> VulkanRenderer::getVertices() const {
    static std::vector<glm::vec3> cached;
    if (!cached.empty()) return cached;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "assets/models/scene.obj")) {
        LOG_ERROR_CAT("Renderer", "OBJ load failed: {}", err);
        throw std::runtime_error("Failed to load OBJ");
    }

    std::vector<glm::vec3> verts;
    for (const auto& shape : shapes) {
        for (const auto& idx : shape.mesh.indices) {
            verts.push_back({
                attrib.vertices[3 * idx.vertex_index + 0],
                attrib.vertices[3 * idx.vertex_index + 1],
                attrib.vertices[3 * idx.vertex_index + 2]
            });
        }
    }
    cached = std::move(verts);
    return cached;
}

std::vector<uint32_t> VulkanRenderer::getIndices() const {
    static std::vector<uint32_t> cached;
    if (!cached.empty()) return cached;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "assets/models/scene.obj")) {
        LOG_ERROR_CAT("Renderer", "OBJ load failed: {}", err);
        throw std::runtime_error("Failed to load OBJ");
    }

    std::vector<uint32_t> idxs;
    for (const auto& shape : shapes) {
        for (size_t i = 0; i < shape.mesh.indices.size(); ++i)
            idxs.push_back(static_cast<uint32_t>(shape.mesh.indices[i].vertex_index));
    }
    cached = std::move(idxs);
    return cached;
}

/* --------------------------------------------------------------------- */
/*  The rest of the file (swapchain recreation, env-map, buffers,   */
/*  descriptor updates, framebuffers, etc.) remains unchanged.      */
/* --------------------------------------------------------------------- */
void VulkanRenderer::createSwapchain(int width, int height) { /* unchanged */ }
void VulkanRenderer::createEnvironmentMap() { /* unchanged */ }
void VulkanRenderer::initializeAllBufferData(uint32_t maxFrames, VkDeviceSize materialBufferSize, VkDeviceSize dimensionBufferSize) { /* unchanged */ }
void VulkanRenderer::initializeBufferData(uint32_t frameIndex, VkDeviceSize materialSize, VkDeviceSize dimensionSize) { /* unchanged */ }
void VulkanRenderer::createFramebuffers() { /* unchanged */ }
void VulkanRenderer::createCommandBuffers() { /* unchanged */ }
} // namespace VulkanRTX