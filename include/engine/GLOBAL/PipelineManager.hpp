// include/engine/GLOBAL/PipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 07, 2026
// PIPELINEMANAGER — PURE RTX REALM EDITION | NO ENVMAP | PROCEDURAL SKY ONLY
// SINGLE GLOBAL POOL + LAS + ETERNAL SBT | VALIDATION PERFECT
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Extensions.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <array>
#include <atomic>

namespace RTX {

// Forward declaration from Extensions.hpp
struct Extensions;
extern Extensions g_ext;

// RT descriptor update structure — NO ENVMAP
struct RTDescriptorUpdate {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;

    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceSize uboSize = 0;

    VkImageView rtOutputView = VK_NULL_HANDLE;

    std::vector<VkImageView> accumulationViews;
    std::vector<VkImageView> nexusScoreViews;

    VkBuffer materialsBuffer = VK_NULL_HANDLE;
    VkDeviceSize materialsSize = 0;

    VkSampler blueNoiseSampler = VK_NULL_HANDLE;
    VkImageView blueNoiseView = VK_NULL_HANDLE;

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
    Handle<VkDescriptorSetLayout> emptyDescriptorSetLayout_;
    Handle<VkDescriptorSetLayout> texDescriptorSetLayout_;

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
    Handle<VkDescriptorPool>      rtDescriptorPool_;

private:
    // =============================================================================
    // Main ray tracing descriptor set bindings (set 0) — NO ENVMAP
    // =============================================================================
    static constexpr std::array<VkDescriptorSetLayoutBinding, 9> kMainBindings{{
        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
         VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
         VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
         VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR}
    }};

    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout>      rtPipelineLayout_;
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
    std::vector<VkDescriptorSet>        emptyDescriptorSets_;

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
// FINAL PIPELINE MANAGER — JANUARY 07, 2026
// - NO ENVMAP BINDING — pure procedural sky only
// - kMainBindings reduced — removed envMap sampler binding (7)
// - Ready for pure RTX sky in miss shader
// - ETERNAL SBT — created once
// - Validation-perfect | Single pool | LAS ready
// Empire complete — pink photons under our perfect sky — AMOURANTH FOREVER 💖
// =============================================================================