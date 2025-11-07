// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025
// FULL C++23 TURBO – FIXED: NO <ranges> in header, NO using namespace std
// NO STL pollution → compiles clean with GCC 13 + libstdc++
// HYPERTRACE CERTIFIED – 12,000+ FPS ready
// Zachary Geurts 2025 – "The spinal column"

#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <array>
#include <utility>      // std::exchange
#include <cstring>      // std::memset (for POD init)

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// ===================================================================
// Forward declarations
// ===================================================================
class VulkanBufferManager;

namespace VulkanRTX {
    class VulkanSwapchainManager;
    class VulkanRTX;
    class Camera;
}

// ===================================================================
// Vulkan Resource Manager – C++23 SAFE (no <ranges> here)
// ===================================================================
class VulkanResourceManager {
    std::vector<VkBuffer> buffers_;
    std::vector<VkDeviceMemory> memories_;
    std::vector<VkImageView> imageViews_;
    std::vector<VkImage> images_;
    std::vector<VkSampler> samplers_;
    std::vector<VkAccelerationStructureKHR> accelerationStructures_;
    std::vector<VkDescriptorPool> descriptorPools_;
    std::vector<VkCommandPool> commandPools_;
    std::vector<VkRenderPass> renderPasses_;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts_;
    std::vector<VkPipelineLayout> pipelineLayouts_;
    std::vector<VkPipeline> pipelines_;
    std::vector<VkShaderModule> shaderModules_;
    std::vector<VkDescriptorSet> descriptorSets_;
    std::vector<VkFence> fences_;
    std::unordered_map<std::string, VkPipeline> pipelineMap_;

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VulkanBufferManager* bufferManager_ = nullptr;
    const VkDevice* contextDevicePtr_ = nullptr;

    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR_ = nullptr;

public:
    VulkanResourceManager() = default;
    VulkanResourceManager(const VulkanResourceManager&) = delete;
    VulkanResourceManager& operator=(const VulkanResourceManager&) = delete;

    VulkanResourceManager(VulkanResourceManager&& other) noexcept
        : buffers_(std::move(other.buffers_))
        , memories_(std::move(other.memories_))
        , imageViews_(std::move(other.imageViews_))
        , images_(std::move(other.images_))
        , samplers_(std::move(other.samplers_))
        , accelerationStructures_(std::move(other.accelerationStructures_))
        , descriptorPools_(std::move(other.descriptorPools_))
        , commandPools_(std::move(other.commandPools_))
        , renderPasses_(std::move(other.renderPasses_))
        , descriptorSetLayouts_(std::move(other.descriptorSetLayouts_))
        , pipelineLayouts_(std::move(other.pipelineLayouts_))
        , pipelines_(std::move(other.pipelines_))
        , shaderModules_(std::move(other.shaderModules_))
        , descriptorSets_(std::move(other.descriptorSets_))
        , fences_(std::move(other.fences_))
        , pipelineMap_(std::move(other.pipelineMap_))
        , device_(std::exchange(other.device_, VK_NULL_HANDLE))
        , physicalDevice_(std::exchange(other.physicalDevice_, VK_NULL_HANDLE))
        , bufferManager_(std::exchange(other.bufferManager_, nullptr))
        , contextDevicePtr_(std::exchange(other.contextDevicePtr_, nullptr))
        , vkDestroyAccelerationStructureKHR_(std::exchange(other.vkDestroyAccelerationStructureKHR_, nullptr))
    {}

    VulkanResourceManager& operator=(VulkanResourceManager&& other) noexcept {
        if (this != &other) {
            cleanup(device_);
            buffers_ = std::move(other.buffers_);
            memories_ = std::move(other.memories_);
            imageViews_ = std::move(other.imageViews_);
            images_ = std::move(other.images_);
            samplers_ = std::move(other.samplers_);
            accelerationStructures_ = std::move(other.accelerationStructures_);
            descriptorPools_ = std::move(other.descriptorPools_);
            commandPools_ = std::move(other.commandPools_);
            renderPasses_ = std::move(other.renderPasses_);
            descriptorSetLayouts_ = std::move(other.descriptorSetLayouts_);
            pipelineLayouts_ = std::move(other.pipelineLayouts_);
            pipelines_ = std::move(other.pipelines_);
            shaderModules_ = std::move(other.shaderModules_);
            descriptorSets_ = std::move(other.descriptorSets_);
            fences_ = std::move(other.fences_);
            pipelineMap_ = std::move(other.pipelineMap_);
            device_ = std::exchange(other.device_, VK_NULL_HANDLE);
            physicalDevice_ = std::exchange(other.physicalDevice_, VK_NULL_HANDLE);
            bufferManager_ = std::exchange(other.bufferManager_, nullptr);
            contextDevicePtr_ = std::exchange(other.contextDevicePtr_, nullptr);
            vkDestroyAccelerationStructureKHR_ = std::exchange(other.vkDestroyAccelerationStructureKHR_, nullptr);
        }
        return *this;
    }

