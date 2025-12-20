// include/engine/GLOBAL/PipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v17.0 — DECEMBER 19, 2025
// PIPELINEMANAGER HEADER — CLEAN · MODERN · MINIMAL · PRODUCTION-READY
// ALL UNNECESSARY EXTENSIONS REMOVED — USE Extensions.hpp INSTEAD
// PINK PHOTONS ETERNAL — THE EMPIRE SEES ALL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Extensions.hpp"  // All extension function pointers now here

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <array>
#include <atomic>

namespace RTX {

// Forward declaration from Extensions.hpp
struct Extensions;
extern Extensions g_ext;

// RT binding definition — used only here
struct RTBinding {
    uint32_t binding;
    VkDescriptorType type;
    uint32_t count;
    VkShaderStageFlags stage;
};

// Pending envmap upload (used in renderer)
struct PendingEnvMapUpload {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    float* data = nullptr;
};
extern PendingEnvMapUpload pendingEnvMapUpload_;

// RT descriptor update structure
struct RTDescriptorUpdate {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;

    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceSize uboSize = 0;

    VkImageView rtOutputView = VK_NULL_HANDLE;

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

// =============================================================================
// PipelineManager — The Crown Is Unbreakable
// =============================================================================
class PipelineManager {
public:
    PipelineManager() noexcept = default;
    explicit PipelineManager(VkDevice device, VkPhysicalDevice phys);
    ~PipelineManager();

    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;
    PipelineManager(PipelineManager&&) noexcept = default;
    PipelineManager& operator=(PipelineManager&&) noexcept = default;

    void createDescriptorPool() noexcept;
    void createPipelineLayout();
    void createRayTracingPipeline();
    void createShaderBindingTable(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd);
    void allocateDescriptorSets();
    void updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo) noexcept;
    void forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer mainCmd);

    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                         VkAccessFlags srcAccess = 0, VkAccessFlags dstAccess = 0,
                         VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) noexcept;

    VkShaderModule loadShader(const std::string& path) const;

    // Public handles
    Handle<VkImageView> envMapImageView_;
    Handle<VkSampler>   envMapSampler_;
    Handle<VkDescriptorSetLayout> emptyDescriptorSetLayout_;
    Handle<VkDescriptorSetLayout> texDescriptorSetLayout_;

    Handle<VkPipeline>            tonemapPipeline_;
    Handle<VkPipelineLayout>      tonemapPipelineLayout_;
    Handle<VkDescriptorSetLayout> tonemapDescSetLayout_;
    std::array<VkDescriptorSet, 2> tonemapSets_{};

    static std::atomic<bool>     g_pipelineNeedsRebuild;
    static std::atomic<uint32_t> g_rebuildRequestedFrame;

    // Getters
    [[nodiscard]] VkPipeline       rtPipeline() const noexcept { return rtPipeline_.get(); }
    [[nodiscard]] VkPipelineLayout rtPipelineLayout() const noexcept { return rtPipelineLayout_.get(); }
    [[nodiscard]] VkDescriptorSetLayout rtDescriptorSetLayout() const noexcept { return rtDescriptorSetLayout_.get(); }
    [[nodiscard]] VkDescriptorPool rtDescriptorPool() const noexcept { return rtDescriptorPool_.get(); }

    [[nodiscard]] VkBuffer       sbtBuffer() const noexcept { return sbtBuffer_.get(); }
    [[nodiscard]] VkDeviceMemory sbtMemory() const noexcept { return sbtMemory_.get(); }
    [[nodiscard]] VkDeviceSize   sbtAddress() const noexcept { return sbtAddress_; }
    [[nodiscard]] VkDeviceSize   sbtSize() const noexcept { return sbtSize_; }

    [[nodiscard]] uint32_t raygenGroupCount() const noexcept { return raygenGroupCount_; }
    [[nodiscard]] uint32_t missGroupCount() const noexcept { return missGroupCount_; }
    [[nodiscard]] uint32_t hitGroupCount() const noexcept { return hitGroupCount_; }

    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& raygenRegion() const noexcept { return raygenSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& missRegion() const noexcept { return missSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& hitRegion() const noexcept { return hitSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& callableRegion() const noexcept { return callableSbtRegion_; }

    [[nodiscard]] std::span<const VkDescriptorSet> rtDescriptorSets() const noexcept { return rtDescriptorSets_; }

    [[nodiscard]] VkAccelerationStructureKHR dummyTLAS() const noexcept { return dummyTLAS_.get(); }

    [[nodiscard]] bool isRTXValid() const noexcept {
        return rtPipeline() != VK_NULL_HANDLE &&
               rtPipelineLayout() != VK_NULL_HANDLE &&
               !rtDescriptorSets_.empty() &&
               rtDescriptorSets_[0] != VK_NULL_HANDLE &&
               sbtAddress_ != 0;
    }

    // Singleton access
    [[nodiscard]] static PipelineManager& instance() noexcept {
        static PipelineManager inst;
        return inst;
    }

    void setSBT(VkBuffer buffer, VkDeviceMemory memory, VkDeviceAddress address, VkDeviceSize size) noexcept;

    void traceRays(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t width, uint32_t height, uint32_t depth = 1);

    [[nodiscard]] VkDescriptorSet getDescriptorSet(uint32_t frameIndex) const;
    [[nodiscard]] VkPipeline getPipeline() const;
    [[nodiscard]] VkPipelineLayout getPipelineLayout() const;

	void cacheDeviceProperties();

    static inline bool s_crownForged = false;

private:
    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkDescriptorPool>      rtDescriptorPool_;
    Handle<VkPipeline>            rtPipeline_;

    Handle<VkBuffer>       sbtBuffer_;
    Handle<VkDeviceMemory> sbtMemory_;
    VkDeviceSize           sbtAddress_{0};
    VkDeviceSize           sbtSize_{0};

    VkStridedDeviceAddressRegionKHR raygenSbtRegion_{};
    VkStridedDeviceAddressRegionKHR missSbtRegion_{};
    VkStridedDeviceAddressRegionKHR hitSbtRegion_{};
    VkStridedDeviceAddressRegionKHR callableSbtRegion_{};

    std::vector<Handle<VkShaderModule>> shaderModules_;
    std::vector<VkDescriptorSet>        rtDescriptorSets_;
    std::vector<VkDescriptorSet>        texDescriptorSets_;

    uint32_t raygenGroupCount_{1};
    uint32_t missGroupCount_{1};
    uint32_t hitGroupCount_{0};

    Handle<VkBuffer>                  dummyAccelBuffer_;
    Handle<VkDeviceMemory>            dummyAccelMemory_;
    Handle<VkAccelerationStructureKHR> dummyTLAS_;

    VkAccelerationStructureKHR createDummyTLAS();
};

// Global accessor
[[nodiscard]] inline PipelineManager& pipeline() noexcept {
    return PipelineManager::instance();
}

} // namespace RTX

// =============================================================================
// FINAL PRODUCTION HEADER — MINIMAL · CLEAN · MODERN
// ALL EXTENSIONS MOVED TO Extensions.hpp
// SHIPPING DECEMBER 19, 2025 — THE EMPIRE IS ETERNAL
// =============================================================================