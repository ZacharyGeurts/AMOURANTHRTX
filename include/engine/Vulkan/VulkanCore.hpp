// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX Engine, November 2025 - Core Vulkan structures and utilities.
// Dependencies: Vulkan 1.3+, GLM, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025
// " The spinal column"
// FINAL: Context owns VulkanResourceManager → SINGLE LIFETIME → NO DOUBLE-FREE
//        ADDED: get/setBufferManager(), getResourceManager()
//        ADDED: hasX() for RAII safety
//        ADDED: contextDevicePtr_ for safe cleanup
//        ADDED: samplers_ + add/remove/has/getSamplers() for texture samplers
//        FIXED: Member order → NO -Wreorder
//        BETA: All ray-tracing extensions from <vulkan/vulkan_beta.h>
//        FIXED: ~Context() → vkDeviceWaitIdle + resourceManager.cleanup
//        ADDED: MUTABLE getX() accessors for cleanupAll()
//        NEW: Context owns swapchain creation/destruction
//        FIXED: addFence() + fences_ for transient submits
//        ADDED: VulkanRTX::Camera* camera = nullptr;  ← CRITICAL FOR renderModeX()
//        FIXED: ~Context() → delete camera (leak prevention for raw ptr)

#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/camera.hpp"          // ← ADDED: Camera integration
#include <vector>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include "engine/logging.hpp"
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <ranges>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

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
    std::vector<VkSampler> samplers_;  // NEW: Track samplers (destroy after ImageViews)
    std::vector<VkAccelerationStructureKHR> accelerationStructures_;
    std::vector<VkDescriptorPool> descriptorPools_;
    std::vector<VkCommandPool> commandPools_;
    std::vector<VkRenderPass> renderPasses_;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts_;
    std::vector<VkPipelineLayout> pipelineLayouts_;
    std::vector<VkPipeline> pipelines_;
    std::vector<VkShaderModule> shaderModules_;
    std::vector<VkDescriptorSet> descriptorSets_;
    std::vector<VkFence> fences_;  // ← ADDED: Track transient fences
    std::unordered_map<std::string, VkPipeline> pipelineMap_;

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VulkanBufferManager* bufferManager_ = nullptr;
    const VkDevice* contextDevicePtr_ = nullptr;

    // NEW: KHR function pointer for cleanup
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR_ = nullptr;

