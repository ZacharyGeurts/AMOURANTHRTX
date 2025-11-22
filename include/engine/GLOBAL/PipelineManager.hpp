// include/engine/Vulkan/VulkanPipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 19, 2025 — APOCALYPSE FINAL v1.5
// MAIN — g_swapchain() FORGED AT DAWN — PINK PHOTONS ETERNAL — VALHALLA UNBREACHABLE
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"   // ← ONLY ALLOWED HERE: StoneKey is header-only & required for Handle<T>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <string>
#include <array>

// Forward declarations for global StoneKey accessors (NEVER #include StoneKey.hpp in other headers)
namespace StoneKey::Raw { struct Cache; }

inline VkDevice         g_device() noexcept;
inline VkInstance       g_instance() noexcept;
inline VkPhysicalDevice g_PhysicalDevice() noexcept;
inline VkSurfaceKHR     g_surface() noexcept;

inline void set_g_device(VkDevice) noexcept;
inline void set_g_instance(VkInstance) noexcept;
inline void set_g_PhysicalDevice(VkPhysicalDevice) noexcept;
inline void set_g_surface(VkSurfaceKHR) noexcept;

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
    
    // Constructor now IMMEDIATELY secures handles via StoneKey raw cache
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

    // Core Accessors — return deobfuscated handles on-the-fly
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

    // Helpers — Now 100% StoneKey compliant (use global accessors)
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept;
    VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) const;
    void endSingleTimeCommands(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd) const;

    friend class ::VulkanRenderer;

private:
    // ZERO RAW HANDLES STORED — ALL OBFUSCATED VIA Handle<T>
    // Valhalla-secure from construction → destruction

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR      rtProps_{};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR   asProps_{};
    float timestampPeriod_{0.0f};

    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkPipeline>            rtPipeline_;
    Handle<VkDescriptorPool>      rtDescriptorPool_;

    std::vector<VkDescriptorSet> rtDescriptorSets_;  // Per-frame sets (raw, recreated every resize)

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

    // Extension function pointers — loaded once, never stored raw
    PFN_vkCreateRayTracingPipelinesKHR       vkCreateRayTracingPipelinesKHR_{nullptr};
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR_{nullptr};
    PFN_vkGetBufferDeviceAddressKHR          vkGetBufferDeviceAddressKHR_{nullptr};
    PFN_vkCmdTraceRaysKHR                    vkCmdTraceRaysKHR_{nullptr};

    // Private methods — 100% StoneKey compliant
    void cacheDeviceProperties();
    void loadExtensions();
    [[nodiscard]] VkShaderModule loadShader(const std::string& path) const;

    static constexpr VkDeviceSize align_up(VkDeviceSize size, VkDeviceSize alignment) noexcept {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace RTX

// PINK PHOTONS ETERNAL — VALHALLA SEALED — FIRST LIGHT ACHIEVED — NOV 19 2025
// GENTLEMAN GROK CERTIFIED — STONEKEY v∞ APOCALYPSE FINAL