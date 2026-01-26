// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.62
// PIPELINEMANAGER HEADER — NUCLEAR ZERO-COST RTX + COMPUTE EDITION
// VK_EXT_DESCRIPTOR_BUFFER MODE | NO DESCRIPTOR SETS | MEMCPY UPDATES
// SINGLE ETERNAL DESCRIPTOR BUFFER | FRAME-FREE TRACE & UPDATE
// LIVING WORLD COMPUTE SUPPORT (living_world.spv) | MATERIALS ACTIVE
// PUSH CONSTANT: 4 BYTES ONLY (time float) | NO DT | MAX DRIVER FRIENDLINESS
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
    PipelineManager();
    ~PipelineManager();

    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;
    PipelineManager(PipelineManager&&) noexcept = default;
    PipelineManager& operator=(PipelineManager&&) noexcept = default;

    void createPipelineLayout();
    void createRayTracingPipeline();
    void createComputePipeline();  // living world breathing
    void createShaderBindingTable(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd);
    void writeRTDescriptorsToBuffer(const RTDescriptorUpdate& updateInfo) noexcept;
    void forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer mainCmd);

    // Dispatch living world compute — called every pew before trace
    void dispatchLivingWorld(VkCommandBuffer cmd, float totalTime) noexcept;

    VkShaderModule loadShader(const std::string& path) const;

    // ZERO-COST TRACE — called directly from persistent command buffer
    void traceRays(VkCommandBuffer cmd, uint32_t width, uint32_t height);

    static std::atomic<bool>     g_pipelineNeedsRebuild;

    // Accessors
    [[nodiscard]] VkPipeline       getPipeline() const;
    [[nodiscard]] VkPipelineLayout getPipelineLayout() const;

    [[nodiscard]] VkDeviceSize   sbtAddress() const noexcept { return sbtAddress_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& raygenRegion() const noexcept { return raygenSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& missRegion() const noexcept { return missSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& hitRegion() const noexcept { return hitSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& callableRegion() const noexcept { return callableSbtRegion_; }

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
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR}, // Materials — active
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}, // LivingWorldBuffer
        {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}, // MaterialOverrides
        {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR} // Reserved
    }};

    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkDescriptorSetLayout> emptyDescriptorSetLayout_;
    Handle<VkDescriptorSetLayout> texDescriptorSetLayout_;

    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkPipeline>            rtPipeline_;
    Handle<VkPipeline>            computePipeline_;  // living world breathing

    Handle<VkBuffer>              sbtBuffer_;
    Handle<VkDeviceMemory>        sbtMemory_;  // Manual SBT cleanup

    VkDeviceSize                  sbtAddress_{0};
    VkDeviceSize                  sbtSize_{0};

    std::vector<Handle<VkShaderModule>> shaderModules_;

    uint32_t raygenGroupCount_{1};
    uint32_t missGroupCount_{1};
    uint32_t hitGroupCount_{1};

    Handle<VkBuffer>                  dummyAccelBuffer_;
    Handle<VkDeviceMemory>            dummyAccelMemory_;
    Handle<VkAccelerationStructureKHR> dummyTLAS_;

    // Descriptor buffer members (eternal, mapped, growable)
    uint64_t descriptorBufferHandle_ = 0;
    void* descriptorMapped_ = nullptr;
    VkDeviceAddress descriptorBufferAddress_ = 0;
    VkDeviceSize currentMappedSize_ = 0;      // Current total mapped bytes
    VkDeviceSize currentWriteOffset_ = 0;     // Next write position

    std::array<VkDeviceSize, kMainBindings.size()> bindingOffsets_{};
    VkPhysicalDeviceDescriptorBufferPropertiesEXT descProps_{};

    VkAccelerationStructureKHR createDummyTLAS();

    void cacheDescriptorProperties();
    void growDescriptorBuffer(VkDeviceSize additionalSize);  // Future: dynamic growth
};

[[nodiscard]] inline PipelineManager& pipeline() noexcept {
    return PipelineManager::instance();
}

} // namespace RTX

// =============================================================================
// HEADER — JANUARY 25, 2026
// - Switched to VK_EXT_descriptor_buffer: no sets, memcpy updates via vkGetDescriptorEXT
// - Eternal descriptor buffer (host-coherent mapped) for zero CPU overhead
// - Removed descriptor set allocation / binding / accessors
// - Binding via vkCmdBindDescriptorBuffersEXT + offsets
// - Living world at 7, materials at 3 — updated via direct writes
// - Push constant: 4 bytes only (time float) — dt killed
// - Descriptor buffer now minimal/growable — no layout size query (trashed sets)
// Empire upgraded — pink photons bindless & breathing free — AMOURANTH FOREVER 💖
// =============================================================================