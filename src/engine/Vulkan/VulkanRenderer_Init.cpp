// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/core.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <array>
#include <iomanip>
#include <span>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <chrono>
#include <bit>
#include "stb/stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"
#include <fstream>

namespace VulkanRTX {

#define VK_CHECK(x) do { VkResult r = (x); if (r != VK_SUCCESS) { LOG_ERROR_CAT("Renderer", #x " failed: {}", static_cast<int>(r)); throw std::runtime_error(#x " failed"); } } while(0)

// -----------------------------------------------------------------------------
// PRIVATE: CREATE SHADER MODULE
// -----------------------------------------------------------------------------
VkShaderModule VulkanRenderer::createShaderModule(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filepath);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(context_.device, &createInfo, nullptr, &shaderModule));
    return shaderModule;
}

// -----------------------------------------------------------------------------
// CONSTRUCTOR — FIXED INITIALIZATION ORDER
// -----------------------------------------------------------------------------
VulkanRenderer::VulkanRenderer(int width, int height, void* window,
                               const std::vector<std::string>& instanceExtensions)
    : width_(width), height_(height), window_(window), currentFrame_(0), frameCount_(0),
      framesThisSecond_(0), lastFPSTime_(std::chrono::steady_clock::now()),
      framesSinceLastLog_(0), lastLogTime_(std::chrono::steady_clock::now()),
      indexCount_(0), rtPipeline_(VK_NULL_HANDLE), rtPipelineLayout_(VK_NULL_HANDLE),
      denoiseImage_(VK_NULL_HANDLE), denoiseImageMemory_(VK_NULL_HANDLE),
      denoiseImageView_(VK_NULL_HANDLE), denoiseSampler_(VK_NULL_HANDLE),
      envMapImage_(VK_NULL_HANDLE), envMapImageMemory_(VK_NULL_HANDLE),
      envMapImageView_(VK_NULL_HANDLE), envMapSampler_(VK_NULL_HANDLE),
      blasHandle_(VK_NULL_HANDLE), blasBuffer_(VK_NULL_HANDLE), blasBufferMemory_(VK_NULL_HANDLE),
      tlasHandle_(VK_NULL_HANDLE), tlasBuffer_(VK_NULL_HANDLE), tlasBufferMemory_(VK_NULL_HANDLE),
      instanceBuffer_(VK_NULL_HANDLE), instanceBufferMemory_(VK_NULL_HANDLE),
      sbtBuffer_(VK_NULL_HANDLE), sbtMemory_(VK_NULL_HANDLE),
      rtOutputImage_(context_.device, VK_NULL_HANDLE, vkDestroyImage),
      rtOutputImageMemory_(context_.device, VK_NULL_HANDLE, vkFreeMemory),
      rtOutputImageView_(context_.device, VK_NULL_HANDLE, vkDestroyImageView),
      context_(), rtx_(), swapchainManager_(), pipelineManager_(), bufferManager_(),
      frames_(), framebuffers_(), commandBuffers_(), descriptorSets_(), computeDescriptorSets_(),
      descriptorPool_(VK_NULL_HANDLE), materialBuffers_(), materialBufferMemory_(),
      dimensionBuffers_(), dimensionBufferMemory_(), camera_(),
      descriptorsUpdated_(false), recreateSwapchain(false)
{
    LOG_INFO_CAT("Renderer", "=== VulkanRenderer Constructor Start ===");
    frames_.resize(MAX_FRAMES_IN_FLIGHT);

    // 1. INIT VULKAN CORE
    VulkanInitializer::initInstance(instanceExtensions, context_);
    VulkanInitializer::initSurface(context_, window_, nullptr);
    context_.physicalDevice = VulkanInitializer::findPhysicalDevice(context_.instance, context_.surface, true);
    VulkanInitializer::initDevice(context_);
    context_.resourceManager.setDevice(context_.device, context_.physicalDevice);

    // 2. LOAD RT EXTENSION FUNCTIONS
    #define LOAD_FUNC(name) \
        context_.name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(context_.device, #name)); \
        if (!context_.name) throw std::runtime_error("Failed to load " #name);
    LOAD_FUNC(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_FUNC(vkCmdBuildAccelerationStructuresKHR);
    LOAD_FUNC(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_FUNC(vkGetBufferDeviceAddressKHR);
    LOAD_FUNC(vkCreateAccelerationStructureKHR);
    LOAD_FUNC(vkDestroyAccelerationStructureKHR);
    LOAD_FUNC(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_FUNC(vkCmdTraceRaysKHR);
    #undef LOAD_FUNC

    context_.vkDeferredOperationJoinKHR = reinterpret_cast<PFN_vkDeferredOperationJoinKHR>(
        vkGetDeviceProcAddr(context_.device, "vkDeferredOperationJoinKHR"));
    context_.vkGetDeferredOperationResultKHR = reinterpret_cast<PFN_vkGetDeferredOperationResultKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetDeferredOperationResultKHR"));
    context_.vkDestroyDeferredOperationKHR = reinterpret_cast<PFN_vkDestroyDeferredOperationKHR>(
        vkGetDeviceProcAddr(context_.device, "vkDestroyDeferredOperationKHR"));

    // 3. COMMAND POOL
    VkCommandPoolCreateInfo cmdPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context_.graphicsQueueFamilyIndex
    };
    VK_CHECK(vkCreateCommandPool(context_.device, &cmdPoolInfo, nullptr, &context_.commandPool));
    context_.resourceManager.addCommandPool(context_.commandPool);

    // 4. SWAPCHAIN & FRAME SYNC
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

    // 5. PIPELINE MANAGER
    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_, width_, height_);

    // === CREATE ALL PIPELINES FIRST ===
    pipelineManager_->createRayTracingPipeline();
    pipelineManager_->createComputePipeline();
    pipelineManager_->createGraphicsPipeline(width_, height_);

    // === GET LAYOUTS AND PIPELINE AFTER CREATION ===
    context_.rayTracingDescriptorSetLayout = pipelineManager_->createRayTracingDescriptorSetLayout();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();
    rtPipeline_ = pipelineManager_->getRayTracingPipeline();

    // === CREATE SBT AFTER PIPELINE ===
    pipelineManager_->createShaderBindingTable();
    sbt_ = pipelineManager_->getShaderBindingTable();

    // 6. BUFFER MANAGER
    bufferManager_ = std::make_unique<VulkanBufferManager>(
        context_,
        std::span<const glm::vec3>(getVertices()),
        std::span<const uint32_t>(getIndices())
    );
    indexCount_ = static_cast<uint32_t>(getIndices().size());

    // 7. GEOMETRY & ACCELERATION STRUCTURES
    buildAccelerationStructures();

    // 8. RT OUTPUT IMAGE
    createRTOutputImage();

    // 9. FRAMEBUFFERS & COMMAND BUFFERS
    createFramebuffers();
    createCommandBuffers();

    // 10. ENVIRONMENT MAP
    createEnvironmentMap();

    // 11. PER-FRAME BUFFERS
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, sizeof(MaterialData) * 128, sizeof(DimensionData));

    // 12. DESCRIPTOR SYSTEM
    createDescriptorPool();
    createDescriptorSets();
    createComputeDescriptorSets();

    // 13. UPDATE DESCRIPTORS
    updateRTDescriptors();

    LOG_INFO_CAT("Renderer", "=== VulkanRenderer Initialized Successfully ===");
}

// -----------------------------------------------------------------------------
// BUILD ACCELERATION STRUCTURES
// -----------------------------------------------------------------------------
void VulkanRenderer::buildAccelerationStructures() {
    const auto& verts = getVertices();
    const auto& idxs = getIndices();
    uint32_t primitiveCount = static_cast<uint32_t>(idxs.size() / 3);
    if (primitiveCount == 0) {
        throw std::runtime_error("No primitives in model; cannot build BLAS");
    }
    LOG_INFO_CAT("Renderer", "Building BLAS with {} primitives", primitiveCount);

    for (size_t i = 0; i < idxs.size(); i += 3) {
        uint32_t a = idxs[i], b = idxs[i+1], c = idxs[i+2];
        if (a >= verts.size() || b >= verts.size() || c >= verts.size() || a == b || b == c || c == a) {
            LOG_ERROR_CAT("Renderer", "Invalid/degenerate triangle {}: indices [{}, {}, {}]", i/3, a, b, c);
            throw std::runtime_error("Degenerate geometry");
        }
        glm::vec3 va = verts[a], vb = verts[b], vc = verts[c];
        float area = 0.5f * glm::length(glm::cross(vb - va, vc - va));
        if (area < 1e-5f) LOG_WARNING_CAT("Renderer", "Near-zero area triangle {}", i/3);
    }
    LOG_INFO_CAT("Renderer", "Geometry integrity: {} valid tris", primitiveCount);

    auto submitAndWait = [&](VkCommandBuffer cmd, const std::string& step) {
        LOG_INFO_CAT("Renderer", "Submitting {} to graphics queue {}", step, context_.graphicsQueueFamilyIndex);
        VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VkFence fence;
        VK_CHECK(vkCreateFence(context_.device, &fenceInfo, nullptr, &fence));
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        VK_CHECK(vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, fence));
        VK_CHECK(vkWaitForFences(context_.device, 1, &fence, VK_TRUE, UINT64_MAX));
        vkDestroyFence(context_.device, fence, nullptr);
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
    };

    auto allocateCmd = [&]() -> VkCommandBuffer {
        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = context_.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(context_.device, &allocInfo, &cmd));
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
        return cmd;
    };

    VkDeviceSize vertexSize = verts.size() * sizeof(glm::vec3);
    VkDeviceSize indexSize = idxs.size() * sizeof(uint32_t);

    VkBuffer stagingVertexBuffer, stagingIndexBuffer;
    VkDeviceMemory stagingVertexMemory, stagingIndexMemory;
    bufferManager_->createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 stagingVertexBuffer, stagingVertexMemory);
    bufferManager_->createBuffer(indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 stagingIndexBuffer, stagingIndexMemory);

    { void* data; VK_CHECK(vkMapMemory(context_.device, stagingVertexMemory, 0, vertexSize, 0, &data)); std::memcpy(data, verts.data(), vertexSize); vkUnmapMemory(context_.device, stagingVertexMemory); }
    { void* data; VK_CHECK(vkMapMemory(context_.device, stagingIndexMemory, 0, indexSize, 0, &data)); std::memcpy(data, idxs.data(), indexSize); vkUnmapMemory(context_.device, stagingIndexMemory); }

    VkBuffer vertexBuffer, indexBuffer;
    VkDeviceMemory vertexMemory, indexMemory;
    bufferManager_->createBuffer(vertexSize,
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 vertexBuffer, vertexMemory);
    bufferManager_->createBuffer(indexSize,
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 indexBuffer, indexMemory);

    VkCommandBuffer cmdCopy = allocateCmd();
    { VkBufferCopy regionV{}; regionV.size = vertexSize; vkCmdCopyBuffer(cmdCopy, stagingVertexBuffer, vertexBuffer, 1, &regionV); }
    { VkBufferCopy regionI{}; regionI.size = indexSize; vkCmdCopyBuffer(cmdCopy, stagingIndexBuffer, indexBuffer, 1, &regionI); }
    VK_CHECK(vkEndCommandBuffer(cmdCopy));
    submitAndWait(cmdCopy, "geometry copy");

    vkDestroyBuffer(context_.device, stagingVertexBuffer, nullptr); vkFreeMemory(context_.device, stagingVertexMemory, nullptr);
    vkDestroyBuffer(context_.device, stagingIndexBuffer, nullptr); vkFreeMemory(context_.device, stagingIndexMemory, nullptr);

    VkBufferDeviceAddressInfo vAddrInfo{}; vAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO; vAddrInfo.buffer = vertexBuffer;
    VkDeviceAddress vertexDeviceAddress = context_.vkGetBufferDeviceAddressKHR(context_.device, &vAddrInfo);

    VkBufferDeviceAddressInfo iAddrInfo{}; iAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO; iAddrInfo.buffer = indexBuffer;
    VkDeviceAddress indexDeviceAddress = context_.vkGetBufferDeviceAddressKHR(context_.device, &iAddrInfo);

    LOG_INFO_CAT("Renderer", "Vertex addr=0x{:x}, Index addr=0x{:x}", vertexDeviceAddress, indexDeviceAddress);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexDeviceAddress;
    triangles.vertexStride = sizeof(glm::vec3);
    triangles.maxVertex = static_cast<uint32_t>(verts.size()) - 1;
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexDeviceAddress;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles = triangles;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    uint32_t maxPrimCount = primitiveCount;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    context_.vkGetAccelerationStructureBuildSizesKHR(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrimCount, &sizeInfo);

    LOG_INFO_CAT("Renderer", "BLAS sizes: storage={}B, scratch={}B", sizeInfo.accelerationStructureSize, sizeInfo.buildScratchSize);

    if (bufferManager_->getScratchBufferCount() == 0) {
        bufferManager_->reserveScratchPool(sizeInfo.buildScratchSize, 1);
        LOG_INFO_CAT("Renderer", "RESERVED scratch buffer: {} B", sizeInfo.buildScratchSize);
    }

    VkDeviceAddress scratchAddr = bufferManager_->getScratchBufferAddress(0);
    if (scratchAddr == 0) throw std::runtime_error("Scratch buffer address is 0");

    bufferManager_->createBuffer(sizeInfo.accelerationStructureSize,
                                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 blasBuffer_, blasBufferMemory_);

    VkAccelerationStructureCreateInfoKHR blasCreateInfo{};
    blasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    blasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasCreateInfo.buffer = blasBuffer_;
    blasCreateInfo.size = sizeInfo.accelerationStructureSize;
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &blasCreateInfo, nullptr, &blasHandle_));

    buildInfo.dstAccelerationStructure = blasHandle_;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkCommandBuffer cmdBLAS = allocateCmd();
    VkAccelerationStructureBuildRangeInfoKHR range{}; range.primitiveCount = primitiveCount;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    context_.vkCmdBuildAccelerationStructuresKHR(cmdBLAS, 1, &buildInfo, &pRange);
    VK_CHECK(vkEndCommandBuffer(cmdBLAS));
    submitAndWait(cmdBLAS, "BLAS build");

    vkDestroyBuffer(context_.device, vertexBuffer, nullptr); vkFreeMemory(context_.device, vertexMemory, nullptr);
    vkDestroyBuffer(context_.device, indexBuffer, nullptr); vkFreeMemory(context_.device, indexMemory, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR blasAddrInfo{};
    blasAddrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    blasAddrInfo.accelerationStructure = blasHandle_;
    VkDeviceAddress blasAddr = context_.vkGetAccelerationStructureDeviceAddressKHR(context_.device, &blasAddrInfo);

    VkAccelerationStructureInstanceKHR instance = {};
    instance.instanceCustomIndex = 0; instance.mask = 0xFF;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = blasAddr;
    instance.transform.matrix[0][0] = 1.0f; instance.transform.matrix[1][1] = 1.0f; instance.transform.matrix[2][2] = 1.0f;

    VkDeviceSize instSize = sizeof(instance);
    VkBuffer staging; VkDeviceMemory stagingMem;
    bufferManager_->createBuffer(instSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
    void* map; VK_CHECK(vkMapMemory(context_.device, stagingMem, 0, instSize, 0, &map)); memcpy(map, &instance, instSize); vkUnmapMemory(context_.device, stagingMem);

    bufferManager_->createBuffer(instSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, instanceBuffer_, instanceBufferMemory_);

    VkCommandBuffer cmdInstCopy = allocateCmd();
    VkBufferCopy copyRegion{}; copyRegion.size = instSize;
    vkCmdCopyBuffer(cmdInstCopy, staging, instanceBuffer_, 1, &copyRegion);
    VK_CHECK(vkEndCommandBuffer(cmdInstCopy));
    submitAndWait(cmdInstCopy, "instance copy");
    vkDestroyBuffer(context_.device, staging, nullptr); vkFreeMemory(context_.device, stagingMem, nullptr);

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    barrier.buffer = instanceBuffer_;
    barrier.size = instSize;
    VkCommandBuffer cmdBarrier = allocateCmd();
    vkCmdPipelineBarrier(cmdBarrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0, nullptr, 1, &barrier, 0, nullptr);
    VK_CHECK(vkEndCommandBuffer(cmdBarrier));
    submitAndWait(cmdBarrier, "instance barrier");

    VkBufferDeviceAddressInfo instAddrInfo{}; instAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO; instAddrInfo.buffer = instanceBuffer_;
    VkDeviceAddress instAddr = context_.vkGetBufferDeviceAddressKHR(context_.device, &instAddrInfo);

    VkAccelerationStructureGeometryInstancesDataKHR instData{};
    instData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instData.data.deviceAddress = instAddr;

    VkAccelerationStructureGeometryKHR tlasGeom{};
    tlasGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeom.geometry.instances = instData;
    tlasGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    buildInfo = VkAccelerationStructureBuildGeometryInfoKHR{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &tlasGeom;

    uint32_t tlasMaxPrim = 1;
    VkAccelerationStructureBuildSizesInfoKHR tlasSize{};
    tlasSize.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    context_.vkGetAccelerationStructureBuildSizesKHR(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &tlasMaxPrim, &tlasSize);

    bufferManager_->createBuffer(tlasSize.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer_, tlasBufferMemory_);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo{};
    tlasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasCreateInfo.buffer = tlasBuffer_;
    tlasCreateInfo.size = tlasSize.accelerationStructureSize;
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &tlasCreateInfo, nullptr, &tlasHandle_));

    buildInfo.dstAccelerationStructure = tlasHandle_;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkCommandBuffer cmdTLAS = allocateCmd();
    VkAccelerationStructureBuildRangeInfoKHR tlasRange{}; tlasRange.primitiveCount = 1;
    const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRange;
    context_.vkCmdBuildAccelerationStructuresKHR(cmdTLAS, 1, &buildInfo, &pTlasRange);
    VK_CHECK(vkEndCommandBuffer(cmdTLAS));
    submitAndWait(cmdTLAS, "TLAS build");

    LOG_INFO_CAT("Renderer", "Acceleration structures built successfully.");
}

