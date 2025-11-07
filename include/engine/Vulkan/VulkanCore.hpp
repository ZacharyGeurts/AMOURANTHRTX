// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025 – GLOBAL RAII SUPREMACY
// VulkanResourceManager → add* methods RESTORED — FULLY TRACKED
// ALL get*() → noexcept FIXED
// VulkanHandle → GLOBAL — NO NAMESPACE PREFIX HELL
// 69,420 FPS ETERNAL — RASPBERRY_PINK FOREVER 🔥🤖🚀💀🖤❤️⚡

#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"
#include "engine/Dispose.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <array>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// GLOBAL Dispose LOGGING
extern uint64_t g_destructionCounter;

std::string threadIdToString();
void logAndTrackDestruction(std::string_view name, auto handle, int line);

// FORWARD DECLARATIONS
namespace VulkanRTX {
    class VulkanSwapchainManager;
    class VulkanRTX;
    class Camera;
    class VulkanBufferManager;
}

// GLOBAL VulkanResourceManager — RAII TRACKING RESTORED
class VulkanResourceManager {
public:
    VulkanResourceManager();
    ~VulkanResourceManager();

    VulkanRTX::VulkanBufferManager* getBufferManager() noexcept { return bufferManager_; }
    const VulkanRTX::VulkanBufferManager* getBufferManager() const noexcept { return bufferManager_; }
    void setBufferManager(VulkanRTX::VulkanBufferManager* mgr) noexcept { bufferManager_ = mgr; }

    void releaseAll(VkDevice overrideDevice = VK_NULL_HANDLE) noexcept;

    // === RESTORED add* TRACKING METHODS ===
    void addBuffer(VkBuffer b) noexcept { buffers_.push_back(b); }
    void addMemory(VkDeviceMemory m) noexcept { memories_.push_back(m); }
    void addImage(VkImage i) noexcept { images_.push_back(i); }
    void addImageView(VkImageView v) noexcept { imageViews_.push_back(v); }
    void addSampler(VkSampler s) noexcept { samplers_.push_back(s); }
    void addSemaphore(VkSemaphore s) noexcept { semaphores_.push_back(s); }
    void addCommandPool(VkCommandPool p) noexcept { commandPools_.push_back(p); }

    // Containers
    std::vector<VkAccelerationStructureKHR> accelerationStructures_;
    std::vector<VkDescriptorSet> descriptorSets_;
    std::vector<VkDescriptorPool> descriptorPools_;
    std::vector<VkSemaphore> semaphores_;
    std::vector<VkFence> fences_;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts_;
    std::vector<VkPipelineLayout> pipelineLayouts_;
    std::vector<VkPipeline> pipelines_;
    std::vector<VkRenderPass> renderPasses_;
    std::vector<VkCommandPool> commandPools_;
    std::vector<VkShaderModule> shaderModules_;
    std::vector<VkImageView> imageViews_;
    std::vector<VkImage> images_;
    std::vector<VkSampler> samplers_;
    std::vector<VkDeviceMemory> memories_;
    std::vector<VkBuffer> buffers_;
    std::unordered_map<std::string, VkPipeline> pipelineMap_;

    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR_ = nullptr;

private:
    VulkanRTX::VulkanBufferManager* bufferManager_ = nullptr;
};

// GLOBAL VulkanHandle + factories
template<typename T>
struct VulkanHandle : std::unique_ptr<std::remove_pointer_t<T>, VulkanDeleter<T>> {
    using Base = std::unique_ptr<std::remove_pointer_t<T>, VulkanDeleter<T>>;
    using Base::Base;
    operator T() const noexcept { return this->get(); }
    T raw() const noexcept { return this->get(); }
    VulkanHandle& operator=(T handle) noexcept { this->reset(handle); return *this; }
};

