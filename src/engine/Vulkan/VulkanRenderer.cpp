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
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

namespace VulkanRTX {

VulkanRenderer::VulkanRenderer(int width, int height, void* window, const std::vector<std::string>& instanceExtensions)
    : width_(width),
      height_(height),
      window_(window),
      currentFrame_(0),
      frameCount_(0),
      lastLogTime_(std::chrono::steady_clock::now()),
      framesSinceLastLog_(0),
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
      context_() {
    LOG_INFO_CAT("Renderer", "Constructing VulkanRenderer with width={}, height={}, window={:p}", width, height, window);
    if (width <= 0 || height <= 0 || window == nullptr) {
        LOG_ERROR_CAT("Renderer", "Invalid window parameters: width={}, height={}, window={:p}", width, height, window);
        throw std::invalid_argument("Invalid window parameters");
    }

    frames_.resize(MAX_FRAMES_IN_FLIGHT);
    if (frames_.size() != MAX_FRAMES_IN_FLIGHT) {
        LOG_ERROR_CAT("Renderer", "Failed to resize frames_ to MAX_FRAMES_IN_FLIGHT ({}), current size: {}", MAX_FRAMES_IN_FLIGHT, frames_.size());
        throw std::runtime_error("Failed to initialize frames_ vector");
    }
    LOG_DEBUG_CAT("Renderer", "Initialized frames_ vector with size: {}", frames_.size());

    VulkanInitializer::initInstance(instanceExtensions, context_);
    VulkanInitializer::initSurface(context_, window_, nullptr);
    context_.physicalDevice = VulkanInitializer::findPhysicalDevice(context_.instance, context_.surface, true);
    VulkanInitializer::initDevice(context_);
    context_.resourceManager.setDevice(context_.device);
    LOG_DEBUG_CAT("Renderer", "Initialized Vulkan instance, surface, physical device, and device");

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context_.graphicsQueueFamilyIndex
    };
    VkResult res = vkCreateCommandPool(context_.device, &poolInfo, nullptr, &context_.commandPool);
    if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to create command pool: VkResult={}", static_cast<int>(res));
        throw std::runtime_error("Command pool creation failed");
    }
    context_.resourceManager.addCommandPool(context_.commandPool);
    LOG_INFO_CAT("Renderer", "Created command pool: {:p} and added to resource manager", static_cast<void*>(context_.commandPool));

    VkDescriptorSetLayoutBinding computeBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };
    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 2,
        .pBindings = computeBindings
    };
    if (vkCreateDescriptorSetLayout(context_.device, &computeLayoutInfo, nullptr, &computeDescriptorSetLayout_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to create compute descriptor set layout");
        throw std::runtime_error("Failed to create compute descriptor set layout");
    }
    context_.resourceManager.addDescriptorSetLayout(computeDescriptorSetLayout_);
    LOG_DEBUG_CAT("Renderer", "Created compute descriptor set layout: {:p}", static_cast<void*>(computeDescriptorSetLayout_));

    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, context_.surface);
    swapchainManager_->initializeSwapchain(width_, height_);
    context_.swapchain = swapchainManager_->getSwapchain();
    context_.swapchainImageFormat = swapchainManager_->getSwapchainImageFormat();
    context_.swapchainExtent = swapchainManager_->getSwapchainExtent();
    context_.swapchainImages = swapchainManager_->getSwapchainImages();
    context_.swapchainImageViews = swapchainManager_->getSwapchainImageViews();
    LOG_DEBUG_CAT("Renderer", "Swapchain initialized: extent={}x{}, imageCount={}, viewCount={}",
                  context_.swapchainExtent.width, context_.swapchainExtent.height,
                  context_.swapchainImages.size(), context_.swapchainImageViews.size());
    if (context_.swapchainImageViews.empty()) {
        LOG_ERROR_CAT("Renderer", "Swapchain image views are empty after initialization");
        throw std::runtime_error("Failed to create swapchain image views");
    }

    pipelineManager_ = std::make_unique<VulkanPipelineManager>(context_, width_, height_);
    pipelineManager_->createRayTracingPipeline();
    pipelineManager_->createGraphicsPipeline(width_, height_);
    pipelineManager_->createComputePipeline();
    rtPipeline_ = pipelineManager_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();
    context_.rayTracingPipeline = rtPipeline_;
    if (!rtPipeline_ || !rtPipelineLayout_ || !context_.rayTracingPipeline) {
        LOG_ERROR_CAT("Renderer", "Failed to initialize ray-tracing pipeline: pipeline={:p}, layout={:p}, context.pipeline={:p}",
                      static_cast<void*>(rtPipeline_), static_cast<void*>(rtPipelineLayout_),
                      static_cast<void*>(context_.rayTracingPipeline));
        throw std::runtime_error("Failed to initialize ray-tracing pipeline");
    }
    LOG_DEBUG_CAT("Renderer", "Created ray-tracing, graphics, and compute pipelines; context_.rayTracingPipeline={:p}",
                  static_cast<void*>(context_.rayTracingPipeline));

    bufferManager_ = std::make_unique<VulkanBufferManager>(
        context_,
        std::span<const glm::vec3>(getVertices()),
        std::span<const uint32_t>(getIndices())
    );
    LOG_DEBUG_CAT("Renderer", "Created VulkanBufferManager");

    // Cache index count once after loading indices
    indexCount_ = static_cast<uint32_t>(getIndices().size());

    bufferManager_->createUniformBuffers(MAX_FRAMES_IN_FLIGHT);
    context_.uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    context_.uniformBufferMemories.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        context_.uniformBuffers[i] = bufferManager_->getUniformBuffer(i);
        context_.uniformBufferMemories[i] = bufferManager_->getUniformBufferMemory(i);
        if (!context_.uniformBuffers[i] || !context_.uniformBufferMemories[i]) {
            LOG_ERROR_CAT("Renderer", "Failed to get uniform buffer[{}] or memory: buffer={:p}, memory={:p}",
                          i, static_cast<void*>(context_.uniformBuffers[i]), static_cast<void*>(context_.uniformBufferMemories[i]));
            throw std::runtime_error("Failed to get uniform buffer or memory");
        }
    }
    if (context_.uniformBuffers.size() != MAX_FRAMES_IN_FLIGHT) {
        LOG_ERROR_CAT("Renderer", "Uniform buffer count mismatch: expected {}, got {}", MAX_FRAMES_IN_FLIGHT, context_.uniformBuffers.size());
        throw std::runtime_error("Uniform buffer count mismatch");
    }
    LOG_DEBUG_CAT("Renderer", "Created {} uniform buffers", context_.uniformBuffers.size());

    createFramebuffers();
    createCommandBuffers();
    createSyncObjects();
    LOG_DEBUG_CAT("Renderer", "Created framebuffers, command buffers, and sync objects");

    VulkanInitializer::createStorageImage(context_.device, context_.physicalDevice, context_.storageImage,
                                         context_.storageImageMemory, context_.storageImageView, width_, height_,
                                         context_.resourceManager);
    if (!context_.storageImage || !context_.storageImageMemory || !context_.storageImageView) {
        LOG_ERROR_CAT("Renderer", "Failed to create storage image: image={:p}, memory={:p}, view={:p}",
                      static_cast<void*>(context_.storageImage), static_cast<void*>(context_.storageImageMemory),
                      static_cast<void*>(context_.storageImageView));
        throw std::runtime_error("Failed to create storage image");
    }
    LOG_DEBUG_CAT("Renderer", "Created storage image: image={:p}, memory={:p}, view={:p}",
                  static_cast<void*>(context_.storageImage), static_cast<void*>(context_.storageImageMemory),
                  static_cast<void*>(context_.storageImageView));

    {
        VkCommandBuffer initCmd = VulkanInitializer::beginSingleTimeCommands(context_);
        VkImageMemoryBarrier initBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = context_.storageImage,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(initCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                             0, 0, nullptr, 0, nullptr, 1, &initBarrier);
        VulkanInitializer::endSingleTimeCommands(context_, initCmd);
        LOG_DEBUG_CAT("Renderer", "Initial transition: storageImage UNDEFINED -> GENERAL");
    }

    VulkanInitializer::createStorageImage(context_.device, context_.physicalDevice, denoiseImage_,
                                         denoiseImageMemory_, denoiseImageView_, width_, height_,
                                         context_.resourceManager);
    if (!denoiseImage_ || !denoiseImageMemory_ || !denoiseImageView_) {
        LOG_ERROR_CAT("Renderer", "Failed to create denoise image: image={:p}, memory={:p}, view={:p}",
                      static_cast<void*>(denoiseImage_), static_cast<void*>(denoiseImageMemory_),
                      static_cast<void*>(denoiseImageView_));
        throw std::runtime_error("Failed to create denoise image");
    }
    LOG_DEBUG_CAT("Renderer", "Created denoise image: image={:p}, memory={:p}, view={:p}",
                  static_cast<void*>(denoiseImage_), static_cast<void*>(denoiseImageMemory_),
                  static_cast<void*>(denoiseImageView_));

    {
        VkCommandBuffer initCmd = VulkanInitializer::beginSingleTimeCommands(context_);
        VkImageMemoryBarrier initBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = denoiseImage_,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(initCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &initBarrier);
        VulkanInitializer::endSingleTimeCommands(context_, initCmd);
        LOG_DEBUG_CAT("Renderer", "Initial transition: denoiseImage UNDEFINED -> GENERAL");
    }

    VkSamplerCreateInfo denoiseSamplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 0.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    if (vkCreateSampler(context_.device, &denoiseSamplerInfo, nullptr, &denoiseSampler_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to create denoise sampler");
        throw std::runtime_error("Failed to create denoise sampler");
    }
    LOG_DEBUG_CAT("Renderer", "Created denoise sampler: {:p}", static_cast<void*>(denoiseSampler_));

    createEnvironmentMap();
    LOG_DEBUG_CAT("Renderer", "Created environment map");

    constexpr uint32_t MATERIAL_COUNT = 128;
    constexpr uint32_t DIMENSION_COUNT = 1;
    VkDeviceSize materialBufferSize = sizeof(MaterialData) * MATERIAL_COUNT;
    VkDeviceSize dimensionBufferSize = sizeof(UE::DimensionData) * DIMENSION_COUNT;
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, materialBufferSize, dimensionBufferSize);
    LOG_DEBUG_CAT("Renderer", "Initialized material and dimension buffers");

    createAccelerationStructures();
    if (context_.topLevelAS == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Failed to create top-level acceleration structure");
        throw std::runtime_error("Failed to create TLAS");
    }
    LOG_DEBUG_CAT("Renderer", "Created TLAS: {:p}", static_cast<void*>(context_.topLevelAS));

    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 3}
    };
    VkDescriptorPoolCreateInfo poolInfo_desc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = MAX_FRAMES_IN_FLIGHT * 3,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes
    };
    if (vkCreateDescriptorPool(context_.device, &poolInfo_desc, nullptr, &context_.descriptorPool) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to create descriptor pool");
        throw std::runtime_error("Failed to create descriptor pool");
    }
    context_.resourceManager.addDescriptorPool(context_.descriptorPool);
    LOG_DEBUG_CAT("Renderer", "Created descriptor pool: {:p}", static_cast<void*>(context_.descriptorPool));

    std::vector<VkDescriptorSetLayout> rayTracingLayouts(MAX_FRAMES_IN_FLIGHT, context_.rayTracingDescriptorSetLayout);
    std::vector<VkDescriptorSetLayout> graphicsLayouts(MAX_FRAMES_IN_FLIGHT, context_.graphicsDescriptorSetLayout);
    std::vector<VkDescriptorSetLayout> computeLayouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout_);
    VkDescriptorSetAllocateInfo rayTracingAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = rayTracingLayouts.data()
    };
    VkDescriptorSetAllocateInfo graphicsAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = graphicsLayouts.data()
    };
    VkDescriptorSetAllocateInfo computeAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = computeLayouts.data()
    };
    std::vector<VkDescriptorSet> rayTracingSets(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSet> graphicsSets(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSet> computeSets(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(context_.device, &rayTracingAllocInfo, rayTracingSets.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to allocate ray-tracing descriptor sets");
        throw std::runtime_error("Failed to allocate ray-tracing descriptor sets");
    }
    if (vkAllocateDescriptorSets(context_.device, &graphicsAllocInfo, graphicsSets.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to allocate graphics descriptor sets");
        throw std::runtime_error("Failed to allocate graphics descriptor sets");
    }
    if (vkAllocateDescriptorSets(context_.device, &computeAllocInfo, computeSets.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to allocate compute descriptor sets");
        throw std::runtime_error("Failed to allocate compute descriptor sets");
    }
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames_[i].rayTracingDescriptorSet = rayTracingSets[i];
        frames_[i].graphicsDescriptorSet = graphicsSets[i];
        frames_[i].computeDescriptorSet = computeSets[i];
        LOG_DEBUG_CAT("Renderer", "Allocated descriptor sets for frame {}: rayTracing={:p}, graphics={:p}, compute={:p}",
                      i, static_cast<void*>(rayTracingSets[i]), static_cast<void*>(graphicsSets[i]), static_cast<void*>(computeSets[i]));
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        updateDescriptorSetForFrame(i, context_.topLevelAS);
        updateGraphicsDescriptorSet(i);
        updateComputeDescriptorSet(i);
    }
    LOG_INFO_CAT("Renderer", "VulkanRenderer initialized successfully");
}

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

void VulkanRenderer::cleanup() noexcept {
    try {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            auto& frame = frames_[i];
            if (frame.commandBuffer != VK_NULL_HANDLE) {
                frame.commandBuffer = VK_NULL_HANDLE;
            }
            if (frame.fence != VK_NULL_HANDLE) {
                vkDestroyFence(context_.device, frame.fence, nullptr);
                frame.fence = VK_NULL_HANDLE;
            }
            if (frame.renderFinishedSemaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(context_.device, frame.renderFinishedSemaphore, nullptr);
                frame.renderFinishedSemaphore = VK_NULL_HANDLE;
            }
            if (frame.imageAvailableSemaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(context_.device, frame.imageAvailableSemaphore, nullptr);
                frame.imageAvailableSemaphore = VK_NULL_HANDLE;
            }
        }
        frames_.clear();

        for (size_t i = 0; i < materialBuffers_.size(); ++i) {
            if (materialBuffers_[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(context_.device, materialBuffers_[i], nullptr);
                materialBuffers_[i] = VK_NULL_HANDLE;
            }
            if (materialBufferMemory_[i] != VK_NULL_HANDLE) {
                vkFreeMemory(context_.device, materialBufferMemory_[i], nullptr);
                materialBufferMemory_[i] = VK_NULL_HANDLE;
            }
        }
        materialBuffers_.clear();
        materialBufferMemory_.clear();

        for (size_t i = 0; i < dimensionBuffers_.size(); ++i) {
            if (dimensionBuffers_[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(context_.device, dimensionBuffers_[i], nullptr);
                dimensionBuffers_[i] = VK_NULL_HANDLE;
            }
            if (dimensionBufferMemory_[i] != VK_NULL_HANDLE) {
                vkFreeMemory(context_.device, dimensionBufferMemory_[i], nullptr);
                dimensionBufferMemory_[i] = VK_NULL_HANDLE;
            }
        }
        dimensionBuffers_.clear();
        dimensionBufferMemory_.clear();

        if (denoiseSampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(context_.device, denoiseSampler_, nullptr);
            denoiseSampler_ = VK_NULL_HANDLE;
        }
        if (denoiseImageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(context_.device, denoiseImageView_, nullptr);
            denoiseImageView_ = VK_NULL_HANDLE;
        }
        if (denoiseImageMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(context_.device, denoiseImageMemory_, nullptr);
            denoiseImageMemory_ = VK_NULL_HANDLE;
        }
        if (denoiseImage_ != VK_NULL_HANDLE) {
            vkDestroyImage(context_.device, denoiseImage_, nullptr);
            denoiseImage_ = VK_NULL_HANDLE;
        }

        if (envMapSampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(context_.device, envMapSampler_, nullptr);
            envMapSampler_ = VK_NULL_HANDLE;
        }
        if (envMapImageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(context_.device, envMapImageView_, nullptr);
            envMapImageView_ = VK_NULL_HANDLE;
        }
        if (envMapImageMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(context_.device, envMapImageMemory_, nullptr);
            envMapImageMemory_ = VK_NULL_HANDLE;
        }
        if (envMapImage_ != VK_NULL_HANDLE) {
            vkDestroyImage(context_.device, envMapImage_, nullptr);
            envMapImage_ = VK_NULL_HANDLE;
        }

        if (computeDescriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(context_.device, computeDescriptorSetLayout_, nullptr);
            computeDescriptorSetLayout_ = VK_NULL_HANDLE;
        }

        bufferManager_.reset();
        pipelineManager_.reset();
        swapchainManager_.reset();
        rtx_.reset();

        rtPipeline_ = VK_NULL_HANDLE;
        rtPipelineLayout_ = VK_NULL_HANDLE;
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Renderer", "Exception in cleanup: {}", e.what());
    }
}

void VulkanRenderer::createSwapchain(int width, int height) {
    LOG_DEBUG_CAT("Renderer", "Creating swapchain with width={}, height={}", width, height);
    if (!context_.surface) {
        LOG_ERROR_CAT("Renderer", "Surface is null");
        throw std::runtime_error("Null surface");
    }
    swapchainManager_ = std::make_unique<VulkanSwapchainManager>(context_, context_.surface);
    swapchainManager_->initializeSwapchain(width, height);
    context_.swapchain = swapchainManager_->getSwapchain();
    context_.swapchainImageFormat = swapchainManager_->getSwapchainImageFormat();
    context_.swapchainExtent = swapchainManager_->getSwapchainExtent();
    context_.swapchainImages = swapchainManager_->getSwapchainImages();
    context_.swapchainImageViews = swapchainManager_->getSwapchainImageViews();
    LOG_DEBUG_CAT("Renderer", "Swapchain created: extent={}x{}, imageCount={}, viewCount={}",
                  context_.swapchainExtent.width, context_.swapchainExtent.height,
                  context_.swapchainImages.size(), context_.swapchainImageViews.size());
    if (context_.swapchainImageViews.empty()) {
        LOG_ERROR_CAT("Renderer", "Swapchain image views are empty after creation");
        throw std::runtime_error("Failed to create swapchain image views");
    }
}

void VulkanRenderer::createEnvironmentMap() {
    LOG_DEBUG_CAT("Renderer", "Creating high-res environment map (4096x2048, 12 mips)");
    static std::vector<float> cachedEnvMapData;
    static bool envLoaded = false;
    int width, height, channels;
    std::vector<float> envMapData;
    stbi_set_flip_vertically_on_load(true);
    if (!envLoaded) {
        float* pixels = stbi_loadf("assets/textures/envmap.hdr", &width, &height, &channels, 4);
        if (!pixels || width != 4096 || height != 2048) {
            LOG_ERROR_CAT("Renderer", "Failed to load environment map: width={}, height={}, channels={}", width, height, channels);
            throw std::runtime_error("Failed to load environment map");
        }
        cachedEnvMapData.assign(pixels, pixels + (width * height * 4));
        stbi_image_free(pixels);
        envLoaded = true;
        LOG_DEBUG_CAT("Renderer", "Loaded and cached HDR environment map: {}x{}", width, height);
    }
    envMapData = cachedEnvMapData;

    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = {4096, 2048, 1},
        .mipLevels = 12,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    if (vkCreateImage(context_.device, &imageInfo, nullptr, &envMapImage_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to create environment map image");
        throw std::runtime_error("Failed to create environment map image");
    }
    context_.resourceManager.addImage(envMapImage_);
    LOG_DEBUG_CAT("Renderer", "Created environment map image: {:p}", static_cast<void*>(envMapImage_));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(context_.device, envMapImage_, &memRequirements);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = VulkanInitializer::findMemoryType(context_.physicalDevice, memRequirements.memoryTypeBits,
                                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    if (vkAllocateMemory(context_.device, &allocInfo, nullptr, &envMapImageMemory_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to allocate environment map memory");
        throw std::runtime_error("Failed to allocate environment map memory");
    }
    context_.resourceManager.addMemory(envMapImageMemory_);
    if (vkBindImageMemory(context_.device, envMapImage_, envMapImageMemory_, 0) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to bind environment map image memory");
        throw std::runtime_error("Failed to bind environment map image memory");
    }
    LOG_DEBUG_CAT("Renderer", "Allocated and bound environment map memory: {:p}", static_cast<void*>(envMapImageMemory_));

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = envMapImage_,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 12,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    if (vkCreateImageView(context_.device, &viewInfo, nullptr, &envMapImageView_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to create environment map image view");
        throw std::runtime_error("Failed to create environment map image view");
    }
    context_.resourceManager.addImageView(envMapImageView_);
    LOG_DEBUG_CAT("Renderer", "Created environment map image view: {:p}", static_cast<void*>(envMapImageView_));

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 12.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        .unnormalizedCoordinates = VK_FALSE
    };
    if (vkCreateSampler(context_.device, &samplerInfo, nullptr, &envMapSampler_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to create environment map sampler");
        throw std::runtime_error("Failed to create environment map sampler");
    }
    LOG_DEBUG_CAT("Renderer", "Created environment map sampler: {:p}, anisotropyEnable=true", static_cast<void*>(envMapSampler_));

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkDeviceSize bufferSize = envMapData.size() * sizeof(float);
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, bufferSize,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   stagingBuffer, stagingMemory, nullptr, context_.resourceManager);
    if (!stagingBuffer || !stagingMemory) {
        LOG_ERROR_CAT("Renderer", "Failed to create staging buffer for environment map: buffer={:p}, memory={:p}",
                      static_cast<void*>(stagingBuffer), static_cast<void*>(stagingMemory));
        throw std::runtime_error("Failed to create staging buffer for environment map");
    }
    LOG_DEBUG_CAT("Renderer", "Created staging buffer for environment map: buffer={:p}, memory={:p}",
                  static_cast<void*>(stagingBuffer), static_cast<void*>(stagingMemory));

    void* data;
    if (vkMapMemory(context_.device, stagingMemory, 0, bufferSize, 0, &data) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to map staging buffer memory for environment map");
        throw std::runtime_error("Failed to map staging buffer memory");
    }
    memcpy(data, envMapData.data(), bufferSize);
    vkUnmapMemory(context_.device, stagingMemory);
    LOG_DEBUG_CAT("Renderer", "Mapped and copied environment map data to staging buffer");

    VkCommandBuffer cmdBuffer = VulkanInitializer::beginSingleTimeCommands(context_);
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = envMapImage_,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy copyRegion{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {4096, 2048, 1}
    };
    vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, envMapImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    int32_t mipWidth = 4096;
    int32_t mipHeight = 2048;
    for (uint32_t i = 1; i < 12; ++i) {
        VkImageMemoryBarrier dstBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = envMapImage_,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = i,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &dstBarrier);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.subresourceRange.levelCount = 1;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkImageBlit blit{
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = i - 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .srcOffsets = {{0, 0, 0}, {mipWidth, mipHeight, 1}},
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = i,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .dstOffsets = {{0, 0, 0}, {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}}
        };
        vkCmdBlitImage(cmdBuffer, envMapImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       envMapImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        mipWidth = mipWidth > 1 ? mipWidth / 2 : 1;
        mipHeight = mipHeight > 1 ? mipHeight / 2 : 1;
    }

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 12;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    VulkanInitializer::endSingleTimeCommands(context_, cmdBuffer);
    LOG_DEBUG_CAT("Renderer", "Copied environment map data, generated 12 mip levels, and transitioned layout");

    context_.resourceManager.removeBuffer(stagingBuffer);
    context_.resourceManager.removeMemory(stagingMemory);
    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingMemory, nullptr);
    LOG_DEBUG_CAT("Renderer", "Cleaned up staging buffer and memory for environment map");
}

void VulkanRenderer::initializeAllBufferData(uint32_t maxFrames, VkDeviceSize materialBufferSize, VkDeviceSize dimensionBufferSize) {
    LOG_DEBUG_CAT("Renderer", "Initializing all buffer data for {} frames", maxFrames);
    if (maxFrames != MAX_FRAMES_IN_FLIGHT) {
        LOG_ERROR_CAT("Renderer", "Mismatch in maxFrames: {} (expected: {})", maxFrames, MAX_FRAMES_IN_FLIGHT);
        throw std::runtime_error("Invalid maxFrames value");
    }
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = nullptr,
        .properties = {}
    };
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    VkDeviceSize minAlignment = props2.properties.limits.minStorageBufferOffsetAlignment;
    materialBufferSize = (materialBufferSize + minAlignment - 1) & ~(minAlignment - 1);
    dimensionBufferSize = (dimensionBufferSize + minAlignment - 1) & ~(minAlignment - 1);
    LOG_DEBUG_CAT("Renderer", "Aligned buffer sizes: materialBufferSize={}, dimensionBufferSize={}, alignment={}",
                  materialBufferSize, dimensionBufferSize, minAlignment);

    materialBuffers_.resize(maxFrames);
    materialBufferMemory_.resize(maxFrames);
    dimensionBuffers_.resize(maxFrames);
    dimensionBufferMemory_.resize(maxFrames);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0
    };

    for (uint32_t i = 0; i < maxFrames; ++i) {
        VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, materialBufferSize,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       materialBuffers_[i], materialBufferMemory_[i], &allocFlagsInfo, context_.resourceManager);
        if (!materialBuffers_[i] || !materialBufferMemory_[i]) {
            LOG_ERROR_CAT("Renderer", "Failed to create material buffer[{}]: buffer={:p}, memory={:p}",
                          i, static_cast<void*>(materialBuffers_[i]), static_cast<void*>(materialBufferMemory_[i]));
            throw std::runtime_error("Failed to create material buffer");
        }
        LOG_DEBUG_CAT("Renderer", "Created material buffer[{}]: buffer={:p}, memory={:p}",
                      i, static_cast<void*>(materialBuffers_[i]), static_cast<void*>(materialBufferMemory_[i]));

        VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, dimensionBufferSize,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       dimensionBuffers_[i], dimensionBufferMemory_[i], &allocFlagsInfo, context_.resourceManager);
        if (!dimensionBuffers_[i] || !dimensionBufferMemory_[i]) {
            LOG_ERROR_CAT("Renderer", "Failed to create dimension buffer[{}]: buffer={:p}, memory={:p}",
                          i, static_cast<void*>(dimensionBuffers_[i]), static_cast<void*>(dimensionBufferMemory_[i]));
            throw std::runtime_error("Failed to create dimension buffer");
        }
        LOG_DEBUG_CAT("Renderer", "Created dimension buffer[{}]: buffer={:p}, memory={:p}",
                      i, static_cast<void*>(dimensionBuffers_[i]), static_cast<void*>(dimensionBufferMemory_[i]));

        initializeBufferData(i, materialBufferSize, dimensionBufferSize);
    }

    LOG_DEBUG_CAT("Renderer", "Material and dimension buffers created for {} frames", maxFrames);
}

void VulkanRenderer::initializeBufferData(uint32_t frameIndex, VkDeviceSize materialSize, VkDeviceSize dimensionSize) {
    LOG_DEBUG_CAT("Renderer", "Initializing buffer data for frame {}", frameIndex);
    if (frames_.empty()) {
        LOG_ERROR_CAT("Renderer", "Frames vector is empty; cannot initialize buffer data");
        throw std::runtime_error("Frames vector is empty");
    }
    if (frameIndex >= frames_.size()) {
        LOG_ERROR_CAT("Renderer", "Invalid frame index: {} (max: {})", frameIndex, frames_.size() - 1);
        throw std::out_of_range("Invalid frame index");
    }
    if (!context_.device) {
        LOG_ERROR_CAT("Renderer", "Invalid device handle in initializeBufferData");
        throw std::runtime_error("Invalid device handle");
    }

    vkDeviceWaitIdle(context_.device);
    LOG_DEBUG_CAT("Renderer", "Device idle for buffer initialization");

    constexpr uint32_t MATERIAL_COUNT = 128;
    constexpr uint32_t DIMENSION_COUNT = 1;
    VkDeviceSize requiredMaterialSize = sizeof(MaterialData) * MATERIAL_COUNT;
    VkDeviceSize requiredDimensionSize = sizeof(UE::DimensionData) * DIMENSION_COUNT;
    if (materialSize < requiredMaterialSize || dimensionSize < requiredDimensionSize) {
        LOG_ERROR_CAT("Renderer", "Insufficient buffer sizes: materialSize={} (required={}), dimensionSize={} (required={})",
                      materialSize, requiredMaterialSize, dimensionSize, requiredDimensionSize);
        throw std::runtime_error("Insufficient buffer sizes for material or dimension data");
    }

    VkMemoryRequirements materialMemReq;
    vkGetBufferMemoryRequirements(context_.device, materialBuffers_[frameIndex], &materialMemReq);
    if (materialMemReq.size < requiredMaterialSize) {
        LOG_ERROR_CAT("Renderer", "Material buffer[{}] size {} too small for {} materials (required: {})",
                      frameIndex, materialMemReq.size, MATERIAL_COUNT, requiredMaterialSize);
        throw std::runtime_error("Material buffer size too small");
    }
    VkMemoryRequirements dimensionMemReq;
    vkGetBufferMemoryRequirements(context_.device, dimensionBuffers_[frameIndex], &dimensionMemReq);
    if (dimensionMemReq.size < requiredDimensionSize) {
        LOG_ERROR_CAT("Renderer", "Dimension buffer[{}] size {} too small for {} dimensions (required: {})",
                      frameIndex, dimensionMemReq.size, DIMENSION_COUNT, requiredDimensionSize);
        throw std::runtime_error("Dimension buffer size too small");
    }

    MaterialData defaultMaterial{
        .diffuse = glm::vec4(1.0f, 0.0f, 0.0f, 0.5f),
        .specular = 0.5f,
        .roughness = 0.5f,
        .metallic = 0.0f,
        .emission = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)
    };
    std::vector<MaterialData> materials(MATERIAL_COUNT, defaultMaterial);
    for (uint32_t j = 0; j < MATERIAL_COUNT; ++j) {
        materials[j].metallic = (j % 2 == 0) ? 1.0f : 0.0f;
        materials[j].roughness = static_cast<float>(j) / static_cast<float>(MATERIAL_COUNT - 1);
        materials[j].diffuse = glm::vec4(glm::normalize(glm::vec3(j * 0.1f, (j * 0.3f) + 0.2f, (j * 0.5f) + 0.1f)), 1.0f);
        materials[j].specular = 0.5f + (j % 3) * 0.166f;
    }
    VkBuffer materialStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory materialStagingBufferMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, requiredMaterialSize,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   materialStagingBuffer, materialStagingBufferMemory, nullptr, context_.resourceManager);
    if (!materialStagingBuffer || !materialStagingBufferMemory) {
        LOG_ERROR_CAT("Renderer", "Failed to create material staging buffer or memory: buffer={:p}, memory={:p}",
                      static_cast<void*>(materialStagingBuffer), static_cast<void*>(materialStagingBufferMemory));
        throw std::runtime_error("Failed to create material staging buffer");
    }
    LOG_DEBUG_CAT("Renderer", "Created material staging buffer: buffer={:p}, memory={:p}",
                  static_cast<void*>(materialStagingBuffer), static_cast<void*>(materialStagingBufferMemory));

    void* data;
    if (vkMapMemory(context_.device, materialStagingBufferMemory, 0, requiredMaterialSize, 0, &data) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to map material staging buffer memory");
        throw std::runtime_error("Failed to map material staging buffer memory");
    }
    memcpy(data, materials.data(), static_cast<size_t>(requiredMaterialSize));
    vkUnmapMemory(context_.device, materialStagingBufferMemory);
    LOG_DEBUG_CAT("Renderer", "Mapped and copied material data to staging buffer");

    VulkanInitializer::copyBuffer(context_.device, context_.commandPool, context_.graphicsQueue,
                                 materialStagingBuffer, materialBuffers_[frameIndex], requiredMaterialSize);
    LOG_DEBUG_CAT("Renderer", "Copied material data to buffer[{}]: {:p}", frameIndex, static_cast<void*>(materialBuffers_[frameIndex]));

    context_.resourceManager.removeBuffer(materialStagingBuffer);
    context_.resourceManager.removeMemory(materialStagingBufferMemory);
    vkDestroyBuffer(context_.device, materialStagingBuffer, nullptr);
    vkFreeMemory(context_.device, materialStagingBufferMemory, nullptr);
    LOG_DEBUG_CAT("Renderer", "Cleaned up material staging buffer and memory");

    UE::DimensionData defaultDimension{
        .dimension = 0,
        .scale = 1.0f,
        .position = glm::vec3(0.0f),
        .value = 1.0f,
        .nurbEnergy = 1.0f,
        .nurbMatter = 0.032774f,
        .potential = 1.0f,
        .observable = 1.0f,
        .spinEnergy = 0.0f,
        .momentumEnergy = 0.0f,
        .fieldEnergy = 0.0f,
        .GodWaveEnergy = 0.0f
    };
    std::vector<UE::DimensionData> dimensions(DIMENSION_COUNT, defaultDimension);
    VkBuffer dimensionStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory dimensionStagingBufferMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, requiredDimensionSize,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   dimensionStagingBuffer, dimensionStagingBufferMemory, nullptr, context_.resourceManager);
    if (!dimensionStagingBuffer || !dimensionStagingBufferMemory) {
        LOG_ERROR_CAT("Renderer", "Failed to create dimension staging buffer or memory: buffer={:p}, memory={:p}",
                      static_cast<void*>(dimensionStagingBuffer), static_cast<void*>(dimensionStagingBufferMemory));
        throw std::runtime_error("Failed to create dimension staging buffer");
    }
    LOG_DEBUG_CAT("Renderer", "Created dimension staging buffer: buffer={:p}, memory={:p}",
                  static_cast<void*>(dimensionStagingBuffer), static_cast<void*>(dimensionStagingBufferMemory));

    if (vkMapMemory(context_.device, dimensionStagingBufferMemory, 0, requiredDimensionSize, 0, &data) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to map dimension staging buffer memory");
        throw std::runtime_error("Failed to map dimension staging buffer memory");
    }
    memcpy(data, dimensions.data(), static_cast<size_t>(requiredDimensionSize));
    vkUnmapMemory(context_.device, dimensionStagingBufferMemory);
    LOG_DEBUG_CAT("Renderer", "Mapped and copied dimension data to staging buffer");

    VulkanInitializer::copyBuffer(context_.device, context_.commandPool, context_.graphicsQueue,
                                 dimensionStagingBuffer, dimensionBuffers_[frameIndex], requiredDimensionSize);
    LOG_DEBUG_CAT("Renderer", "Copied dimension data to buffer[{}]: {:p}", frameIndex, static_cast<void*>(dimensionBuffers_[frameIndex]));

    context_.resourceManager.removeBuffer(dimensionStagingBuffer);
    context_.resourceManager.removeMemory(dimensionStagingBufferMemory);
    vkDestroyBuffer(context_.device, dimensionStagingBuffer, nullptr);
    vkFreeMemory(context_.device, dimensionStagingBufferMemory, nullptr);
    LOG_DEBUG_CAT("Renderer", "Cleaned up dimension staging buffer and memory");

    LOG_INFO_CAT("Renderer", "Initialized material buffer ({} materials, size={}) and dimension buffer ({} dimensions, size={}) for frame {}",
                 MATERIAL_COUNT, materialSize, DIMENSION_COUNT, dimensionSize, frameIndex);
}

