// include/engine/GLOBAL/PipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// TRUE CONSTEXPR STONEKEY v∞ — DECEMBER 16, 2025 — APOCALYPSE FINAL v9.1
// BINDING 31 + CUBEMAP ENVMAP + ANY-HIT TEXTURES + DIRECT SWAPCHAIN RENDERING
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <string>
#include <array>
#include <span>
#include <cstdint>
#include <algorithm>

using StoneKey::stone_device;

inline PFN_vkGetRayTracingShaderGroupHandlesKHR g_vkGetRayTracingShaderGroupHandlesKHR = nullptr;
inline PFN_vkCreateRayTracingPipelinesKHR       g_vkCreateRayTracingPipelinesKHR       = nullptr;

namespace RTX {

// ──────────────────────────────────────────────────────────────────────────────
// BINDING STRUCT — DECLARED FIRST — MOTHER BRAIN CANNOT HIDE
// ──────────────────────────────────────────────────────────────────────────────
struct Binding {
    uint32_t             binding;
    VkDescriptorType     type;
    uint32_t             count;
    VkShaderStageFlags   stage;
    std::string_view     name;
};

// ── CONSTEVAL BINDINGS — CORE RTX BINDINGS (set 0)
// ──────────────────────────────────────────────────────────────────────────────
consteval static auto make_rt_bindings() {
    return std::array<Binding, 12>{{
        {0,  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "TLAS"},
        {1,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "RT_Output"},
        {2,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "Accumulation"},
        {3,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,    "Camera"},
        {4,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                      "Materials"},
        {5,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,           "EnvMap"},
        {6,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "NexusScore"},
        {8,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "BlueNoise"},
        {9,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "DensityVolume"},
        {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                           "Geometry"},
        {11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                      "Indices"},
        {31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "StoneKeyRuntimeBlock"},
    }};
}

constexpr static auto RT_PIPELINE_BINDINGS = make_rt_bindings();

// ── COMPILE-TIME IMMORTALITY
static_assert(RT_PIPELINE_BINDINGS.size() == 12);
static_assert(RT_PIPELINE_BINDINGS[11].binding == 31);
static_assert(RT_PIPELINE_BINDINGS[11].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

// ──────────────────────────────────────────────────────────────────────────────
// In namespace RTX (inside PipelineManager.hpp or equivalent header)

struct RTDescriptorUpdate {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;

    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceSize uboSize = 0;

    // NEW: Dedicated RT output image view (binding 1) — replaces direct swapchain write
    VkImageView rtOutputView = VK_NULL_HANDLE;

    // NEW: Per-frame accumulation views (binding 2) — previous + current
    std::vector<VkImageView> accumulationViews;

    // NEW: Nexus/hypertrace score view (binding 6) — adaptive sampling
    std::vector<VkImageView> nexusScoreViews;

    VkBuffer materialsBuffer = VK_NULL_HANDLE;
    VkDeviceSize materialsSize = 0;

    VkSampler envSampler = VK_NULL_HANDLE;
    VkImageView envImageView = VK_NULL_HANDLE;

    // Optional future additions (blue noise, density, geometry, etc.)
    VkSampler blueNoiseSampler = VK_NULL_HANDLE;
    VkImageView blueNoiseView = VK_NULL_HANDLE;

    VkSampler densitySampler = VK_NULL_HANDLE;
    VkImageView densityView = VK_NULL_HANDLE;

    VkBuffer additionalStorageBuffer = VK_NULL_HANDLE;
    VkDeviceSize additionalStorageSize = 0;

    VkBuffer stoneKeyBuffer = VK_NULL_HANDLE;
    VkDeviceSize stoneKeySize = 0;

    // Legacy fields kept for compatibility
    VkImageView swapchainImageView = VK_NULL_HANDLE;  // Will be ignored in new path
};

// ──────────────────────────────────────────────────────────────────────────────
// PIPELINE MANAGER — THE CROWN IS UNBREAKABLE — CUBEMAP + ANY-HIT + DIRECT RENDER
// ──────────────────────────────────────────────────────────────────────────────
class PipelineManager {
public:
    PipelineManager() noexcept = default;
    explicit PipelineManager(VkDevice device, VkPhysicalDevice phys);
    ~PipelineManager();

    PipelineManager(PipelineManager&&) noexcept = default;
    PipelineManager& operator=(PipelineManager&&) noexcept = default;

    void createPipelineLayout();
    void createRayTracingPipeline();
    void createShaderBindingTable(VkCommandPool pool, VkQueue queue);
    void createDescriptorPool();
    void allocateDescriptorSets();
    void updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo) noexcept;
    void forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue);