// -----------------------------------------------------------------------------
// CREATE RT OUTPUT IMAGE — FIXED RAII
// -----------------------------------------------------------------------------
void VulkanRenderer::createRTOutputImage()
{
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.extent        = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 };
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage img = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(context_.device, &imageInfo, nullptr, &img));

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(context_.device, img, &memReqs);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = VulkanInitializer::findMemoryType(
        context_.physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory mem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(context_.device, &allocInfo, nullptr, &mem));
    VK_CHECK(vkBindImageMemory(context_.device, img, mem, 0));

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image            = img;
    viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkImageView view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(context_.device, &viewInfo, nullptr, &view));

    VulkanInitializer::transitionImageLayout(
        context_,
        img,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL
    );

    rtOutputImage_.reset(img);
    rtOutputImageMemory_.reset(mem);
    rtOutputImageView_.reset(view);
}

// -----------------------------------------------------------------------------
// CREATE COMPUTE DESCRIPTOR SETS
// -----------------------------------------------------------------------------
void VulkanRenderer::createComputeDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, pipelineManager_->getComputeDescriptorSetLayout());
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();

    computeDescriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(context_.device, &allocInfo, computeDescriptorSets_.data()));

    LOG_INFO_CAT("Renderer", "Compute descriptor sets allocated.");
}

