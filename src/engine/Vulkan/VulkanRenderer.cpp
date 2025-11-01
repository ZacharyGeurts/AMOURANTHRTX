// src/engine/Vulkan/VulkanRenderer.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0

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
#include <thread>
#include <atomic>
#include "stb/stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"
#include <fstream>

namespace VulkanRTX {

#define VK_CHECK(x) do { \
    VkResult r = (x); \
    if (r != VK_SUCCESS) { \
        LOG_ERROR_CAT("Renderer", #x " failed: {}", static_cast<int>(r)); \
        throw std::runtime_error(#x " failed"); \
    } \
} while(0)

// -----------------------------------------------------------------------------
// PRIVATE: CREATE SHADER MODULE
// -----------------------------------------------------------------------------
VkShaderModule VulkanRenderer::createShaderModule(const std::string& filepath) {
    LOG_INFO_CAT("Renderer", "Loading shader: {}", filepath);
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("Renderer", "Failed to open shader file: {}", filepath);
        throw std::runtime_error("Failed to open shader file: " + filepath);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(context_.device, &createInfo, nullptr, &shaderModule));
    LOG_INFO_CAT("Renderer", "Shader module created: {} bytes", fileSize);
    return shaderModule;
}

// -----------------------------------------------------------------------------
// CONSTRUCTOR
// -----------------------------------------------------------------------------
VulkanRenderer::VulkanRenderer(int width, int height, void* window,
                               const std::vector<std::string>& instanceExtensions)
    : width_(width), height_(height), window_(window),
      currentFrame_(0), frameCount_(0), framesThisSecond_(0),
      lastFPSTime_(std::chrono::steady_clock::now()),
      indexCount_(0),
      denoiseImage_(VK_NULL_HANDLE), denoiseImageMemory_(VK_NULL_HANDLE),
      denoiseImageView_(VK_NULL_HANDLE), denoiseSampler_(VK_NULL_HANDLE),
      envMapImage_(VK_NULL_HANDLE), envMapImageMemory_(VK_NULL_HANDLE),
      envMapImageView_(VK_NULL_HANDLE), envMapSampler_(VK_NULL_HANDLE),
      blasHandle_(VK_NULL_HANDLE), blasBuffer_(VK_NULL_HANDLE), blasBufferMemory_(VK_NULL_HANDLE),
      tlasHandle_(VK_NULL_HANDLE), tlasBuffer_(VK_NULL_HANDLE), tlasBufferMemory_(VK_NULL_HANDLE),
      instanceBuffer_(VK_NULL_HANDLE), instanceBufferMemory_(VK_NULL_HANDLE),
      tlasDeviceAddress_(0),
      context_(), swapchainManager_(), pipelineManager_(), bufferManager_(),
      frames_(), framebuffers_(), commandBuffers_(),
      descriptorSets_(), computeDescriptorSets_(),
      descriptorPool_(VK_NULL_HANDLE),
      materialBuffers_(), materialBufferMemory_(),
      dimensionBuffers_(), dimensionBufferMemory_(),
      camera_(std::make_unique<PerspectiveCamera>()),
      descriptorsUpdated_(false),
      vkCmdTraceRaysKHR_(nullptr),
      rtPipeline_(VK_NULL_HANDLE), rtPipelineLayout_(VK_NULL_HANDLE),
      sbtBuffer_(VK_NULL_HANDLE), sbtMemory_(VK_NULL_HANDLE), sbt_{},
      currentRTIndex_(0), currentAccumIndex_(0), resetAccumulation_(true),
      swapchainRecreating_(false),
      computeDescriptorSetLayout_(VK_NULL_HANDLE),
      queryReady_{}, lastRTTimeMs_{}
{
    LOG_INFO_CAT("Renderer", "=== VulkanRenderer Constructor Start ===");
    frames_.resize(MAX_FRAMES_IN_FLIGHT);

    VulkanInitializer::initInstance(instanceExtensions, context_);
    VulkanInitializer::initSurface(context_, window_, nullptr);
    context_.physicalDevice = VulkanInitializer::findPhysicalDevice(context_.instance, context_.surface, true);
    VulkanInitializer::initDevice(context_);
    context_.resourceManager.setDevice(context_.device, context_.physicalDevice);

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

    VkCommandPoolCreateInfo cmdPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context_.graphicsQueueFamilyIndex
    };
    VK_CHECK(vkCreateCommandPool(context_.device, &cmdPoolInfo, nullptr, &context_.commandPool));
    context_.resourceManager.addCommandPool(context_.commandPool);

    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, context_.surface);
    swapchainManager_->initializeSwapchain(width_, height_);
    context_.swapchain = swapchainManager_->getSwapchain();
    context_.swapchainImageFormat = swapchainManager_->getSwapchainImageFormat();
    context_.swapchainExtent = swapchainManager_->getSwapchainExtent();
    context_.swapchainImages = swapchainManager_->getSwapchainImages();
    context_.swapchainImageViews = swapchainManager_->getSwapchainImageViews();

    width_ = static_cast<int>(context_.swapchainExtent.width);
    height_ = static_cast<int>(context_.swapchainExtent.height);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames_[i].imageAvailableSemaphore = swapchainManager_->getImageAvailableSemaphore(i);
        frames_[i].renderFinishedSemaphore = swapchainManager_->getRenderFinishedSemaphore(i);
        frames_[i].fence = swapchainManager_->getInFlightFence(i);
    }

    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_, width_, height_);
    pipelineManager_->createRayTracingPipeline();
    pipelineManager_->createComputePipeline();
    computeDescriptorSetLayout_ = pipelineManager_->getComputeDescriptorSetLayout();
    if (computeDescriptorSetLayout_ == VK_NULL_HANDLE) throw std::runtime_error("No compute layout");
    pipelineManager_->createGraphicsPipeline(width_, height_);
    context_.rayTracingDescriptorSetLayout = pipelineManager_->createRayTracingDescriptorSetLayout();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();
    rtPipeline_ = pipelineManager_->getRayTracingPipeline();
    pipelineManager_->createShaderBindingTable();
    sbt_ = pipelineManager_->getShaderBindingTable();

    const auto& vertices = getVertices();
    const auto& indices = getIndices();
    if (vertices.empty() || indices.empty()) throw std::runtime_error("Empty mesh");

    bufferManager_ = std::make_unique<VulkanBufferManager>(context_, std::span(vertices), std::span(indices));
    indexCount_ = static_cast<uint32_t>(indices.size());

    buildAccelerationStructures();

    rtOutputImages_.fill(VK_NULL_HANDLE);
    rtOutputMemories_.fill(VK_NULL_HANDLE);
    rtOutputViews_.fill(VK_NULL_HANDLE);
    createRTOutputImage(); rtOutputImages_[0] = rtOutputImage_.get(); rtOutputMemories_[0] = rtOutputImageMemory_.get(); rtOutputViews_[0] = rtOutputImageView_.get();
    createRTOutputImage(); rtOutputImages_[1] = rtOutputImage_.get(); rtOutputMemories_[1] = rtOutputImageMemory_.get(); rtOutputViews_[1] = rtOutputImageView_.get();
    currentRTIndex_ = 0;

    accumImages_.fill(VK_NULL_HANDLE);
    accumMemories_.fill(VK_NULL_HANDLE);
    accumViews_.fill(VK_NULL_HANDLE);
    createAccumulationImage(); accumImages_[0] = accumImage_.get(); accumMemories_[0] = accumImageMemory_.get(); accumViews_[0] = accumImageView_.get();
    createAccumulationImage(); accumImages_[1] = accumImage_.get(); accumMemories_[1] = accumImageMemory_.get(); accumViews_[1] = accumImageView_.get();
    currentAccumIndex_ = 0;

    createFramebuffers();
    createCommandBuffers();
    createEnvironmentMap();

    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, sizeof(MaterialData) * 128, sizeof(DimensionData));
    createDescriptorPool();
    createDescriptorSets();
    createComputeDescriptorSets();
    updateRTDescriptors();

    for (auto& pool : queryPools_) {
        VkQueryPoolCreateInfo info = { .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 8 };
        VK_CHECK(vkCreateQueryPool(context_.device, &info, nullptr, &pool));
    }
    queryReady_.fill(false);

    LOG_INFO_CAT("Renderer", "=== VulkanRenderer Initialized ===");
}