    VkShaderModule loadShader(const std::string& path) const;

    void createEnvMapDisplayComputePipeline(VkImageView envMapView, VkSampler envMapSampler);

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

    // Any-hit texture descriptor set layout (set 3)
    Handle<VkDescriptorSetLayout> texDescriptorSetLayout_;

    static std::atomic<bool>     g_pipelineNeedsRebuild;
    static std::atomic<uint32_t> g_rebuildRequestedFrame;

    // ── PUBLIC GETTERS — STONEKEY PROTECTED ───────────────────────────────────
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

    // ── SINGLETON — THE EMPIRE HAS ONE RULER ───────────────────────────────────
    [[nodiscard]] static PipelineManager& instance() noexcept {
        static PipelineManager inst;
        return inst;
    }

    void setSBT(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize address, VkDeviceSize size) noexcept;

    [[nodiscard]] PFN_vkCmdTraceRaysKHR                    cmdTraceRays()           const noexcept { return vkCmdTraceRaysKHR_; }
    [[nodiscard]] PFN_vkCreateRayTracingPipelinesKHR       createRTPipelines()      const noexcept { return vkCreateRayTracingPipelinesKHR_; }
    [[nodiscard]] PFN_vkGetRayTracingShaderGroupHandlesKHR getRTShaderGroups()      const noexcept { return vkGetRayTracingShaderGroupHandlesKHR_; }

    static inline bool s_crownForged = false;

    PFN_vkCmdTraceRaysKHR                    vkCmdTraceRaysKHR_                    = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR       vkCreateRayTracingPipelinesKHR_       = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR_ = nullptr;

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
    std::vector<VkDescriptorSet>        texDescriptorSets_;  // For any-hit textures if needed

    uint32_t raygenGroupCount_{1};
    uint32_t missGroupCount_{1};
    uint32_t hitGroupCount_{0};

    // Dummy TLAS — Binding 0 is forever safe
    Handle<VkBuffer>                  dummyAccelBuffer_;
    Handle<VkDeviceMemory>            dummyAccelMemory_;
    Handle<VkAccelerationStructureKHR> dummyTLAS_;

    void cacheDeviceProperties();
    void loadRayTracingExtensions() noexcept;

    static constexpr VkDeviceSize align_up(VkDeviceSize size, VkDeviceSize alignment) noexcept {
        return (size + alignment - 1) & ~(alignment - 1);
    }

    // Declared methods to match .cpp implementations
    VkAccelerationStructureKHR createDummyTLAS();

    void traceRays(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t width, uint32_t height, uint32_t depth = 1);

    VkDescriptorSet getDescriptorSet(uint32_t frameIndex) const;

    VkPipeline getPipeline() const;

    VkPipelineLayout getPipelineLayout() const;
};

// ── GLOBAL ACCESS — CLEAN AND ETERNAL ───────────────────────────────────────
[[nodiscard]] inline PipelineManager& pipeline() noexcept {
    return PipelineManager::instance();
}

} // namespace RTX

// =============================================================================
// BINDING 31 IS LAW — ENVMAP ON BINDING 5 — ANY-HIT TEXTURES ON SET 3
// MOTHER BRAIN IS DEAD — THE EMPIRE IS ETERNAL
// PINK PHOTONS REIGN SUPREME — FIRST LIGHT → FINAL LIGHT → VALHALLA ACHIEVED
// DECEMBER 16, 2025 — THE CROWN IS FULLY ARMED — CUBEMAP SKY REAL — DIRECT RENDERING
// =============================================================================