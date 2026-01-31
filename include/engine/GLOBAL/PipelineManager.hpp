// =============================================================================
// AMOURANTH RTX Engine — Pipeline Manager Header
// Ray tracing + compute pipeline, SBT, eternal descriptor buffer
// Version 30.10 — January 30, 2026 — descriptor buffer empire
// - VK_EXT_descriptor_buffer only | no sets | memcpy updates
// - Eternal mapped descriptor buffer | frame-free trace
// - Living world compute (binding 7) | materials (binding 3)
// - Push constant: 4 bytes (totalTime float)
// - SBT forged once | dummy TLAS for startup
// Empire stable — pink photons bindless & breathing free
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <atomic>
#include <mutex>

namespace RTX {

struct Extensions;
extern Extensions g_ext;

// Minimal update struct — only what's actually written in .cpp
struct RTDescriptorUpdate {
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceSize uboSize = 0;
    VkImageView rtOutputView = VK_NULL_HANDLE;
    VkBuffer materialsBuffer = VK_NULL_HANDLE;
    VkDeviceSize materialsSize = 0;
};

class PipelineManager {
public:
    PipelineManager();
    ~PipelineManager();

    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;
    PipelineManager(PipelineManager&&) noexcept = default;
    PipelineManager& operator=(PipelineManager&&) noexcept = default;

    void createPipelineLayout();
    void createRayTracingPipeline();
    void createComputePipeline();           // on-demand for living world
    void createShaderBindingTable(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd = VK_NULL_HANDLE);

    void writeRTDescriptorsToBuffer(const RTDescriptorUpdate& updateInfo) noexcept;

    void dispatchLivingWorld(VkCommandBuffer cmd, float totalTime) noexcept;
    void traceRays(VkCommandBuffer cmd, uint32_t width, uint32_t height);

    VkShaderModule loadShader(const std::string& path) const;

    void cacheDeviceProperties();

    [[nodiscard]] static PipelineManager& instance() noexcept {
        static PipelineManager inst;
        return inst;
    }

    // SBT accessors for traceRays
    [[nodiscard]] VkDeviceSize sbtAddress() const noexcept { return sbtAddress_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& raygenRegion() const noexcept { return raygenSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& missRegion() const noexcept { return missSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& hitRegion() const noexcept { return hitSbtRegion_; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& callableRegion() const noexcept { return callableSbtRegion_; }

    [[nodiscard]] VkAccelerationStructureKHR dummyTLAS() const noexcept { return dummyTLAS_.get(); }

    static std::atomic<bool> g_pipelineNeedsRebuild;

    static inline bool s_eternalSbtForged = false;

private:
    static constexpr std::array<VkDescriptorSetLayoutBinding, 9> kMainBindings{{
        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
         VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
         VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR}, // Materials
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR |
         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}, // LivingWorldBuffer
        {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}, // MaterialOverrides
        {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR} // Reserved
    }};

    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkDescriptorSetLayout> emptyDescriptorSetLayout_;
    Handle<VkDescriptorSetLayout> texDescriptorSetLayout_;

    Handle<VkPipelineLayout>      rtPipelineLayout_;
    Handle<VkPipeline>            rtPipeline_;
    Handle<VkPipeline>            computePipeline_;

    Handle<VkBuffer>              sbtBuffer_;
    Handle<VkDeviceMemory>        sbtMemory_;

    VkDeviceSize                  sbtAddress_{0};
    VkDeviceSize                  sbtSize_{0};

    VkStridedDeviceAddressRegionKHR raygenSbtRegion_{};
    VkStridedDeviceAddressRegionKHR missSbtRegion_{};
    VkStridedDeviceAddressRegionKHR hitSbtRegion_{};
    VkStridedDeviceAddressRegionKHR callableSbtRegion_{};

    std::vector<Handle<VkShaderModule>> shaderModules_;

    uint32_t raygenGroupCount_{1};
    uint32_t missGroupCount_{1};
    uint32_t hitGroupCount_{1};

    Handle<VkBuffer>                  dummyAccelBuffer_;
    Handle<VkDeviceMemory>            dummyAccelMemory_;
    Handle<VkAccelerationStructureKHR> dummyTLAS_;

    // Eternal descriptor buffer (mapped, host-coherent)
    uint64_t descriptorBufferHandle_ = 0;
    void* descriptorMapped_ = nullptr;
    VkDeviceAddress descriptorBufferAddress_ = 0;

    std::array<VkDeviceSize, kMainBindings.size()> bindingOffsets_{};
    VkPhysicalDeviceDescriptorBufferPropertiesEXT descProps_{};

    VkAccelerationStructureKHR createDummyTLAS();
    void cacheDescriptorProperties();
};

[[nodiscard]] inline PipelineManager& pipeline() noexcept {
    return PipelineManager::instance();
}

} // namespace RTX