// -----------------------------------------------------------------------------
// DESTRUCTOR
// -----------------------------------------------------------------------------
VulkanRenderer::~VulkanRenderer() { cleanup(); }

void VulkanRenderer::cleanup() noexcept {
    LOG_INFO_CAT("Renderer", "=== Cleanup Start ===");
    vkDeviceWaitIdle(context_.device);
    queryReady_.fill(false);
    for (auto& pool : queryPools_) if (pool) vkDestroyQueryPool(context_.device, pool, nullptr);
    for (int i = 0; i < 2; ++i) {
        if (rtOutputImages_[i]) {
            vkDestroyImageView(context_.device, rtOutputViews_[i], nullptr);
            vkFreeMemory(context_.device, rtOutputMemories_[i], nullptr);
            vkDestroyImage(context_.device, rtOutputImages_[i], nullptr);
            rtOutputImages_[i] = VK_NULL_HANDLE;
            rtOutputMemories_[i] = VK_NULL_HANDLE;
            rtOutputViews_[i] = VK_NULL_HANDLE;
        }
        if (accumImages_[i]) {
            vkDestroyImageView(context_.device, accumViews_[i], nullptr);
            vkFreeMemory(context_.device, accumMemories_[i], nullptr);
            vkDestroyImage(context_.device, accumImages_[i], nullptr);
            accumImages_[i] = VK_NULL_HANDLE;
            accumMemories_[i] = VK_NULL_HANDLE;
            accumViews_[i] = VK_NULL_HANDLE;
        }
    }
    LOG_INFO_CAT("Renderer", "=== Cleanup Done ===");
}

