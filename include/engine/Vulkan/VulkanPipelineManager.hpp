// include/engine/Vulkan/VulkanPipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VulkanPipelineManager — Production Edition v10.2 (Validation Fixes) — NOV 14 2025
// • Fixed descriptor bindings (0-7) to match raygen shader requirements
// • Removed invalid VkPipelineLibraryCreateInfoKHR from pNext
// • Explicit VK_SHADER_UNUSED_KHR for hit shaders in general groups
// • Added push constant range matching shader stages
// • SBT memory allocation with VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR
// • Exhaustive logging with numbered stack build order
// • StoneKey runtime decryption + SBT perfect alignment
// • MAX_FRAMES_IN_FLIGHT-aware descriptor arrays
// • Zero allocations after startup
// • Hypertrace™ Nexus Score + per-frame storage images
// • Dynamic frame buffering via Options::Performance::MAX_FRAMES_IN_FLIGHT
// • C++23 compliant, -Werror clean
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
#include <array>

namespace RTX {

class PipelineManager {
public:
    explicit PipelineManager(VkDevice device, VkPhysicalDevice phys);
    ~PipelineManager() = default;

    // === Pipeline Creation API ===
    void createDescriptorSetLayout();
    void createPipelineLayout();
    void createRayTracingPipeline(const std::vector<std::string>& shaderPaths);
    void createShaderBindingTable(VkCommandPool pool, VkQueue queue);  // UPDATED: Requires external pool/queue

    // === Accessors used by VulkanRenderer ===
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

private:
    // Device
    VkDevice       device_{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};

    // Cached RT properties — queried once
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR      rtProps_{};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR   asProps_{};
    float timestampPeriod_{0.0f};  // NEW: Matches VulkanRenderer

    // Core pipeline objects
    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkPipeline>            rtPipeline_;

    // SBT objects — NEW: Matches VulkanRenderer
    Handle<VkBuffer>        sbtBuffer_;
    Handle<VkDeviceMemory>  sbtMemory_;
    VkDeviceSize            sbtAddress_{0};
    VkDeviceSize            raygenSbtOffset_{0};
    VkDeviceSize            missSbtOffset_{0};
    VkDeviceSize            hitSbtOffset_{0};
    VkDeviceSize            callableSbtOffset_{0};
    VkDeviceSize            sbtStride_{0};

    // Shader modules kept alive for pipeline lifetime
    std::vector<Handle<VkShaderModule>> shaderModules_;

    // Group counts — NEW: Matches VulkanRenderer
    uint32_t    raygenGroupCount_{0};
    uint32_t    missGroupCount_{0};
    uint32_t    hitGroupCount_{0};
    uint32_t    callableGroupCount_{0};

    // === Private helpers ===
    void cacheDeviceProperties() noexcept;
    [[nodiscard]] VkShaderModule loadShader(const std::string& path) const;  // UPDATED: Matches VulkanRenderer
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, 
                            VkMemoryPropertyFlags properties) const noexcept;  // NEW: Matches VulkanRenderer
    VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) const;  // NEW: Matches VulkanRenderer (adapted)
    void endSingleTimeCommands(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd) const;  // NEW: Matches VulkanRenderer (adapted)

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