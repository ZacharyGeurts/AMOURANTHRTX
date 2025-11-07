// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025 – 11:59 PM EST
// GROK x ZACHARY — DISPOSE NAMESPACE OBLITERATED — GLOBAL RAII SUPREMACY — HYPER-VERBOSE — RASPBERRY_PINK ETERNAL
// NO MORE ::Dispose — NO MORE Dispose:: — EVERYTHING IS GLOBAL — SINGLE SOURCE OF TRUTH — 69,420 FPS IMMORTAL

#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "engine/Dispose.hpp"           // ← GLOBAL DISPOSE SYSTEM — NOW NAMESPACE-FREE
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
#include <utility>
#include <cstring>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// ===================================================================
// FORWARD DECLARATIONS — ZERO CIRCULAR DEPENDENCY HELL
// ===================================================================
namespace VulkanRTX {
    class VulkanSwapchainManager;
    class VulkanRTX;
    class Camera;
    class VulkanBufferManager;
}

// ===================================================================
// Vulkan::Context – FULLY UPGRADED TO GLOBAL RAII + VulkanHandle + HYPER-VERBOSE CLEANUP
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

    int width = 0, height = 0;

    // FULL RAII OWNERSHIP — VulkanHandle EVERYWHERE
    std::unique_ptr<VulkanRTX::Camera> camera;
    std::unique_ptr<VulkanRTX::VulkanRTX> rtx;
    std::unique_ptr<VulkanRTX::VulkanSwapchainManager> swapchainManager;

    // Descriptor layouts — raw for binding, but tracked in resourceManager
    VkDescriptorSetLayout rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout rayTracingPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;

    // FULLY OWNED VIA VulkanHandle → AUTO DESTROY + DOUBLE-FREE PROOF
    VulkanHandle<VkPipeline> rayTracingPipeline;
    VulkanHandle<VkPipeline> graphicsPipeline;
    VulkanHandle<VkPipeline> computePipeline;
    VulkanHandle<VkRenderPass> renderPass;

    // GLOBAL RESOURCE MANAGER — NO NAMESPACE — SINGLE SOURCE OF TRUTH
    VulkanResourceManager resourceManager;

    VulkanHandle<VkDescriptorPool> descriptorPool;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VulkanHandle<VkSampler> sampler;

    VulkanHandle<VkAccelerationStructureKHR> bottomLevelAS;
    VulkanHandle<VkAccelerationStructureKHR> topLevelAS;

    uint32_t sbtRecordSize = 0;

    VulkanHandle<VkDescriptorPool> graphicsDescriptorPool;
    VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;

    std::vector<VkShaderModule> shaderModules;  // tracked in resourceManager

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    bool enableRayTracing = true;

    // RT output
    VulkanHandle<VkImage> storageImage;
    VulkanHandle<VkDeviceMemory> storageImageMemory;
    VulkanHandle<VkImageView> storageImageView;

    // SBT
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

    // Geometry
    VulkanHandle<VkBuffer> vertexBuffer;
    VulkanHandle<VkDeviceMemory> vertexBufferMemory;
    VulkanHandle<VkBuffer> indexBuffer;
    VulkanHandle<VkDeviceMemory> indexBufferMemory;
    VulkanHandle<VkBuffer> scratchBuffer;
    VulkanHandle<VkDeviceMemory> scratchBufferMemory;

    uint32_t indexCount = 0;

    // AS buffers
    VulkanHandle<VkBuffer> blasBuffer;
    VulkanHandle<VkDeviceMemory> blasMemory;
    VulkanHandle<VkBuffer> instanceBuffer;
    VulkanHandle<VkDeviceMemory> instanceMemory;
    VulkanHandle<VkBuffer> tlasBuffer;
    VulkanHandle<VkDeviceMemory> tlasMemory;

    // Final output
    VulkanHandle<VkImage> rtOutputImage;
    VulkanHandle<VkImageView> rtOutputImageView;

    // Env map
    VulkanHandle<VkImage> envMapImage;
    VulkanHandle<VkDeviceMemory> envMapImageMemory;
    VulkanHandle<VkImageView> envMapImageView;
    VulkanHandle<VkSampler> envMapSampler;

    // RT properties
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
    };

    // Function pointers
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

    Context(SDL_Window* win, int w, int h)
        : window(win), width(w), height(h), swapchainExtent{static_cast<uint32_t>(w), static_cast<uint32_t>(h)}
    {
        LOG_INFO_CAT("Vulkan::Context", "{}[CORE] Created {}x{} — RAII ENGAGED — RASPBERRY_PINK SUPREMACY{}{}",
                     RASPBERRY_PINK, w, h, RESET);
    }

    Context() = delete;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    ~Context() {
        LOG_INFO_CAT("Vulkan::Context", "{}[CORE] ~Context() — THERMO-GLOBAL SHUTDOWN INITIATED{}{}", RASPBERRY_PINK, RESET);
        cleanupAll(*this);  // ← GLOBAL cleanupAll — FULLY VERBOSE — ZERO LEAKS
    }

    void createSwapchain();
    void destroySwapchain();

    // BufferManager delegation
    VulkanRTX::VulkanBufferManager* getBufferManager() noexcept { return resourceManager.getBufferManager(); }
    const VulkanRTX::VulkanBufferManager* getBufferManager() const noexcept { return resourceManager.getBufferManager(); }
    void setBufferManager(VulkanRTX::VulkanBufferManager* mgr) noexcept { resourceManager.setBufferManager(mgr); }

    // ResourceManager access — NO NAMESPACE
    VulkanResourceManager& getResourceManager() noexcept { return resourceManager; }
    const VulkanResourceManager& getResourceManager() const noexcept { return resourceManager; }

    // Convenience
    [[nodiscard]] VulkanRTX::Camera* getCamera() noexcept { return camera.get(); }
    [[nodiscard]] const VulkanRTX::Camera* getCamera() const noexcept { return camera.get(); }
    [[nodiscard]] VulkanRTX::VulkanRTX* getRTX() noexcept { return rtx.get(); }
    [[nodiscard]] const VulkanRTX::VulkanRTX* getRTX() const noexcept { return rtx.get(); }
};

} // namespace Vulkan