public:
    VulkanResourceManager() = default;
    VulkanResourceManager(const VulkanResourceManager&) = delete;
    VulkanResourceManager& operator=(const VulkanResourceManager&) = delete;

    VulkanResourceManager(VulkanResourceManager&& other) noexcept;
    VulkanResourceManager& operator=(VulkanResourceManager&& other) noexcept;
    ~VulkanResourceManager();

    // NEW: Set KHR function
    void setAccelerationStructureDestroyFunc(PFN_vkDestroyAccelerationStructureKHR func) {
        vkDestroyAccelerationStructureKHR_ = func;
    }

    // === Add ===
    void addBuffer(VkBuffer buffer) {
        if (buffer != VK_NULL_HANDLE) {
            buffers_.push_back(buffer);
            LOG_DEBUG("Added buffer: 0x{:x}", reinterpret_cast<uintptr_t>(buffer));
        }
    }
    void addMemory(VkDeviceMemory memory) {
        if (memory != VK_NULL_HANDLE) {
            memories_.push_back(memory);
            LOG_DEBUG("Added memory: 0x{:x}", reinterpret_cast<uintptr_t>(memory));
        }
    }
    void addImageView(VkImageView view) {
        if (view != VK_NULL_HANDLE) {
            imageViews_.push_back(view);
            LOG_DEBUG("Added image view: 0x{:x}", reinterpret_cast<uintptr_t>(view));
        }
    }
    void addImage(VkImage image) {
        if (image != VK_NULL_HANDLE) {
            images_.push_back(image);
            LOG_DEBUG("Added image: 0x{:x}", reinterpret_cast<uintptr_t>(image));
        }
    }
    void addSampler(VkSampler sampler) {  // NEW: Add sampler
        if (sampler != VK_NULL_HANDLE) {
            samplers_.push_back(sampler);
            LOG_DEBUG("Added sampler: 0x{:x}", reinterpret_cast<uintptr_t>(sampler));
        }
    }
    void addAccelerationStructure(VkAccelerationStructureKHR as) {
        if (as != VK_NULL_HANDLE) {
            accelerationStructures_.push_back(as);
            LOG_DEBUG("Added acceleration structure: 0x{:x}", reinterpret_cast<uintptr_t>(as));
        }
    }
    void addDescriptorPool(VkDescriptorPool descriptorPool) {
        if (descriptorPool != VK_NULL_HANDLE) {
            descriptorPools_.push_back(descriptorPool);
            LOG_DEBUG("Added descriptor pool: 0x{:x}", reinterpret_cast<uintptr_t>(descriptorPool));
        }
    }
    void addDescriptorSet(VkDescriptorSet set) {
        if (set != VK_NULL_HANDLE) {
            descriptorSets_.push_back(set);
            LOG_DEBUG("Added descriptor set: 0x{:x}", reinterpret_cast<uintptr_t>(set));
        }
    }
    void addCommandPool(VkCommandPool commandPool) {
        if (commandPool != VK_NULL_HANDLE) {
            commandPools_.push_back(commandPool);
            LOG_DEBUG("Added command pool: 0x{:x}", reinterpret_cast<uintptr_t>(commandPool));
        }
    }
    void addRenderPass(VkRenderPass renderPass) {
        if (renderPass != VK_NULL_HANDLE) {
            renderPasses_.push_back(renderPass);
            LOG_DEBUG("Added render pass: 0x{:x}", reinterpret_cast<uintptr_t>(renderPass));
        }
    }
    void addDescriptorSetLayout(VkDescriptorSetLayout layout) {
        if (layout != VK_NULL_HANDLE) {
            descriptorSetLayouts_.push_back(layout);
            LOG_DEBUG("Added descriptor set layout: 0x{:x}", reinterpret_cast<uintptr_t>(layout));
        }
    }
    void addPipelineLayout(VkPipelineLayout layout) {
        if (layout != VK_NULL_HANDLE) {
            pipelineLayouts_.push_back(layout);
            LOG_DEBUG("Added pipeline layout: 0x{:x}", reinterpret_cast<uintptr_t>(layout));
        }
    }
    void addPipeline(VkPipeline pipeline, const std::string& name = "") {
        if (pipeline != VK_NULL_HANDLE) {
            pipelines_.push_back(pipeline);
            if (!name.empty()) pipelineMap_[name] = pipeline;
            LOG_DEBUG("Added pipeline: 0x{:x} ({})", reinterpret_cast<uintptr_t>(pipeline), name);
        }
    }
    void addShaderModule(VkShaderModule module) {
        if (module != VK_NULL_HANDLE) {
            shaderModules_.push_back(module);
            LOG_DEBUG("Added shader module: 0x{:x}", reinterpret_cast<uintptr_t>(module));
        }
    }

    // ← NEW: addFence
    void addFence(VkFence fence) {
        if (fence != VK_NULL_HANDLE) {
            fences_.push_back(fence);
            LOG_DEBUG("Added fence: 0x{:x}", reinterpret_cast<uintptr_t>(fence));
        }
    }

    // === Remove ===
    void removeBuffer(VkBuffer buffer) {
        if (buffer == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(buffers_, buffer);
        if (it != buffers_.end()) {
            buffers_.erase(it);
            LOG_DEBUG("Removed buffer: 0x{:x}", reinterpret_cast<uintptr_t>(buffer));
        }
    }
    void removeMemory(VkDeviceMemory memory) {
        if (memory == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(memories_, memory);
        if (it != memories_.end()) {
            memories_.erase(it);
            LOG_DEBUG("Removed memory: 0x{:x}", reinterpret_cast<uintptr_t>(memory));
        }
    }
    void removeImageView(VkImageView view) {
        if (view == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(imageViews_, view);
        if (it != imageViews_.end()) {
            imageViews_.erase(it);
            LOG_DEBUG("Removed image view: 0x{:x}", reinterpret_cast<uintptr_t>(view));
        }
    }
    void removeImage(VkImage image) {
        if (image == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(images_, image);
        if (it != images_.end()) {
            images_.erase(it);
            LOG_DEBUG("Removed image: 0x{:x}", reinterpret_cast<uintptr_t>(image));
        }
    }
    void removeSampler(VkSampler sampler) {  // NEW: Remove sampler
        if (sampler == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(samplers_, sampler);
        if (it != samplers_.end()) {
            samplers_.erase(it);
            LOG_DEBUG("Removed sampler: 0x{:x}", reinterpret_cast<uintptr_t>(sampler));
        }
    }
    void removeAccelerationStructure(VkAccelerationStructureKHR as) {
        if (as == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(accelerationStructures_, as);
        if (it != accelerationStructures_.end()) {
            accelerationStructures_.erase(it);
            LOG_DEBUG("Removed acceleration structure: 0x{:x}", reinterpret_cast<uintptr_t>(as));
        }
    }
    void removeDescriptorPool(VkDescriptorPool descriptorPool) {
        if (descriptorPool == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(descriptorPools_, descriptorPool);
        if (it != descriptorPools_.end()) {
            descriptorPools_.erase(it);
            LOG_DEBUG("Removed descriptor pool: 0x{:x}", reinterpret_cast<uintptr_t>(descriptorPool));
        }
    }
    void removeDescriptorSet(VkDescriptorSet set) {
        if (set == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(descriptorSets_, set);
        if (it != descriptorSets_.end()) {
            descriptorSets_.erase(it);
            LOG_DEBUG("Removed descriptor set: 0x{:x}", reinterpret_cast<uintptr_t>(set));
        }
    }
    void removeCommandPool(VkCommandPool commandPool) {
        if (commandPool == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(commandPools_, commandPool);
        if (it != commandPools_.end()) {
            commandPools_.erase(it);
            LOG_DEBUG("Removed command pool: 0x{:x}", reinterpret_cast<uintptr_t>(commandPool));
        }
    }
    void removeRenderPass(VkRenderPass renderPass) {
        if (renderPass == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(renderPasses_, renderPass);
        if (it != renderPasses_.end()) {
            renderPasses_.erase(it);
            LOG_DEBUG("Removed render pass: 0x{:x}", reinterpret_cast<uintptr_t>(renderPass));
        }
    }
    void removeDescriptorSetLayout(VkDescriptorSetLayout layout) {
        if (layout == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(descriptorSetLayouts_, layout);
        if (it != descriptorSetLayouts_.end()) {
            descriptorSetLayouts_.erase(it);
            LOG_DEBUG("Removed descriptor set layout: 0x{:x}", reinterpret_cast<uintptr_t>(layout));
        }
    }
    void removePipelineLayout(VkPipelineLayout layout) {
        if (layout == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(pipelineLayouts_, layout);
        if (it != pipelineLayouts_.end()) {
            pipelineLayouts_.erase(it);
            LOG_DEBUG("Removed pipeline layout: 0x{:x}", reinterpret_cast<uintptr_t>(layout));
        }
    }
    void removePipeline(VkPipeline pipeline) {
        if (pipeline == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(pipelines_, pipeline);
        if (it != pipelines_.end()) {
            pipelines_.erase(it);
            auto pred = [pipeline](const auto& p) { return p.second == pipeline; };
            auto map_it = std::ranges::find_if(pipelineMap_, pred);
            if (map_it != pipelineMap_.end()) {
                pipelineMap_.erase(map_it);
            }
            LOG_DEBUG("Removed pipeline: 0x{:x}", reinterpret_cast<uintptr_t>(pipeline));
        }
    }
    void removeShaderModule(VkShaderModule module) {
        if (module == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(shaderModules_, module);
        if (it != shaderModules_.end()) {
            shaderModules_.erase(it);
            LOG_DEBUG("Removed shader module: 0x{:x}", reinterpret_cast<uintptr_t>(module));
        }
    }

    // ← NEW: removeFence
    void removeFence(VkFence fence) {
        if (fence == VK_NULL_HANDLE) return;
        auto it = std::ranges::find(fences_, fence);
        if (it != fences_.end()) {
            fences_.erase(it);
            LOG_DEBUG("Removed fence: 0x{:x}", reinterpret_cast<uintptr_t>(fence));
        }
    }

    // === Has ===
    bool hasBuffer(VkBuffer buffer) const { return std::ranges::find(buffers_, buffer) != buffers_.end(); }
    bool hasMemory(VkDeviceMemory memory) const { return std::ranges::find(memories_, memory) != memories_.end(); }
    bool hasImageView(VkImageView view) const { return std::ranges::find(imageViews_, view) != imageViews_.end(); }
    bool hasImage(VkImage image) const { return std::ranges::find(images_, image) != images_.end(); }
    bool hasSampler(VkSampler sampler) const { return std::ranges::find(samplers_, sampler) != samplers_.end(); }  // NEW
    bool hasAccelerationStructure(VkAccelerationStructureKHR as) const { return std::ranges::find(accelerationStructures_, as) != accelerationStructures_.end(); }
    bool hasDescriptorPool(VkDescriptorPool pool) const { return std::ranges::find(descriptorPools_, pool) != descriptorPools_.end(); }
    bool hasDescriptorSet(VkDescriptorSet set) const { return std::ranges::find(descriptorSets_, set) != descriptorSets_.end(); }
    bool hasCommandPool(VkCommandPool pool) const { return std::ranges::find(commandPools_, pool) != commandPools_.end(); }
    bool hasRenderPass(VkRenderPass rp) const { return std::ranges::find(renderPasses_, rp) != renderPasses_.end(); }
    bool hasDescriptorSetLayout(VkDescriptorSetLayout layout) const { return std::ranges::find(descriptorSetLayouts_, layout) != descriptorSetLayouts_.end(); }
    bool hasPipelineLayout(VkPipelineLayout layout) const { return std::ranges::find(pipelineLayouts_, layout) != pipelineLayouts_.end(); }
    bool hasPipeline(VkPipeline pipeline) const { return std::ranges::find(pipelines_, pipeline) != pipelines_.end(); }
    bool hasShaderModule(VkShaderModule module) const { return std::ranges::find(shaderModules_, module) != shaderModules_.end(); }
    bool hasFence(VkFence fence) const { return std::ranges::find(fences_, fence) != fences_.end(); }

    // === Getters (CONST) ===
    const std::vector<VkBuffer>& getBuffers() const { return buffers_; }
    const std::vector<VkDeviceMemory>& getMemories() const { return memories_; }
    const std::vector<VkImageView>& getImageViews() const { return imageViews_; }
    const std::vector<VkImage>& getImages() const { return images_; }
    const std::vector<VkSampler>& getSamplers() const { return samplers_; }  // NEW
    const std::vector<VkAccelerationStructureKHR>& getAccelerationStructures() const { return accelerationStructures_; }
    const std::vector<VkDescriptorPool>& getDescriptorPools() const { return descriptorPools_; }
    const std::vector<VkDescriptorSet>& getDescriptorSets() const { return descriptorSets_; }
    const std::vector<VkCommandPool>& getCommandPools() const { return commandPools_; }
    const std::vector<VkRenderPass>& getRenderPasses() const { return renderPasses_; }
    const std::vector<VkDescriptorSetLayout>& getDescriptorSetLayouts() const { return descriptorSetLayouts_; }
    const std::vector<VkPipelineLayout>& getPipelineLayouts() const { return pipelineLayouts_; }
    const std::vector<VkPipeline>& getPipelines() const { return pipelines_; }
    const std::vector<VkShaderModule>& getShaderModules() const { return shaderModules_; }
    const std::vector<VkFence>& getFences() const { return fences_; }

    // === MUTABLE GETTERS (FOR cleanupAll()) ===
    std::vector<VkBuffer>& getBuffersMutable() { return buffers_; }
    std::vector<VkDeviceMemory>& getMemoriesMutable() { return memories_; }
    std::vector<VkImageView>& getImageViewsMutable() { return imageViews_; }
    std::vector<VkImage>& getImagesMutable() { return images_; }
    std::vector<VkSampler>& getSamplersMutable() { return samplers_; }  // NEW
    std::vector<VkAccelerationStructureKHR>& getAccelerationStructuresMutable() { return accelerationStructures_; }
    std::vector<VkDescriptorPool>& getDescriptorPoolsMutable() { return descriptorPools_; }
    std::vector<VkDescriptorSet>& getDescriptorSetsMutable() { return descriptorSets_; }
    std::vector<VkCommandPool>& getCommandPoolsMutable() { return commandPools_; }
    std::vector<VkRenderPass>& getRenderPassesMutable() { return renderPasses_; }
    std::vector<VkDescriptorSetLayout>& getDescriptorSetLayoutsMutable() { return descriptorSetLayouts_; }
    std::vector<VkPipelineLayout>& getPipelineLayoutsMutable() { return pipelineLayouts_; }
    std::vector<VkPipeline>& getPipelinesMutable() { return pipelines_; }
    std::vector<VkShaderModule>& getShaderModulesMutable() { return shaderModules_; }
    std::vector<VkFence>& getFencesMutable() { return fences_; }

    void setDevice(VkDevice newDevice, VkPhysicalDevice physicalDevice, const VkDevice* contextDevicePtr = nullptr) {
        if (newDevice == VK_NULL_HANDLE) {
            LOG_ERROR("Cannot set null device to resource manager");
            throw std::invalid_argument("Cannot set null device");
        }
        device_ = newDevice;
        physicalDevice_ = physicalDevice;
        contextDevicePtr_ = contextDevicePtr;
        LOG_INFO("Resource manager device set: 0x{:x}", reinterpret_cast<uintptr_t>(device_));
    }
    VkDevice getDevice() const { return contextDevicePtr_ ? *contextDevicePtr_ : device_; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }

    VkPipeline getPipeline(const std::string& name) const {
        auto it = pipelineMap_.find(name);
        if (it != pipelineMap_.end()) return it->second;
        LOG_WARNING("Pipeline '{}' not found", name);
        return VK_NULL_HANDLE;
    }

    void setBufferManager(VulkanBufferManager* mgr) { bufferManager_ = mgr; }
    VulkanBufferManager* getBufferManager() { return bufferManager_; }
    const VulkanBufferManager* getBufferManager() const { return bufferManager_; }

    void cleanup(VkDevice device = VK_NULL_HANDLE);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
};

// ===================================================================
// Vulkan Context — GLOBAL NAMESPACE
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

    // --- SWAPCHAIN OWNED BY CONTEXT ---
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent = {0, 0};

    int width = 0;
    int height = 0;

    // --- CAMERA ---
    VulkanRTX::Camera* camera = nullptr;  // ← ADDED: Critical for renderModeX()

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

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
    };

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

    // --- SWAPCHAIN MANAGER (OWNED BY CONTEXT) ---
    std::unique_ptr<VulkanRTX::VulkanSwapchainManager> swapchainManager;

    Context(SDL_Window* win, int w, int h)
        : window(win),
          width(w),
          height(h),
          swapchainExtent{static_cast<uint32_t>(w), static_cast<uint32_t>(h)}
    {
        LOG_INFO_CAT("Vulkan::Context",
                     "Created with window @ {}x{}{}",
                     Logging::Color::ARCTIC_CYAN, w, h, Logging::Color::RESET);
    }

    Context() = delete;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    ~Context() {
        if (device) {
            vkDeviceWaitIdle(device);
            if (camera) {
                camera = nullptr;  // ← CORRECT: just null it
            }
            destroySwapchain();
            resourceManager.cleanup(device);
        }
    }

    // --- SWAPCHAIN LIFECYCLE ---
    void createSwapchain();
    void destroySwapchain();

    // --- BUFFER MANAGER ACCESS ---
    VulkanBufferManager* getBufferManager() { return resourceManager.getBufferManager(); }
    const VulkanBufferManager* getBufferManager() const { return resourceManager.getBufferManager(); }
    void setBufferManager(VulkanBufferManager* mgr) { resourceManager.setBufferManager(mgr); }

    // --- RESOURCE MANAGER ACCESS ---
    VulkanResourceManager& getResourceManager() { return resourceManager; }
    const VulkanResourceManager& getResourceManager() const { return resourceManager; }
};

} // namespace Vulkan

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"  // ← MOVED HERE TO AVOID CYCLE