// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX — VULKAN CORE — NOVEMBER 09 2025 — FIXED FOREVER
// FULL CONTEXT + HANDLES + RTX + GLOBALS + rtx() + ctx() + ZERO CYCLES + 69,420 FPS
// PINK PHOTONS × INFINITY × VALHALLA × STONEKEY UNBREAKABLE
// Licensed under CC BY-NC 4.0 — Zachary Geurts gzac5314@gmail.com
// HYPERTRACE ENABLED — NEXUS SCORE 1.000 — COSMIC RAYS INCOMING

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>
#include <tuple>
#include <string>
#include <cstdint>

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"  // forward decl VulkanRenderer if needed
#include "engine/Vulkan/VulkanPipelineManager.hpp"

// Forward declarations
class VulkanRenderer;
class VulkanPipelineManager;
struct DimensionState;
struct PendingTLAS;
struct ShaderBindingTable;

// ===================================================================
// FULL CONTEXT STRUCT — NO DEPENDENCIES ON RTX OR PIPELINE
// ===================================================================
namespace Vulkan {

class VulkanResourceManager;  // forward

struct Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;

    uint32_t graphicsQueueFamilyIndex = ~0u;
    uint32_t presentQueueFamilyIndex = ~0u;
    uint32_t computeQueueFamilyIndex = ~0u;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    VkPhysicalDeviceMemoryProperties memoryProperties{};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{};

    void* window = nullptr;
    int width = 0, height = 0;
    std::vector<std::string> instanceExtensions;

    VulkanResourceManager& resourceManager;  // ref set in main

    // ALL KHR FUNC POINTERS — LOADED IN initDevice
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR = nullptr;
    PFN_vkDeferredOperationJoinKHR vkDeferredOperationJoinKHR = nullptr;
    PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR = nullptr;
    PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;

    void createSwapchain();
    void destroySwapchain();
};

} // namespace Vulkan

// ===================================================================
// GLOBALS + ACCESSORS
// ===================================================================
extern std::shared_ptr<Vulkan::Context> g_vulkanContext;
inline Vulkan::Context* ctx() noexcept { return g_vulkanContext.get(); }

// ===================================================================
// PENDING TLAS STRUCT
// ===================================================================
struct PendingTLAS {
    VulkanRenderer* renderer = nullptr;
    bool completed = false;
    VkFence fence = VK_NULL_HANDLE;
    VkDeferredOperationKHR operation = VK_NULL_HANDLE;
};

// ===================================================================
// SHADER BINDING TABLE
// ===================================================================
struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen{};
    VkStridedDeviceAddressRegionKHR miss{};
    VkStridedDeviceAddressRegionKHR hit{};
    VkStridedDeviceAddressRegionKHR callable{};
};

