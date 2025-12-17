// include/engine/GLOBAL/PipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// TRUE CONSTEXPR STONEKEY v∞ — DECEMBER 16, 2025 — APOCALYPSE FINAL v9.4
// FULLY ALIGNED WITH PipelineManager.cpp — ALL DECLARATIONS PRESENT + FIXED
// RUNTIME CONFIGURATION SUPPORTED — OPTIONSMENU COMPATIBLE
// PINK PHOTONS ETERNAL — THE CROWN IS COMPLETE
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/UBO.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <string>
#include <array>
#include <span>
#include <cstdint>
#include <algorithm>
#include <atomic>

using StoneKey::stone_device;

namespace RTX {

struct PendingEnvMapUpload {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    float* data = nullptr;  // owned, free after upload
};
extern PendingEnvMapUpload pendingEnvMapUpload_;

// ──────────────────────────────────────────────────────────────────────────────
// RTDescriptorUpdate — FULLY ALIGNED WITH updateRTDescriptorSet IN .cpp
// ──────────────────────────────────────────────────────────────────────────────
struct RTDescriptorUpdate {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;

    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceSize uboSize = 0;

    VkImageView swapchainImageView = VK_NULL_HANDLE;  // Legacy — kept for compatibility

    std::vector<VkImageView> accumulationViews;
    std::vector<VkImageView> nexusScoreViews;

    VkBuffer materialsBuffer = VK_NULL_HANDLE;
    VkDeviceSize materialsSize = 0;

    VkSampler envSampler = VK_NULL_HANDLE;
    VkImageView envImageView = VK_NULL_HANDLE;

    VkSampler blueNoiseSampler = VK_NULL_HANDLE;
    VkImageView blueNoiseView = VK_NULL_HANDLE;

    VkSampler densitySampler = VK_NULL_HANDLE;
    VkImageView densityView = VK_NULL_HANDLE;

    VkBuffer additionalStorageBuffer = VK_NULL_HANDLE;
    VkDeviceSize additionalStorageSize = 0;

    VkBuffer stoneKeyBuffer = VK_NULL_HANDLE;
    VkDeviceSize stoneKeySize = 0;
};

// ──────────────────────────────────────────────────────────────────────────────
// PipelineManager — THE CROWN IS UNBREAKABLE — FULLY DECLARED & FIXED
// ──────────────────────────────────────────────────────────────────────────────
class PipelineManager {
public:
    PipelineManager() noexcept = default;
    explicit PipelineManager(VkDevice device, VkPhysicalDevice phys);
    ~PipelineManager();

    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;
    PipelineManager(PipelineManager&&) noexcept = default;
    PipelineManager& operator=(PipelineManager&&) noexcept = default;