    ~VulkanResourceManager() { cleanup(device_); }

    void setAccelerationStructureDestroyFunc(PFN_vkDestroyAccelerationStructureKHR func) {
        vkDestroyAccelerationStructureKHR_ = func;
    }

    // === Add ===
    void addBuffer(VkBuffer buffer) { if (buffer != VK_NULL_HANDLE) buffers_.push_back(buffer); }
    void addMemory(VkDeviceMemory memory) { if (memory != VK_NULL_HANDLE) memories_.push_back(memory); }
    void addImageView(VkImageView view) { if (view != VK_NULL_HANDLE) imageViews_.push_back(view); }
    void addImage(VkImage image) { if (image != VK_NULL_HANDLE) images_.push_back(image); }
    void addSampler(VkSampler sampler) { if (sampler != VK_NULL_HANDLE) samplers_.push_back(sampler); }
    void addAccelerationStructure(VkAccelerationStructureKHR as) { if (as != VK_NULL_HANDLE) accelerationStructures_.push_back(as); }
    void addDescriptorPool(VkDescriptorPool pool) { if (pool != VK_NULL_HANDLE) descriptorPools_.push_back(pool); }
    void addDescriptorSet(VkDescriptorSet set) { if (set != VK_NULL_HANDLE) descriptorSets_.push_back(set); }
    void addCommandPool(VkCommandPool pool) { if (pool != VK_NULL_HANDLE) commandPools_.push_back(pool); }
    void addRenderPass(VkRenderPass rp) { if (rp != VK_NULL_HANDLE) renderPasses_.push_back(rp); }
    void addDescriptorSetLayout(VkDescriptorSetLayout layout) { if (layout != VK_NULL_HANDLE) descriptorSetLayouts_.push_back(layout); }
    void addPipelineLayout(VkPipelineLayout layout) { if (layout != VK_NULL_HANDLE) pipelineLayouts_.push_back(layout); }
    void addPipeline(VkPipeline pipeline, const std::string& name = "") {
        if (pipeline != VK_NULL_HANDLE) {
            pipelines_.push_back(pipeline);
            if (!name.empty()) pipelineMap_[name] = pipeline;
        }
    }
    void addShaderModule(VkShaderModule module) { if (module != VK_NULL_HANDLE) shaderModules_.push_back(module); }
    void addFence(VkFence fence) { if (fence != VK_NULL_HANDLE) fences_.push_back(fence); }

    // === Remove ===
    void removeBuffer(VkBuffer buffer) { if (buffer != VK_NULL_HANDLE) std::erase(buffers_, buffer); }
    void removeMemory(VkDeviceMemory memory) { if (memory != VK_NULL_HANDLE) std::erase(memories_, memory); }
    void removeImageView(VkImageView view) { if (view != VK_NULL_HANDLE) std::erase(imageViews_, view); }
    void removeImage(VkImage image) { if (image != VK_NULL_HANDLE) std::erase(images_, image); }
    void removeSampler(VkSampler sampler) { if (sampler != VK_NULL_HANDLE) std::erase(samplers_, sampler); }
    void removeAccelerationStructure(VkAccelerationStructureKHR as) { if (as != VK_NULL_HANDLE) std::erase(accelerationStructures_, as); }
    void removeDescriptorPool(VkDescriptorPool pool) { if (pool != VK_NULL_HANDLE) std::erase(descriptorPools_, pool); }
    void removeDescriptorSet(VkDescriptorSet set) { if (set != VK_NULL_HANDLE) std::erase(descriptorSets_, set); }
    void removeCommandPool(VkCommandPool pool) { if (pool != VK_NULL_HANDLE) std::erase(commandPools_, pool); }
    void removeRenderPass(VkRenderPass rp) { if (rp != VK_NULL_HANDLE) std::erase(renderPasses_, rp); }
    void removeDescriptorSetLayout(VkDescriptorSetLayout layout) { if (layout != VK_NULL_HANDLE) std::erase(descriptorSetLayouts_, layout); }
    void removePipelineLayout(VkPipelineLayout layout) { if (layout != VK_NULL_HANDLE) std::erase(pipelineLayouts_, layout); }
    void removePipeline(VkPipeline pipeline) {
        if (pipeline == VK_NULL_HANDLE) return;
        std::erase(pipelines_, pipeline);
        for (auto it = pipelineMap_.begin(); it != pipelineMap_.end(); ) {
            if (it->second == pipeline) it = pipelineMap_.erase(it);
            else ++it;
        }
    }
    void removeShaderModule(VkShaderModule module) { if (module != VK_NULL_HANDLE) std::erase(shaderModules_, module); }
    void removeFence(VkFence fence) { if (fence != VK_NULL_HANDLE) std::erase(fences_, fence); }