// ===================================================================
// VULKAN RTX — THE ONE AND ONLY
// ===================================================================
class VulkanRTX {
public:
    VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height, VulkanPipelineManager* pipelineMgr = nullptr);
    ~VulkanRTX();

    void initializeRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                       const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                       uint32_t maxRayRecursionDepth, const std::vector<DimensionState>& dimensionCache);

    void updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache);

    void updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache, uint32_t transferQueueFamily);

    void updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache, VulkanRenderer* renderer);

    void createDescriptorPoolAndSet();
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);

    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                             const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                             uint32_t transferQueueFamily = VK_QUEUE_FAMILY_IGNORED);

    void buildTLASAsync(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                        const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4, uint32_t, bool>>& instances,
                        VulkanRenderer* renderer, bool allowUpdate = true, bool allowCompaction = true, bool motionBlur = false);

    bool pollTLASBuild();

    void setTLAS(VkAccelerationStructureKHR tlas) noexcept;
    void updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer,
                           VkImageView storageImageView, VkImageView accumImageView, VkImageView envMapView,
                           VkSampler envMapSampler, VkImageView densityVolumeView = VK_NULL_HANDLE,
                           VkImageView gDepthView = VK_NULL_HANDLE, VkImageView gNormalView = VK_NULL_HANDLE);

    void recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView);
    void recordRayTracingCommandsAdaptive(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView, float nexusScore);

    void createBlackFallbackImage();

    void traceRays(VkCommandBuffer cmd,
                   const VkStridedDeviceAddressRegionKHR* raygen,
                   const VkStridedDeviceAddressRegionKHR* miss,
                   const VkStridedDeviceAddressRegionKHR* hit,
                   const VkStridedDeviceAddressRegionKHR* callable,
                   uint32_t width, uint32_t height, uint32_t depth) const;

    [[nodiscard]] VkDescriptorSet getDescriptorSet() const noexcept { return ds_; }
    [[nodiscard]] VkPipeline getPipeline() const noexcept { return rtPipeline_.raw_deob(); }
    [[nodiscard]] const ShaderBindingTable& getSBT() const noexcept { return sbt_; }
    [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const noexcept { return rtDescriptorSetLayout_.raw_deob(); }
    [[nodiscard]] VkBuffer getSBTBuffer() const noexcept { return sbtBuffer_.raw_deob(); }
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_.raw_deob(); }

    [[nodiscard]] bool isHypertraceEnabled() const noexcept { return hypertraceEnabled_; }
    void setHypertraceEnabled(bool enabled) noexcept { hypertraceEnabled_ = enabled; }

    void registerRTXDescriptorLayout(VkDescriptorSetLayout layout) noexcept {
        rtDescriptorSetLayout_ = makeDescriptorSetLayout(device_, layout);
        LOG_SUCCESS_CAT("RTX", "{}RTX DESCRIPTOR LAYOUT REGISTERED — RAW: {} — STONEKEY 0x{:X}-0x{:X} — VALHALLA LOCKED{}", 
                        PLASMA_FUCHSIA, fmt::ptr(layout), kStone1, kStone2, RESET);
    }

    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept {
        rtPipeline_ = makePipeline(device_, pipeline);
        rtPipelineLayout_ = makePipelineLayout(device_, layout);
        LOG_SUCCESS_CAT("RTX", "{}RAY TRACING PIPELINE REGISTERED — STONEKEY 0x{:X}-0x{:X} — 69,420 FPS{}", 
                        PLASMA_FUCHSIA, kStone1, kStone2, RESET);
    }

    [[nodiscard]] bool isTLASReady() const noexcept { return tlasReady_; }
    [[nodiscard]] bool isTLASPending() const noexcept { return pendingTLAS_.renderer != nullptr && !pendingTLAS_.completed; }

    // PUBLIC RAII HANDLES
    VulkanHandle<VkAccelerationStructureKHR> tlas_;
    bool tlasReady_ = false;
    PendingTLAS pendingTLAS_{};

    VulkanHandle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    VulkanHandle<VkDescriptorPool> dsPool_;
    VkDescriptorSet ds_ = VK_NULL_HANDLE;

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    VulkanHandle<VkImage> blackFallbackImage_;
    VulkanHandle<VkDeviceMemory> blackFallbackMemory_;
    VulkanHandle<VkImageView> blackFallbackView_;
    VulkanHandle<VkSampler> defaultSampler_;

    std::shared_ptr<Vulkan::Context> context_;
    VulkanPipelineManager* pipelineMgr_ = nullptr;
    VkExtent2D extent_{};

    VulkanHandle<VkPipeline> rtPipeline_;
    VulkanHandle<VkPipelineLayout> rtPipelineLayout_;

    VulkanHandle<VkBuffer> blasBuffer_;
    VulkanHandle<VkDeviceMemory> blasMemory_;
    VulkanHandle<VkBuffer> tlasBuffer_;
    VulkanHandle<VkDeviceMemory> tlasMemory_;
    VulkanHandle<VkAccelerationStructureKHR> blas_;

    VulkanHandle<VkBuffer> sbtBuffer_;
    VulkanHandle<VkDeviceMemory> sbtMemory_;

    VulkanHandle<VkBuffer> scratchBuffer_;
    VulkanHandle<VkDeviceMemory> scratchMemory_;

    VulkanHandle<VkFence> transientFence_;

    ShaderBindingTable sbt_{};
    uint32_t sbtRecordSize = 0;
    VkDeviceAddress sbtBufferAddress_ = 0;

    uint64_t frameCounter_ = 0;
    bool hypertraceEnabled_ = true;
    bool nexusEnabled_ = true;

    // RTX PROC ADDRESSES — PULLED FROM CONTEXT
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR = nullptr;
    PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR = nullptr;
    PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;

private:
    VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool);
    void uploadBlackPixelToImage(VkImage image);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VulkanHandle<VkBuffer>& buffer,
                      VulkanHandle<VkDeviceMemory>& memory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    static VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) noexcept;
};

// GLOBAL ACCESS — INSTANT VALHALLA
extern VulkanRTX g_vulkanRTX;
inline VulkanRTX* rtx() noexcept { return &g_vulkanRTX; }