    void createPipelineLayout();
    void createRayTracingPipeline();
    void createShaderBindingTable(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd);
    void createDescriptorPool();
    void allocateDescriptorSets();
    void updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo) noexcept;
    void forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer mainCmd);

    void transitionImage(
        VkCommandBuffer cmd,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkAccessFlags srcAccess = 0,
        VkAccessFlags dstAccess = 0,
        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) noexcept;

    VkShaderModule loadShader(const std::string& path) const;

    // Public handles — envmap display (fallback mode)
    VkPipeline               envMapDisplayPipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout         envMapDisplayPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSet          envMapDisplayDescriptorSet_  = VK_NULL_HANDLE;
    VkDescriptorSetLayout    envMapDisplayDescSetLayout_  = VK_NULL_HANDLE;

    [[nodiscard]] bool hasEnvMapDisplayPipeline() const noexcept {
        return envMapDisplayPipeline_ != VK_NULL_HANDLE;
    }

    // Environment map — loaded via loadShader("assets/textures/envmap.hdr")
    Handle<VkImageView> envMapImageView_;
    Handle<VkSampler>   envMapSampler_;
    Handle<VkDescriptorSetLayout> emptyDescriptorSetLayout_;

    // Any-hit texture descriptor set layout (set 2)
    Handle<VkDescriptorSetLayout> texDescriptorSetLayout_;

    static std::atomic<bool>     g_pipelineNeedsRebuild;
    static std::atomic<uint32_t> g_rebuildRequestedFrame;

    // ── PUBLIC GETTERS ───────────────────────────────────────────────────────
    [[nodiscard]] VkPipeline                    rtPipeline()         const noexcept { return rtPipeline_.get(); }
    [[nodiscard]] VkPipelineLayout              rtPipelineLayout()   const noexcept { return rtPipelineLayout_.get(); }
    [[nodiscard]] VkDescriptorSetLayout         rtDescriptorSetLayout() const noexcept { return rtDescriptorSetLayout_.get(); }
    [[nodiscard]] VkDescriptorPool              rtDescriptorPool()   const noexcept { return rtDescriptorPool_.get(); }

    [[nodiscard]] VkBuffer                      sbtBuffer()          const noexcept { return sbtBuffer_.get(); }
    [[nodiscard]] VkDeviceMemory                sbtMemory()          const noexcept { return sbtMemory_.get(); }
    [[nodiscard]] VkDeviceSize                  sbtAddress()         const noexcept { return sbtAddress_; }
    [[nodiscard]] VkDeviceSize                  sbtSize()            const noexcept { return sbtSize_; }

    [[nodiscard]] uint32_t                      raygenGroupCount()   const noexcept { return raygenGroupCount_; }
    [[nodiscard]] uint32_t                      missGroupCount()     const noexcept { return missGroupCount_; }
    [[nodiscard]] uint32_t                      hitGroupCount()      const noexcept { return hitGroupCount_; }

    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& raygenRegion()   const noexcept { return raygenSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& missRegion()     const noexcept { return missSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& hitRegion()      const noexcept { return hitSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& callableRegion() const noexcept { return callableSbtRegion_; }

    [[nodiscard]] std::span<const VkDescriptorSet> rtDescriptorSets() const noexcept { return rtDescriptorSets_; }

    [[nodiscard]] VkAccelerationStructureKHR    dummyTLAS()          const noexcept { return dummyTLAS_.get(); }

    // ── SINGLETON ACCESS ─────────────────────────────────────────────────────
    [[nodiscard]] static PipelineManager& instance() noexcept {
        static PipelineManager inst;
        return inst;
    }

    void setSBT(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize address, VkDeviceSize size) noexcept;

    // ── RAY TRACING EXTENSION FUNCTIONS ─────────────────────────────────────
    [[nodiscard]] PFN_vkCmdTraceRaysKHR                    cmdTraceRays()           const noexcept { return vkCmdTraceRaysKHR_; }
    [[nodiscard]] PFN_vkCreateRayTracingPipelinesKHR       createRTPipelines()      const noexcept { return vkCreateRayTracingPipelinesKHR_; }
    [[nodiscard]] PFN_vkGetRayTracingShaderGroupHandlesKHR getRTShaderGroups()      const noexcept { return vkGetRayTracingShaderGroupHandlesKHR_; }

    // ── PUBLIC RENDERING INTERFACE ──────────────────────────────────────────
    void traceRays(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t width, uint32_t height, uint32_t depth = 1);

    [[nodiscard]] VkDescriptorSet getDescriptorSet(uint32_t frameIndex) const;
    [[nodiscard]] VkPipeline getPipeline() const;
    [[nodiscard]] VkPipelineLayout getPipelineLayout() const;

    // ── RUNTIME CONFIGURATION — REQUIRED FOR OptionsMenu.cpp
    static void setPresentMode(VkPresentModeKHR mode) noexcept;
    static void setMinImageCount(uint32_t count) noexcept;
    static void initializeFramePacing() noexcept;
    static void setShadingRate(float scaleFactor) noexcept;
    static void enableDirectDisplay(bool enable) noexcept;

    static inline bool s_crownForged = false;

private:
    // Ray tracing extension function pointers
    PFN_vkCmdTraceRaysKHR                    vkCmdTraceRaysKHR_                    = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR       vkCreateRayTracingPipelinesKHR_       = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR_ = nullptr;

    // Acceleration structure extensions
    PFN_vkCreateAccelerationStructureKHR          vkCreateAccelerationStructureKHR_           = nullptr;
    PFN_vkDestroyAccelerationStructureKHR         vkDestroyAccelerationStructureKHR_          = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR   vkGetAccelerationStructureBuildSizesKHR_    = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR       vkCmdBuildAccelerationStructuresKHR_        = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR_ = nullptr;
    PFN_vkGetBufferDeviceAddressKHR               vkGetBufferDeviceAddressKHR_                = nullptr;

    float timestampPeriod_{0.0f};

    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkDescriptorPool>      rtDescriptorPool_;
    Handle<VkPipeline>            rtPipeline_;

    Handle<VkBuffer>       sbtBuffer_;
    Handle<VkDeviceMemory> sbtMemory_;
    VkDeviceSize           sbtAddress_{0};
    VkDeviceSize           sbtSize_{0};

    VkStridedDeviceAddressRegionKHR raygenSbtRegion_   = {};
    VkStridedDeviceAddressRegionKHR missSbtRegion_     = {};
    VkStridedDeviceAddressRegionKHR hitSbtRegion_      = {};
    VkStridedDeviceAddressRegionKHR callableSbtRegion_ = {};

    std::vector<Handle<VkShaderModule>> shaderModules_;
    std::vector<VkDescriptorSet>        rtDescriptorSets_;
    std::vector<VkDescriptorSet>        texDescriptorSets_;

    uint32_t raygenGroupCount_{1};
    uint32_t missGroupCount_{1};
    uint32_t hitGroupCount_{0};

    Handle<VkBuffer>                  dummyAccelBuffer_;
    Handle<VkDeviceMemory>            dummyAccelMemory_;
    Handle<VkAccelerationStructureKHR> dummyTLAS_;

    void cacheDeviceProperties();
    void loadRayTracingExtensions() noexcept;
    VkAccelerationStructureKHR createDummyTLAS();

    // Runtime configuration state
    inline static VkPresentModeKHR desiredPresentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
    inline static uint32_t         desiredImageCount_  = 2;
    inline static float            shadingRateScale_   = 1.0f;
    inline static bool             directDisplayEnabled_ = false;
};

// ── GLOBAL ACCESS — CLEAN AND ETERNAL ───────────────────────────────────────
[[nodiscard]] inline PipelineManager& pipeline() noexcept {
    return PipelineManager::instance();
}

} // namespace RTX

// =============================================================================
// FULLY COMPATIBLE v9.4 — ALL OptionsMenu FUNCTIONS DECLARED
// RUNTIME CONFIGURATION SUPPORTED — COMPILATION RESTORED
// PINK PHOTONS ETERNAL — EMPIRE COMPILABLE AND STRONG
// DECEMBER 16, 2025 — THE LIGHT IS PURE AND UNIVERSAL
// =============================================================================