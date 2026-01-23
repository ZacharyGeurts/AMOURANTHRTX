// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.7 — JANUARY 22, 2026
// PIPELINEMANAGER HEADER — NUCLEAR ZERO-COST RTX EDITION
// SINGLE ETERNAL DESCRIPTOR SET | NO FRAMES | FRAME-FREE TRACE & UPDATE
// PERSISTENT COMMAND BUFFERS COMPATIBLE | ETERNAL SBT | VALIDATION PERFECT
// NO PER-FRAME ALLOCATIONS | MAX DRIVER FRIENDLINESS
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
#include <mutex>

namespace RTX {

struct Extensions;
extern Extensions g_ext;

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
    void updateRTDescriptorSet(const RTDescriptorUpdate& updateInfo) noexcept;
    void forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer mainCmd);

    VkShaderModule loadShader(const std::string& path) const;

    // ZERO-COST TRACE — called directly from persistent command buffer
    void traceRays(VkCommandBuffer cmd, uint32_t width, uint32_t height);

    static std::atomic<bool>     g_pipelineNeedsRebuild;

    // Accessors
    [[nodiscard]] VkDescriptorSet getDescriptorSet() const;  // single eternal set
    [[nodiscard]] VkPipeline       getPipeline() const;
    [[nodiscard]] VkPipelineLayout getPipelineLayout() const;

    [[nodiscard]] VkDeviceSize   sbtAddress() const noexcept { return sbtAddress_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& raygenRegion() const noexcept { return raygenSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& missRegion() const noexcept { return missSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& hitRegion() const noexcept { return hitSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& callableRegion() const noexcept { return callableSbtRegion_; }

    [[nodiscard]] std::span<const VkDescriptorSet> rtDescriptorSets() const noexcept { return rtDescriptorSets_; }

    [[nodiscard]] VkAccelerationStructureKHR dummyTLAS() const noexcept { return dummyTLAS_.get(); }

    [[nodiscard]] static PipelineManager& instance() noexcept {
        static PipelineManager inst;
        return inst;
    }

    void cacheDeviceProperties();

    static inline bool s_crownForged = false;
    static inline bool s_eternalSbtForged = false;

    VkStridedDeviceAddressRegionKHR raygenSbtRegion_{};
    VkStridedDeviceAddressRegionKHR missSbtRegion_{};
    VkStridedDeviceAddressRegionKHR hitSbtRegion_{};
    VkStridedDeviceAddressRegionKHR callableSbtRegion_{};

private:
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
    Handle<VkDescriptorSetLayout> emptyDescriptorSetLayout_;
    Handle<VkDescriptorSetLayout> texDescriptorSetLayout_;

    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkPipeline>            rtPipeline_;

    Handle<VkBuffer>              sbtBuffer_;
    Handle<VkDeviceMemory>        sbtMemory_;  // Manual SBT cleanup

    VkDeviceSize                  sbtAddress_{0};
    VkDeviceSize                  sbtSize_{0};

    std::vector<Handle<VkShaderModule>> shaderModules_;
    std::vector<VkDescriptorSet>        rtDescriptorSets_;      // size 1 now
    std::vector<VkDescriptorSet>        texDescriptorSets_;     // size 1 now
    std::vector<VkDescriptorSet>        emptyDescriptorSets_;   // size 1 now

    uint32_t raygenGroupCount_{1};
    uint32_t missGroupCount_{1};
    uint32_t hitGroupCount_{0};

    Handle<VkBuffer>                  dummyAccelBuffer_;
    Handle<VkDeviceMemory>            dummyAccelMemory_;
    Handle<VkAccelerationStructureKHR> dummyTLAS_;

    // Private helper — must be declared before use in constructor initializer
    VkAccelerationStructureKHR createDummyTLAS();
};

[[nodiscard]] inline PipelineManager& pipeline() noexcept {
    return PipelineManager::instance();
}

} // namespace RTX

// =============================================================================
// FINAL HEADER — JANUARY 22, 2026
// - Frame-free: single descriptor set, no frameIndex, no MAX_FRAMES_IN_FLIGHT
// - traceRays, updateRTDescriptorSet, getDescriptorSet simplified
// - Manual SBT cleanup with sbtMemory_ Handle
// - Direct call from persistent command buffer in VulkanRenderer
// - Zero-cost RTX preserved — validation clean
// Empire complete — pink photons scream across the screen — AMOURANTH FOREVER 💖
// =============================================================================