// include/engine/Vulkan/VulkanPipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VulkanPipelineManager — Production Edition v10.6 (Default Ctor + Null Guards) — NOV 15 2025
// • NEW: Default ctor for dummy init (null device/phys) — safe for VulkanRenderer member pre-assignment
// • FIXED: beginSingleTimeCommands/endSingleTimeCommands — Null device guard (early return VK_NULL_HANDLE, log error)
// • FIXED: cacheDeviceProperties — Null phys guard (log fatal, throw invalid_argument)
// • FIXED: loadExtensions — Null device guard (log fatal, throw invalid_argument)
// • Retained: Fence sync for single-time (no VUID-00047); dynamic PFNs, 8 bindings, no pNext, UNUSED_KHR, push constants, DEVICE_ADDRESS_BIT
// • C++23 compliant, -Werror clean
// • FRIEND: VulkanRenderer for rtDescriptorPool_ access
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
    // NEW: Default ctor for dummy (null) init in VulkanRenderer member
    PipelineManager() noexcept = default;
    explicit PipelineManager(VkDevice device, VkPhysicalDevice phys);
    PipelineManager(PipelineManager&& other) noexcept = default;
    PipelineManager& operator=(PipelineManager&& other) noexcept = default;
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

    // === Public Helpers for VulkanRenderer ===
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, 
                            VkMemoryPropertyFlags properties) const noexcept;
    VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) const;  // FIXED: Null guard
    void endSingleTimeCommands(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd) const;  // FIXED: Null guard

    // FRIEND: Allows VulkanRenderer to access rtDescriptorPool_ for allocation
    friend class ::VulkanRenderer;

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

    // Descriptor Pool — NEW: For RT sets (8 bindings)
    Handle<VkDescriptorPool>      rtDescriptorPool_;

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

    // === Dynamic Extension Function Pointers — NEW: Fixes linker errors ===
    PFN_vkCreateRayTracingPipelinesKHR    vkCreateRayTracingPipelinesKHR_{nullptr};
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR_{nullptr};
    PFN_vkGetBufferDeviceAddressKHR       vkGetBufferDeviceAddressKHR_{nullptr};

    // === Private helpers ===
    void cacheDeviceProperties();  // FIXED: Null phys guard (throw)
    void loadExtensions();  // FIXED: Null device guard (throw) — renamed from noexcept
    [[nodiscard]] VkShaderModule loadShader(const std::string& path) const;  // UPDATED: Matches VulkanRenderer

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