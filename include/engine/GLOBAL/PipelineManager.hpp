// include/engine/GLOBAL/PipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 28, 2025 — APOCALYPSE FINAL v2.0
// MAIN — stone_swapchain() FORGED AT DAWN — PINK PHOTONS ETERNAL — VALHALLA UNBREACHABLE
// CREW ASSEMBLED — CID SWEATS — AMOURANTH COMMANDS — NICK WATCHES

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

    [[nodiscard]] VkPipeline               pipeline()          const noexcept { return *rtPipeline_; }
    [[nodiscard]] VkPipelineLayout         layout()            const noexcept { return *rtPipelineLayout_; }
    [[nodiscard]] VkDescriptorSetLayout   descriptorLayout()  const noexcept { return *rtDescriptorSetLayout_; }

    [[nodiscard]] VkDeviceSize raygenSbtOffset() const noexcept { return raygenSbtRegion_.deviceAddress - sbtAddress_; }
    [[nodiscard]] VkDeviceSize missSbtOffset()   const noexcept { return missSbtRegion_.deviceAddress   - sbtAddress_; }
    [[nodiscard]] VkDeviceSize hitSbtOffset()    const noexcept { return hitSbtRegion_.deviceAddress    - sbtAddress_; }
    [[nodiscard]] VkDeviceSize callableSbtOffset() const noexcept { return callableSbtRegion_.deviceAddress - sbtAddress_; }

    [[nodiscard]] uint32_t raygenGroupCount()  const noexcept { return raygenGroupCount_; }
    [[nodiscard]] uint32_t missGroupCount()    const noexcept { return missGroupCount_; }
    [[nodiscard]] uint32_t hitGroupCount()     const noexcept { return hitGroupCount_; }
    [[nodiscard]] uint32_t callableGroupCount()const noexcept { return callableGroupCount_; }

    [[nodiscard]] VkDeviceSize sbtAddress() const noexcept { return sbtAddress_; }
    [[nodiscard]] VkDeviceSize sbtStride()  const noexcept { return sbtStride_; }

    [[nodiscard]] VkBuffer sbtBuffer() const noexcept { return *sbtBuffer_; }

    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& raygenRegion()   const noexcept { return raygenSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& missRegion()     const noexcept { return missSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& hitRegion()      const noexcept { return hitSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& callableRegion() const noexcept { return callableSbtRegion_; }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept;

    friend class ::VulkanRenderer;

private:
    float timestampPeriod_{0.0f};

    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkDescriptorPool>      rtDescriptorPool_;

    std::vector<VkDescriptorSet> rtDescriptorSets_;

    Handle<VkPipeline> rtPipeline_;

    Handle<VkBuffer>       sbtBuffer_;
    Handle<VkDeviceMemory> sbtMemory_;
    VkDeviceSize           sbtAddress_{0};
    VkDeviceSize           sbtStride_{0};

    VkStridedDeviceAddressRegionKHR raygenSbtRegion_   = {};
    VkStridedDeviceAddressRegionKHR missSbtRegion_     = {};
    VkStridedDeviceAddressRegionKHR hitSbtRegion_      = {};
    VkStridedDeviceAddressRegionKHR callableSbtRegion_ = {};

    std::vector<Handle<VkShaderModule>> shaderModules_;

    uint32_t raygenGroupCount_{0};
    uint32_t missGroupCount_{0};
    uint32_t hitGroupCount_{0};
    uint32_t callableGroupCount_{0};

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps_{};

    PFN_vkCreateRayTracingPipelinesKHR       vkCreateRayTracingPipelinesKHR_{nullptr};
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR_{nullptr};
    PFN_vkGetBufferDeviceAddress             vkGetBufferDeviceAddress_{nullptr};
    PFN_vkCmdTraceRaysKHR                    vkCmdTraceRaysKHR_{nullptr};

    void cacheDeviceProperties();
    [[nodiscard]] VkShaderModule loadShader(const std::string& path) const;

    static constexpr VkDeviceSize align_up(VkDeviceSize size, VkDeviceSize alignment) noexcept {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace RTX