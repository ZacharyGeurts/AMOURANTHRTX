// include/engine/GLOBAL/PipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 28, 2025 — APOCALYPSE FINAL v2.2
// CAPTAIN N EDITION — HE SAVED VIDEOLAND — WE OWE HIM EVERYTHING
// FIXED: Handle<T> raw-pointer constructor removed — now 100% compatible
// PINK PHOTONS ETERNAL — VALHALLA UNBREACHABLE — FIRST LIGHT RESTORED
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/bindings.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <string>
#include <array>
#include <span>

using StoneKey::stone_device;
using StoneKey::stone_physical;

namespace RTX {

struct RTDescriptorUpdate {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceSize uboSize = VK_WHOLE_SIZE;
    VkBuffer materialsBuffer = VK_NULL_HANDLE;
    VkDeviceSize materialsSize = VK_WHOLE_SIZE;
    VkSampler envSampler = VK_NULL_HANDLE;
    VkImageView envImageView = VK_NULL_HANDLE;
    std::array<VkImageView, 3> rtOutputViews     = {};
    std::array<VkImageView, 3> accumulationViews = {};
    std::array<VkImageView, 3> nexusScoreViews   = {};
    VkBuffer additionalStorageBuffer = VK_NULL_HANDLE;
    VkDeviceSize additionalStorageSize = VK_WHOLE_SIZE;
};

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
    void allocateDescriptorSets();
    void updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo);

    // GETTERS — EVERYTHING
    [[nodiscard]] VkPipeline                    pipeline()           const noexcept { return rtPipeline_.get(); }
    [[nodiscard]] VkPipelineLayout              layout()             const noexcept { return rtPipelineLayout_.get(); }
    [[nodiscard]] VkDescriptorSetLayout         descriptorLayout()   const noexcept { return rtDescriptorSetLayout_.get(); }
    [[nodiscard]] VkDescriptorPool              descriptorPool()     const noexcept { return rtDescriptorPool_.get(); }
    [[nodiscard]] VkBuffer                      sbtBuffer()          const noexcept { return sbtBuffer_.get(); }
    [[nodiscard]] VkDeviceMemory                sbtMemory()          const noexcept { return sbtMemory_.get(); }

    [[nodiscard]] VkDeviceSize                 sbtAddress()         const noexcept { return sbtAddress_; }
    [[nodiscard]] VkDeviceSize                 sbtStride()          const noexcept { return sbtStride_; }
    [[nodiscard]] VkDeviceSize                 sbtSize()            const noexcept { return sbtBufferSize_; }

    [[nodiscard]] uint32_t                      raygenGroupCount()   const noexcept { return raygenGroupCount_; }
    [[nodiscard]] uint32_t                      missGroupCount()     const noexcept { return missGroupCount_; }
    [[nodiscard]] uint32_t                      hitGroupCount()      const noexcept { return hitGroupCount_; }
    [[nodiscard]] uint32_t                      callableGroupCount() const noexcept { return callableGroupCount_; }

    [[nodiscard]] VkDeviceSize                  raygenSbtOffset()    const noexcept { return raygenSbtRegion_.deviceAddress - sbtAddress_; }
    [[nodiscard]] VkDeviceSize                  missSbtOffset()      const noexcept { return missSbtRegion_.deviceAddress   - sbtAddress_; }
    [[nodiscard]] VkDeviceSize                  hitSbtOffset()       const noexcept { return hitSbtRegion_.deviceAddress    - sbtAddress_; }
    [[nodiscard]] VkDeviceSize                  callableSbtOffset()  const noexcept { return callableSbtRegion_.deviceAddress - sbtAddress_; }

    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& raygenRegion()   const noexcept { return raygenSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& missRegion()     const noexcept { return missSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& hitRegion()      const noexcept { return hitSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& callableRegion() const noexcept { return callableSbtRegion_; }

    [[nodiscard]] std::span<const VkDescriptorSet> descriptorSets() const noexcept { return rtDescriptorSets_; }
    [[nodiscard]] float                         timestampPeriod()    const noexcept { return timestampPeriod_; }
    [[nodiscard]] const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtProperties() const noexcept { return rtProps_; }

    // SETTERS — NOW 100% COMPATIBLE WITH YOUR Handle<T>
    void setDescriptorPool(VkDescriptorPool pool) noexcept {
        rtDescriptorPool_ = Handle<VkDescriptorPool>(pool, stone_device(),
            [](VkDevice d, VkDescriptorPool p, auto*) { vkDestroyDescriptorPool(d, p, nullptr); });
    }

    void setDescriptorSetLayout(VkDescriptorSetLayout layout) noexcept {
        rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(layout, stone_device(),
            [](VkDevice d, VkDescriptorSetLayout l, auto*) { vkDestroyDescriptorSetLayout(d, l, nullptr); });
    }

    [[nodiscard]] bool isValid() const noexcept {
        return stone_device() != VK_NULL_HANDLE &&
               rtPipeline_.valid() &&
               rtPipelineLayout_.valid() &&
               sbtBuffer_.valid();
    }

    friend class ::VulkanRenderer;

private:
    float timestampPeriod_{0.0f};

    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkDescriptorPool>      rtDescriptorPool_;
    Handle<VkPipeline>            rtPipeline_;

    Handle<VkBuffer>              sbtBuffer_;
    Handle<VkDeviceMemory>        sbtMemory_;
    VkDeviceSize                  sbtAddress_{0};
    VkDeviceSize                  sbtStride_{0};
    VkDeviceSize                  sbtBufferSize_{0};

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

    void cacheDeviceProperties();
    void loadRayTracingExtensions();
    [[nodiscard]] VkShaderModule loadShader(const std::string& path) const;

    static constexpr VkDeviceSize align_up(VkDeviceSize size, VkDeviceSize alignment) noexcept {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace RTX

// CAPTAIN N SAVED VIDEOLAND
// HE DESERVES THIS HEADER
// PINK PHOTONS ETERNAL — FIRST LIGHT RESTORED — NOV 28 2025