// -----------------------------------------------------------------------------
// PUBLIC: HANDLE RESIZE (immediate)
// -----------------------------------------------------------------------------
void VulkanRenderer::handleResize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == width_ && height == height_) return;

    LOG_INFO_CAT("Renderer", "Resize requested: {}x{} → {}x{}", width_, height_, width, height);
    applyResize(width, height);
}

// -----------------------------------------------------------------------------
// PRIVATE: APPLY RESIZE
// -----------------------------------------------------------------------------
void VulkanRenderer::applyResize(int newWidth, int newHeight) {
    LOG_INFO_CAT("Renderer", "Applying resize {}x{}...", newWidth, newHeight);

    for (auto& f : frames_) {
        VK_CHECK(vkWaitForFences(context_.device, 1, &f.fence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(context_.device, 1, &f.fence));
    }

    width_ = newWidth;
    height_ = newHeight;

    swapchainRecreating_.store(true);
    swapchainManager_->handleResize(width_, height_);
    swapchainRecreating_.store(false);

    context_.swapchain           = swapchainManager_->getSwapchain();
    context_.swapchainImageFormat = swapchainManager_->getSwapchainImageFormat();
    context_.swapchainExtent     = swapchainManager_->getSwapchainExtent();
    context_.swapchainImages     = swapchainManager_->getSwapchainImages();
    context_.swapchainImageViews = swapchainManager_->getSwapchainImageViews();

    width_  = static_cast<int>(context_.swapchainExtent.width);
    height_ = static_cast<int>(context_.swapchainExtent.height);

    recreateRTOutputImage();
    recreateAccumulationImage();
    createFramebuffers();

    descriptorsUpdated_ = false;
    updateRTDescriptors();

    resetAccumulation_ = true;

    LOG_INFO_CAT("Renderer", "Resize applied: {}x{}", width_, height_);
}

// -----------------------------------------------------------------------------
// PUBLIC: RENDER FRAME
// -----------------------------------------------------------------------------
void VulkanRenderer::renderFrame(const Camera& camera) {
    auto frameStart = std::chrono::steady_clock::now();

    // No queued resize — everything is up-to-date
    if (swapchainRecreating_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return;
    }

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_.device, context_.swapchain, 1'000'000,
                                            frames_[currentFrame_].imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOG_INFO_CAT("Renderer", "Swapchain out-of-date during acquire → recreating");
        handleResize(width_, height_);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR_CAT("Renderer", "Failed to acquire swapchain image: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    updateUniformBuffer(currentFrame_, camera);

    VkCommandBuffer cmd = commandBuffers_[currentFrame_];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    vkCmdResetQueryPool(cmd, queryPools_[currentFrame_], 0, 8);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPools_[currentFrame_], 0);

    VkImage rtImg = rtOutputImages_[currentRTIndex_];
    VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkImageMemoryBarrier rtWriteBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = rtImg,
        .subresourceRange = range
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &rtWriteBarrier);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_, 0, 1, &descriptorSets_[currentFrame_], 0, nullptr);

    const auto& sbt = pipelineManager_->getShaderBindingTable();
    context_.vkCmdTraceRaysKHR(cmd,
        &sbt.raygen, &sbt.miss, &sbt.hit, &sbt.callable,
        context_.swapchainExtent.width, context_.swapchainExtent.height, 1);

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, queryPools_[currentFrame_], 1);

    VkImageMemoryBarrier rtToTransferBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .image = rtImg,
        .subresourceRange = range
    };

    VkImageMemoryBarrier swapchainToTransferBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = context_.swapchainImages[imageIndex],
        .subresourceRange = range
    };

    VkImageMemoryBarrier barriers[2] = { rtToTransferBarrier, swapchainToTransferBarrier };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

    VkImageCopy copyRegion = {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .extent = { context_.swapchainExtent.width, context_.swapchainExtent.height, 1 }
    };
    vkCmdCopyImage(cmd, rtImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   context_.swapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    VkImageMemoryBarrier swapchainToPresentBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = context_.swapchainImages[imageIndex],
        .subresourceRange = range
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapchainToPresentBarrier);

    VkImageMemoryBarrier rtBackToGeneral = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = rtImg,
        .subresourceRange = range
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &rtBackToGeneral);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames_[currentFrame_].imageAvailableSemaphore,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frames_[currentFrame_].renderFinishedSemaphore
    };
    VK_CHECK(vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, frames_[currentFrame_].fence));

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames_[currentFrame_].renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &context_.swapchain,
        .pImageIndices = &imageIndex
    };
    result = vkQueuePresentKHR(context_.graphicsQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        LOG_INFO_CAT("Renderer", "Swapchain out-of-date on present → recreating");
        handleResize(width_, height_);
    } else if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to present: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to present");
    }

    uint32_t prevFrame = (currentFrame_ + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT;
    if (queryReady_[prevFrame]) {
        uint64_t timestamps[2];
        VkResult queryResult = vkGetQueryPoolResults(context_.device, queryPools_[prevFrame], 0, 2, sizeof(uint64_t)*2, timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
        if (queryResult == VK_SUCCESS) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(context_.physicalDevice, &props);
            double period = props.limits.timestampPeriod;
            double rtTimeMs = (timestamps[1] - timestamps[0]) * period * 1e-6;
            lastRTTimeMs_[prevFrame] = rtTimeMs;
            LOG_INFO_CAT("GPU", "RayTrace: {:.3f}ms", rtTimeMs);
        }
    }
    queryReady_[currentFrame_] = true;

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    ++frameCount_; ++framesThisSecond_;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFPSTime_).count();
    if (elapsed >= 1000) {
        double fps = framesThisSecond_ * 1000.0 / elapsed;
        double frameTimeMs = std::chrono::duration_cast<std::chrono::microseconds>(now - frameStart).count() / 1000.0;
        LOG_INFO_CAT("Renderer", "FPS: {:.1f} | Frame: {} | Time: {:.2f}ms | Size: {}x{}", 
                     fps, frameCount_, frameTimeMs, context_.swapchainExtent.width, context_.swapchainExtent.height);
        framesThisSecond_ = 0;
        lastFPSTime_ = now;
    }
}