// -----------------------------------------------------------------------------
// UPDATE RT DESCRIPTORS
// -----------------------------------------------------------------------------
void VulkanRenderer::updateRTDescriptors() {
    if (tlasHandle_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "TLAS handle is null! Cannot update descriptors.");
        throw std::runtime_error("TLAS not built");
    }

    if (descriptorsUpdated_) {
        return;
    }

    const uint32_t frameCount = MAX_FRAMES_IN_FLIGHT;
    const uint32_t totalWrites = frameCount * 10;

    std::vector<VkWriteDescriptorSetAccelerationStructureKHR> asWrites(frameCount);
    std::vector<VkDescriptorImageInfo> outputImageInfos(frameCount);
    std::vector<VkDescriptorBufferInfo> uniformInfos(frameCount);
    std::vector<VkDescriptorBufferInfo> materialInfos(frameCount);
    std::vector<VkDescriptorBufferInfo> dimensionInfos(frameCount);
    std::vector<VkDescriptorImageInfo> envMapInfos(frameCount);

    std::vector<VkWriteDescriptorSet> writes(totalWrites);

    for (uint32_t i = 0; i < frameCount; ++i) {
        const uint32_t base = i * 10;

        asWrites[i] = {};
        asWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asWrites[i].accelerationStructureCount = 1;
        asWrites[i].pAccelerationStructures = &tlasHandle_;

        writes[base + 0] = {};
        writes[base + 0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[base + 0].pNext = &asWrites[i];
        writes[base + 0].dstSet = descriptorSets_[i];
        writes[base + 0].dstBinding = 0;
        writes[base + 0].descriptorCount = 1;
        writes[base + 0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

        outputImageInfos[i] = {};
        outputImageInfos[i].imageView = rtOutputImageView_.get();
        outputImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[base + 1] = {};
        writes[base + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[base + 1].dstSet = descriptorSets_[i];
        writes[base + 1].dstBinding = 1;
        writes[base + 1].descriptorCount = 1;
        writes[base + 1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[base + 1].pImageInfo = &outputImageInfos[i];

        uniformInfos[i] = {};
        uniformInfos[i].buffer = context_.uniformBuffers[i];
        uniformInfos[i].range = VK_WHOLE_SIZE;

        writes[base + 2] = {};
        writes[base + 2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[base + 2].dstSet = descriptorSets_[i];
        writes[base + 2].dstBinding = 2;
        writes[base + 2].descriptorCount = 1;
        writes[base + 2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[base + 2].pBufferInfo = &uniformInfos[i];

        materialInfos[i] = {};
        materialInfos[i].buffer = materialBuffers_[i];
        materialInfos[i].range = VK_WHOLE_SIZE;

        writes[base + 3] = {};
        writes[base + 3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[base + 3].dstSet = descriptorSets_[i];
        writes[base + 3].dstBinding = 3;
        writes[base + 3].descriptorCount = 1;
        writes[base + 3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[base + 3].pBufferInfo = &materialInfos[i];

        dimensionInfos[i] = {};
        dimensionInfos[i].buffer = dimensionBuffers_[i];
        dimensionInfos[i].range = VK_WHOLE_SIZE;

        writes[base + 4] = {};
        writes[base + 4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[base + 4].dstSet = descriptorSets_[i];
        writes[base + 4].dstBinding = 4;
        writes[base + 4].descriptorCount = 1;
        writes[base + 4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[base + 4].pBufferInfo = &dimensionInfos[i];

        envMapInfos[i] = {};
        envMapInfos[i].sampler = envMapSampler_;
        envMapInfos[i].imageView = envMapImageView_;
        envMapInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        for (int b = 5; b <= 7; ++b) {
            writes[base + b] = {};
            writes[base + b].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[base + b].dstSet = descriptorSets_[i];
            writes[base + b].dstBinding = static_cast<uint32_t>(b);
            writes[base + b].descriptorCount = 1;
            writes[base + b].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[base + b].pImageInfo = &envMapInfos[i];
        }

        writes[base + 8] = {};
        writes[base + 8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[base + 8].dstSet = descriptorSets_[i];
        writes[base + 8].dstBinding = 8;
        writes[base + 8].descriptorCount = 1;
        writes[base + 8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[base + 8].pImageInfo = &outputImageInfos[i];

        writes[base + 9] = {};
        writes[base + 9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[base + 9].dstSet = descriptorSets_[i];
        writes[base + 9].dstBinding = 9;
        writes[base + 9].descriptorCount = 1;
        writes[base + 9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[base + 9].pImageInfo = &outputImageInfos[i];
    }

    vkUpdateDescriptorSets(context_.device, totalWrites, writes.data(), 0, nullptr);

    descriptorsUpdated_ = true;
    LOG_INFO_CAT("Renderer", "RT descriptors updated ({} writes).", totalWrites);
}

// -----------------------------------------------------------------------------
// CLEANUP — SAFE DESTRUCTION
// -----------------------------------------------------------------------------
VulkanRenderer::~VulkanRenderer() { cleanup(); }

void VulkanRenderer::cleanup() noexcept {
    LOG_INFO_CAT("Renderer", "=== Starting VulkanRenderer Cleanup ===");

    if (sbtBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(context_.device, sbtBuffer_, nullptr);
    if (sbtMemory_ != VK_NULL_HANDLE) vkFreeMemory(context_.device, sbtMemory_, nullptr);

    if (tlasHandle_ != VK_NULL_HANDLE && context_.vkDestroyAccelerationStructureKHR)
        context_.vkDestroyAccelerationStructureKHR(context_.device, tlasHandle_, nullptr);
    if (tlasBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(context_.device, tlasBuffer_, nullptr);
    if (tlasBufferMemory_ != VK_NULL_HANDLE) vkFreeMemory(context_.device, tlasBufferMemory_, nullptr);

    if (blasHandle_ != VK_NULL_HANDLE && context_.vkDestroyAccelerationStructureKHR)
        context_.vkDestroyAccelerationStructureKHR(context_.device, blasHandle_, nullptr);
    if (blasBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(context_.device, blasBuffer_, nullptr);
    if (blasBufferMemory_ != VK_NULL_HANDLE) vkFreeMemory(context_.device, blasBufferMemory_, nullptr);

    if (instanceBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(context_.device, instanceBuffer_, nullptr);
    if (instanceBufferMemory_ != VK_NULL_HANDLE) vkFreeMemory(context_.device, instanceBufferMemory_, nullptr);

    if (rtPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(context_.device, rtPipeline_, nullptr);

    rtOutputImage_.reset();
    rtOutputImageMemory_.reset();
    rtOutputImageView_.reset();

    for (auto& b : materialBuffers_) if (b != VK_NULL_HANDLE) vkDestroyBuffer(context_.device, b, nullptr);
    for (auto& m : materialBufferMemory_) if (m != VK_NULL_HANDLE) vkFreeMemory(context_.device, m, nullptr);
    materialBuffers_.clear(); materialBufferMemory_.clear();

    for (auto& b : dimensionBuffers_) if (b != VK_NULL_HANDLE) vkDestroyBuffer(context_.device, b, nullptr);
    for (auto& m : dimensionBufferMemory_) if (m != VK_NULL_HANDLE) vkFreeMemory(context_.device, m, nullptr);
    dimensionBuffers_.clear(); dimensionBufferMemory_.clear();

    if (denoiseSampler_ != VK_NULL_HANDLE) vkDestroySampler(context_.device, denoiseSampler_, nullptr);
    if (denoiseImageView_ != VK_NULL_HANDLE) vkDestroyImageView(context_.device, denoiseImageView_, nullptr);
    if (denoiseImageMemory_ != VK_NULL_HANDLE) vkFreeMemory(context_.device, denoiseImageMemory_, nullptr);
    if (denoiseImage_ != VK_NULL_HANDLE) vkDestroyImage(context_.device, denoiseImage_, nullptr);

    if (envMapSampler_ != VK_NULL_HANDLE) vkDestroySampler(context_.device, envMapSampler_, nullptr);
    if (envMapImageView_ != VK_NULL_HANDLE) vkDestroyImageView(context_.device, envMapImageView_, nullptr);
    if (envMapImageMemory_ != VK_NULL_HANDLE) vkFreeMemory(context_.device, envMapImageMemory_, nullptr);
    if (envMapImage_ != VK_NULL_HANDLE) vkDestroyImage(context_.device, envMapImage_, nullptr);

    LOG_INFO_CAT("Renderer", "=== VulkanRenderer Cleanup Complete ===");
}

// -----------------------------------------------------------------------------
// CREATE ENVIRONMENT MAP
// -----------------------------------------------------------------------------
void VulkanRenderer::createEnvironmentMap() {
    LOG_INFO_CAT("Renderer", "Loading environment map from assets/textures/envmap.hdr...");
    int texWidth, texHeight, texChannels;
    stbi_set_flip_vertically_on_load(false);
    float* pixels = stbi_loadf("assets/textures/envmap.hdr", &texWidth, &texHeight, &texChannels, 4);
    if (!pixels) throw std::runtime_error("Failed to load envmap");

    VkFormat imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkExtent3D imageExtent = { static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };
    VkDeviceSize imageSize = static_cast<uint64_t>(texWidth) * texHeight * 4 * sizeof(float);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = imageFormat;
    imageInfo.extent = imageExtent;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(vkCreateImage(context_.device, &imageInfo, nullptr, &envMapImage_));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(context_.device, envMapImage_, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = VulkanInitializer::findMemoryType(context_.physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(context_.device, &allocInfo, nullptr, &envMapImageMemory_));
    VK_CHECK(vkBindImageMemory(context_.device, envMapImage_, envMapImageMemory_, 0));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = envMapImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imageFormat;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    VK_CHECK(vkCreateImageView(context_.device, &viewInfo, nullptr, &envMapImageView_));

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxAnisotropy = 1.0f;
    VK_CHECK(vkCreateSampler(context_.device, &samplerInfo, nullptr, &envMapSampler_));

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    bufferManager_->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 stagingBuffer, stagingMemory);
    void* data;
    VK_CHECK(vkMapMemory(context_.device, stagingMemory, 0, imageSize, 0, &data));
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(context_.device, stagingMemory);

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = context_.commandPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(context_.device, &cmdAlloc, &cmd));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = envMapImage_;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = imageExtent;
    vkCmdCopyBufferToImage(cmd, stagingBuffer, envMapImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    VK_CHECK(vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    vkQueueWaitIdle(context_.graphicsQueue);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);

    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingMemory, nullptr);
    stbi_image_free(pixels);

    LOG_INFO_CAT("Renderer", "Environment map loaded: {}x{}", texWidth, texHeight);
}

// -----------------------------------------------------------------------------
// INITIALIZE ALL BUFFER DATA
// -----------------------------------------------------------------------------
void VulkanRenderer::initializeAllBufferData(uint32_t maxFrames, VkDeviceSize materialSize, VkDeviceSize dimensionSize) {
    materialBuffers_.resize(maxFrames, VK_NULL_HANDLE);
    materialBufferMemory_.resize(maxFrames);
    dimensionBuffers_.resize(maxFrames, VK_NULL_HANDLE);
    dimensionBufferMemory_.resize(maxFrames);

    context_.uniformBuffers.resize(maxFrames, VK_NULL_HANDLE);
    context_.uniformBufferMemories.resize(maxFrames);

    for (uint32_t i = 0; i < maxFrames; ++i) {
        initializeBufferData(i, materialSize, dimensionSize);
    }
    LOG_INFO_CAT("Renderer", "All per-frame storage buffers initialized.");
}

void VulkanRenderer::initializeBufferData(uint32_t frameIndex, VkDeviceSize materialSize, VkDeviceSize dimensionSize) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;

    bufferManager_->createBuffer(materialSize,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 materialBuffers_[frameIndex], materialBufferMemory_[frameIndex]);

    bufferManager_->createBuffer(dimensionSize,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 dimensionBuffers_[frameIndex], dimensionBufferMemory_[frameIndex]);

    VkBuffer uboBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uboMemory = VK_NULL_HANDLE;
    bufferManager_->createBuffer(sizeof(UniformBufferObject),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 uboBuffer, uboMemory);

    context_.uniformBuffers[frameIndex] = uboBuffer;
    context_.uniformBufferMemories[frameIndex] = uboMemory;
}

// -----------------------------------------------------------------------------
// CREATE FRAMEBUFFERS
// -----------------------------------------------------------------------------
void VulkanRenderer::createFramebuffers() {
    framebuffers_.resize(context_.swapchainImageViews.size());
    for (size_t i = 0; i < context_.swapchainImageViews.size(); ++i) {
        VkImageView attachments[] = { context_.swapchainImageViews[i] };
        VkFramebufferCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = pipelineManager_->getRenderPass(),
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = context_.swapchainExtent.width,
            .height = context_.swapchainExtent.height,
            .layers = 1
        };
        VK_CHECK(vkCreateFramebuffer(context_.device, &info, nullptr, &framebuffers_[i]));
    }
    LOG_INFO_CAT("Renderer", "Framebuffers created successfully.");
}

// -----------------------------------------------------------------------------
// CREATE COMMAND BUFFERS
// -----------------------------------------------------------------------------
void VulkanRenderer::createCommandBuffers() {
    commandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers_.size())
    };
    VK_CHECK(vkAllocateCommandBuffers(context_.device, &info, commandBuffers_.data()));
    LOG_INFO_CAT("Renderer", "Command buffers allocated.");
}

// -----------------------------------------------------------------------------
// CREATE DESCRIPTOR POOL
// -----------------------------------------------------------------------------
void VulkanRenderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 5> poolSizes{{
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 6 * MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 54 * MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6 * MAX_FRAMES_IN_FLIGHT }
    }};

    VkDescriptorPoolCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
        .maxSets = 2 * MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };
    VK_CHECK(vkCreateDescriptorPool(context_.device, &info, nullptr, &descriptorPool_));
    context_.resourceManager.addDescriptorPool(descriptorPool_);
    LOG_INFO_CAT("Renderer", "Descriptor pool created.");
}

// -----------------------------------------------------------------------------
// CREATE DESCRIPTOR SETS
// -----------------------------------------------------------------------------
void VulkanRenderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, context_.rayTracingDescriptorSetLayout);
    VkDescriptorSetAllocateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool_,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts.data()
    };
    descriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(context_.device, &info, descriptorSets_.data()));
    LOG_INFO_CAT("Renderer", "Descriptor sets allocated.");
}

// -----------------------------------------------------------------------------
// RENDER FRAME — SAFE SUBMIT
// -----------------------------------------------------------------------------
void VulkanRenderer::renderFrame(const Camera& camera) {
    auto frameStart = std::chrono::steady_clock::now();

    vkWaitForFences(context_.device, 1, &frames_[currentFrame_].fence, VK_TRUE, UINT64_MAX);
    vkResetFences(context_.device, 1, &frames_[currentFrame_].fence);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_.device, context_.swapchain, UINT64_MAX,
                                            frames_[currentFrame_].imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain = true;
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    updateUniformBuffer(currentFrame_, camera);

    VkCommandBuffer cmd = commandBuffers_[currentFrame_];
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image = rtOutputImage_.get();
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipelineLayout_, 0, 1, &descriptorSets_[currentFrame_], 0, nullptr);

    const auto& sbt = sbt_;
    if (sbt.raygen.deviceAddress == 0 || sbt.miss.deviceAddress == 0 || sbt.hit.deviceAddress == 0) {
        LOG_ERROR_CAT("Renderer", "Invalid SBT addresses! Raygen=0x{:x}, Miss=0x{:x}, Hit=0x{:x}",
                      sbt.raygen.deviceAddress, sbt.miss.deviceAddress, sbt.hit.deviceAddress);
        throw std::runtime_error("Invalid SBT");
    }

    VkStridedDeviceAddressRegionKHR raygenRegion{};
    raygenRegion.deviceAddress = sbt.raygen.deviceAddress;
    raygenRegion.stride       = sbt.raygen.stride;
    raygenRegion.size         = sbt.raygen.size;

    VkStridedDeviceAddressRegionKHR missRegion{};
    missRegion.deviceAddress = sbt.miss.deviceAddress;
    missRegion.stride       = sbt.miss.stride;
    missRegion.size         = sbt.miss.size;

    VkStridedDeviceAddressRegionKHR hitRegion{};
    hitRegion.deviceAddress = sbt.hit.deviceAddress;
    hitRegion.stride       = sbt.hit.stride;
    hitRegion.size         = sbt.hit.size;

    VkStridedDeviceAddressRegionKHR callableRegion{};

    context_.vkCmdTraceRaysKHR(cmd,
        &raygenRegion,
        &missRegion,
        &hitRegion,
        &callableRegion,
        static_cast<uint32_t>(width_),
        static_cast<uint32_t>(height_),
        1);

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkImageMemoryBarrier swapBarrier{};
    swapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapBarrier.srcAccessMask = 0;
    swapBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapBarrier.image = context_.swapchainImages[imageIndex];
    swapBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapBarrier);

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.extent = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 };
    vkCmdCopyImage(cmd, rtOutputImage_.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   context_.swapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    swapBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    swapBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapBarrier);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frames_[currentFrame_].imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frames_[currentFrame_].renderFinishedSemaphore;

    VK_CHECK(vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, frames_[currentFrame_].fence));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frames_[currentFrame_].renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &context_.swapchain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(context_.graphicsQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain = true;
    } else {
        VK_CHECK(result);
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    ++frameCount_;
    ++framesThisSecond_;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFPSTime_).count();
    if (elapsed >= 1000) {
        LOG_INFO_CAT("Renderer", "FPS: {:.2f}", static_cast<float>(framesThisSecond_) * 1000.0f / elapsed);
        framesThisSecond_ = 0;
        lastFPSTime_ = now;
    }

    pipelineManager_->logFrameTimeIfSlow(frameStart);
}

void VulkanRenderer::updateUniformBuffer(uint32_t frameIndex, const Camera& camera) {
    UniformBufferObject ubo{};
    ubo.viewInverse = glm::inverse(camera.getViewMatrix());
    ubo.projInverse = glm::inverse(camera.getProjectionMatrix());
    ubo.camPos = glm::vec4(camera.getPosition(), 1.0f);
    ubo.time = 0.0f;
    ubo.frame = frameCount_;

    void* data;
    VK_CHECK(vkMapMemory(context_.device, context_.uniformBufferMemories[frameIndex], 0, sizeof(ubo), 0, &data));
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_.device, context_.uniformBufferMemories[frameIndex]);
}

// -----------------------------------------------------------------------------
// HANDLE RESIZE
// -----------------------------------------------------------------------------
void VulkanRenderer::handleResize(int width, int height) {
    if (width == 0 || height == 0) return;
    LOG_INFO_CAT("Renderer", "Handling window resize: {}x{} to {}x{}", width_, height_, width, height);
    vkDeviceWaitIdle(context_.device);
    width_ = width; height_ = height;
    swapchainManager_->handleResize(width, height);
    createFramebuffers();
    recreateSwapchain = false;
    LOG_INFO_CAT("Renderer", "Resize complete.");
}

// -----------------------------------------------------------------------------
// GET VERTICES / INDICES
// -----------------------------------------------------------------------------
std::vector<glm::vec3> VulkanRenderer::getVertices() const {
    static std::vector<glm::vec3> cached;
    if (!cached.empty()) return cached;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "assets/models/scene.obj")) {
        LOG_ERROR_CAT("Renderer", "Failed to load OBJ: {}", err.empty() ? warn : err);
        throw std::runtime_error("Failed to load OBJ");
    }

    std::vector<glm::vec3> verts;
    verts.reserve(attrib.vertices.size() / 3);
    for (size_t i = 0; i < attrib.vertices.size(); i += 3) {
        verts.emplace_back(attrib.vertices[i], attrib.vertices[i + 1], attrib.vertices[i + 2]);
    }
    cached = std::move(verts);
    LOG_INFO_CAT("Renderer", "Loaded {} unique vertices.", cached.size());
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
        LOG_ERROR_CAT("Renderer", "Failed to load OBJ: {}", err.empty() ? warn : err);
        throw std::runtime_error("Failed to load OBJ");
    }

    std::vector<uint32_t> idxs;
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            idxs.push_back(static_cast<uint32_t>(index.vertex_index));
        }
    }
    cached = std::move(idxs);
    LOG_INFO_CAT("Renderer", "Loaded {} indices.", cached.size());
    return cached;
}

} // namespace VulkanRTX