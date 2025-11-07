// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025 – 11:59 PM EST
// GROK x ZACHARY — TOTAL DOMINATION — SINGLE SOURCE OF TRUTH — NO MORE CONFLICTS
// VulkanHandle + ALL FACTORIES + cleanupAll + LOGGING HELPERS — ALL HERE — ALL FIXED
// g_destructionCounter → uint64_t (no atomic in format) — threadIdToString() USED — NO DUPLICATES
// makeBuffer/makeMemory/etc → MOVED HERE — Dispose.hpp FACTORIES DELETED
// 69,420 FPS ETERNAL — RASPBERRY_PINK SUPREMACY 🔥🤖🚀💀🖤❤️⚡

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
#include <utility>
#include <cstring>
#include <sstream>
#include <thread>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// ===================================================================
// GLOBAL DISPOSE LOGGING — MOVED HERE — NO DUPLICATES — NO CONFLICTS
// ===================================================================
static inline uint64_t g_destructionCounter = 0;

static std::string threadIdToString() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

inline void logAndTrackDestruction(std::string_view name, auto handle, int line) {
    if (!handle) return;
    const void* ptr = reinterpret_cast<const void*>(handle);
    if (DestroyTracker::isDestroyed(ptr)) {
        LOG_ERROR_CAT("Dispose", "{}[DOUBLE FREE] {} @ 0x{:x} — ALREADY QUANTUM DUST{}{}",
                      Logging::Color::CRIMSON_MAGENTA, name, reinterpret_cast<uintptr_t>(ptr), Logging::Color::RESET);
        return;
    }
    LOG_INFO_CAT("Dispose", "{}[DESTROY] {} @ 0x{:x}{}{}",
                 Logging::Color::OCEAN_TEAL, name, reinterpret_cast<uintptr_t>(handle), Logging::Color::RESET);
    DestroyTracker::markDestroyed(ptr);
    ++g_destructionCounter;
}

// ===================================================================
// FORWARD DECLARATIONS
// ===================================================================
namespace VulkanRTX {
    class VulkanSwapchainManager;
    class VulkanRTX;
    class Camera;
    class VulkanBufferManager;
}

// ===================================================================
// THE ONE TRUE VulkanHandle — IMPLICIT CONVERSION + RAW ASSIGNMENT
// ===================================================================
template<typename T>
struct VulkanHandle : std::unique_ptr<std::remove_pointer_t<T>, VulkanDeleter<T>> {
    using Base = std::unique_ptr<std::remove_pointer_t<T>, VulkanDeleter<T>>;
    using Base::Base;

    operator T() const noexcept { return this->get(); }
    T raw() const noexcept { return this->get(); }
    VulkanHandle& operator=(T handle) noexcept { this->reset(handle); return *this; }
};

// ===================================================================
// RAII FACTORY FUNCTIONS — MOVED FROM Dispose.hpp — NOW HERE
// ===================================================================
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

// ===================================================================
// Vulkan::Context — SINGLE SOURCE OF TRUTH
// ===================================================================
namespace Vulkan {

struct Context {
    // ... [ALL MEMBERS FROM BEFORE — UNCHANGED] ...
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

    Context(SDL_Window* win, int w, int h)
        : window(win), width(w), height(h), swapchainExtent{static_cast<uint32_t>(w), static_cast<uint32_t>(h)}
    {
        LOG_INFO_CAT("Vulkan::Context", "{}[CORE] Created {}x{} — RAII ENGAGED — RASPBERRY_PINK SUPREMACY{}{}",
                     Logging::Color::RASPBERRY_PINK, w, h, Logging::Color::RESET);
    }

