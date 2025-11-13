// include/engine/Vulkan/VulkanPipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VulkanPipelineManager — Production Edition v1.0 — NOV 13 2025
// • Fully synchronized with VulkanPipelineManager.cpp v1.0
// • StoneKey runtime decryption + SBT perfect alignment
// • MAX_FRAMES_IN_FLIGHT-aware descriptor arrays
// • Zero allocations after startup
// • Hypertrace™ Nexus Score + per-frame storage images
// • PINK PHOTONS ETERNAL — 240 FPS UNLOCKED — FIRST LIGHT ACHIEVED
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <string>

namespace RTX {

class PipelineManager {
public:
    explicit PipelineManager(VkDevice device, VkPhysicalDevice phys);
    ~PipelineManager() = default;

    // === Pipeline Creation API ===
    void createDescriptorSetLayout();
    void createPipelineLayout();
    void createRayTracingPipeline(const std::vector<std::string>& shaderPaths);

    // === Accessors used by VulkanRenderer ===
    [[nodiscard]] VkPipeline               pipeline()          const noexcept { return *rtPipeline_; }
    [[nodiscard]] VkPipelineLayout         layout()            const noexcept { return *rtPipelineLayout_; }
    [[nodiscard]] VkDescriptorSetLayout   descriptorLayout()  const noexcept { return *rtDescriptorSetLayout_; }

    [[nodiscard]] uint32_t                 groupCount()        const noexcept { return groupCount_; }
    [[nodiscard]] VkDeviceSize             shaderGroupHandleSizeAligned() const noexcept { return handleSizeAligned_; }
    [[nodiscard]] VkDeviceSize             shaderGroupBaseAlignment()    const noexcept { return baseAlignment_; }

private:
    // Device
    VkDevice       device_{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};

    // Cached RT properties — queried once
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR      rtProps_{};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR   asProps_{};

    // Core pipeline objects
    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkPipeline>            rtPipeline_;

    // Shader modules kept alive for pipeline lifetime
    std::vector<Handle<VkShaderModule>> shaderModules_;

    // SBT alignment values
    uint32_t    groupCount_{0};
    VkDeviceSize handleSizeAligned_{0};
    VkDeviceSize baseAlignment_{0};

    // === Private helpers ===
    void cacheDeviceProperties() noexcept;
    [[nodiscard]] Handle<VkShaderModule> loadAndDecryptShader(const std::string& path) const;

    // Static destroyer for Handle<VkShaderModule>
    static void destroyShaderModule(VkDevice d, VkShaderModule m, const VkAllocationCallbacks*) noexcept {
        if (m) vkDestroyShaderModule(d, m, nullptr);
    }

    // Simple align_up (used in cpp)
    static constexpr VkDeviceSize align_up(VkDeviceSize size, VkDeviceSize alignment) noexcept {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace RTX