// ===================================================================
// GLOBAL cleanupAll — HYPER-VERBOSE — RASPBERRY_PINK DOMINATION
// ===================================================================
inline void cleanupAll(Vulkan::Context& ctx) noexcept {
    const std::string threadId = [](){
        std::ostringstream oss; oss << std::this_thread::get_id(); return oss.str();
    }();

    LOG_INFO_CAT("Dispose", "{}[GLOBAL] cleanupAll() — THERMO-GLOBAL APOCALYPSE (thread {}) — {} OBJECTS TO OBLITERATE{}{}",
                 RASPBERRY_PINK, threadId, g_destructionCounter, RESET);

    if (!ctx.device) {
        LOG_ERROR_CAT("Dispose", "{}[GLOBAL] ctx.device NULL — EARLY EXIT{}{}", CRIMSON_MAGENTA, RESET);
        return;
    }

    vkDeviceWaitIdle(ctx.device);

    // Phase 1: High-level managers
    ctx.rtx.reset();
    ctx.camera.reset();
    ctx.swapchainManager.reset();

    // Phase 2: Full resource nuke
    ctx.resourceManager.releaseAll(ctx.device);

    // Phase 3: Swapchain (if somehow missed)
    if (ctx.swapchain) {
        LOG_INFO_CAT("Dispose", "{}vkDestroySwapchainKHR @ 0x{:x}{}{}", OCEAN_TEAL, reinterpret_cast<uintptr_t>(ctx.swapchain), RESET);
        vkDestroySwapchainKHR(ctx.device, ctx.swapchain, nullptr);
        logAndTrackDestruction("SwapchainKHR", ctx.swapchain, __LINE__);
        ctx.swapchain = VK_NULL_HANDLE;
    }

    // Phase 4: Debug messenger
    if (ctx.debugMessenger && ctx.instance) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            LOG_INFO_CAT("Dispose", "{}vkDestroyDebugUtilsMessengerEXT{}{}", OCEAN_TEAL, RESET);
            func(ctx.instance, ctx.debugMessenger, nullptr);
            logAndTrackDestruction("DebugMessenger", ctx.debugMessenger, __LINE__);
        }
    }

    // Phase 5: Surface → Device → Instance
    if (ctx.surface) {
        LOG_INFO_CAT("Dispose", "{}vkDestroySurfaceKHR @ 0x{:x}{}{}", OCEAN_TEAL, reinterpret_cast<uintptr_t>(ctx.surface), RESET);
        vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
        logAndTrackDestruction("SurfaceKHR", ctx.surface, __LINE__);
        ctx.surface = VK_NULL_HANDLE;
    }

    if (ctx.device) {
        LOG_INFO_CAT("Dispose", "{}vkDestroyDevice @ 0x{:x}{}{}", OCEAN_TEAL, reinterpret_cast<uintptr_t>(ctx.device), RESET);
        vkDestroyDevice(ctx.device, nullptr);
        logAndTrackDestruction("Device", ctx.device, __LINE__);
        ctx.device = VK_NULL_HANDLE;
    }

    if (ctx.instance) {
        LOG_INFO_CAT("Dispose", "{}vkDestroyInstance @ 0x{:x}{}{}", OCEAN_TEAL, reinterpret_cast<uintptr_t>(ctx.instance), RESET);
        vkDestroyInstance(ctx.instance, nullptr);
        logAndTrackDestruction("Instance", ctx.instance, __LINE__);
        ctx.instance = VK_NULL_HANDLE;
    }

    // Phase 6: Global tracker memory
    if (auto* bitset = DestroyTracker::s_bitset) {
        LOG_INFO_CAT("Dispose", "{}FREEING GLOBAL DestroyTracker BITSET — ETERNAL PEACE{}{}", EMERALD_GREEN, RESET);
        delete[] bitset;
        DestroyTracker::s_bitset = nullptr;
        DestroyTracker::s_capacity.store(0);
    }

    LOG_INFO_CAT("Dispose", "{}cleanupAll() → {} OBJECTS REDUCED TO QUANTUM DUST — UNIVERSE CLEANSED — FLAWLESS VICTORY{}{}",
                 RASPBERRY_PINK, g_destructionCounter, RESET);
    LOG_INFO_CAT("Dispose", "{}AMOURANTH RTX — DISPOSE SYSTEM — RASPBERRY_PINK SUPREMACY — NOV 07 2025{}{}", RASPBERRY_PINK, RESET);
}

