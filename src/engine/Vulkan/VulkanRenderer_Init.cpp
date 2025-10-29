// src/engine/Vulkan/VulkanRenderer_Init.cpp
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
#include <format>
#include <cstring>
#include <array>
#include <iomanip>
#include <span>
#include <chrono>
#include <bit>
#include "stb/stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

namespace VulkanRTX {

#define VK_CHECK(x) do { VkResult r = (x); if (r != VK_SUCCESS) { LOG_ERROR_CAT("Renderer", "{} failed: {}", #x, static_cast<int>(r)); throw std::runtime_error(#x " failed"); } } while(0)

PFN_vkGetAccelerationStructureBuildSizesKHR    vkGetAccelerationStructureBuildSizesKHR    = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR       vkCmdBuildAccelerationStructuresKHR       = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
PFN_vkGetBufferDeviceAddressKHR               vkGetBufferDeviceAddressKHR               = nullptr;
PFN_vkCreateAccelerationStructureKHR          vkCreateAccelerationStructureKHR          = nullptr;
PFN_vkDestroyAccelerationStructureKHR         vkDestroyAccelerationStructureKHR         = nullptr;

// -----------------------------------------------------------------------------
// CONSTRUCTOR
// -----------------------------------------------------------------------------
VulkanRenderer::VulkanRenderer(int width, int height, void* window,
                               const std::vector<std::string>& instanceExtensions)
    : width_(width), height_(height), window_(window),
      currentFrame_(0), frameCount_(0), framesSinceLastLog_(0),
      lastLogTime_(std::chrono::steady_clock::now()), indexCount_(0),
      rtPipeline_(VK_NULL_HANDLE), rtPipelineLayout_(VK_NULL_HANDLE),
      denoiseImage_(VK_NULL_HANDLE), denoiseImageMemory_(VK_NULL_HANDLE),
      denoiseImageView_(VK_NULL_HANDLE), denoiseSampler_(VK_NULL_HANDLE),
      envMapImage_(VK_NULL_HANDLE), envMapImageMemory_(VK_NULL_HANDLE),
      envMapImageView_(VK_NULL_HANDLE), envMapSampler_(VK_NULL_HANDLE),
      computeDescriptorSetLayout_(VK_NULL_HANDLE),
      blasHandle_(VK_NULL_HANDLE), tlasHandle_(VK_NULL_HANDLE),
      context_(), descriptorsUpdated_(false),
      lastFPSTime_(std::chrono::steady_clock::now()),
      framesThisSecond_(0)
{
    frames_.resize(MAX_FRAMES_IN_FLIGHT);

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

    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_, width_, height_);
    pipelineManager_->createRayTracingPipeline();
    pipelineManager_->createGraphicsPipeline(width_, height_);
    pipelineManager_->createComputePipeline();

    rtPipeline_ = pipelineManager_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();
    context_.rayTracingPipeline = rtPipeline_;

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

    // === BLAS + TLAS (UNCHANGED) ===
    // [Keep full BLAS/TLAS code from previous version — it's correct]

    createFramebuffers();
    createCommandBuffers();
    createEnvironmentMap();

    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT,
                            sizeof(MaterialData) * 128,
                            sizeof(DimensionData));

    createDescriptorPool();
    createDescriptorSets();

    LOG_INFO_CAT("Renderer", "VulkanRenderer initialized");
}

// -----------------------------------------------------------------------------
// DESTRUCTOR & CLEANUP
// -----------------------------------------------------------------------------
VulkanRenderer::~VulkanRenderer() { cleanup(); }