void VulkanRenderer::updateDescriptorSetForFrame(uint32_t frameIndex, VkAccelerationStructureKHR tlas) {
    LOG_DEBUG_CAT("Renderer", "Updating descriptor set for frame {} with TLAS: {:p}", frameIndex, static_cast<void*>(tlas));
    if (frameIndex >= frames_.size()) {
        LOG_ERROR_CAT("Renderer", "Invalid frame index: {} (max: {})", frameIndex, frames_.size() - 1);
        throw std::out_of_range("Invalid frame index");
    }
    if (tlas == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Invalid TLAS for frame {}: {:p}", frameIndex, static_cast<void*>(tlas));
        throw std::runtime_error("Invalid TLAS");
    }

    VkDescriptorSet descriptorSet = frames_[frameIndex].rayTracingDescriptorSet;
    if (descriptorSet == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Null descriptor set for frame {}", frameIndex);
        throw std::runtime_error("Null descriptor set");
    }

    VkWriteDescriptorSetAccelerationStructureKHR accelDescriptor{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkDescriptorImageInfo storageImageInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = context_.storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorImageInfo envMapInfo{
        .sampler = envMapSampler_,
        .imageView = envMapImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorBufferInfo uniformBufferInfo{
        .buffer = context_.uniformBuffers[frameIndex],
        .offset = 0,
        .range = sizeof(UE::UniformBufferObject)
    };

    VkDescriptorBufferInfo materialBufferInfo{
        .buffer = materialBuffers_[frameIndex],
        .offset = 0,
        .range = sizeof(MaterialData) * 128
    };

    VkDescriptorBufferInfo dimensionBufferInfo{
        .buffer = dimensionBuffers_[frameIndex],
        .offset = 0,
        .range = sizeof(UE::DimensionData) * 1
    };

    std::vector<VkWriteDescriptorSet> descriptorWrites = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &accelDescriptor,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::StorageImage),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &storageImageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::CameraUBO),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &uniformBufferInfo,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &materialBufferInfo,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &dimensionBufferInfo,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::EnvMap),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &envMapInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        }
    };

    vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    LOG_DEBUG_CAT("Renderer", "Updated descriptor set {:p} for frame {}", static_cast<void*>(descriptorSet), frameIndex);
}