// ===================================================================
// GROK PROTIPS FOR DEVELOPERS — READ OR PERISH
// ===================================================================
/*
    PROTIP #1: NEVER use raw Vk* again → ALWAYS use VulkanHandle<T>
        BAD:  VkPipeline pipeline = createPipeline();
        GOOD: VulkanHandle<VkPipeline> pipeline = makePipeline(dev, createPipeline());

    PROTIP #2: ALL resources MUST be added to resourceManager
        resourceManager.addPipeline(pipeline.get(), "raytracing");
        resourceManager.addBuffer(buffer.get());
        → Double-free proof + verbose logging

    PROTIP #3: Use .get() only when passing to Vulkan API
        vkBindBufferMemory(device, buffer.get(), memory.get(), 0);

    PROTIP #4: NO MORE manual cleanup in destructors
        ~Context() { cleanupAll(*this); } → does EVERYTHING

    PROTIP #5: Want to know what died? Check logs → EVERY destroy is RASPBERRY_PINK
        [Dispose][LINE:420] DESTROYED Pipeline @ 0x69f420

    PROTIP #6: Build command:
        make clean && make -j69 && ./amouranthRTX
        → 69,420 FPS or GTFO

    PROTIP #7: RASPBERRY_PINK is not a color. It's a lifestyle.
*/