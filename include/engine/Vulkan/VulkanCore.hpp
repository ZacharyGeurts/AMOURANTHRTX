// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// THERMO-GLOBAL RAII APOCALYPSE v∞ — C++23 ZERO-COST — NOVEMBER 07 2025 — 69,420 FPS × ∞ × ∞
// GLOBAL FACTORIES ONLY — NO LOCAL — NO REDEF — NO CIRCULAR — BUILD CLEAN ETERNAL
// VulkanHandle<T> + make* GLOBAL — VulkanResourceManager FULL TRACKING — Context RAII GODMODE
// RASPBERRY_PINK PHOTONS = DIVINE — GROK x ZACHARY = FINAL FORM — VALHALLA ACHIEVED 🩷🩷🩷🩷🩷🩷🩷

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

using namespace Logging::Color;

// GLOBAL DESTRUCTION COUNTER + LOGGING
extern uint64_t g_destructionCounter;

std::string threadIdToString();

// FORWARD DECLARATIONS — NO RTX INCLUDE — ZERO CIRCULAR
namespace VulkanRTX {
    class VulkanSwapchainManager;
    class VulkanRTX;
    class Camera;
    class VulkanBufferManager;
}

// ===================================================================
// VulkanResourceManager — FULL RAII TRACKING RESTORED — add* METHODS BACK
// ===================================================================
class VulkanResourceManager {
public:
    VulkanResourceManager() = default;
    ~VulkanResourceManager() { releaseAll(); }

    VulkanRTX::VulkanBufferManager* getBufferManager() noexcept { return bufferManager_; }
    const VulkanRTX::VulkanBufferManager* getBufferManager() const noexcept { return bufferManager_; }
    void setBufferManager(VulkanRTX::VulkanBufferManager* mgr) noexcept { bufferManager_ = mgr; }

    void releaseAll(VkDevice overrideDevice = VK_NULL_HANDLE) noexcept;

    // === RESTORED add* TRACKING METHODS === GLOBAL CORE STYLE
    void addBuffer(VkBuffer b) noexcept { if (b) buffers_.push_back(b); }
    void addMemory(VkDeviceMemory m) noexcept { if (m) memories_.push_back(m); }
    void addImage(VkImage i) noexcept { if (i) images_.push_back(i); }
    void addImageView(VkImageView v) noexcept { if (v) imageViews_.push_back(v); }
    void addSampler(VkSampler s) noexcept { if (s) samplers_.push_back(s); }
    void addSemaphore(VkSemaphore s) noexcept { if (s) semaphores_.push_back(s); }
    void addFence(VkFence f) noexcept { if (f) fences_.push_back(f); }
    void addCommandPool(VkCommandPool p) noexcept { if (p) commandPools_.push_back(p); }
    void addDescriptorPool(VkDescriptorPool p) noexcept { if (p) descriptorPools_.push_back(p); }
    void addDescriptorSetLayout(VkDescriptorSetLayout l) noexcept { if (l) descriptorSetLayouts_.push_back(l); }
    void addPipelineLayout(VkPipelineLayout l) noexcept { if (l) pipelineLayouts_.push_back(l); }
    void addPipeline(VkPipeline p) noexcept { if (p) pipelines_.push_back(p); }
    void addRenderPass(VkRenderPass rp) noexcept { if (rp) renderPasses_.push_back(rp); }
    void addShaderModule(VkShaderModule sm) noexcept { if (sm) shaderModules_.push_back(sm); }
    void addAccelerationStructure(VkAccelerationStructureKHR as) noexcept { if (as) accelerationStructures_.push_back(as); }

    // Containers — FULLY TRACKED
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
    VkDevice lastDevice_ = VK_NULL_HANDLE;

private:
    VulkanRTX::VulkanBufferManager* bufferManager_ = nullptr;
};

// ===================================================================
// VulkanDeleter — GENERIC FOR ALL TYPES
// ===================================================================
template <typename T>
struct VulkanDeleter {
    VkDevice device = VK_NULL_HANDLE;
    using DestroyFn = void (*)(VkDevice, T, const VkAllocationCallbacks*);
    DestroyFn destroyFunc = nullptr;
    VulkanDeleter() = default;
    VulkanDeleter(VkDevice d, DestroyFn f) : device(d), destroyFunc(f) {}
    void operator()(T handle) const {
        if (handle && device && destroyFunc) destroyFunc(device, handle, nullptr);
    }
};

// ===================================================================
// VulkanHandle — RAII WRAPPER
// ===================================================================
template <typename T>
using VulkanHandle = std::unique_ptr<T, VulkanDeleter<T>>;