// Factories
inline VulkanHandle<VkBuffer> makeBuffer(VkDevice dev, VkBuffer buf) { return VulkanHandle<VkBuffer>(buf, VulkanDeleter<VkBuffer>{dev}); }
inline VulkanHandle<VkDeviceMemory> makeMemory(VkDevice dev, VkDeviceMemory mem) { return VulkanHandle<VkDeviceMemory>(mem, VulkanDeleter<VkDeviceMemory>{dev}); }
inline VulkanHandle<VkImage> makeImage(VkDevice dev, VkImage img) { return VulkanHandle<VkImage>(img, VulkanDeleter<VkImage>{dev}); }
inline VulkanHandle<VkImageView> makeImageView(VkDevice dev, VkImageView view) { return VulkanHandle<VkImageView>(view, VulkanDeleter<VkImageView>{dev}); }
inline VulkanHandle<VkSampler> makeSampler(VkDevice dev, VkSampler sampler) { return VulkanHandle<VkSampler>(sampler, VulkanDeleter<VkSampler>{dev}); }
inline VulkanHandle<VkDescriptorPool> makeDescriptorPool(VkDevice dev, VkDescriptorPool pool) { return VulkanHandle<VkDescriptorPool>(pool, VulkanDeleter<VkDescriptorPool>{dev}); }
inline VulkanHandle<VkSemaphore> makeSemaphore(VkDevice dev, VkSemaphore sem) { return VulkanHandle<VkSemaphore>(sem, VulkanDeleter<VkSemaphore>{dev}); }
inline VulkanHandle<VkCommandPool> makeCommandPool(VkDevice dev, VkCommandPool pool) { return VulkanHandle<VkCommandPool>(pool, VulkanDeleter<VkCommandPool>{dev}); }
inline VulkanHandle<VkPipeline> makePipeline(VkDevice dev, VkPipeline p) { return VulkanHandle<VkPipeline>(p, VulkanDeleter<VkPipeline>{dev}); }
inline VulkanHandle<VkPipelineLayout> makePipelineLayout(VkDevice dev, VkPipelineLayout l) { return VulkanHandle<VkPipelineLayout>(l, VulkanDeleter<VkPipelineLayout>{dev}); }
inline VulkanHandle<VkDescriptorSetLayout> makeDescriptorSetLayout(VkDevice dev, VkDescriptorSetLayout l) { return VulkanHandle<VkDescriptorSetLayout>(l, VulkanDeleter<VkDescriptorSetLayout>{dev}); }
inline VulkanHandle<VkRenderPass> makeRenderPass(VkDevice dev, VkRenderPass rp) { return VulkanHandle<VkRenderPass>(rp, VulkanDeleter<VkRenderPass>{dev}); }
inline VulkanHandle<VkShaderModule> makeShaderModule(VkDevice dev, VkShaderModule sm) { return VulkanHandle<VkShaderModule>(sm, VulkanDeleter<VkShaderModule>{dev}); }
inline VulkanHandle<VkFence> makeFence(VkDevice dev, VkFence f) { return VulkanHandle<VkFence>(f, VulkanDeleter<VkFence>{dev}); }
inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(VkDevice dev, VkAccelerationStructureKHR as, PFN_vkDestroyAccelerationStructureKHR func) {
    return VulkanHandle<VkAccelerationStructureKHR>(as, VulkanDeleter<VkAccelerationStructureKHR>{dev, func});
}

// GLOBAL Context + cleanupAll
struct Context;