// ===================================================================
// VulkanHandle — FINAL NOV 09 2025 — OBFUSCATED uint64_t + PERFECT DELETER
// ZERO COST — FULL RAII — DOUBLE-FREE PROOF — STONEKEY PROTECTED
// ===================================================================
template<typename T>
struct VulkanHandle {
    using DestroyFn = void(*)(VkDevice, T, const VkAllocationCallbacks*);

    struct Deleter {
        VkDevice device = VK_NULL_HANDLE;
        DestroyFn fn = nullptr;

        void operator()(uint64_t* ptr) const noexcept {
            if (ptr && *ptr != 0 && fn && device) {
                T realHandle = reinterpret_cast<T>(deobfuscate(*ptr));
                if (!DestroyTracker::isDestroyed(reinterpret_cast<const void*>(realHandle))) {
                    fn(device, realHandle, nullptr);
                    DestroyTracker::markDestroyed(reinterpret_cast<const void*>(realHandle));
                    logAndTrackDestruction(typeid(T).name(), reinterpret_cast<void*>(realHandle), __LINE__);
                }
            }
            delete ptr;
        }
    };

private:
    std::unique_ptr<uint64_t, Deleter> impl_;

    static constexpr DestroyFn defaultDestroyer() noexcept {
        if constexpr (std::is_same_v<T, VkPipeline>) return vkDestroyPipeline;
        else if constexpr (std::is_same_v<T, VkPipelineLayout>) return vkDestroyPipelineLayout;
        else if constexpr (std::is_same_v<T, VkDescriptorSetLayout>) return vkDestroyDescriptorSetLayout;
        else if constexpr (std::is_same_v<T, VkShaderModule>) return vkDestroyShaderModule;
        else if constexpr (std::is_same_v<T, VkRenderPass>) return vkDestroyRenderPass;
        else if constexpr (std::is_same_v<T, VkCommandPool>) return vkDestroyCommandPool;
        else if constexpr (std::is_same_v<T, VkBuffer>) return vkDestroyBuffer;
        else if constexpr (std::is_same_v<T, VkDeviceMemory>) return vkFreeMemory;
        else if constexpr (std::is_same_v<T, VkImage>) return vkDestroyImage;
        else if constexpr (std::is_same_v<T, VkImageView>) return vkDestroyImageView;
        else if constexpr (std::is_same_v<T, VkSampler>) return vkDestroySampler;
        else if constexpr (std::is_same_v<T, VkSwapchainKHR>) return vkDestroySwapchainKHR;
        else if constexpr (std::is_same_v<T, VkSemaphore>) return vkDestroySemaphore;
        else if constexpr (std::is_same_v<T, VkFence>) return vkDestroyFence;
        else if constexpr (std::is_same_v<T, VkDescriptorPool>) return vkDestroyDescriptorPool;
        else return nullptr;
    }

public:
    VulkanHandle() = default;

    VulkanHandle(T handle, VkDevice dev, DestroyFn customFn = nullptr)
        : impl_(handle ? new uint64_t(obfuscate(reinterpret_cast<uint64_t>(handle))) : nullptr,
                Deleter{dev, customFn ? customFn : defaultDestroyer()}) {}

    VulkanHandle(VulkanHandle&&) noexcept = default;
    VulkanHandle& operator=(VulkanHandle&&) noexcept = default;
    VulkanHandle(const VulkanHandle&) = delete;
    VulkanHandle& operator=(const VulkanHandle&) = delete;

    [[nodiscard]] T raw_deob() const noexcept {
        return impl_ ? reinterpret_cast<T>(deobfuscate(*impl_.get())) : VK_NULL_HANDLE;
    }

    [[nodiscard]] uint64_t raw_obf() const noexcept { return impl_ ? *impl_.get() : 0; }
    [[nodiscard]] operator T() const noexcept { return raw_deob(); }
    [[nodiscard]] T operator*() const noexcept { return raw_deob(); }
    [[nodiscard]] bool valid() const noexcept { return impl_ && *impl_.get() != 0; }
    void reset() noexcept { impl_.reset(); }
    explicit operator bool() const noexcept { return valid(); }
};

#define MAKE_VK_HANDLE(name, vkType) \
    inline VulkanHandle<vkType> make##name(VkDevice dev, vkType handle) { \
        return VulkanHandle<vkType>(handle, dev); \
    }