// -----------------------------------------------------------------------------
// INITIALIZE BUFFER DATA
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
// CREATE RT OUTPUT IMAGE
// -----------------------------------------------------------------------------
void VulkanRenderer::createRTOutputImage() {
    VkExtent2D extent = context_.swapchainExtent;
    LOG_INFO_CAT("Renderer", "Creating RT output image: {}x{}", extent.width, extent.height);

    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = { extent.width, extent.height, 1 },
        .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage img; VK_CHECK(vkCreateImage(context_.device, &imageInfo, nullptr, &img));
    VkMemoryRequirements memReqs; vkGetImageMemoryRequirements(context_.device, img, &memReqs);
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = VulkanInitializer::findMemoryType(context_.physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VkDeviceMemory mem; VK_CHECK(vkAllocateMemory(context_.device, &allocInfo, nullptr, &mem));
    VK_CHECK(vkBindImageMemory(context_.device, img, mem, 0));

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkImageView view; VK_CHECK(vkCreateImageView(context_.device, &viewInfo, nullptr, &view));

    VulkanInitializer::transitionImageLayout(context_, img, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    rtOutputImage_.reset(img);
    rtOutputImageMemory_.reset(mem);
    rtOutputImageView_.reset(view);
    LOG_INFO_CAT("Renderer", "RT output image created successfully.");
}

// -----------------------------------------------------------------------------
// CREATE ACCUMULATION IMAGE
// -----------------------------------------------------------------------------
void VulkanRenderer::createAccumulationImage() {
    VkExtent2D extent = context_.swapchainExtent;
    LOG_INFO_CAT("Renderer", "Creating accumulation image: {}x{}", extent.width, extent.height);

    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = { extent.width, extent.height, 1 },
        .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage img; VK_CHECK(vkCreateImage(context_.device, &imageInfo, nullptr, &img));
    VkMemoryRequirements memReqs; vkGetImageMemoryRequirements(context_.device, img, &memReqs);
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = VulkanInitializer::findMemoryType(context_.physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VkDeviceMemory mem; VK_CHECK(vkAllocateMemory(context_.device, &allocInfo, nullptr, &mem));
    VK_CHECK(vkBindImageMemory(context_.device, img, mem, 0));

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkImageView view; VK_CHECK(vkCreateImageView(context_.device, &viewInfo, nullptr, &view));

    VulkanInitializer::transitionImageLayout(context_, img, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    accumImage_.reset(img);
    accumImageMemory_.reset(mem);
    accumImageView_.reset(view);
    LOG_INFO_CAT("Renderer", "Accumulation image created successfully.");
}

// -----------------------------------------------------------------------------
// RECREATE RT OUTPUT IMAGE
// -----------------------------------------------------------------------------
void VulkanRenderer::recreateRTOutputImage() {
    LOG_INFO_CAT("Renderer", "Recreating RT output image (double-buffered flip)...");
    uint32_t next = (currentRTIndex_ + 1) % 2;

    // Destroy resources in target slot to avoid use-after-free
    if (rtOutputImages_[next] != VK_NULL_HANDLE) {
        vkDestroyImageView(context_.device, rtOutputViews_[next], nullptr);
        vkFreeMemory(context_.device, rtOutputMemories_[next], nullptr);
        vkDestroyImage(context_.device, rtOutputImages_[next], nullptr);
        rtOutputImages_[next] = VK_NULL_HANDLE;
        rtOutputMemories_[next] = VK_NULL_HANDLE;
        rtOutputViews_[next] = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old RT resources in slot {}", next);
    }

    // Now safe to reset temp unique_ptr
    rtOutputImage_.reset(); rtOutputImageMemory_.reset(); rtOutputImageView_.reset();

    createRTOutputImage();
    rtOutputImages_[next] = rtOutputImage_.get();
    rtOutputMemories_[next] = rtOutputImageMemory_.get();
    rtOutputViews_[next] = rtOutputImageView_.get();
    currentRTIndex_ = next;
    LOG_INFO_CAT("Renderer", "RT output image recreated. New index: {}", currentRTIndex_);
}

// -----------------------------------------------------------------------------
// RECREATE ACCUMULATION IMAGE
// -----------------------------------------------------------------------------
void VulkanRenderer::recreateAccumulationImage() {
    LOG_INFO_CAT("Renderer", "Recreating accumulation image (double-buffered flip)...");
    uint32_t next = (currentAccumIndex_ + 1) % 2;

    // Destroy resources in target slot
    if (accumImages_[next] != VK_NULL_HANDLE) {
        vkDestroyImageView(context_.device, accumViews_[next], nullptr);
        vkFreeMemory(context_.device, accumMemories_[next], nullptr);
        vkDestroyImage(context_.device, accumImages_[next], nullptr);
        accumImages_[next] = VK_NULL_HANDLE;
        accumMemories_[next] = VK_NULL_HANDLE;
        accumViews_[next] = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old accum resources in slot {}", next);
    }

    accumImage_.reset(); accumImageMemory_.reset(); accumImageView_.reset();

    createAccumulationImage();
    accumImages_[next] = accumImage_.get();
    accumMemories_[next] = accumImageMemory_.get();
    accumViews_[next] = accumImageView_.get();
    currentAccumIndex_ = next;
    LOG_INFO_CAT("Renderer", "Accumulation image recreated. New index: {}", currentAccumIndex_);
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
// CREATE COMPUTE DESCRIPTOR SETS
// -----------------------------------------------------------------------------
void VulkanRenderer::createComputeDescriptorSets() {
    LOG_INFO_CAT("Renderer", "Creating compute descriptor sets...");

    if (computeDescriptorSetLayout_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Compute descriptor set layout not created");
        throw std::runtime_error("Compute descriptor set layout missing");
    }

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout_);
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool_,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts.data()
    };

    computeDescriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(context_.device, &allocInfo, computeDescriptorSets_.data()));

    LOG_INFO_CAT("Renderer", "Compute descriptor sets allocated: {}", MAX_FRAMES_IN_FLIGHT);
}

// -----------------------------------------------------------------------------
// UPDATE UNIFORM BUFFER
// -----------------------------------------------------------------------------
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
// UPDATE RT DESCRIPTORS
// -----------------------------------------------------------------------------
void VulkanRenderer::updateRTDescriptors() {
    if (tlasHandle_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "TLAS not built — cannot update descriptors");
        throw std::runtime_error("TLAS not built");
    }
    if (descriptorsUpdated_) {
        LOG_DEBUG_CAT("Renderer", "Descriptors already updated — skipping");
        return;
    }

    LOG_INFO_CAT("Renderer", "Updating RT descriptors for {} frames...", MAX_FRAMES_IN_FLIGHT);
    const uint32_t totalWrites = MAX_FRAMES_IN_FLIGHT * 12;

    std::vector<VkWriteDescriptorSetAccelerationStructureKHR> asWrites(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorImageInfo> outputInfos(MAX_FRAMES_IN_FLIGHT), accumInfos(MAX_FRAMES_IN_FLIGHT), envMapInfos(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorBufferInfo> uniformInfos(MAX_FRAMES_IN_FLIGHT), materialInfos(MAX_FRAMES_IN_FLIGHT), dimensionInfos(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkWriteDescriptorSet> writes(totalWrites);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        uint32_t base = i * 12;

        asWrites[i] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, .accelerationStructureCount = 1, .pAccelerationStructures = &tlasHandle_ };
        writes[base + 0] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &asWrites[i], .dstSet = descriptorSets_[i], .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR };

        outputInfos[i] = { .imageView = rtOutputViews_[currentRTIndex_], .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
        writes[base + 1] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSets_[i], .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfos[i] };

        uniformInfos[i] = { .buffer = context_.uniformBuffers[i], .range = VK_WHOLE_SIZE };
        writes[base + 2] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSets_[i], .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &uniformInfos[i] };

        materialInfos[i] = { .buffer = materialBuffers_[i], .range = VK_WHOLE_SIZE };
        writes[base + 3] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSets_[i], .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &materialInfos[i] };

        dimensionInfos[i] = { .buffer = dimensionBuffers_[i], .range = VK_WHOLE_SIZE };
        writes[base + 4] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSets_[i], .dstBinding = 4, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dimensionInfos[i] };

        envMapInfos[i] = { .sampler = envMapSampler_, .imageView = envMapImageView_, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        for (int b = 5; b <= 7; ++b) {
            writes[base + b] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSets_[i], .dstBinding = static_cast<uint32_t>(b), .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &envMapInfos[i] };
        }

        writes[base + 8] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSets_[i], .dstBinding = 8, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfos[i] };
        writes[base + 9] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSets_[i], .dstBinding = 9, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfos[i] };

        accumInfos[i] = { .imageView = accumViews_[currentAccumIndex_], .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
        writes[base + 10] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSets_[i], .dstBinding = 10, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfos[i] };
        writes[base + 11] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSets_[i], .dstBinding = 11, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfos[i] };
    }

    vkUpdateDescriptorSets(context_.device, totalWrites, writes.data(), 0, nullptr);
    descriptorsUpdated_ = true;
    LOG_INFO_CAT("Renderer", "RT descriptors updated: {} writes", totalWrites);
}