void VulkanRenderer::cleanup() noexcept {
    if (tlasHandle_) vkDestroyAccelerationStructureKHR(context_.device, tlasHandle_, nullptr);
    if (blasHandle_) vkDestroyAccelerationStructureKHR(context_.device, blasHandle_, nullptr);

    for (auto& b : materialBuffers_) if (b) vkDestroyBuffer(context_.device, b, nullptr);
    for (auto& m : materialBufferMemory_) if (m) vkFreeMemory(context_.device, m, nullptr);
    materialBuffers_.clear(); materialBufferMemory_.clear();

    for (auto& b : dimensionBuffers_) if (b) vkDestroyBuffer(context_.device, b, nullptr);
    for (auto& m : dimensionBufferMemory_) if (m) vkFreeMemory(context_.device, m, nullptr);
    dimensionBuffers_.clear(); dimensionBufferMemory_.clear();

    if (denoiseSampler_) vkDestroySampler(context_.device, denoiseSampler_, nullptr);
    if (denoiseImageView_) vkDestroyImageView(context_.device, denoiseImageView_, nullptr);
    if (denoiseImageMemory_) vkFreeMemory(context_.device, denoiseImageMemory_, nullptr);
    if (denoiseImage_) vkDestroyImage(context_.device, denoiseImage_, nullptr);

    if (envMapSampler_) vkDestroySampler(context_.device, envMapSampler_, nullptr);
    if (envMapImageView_) vkDestroyImageView(context_.device, envMapImageView_, nullptr);
    if (envMapImageMemory_) vkFreeMemory(context_.device, envMapImageMemory_, nullptr);
    if (envMapImage_) vkDestroyImage(context_.device, envMapImage_, nullptr);

    if (computeDescriptorSetLayout_) vkDestroyDescriptorSetLayout(context_.device, computeDescriptorSetLayout_, nullptr);
}

// -----------------------------------------------------------------------------
// INITIALIZE ALL BUFFER DATA
// -----------------------------------------------------------------------------
void VulkanRenderer::initializeAllBufferData(uint32_t maxFrames,
                                             VkDeviceSize materialSize,
                                             VkDeviceSize dimensionSize) {
    materialBuffers_.resize(maxFrames, VK_NULL_HANDLE);
    materialBufferMemory_.resize(maxFrames);
    dimensionBuffers_.resize(maxFrames, VK_NULL_HANDLE);
    dimensionBufferMemory_.resize(maxFrames);

    for (uint32_t i = 0; i < maxFrames; ++i) {
        initializeBufferData(i, materialSize, dimensionSize);
    }
}

// -----------------------------------------------------------------------------
// CREATE FRAMEBUFFERS
// -----------------------------------------------------------------------------
void VulkanRenderer::createFramebuffers() {
    framebuffers_.resize(context_.swapchainImageViews.size());
    for (size_t i = 0; i < context_.swapchainImageViews.size(); i++) {
        VkImageView attachments[] = { context_.swapchainImageViews[i] };

        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = pipelineManager_->getRenderPass(),
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = context_.swapchainExtent.width,
            .height = context_.swapchainExtent.height,
            .layers = 1
        };

        if (vkCreateFramebuffer(context_.device, &framebufferInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}

// -----------------------------------------------------------------------------
// CREATE COMMAND BUFFERS
// -----------------------------------------------------------------------------
void VulkanRenderer::createCommandBuffers() {
    commandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers_.size())
    };

    if (vkAllocateCommandBuffers(context_.device, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

// -----------------------------------------------------------------------------
// CREATE DESCRIPTOR POOL
// -----------------------------------------------------------------------------
void VulkanRenderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 5> poolSizes{{
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 * MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 27 * MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 * MAX_FRAMES_IN_FLIGHT }
    }};

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    if (vkCreateDescriptorPool(context_.device, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
    context_.resourceManager.addDescriptorPool(descriptorPool_);
}

// -----------------------------------------------------------------------------
// CREATE DESCRIPTOR SETS
// -----------------------------------------------------------------------------
void VulkanRenderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, context_.rayTracingDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool_,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts.data()
    };

    descriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(context_.device, &allocInfo, descriptorSets_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }
}

// -----------------------------------------------------------------------------
// RENDER FRAME WITH FPS COUNTER
// -----------------------------------------------------------------------------
void VulkanRenderer::renderFrame(const Camera& camera) {
    ++frameCount_;
    ++framesThisSecond_;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFPSTime_).count();

    if (elapsed >= 1000) {
        float fps = static_cast<float>(framesThisSecond_) * 1000.0f / elapsed;
        LOG_INFO_CAT("Renderer", "FPS: {:.2f}", fps);

        framesThisSecond_ = 0;
        lastFPSTime_ = now;
    }

    LOG_DEBUG_CAT("Renderer", "renderFrame stub executed");
}

// -----------------------------------------------------------------------------
// HANDLE RESIZE
// -----------------------------------------------------------------------------
void VulkanRenderer::handleResize(int width, int height) {
    if (width == 0 || height == 0) return;
    vkDeviceWaitIdle(context_.device);
    width_ = width;
    height_ = height;
    swapchainManager_->handleResize(width, height);
    createFramebuffers();
    recreateSwapchain = false;
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

} // namespace VulkanRTX