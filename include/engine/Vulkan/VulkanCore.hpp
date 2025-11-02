// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX Engine, October 2025 - Core Vulkan structures and utilities.
// Dependencies: Vulkan 1.3+, GLM, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025
// FINAL: Context owns VulkanResourceManager → SINGLE LIFETIME → NO DOUBLE-FREE
//        ADDED: width, height, swapchainManager (namespace VulkanRTX)
//        FIXED: Member order → NO -Wreorder
//        BETA: All ray-tracing extensions come from <vulkan/vulkan_beta.h>
//        FIXED: ~Context() adds vkDeviceWaitIdle before resourceManager.cleanup (prevents segfault)

#pragma once
#ifndef VULKAN_CORE_HPP
#define VULKAN_CORE_HPP

#define VK_ENABLE_BETA_EXTENSIONS          // <-- BETA extensions enabled
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>            // <-- contains KHR_ray_tracing_pipeline, etc.

#include <vector>
#include <string>
#include <span>
#include <cstdint>
#include <format>
#include <glm/glm.hpp>
#include "engine/logging.hpp"
#include <unordered_map>
#include <memory>

// ===================================================================
// SDL3 ONLY: Force correct headers + safety guard
// ===================================================================
#include <SDL3/SDL.h>               // Main SDL3 header
#include <SDL3/SDL_vulkan.h>        // Vulkan integration (returns int, not SDL_bool)

// ===================================================================
// Forward declarations
// ===================================================================
class VulkanBufferManager;

namespace VulkanRTX {
    class VulkanSwapchainManager;
}

// ===================================================================
// Vulkan Resource Manager
// ===================================================================
class VulkanResourceManager {
    std::vector<VkBuffer> buffers_;
    std::vector<VkDeviceMemory> memories_;
    std::vector<VkImageView> imageViews_;
    std::vector<VkImage> images_;
    std::vector<VkAccelerationStructureKHR> accelerationStructures_;
    std::vector<VkDescriptorPool> descriptorPools_;
    std::vector<VkCommandPool> commandPools_;
    std::vector<VkRenderPass> renderPasses_;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts_;
    std::vector<VkPipelineLayout> pipelineLayouts_;
    std::vector<VkPipeline> pipelines_;
    std::vector<VkShaderModule> shaderModules_;
    std::unordered_map<std::string, VkPipeline> pipelineMap_;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VulkanBufferManager* bufferManager_ = nullptr;

public:
    VulkanResourceManager() = default;
    VulkanResourceManager(const VulkanResourceManager&) = delete;
    VulkanResourceManager& operator=(const VulkanResourceManager&) = delete;

    VulkanResourceManager(VulkanResourceManager&& other) noexcept
        : buffers_(std::move(other.buffers_))
        , memories_(std::move(other.memories_))
        , imageViews_(std::move(other.imageViews_))
        , images_(std::move(other.images_))
        , accelerationStructures_(std::move(other.accelerationStructures_))
        , descriptorPools_(std::move(other.descriptorPools_))
        , commandPools_(std::move(other.commandPools_))
        , renderPasses_(std::move(other.renderPasses_))
        , descriptorSetLayouts_(std::move(other.descriptorSetLayouts_))
        , pipelineLayouts_(std::move(other.pipelineLayouts_))
        , pipelines_(std::move(other.pipelines_))
        , shaderModules_(std::move(other.shaderModules_))
        , pipelineMap_(std::move(other.pipelineMap_))
        , device_(other.device_)
        , physicalDevice_(other.physicalDevice_)
        , bufferManager_(other.bufferManager_)
    {
        other.device_ = VK_NULL_HANDLE;
        other.physicalDevice_ = VK_NULL_HANDLE;
        other.bufferManager_ = nullptr;
    }