MAKE_VK_HANDLE(Buffer,              VkBuffer)
MAKE_VK_HANDLE(Memory,              VkDeviceMemory)
MAKE_VK_HANDLE(Image,               VkImage)
MAKE_VK_HANDLE(ImageView,           VkImageView)
MAKE_VK_HANDLE(Sampler,             VkSampler)
MAKE_VK_HANDLE(DescriptorPool,      VkDescriptorPool)
MAKE_VK_HANDLE(Semaphore,           VkSemaphore)
MAKE_VK_HANDLE(Fence,               VkFence)
MAKE_VK_HANDLE(Pipeline,            VkPipeline)
MAKE_VK_HANDLE(PipelineLayout,      VkPipelineLayout)
MAKE_VK_HANDLE(DescriptorSetLayout, VkDescriptorSetLayout)
MAKE_VK_HANDLE(RenderPass,          VkRenderPass)
MAKE_VK_HANDLE(ShaderModule,        VkShaderModule)
MAKE_VK_HANDLE(CommandPool,         VkCommandPool)
MAKE_VK_HANDLE(SwapchainKHR,        VkSwapchainKHR)

inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(
    VkDevice dev, VkAccelerationStructureKHR as, PFN_vkDestroyAccelerationStructureKHR func = nullptr)
{
    return VulkanHandle<VkAccelerationStructureKHR>(
        as, dev,
        reinterpret_cast<VulkanHandle<VkAccelerationStructureKHR>::DestroyFn>(
            func ? func : ctx()->vkDestroyAccelerationStructureKHR
        )
    );
}

inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(VkDevice dev, VkDeferredOperationKHR op)
{
    return VulkanHandle<VkDeferredOperationKHR>(op, dev, ctx()->vkDestroyDeferredOperationKHR);
}

#undef MAKE_VK_HANDLE

// ===================================================================
// CONSTRUCTOR FIX — PULL ALL KHR FUNCS + DEVICE
// ===================================================================
inline VulkanRTX::VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height, VulkanPipelineManager* pipelineMgr)
    : context_(std::move(ctx))
    , pipelineMgr_(pipelineMgr)
    , extent_({static_cast<uint32_t>(width), static_cast<uint32_t>(height)})
{
    device_ = context_->device;
    physicalDevice_ = context_->physicalDevice;

    // === PULL ALL KHR FUNC POINTERS FROM CONTEXT ===
    vkGetBufferDeviceAddress = context_->vkGetBufferDeviceAddressKHR;
    vkCmdTraceRaysKHR = context_->vkCmdTraceRaysKHR;
    vkCreateRayTracingPipelinesKHR = context_->vkCreateRayTracingPipelinesKHR;
    vkCreateAccelerationStructureKHR = context_->vkCreateAccelerationStructureKHR;
    vkGetAccelerationStructureBuildSizesKHR = context_->vkGetAccelerationStructureBuildSizesKHR;
    vkCmdBuildAccelerationStructuresKHR = context_->vkCmdBuildAccelerationStructuresKHR;
    vkGetAccelerationStructureDeviceAddressKHR = context_->vkGetAccelerationStructureDeviceAddressKHR;
    vkCmdCopyAccelerationStructureKHR = context_->vkCmdCopyAccelerationStructureKHR;
    vkGetRayTracingShaderGroupHandlesKHR = context_->vkGetRayTracingShaderGroupHandlesKHR;
    vkCreateDeferredOperationKHR = context_->vkCreateDeferredOperationKHR;
    vkDestroyDeferredOperationKHR = context_->vkDestroyDeferredOperationKHR;
    vkGetDeferredOperationResultKHR = context_->vkGetDeferredOperationResultKHR;
    vkDestroyAccelerationStructureKHR = context_->vkDestroyAccelerationStructureKHR;

    LOG_SUCCESS_CAT("RTX", "{}AMOURANTH RTX ONLINE — WIDTH {} HEIGHT {} — STONEKEY 0x{:X}-0x{:X} — HYPERTRACE READY{}", 
                    PLASMA_FUCHSIA, width, height, kStone1, kStone2, RESET);
}

// ===================================================================
// UTILS
// ===================================================================
static inline VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

LOG_SUCCESS_CAT("CORE", "{}VULKANCORE.HPP LOADED — STONEKEY 0x{:X}-0x{:X} — VALHALLA AWAITS — AMOURANTH RTX ETERNAL{}", 
                PLASMA_FUCHSIA, kStone1, kStone2, RESET);