void VulkanRenderer::updateGraphicsDescriptorSet(uint32_t frameIndex) {
    LOG_DEBUG_CAT("Renderer", "Updating graphics descriptor set for frame {}", frameIndex);
    if (frameIndex >= frames_.size()) {
        LOG_ERROR_CAT("Renderer", "Invalid frame index for graphics update: {}", frameIndex);
        return;
    }
    VkDescriptorSet descSet = frames_[frameIndex].graphicsDescriptorSet;
    if (descSet == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Null graphics descriptor set for frame {}", frameIndex);
        return;
    }
    if (!denoiseImageView_ || !denoiseSampler_) {
        LOG_ERROR_CAT("Renderer", "Invalid denoise resources: imageView={:p}, sampler={:p}",
                      static_cast<void*>(denoiseImageView_), static_cast<void*>(denoiseSampler_));
        return;
    }

    VkDescriptorImageInfo imageInfo{
        .sampler = denoiseSampler_,
        .imageView = denoiseImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
    LOG_DEBUG_CAT("Renderer", "Updated graphics descriptor set {:p} for frame {} with denoise image", static_cast<void*>(descSet), frameIndex);
}

void VulkanRenderer::updateComputeDescriptorSet(uint32_t frameIndex) {
    LOG_DEBUG_CAT("Renderer", "Updating compute descriptor set for frame {}", frameIndex);
    if (frameIndex >= frames_.size()) {
        LOG_ERROR_CAT("Renderer", "Invalid frame index for compute update: {}", frameIndex);
        return;
    }
    VkDescriptorSet descSet = frames_[frameIndex].computeDescriptorSet;
    if (descSet == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Null compute descriptor set for frame {}", frameIndex);
        return;
    }
    if (!context_.storageImageView || !denoiseImageView_) {
        LOG_ERROR_CAT("Renderer", "Invalid image views: storageImageView={:p}, denoiseImageView={:p}",
                      static_cast<void*>(context_.storageImageView), static_cast<void*>(denoiseImageView_));
        return;
    }

    VkDescriptorImageInfo inputInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = context_.storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorImageInfo outputInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = denoiseImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &inputInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descSet,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &outputInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        }
    };

    vkUpdateDescriptorSets(context_.device, 2, writes, 0, nullptr);
    LOG_DEBUG_CAT("Renderer", "Updated compute descriptor set {:p} for frame {} with input/output images", static_cast<void*>(descSet), frameIndex);
}