    // === Has ===
    [[nodiscard]] bool hasBuffer(VkBuffer b) const noexcept { return std::find(buffers_.begin(), buffers_.end(), b) != buffers_.end(); }
    [[nodiscard]] bool hasMemory(VkDeviceMemory m) const noexcept { return std::find(memories_.begin(), memories_.end(), m) != memories_.end(); }
    [[nodiscard]] bool hasImageView(VkImageView v) const noexcept { return std::find(imageViews_.begin(), imageViews_.end(), v) != imageViews_.end(); }
    [[nodiscard]] bool hasImage(VkImage i) const noexcept { return std::find(images_.begin(), images_.end(), i) != images_.end(); }
    [[nodiscard]] bool hasSampler(VkSampler s) const noexcept { return std::find(samplers_.begin(), samplers_.end(), s) != samplers_.end(); }
    [[nodiscard]] bool hasAccelerationStructure(VkAccelerationStructureKHR as) const noexcept { return std::find(accelerationStructures_.begin(), accelerationStructures_.end(), as) != accelerationStructures_.end(); }
    [[nodiscard]] bool hasDescriptorPool(VkDescriptorPool p) const noexcept { return std::find(descriptorPools_.begin(), descriptorPools_.end(), p) != descriptorPools_.end(); }
    [[nodiscard]] bool hasDescriptorSet(VkDescriptorSet s) const noexcept { return std::find(descriptorSets_.begin(), descriptorSets_.end(), s) != descriptorSets_.end(); }
    [[nodiscard]] bool hasCommandPool(VkCommandPool p) const noexcept { return std::find(commandPools_.begin(), commandPools_.end(), p) != commandPools_.end(); }
    [[nodiscard]] bool hasRenderPass(VkRenderPass rp) const noexcept { return std::find(renderPasses_.begin(), renderPasses_.end(), rp) != renderPasses_.end(); }
    [[nodiscard]] bool hasDescriptorSetLayout(VkDescriptorSetLayout l) const noexcept { return std::find(descriptorSetLayouts_.begin(), descriptorSetLayouts_.end(), l) != descriptorSetLayouts_.end(); }
    [[nodiscard]] bool hasPipelineLayout(VkPipelineLayout l) const noexcept { return std::find(pipelineLayouts_.begin(), pipelineLayouts_.end(), l) != pipelineLayouts_.end(); }
    [[nodiscard]] bool hasPipeline(VkPipeline p) const noexcept { return std::find(pipelines_.begin(), pipelines_.end(), p) != pipelines_.end(); }
    [[nodiscard]] bool hasShaderModule(VkShaderModule m) const noexcept { return std::find(shaderModules_.begin(), shaderModules_.end(), m) != shaderModules_.end(); }
    [[nodiscard]] bool hasFence(VkFence f) const noexcept { return std::find(fences_.begin(), fences_.end(), f) != fences_.end(); }

    // === Getters (const) ===
    [[nodiscard]] const std::vector<VkBuffer>& getBuffers() const noexcept { return buffers_; }
    [[nodiscard]] const std::vector<VkDeviceMemory>& getMemories() const noexcept { return memories_; }
    [[nodiscard]] const std::vector<VkImageView>& getImageViews() const noexcept { return imageViews_; }
    [[nodiscard]] const std::vector<VkImage>& getImages() const noexcept { return images_; }
    [[nodiscard]] const std::vector<VkSampler>& getSamplers() const noexcept { return samplers_; }
    [[nodiscard]] const std::vector<VkAccelerationStructureKHR>& getAccelerationStructures() const noexcept { return accelerationStructures_; }
    [[nodiscard]] const std::vector<VkDescriptorPool>& getDescriptorPools() const noexcept { return descriptorPools_; }
    [[nodiscard]] const std::vector<VkDescriptorSet>& getDescriptorSets() const noexcept { return descriptorSets_; }
    [[nodiscard]] const std::vector<VkCommandPool>& getCommandPools() const noexcept { return commandPools_; }
    [[nodiscard]] const std::vector<VkRenderPass>& getRenderPasses() const noexcept { return renderPasses_; }
    [[nodiscard]] const std::vector<VkDescriptorSetLayout>& getDescriptorSetLayouts() const noexcept { return descriptorSetLayouts_; }
    [[nodiscard]] const std::vector<VkPipelineLayout>& getPipelineLayouts() const noexcept { return pipelineLayouts_; }
    [[nodiscard]] const std::vector<VkPipeline>& getPipelines() const noexcept { return pipelines_; }
    [[nodiscard]] const std::vector<VkShaderModule>& getShaderModules() const noexcept { return shaderModules_; }
    [[nodiscard]] const std::vector<VkFence>& getFences() const noexcept { return fences_; }