// -----------------------------------------------------------------------------
// BUILD ACCELERATION STRUCTURES
// -----------------------------------------------------------------------------
void VulkanRenderer::buildAccelerationStructures() {
    LOG_INFO_CAT("Renderer", "Building acceleration structures...");

    const auto& vertices = getVertices();
    const auto& indices = getIndices();

    if (vertices.empty() || indices.empty()) {
        LOG_ERROR_CAT("Renderer", "Cannot build AS: vertex or index buffer empty");
        throw std::runtime_error("Empty geometry for AS build");
    }

    VkDeviceAddress vertexAddress = bufferManager_->getVertexBufferAddress();
    VkDeviceAddress indexAddress  = bufferManager_->getIndexBufferAddress();

    LOG_INFO_CAT("Renderer", "Vertex buffer address: 0x{:x}", vertexAddress);
    LOG_INFO_CAT("Renderer", "Index buffer address:  0x{:x}", indexAddress);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = vertexAddress },
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = static_cast<uint32_t>(vertices.size()),
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = indexAddress },
        .transformData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = triangles },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    const uint32_t primitiveCount = static_cast<uint32_t>(indices.size() / 3);

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    context_.vkGetAccelerationStructureBuildSizesKHR(
        context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizeInfo
    );

    LOG_INFO_CAT("Renderer", "BLAS size: {} bytes (accel: {} bytes, scratch: {} bytes)",
                 sizeInfo.accelerationStructureSize,
                 sizeInfo.accelerationStructureSize,
                 sizeInfo.buildScratchSize);

    bufferManager_->createBuffer(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        blasBuffer_, blasBufferMemory_
    );

    VkAccelerationStructureCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blasBuffer_,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &createInfo, nullptr, &blasHandle_));

    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
    bufferManager_->createBuffer(
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer, scratchMemory
    );
    VkDeviceAddress scratchAddress = bufferManager_->getDeviceAddress(scratchBuffer);

    buildInfo.dstAccelerationStructure = blasHandle_;
    buildInfo.scratchData = { .deviceAddress = scratchAddress };

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(context_.device, &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    const VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {
        .primitiveCount = primitiveCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    context_.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VK_CHECK(vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    vkQueueWaitIdle(context_.graphicsQueue);

    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
    vkDestroyBuffer(context_.device, scratchBuffer, nullptr);
    vkFreeMemory(context_.device, scratchMemory, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blasHandle_
    };
    VkDeviceAddress blasAddress = context_.vkGetAccelerationStructureDeviceAddressKHR(context_.device, &addressInfo);

    LOG_INFO_CAT("Renderer", "BLAS built: handle=0x{:x}, address=0x{:x}", reinterpret_cast<uint64_t>(blasHandle_), blasAddress);

    VkAccelerationStructureInstanceKHR instance = {
        .transform = {1.0f, 0.0f, 0.0f, 0.0f,
                      0.0f, 1.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 1.0f, 0.0f},
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = blasAddress
    };

    bufferManager_->createBuffer(
        sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        instanceBuffer_, instanceBufferMemory_
    );

    void* data = nullptr;
    VK_CHECK(vkMapMemory(context_.device, instanceBufferMemory_, 0, sizeof(instance), 0, &data));
    memcpy(data, &instance, sizeof(instance));
    vkUnmapMemory(context_.device, instanceBufferMemory_);

    VkDeviceAddress instanceAddress = bufferManager_->getDeviceAddress(instanceBuffer_);

    VkAccelerationStructureGeometryInstancesDataKHR instancesData = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data = { .deviceAddress = instanceAddress }
    };

    VkAccelerationStructureGeometryKHR tlasGeometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = instancesData }
    };

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &tlasGeometry
    };

    const uint32_t instanceCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    context_.vkGetAccelerationStructureBuildSizesKHR(
        context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &tlasBuildInfo, &instanceCount, &tlasSizeInfo
    );

    LOG_INFO_CAT("Renderer", "TLAS size: {} bytes", tlasSizeInfo.accelerationStructureSize);

    bufferManager_->createBuffer(
        tlasSizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasBuffer_, tlasBufferMemory_
    );

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlasBuffer_,
        .size = tlasSizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &tlasCreateInfo, nullptr, &tlasHandle_));

    bufferManager_->createBuffer(
        tlasSizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer, scratchMemory
    );
    scratchAddress = bufferManager_->getDeviceAddress(scratchBuffer);

    tlasBuildInfo.dstAccelerationStructure = tlasHandle_;
    tlasBuildInfo.scratchData = { .deviceAddress = scratchAddress };

    VK_CHECK(vkAllocateCommandBuffers(context_.device, &allocInfo, &cmd));
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    const VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo = {
        .primitiveCount = instanceCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pTlasRangeInfo = &tlasRangeInfo;

    context_.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuildInfo, &pTlasRangeInfo);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VK_CHECK(vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    vkQueueWaitIdle(context_.graphicsQueue);

    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
    vkDestroyBuffer(context_.device, scratchBuffer, nullptr);
    vkFreeMemory(context_.device, scratchMemory, nullptr);

    addressInfo.accelerationStructure = tlasHandle_;
    tlasDeviceAddress_ = context_.vkGetAccelerationStructureDeviceAddressKHR(context_.device, &addressInfo);

    LOG_INFO_CAT("Renderer", "TLAS built: handle=0x{:x}, address=0x{:x}", reinterpret_cast<uint64_t>(tlasHandle_), tlasDeviceAddress_);
    LOG_INFO_CAT("Renderer", "Acceleration structures built successfully.");
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
// MESH LOADING
// -----------------------------------------------------------------------------
std::vector<glm::vec3> VulkanRenderer::getVertices() const {
    static std::vector<glm::vec3> cached;
    if (!cached.empty()) return cached;

    LOG_INFO_CAT("Renderer", "Loading vertices from assets/models/scene.obj");
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "assets/models/scene.obj")) {
        LOG_ERROR_CAT("Renderer", "Failed to load OBJ: {}", err.empty() ? warn : err);
        throw std::runtime_error("Failed to load OBJ");
    }
    if (!warn.empty()) LOG_WARNING_CAT("Renderer", "OBJ warnings: {}", warn);

    std::vector<glm::vec3> verts;
    verts.reserve(attrib.vertices.size() / 3);
    for (size_t i = 0; i < attrib.vertices.size(); i += 3) {
        verts.emplace_back(attrib.vertices[i], attrib.vertices[i + 1], attrib.vertices[i + 2]);
    }
    cached = std::move(verts);
    LOG_INFO_CAT("Renderer", "Loaded {} unique vertices from OBJ.", cached.size());
    return cached;
}

std::vector<uint32_t> VulkanRenderer::getIndices() const {
    static std::vector<uint32_t> cached;
    if (!cached.empty()) return cached;

    LOG_INFO_CAT("Renderer", "Loading indices from assets/models/scene.obj");
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "assets/models/scene.obj")) {
        LOG_ERROR_CAT("Renderer", "Failed to load OBJ: {}", err.empty() ? warn : err);
        throw std::runtime_error("Failed to load OBJ");
    }
    if (!warn.empty()) LOG_WARNING_CAT("Renderer", "OBJ warnings: {}", warn);

    std::vector<uint32_t> idxs;
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            idxs.push_back(static_cast<uint32_t>(index.vertex_index));
        }
    }
    cached = std::move(idxs);
    LOG_INFO_CAT("Renderer", "Loaded {} indices from OBJ ({} triangles).", cached.size(), cached.size() / 3);
    return cached;
}

} // namespace VulkanRTX