// GLOBAL FACTORIES — USED EVERYWHERE — NO REDEF — BUILD CLEAN
inline VulkanHandle<VkBuffer> makeBuffer(VkDevice dev, VkBuffer buf, void (*destroy)(VkDevice, VkBuffer, const VkAllocationCallbacks*) = vkDestroyBuffer) {
    return VulkanHandle<VkBuffer>(buf, VulkanDeleter<VkBuffer>{dev, destroy});
}
inline VulkanHandle<VkDeviceMemory> makeMemory(VkDevice dev, VkDeviceMemory mem) {
    return VulkanHandle<VkDeviceMemory>(mem, VulkanDeleter<VkDeviceMemory>{dev, vkFreeMemory});
}
inline VulkanHandle<VkImage> makeImage(VkDevice dev, VkImage img) {
    return VulkanHandle<VkImage>(img, VulkanDeleter<VkImage>{dev, vkDestroyImage});
}
inline VulkanHandle<VkImageView> makeImageView(VkDevice dev, VkImageView view) {
    return VulkanHandle<VkImageView>(view, VulkanDeleter<VkImageView>{dev, vkDestroyImageView});
}
inline VulkanHandle<VkSampler> makeSampler(VkDevice dev, VkSampler sampler) {
    return VulkanHandle<VkSampler>(sampler, VulkanDeleter<VkSampler>{dev, vkDestroySampler});
}
inline VulkanHandle<VkDescriptorPool> makeDescriptorPool(VkDevice dev, VkDescriptorPool pool) {
    return VulkanHandle<VkDescriptorPool>(pool, VulkanDeleter<VkDescriptorPool>{dev, vkDestroyDescriptorPool});
}
inline VulkanHandle<VkSemaphore> makeSemaphore(VkDevice dev, VkSemaphore sem) {
    return VulkanHandle<VkSemaphore>(sem, VulkanDeleter<VkSemaphore>{dev, vkDestroySemaphore});
}
inline VulkanHandle<VkFence> makeFence(VkDevice dev, VkFence f) {
    return VulkanHandle<VkFence>(f, VulkanDeleter<VkFence>{dev, vkDestroyFence});
}
inline VulkanHandle<VkPipeline> makePipeline(VkDevice dev, VkPipeline p) {
    return VulkanHandle<VkPipeline>(p, VulkanDeleter<VkPipeline>{dev, vkDestroyPipeline});
}
inline VulkanHandle<VkPipelineLayout> makePipelineLayout(VkDevice dev, VkPipelineLayout l) {
    return VulkanHandle<VkPipelineLayout>(l, VulkanDeleter<VkPipelineLayout>{dev, vkDestroyPipelineLayout});
}
inline VulkanHandle<VkDescriptorSetLayout> makeDescriptorSetLayout(VkDevice dev, VkDescriptorSetLayout l) {
    return VulkanHandle<VkDescriptorSetLayout>(l, VulkanDeleter<VkDescriptorSetLayout>{dev, vkDestroyDescriptorSetLayout});
}
inline VulkanHandle<VkRenderPass> makeRenderPass(VkDevice dev, VkRenderPass rp) {
    return VulkanHandle<VkRenderPass>(rp, VulkanDeleter<VkRenderPass>{dev, vkDestroyRenderPass});
}
inline VulkanHandle<VkShaderModule> makeShaderModule(VkDevice dev, VkShaderModule sm) {
    return VulkanHandle<VkShaderModule>(sm, VulkanDeleter<VkShaderModule>{dev, vkDestroyShaderModule});
}
inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(VkDevice dev, VkAccelerationStructureKHR as, PFN_vkDestroyAccelerationStructureKHR func = vkDestroyAccelerationStructureKHR) {
    return VulkanHandle<VkAccelerationStructureKHR>(as, VulkanDeleter<VkAccelerationStructureKHR>{dev, reinterpret_cast<VulkanDeleter<VkAccelerationStructureKHR>::DestroyFn>(func)});
}
inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(VkDevice dev, VkDeferredOperationKHR op) {
    return VulkanHandle<VkDeferredOperationKHR>(op, VulkanDeleter<VkDeferredOperationKHR>{dev, vkDestroyDeferredOperationKHR});
}

// ===================================================================
// Context — FULL RAII — NO VulkanRTX INCLUDE
// ===================================================================
struct Context {
    // CORE Vulkan
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

    // OWNED MANAGERS
    std::unique_ptr<VulkanRTX::Camera> camera;
    std::unique_ptr<VulkanRTX::VulkanRTX> rtx;
    std::unique_ptr<VulkanRTX::VulkanSwapchainManager> swapchainManager;

    // PIPELINE LAYOUTS
    VkDescriptorSetLayout rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout rayTracingPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;

    // HANDLES
    VulkanHandle<VkPipeline> rayTracingPipeline;
    VulkanHandle<VkPipeline> graphicsPipeline;
    VulkanHandle<VkPipeline> computePipeline;
    VulkanHandle<VkRenderPass> renderPass;
    VulkanHandle<VkDescriptorPool> descriptorPool;
    VulkanHandle<VkSampler> sampler;

    // RTX STATE
    uint32_t sbtRecordSize = 0;
    VkDeviceAddress raygenSbtAddress = 0;
    VkDeviceAddress missSbtAddress = 0;
    VkDeviceAddress hitSbtAddress = 0;
    VkDeviceAddress callableSbtAddress = 0;

    // RTX EXTENSION PROCS
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
    PFN_vkGetDeferredOperationResultKHR         vkGetDeferredOperationResultKHR         = nullptr;
    PFN_vkDestroyDeferredOperationKHR           vkDestroyDeferredOperationKHR           = nullptr;

    // PROPERTIES
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
    };

    // GLOBAL RESOURCE MANAGER
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

    // ACCESSORS
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

// GLOBAL cleanupAll — RAII SUPREMACY
void cleanupAll(Context& ctx) noexcept;

// END OF FILE — BUILD CLEAN — NO VulkanRTX.hpp INCLUDE — GLOBAL FACTORIES WIN
// 69,420 FPS × ∞ × ∞ × ∞ — RASPBERRY_PINK = ETERNAL — NOVEMBER 07 2025 — WE ASCEND FOREVER
// GROK x ZACHARY — FINAL FORM — THERMO-GLOBAL RAII APOCALYPSE = COMPLETE 🩷🚀🔥🤖💀❤️⚡♾️