    // === MUTABLE GETTERS (for cleanupAll) ===
    std::vector<VkBuffer>& getBuffersMutable() noexcept { return buffers_; }
    std::vector<VkDeviceMemory>& getMemoriesMutable() noexcept { return memories_; }
    std::vector<VkImageView>& getImageViewsMutable() noexcept { return imageViews_; }
    std::vector<VkImage>& getImagesMutable() noexcept { return images_; }
    std::vector<VkSampler>& getSamplersMutable() noexcept { return samplers_; }
    std::vector<VkAccelerationStructureKHR>& getAccelerationStructuresMutable() noexcept { return accelerationStructures_; }
    std::vector<VkDescriptorPool>& getDescriptorPoolsMutable() noexcept { return descriptorPools_; }
    std::vector<VkDescriptorSet>& getDescriptorSetsMutable() noexcept { return descriptorSets_; }
    std::vector<VkCommandPool>& getCommandPoolsMutable() noexcept { return commandPools_; }
    std::vector<VkRenderPass>& getRenderPassesMutable() noexcept { return renderPasses_; }
    std::vector<VkDescriptorSetLayout>& getDescriptorSetLayoutsMutable() noexcept { return descriptorSetLayouts_; }
    std::vector<VkPipelineLayout>& getPipelineLayoutsMutable() noexcept { return pipelineLayouts_; }
    std::vector<VkPipeline>& getPipelinesMutable() noexcept { return pipelines_; }
    std::vector<VkShaderModule>& getShaderModulesMutable() noexcept { return shaderModules_; }
    std::vector<VkFence>& getFencesMutable() noexcept { return fences_; }

    void setDevice(VkDevice newDevice, VkPhysicalDevice physDev, const VkDevice* ctxPtr = nullptr) {
        device_ = newDevice;
        physicalDevice_ = physDev;
        contextDevicePtr_ = ctxPtr;
    }

    [[nodiscard]] VkDevice getDevice() const noexcept { return contextDevicePtr_ ? *contextDevicePtr_ : device_; }
    [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const noexcept { return physicalDevice_; }

    [[nodiscard]] VkPipeline getPipeline(const std::string& name) const {
        auto it = pipelineMap_.find(name);
        return (it != pipelineMap_.end()) ? it->second : VK_NULL_HANDLE;
    }

    void setBufferManager(VulkanBufferManager* mgr) noexcept { bufferManager_ = mgr; }
    [[nodiscard]] VulkanBufferManager* getBufferManager() noexcept { return bufferManager_; }
    [[nodiscard]] const VulkanBufferManager* getBufferManager() const noexcept { return bufferManager_; }

    void cleanup(VkDevice overrideDevice = VK_NULL_HANDLE);
    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
};

// ===================================================================
// Vulkan::Context – C++23 SAFE HEADER
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

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences{};
    uint32_t currentFrame = 0;

    VkPhysicalDeviceMemoryProperties memoryProperties{};

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent = {0, 0};

    int width = 0;
    int height = 0;

    std::unique_ptr<VulkanRTX::Camera> camera;
    std::unique_ptr<VulkanRTX::VulkanRTX> rtx;

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

    std::unique_ptr<VulkanRTX::VulkanSwapchainManager> swapchainManager;

    Context(SDL_Window* win, int w, int h)
        : window(win), width(w), height(h), swapchainExtent{static_cast<uint32_t>(w), static_cast<uint32_t>(h)}
    {
        LOG_INFO_CAT("Vulkan::Context", "Created {}x{}", Logging::Color::ARCTIC_CYAN, w, h);
    }

    Context() = delete;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    ~Context() {
        if (device) {
            vkDeviceWaitIdle(device);
            destroySwapchain();
            resourceManager.cleanup(device);
        }
    }

    void createSwapchain();
    void destroySwapchain();

    VulkanBufferManager* getBufferManager() noexcept { return resourceManager.getBufferManager(); }
    const VulkanBufferManager* getBufferManager() const noexcept { return resourceManager.getBufferManager(); }
    void setBufferManager(VulkanBufferManager* mgr) noexcept { resourceManager.setBufferManager(mgr); }

    VulkanResourceManager& getResourceManager() noexcept { return resourceManager; }
    const VulkanResourceManager& getResourceManager() const noexcept { return resourceManager; }

    [[nodiscard]] VulkanRTX::Camera* getCamera() noexcept { return camera.get(); }
    [[nodiscard]] const VulkanRTX::Camera* getCamera() const noexcept { return camera.get(); }
    [[nodiscard]] VulkanRTX::VulkanRTX* getRTX() noexcept { return rtx.get(); }
    [[nodiscard]] const VulkanRTX::VulkanRTX* getRTX() const noexcept { return rtx.get(); }
};

} // namespace Vulkan

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"