    VulkanResourceManager& operator=(VulkanResourceManager&& other) noexcept {
        if (this != &other) {
            cleanup(device_);
            buffers_ = std::move(other.buffers_);
            memories_ = std::move(other.memories_);
            imageViews_ = std::move(other.imageViews_);
            images_ = std::move(other.images_);
            accelerationStructures_ = std::move(other.accelerationStructures_);
            descriptorPools_ = std::move(other.descriptorPools_);
            commandPools_ = std::move(other.commandPools_);
            renderPasses_ = std::move(other.renderPasses_);
            descriptorSetLayouts_ = std::move(other.descriptorSetLayouts_);
            pipelineLayouts_ = std::move(other.pipelineLayouts_);
            pipelines_ = std::move(other.pipelines_);
            shaderModules_ = std::move(other.shaderModules_);
            pipelineMap_ = std::move(other.pipelineMap_);
            device_ = other.device_;
            physicalDevice_ = other.physicalDevice_;
            bufferManager_ = other.bufferManager_;

            other.device_ = VK_NULL_HANDLE;
            other.physicalDevice_ = VK_NULL_HANDLE;
            other.bufferManager_ = nullptr;
        }
        return *this;
    }

    ~VulkanResourceManager();

    // === Add / Remove ===
    void addBuffer(VkBuffer buffer) {
        if (buffer != VK_NULL_HANDLE) {
            buffers_.push_back(buffer);
            LOG_DEBUG(std::format("Added buffer: {:p}", static_cast<void*>(buffer)));
        }
    }
    void addMemory(VkDeviceMemory memory) {
        if (memory != VK_NULL_HANDLE) {
            memories_.push_back(memory);
            LOG_DEBUG(std::format("Added memory: {:p}", static_cast<void*>(memory)));
        }
    }
    void addImageView(VkImageView view) {
        if (view != VK_NULL_HANDLE) {
            imageViews_.push_back(view);
            LOG_DEBUG(std::format("Added image view: {:p}", static_cast<void*>(view)));
        }
    }
    void addImage(VkImage image) {
        if (image != VK_NULL_HANDLE) {
            images_.push_back(image);
            LOG_DEBUG(std::format("Added image: {:p}", static_cast<void*>(image)));
        }
    }
    void addAccelerationStructure(VkAccelerationStructureKHR as) {
        if (as != VK_NULL_HANDLE) {
            accelerationStructures_.push_back(as);
            LOG_DEBUG(std::format("Added acceleration structure: {:p}", static_cast<void*>(as)));
        }
    }
    void addDescriptorPool(VkDescriptorPool descriptorPool) {
        if (descriptorPool != VK_NULL_HANDLE) {
            descriptorPools_.push_back(descriptorPool);
            LOG_DEBUG(std::format("Added descriptor pool: {:p}", static_cast<void*>(descriptorPool)));
        }
    }
    void addCommandPool(VkCommandPool commandPool) {
        if (commandPool != VK_NULL_HANDLE) {
            commandPools_.push_back(commandPool);
            LOG_DEBUG(std::format("Added command pool: {:p}", static_cast<void*>(commandPool)));
        }
    }
    void addRenderPass(VkRenderPass renderPass) {
        if (renderPass != VK_NULL_HANDLE) {
            renderPasses_.push_back(renderPass);
            LOG_DEBUG(std::format("Added render pass: {:p}", static_cast<void*>(renderPass)));
        }
    }
    void addDescriptorSetLayout(VkDescriptorSetLayout layout) {
        if (layout != VK_NULL_HANDLE) {
            descriptorSetLayouts_.push_back(layout);
            LOG_DEBUG(std::format("Added descriptor set layout: {:p}", static_cast<void*>(layout)));
        }
    }
    void addPipelineLayout(VkPipelineLayout layout) {
        if (layout != VK_NULL_HANDLE) {
            pipelineLayouts_.push_back(layout);
            LOG_DEBUG(std::format("Added pipeline layout: {:p}", static_cast<void*>(layout)));
        }
    }
    void addPipeline(VkPipeline pipeline, const std::string& name = "") {
        if (pipeline != VK_NULL_HANDLE) {
            pipelines_.push_back(pipeline);
            if (!name.empty()) pipelineMap_[name] = pipeline;
            LOG_DEBUG(std::format("Added pipeline: {:p} ({})", static_cast<void*>(pipeline), name));
        }
    }
    void addShaderModule(VkShaderModule module) {
        if (module != VK_NULL_HANDLE) {
            shaderModules_.push_back(module);
            LOG_DEBUG(std::format("Added shader module: {:p}", static_cast<void*>(module)));
        }
    }

