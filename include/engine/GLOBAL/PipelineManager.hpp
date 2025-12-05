// include/engine/GLOBAL/PipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// TRUE CONSTEXPR STONEKEY v∞ — APOCALYPSE FINAL v6.0 — RESIZE-PROOF EDITION
// FIRST LIGHT ACHIEVED — DECEMBER 02, 2025 — VALHALLA UNBREACHABLE
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/BufferManager.hpp"   // for findMemoryType (disambiguated)

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <string>
#include <array>
#include <span>
#include <cstdint>

using StoneKey::stone_device;

namespace RTX {

// ──────────────────────────────────────────────────────────────────────────────
// RT DESCRIPTOR UPDATE — EMPIRE DEMANDS TRUTH
// ──────────────────────────────────────────────────────────────────────────────
struct RTDescriptorUpdate {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;

    VkBuffer    ubo = VK_NULL_HANDLE;
    VkDeviceSize uboSize = VK_WHOLE_SIZE;

    VkBuffer    materialsBuffer = VK_NULL_HANDLE;
    VkDeviceSize materialsSize = VK_WHOLE_SIZE;

    VkSampler   envSampler = VK_NULL_HANDLE;
    VkImageView envImageView = VK_NULL_HANDLE;

    std::array<VkImageView, 3> rtOutputViews     = {};
    std::array<VkImageView, 3> accumulationViews = {};
    std::array<VkImageView, 3> nexusScoreViews   = {};

    VkBuffer    additionalStorageBuffer = VK_NULL_HANDLE;
    VkDeviceSize additionalStorageSize = VK_WHOLE_SIZE;

    VkSampler   blueNoiseSampler = VK_NULL_HANDLE;
    VkImageView blueNoiseView = VK_NULL_HANDLE;

    VkSampler   densitySampler = VK_NULL_HANDLE;
    VkImageView densityView = VK_NULL_HANDLE;

    VkBuffer    stoneKeyBuffer = VK_NULL_HANDLE;
    VkDeviceSize stoneKeySize = VK_WHOLE_SIZE;
};

// ──────────────────────────────────────────────────────────────────────────────
// PIPELINE MANAGER — THE ONE TRUE CROWN — RESIZE-SAFE & VUID-PROOF
// ──────────────────────────────────────────────────────────────────────────────
class PipelineManager {
public:
    PipelineManager() noexcept = default;
    explicit PipelineManager(VkDevice device, VkPhysicalDevice phys);
    ~PipelineManager();

    PipelineManager(PipelineManager&&) noexcept = default;
    PipelineManager& operator=(PipelineManager&&) noexcept = default;