void VulkanRenderer::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    LOG_DEBUG_CAT("Renderer", "Updating TLAS descriptor for all frames");
    if (!tlas) {
        LOG_ERROR_CAT("Renderer", "Invalid TLAS handle: {:p}", static_cast<void*>(tlas));
        throw std::runtime_error("Invalid TLAS handle");
    }
    for (uint32_t i = 0; i < frames_.size(); ++i) {
        if (!frames_[i].rayTracingDescriptorSet) {
            LOG_ERROR_CAT("Renderer", "Invalid ray tracing descriptor set for frame {}", i);
            throw std::runtime_error("Invalid ray tracing descriptor set");
        }
        VkWriteDescriptorSetAccelerationStructureKHR tlasInfo{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .pNext = nullptr,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &tlas
        };
        VkWriteDescriptorSet tlasWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &tlasInfo,
            .dstSet = frames_[i].rayTracingDescriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };
        vkUpdateDescriptorSets(context_.device, 1, &tlasWrite, 0, nullptr);
        LOG_DEBUG_CAT("Renderer", "Updated TLAS descriptor for frame {}: tlas={:p}", i, static_cast<void*>(tlas));
    }
}

void VulkanRenderer::renderFrame(const Camera& camera) {
    if (!context_.device || frames_.empty() || currentFrame_ >= frames_.size()) {
        LOG_ERROR_CAT("Renderer", "Invalid state: device={:p}, frames.size={}, currentFrame={}",
                      static_cast<void*>(context_.device), frames_.size(), currentFrame_);
        throw std::runtime_error("Invalid render state");
    }

    vkWaitForFences(context_.device, 1, &frames_[currentFrame_].fence, VK_TRUE, UINT64_MAX);
    vkResetFences(context_.device, 1, &frames_[currentFrame_].fence);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_.device, context_.swapchain, UINT64_MAX,
                                            frames_[currentFrame_].imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        handleResize(width_, height_);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR_CAT("Renderer", "Failed to acquire swapchain image: result={}", static_cast<int>(result));
        throw std::runtime_error("Failed to acquire swapchain image");
    }
    if (imageIndex >= context_.framebuffers.size()) {
        LOG_ERROR_CAT("Renderer", "Invalid framebuffer index: {} (size={})", imageIndex, context_.framebuffers.size());
        throw std::runtime_error("Invalid framebuffer index");
    }

    if (vkResetCommandBuffer(frames_[currentFrame_].commandBuffer, 0) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to reset command buffer for frame {}", currentFrame_);
        throw std::runtime_error("Failed to reset command buffer");
    }

    // Update uniform buffer
    UE::UniformBufferObject ubo{
        .model = glm::mat4(1.0f), // Default identity matrix
        .view = camera.getViewMatrix(),
        .proj = camera.getProjectionMatrix(),
        .mode = 0 // Default mode, as AMOURANTH is removed
    };
    VkDeviceMemory uniformBufferMemory = bufferManager_->getUniformBufferMemory(currentFrame_);
    if (!uniformBufferMemory) {
        LOG_ERROR_CAT("Renderer", "Invalid uniform buffer memory for frame {}", currentFrame_);
        throw std::runtime_error("Invalid uniform buffer memory");
    }
    void* data;
    if (vkMapMemory(context_.device, uniformBufferMemory, 0, sizeof(UE::UniformBufferObject), 0, &data) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to map uniform buffer memory");
        throw std::runtime_error("Failed to map uniform buffer memory");
    }
    memcpy(data, &ubo, sizeof(UE::UniformBufferObject));
    vkUnmapMemory(context_.device, uniformBufferMemory);

    // Common push constants
    MaterialData::PushConstants pushConstants{
        .clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
        .cameraPosition = camera.getPosition(),
        .lightDirection = glm::vec3(2.0f, 2.0f, 2.0f),
        .lightIntensity = 5.0f,
        .samplesPerPixel = 1u,
        .maxDepth = 5u,
        .maxBounces = 3u,
        .russianRoulette = 0.8f,
        .resolution = {context_.swapchainExtent.width, context_.swapchainExtent.height}
    };

    // Default rendering path (ray-tracing + graphics)
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(frames_[currentFrame_].commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to begin command buffer");
        throw std::runtime_error("Failed to begin command buffer");
    }

    recordRayTracingCommands(frames_[currentFrame_].commandBuffer, context_.swapchainExtent, context_.storageImage,
                            context_.storageImageView, pushConstants, context_.topLevelAS);
    denoiseImage(frames_[currentFrame_].commandBuffer, context_.storageImage, context_.storageImageView,
                 denoiseImage_, denoiseImageView_);

    VkImageMemoryBarrier graphicsBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = denoiseImage_,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(frames_[currentFrame_].commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &graphicsBarrier);

    VkClearValue clearValue = {{{0.2f, 0.2f, 0.3f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = pipelineManager_->getRenderPass(),
        .framebuffer = context_.framebuffers[imageIndex],
        .renderArea = {{0, 0}, context_.swapchainExtent},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    if (!renderPassInfo.renderPass || !renderPassInfo.framebuffer) {
        LOG_ERROR_CAT("Renderer", "Invalid render pass or framebuffer: renderPass={:p}, framebuffer={:p}",
                      static_cast<void*>(renderPassInfo.renderPass), static_cast<void*>(renderPassInfo.framebuffer));
        throw std::runtime_error("Invalid render pass or framebuffer");
    }
    vkCmdBeginRenderPass(frames_[currentFrame_].commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkPipeline graphicsPipeline = pipelineManager_->getGraphicsPipeline();
    VkPipelineLayout graphicsPipelineLayout = pipelineManager_->getGraphicsPipelineLayout();
    if (!graphicsPipeline || !graphicsPipelineLayout) {
        LOG_ERROR_CAT("Renderer", "Invalid graphics pipeline state: pipeline={:p}, layout={:p}",
                      static_cast<void*>(graphicsPipeline), static_cast<void*>(graphicsPipelineLayout));
        throw std::runtime_error("Invalid graphics pipeline state");
    }
    vkCmdBindPipeline(frames_[currentFrame_].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    if (frames_[currentFrame_].graphicsDescriptorSet == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Null graphics descriptor set for frame {}", currentFrame_);
        throw std::runtime_error("Null graphics descriptor set");
    }
    vkCmdBindDescriptorSets(frames_[currentFrame_].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            graphicsPipelineLayout, 0, 1, &frames_[currentFrame_].graphicsDescriptorSet, 0, nullptr);

    vkCmdPushConstants(frames_[currentFrame_].commandBuffer, graphicsPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    VkBuffer vertexBuffer = bufferManager_->getVertexBuffer();
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(frames_[currentFrame_].commandBuffer, 0, 1, &vertexBuffer, offsets);
    VkBuffer indexBuffer = bufferManager_->getIndexBuffer();
    vkCmdBindIndexBuffer(frames_[currentFrame_].commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(frames_[currentFrame_].commandBuffer, indexCount_, 1, 0, 0, 0);
    vkCmdEndRenderPass(frames_[currentFrame_].commandBuffer);

    VkImageMemoryBarrier postGraphicsBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = denoiseImage_,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(frames_[currentFrame_].commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &postGraphicsBarrier);

    if (vkEndCommandBuffer(frames_[currentFrame_].commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to end command buffer");
        throw std::runtime_error("Failed to end command buffer");
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames_[currentFrame_].imageAvailableSemaphore,
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &frames_[currentFrame_].commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frames_[currentFrame_].renderFinishedSemaphore
    };
    if (vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, frames_[currentFrame_].fence) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to submit queue");
        throw std::runtime_error("Failed to submit queue");
    }

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames_[currentFrame_].renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &context_.swapchain,
        .pImageIndices = &imageIndex,
        .pResults = nullptr
    };
    VkResult presentResult = vkQueuePresentKHR(context_.presentQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        handleResize(width_, height_);
    } else if (presentResult != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to present queue: VkResult={}", static_cast<int>(presentResult));
        throw std::runtime_error("Failed to present queue");
    }

    frameCount_++;
    if (FPS_COUNTER) {
        framesSinceLastLog_++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastLogTime_).count();
        if (elapsed >= 1) {
            double fps = static_cast<double>(framesSinceLastLog_) / static_cast<double>(elapsed);
            LOG_WARNING_CAT("FPS", "Total frame count: {}, Average FPS over {} seconds: {:.2f}", frameCount_, elapsed, fps);
            lastLogTime_ = now;
            framesSinceLastLog_ = 0;
        }
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::recordRayTracingCommands(VkCommandBuffer commandBuffer, VkExtent2D extent, VkImage outputImage,
                                             VkImageView outputImageView, const MaterialData::PushConstants& pushConstants,
                                             VkAccelerationStructureKHR tlas) {
    if (!commandBuffer || !outputImage || !outputImageView || !tlas || !rtPipeline_ || !rtPipelineLayout_) {
        LOG_ERROR_CAT("Renderer", "Invalid parameters: cmd={:p}, outputImage={:p}, outputView={:p}, tlas={:p}, pipeline={:p}, layout={:p}",
                      static_cast<void*>(commandBuffer), static_cast<void*>(outputImage),
                      static_cast<void*>(outputImageView), static_cast<void*>(tlas),
                      static_cast<void*>(rtPipeline_), static_cast<void*>(rtPipelineLayout_));
        throw std::runtime_error("Invalid ray tracing parameters");
    }

    VkImageMemoryBarrier outputBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = outputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 0, nullptr, 0, nullptr, 1, &outputBarrier);

    const ShaderBindingTable& sbt = pipelineManager_->getShaderBindingTable();
    if (sbt.raygen.deviceAddress == 0 || sbt.miss.deviceAddress == 0 || sbt.hit.deviceAddress == 0) {
        LOG_ERROR_CAT("Renderer", "Invalid shader binding table: raygen={:x}, miss={:x}, hit={:x}",
                      sbt.raygen.deviceAddress, sbt.miss.deviceAddress, sbt.hit.deviceAddress);
        throw std::runtime_error("Invalid shader binding table");
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipelineLayout_, 0, 1, &frames_[currentFrame_].rayTracingDescriptorSet, 0, nullptr);
    vkCmdPushConstants(commandBuffer, rtPipelineLayout_, 
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    auto vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCmdTraceRaysKHR"));
    if (!vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("Renderer", "Failed to load vkCmdTraceRaysKHR function pointer");
        throw std::runtime_error("vkCmdTraceRaysKHR not loaded");
    }

    vkCmdTraceRaysKHR(commandBuffer, &sbt.raygen, &sbt.miss, &sbt.hit, &sbt.callable,
                      extent.width, extent.height, 1);
}

void VulkanRenderer::denoiseImage(VkCommandBuffer commandBuffer, VkImage inputImage, VkImageView inputImageView,
                                 VkImage outputImage, VkImageView outputImageView) {
    if (!commandBuffer || !inputImage || !inputImageView || !outputImage || !outputImageView) {
        LOG_ERROR_CAT("Renderer", "Invalid parameters: cmd={:p}, inputImage={:p}, inputView={:p}, outputImage={:p}, outputView={:p}",
                      static_cast<void*>(commandBuffer), static_cast<void*>(inputImage),
                      static_cast<void*>(inputImageView), static_cast<void*>(outputImage),
                      static_cast<void*>(outputImageView));
        throw std::runtime_error("Invalid denoise parameters");
    }

    VkImageMemoryBarrier inputBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = inputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &inputBarrier);

    VkImageMemoryBarrier outputBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = outputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &outputBarrier);

    VkPipeline computePipeline = pipelineManager_->getComputePipeline();
    VkPipelineLayout computePipelineLayout = pipelineManager_->getComputePipelineLayout();
    if (!computePipeline || !computePipelineLayout || !frames_[currentFrame_].computeDescriptorSet) {
        LOG_ERROR_CAT("Renderer", "Invalid compute pipeline state: pipeline={:p}, layout={:p}, descriptorSet={:p}",
                      static_cast<void*>(computePipeline), static_cast<void*>(computePipelineLayout),
                      static_cast<void*>(frames_[currentFrame_].computeDescriptorSet));
        throw std::runtime_error("Invalid compute pipeline state");
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout,
                            0, 1, &frames_[currentFrame_].computeDescriptorSet, 0, nullptr);

    MaterialData::PushConstants pushConstants{
        .clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
        .cameraPosition = glm::vec3(0.0f, 0.0f, 0.0f),
        .lightDirection = glm::vec3(2.0f, 2.0f, 2.0f),
        .lightIntensity = 5.0f,
        .samplesPerPixel = 1u,
        .maxDepth = 5u,
        .maxBounces = 3u,
        .russianRoulette = 0.8f,
        .resolution = {context_.swapchainExtent.width, context_.swapchainExtent.height}
    };
    vkCmdPushConstants(commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = nullptr,
        .properties = {}
    };
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    uint32_t maxWorkgroupSize = props2.properties.limits.maxComputeWorkGroupInvocations;
    uint32_t groupSizeX = 16;
    uint32_t groupSizeY = 16;
    uint32_t groupCountX = (context_.swapchainExtent.width + groupSizeX - 1) / groupSizeX;
    uint32_t groupCountY = (context_.swapchainExtent.height + groupSizeY - 1) / groupSizeY;
    if (groupSizeX * groupSizeY > maxWorkgroupSize) {
        groupSizeX = std::max(8u, static_cast<uint32_t>(std::sqrt(maxWorkgroupSize)));
        groupSizeY = groupSizeX;
        groupCountX = (context_.swapchainExtent.width + groupSizeX - 1) / groupSizeX;
        groupCountY = (context_.swapchainExtent.height + groupSizeY - 1) / groupSizeY;
    }
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
}

void VulkanRenderer::createFramebuffers() {
    LOG_DEBUG_CAT("Renderer", "Creating {} framebuffers", swapchainManager_->getSwapchainImageViews().size());
    context_.framebuffers.resize(swapchainManager_->getSwapchainImageViews().size());
    if (context_.framebuffers.empty()) {
        LOG_WARNING_CAT("Renderer", "No framebuffers created due to empty swapchain image views");
        return;
    }
    for (size_t i = 0; i < context_.framebuffers.size(); ++i) {
        VkImageView attachments[] = {swapchainManager_->getSwapchainImageViews()[i]};
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = pipelineManager_->getRenderPass(),
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = swapchainManager_->getSwapchainExtent().width,
            .height = swapchainManager_->getSwapchainExtent().height,
            .layers = 1
        };
        if (!framebufferInfo.renderPass || !attachments[0]) {
            LOG_ERROR_CAT("Renderer", "Invalid framebuffer parameters: renderPass={:p}, attachment={:p}",
                          static_cast<void*>(framebufferInfo.renderPass), static_cast<void*>(attachments[0]));
            throw std::runtime_error("Invalid framebuffer parameters");
        }
        if (vkCreateFramebuffer(context_.device, &framebufferInfo, nullptr, &context_.framebuffers[i]) != VK_SUCCESS) {
            LOG_ERROR_CAT("Renderer", "Failed to create framebuffer[{}]", i);
            throw std::runtime_error("Failed to create framebuffer");
        }
        LOG_DEBUG_CAT("Renderer", "Created framebuffer[{}]: {:p}", i, static_cast<void*>(context_.framebuffers[i]));
    }
}

void VulkanRenderer::createCommandBuffers() {
    LOG_DEBUG_CAT("Renderer", "Creating command buffers");
    if (context_.commandPool == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Command pool is null");
        throw std::runtime_error("Null command pool");
    }
    if (!swapchainManager_) {
        LOG_ERROR_CAT("Renderer", "Swapchain manager is null");
        throw std::runtime_error("Null swapchain manager");
    }
    if (context_.device == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Device is null");
        throw std::runtime_error("Null device");
    }
    LOG_DEBUG_CAT("Renderer", "swapchainManager_ = {:p}", static_cast<void*>(swapchainManager_.get()));
    LOG_DEBUG_CAT("Renderer", "Creating command buffers for {} swapchain images", swapchainManager_->getSwapchainImages().size());
    auto& imageViews = swapchainManager_->getSwapchainImageViews();
    LOG_DEBUG_CAT("Renderer", "Using {} image views for command buffers", imageViews.size());
    size_t imageViewCount = imageViews.size();
    if (imageViewCount == 0) {
        LOG_ERROR_CAT("Renderer", "No swapchain image views available. Check swapchain initialization.");
        throw std::runtime_error("Empty swapchain image views");
    }

    context_.commandBuffers.resize(imageViewCount);
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(context_.commandBuffers.size())
    };
    VkResult result = vkAllocateCommandBuffers(context_.device, &allocInfo, context_.commandBuffers.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to allocate command buffers: VkResult={}", static_cast<int>(result));
        throw std::runtime_error("Failed to allocate command buffers");
    }
    for (size_t i = 0; i < context_.commandBuffers.size(); ++i) {
        if (context_.commandBuffers[i] == VK_NULL_HANDLE) {
            LOG_ERROR_CAT("Renderer", "Command buffer[{}] is null after allocation", i);
            throw std::runtime_error("Null command buffer allocated");
        }
        LOG_DEBUG_CAT("Renderer", "Allocated command buffer[{}]: {:p}", i, static_cast<void*>(context_.commandBuffers[i]));
    }
    if (context_.commandBuffers.empty()) {
        LOG_ERROR_CAT("Renderer", "No command buffers allocated");
        throw std::runtime_error("Empty command buffers");
    }
    for (size_t i = 0; i < frames_.size(); ++i) {
        frames_[i].commandBuffer = context_.commandBuffers[i % context_.commandBuffers.size()];
        LOG_DEBUG_CAT("Renderer", "Assigned command buffer[{}] to frame {}: {:p}", i % context_.commandBuffers.size(), i,
                      static_cast<void*>(frames_[i].commandBuffer));
    }
    LOG_INFO_CAT("Renderer", "Allocated {} command buffers", context_.commandBuffers.size());
}

void VulkanRenderer::createSyncObjects() {
    LOG_DEBUG_CAT("Renderer", "Creating sync objects");
    context_.imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    context_.renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    context_.inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };
        VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        if (vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &context_.imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &context_.renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(context_.device, &fenceInfo, nullptr, &context_.inFlightFences[i]) != VK_SUCCESS) {
            LOG_ERROR_CAT("Renderer", "Failed to create sync objects for frame {}", i);
            throw std::runtime_error("Failed to create sync objects");
        }
        frames_[i].imageAvailableSemaphore = context_.imageAvailableSemaphores[i];
        frames_[i].renderFinishedSemaphore = context_.renderFinishedSemaphores[i];
        frames_[i].fence = context_.inFlightFences[i];
        LOG_DEBUG_CAT("Renderer", "Created sync objects for frame {}: fence={:p}, imageSemaphore={:p}, renderSemaphore={:p}",
                      i, static_cast<void*>(context_.inFlightFences[i]),
                      static_cast<void*>(context_.imageAvailableSemaphores[i]),
                      static_cast<void*>(context_.renderFinishedSemaphores[i]));
    }
    LOG_INFO_CAT("Renderer", "Created sync objects for {} frames", MAX_FRAMES_IN_FLIGHT);
}