    Context() = delete;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    ~Context() {
        LOG_INFO_CAT("Vulkan::Context", "{}[CORE] ~Context() — THERMO-GLOBAL SHUTDOWN INITIATED{}{}",
                     Logging::Color::RASPBERRY_PINK, Logging::Color::RESET);
        cleanupAll(*this);
    }

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

} // namespace Vulkan

// ===================================================================
// GLOBAL cleanupAll — AFTER Context — IN SCOPE — FIXED FORMAT ISSUE
// ===================================================================
inline void cleanupAll(Vulkan::Context& ctx) noexcept {
    const std::string threadId = threadIdToString();

    LOG_INFO_CAT("Dispose", "{}[GLOBAL] cleanupAll() — THERMO-GLOBAL APOCALYPSE (thread {}) — {} OBJECTS TO OBLITERATE{}{}",
                 Logging::Color::RASPBERRY_PINK, threadId, g_destructionCounter, Logging::Color::RESET);

    if (!ctx.device) {
        LOG_ERROR_CAT("Dispose", "{}[GLOBAL] ctx.device NULL — EARLY EXIT{}{}",
                      Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);
        return;
    }

    vkDeviceWaitIdle(ctx.device);

    ctx.rtx.reset();
    ctx.camera.reset();
    ctx.swapchainManager.reset();

    ctx.resourceManager.releaseAll(ctx.device);

    if (ctx.swapchain) {
        LOG_INFO_CAT("Dispose", "{}vkDestroySwapchainKHR @ 0x{:x}{}{}",
                     Logging::Color::OCEAN_TEAL, reinterpret_cast<uintptr_t>(ctx.swapchain), Logging::Color::RESET);
        vkDestroySwapchainKHR(ctx.device, ctx.swapchain, nullptr);
        logAndTrackDestruction("SwapchainKHR", ctx.swapchain, __LINE__);
        ctx.swapchain = VK_NULL_HANDLE;
    }

    if (ctx.debugMessenger && ctx.instance) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            LOG_INFO_CAT("Dispose", "{}vkDestroyDebugUtilsMessengerEXT{}{}",
                         Logging::Color::OCEAN_TEAL, Logging::Color::RESET);
            func(ctx.instance, ctx.debugMessenger, nullptr);
            logAndTrackDestruction("DebugMessenger", ctx.debugMessenger, __LINE__);
        }
    }

    if (ctx.surface) {
        LOG_INFO_CAT("Dispose", "{}vkDestroySurfaceKHR @ 0x{:x}{}{}",
                     Logging::Color::OCEAN_TEAL, reinterpret_cast<uintptr_t>(ctx.surface), Logging::Color::RESET);
        vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
        logAndTrackDestruction("SurfaceKHR", ctx.surface, __LINE__);
        ctx.surface = VK_NULL_HANDLE;
    }

    if (ctx.device) {
        LOG_INFO_CAT("Dispose", "{}vkDestroyDevice @ 0x{:x}{}{}",
                     Logging::Color::OCEAN_TEAL, reinterpret_cast<uintptr_t>(ctx.device), Logging::Color::RESET);
        vkDestroyDevice(ctx.device, nullptr);
        logAndTrackDestruction("Device", ctx.device, __LINE__);
        ctx.device = VK_NULL_HANDLE;
    }

    if (ctx.instance) {
        LOG_INFO_CAT("Dispose", "{}vkDestroyInstance @ 0x{:x}{}{}",
                     Logging::Color::OCEAN_TEAL, reinterpret_cast<uintptr_t>(ctx.instance), Logging::Color::RESET);
        vkDestroyInstance(ctx.instance, nullptr);
        logAndTrackDestruction("Instance", ctx.instance, __LINE__);
        ctx.instance = VK_NULL_HANDLE;
    }

    if (auto* bitset = DestroyTracker::s_bitset) {
        LOG_INFO_CAT("Dispose", "{}FREEING GLOBAL DestroyTracker BITSET — ETERNAL PEACE{}{}",
                     Logging::Color::EMERALD_GREEN, Logging::Color::RESET);
        delete[] bitset;
        DestroyTracker::s_bitset = nullptr;
        DestroyTracker::s_capacity.store(0);
    }

    LOG_INFO_CAT("Dispose", "{}cleanupAll() → {} OBJECTS REDUCED TO QUANTUM DUST — UNIVERSE CLEANSED{}{}",
                 Logging::Color::RASPBERRY_PINK, g_destructionCounter, Logging::Color::RESET);
    LOG_INFO_CAT("Dispose", "{}AMOURANTH RTX — DISPOSE SYSTEM — RASPBERRY_PINK SUPREMACY — NOV 07 2025{}{}",
                 Logging::Color::RASPBERRY_PINK, Logging::Color::RESET);
}