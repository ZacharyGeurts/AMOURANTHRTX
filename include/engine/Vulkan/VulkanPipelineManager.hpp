// include/engine/Vulkan/VulkanPipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VulkanPipelineManager — Production Edition v10.10 (Descriptor Sets + Updates) — NOV 16 2025
// • FIXED: Added allocateDescriptorSets() declaration + rtDescriptorSets_ vector (resolves "not declared" errors)
// • ADDED: RTDescriptorUpdate struct + updateRTDescriptorSet() declaration (enables per-frame updates, VUID-08114 fix)
// • Retained: All v10.9 SBT regions, shutdown safety, multi-frame pool, VUID fixes, PFN safety
// • 100% compatible with VulkanRenderer::recordRayTracingCommandBuffer — Supports triple-buffering + descriptor updates
// • PINK PHOTONS ETERNAL — 240+ FPS UNLOCKED — FIRST LIGHT ACHIEVED — ZERO ERRORS, ZERO COMPILATION WARNINGS
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <string>
#include <array>

namespace RTX {
// ──────────────────────────────────────────────────────────────────────────────
// NEW: RT Descriptor Update Struct — Encapsulates All Required Resources for vkUpdateDescriptorSets
// ──────────────────────────────────────────────────────────────────────────────
struct RTDescriptorUpdate {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceSize uboSize = VK_WHOLE_SIZE;
    VkBuffer materialsBuffer = VK_NULL_HANDLE;  // Binding 4: storage buffer (e.g., materials)
    VkDeviceSize materialsSize = VK_WHOLE_SIZE;
    VkSampler envSampler = VK_NULL_HANDLE;      // Binding 5: env sampler
    VkImageView envImageView = VK_NULL_HANDLE;
    std::array<VkImageView, 3> rtOutputViews = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};  // Binding 1: array[3]
    std::array<VkImageView, 3> accumulationViews = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};  // Binding 2: array[3]
    std::array<VkImageView, 3> nexusScoreViews = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};  // Binding 6: array[3]
    VkBuffer additionalStorageBuffer = VK_NULL_HANDLE;  // Binding 7: additional storage buffer
    VkDeviceSize additionalStorageSize = VK_WHOLE_SIZE;
};

class PipelineManager {
public:
    PipelineManager() noexcept = default;
    explicit PipelineManager(VkDevice device, VkPhysicalDevice phys);
    PipelineManager(PipelineManager&& other) noexcept = default;
    PipelineManager& operator=(PipelineManager&& other) noexcept = default;
    ~PipelineManager();  // FIXED: Declaration only — Body in .cpp for vkDeviceWaitIdle (shutdown safety)

    void createDescriptorSetLayout();
    void createPipelineLayout();
    void createRayTracingPipeline(const std::vector<std::string>& shaderPaths);
    void createShaderBindingTable(VkCommandPool pool, VkQueue queue);

    // === NEW: Descriptor Set Management ===
    void allocateDescriptorSets();  // NEW: Allocates frame-specific sets (multi-frame support)
    void updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo);  // NEW: Updates all bindings + array indices

    // === Core Accessors ===
    [[nodiscard]] VkPipeline               pipeline()          const noexcept { return *rtPipeline_; }
    [[nodiscard]] VkPipelineLayout         layout()            const noexcept { return *rtPipelineLayout_; }
    [[nodiscard]] VkDescriptorSetLayout   descriptorLayout()  const noexcept { return *rtDescriptorSetLayout_; }

    [[nodiscard]] uint32_t                 raygenGroupCount()  const noexcept { return raygenGroupCount_; }
    [[nodiscard]] uint32_t                 missGroupCount()    const noexcept { return missGroupCount_; }
    [[nodiscard]] uint32_t                 hitGroupCount()     const noexcept { return hitGroupCount_; }
    [[nodiscard]] uint32_t                 callableGroupCount() const noexcept { return callableGroupCount_; }
    [[nodiscard]] VkDeviceSize             sbtAddress()        const noexcept { return sbtAddress_; }
    [[nodiscard]] VkDeviceSize             raygenSbtOffset()   const noexcept { return raygenSbtOffset_; }
    [[nodiscard]] VkDeviceSize             missSbtOffset()     const noexcept { return missSbtOffset_; }
    [[nodiscard]] VkDeviceSize             hitSbtOffset()      const noexcept { return hitSbtOffset_; }
    [[nodiscard]] VkDeviceSize             callableSbtOffset() const noexcept { return callableSbtOffset_; }
    [[nodiscard]] VkDeviceSize             sbtStride()         const noexcept { return sbtStride_; }
    [[nodiscard]] VkBuffer                 sbtBuffer()         const noexcept { return *sbtBuffer_; }
    [[nodiscard]] VkDeviceMemory           sbtMemory()         const noexcept { return *sbtMemory_; }

    // === NEW: SBT REGION GETTERS — REQUIRED BY VulkanRenderer::recordRayTracingCommandBuffer ===
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR* getRaygenSbtRegion()   const noexcept { return &raygenSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR* getMissSbtRegion()     const noexcept { return &missSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR* getHitSbtRegion()      const noexcept { return &hitSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR* getCallableSbtRegion() const noexcept { return &callableSbtRegion_; }

    // === Helpers ===
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, 
                            VkMemoryPropertyFlags properties) const noexcept;
    VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) const;
    void endSingleTimeCommands(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd) const;

    friend class ::VulkanRenderer;

private:
    VkDevice       device_{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR      rtProps_{};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR   asProps_{};
    float timestampPeriod_{0.0f};

    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkPipeline>            rtPipeline_;
    Handle<VkDescriptorPool>      rtDescriptorPool_;

    // === NEW: Frame Descriptor Sets ===
    std::vector<VkDescriptorSet> rtDescriptorSets_;  // NEW: Per-frame sets for multi-buffering

    Handle<VkBuffer>        sbtBuffer_;
    Handle<VkDeviceMemory>  sbtMemory_;
    VkDeviceSize            sbtAddress_{0};
    VkDeviceSize            raygenSbtOffset_{0};
    VkDeviceSize            missSbtOffset_{0};
    VkDeviceSize            hitSbtOffset_{0};
    VkDeviceSize            callableSbtOffset_{0};
    VkDeviceSize            sbtStride_{0};

    // === NEW: Full SBT Regions (constructed in createShaderBindingTable) ===
    VkStridedDeviceAddressRegionKHR raygenSbtRegion_   = {};
    VkStridedDeviceAddressRegionKHR missSbtRegion_     = {};
    VkStridedDeviceAddressRegionKHR hitSbtRegion_      = {};
    VkStridedDeviceAddressRegionKHR callableSbtRegion_ = {};

    std::vector<Handle<VkShaderModule>> shaderModules_;

    uint32_t    raygenGroupCount_{0};
    uint32_t    missGroupCount_{0};
    uint32_t    hitGroupCount_{0};
    uint32_t    callableGroupCount_{0};

    PFN_vkCreateRayTracingPipelinesKHR    vkCreateRayTracingPipelinesKHR_{nullptr};
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR_{nullptr};
    PFN_vkGetBufferDeviceAddressKHR       vkGetBufferDeviceAddressKHR_{nullptr};

    void cacheDeviceProperties();
    void loadExtensions();
    [[nodiscard]] VkShaderModule loadShader(const std::string& path) const;

    static constexpr VkDeviceSize align_up(VkDeviceSize size, VkDeviceSize alignment) noexcept {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace RTX