void VulkanRenderer::handleResize(int width, int height) {
    LOG_DEBUG_CAT("Renderer", "Handling resize to {}x{}", width, height);
    if (width <= 0 || height <= 0) {
        LOG_WARNING_CAT("Renderer", "Invalid resize dimensions: {}x{}", width, height);
        return;
    }
    vkDeviceWaitIdle(context_.device);
    LOG_DEBUG_CAT("Renderer", "Device idle for resize");

    swapchainManager_->handleResize(width, height);
    context_.swapchain = swapchainManager_->getSwapchain();
    context_.swapchainImageFormat = swapchainManager_->getSwapchainImageFormat();
    context_.swapchainExtent = swapchainManager_->getSwapchainExtent();
    context_.swapchainImages = swapchainManager_->getSwapchainImages();
    context_.swapchainImageViews = swapchainManager_->getSwapchainImageViews();
    LOG_DEBUG_CAT("Renderer", "Swapchain resized: extent={}x{}, imageCount={}, viewCount={}",
                  context_.swapchainExtent.width, context_.swapchainExtent.height,
                  context_.swapchainImages.size(), context_.swapchainImageViews.size());
    if (context_.swapchainImageViews.empty()) {
        LOG_ERROR_CAT("Renderer", "Swapchain image views are empty after resize");
        throw std::runtime_error("Failed to create swapchain image views after resize");
    }

    Dispose::destroyFramebuffers(context_.device, context_.framebuffers);
    LOG_DEBUG_CAT("Renderer", "Destroyed existing framebuffers");

    createFramebuffers();
    LOG_DEBUG_CAT("Renderer", "Recreated framebuffers");

    if (!bufferManager_) {
        LOG_ERROR_CAT("Renderer", "Buffer manager is null during resize");
        throw std::runtime_error("Buffer manager is null");
    }
    bufferManager_->createUniformBuffers(MAX_FRAMES_IN_FLIGHT);
    LOG_DEBUG_CAT("Renderer", "Recreated uniform buffers for {} frames", MAX_FRAMES_IN_FLIGHT);

    if (context_.storageImage) {
        context_.resourceManager.removeImage(context_.storageImage);
        vkDestroyImage(context_.device, context_.storageImage, nullptr);
        context_.storageImage = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old storage image");
    }
    if (context_.storageImageMemory) {
        context_.resourceManager.removeMemory(context_.storageImageMemory);
        vkFreeMemory(context_.device, context_.storageImageMemory, nullptr);
        context_.storageImageMemory = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Freed old storage image memory");
    }
    if (context_.storageImageView) {
        context_.resourceManager.removeImageView(context_.storageImageView);
        vkDestroyImageView(context_.device, context_.storageImageView, nullptr);
        context_.storageImageView = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old storage image view");
    }
    VulkanInitializer::createStorageImage(context_.device, context_.physicalDevice, context_.storageImage,
                                         context_.storageImageMemory, context_.storageImageView, width, height,
                                         context_.resourceManager);
    if (!context_.storageImage || !context_.storageImageMemory || !context_.storageImageView) {
        LOG_ERROR_CAT("Renderer", "Failed to recreate storage image: image={:p}, memory={:p}, view={:p}",
                      static_cast<void*>(context_.storageImage), static_cast<void*>(context_.storageImageMemory),
                      static_cast<void*>(context_.storageImageView));
        throw std::runtime_error("Failed to recreate storage image");
    }
    LOG_DEBUG_CAT("Renderer", "Recreated storage image: image={:p}, memory={:p}, view={:p}",
                  static_cast<void*>(context_.storageImage), static_cast<void*>(context_.storageImageMemory),
                  static_cast<void*>(context_.storageImageView));

    if (denoiseImage_) {
        context_.resourceManager.removeImage(denoiseImage_);
        vkDestroyImage(context_.device, denoiseImage_, nullptr);
        denoiseImage_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old denoise image");
    }
    if (denoiseImageMemory_) {
        context_.resourceManager.removeMemory(denoiseImageMemory_);
        vkFreeMemory(context_.device, denoiseImageMemory_, nullptr);
        denoiseImageMemory_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Freed old denoise image memory");
    }
    if (denoiseImageView_) {
        context_.resourceManager.removeImageView(denoiseImageView_);
        vkDestroyImageView(context_.device, denoiseImageView_, nullptr);
        denoiseImageView_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old denoise image view");
    }
    if (denoiseSampler_) {
        vkDestroySampler(context_.device, denoiseSampler_, nullptr);
        denoiseSampler_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old denoise sampler");
    }
    VulkanInitializer::createStorageImage(context_.device, context_.physicalDevice, denoiseImage_,
                                         denoiseImageMemory_, denoiseImageView_, width, height,
                                         context_.resourceManager);
    if (!denoiseImage_ || !denoiseImageMemory_ || !denoiseImageView_) {
        LOG_ERROR_CAT("Renderer", "Failed to recreate denoise image: image={:p}, memory={:p}, view={:p}",
                      static_cast<void*>(denoiseImage_), static_cast<void*>(denoiseImageMemory_),
                      static_cast<void*>(denoiseImageView_));
        throw std::runtime_error("Failed to recreate denoise image");
    }
    LOG_DEBUG_CAT("Renderer", "Recreated denoise image: image={:p}, memory={:p}, view={:p}",
                  static_cast<void*>(denoiseImage_), static_cast<void*>(denoiseImageMemory_),
                  static_cast<void*>(denoiseImageView_));

    VkSamplerCreateInfo denoiseSamplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 0.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    if (vkCreateSampler(context_.device, &denoiseSamplerInfo, nullptr, &denoiseSampler_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to recreate denoise sampler");
        throw std::runtime_error("Failed to recreate denoise sampler");
    }
    LOG_DEBUG_CAT("Renderer", "Recreated denoise sampler: {:p}", static_cast<void*>(denoiseSampler_));

    if (envMapImage_) {
        context_.resourceManager.removeImage(envMapImage_);
        vkDestroyImage(context_.device, envMapImage_, nullptr);
        envMapImage_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old environment map image");
    }
    if (envMapImageMemory_) {
        context_.resourceManager.removeMemory(envMapImageMemory_);
        vkFreeMemory(context_.device, envMapImageMemory_, nullptr);
        envMapImageMemory_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Freed old environment map image memory");
    }
    if (envMapImageView_) {
        context_.resourceManager.removeImageView(envMapImageView_);
        vkDestroyImageView(context_.device, envMapImageView_, nullptr);
        envMapImageView_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old environment map image view");
    }
    if (envMapSampler_) {
        vkDestroySampler(context_.device, envMapSampler_, nullptr);
        envMapSampler_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old environment map sampler");
    }
    createEnvironmentMap();
    LOG_DEBUG_CAT("Renderer", "Recreated environment map (high-res)");

    for (size_t i = 0; i < materialBuffers_.size(); ++i) {
        if (materialBuffers_[i]) {
            context_.resourceManager.removeBuffer(materialBuffers_[i]);
            vkDestroyBuffer(context_.device, materialBuffers_[i], nullptr);
            materialBuffers_[i] = VK_NULL_HANDLE;
            LOG_DEBUG_CAT("Renderer", "Destroyed old material buffer[{}]", i);
        }
        if (materialBufferMemory_[i]) {
            context_.resourceManager.removeMemory(materialBufferMemory_[i]);
            vkFreeMemory(context_.device, materialBufferMemory_[i], nullptr);
            materialBufferMemory_[i] = VK_NULL_HANDLE;
            LOG_DEBUG_CAT("Renderer", "Freed old material buffer memory[{}]", i);
        }
    }
    for (size_t i = 0; i < dimensionBuffers_.size(); ++i) {
        if (dimensionBuffers_[i]) {
            context_.resourceManager.removeBuffer(dimensionBuffers_[i]);
            vkDestroyBuffer(context_.device, dimensionBuffers_[i], nullptr);
            dimensionBuffers_[i] = VK_NULL_HANDLE;
            LOG_DEBUG_CAT("Renderer", "Destroyed old dimension buffer[{}]", i);
        }
        if (dimensionBufferMemory_[i]) {
            context_.resourceManager.removeMemory(dimensionBufferMemory_[i]);
            vkFreeMemory(context_.device, dimensionBufferMemory_[i], nullptr);
            dimensionBufferMemory_[i] = VK_NULL_HANDLE;
            LOG_DEBUG_CAT("Renderer", "Freed old dimension buffer memory[{}]", i);
        }
    }

    constexpr uint32_t MATERIAL_COUNT = 128;
    constexpr uint32_t DIMENSION_COUNT = 1;
    VkDeviceSize materialBufferSize = sizeof(MaterialData) * MATERIAL_COUNT;
    VkDeviceSize dimensionBufferSize = sizeof(UE::DimensionData) * DIMENSION_COUNT;
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = nullptr,
        .properties = {}
    };
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        .pNext = nullptr,
        .shaderGroupHandleSize = 0,
        .maxRayRecursionDepth = 0,
        .maxShaderGroupStride = 0,
        .shaderGroupBaseAlignment = 0,
        .shaderGroupHandleCaptureReplaySize = 0,
        .maxRayDispatchInvocationCount = 0,
        .shaderGroupHandleAlignment = 0,
        .maxRayHitAttributeSize = 0
    };
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    VkDeviceSize minStorageBufferOffsetAlignment = props2.properties.limits.minStorageBufferOffsetAlignment;
    materialBufferSize = (materialBufferSize + minStorageBufferOffsetAlignment - 1) & ~(minStorageBufferOffsetAlignment - 1);
    dimensionBufferSize = (dimensionBufferSize + minStorageBufferOffsetAlignment - 1) & ~(minStorageBufferOffsetAlignment - 1);
    LOG_DEBUG_CAT("Renderer", "Buffer sizes for resize: materialBufferSize={} ({} materials), dimensionBufferSize={}, alignment={}",
                  materialBufferSize, MATERIAL_COUNT, dimensionBufferSize, minStorageBufferOffsetAlignment);

    materialBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    materialBufferMemory_.resize(MAX_FRAMES_IN_FLIGHT);
    dimensionBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    dimensionBufferMemory_.resize(MAX_FRAMES_IN_FLIGHT);
    frames_.resize(MAX_FRAMES_IN_FLIGHT);
    LOG_DEBUG_CAT("Renderer", "Resized frames_ to size: {}", frames_.size());
    if (frames_.size() != MAX_FRAMES_IN_FLIGHT) {
        LOG_ERROR_CAT("Renderer", "Failed to resize frames_ to MAX_FRAMES_IN_FLIGHT ({}), current size: {}", MAX_FRAMES_IN_FLIGHT, frames_.size());
        throw std::runtime_error("Failed to resize frames_ vector");
    }

    VkMemoryAllocateFlagsInfo allocFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0
    };

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, materialBufferSize,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       materialBuffers_[i], materialBufferMemory_[i], &allocFlagsInfo, context_.resourceManager);
        if (!materialBuffers_[i] || !materialBufferMemory_[i]) {
            LOG_ERROR_CAT("Renderer", "Failed to recreate material buffer[{}]: buffer={:p}, memory={:p}",
                          i, static_cast<void*>(materialBuffers_[i]), static_cast<void*>(materialBufferMemory_[i]));
            throw std::runtime_error("Failed to recreate material buffer");
        }
        LOG_DEBUG_CAT("Renderer", "Recreated material buffer[{}]: buffer={:p}, memory={:p}",
                      i, static_cast<void*>(materialBuffers_[i]), static_cast<void*>(materialBufferMemory_[i]));

        VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, dimensionBufferSize,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       dimensionBuffers_[i], dimensionBufferMemory_[i], &allocFlagsInfo, context_.resourceManager);
        if (!dimensionBuffers_[i] || !dimensionBufferMemory_[i]) {
            LOG_ERROR_CAT("Renderer", "Failed to recreate dimension buffer[{}]: buffer={:p}, memory={:p}",
                          i, static_cast<void*>(dimensionBuffers_[i]), static_cast<void*>(dimensionBufferMemory_[i]));
            throw std::runtime_error("Failed to recreate dimension buffer");
        }
        LOG_DEBUG_CAT("Renderer", "Recreated dimension buffer[{}]: buffer={:p}, memory={:p}",
                      i, static_cast<void*>(dimensionBuffers_[i]), static_cast<void*>(dimensionBufferMemory_[i]));

        initializeBufferData(i, materialBufferSize, dimensionBufferSize);
        LOG_DEBUG_CAT("Renderer", "Initialized buffer data for frame {}", i);
    }

    // Recache index count after potential model reload or resize
    // indexCount_ = static_cast<uint32_t>(getIndices().size());  // Removed: already cached statically

    if (context_.descriptorPool) {
        context_.resourceManager.removeDescriptorPool(context_.descriptorPool);
        vkDestroyDescriptorPool(context_.device, context_.descriptorPool, nullptr);
        context_.descriptorPool = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old descriptor pool");
    }
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 3}
    };
    VkDescriptorPoolCreateInfo poolInfo_desc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = MAX_FRAMES_IN_FLIGHT * 3,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes
    };
    if (vkCreateDescriptorPool(context_.device, &poolInfo_desc, nullptr, &context_.descriptorPool) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to recreate descriptor pool during resize");
        throw std::runtime_error("Failed to recreate descriptor pool");
    }
    context_.resourceManager.addDescriptorPool(context_.descriptorPool);
    LOG_DEBUG_CAT("Renderer", "Recreated descriptor pool: {:p}", static_cast<void*>(context_.descriptorPool));

    if (computeDescriptorSetLayout_) {
        context_.resourceManager.removeDescriptorSetLayout(computeDescriptorSetLayout_);
        vkDestroyDescriptorSetLayout(context_.device, computeDescriptorSetLayout_, nullptr);
        computeDescriptorSetLayout_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old compute descriptor set layout");
    }
    VkDescriptorSetLayoutBinding computeBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };
    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 2,
        .pBindings = computeBindings
    };
    if (vkCreateDescriptorSetLayout(context_.device, &computeLayoutInfo, nullptr, &computeDescriptorSetLayout_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to recreate compute descriptor set layout during resize");
        throw std::runtime_error("Failed to recreate compute descriptor set layout");
    }
    context_.resourceManager.addDescriptorSetLayout(computeDescriptorSetLayout_);
    LOG_DEBUG_CAT("Renderer", "Recreated compute descriptor set layout: {:p}", static_cast<void*>(computeDescriptorSetLayout_));

    std::vector<VkDescriptorSetLayout> rayTracingLayouts(MAX_FRAMES_IN_FLIGHT, context_.rayTracingDescriptorSetLayout);
    std::vector<VkDescriptorSetLayout> graphicsLayouts(MAX_FRAMES_IN_FLIGHT, context_.graphicsDescriptorSetLayout);
    std::vector<VkDescriptorSetLayout> computeLayouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout_);
    VkDescriptorSetAllocateInfo rayTracingAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = rayTracingLayouts.data()
    };
    VkDescriptorSetAllocateInfo graphicsAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = graphicsLayouts.data()
    };
    VkDescriptorSetAllocateInfo computeAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = computeLayouts.data()
    };
    std::vector<VkDescriptorSet> rayTracingSets(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSet> graphicsSets(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSet> computeSets(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(context_.device, &rayTracingAllocInfo, rayTracingSets.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to allocate ray-tracing descriptor sets during resize");
        throw std::runtime_error("Failed to allocate ray-tracing descriptor sets");
    }
    if (vkAllocateDescriptorSets(context_.device, &graphicsAllocInfo, graphicsSets.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to allocate graphics descriptor sets during resize");
        throw std::runtime_error("Failed to allocate graphics descriptor sets");
    }
    if (vkAllocateDescriptorSets(context_.device, &computeAllocInfo, computeSets.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to allocate compute descriptor sets during resize");
        throw std::runtime_error("Failed to allocate compute descriptor sets");
    }
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames_[i].rayTracingDescriptorSet = rayTracingSets[i];
        frames_[i].graphicsDescriptorSet = graphicsSets[i];
        frames_[i].computeDescriptorSet = computeSets[i];
        if (context_.topLevelAS != VK_NULL_HANDLE) {
            updateDescriptorSetForFrame(i, context_.topLevelAS);
            updateGraphicsDescriptorSet(i);
            updateComputeDescriptorSet(i);
        }
        LOG_DEBUG_CAT("Renderer", "Reallocated descriptor sets for frame {}: rayTracing={:p}, graphics={:p}, compute={:p}",
                      i, static_cast<void*>(rayTracingSets[i]), static_cast<void*>(graphicsSets[i]), static_cast<void*>(computeSets[i]));
    }

    Dispose::freeCommandBuffers(context_.device, context_.commandPool, context_.commandBuffers);
    LOG_DEBUG_CAT("Renderer", "Freed existing command buffers");
    for (size_t i = 0; i < context_.imageAvailableSemaphores.size(); ++i) {
        if (context_.imageAvailableSemaphores[i]) {
            vkDestroySemaphore(context_.device, context_.imageAvailableSemaphores[i], nullptr);
            context_.imageAvailableSemaphores[i] = VK_NULL_HANDLE;
            LOG_DEBUG_CAT("Renderer", "Destroyed image available semaphore[{}]", i);
        }
        if (context_.renderFinishedSemaphores[i]) {
            vkDestroySemaphore(context_.device, context_.renderFinishedSemaphores[i], nullptr);
            context_.renderFinishedSemaphores[i] = VK_NULL_HANDLE;
            LOG_DEBUG_CAT("Renderer", "Destroyed render finished semaphore[{}]", i);
        }
        if (context_.inFlightFences[i]) {
            vkDestroyFence(context_.device, context_.inFlightFences[i], nullptr);
            context_.inFlightFences[i] = VK_NULL_HANDLE;
            LOG_DEBUG_CAT("Renderer", "Destroyed in-flight fence[{}]", i);
        }
    }
    createCommandBuffers();
    createSyncObjects();
    LOG_DEBUG_CAT("Renderer", "Recreated command buffers and sync objects");

    width_ = width;
    height_ = height;
    LOG_INFO_CAT("Renderer", "VulkanRenderer resized successfully to {}x{}", width, height);
}

void VulkanRenderer::createAccelerationStructures() {
    LOG_DEBUG_CAT("Renderer", "Creating acceleration structures");
    std::vector<glm::vec3> vertices = getVertices();
    std::vector<uint32_t> indices = getIndices();
    if (vertices.empty() || indices.empty()) {
        LOG_ERROR_CAT("Renderer", "Empty vertex or index data for acceleration structure creation");
        throw std::runtime_error("Empty vertex or index data");
    }
    VulkanInitializer::createAccelerationStructures(context_, *bufferManager_, std::span<const glm::vec3>(vertices), std::span<const uint32_t>(indices));
    if (context_.topLevelAS == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Failed to create top-level acceleration structure");
        throw std::runtime_error("Failed to create TLAS");
    }
    LOG_DEBUG_CAT("Renderer", "Created TLAS: {:p}", static_cast<void*>(context_.topLevelAS));
}

std::vector<glm::vec3> VulkanRenderer::getVertices() const {
    static std::vector<glm::vec3> cachedVertices;
    if (!cachedVertices.empty()) {
        return cachedVertices;
    }
    LOG_DEBUG_CAT("Renderer", "Loading vertices from OBJ file");
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "assets/models/scene.obj")) {
        LOG_ERROR_CAT("Renderer", "Failed to load OBJ file: warn={}, err={}", warn, err);
        throw std::runtime_error("Failed to load OBJ file");
    }
    if (!warn.empty()) {
        LOG_WARNING_CAT("Renderer", "OBJ loading warning: {}", warn);
    }

    std::vector<glm::vec3> vertices;
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            glm::vec3 vertex{
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            vertices.push_back(vertex);
        }
    }
    cachedVertices = vertices;
    LOG_DEBUG_CAT("Renderer", "Loaded and cached {} vertices from OBJ", vertices.size());
    return vertices;
}

std::vector<uint32_t> VulkanRenderer::getIndices() const {
    static std::vector<uint32_t> cachedIndices;
    if (!cachedIndices.empty()) {
        return cachedIndices;
    }
    LOG_DEBUG_CAT("Renderer", "Loading indices from OBJ file");
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "assets/models/scene.obj")) {
        LOG_ERROR_CAT("Renderer", "Failed to load OBJ file: warn={}, err={}", warn, err);
        throw std::runtime_error("Failed to load OBJ file");
    }
    if (!warn.empty()) {
        LOG_WARNING_CAT("Renderer", "OBJ loading warning: {}", warn);
    }

    std::vector<uint32_t> indices;
    for (const auto& shape : shapes) {
        for (size_t i = 0; i < shape.mesh.indices.size(); ++i) {
            indices.push_back(static_cast<uint32_t>(shape.mesh.indices[i].vertex_index));
        }
    }
    cachedIndices = indices;
    LOG_DEBUG_CAT("Renderer", "Loaded and cached {} indices from OBJ", indices.size());
    return indices;
}

} // namespace VulkanRTX