    // === Remove ===
    void removeBuffer(VkBuffer buffer) {
        if (buffer == VK_NULL_HANDLE) return;
        auto it = std::find(buffers_.begin(), buffers_.end(), buffer);
        if (it != buffers_.end()) {
            buffers_.erase(it);
            LOG_DEBUG(std::format("Removed buffer: {:p}", static_cast<void*>(buffer)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent buffer: {:p}", static_cast<void*>(buffer)));
        }
    }
    void removeMemory(VkDeviceMemory memory) {
        if (memory == VK_NULL_HANDLE) return;
        auto it = std::find(memories_.begin(), memories_.end(), memory);
        if (it != memories_.end()) {
            memories_.erase(it);
            LOG_DEBUG(std::format("Removed memory: {:p}", static_cast<void*>(memory)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent memory: {:p}", static_cast<void*>(memory)));
        }
    }
    void removeImageView(VkImageView view) {
        if (view == VK_NULL_HANDLE) return;
        auto it = std::find(imageViews_.begin(), imageViews_.end(), view);
        if (it != imageViews_.end()) {
            imageViews_.erase(it);
            LOG_DEBUG(std::format("Removed image view: {:p}", static_cast<void*>(view)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent image view: {:p}", static_cast<void*>(view)));
        }
    }
    void removeImage(VkImage image) {
        if (image == VK_NULL_HANDLE) return;
        auto it = std::find(images_.begin(), images_.end(), image);
        if (it != images_.end()) {
            images_.erase(it);
            LOG_DEBUG(std::format("Removed image: {:p}", static_cast<void*>(image)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent image: {:p}", static_cast<void*>(image)));
        }
    }
    void removeAccelerationStructure(VkAccelerationStructureKHR as) {
        if (as != VK_NULL_HANDLE) {
            auto it = std::find(accelerationStructures_.begin(), accelerationStructures_.end(), as);
            if (it != accelerationStructures_.end()) {
                accelerationStructures_.erase(it);
                LOG_DEBUG(std::format("Removed acceleration structure: {:p}", static_cast<void*>(as)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent acceleration structure: {:p}", static_cast<void*>(as)));
            }
        }
    }
    void removeDescriptorPool(VkDescriptorPool descriptorPool) {
        if (descriptorPool != VK_NULL_HANDLE) {
            auto it = std::find(descriptorPools_.begin(), descriptorPools_.end(), descriptorPool);
            if (it != descriptorPools_.end()) {
                descriptorPools_.erase(it);
                LOG_DEBUG(std::format("Removed descriptor pool: {:p}", static_cast<void*>(descriptorPool)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent descriptor pool: {:p}", static_cast<void*>(descriptorPool)));
            }
        }
    }
    void removeCommandPool(VkCommandPool commandPool) {
        if (commandPool != VK_NULL_HANDLE) {
            auto it = std::find(commandPools_.begin(), commandPools_.end(), commandPool);
            if (it != commandPools_.end()) {
                commandPools_.erase(it);
                LOG_DEBUG(std::format("Removed command pool: {:p}", static_cast<void*>(commandPool)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent command pool: {:p}", static_cast<void*>(commandPool)));
            }
        }
    }
    void removeRenderPass(VkRenderPass renderPass) {
        if (renderPass != VK_NULL_HANDLE) {
            auto it = std::find(renderPasses_.begin(), renderPasses_.end(), renderPass);
            if (it != renderPasses_.end()) {
                renderPasses_.erase(it);
                LOG_DEBUG(std::format("Removed render pass: {:p}", static_cast<void*>(renderPass)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent render pass: {:p}", static_cast<void*>(renderPass)));
            }
        }
    }
    void removeDescriptorSetLayout(VkDescriptorSetLayout layout) {
        if (layout != VK_NULL_HANDLE) {
            auto it = std::find(descriptorSetLayouts_.begin(), descriptorSetLayouts_.end(), layout);
            if (it != descriptorSetLayouts_.end()) {
                descriptorSetLayouts_.erase(it);
                LOG_DEBUG(std::format("Removed descriptor set layout: {:p}", static_cast<void*>(layout)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent descriptor set layout: {:p}", static_cast<void*>(layout)));
            }
        }
    }
    void removePipelineLayout(VkPipelineLayout layout) {
        if (layout != VK_NULL_HANDLE) {
            auto it = std::find(pipelineLayouts_.begin(), pipelineLayouts_.end(), layout);
            if (it != pipelineLayouts_.end()) {
                pipelineLayouts_.erase(it);
                LOG_DEBUG(std::format("Removed pipeline layout: {:p}", static_cast<void*>(layout)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent pipeline layout: {:p}", static_cast<void*>(layout)));
            }
        }
    }
    void removePipeline(VkPipeline pipeline) {
        if (pipeline != VK_NULL_HANDLE) {
            auto it = std::find(pipelines_.begin(), pipelines_.end(), pipeline);
            if (it != pipelines_.end()) {
                pipelines_.erase(it);
                for (auto mapIt = pipelineMap_.begin(); mapIt != pipelineMap_.end(); ) {
                    if (mapIt->second == pipeline) mapIt = pipelineMap_.erase(mapIt);
                    else ++mapIt;
                }
                LOG_DEBUG(std::format("Removed pipeline: {:p}", static_cast<void*>(pipeline)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent pipeline: {:p}", static_cast<void*>(pipeline)));
            }
        }
    }
    void removeShaderModule(VkShaderModule module) {
        if (module != VK_NULL_HANDLE) {
            auto it = std::find(shaderModules_.begin(), shaderModules_.end(), module);
            if (it != shaderModules_.end()) {
                shaderModules_.erase(it);
                LOG_DEBUG(std::format("Removed shader module: {:p}", static_cast<void*>(module)));
            } else {
                LOG_WARNING(std::format("Attempted to remove non-existent shader module: {:p}", static_cast<void*>(module)));
            }
        }
    }

    // === Getters ===
    const std::vector<VkBuffer>& getBuffers() const { return buffers_; }
    const std::vector<VkDeviceMemory>& getMemories() const { return memories_; }
    const std::vector<VkImageView>& getImageViews() const { return imageViews_; }
    const std::vector<VkImage>& getImages() const { return images_; }
    const std::vector<VkAccelerationStructureKHR>& getAccelerationStructures() const { return accelerationStructures_; }
    const std::vector<VkDescriptorPool>& getDescriptorPools() const { return descriptorPools_; }
    const std::vector<VkCommandPool>& getCommandPools() const { return commandPools_; }
    const std::vector<VkRenderPass>& getRenderPasses() const { return renderPasses_; }
    const std::vector<VkDescriptorSetLayout>& getDescriptorSetLayouts() const { return descriptorSetLayouts_; }
    const std::vector<VkPipelineLayout>& getPipelineLayouts() const { return pipelineLayouts_; }
    const std::vector<VkPipeline>& getPipelines() const { return pipelines_; }
    const std::vector<VkShaderModule>& getShaderModules() const { return shaderModules_; }

    void setDevice(VkDevice newDevice, VkPhysicalDevice physicalDevice) {
        if (newDevice == VK_NULL_HANDLE) {
            LOG_ERROR("Cannot set null device to resource manager");
            throw std::invalid_argument("Cannot set null device");
        }
        device_ = newDevice;
        physicalDevice_ = physicalDevice;
        LOG_INFO(std::format("Resource manager device set: {:p}", static_cast<void*>(device_)));
    }
    VkDevice getDevice() const { return device_; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }

    VkPipeline getPipeline(const std::string& name) const {
        auto it = pipelineMap_.find(name);
        if (it != pipelineMap_.end()) return it->second;
        LOG_WARNING(std::format("Pipeline '{}' not found", name));
        return VK_NULL_HANDLE;
    }

    void setBufferManager(VulkanBufferManager* mgr) { bufferManager_ = mgr; }
    VulkanBufferManager* getBufferManager() { return bufferManager_; }
    const VulkanBufferManager* getBufferManager() const { return bufferManager_; }

    void cleanup(VkDevice device = VK_NULL_HANDLE);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
};

// ===================================================================
// Vulkan Context
// ===================================================================
namespace Vulkan {

struct Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    std::vector<std::string> instanceExtensions;
    SDL_Window* window = nullptr;

    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    uint32_t presentQueueFamilyIndex = UINT32_MAX;
    uint32_t computeQueueFamilyIndex = UINT32_MAX;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    VkPhysicalDeviceMemoryProperties memoryProperties;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    std::unique_ptr<VulkanRTX::VulkanSwapchainManager> swapchainManager;

    int width = 0;
    int height = 0;
    VkExtent2D swapchainExtent = {0, 0};

    VkDescriptorSetLayout rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout rayTracingPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;

    VkPipeline rayTracingPipeline = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkPipeline computePipeline = VK_NULL_HANDLE;

    VkRenderPass renderPass = VK_NULL_HANDLE;

    VulkanResourceManager resourceManager;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    VkAccelerationStructureKHR bottomLevelAS = VK_NULL_HANDLE;
    VkAccelerationStructureKHR topLevelAS = VK_NULL_HANDLE;
    VkBuffer bottomLevelASBuffer = VK_NULL_HANDLE;
    VkDeviceMemory bottomLevelASMemory = VK_NULL_HANDLE;
    VkBuffer topLevelASBuffer = VK_NULL_HANDLE;
    VkDeviceMemory topLevelASMemory = VK_NULL_HANDLE;

    uint32_t sbtRecordSize = 0;

    VkDescriptorPool graphicsDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBufferMemories;
    std::vector<VkShaderModule> shaderModules;

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    bool enableRayTracing = true;

    VkImage storageImage = VK_NULL_HANDLE;
    VkDeviceMemory storageImageMemory = VK_NULL_HANDLE;
    VkImageView storageImageView = VK_NULL_HANDLE;

    VkBuffer raygenSbtBuffer = VK_NULL_HANDLE;
    VkDeviceMemory raygenSbtMemory = VK_NULL_HANDLE;
    VkBuffer missSbtBuffer = VK_NULL_HANDLE;
    VkDeviceMemory missSbtMemory = VK_NULL_HANDLE;
    VkBuffer hitSbtBuffer = VK_NULL_HANDLE;
    VkDeviceMemory hitSbtMemory = VK_NULL_HANDLE;

    VkDeviceAddress raygenSbtAddress   = 0;
    VkDeviceAddress missSbtAddress     = 0;
    VkDeviceAddress hitSbtAddress      = 0;
    VkDeviceAddress callableSbtAddress = 0;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchBufferMemory = VK_NULL_HANDLE;

    uint32_t indexCount = 0;

    VkBuffer blasBuffer   = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory   = VK_NULL_HANDLE;
    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
    VkBuffer tlasBuffer   = VK_NULL_HANDLE;
    VkDeviceMemory tlasMemory   = VK_NULL_HANDLE;

    VkImage rtOutputImage      = VK_NULL_HANDLE;
    VkImageView rtOutputImageView  = VK_NULL_HANDLE;

    VkImage envMapImage        = VK_NULL_HANDLE;
    VkDeviceMemory envMapImageMemory  = VK_NULL_HANDLE;
    VkImageView envMapImageView    = VK_NULL_HANDLE;
    VkSampler envMapSampler      = VK_NULL_HANDLE;

    // Ray-tracing properties (filled by vkGetPhysicalDeviceProperties2)
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
    };

    // -----------------------------------------------------------------
    // Ray-tracing function pointers (beta extensions)
    // -----------------------------------------------------------------
    PFN_vkCmdTraceRaysKHR                       vkCmdTraceRaysKHR                       = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR          vkCreateRayTracingPipelinesKHR          = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR    vkGetRayTracingShaderGroupHandlesKHR    = nullptr;
    PFN_vkCreateAccelerationStructureKHR        vkCreateAccelerationStructureKHR        = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR     vkCmdBuildAccelerationStructuresKHR     = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkGetBufferDeviceAddressKHR             vkGetBufferDeviceAddressKHR             = nullptr;
    PFN_vkDestroyAccelerationStructureKHR       vkDestroyAccelerationStructureKHR       = nullptr;
    PFN_vkCreateDeferredOperationKHR            vkCreateDeferredOperationKHR            = nullptr;
    PFN_vkDeferredOperationJoinKHR              vkDeferredOperationJoinKHR             = nullptr;
    PFN_vkGetDeferredOperationResultKHR         vkGetDeferredOperationResultKHR         = nullptr;
    PFN_vkDestroyDeferredOperationKHR           vkDestroyDeferredOperationKHR           = nullptr;

    // Constructor: width/height BEFORE swapchainExtent → NO -Wreorder
    Context(SDL_Window* win, int w, int h)
        : window(win),
          width(w),
          height(h),
          swapchainExtent{static_cast<uint32_t>(w), static_cast<uint32_t>(h)}
    {
        LOG_INFO_CAT("Vulkan::Context",
                     "{}Created with window @ {}x{}{}",
                     Logging::Color::ARCTIC_CYAN, w, h, Logging::Color::RESET);
    }

    Context() = delete;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    // FIXED: Add vkDeviceWaitIdle before cleanup to ensure no pending ops (prevents segfault)
    ~Context() {
        if (device) {
            vkDeviceWaitIdle(device);
            resourceManager.cleanup(device);
        }
    }

    VulkanBufferManager* getBufferManager() { return resourceManager.getBufferManager(); }
    const VulkanBufferManager* getBufferManager() const { return resourceManager.getBufferManager(); }
};

} // namespace Vulkan

// ===================================================================
// Include dependent headers
// ===================================================================
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"

namespace VulkanInitializer {
    void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                    VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    void createAccelerationStructures(Vulkan::Context& context, VulkanBufferManager& bufferManager,
                                     std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);

    void createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& image,
                           VkDeviceMemory& memory, VkImageView& view, uint32_t width, uint32_t height,
                           VulkanResourceManager& resourceManager);

    void createShaderBindingTable(Vulkan::Context& context);

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkCommandBuffer beginSingleTimeCommands(Vulkan::Context& context);
    void endSingleTimeCommands(Vulkan::Context& context, VkCommandBuffer commandBuffer);

    void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                     VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                     VkBuffer& buffer, VkDeviceMemory& bufferMemory,
                     const VkMemoryAllocateFlagsInfo* allocFlagsInfo,
                     VulkanResourceManager& resourceManager);

    void initializeVulkan(Vulkan::Context& context);

    VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer);
    VkDeviceAddress getAccelerationStructureDeviceAddress(VkDevice device, VkAccelerationStructureKHR as);

    VkPhysicalDevice findPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, bool preferNvidia);

    void initInstance(const std::vector<std::string>& instanceExtensions, Vulkan::Context& context);
    void initSurface(Vulkan::Context& context, void* window, VkSurfaceKHR* rawsurface);
    void initDevice(Vulkan::Context& context);

    void createDescriptorSetLayout(VkDevice device, VkPhysicalDevice physicalDevice,
                                  VkDescriptorSetLayout& rayTracingLayout, VkDescriptorSetLayout& graphicsLayout);

    void createDescriptorPoolAndSet(VkDevice device, VkPhysicalDevice physicalDevice,
                                   VkDescriptorSetLayout descriptorSetLayout,
                                   VkDescriptorPool& descriptorPool, std::vector<VkDescriptorSet>& descriptorSets,
                                   VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
                                   VkAccelerationStructureKHR topLevelAS, bool forRayTracing,
                                   std::vector<VkBuffer> materialBuffers, std::vector<VkBuffer> dimensionBuffers,
                                   VkImageView denoiseImageView, VkImageView envMapView, VkImageView densityVolumeView,
                                   VkImageView gDepthView, VkImageView gNormalView);

    void transitionImageLayout(Vulkan::Context& context, VkImage image, VkFormat format,
                               VkImageLayout oldLayout, VkImageLayout newLayout);

    // -----------------------------------------------------------------
    // Load all ray-tracing function pointers (beta extensions)
    // -----------------------------------------------------------------
    void loadRayTracingExtensions(Vulkan::Context& context);
}

#endif // VULKAN_CORE_HPP