// AMOURANTH RTX Engine, October 2025 - Core Vulkan structures and utilities.
// Dependencies: Vulkan 1.3+, GLM, logging.hpp, VulkanBufferManager.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#pragma once
#ifndef VULKAN_CORE_HPP
#define VULKAN_CORE_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <span>
#include <cstdint>
#include <format>
#include <glm/glm.hpp>
#include "engine/logging.hpp"
#include <unordered_map>

// Forward declaration to avoid circular dependency
class VulkanBufferManager;

// ====================================================================
// Vulkan Resource Manager – FULL IMPLEMENTATION (fixes all undefined refs)
// ====================================================================
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

public:
    // --------------------- ADDERS ---------------------
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

    // --------------------- REMOVERS (IMPLEMENTED) ---------------------
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
        if (as == VK_NULL_HANDLE) return;
        auto it = std::find(accelerationStructures_.begin(), accelerationStructures_.end(), as);
        if (it != accelerationStructures_.end()) {
            accelerationStructures_.erase(it);
            LOG_DEBUG(std::format("Removed acceleration structure: {:p}", static_cast<void*>(as)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent acceleration structure: {:p}", static_cast<void*>(as)));
        }
    }
    void removeDescriptorPool(VkDescriptorPool descriptorPool) {
        if (descriptorPool == VK_NULL_HANDLE) return;
        auto it = std::find(descriptorPools_.begin(), descriptorPools_.end(), descriptorPool);
        if (it != descriptorPools_.end()) {
            descriptorPools_.erase(it);
            LOG_DEBUG(std::format("Removed descriptor pool: {:p}", static_cast<void*>(descriptorPool)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent descriptor pool: {:p}", static_cast<void*>(descriptorPool)));
        }
    }
    void removeCommandPool(VkCommandPool commandPool) {
        if (commandPool == VK_NULL_HANDLE) return;
        auto it = std::find(commandPools_.begin(), commandPools_.end(), commandPool);
        if (it != commandPools_.end()) {
            commandPools_.erase(it);
            LOG_DEBUG(std::format("Removed command pool: {:p}", static_cast<void*>(commandPool)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent command pool: {:p}", static_cast<void*>(commandPool)));
        }
    }
    void removeRenderPass(VkRenderPass renderPass) {
        if (renderPass == VK_NULL_HANDLE) return;
        auto it = std::find(renderPasses_.begin(), renderPasses_.end(), renderPass);
        if (it != renderPasses_.end()) {
            renderPasses_.erase(it);
            LOG_DEBUG(std::format("Removed render pass: {:p}", static_cast<void*>(renderPass)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent render pass: {:p}", static_cast<void*>(renderPass)));
        }
    }
    void removeDescriptorSetLayout(VkDescriptorSetLayout layout) {
        if (layout == VK_NULL_HANDLE) return;
        auto it = std::find(descriptorSetLayouts_.begin(), descriptorSetLayouts_.end(), layout);
        if (it != descriptorSetLayouts_.end()) {
            descriptorSetLayouts_.erase(it);
            LOG_DEBUG(std::format("Removed descriptor set layout: {:p}", static_cast<void*>(layout)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent descriptor set layout: {:p}", static_cast<void*>(layout)));
        }
    }
    void removePipelineLayout(VkPipelineLayout layout) {
        if (layout == VK_NULL_HANDLE) return;
        auto it = std::find(pipelineLayouts_.begin(), pipelineLayouts_.end(), layout);
        if (it != pipelineLayouts_.end()) {
            pipelineLayouts_.erase(it);
            LOG_DEBUG(std::format("Removed pipeline layout: {:p}", static_cast<void*>(layout)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent pipeline layout: {:p}", static_cast<void*>(layout)));
        }
    }
    void removePipeline(VkPipeline pipeline) {
        if (pipeline == VK_NULL_HANDLE) return;
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
    void removeShaderModule(VkShaderModule module) {
        if (module == VK_NULL_HANDLE) return;
        auto it = std::find(shaderModules_.begin(), shaderModules_.end(), module);
        if (it != shaderModules_.end()) {
            shaderModules_.erase(it);
            LOG_DEBUG(std::format("Removed shader module: {:p}", static_cast<void*>(module)));
        } else {
            LOG_WARNING(std::format("Attempted to remove non-existent shader module: {:p}", static_cast<void*>(module)));
        }
    }

    // --------------------- GETTERS ---------------------
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

    // --------------------- DEVICE & LOOKUP ---------------------
    void setDevice(VkDevice newDevice) {
        if (newDevice == VK_NULL_HANDLE) {
            LOG_ERROR("Cannot set null device to resource manager");
            throw std::invalid_argument("Cannot set null device");
        }
        device_ = newDevice;
        LOG_INFO(std::format("Resource manager device set: {:p}", static_cast<void*>(device_)));
    }
    VkDevice getDevice() const { return device_; }

    VkPipeline getPipeline(const std::string& name) const {
        auto it = pipelineMap_.find(name);
        if (it != pipelineMap_.end()) return it->second;
        LOG_WARNING(std::format("Pipeline '{}' not found", name));
        return VK_NULL_HANDLE;
    }

    // --------------------- FULL CLEANUP ---------------------
    void cleanup(VkDevice device = VK_NULL_HANDLE) {
        VkDevice effectiveDevice = (device == VK_NULL_HANDLE) ? device_ : device;
        if (effectiveDevice == VK_NULL_HANDLE) {
            LOG_WARNING("Device is null, skipping resource manager cleanup");
            return;
        }
        vkDeviceWaitIdle(effectiveDevice);
        LOG_DEBUG("Starting VulkanResourceManager cleanup");

        // Pipelines
        for (auto p : pipelines_) {
            if (p != VK_NULL_HANDLE) {
                vkDestroyPipeline(effectiveDevice, p, nullptr);
                LOG_INFO(std::format("Destroyed pipeline: {:p}", static_cast<void*>(p)));
            }
        }
        pipelines_.clear(); pipelineMap_.clear();

        // Pipeline layouts
        for (auto l : pipelineLayouts_) {
            if (l != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(effectiveDevice, l, nullptr);
                LOG_INFO(std::format("Destroyed pipeline layout: {:p}", static_cast<void*>(l)));
            }
        }
        pipelineLayouts_.clear();

        // Descriptor set layouts
        for (auto l : descriptorSetLayouts_) {
            if (l != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(effectiveDevice, l, nullptr);
                LOG_INFO(std::format("Destroyed descriptor set layout: {:p}", static_cast<void*>(l)));
            }
        }
        descriptorSetLayouts_.clear();

        // Render passes
        for (auto rp : renderPasses_) {
            if (rp != VK_NULL_HANDLE) {
                vkDestroyRenderPass(effectiveDevice, rp, nullptr);
                LOG_INFO(std::format("Destroyed render pass: {:p}", static_cast<void*>(rp)));
            }
        }
        renderPasses_.clear();

        // Shader modules
        for (auto m : shaderModules_) {
            if (m != VK_NULL_HANDLE) {
                vkDestroyShaderModule(effectiveDevice, m, nullptr);
                LOG_INFO(std::format("Destroyed shader module: {:p}", static_cast<void*>(m)));
            }
        }
        shaderModules_.clear();

        // Acceleration structures (use extension function)
        for (auto as : accelerationStructures_) {
            if (as != VK_NULL_HANDLE) {
                auto destroyFn = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
                    vkGetDeviceProcAddr(effectiveDevice, "vkDestroyAccelerationStructureKHR"));
                if (destroyFn) {
                    destroyFn(effectiveDevice, as, nullptr);
                    LOG_INFO(std::format("Destroyed acceleration structure: {:p}", static_cast<void*>(as)));
                }
            }
        }
        accelerationStructures_.clear();

        // Image views
        for (auto iv : imageViews_) {
            if (iv != VK_NULL_HANDLE) {
                vkDestroyImageView(effectiveDevice, iv, nullptr);
                LOG_INFO(std::format("Destroyed image view: {:p}", static_cast<void*>(iv)));
            }
        }
        imageViews_.clear();

        // Images
        for (auto img : images_) {
            if (img != VK_NULL_HANDLE) {
                vkDestroyImage(effectiveDevice, img, nullptr);
                LOG_INFO(std::format("Destroyed image: {:p}", static_cast<void*>(img)));
            }
        }
        images_.clear();

        // Buffers
        for (auto b : buffers_) {
            if (b != VK_NULL_HANDLE) {
                vkDestroyBuffer(effectiveDevice, b, nullptr);
                LOG_INFO(std::format("Destroyed buffer: {:p}", static_cast<void*>(b)));
            }
        }
        buffers_.clear();

        // Device memory
        for (auto mem : memories_) {
            if (mem != VK_NULL_HANDLE) {
                vkFreeMemory(effectiveDevice, mem, nullptr);
                LOG_INFO(std::format("Freed memory: {:p}", static_cast<void*>(mem)));
            }
        }
        memories_.clear();

        // Descriptor pools
        for (auto dp : descriptorPools_) {
            if (dp != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(effectiveDevice, dp, nullptr);
                LOG_INFO(std::format("Destroyed descriptor pool: {:p}", static_cast<void*>(dp)));
            }
        }
        descriptorPools_.clear();

        // Command pools
        for (auto cp : commandPools_) {
            if (cp != VK_NULL_HANDLE) {
                vkDestroyCommandPool(effectiveDevice, cp, nullptr);
                LOG_INFO(std::format("Destroyed command pool: {:p}", static_cast<void*>(cp)));
            }
        }
        commandPools_.clear();

        LOG_INFO("VulkanResourceManager cleanup completed");
    }
};

// ====================================================================
// Material Data (aligned for std140)
// ====================================================================
namespace VulkanRTX {
    struct alignas(16) MaterialData {
        alignas(16) glm::vec4 diffuse;
        alignas(4)  float specular;
        alignas(4)  float roughness;
        alignas(4)  float metallic;
        alignas(16) glm::vec4 emission;

        struct PushConstants {
            alignas(16) glm::vec4 clearColor;
            alignas(16) glm::vec3 cameraPosition;
            alignas(4)  float _pad0;
            alignas(16) glm::vec3 lightDirection;
            alignas(4)  float lightIntensity;
            alignas(4)  uint32_t samplesPerPixel;
            alignas(4)  uint32_t maxDepth;
            alignas(4)  uint32_t maxBounces;
            alignas(4)  float russianRoulette;
            alignas(8)  glm::vec2 resolution;
        };
    };
}

// ====================================================================
// Vulkan Context with Ray Tracing Function Pointers
// ====================================================================
namespace Vulkan {

struct Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

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
    VkExtent2D swapchainExtent = {0, 0};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

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

    VkDeviceAddress raygenSbtAddress = 0;
    VkDeviceAddress missSbtAddress = 0;
    VkDeviceAddress hitSbtAddress = 0;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchBufferMemory = VK_NULL_HANDLE;

    // ================================================================
    // RAY TRACING & BUFFER DEVICE ADDRESS EXTENSION FUNCTION POINTERS
    // ================================================================
    PFN_vkCmdTraceRaysKHR                       vkCmdTraceRaysKHR                       = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR          vkCreateRayTracingPipelinesKHR          = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR    vkGetRayTracingShaderGroupHandlesKHR    = nullptr;
    PFN_vkCreateAccelerationStructureKHR        vkCreateAccelerationStructureKHR        = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR     vkCmdBuildAccelerationStructuresKHR     = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkGetBufferDeviceAddressKHR             vkGetBufferDeviceAddressKHR             = nullptr;
    PFN_vkDestroyAccelerationStructureKHR       vkDestroyAccelerationStructureKHR       = nullptr;
};

} // namespace Vulkan

// ====================================================================
// Vulkan Initializer Functions
// ====================================================================
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
                     const VkMemoryAllocateFlagsInfo* allocFlagsInfo, VulkanResourceManager& resourceManager);

    void initializeVulkan(Vulkan::Context& context);

    VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer);

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
}

#endif // VULKAN_CORE_HPP