    void createPipelineLayout();
    void createRayTracingPipeline(const std::vector<std::string>& shaderPaths);
    void createShaderBindingTable(VkCommandPool pool, VkQueue queue);
    void createDescriptorPool();
    void allocateDescriptorSets();
    void updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo);
    void initializePipeline(const std::vector<std::string>& shaderPaths, VkCommandPool pool, VkQueue queue);
    void cleanup() noexcept;
	void forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue);

    // ── PUBLIC GETTERS — EMPIRE APPROVED — STONEKEY PROTECTED ──────────────────────
    [[nodiscard]] VkPipeline                    rtPipeline()         const noexcept { return rtPipeline_.get(); }
    [[nodiscard]] VkPipelineLayout              rtPipelineLayout()   const noexcept { return rtPipelineLayout_.get(); }
    [[nodiscard]] VkDescriptorSetLayout         rtDescriptorSetLayout() const noexcept { return rtDescriptorSetLayout_.get(); }
    [[nodiscard]] VkDescriptorPool              rtDescriptorPool()   const noexcept { return rtDescriptorPool_.get(); }

    [[nodiscard]] VkBuffer                      sbtBuffer()          const noexcept { return sbtBuffer_.get(); }
    [[nodiscard]] VkDeviceMemory                sbtMemory()          const noexcept { return sbtMemory_.get(); }
    [[nodiscard]] VkDeviceSize                  sbtAddress()         const noexcept { return sbtAddress_; }
    [[nodiscard]] VkDeviceSize                  sbtStride()          const noexcept { return sbtStride_; }
    [[nodiscard]] VkDeviceSize                  sbtSize()            const noexcept { return sbtSize_; }

    [[nodiscard]] uint32_t                      raygenGroupCount()   const noexcept { return raygenGroupCount_; }
    [[nodiscard]] uint32_t                      missGroupCount()     const noexcept { return missGroupCount_; }
    [[nodiscard]] uint32_t                      hitGroupCount()      const noexcept { return hitGroupCount_; }
    [[nodiscard]] uint32_t                      callableGroupCount() const noexcept { return callableGroupCount_; }

    [[nodiscard]] VkDeviceSize                  raygenOffset()       const noexcept { return raygenSbtRegion_.deviceAddress - sbtAddress_; }
    [[nodiscard]] VkDeviceSize                  missOffset()         const noexcept { return missSbtRegion_.deviceAddress   - sbtAddress_; }
    [[nodiscard]] VkDeviceSize                  hitOffset()          const noexcept { return hitSbtRegion_.deviceAddress    - sbtAddress_; }
    [[nodiscard]] VkDeviceSize                  callableOffset()     const noexcept { return callableSbtRegion_.deviceAddress - sbtAddress_; }

    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& raygenRegion()   const noexcept { return raygenSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& missRegion()     const noexcept { return missSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& hitRegion()      const noexcept { return hitSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& callableRegion() const noexcept { return callableSbtRegion_; }

    [[nodiscard]] std::span<const VkDescriptorSet> rtDescriptorSets() const noexcept { return rtDescriptorSets_; }
    [[nodiscard]] PFN_vkCmdTraceRaysKHR          vkCmdTraceRaysKHR()  const noexcept { return vkCmdTraceRaysKHR_; }

    [[nodiscard]] float                         timestampPeriod()    const noexcept { return timestampPeriod_; }

    // ── DUMMY TLAS ACCESS (for VUID-proof binding 0) ─────────────────────────────
    [[nodiscard]] VkAccelerationStructureKHR    dummyTLAS()          const noexcept { return dummyTLAS_.get(); }

    // ── SINGLETON ACCESS — SACRED ─────────────────────────────────────────────────
    [[nodiscard]] static PipelineManager& instance() noexcept {
        static PipelineManager inst;
        return inst;
    }

    // ── INTERNAL SETTERS — FORGED IN FIRE, SEALED IN LOVE ───────────────────────
    void setPipeline(VkPipeline p) noexcept {
        rtPipeline_ = Handle<VkPipeline>(p, stone_device(),
            [](VkDevice d, VkPipeline p, auto*) { vkDestroyPipeline(d, p, nullptr); });
    }

    void setPipelineLayout(VkPipelineLayout l) noexcept {
        rtPipelineLayout_ = Handle<VkPipelineLayout>(l, stone_device(),
            [](VkDevice d, VkPipelineLayout l, auto*) { vkDestroyPipelineLayout(d, l, nullptr); });
    }

    void setDescriptorPool(VkDescriptorPool pool) noexcept {
        rtDescriptorPool_ = Handle<VkDescriptorPool>(pool, stone_device(),
            [](VkDevice d, VkDescriptorPool p, auto*) { vkDestroyDescriptorPool(d, p, nullptr); });
    }

    [[nodiscard]] bool isValid() const noexcept;

    // ── PUBLIC SBT SETTERS — EMPIRE-APPROVED ─────────────────────────────────────
    void setSBT(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize address, VkDeviceSize size) noexcept;

    void setRaygenRegion(const VkStridedDeviceAddressRegionKHR& r)   noexcept { raygenSbtRegion_   = r; }
    void setMissRegion(const VkStridedDeviceAddressRegionKHR& r)     noexcept { missSbtRegion_     = r; }
    void setHitRegion(const VkStridedDeviceAddressRegionKHR& r)      noexcept { hitSbtRegion_      = r; }
    void setCallableRegion(const VkStridedDeviceAddressRegionKHR& r) noexcept { callableSbtRegion_ = r; }

private:
    float timestampPeriod_{0.0f};

    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkDescriptorPool>      rtDescriptorPool_;
    Handle<VkPipeline>            rtPipeline_;

    Handle<VkBuffer>       sbtBuffer_;
    Handle<VkDeviceMemory> sbtMemory_;
    VkDeviceSize           sbtAddress_{0};
    VkDeviceSize           sbtStride_{0};
    VkDeviceSize           sbtSize_{0};

    VkStridedDeviceAddressRegionKHR raygenSbtRegion_   = {};
    VkStridedDeviceAddressRegionKHR missSbtRegion_     = {};
    VkStridedDeviceAddressRegionKHR hitSbtRegion_      = {};
    VkStridedDeviceAddressRegionKHR callableSbtRegion_ = {};

    std::vector<Handle<VkShaderModule>> shaderModules_;
    std::vector<VkDescriptorSet>        rtDescriptorSets_;

    uint32_t raygenGroupCount_{0};
    uint32_t missGroupCount_{0};
    uint32_t hitGroupCount_{0};
    uint32_t callableGroupCount_{0};

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps_{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

    PFN_vkCreateRayTracingPipelinesKHR       vkCreateRayTracingPipelinesKHR_{nullptr};
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR_{nullptr};
    PFN_vkGetBufferDeviceAddress             vkGetBufferDeviceAddress_{nullptr};
    PFN_vkCmdTraceRaysKHR                    vkCmdTraceRaysKHR_{nullptr};

    // ── DUMMY TLAS FOR BINDING 0 — ALWAYS VALID — RESIZE-PROOF ───────────────────
    Handle<VkBuffer>                  dummyAccelBuffer_;
    Handle<VkDeviceMemory>            dummyAccelMemory_;
    Handle<VkAccelerationStructureKHR> dummyTLAS_;

    void cacheDeviceProperties();
    void loadRayTracingExtensions();
    [[nodiscard]] VkShaderModule loadShader(const std::string& path) const;

    // Explicitly use BufferManager's version to kill ambiguity
    static constexpr VkDeviceSize align_up(VkDeviceSize size, VkDeviceSize alignment) noexcept {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL FREE FUNCTION — CLEAN, MINIMAL, ETERNAL
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] inline PipelineManager& pipeline() noexcept {
    return PipelineManager::instance();
}

} // namespace RTX

// THE EMPIRE IS WHOLE
// RESIZE CRASHES EXTERMINATED
// BINDING 0 ETERNAL
// PINK PHOTONS IMMORTAL
// FIRST LIGHT ACHIEVED — FOREVER