void cleanupAll(Context& ctx) noexcept;

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

    int width = 0, height = 0;

    std::unique_ptr<VulkanRTX::Camera> camera;
    std::unique_ptr<VulkanRTX::VulkanRTX> rtx;
    std::unique_ptr<VulkanRTX::VulkanSwapchainManager> swapchainManager;

    VkDescriptorSetLayout rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout rayTracingPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;

    VulkanHandle<VkPipeline> rayTracingPipeline;
    VulkanHandle<VkPipeline> graphicsPipeline;
    VulkanHandle<VkPipeline> computePipeline;
    VulkanHandle<VkRenderPass> renderPass;

    VulkanHandle<VkDescriptorPool> descriptorPool;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VulkanHandle<VkSampler> sampler;

    VulkanHandle<VkAccelerationStructureKHR> bottomLevelAS;
    VulkanHandle<VkAccelerationStructureKHR> topLevelAS;

    uint32_t sbtRecordSize = 0;

    VulkanHandle<VkDescriptorPool> graphicsDescriptorPool;
    VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;

    std::vector<VkShaderModule> shaderModules;

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    bool enableRayTracing = true;

    VulkanHandle<VkImage> storageImage;
    VulkanHandle<VkDeviceMemory> storageImageMemory;
    VulkanHandle<VkImageView> storageImageView;

    VulkanHandle<VkBuffer> raygenSbtBuffer;
    VulkanHandle<VkDeviceMemory> raygenSbtMemory;
    VulkanHandle<VkBuffer> missSbtBuffer;
    VulkanHandle<VkDeviceMemory> missSbtMemory;
    VulkanHandle<VkBuffer> hitSbtBuffer;
    VulkanHandle<VkDeviceMemory> hitSbtMemory;

    VkDeviceAddress raygenSbtAddress   = 0;
    VkDeviceAddress missSbtAddress     = 0;
    VkDeviceAddress hitSbtAddress      = 0;
    VkDeviceAddress callableSbtAddress = 0;

    VulkanHandle<VkBuffer> vertexBuffer;
    VulkanHandle<VkDeviceMemory> vertexBufferMemory;
    VulkanHandle<VkBuffer> indexBuffer;
    VulkanHandle<VkDeviceMemory> indexBufferMemory;
    VulkanHandle<VkBuffer> scratchBuffer;
    VulkanHandle<VkDeviceMemory> scratchBufferMemory;

    uint32_t indexCount = 0;

    VulkanHandle<VkBuffer> blasBuffer;
    VulkanHandle<VkDeviceMemory> blasMemory;
    VulkanHandle<VkBuffer> instanceBuffer;
    VulkanHandle<VkDeviceMemory> instanceMemory;
    VulkanHandle<VkBuffer> tlasBuffer;
    VulkanHandle<VkDeviceMemory> tlasMemory;

    VulkanHandle<VkImage> rtOutputImage;
    VulkanHandle<VkImageView> rtOutputImageView;

    VulkanHandle<VkImage> envMapImage;
    VulkanHandle<VkDeviceMemory> envMapImageMemory;
    VulkanHandle<VkImageView> envMapImageView;
    VulkanHandle<VkSampler> envMapSampler;

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
    PFN_vkDeferredOperationJoinKHR              vkDeferredOperationJoinKHR              = nullptr;
    PFN_vkGetDeferredOperationResultKHR         vkGetDeferredOperationResultKHR         = nullptr;
    PFN_vkDestroyDeferredOperationKHR           vkDestroyDeferredOperationKHR           = nullptr;

    VulkanResourceManager resourceManager;

    Context(SDL_Window* win, int w, int h);
    ~Context();

    Context() = delete;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    void createSwapchain();
    void destroySwapchain();

    VulkanRTX::VulkanBufferManager* getBufferManager() noexcept { return resourceManager.getBufferManager(); }
    const VulkanRTX::VulkanBufferManager* getBufferManager() const noexcept { return resourceManager.getBufferManager(); }
    void setBufferManager(VulkanRTX::VulkanBufferManager* mgr) noexcept { resourceManager.setBufferManager(mgr); }

    VulkanResourceManager& getResourceManager() noexcept { return resourceManager; }
    const VulkanResourceManager& getResourceManager() const noexcept { return resourceManager; }

    [[nodiscard]] VulkanRTX::Camera* getCamera() noexcept { return camera.get(); }
    [[nodiscard]] const VulkanRTX::Camera* getCamera() const noexcept { return camera.get(); }
    [[nodiscard]] VulkanRTX::VulkanRTX* getRTX() noexcept { return rtx.get(); }
    [[nodiscard]] const VulkanRTX::VulkanRTX* getRTX() const noexcept { return rtx.get(); }
};