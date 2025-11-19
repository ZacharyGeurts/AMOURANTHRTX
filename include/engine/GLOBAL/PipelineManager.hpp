// include/engine/Vulkan/VulkanPipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VulkanPipelineManager — STONEKEY v∞ EDITION — NOV 18 2025
// • FULLY STONEKEY-COMPLIANT: ZERO RAW HANDLES STORED
// • NO device_ / physicalDevice_ MEMBERS — ALL ACCESS VIA g_device() / g_PhysicalDevice()
// • Constructor calls set_g_device() / set_g_PhysicalDevice() — immediate obfuscation
// • All internal guards and Vulkan calls use StoneKey accessors
// • Ready for transition_to_obfuscated() — Valhalla-secure post-first-frame
// • PINK PHOTONS ETERNAL — VALHALLA ACHIEVED — ZERO EXPLOIT WINDOW
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
// RT Descriptor Update Struct — Unchanged, Perfect
// ──────────────────────────────────────────────────────────────────────────────
struct RTDescriptorUpdate {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceSize uboSize = VK_WHOLE_SIZE;
    VkBuffer materialsBuffer = VK_NULL_HANDLE;
    VkDeviceSize materialsSize = VK_WHOLE_SIZE;
    VkSampler envSampler = VK_NULL_HANDLE;
    VkImageView envImageView = VK_NULL_HANDLE;
    std::array<VkImageView, 3> rtOutputViews     = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::array<VkImageView, 3> accumulationViews = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::array<VkImageView, 3> nexusScoreViews   = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkBuffer additionalStorageBuffer = VK_NULL_HANDLE;
    VkDeviceSize additionalStorageSize = VK_WHOLE_SIZE;
};

class PipelineManager {
public:
    PipelineManager() noexcept = default;
    
    // Constructor now IMMEDIATELY secures handles via StoneKey
    explicit PipelineManager(VkDevice device, VkPhysicalDevice phys);
    
    PipelineManager(PipelineManager&& other) noexcept = default;
    PipelineManager& operator=(PipelineManager&& other) noexcept = default;
    ~PipelineManager();

    void createDescriptorSetLayout();
    void createPipelineLayout();
    void createRayTracingPipeline(const std::vector<std::string>& shaderPaths);
    void createShaderBindingTable(VkCommandPool pool, VkQueue queue);

    // Descriptor Set Management
    void allocateDescriptorSets();
    void updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo);

    // Core Accessors
    [[nodiscard]] VkPipeline               pipeline()          const noexcept { return *rtPipeline_; }
    [[nodiscard]] VkPipelineLayout         layout()            const noexcept { return *rtPipelineLayout_; }
    [[nodiscard]] VkDescriptorSetLayout   descriptorLayout()  const noexcept { return *rtDescriptorSetLayout_; }

    [[nodiscard]] uint32_t     raygenGroupCount()  const noexcept { return raygenGroupCount_; }
    [[nodiscard]] uint32_t     missGroupCount()    const noexcept { return missGroupCount_; }
    [[nodiscard]] uint32_t     hitGroupCount()     const noexcept { return hitGroupCount_; }
    [[nodiscard]] uint32_t     callableGroupCount()const noexcept { return callableGroupCount_; }
    
    [[nodiscard]] VkDeviceSize sbtAddress()        const noexcept { return sbtAddress_; }
    [[nodiscard]] VkDeviceSize raygenSbtOffset()   const noexcept { return raygenSbtOffset_; }
    [[nodiscard]] VkDeviceSize missSbtOffset()     const noexcept { return missSbtOffset_; }
    [[nodiscard]] VkDeviceSize hitSbtOffset()      const noexcept { return hitSbtOffset_; }
    [[nodiscard]] VkDeviceSize callableSbtOffset() const noexcept { return callableSbtOffset_; }
    [[nodiscard]] VkDeviceSize sbtStride()         const noexcept { return sbtStride_; }
    
    [[nodiscard]] VkBuffer       sbtBuffer() const noexcept { return *sbtBuffer_; }
    [[nodiscard]] VkDeviceMemory sbtMemory() const noexcept { return *sbtMemory_; }

    // SBT Region Getters — Required by Renderer
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR* getRaygenSbtRegion()   const noexcept { return &raygenSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR* getMissSbtRegion()     const noexcept { return &missSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR* getHitSbtRegion()      const noexcept { return &hitSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR* getCallableSbtRegion() const noexcept { return &callableSbtRegion_; }

    // Helpers — Now use StoneKey accessors internally
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                            VkMemoryPropertyFlags properties) const noexcept;
    VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) const;
    void endSingleTimeCommands(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd) const;

    friend class ::VulkanRenderer;

private:
    // REMOVED: VkDevice device_ and VkPhysicalDevice physicalDevice_
    // ALL device access now goes through g_device() and g_PhysicalDevice()
    // This class stores ZERO raw handles — Valhalla-secure

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR      rtProps_{};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR   asProps_{};
    float timestampPeriod_{0.0f};

    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkPipeline>            rtPipeline_;
    Handle<VkDescriptorPool>      rtDescriptorPool_;

    std::vector<VkDescriptorSet> rtDescriptorSets_;  // Per-frame sets

    Handle<VkBuffer>        sbtBuffer_;
    Handle<VkDeviceMemory>  sbtMemory_;
    VkDeviceSize            sbtAddress_{0};
    VkDeviceSize            raygenSbtOffset_{0};
    VkDeviceSize            missSbtOffset_{0};
    VkDeviceSize            hitSbtOffset_{0};
    VkDeviceSize            callableSbtOffset_{0};
    VkDeviceSize            sbtStride_{0};

    VkStridedDeviceAddressRegionKHR raygenSbtRegion_   = {};
    VkStridedDeviceAddressRegionKHR missSbtRegion_     = {};
    VkStridedDeviceAddressRegionKHR hitSbtRegion_      = {};
    VkStridedDeviceAddressRegionKHR callableSbtRegion_ = {};

    std::vector<Handle<VkShaderModule>> shaderModules_;

    uint32_t raygenGroupCount_{0};
    uint32_t missGroupCount_{0};
    uint32_t hitGroupCount_{0};
    uint32_t callableGroupCount_{0};

    // PFN pointers — loaded once, used forever
    PFN_vkCreateRayTracingPipelinesKHR       vkCreateRayTracingPipelinesKHR_{nullptr};
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR_{nullptr};
    PFN_vkGetBufferDeviceAddressKHR          vkGetBufferDeviceAddressKHR_{nullptr};

    // Private methods — all use g_device() / g_PhysicalDevice()
    void cacheDeviceProperties();
    void loadExtensions();
    [[nodiscard]] VkShaderModule loadShader(const std::string& path) const;

    static constexpr VkDeviceSize align_up(VkDeviceSize size, VkDeviceSize alignment) noexcept {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace RTX