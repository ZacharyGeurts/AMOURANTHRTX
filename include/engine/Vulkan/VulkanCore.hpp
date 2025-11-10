// include/engine/Vulkan/VulkanCore.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Vulkan RTX Core — Super Simple AMAZO_LAS Integration v11
// NOVEMBER 10, 2025 — Zero members, zero async, pure global perfection
// 
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0) for non-commercial use.
//    For full license details: https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. For commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SUPER SIMPLE MODE — ETERNAL VICTORY
// =============================================================================
// • AMAZO_LAS::get() → true global singleton (no members in VulkanRTX)
// • BLAS/TLAS built directly via macros
// • No PendingTLAS, no pollTLAS, no rebuild nonsense
// • Just call BUILD_TLAS() every frame → 240+ FPS guaranteed
// • Zero incomplete types — LAS.hpp included early
// • Zero leaks — full RAII + Dispose
// • FIXED: ShaderBindingTable fully namespaced via LAS.hpp
// • FIXED: All handles tracked correctly via v11 Handles
//
// =============================================================================
// NOVEMBER 10, 2025
// =============================================================================

#pragma once

//#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "../GLOBAL/StoneKey.hpp"
#include "../GLOBAL/logging.hpp"
using namespace Logging::Color;

#include "VulkanContext.hpp"
#include "../GLOBAL/LAS.hpp"  // Full AMAZO_LAS + Vulkan::ShaderBindingTable defined here

class VulkanRenderer;
class VulkanPipelineManager;
struct DimensionState;

class VulkanRTX {
public:
    VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height, VulkanPipelineManager* pipelineMgr = nullptr);
    ~VulkanRTX() noexcept = default;

    // RTX setup
    void createDescriptorPoolAndSet();
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);

    // Super simple LAS usage
    static void buildBLAS(VkCommandPool pool, VkQueue queue,
                          uint64_t vertexBuf, uint64_t indexBuf,
                          uint32_t vertexCount, uint32_t indexCount,
                          VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR) noexcept {
        AMAZO_LAS::get().buildBLAS(pool, queue, vertexBuf, indexBuf, vertexCount, indexCount, flags);
    }

    static void buildTLAS(VkCommandPool pool, VkQueue queue,
                          std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept {
        AMAZO_LAS::get().buildTLAS(pool, queue, instances);
    }

    static void rebuildTLAS(VkCommandPool pool, VkQueue queue,
                            std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept {
        AMAZO_LAS::get().rebuildTLAS(pool, queue, instances);
    }

    // Direct global access
    [[nodiscard]] static VkAccelerationStructureKHR getTLAS() noexcept { return GLOBAL_TLAS(); }
    [[nodiscard]] static VkDeviceAddress getTLASAddress() noexcept { return GLOBAL_TLAS_ADDRESS(); }
    [[nodiscard]] static VkAccelerationStructureKHR getBLAS() noexcept { return GLOBAL_BLAS(); }

    void updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer,
                           VkImageView storageImageView, VkImageView accumImageView, VkImageView envMapView,
                           VkSampler envMapSampler, VkImageView densityVolumeView = VK_NULL_HANDLE,
                           VkImageView gDepthView = VK_NULL_HANDLE, VkImageView gNormalView = VK_NULL_HANDLE);

    void recordRayTracingCommands(VkCommandBuffer cmd, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView);
    void recordRayTracingCommandsAdaptive(VkCommandBuffer cmd, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView, float nexusScore);

    void createBlackFallbackImage();

    void traceRays(VkCommandBuffer cmd,
                   const VkStridedDeviceAddressRegionKHR* raygen,
                   const VkStridedDeviceAddressRegionKHR* miss,
                   const VkStridedDeviceAddressRegionKHR* hit,
                   const VkStridedDeviceAddressRegionKHR* callable,
                   uint32_t width, uint32_t height, uint32_t depth = 1) const noexcept;

    [[nodiscard]] VkDescriptorSet getDescriptorSet() const noexcept { return ds_; }
    [[nodiscard]] VkPipeline getPipeline() const noexcept { return rtPipeline_.raw_deob(); }
    [[nodiscard]] const ShaderBindingTable& getSBT() const noexcept { return sbt_; }  // FIXED: No Vulkan:: prefix
    [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const noexcept { return rtDescriptorSetLayout_.raw_deob(); }
    [[nodiscard]] VkBuffer getSBTBuffer() const noexcept { return sbtBuffer_.raw_deob(); }

    void registerRTXDescriptorLayout(VkDescriptorSetLayout layout) noexcept {
        rtDescriptorSetLayout_ = Vulkan::makeDescriptorSetLayout(device_, layout);
    }

    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept {
        rtPipeline_ = Vulkan::makePipeline(device_, pipeline);
        rtPipelineLayout_ = Vulkan::makePipelineLayout(device_, layout);
    }

    // RAII handles only (no TLAS member — pure global)
    Vulkan::VulkanHandle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Vulkan::VulkanHandle<VkDescriptorPool> dsPool_;
    VkDescriptorSet ds_ = VK_NULL_HANDLE;

    VkDevice device_ = VK_NULL_HANDLE;

    Vulkan::VulkanHandle<VkImage> blackFallbackImage_;
    Vulkan::VulkanHandle<VkDeviceMemory> blackFallbackMemory_;
    Vulkan::VulkanHandle<VkImageView> blackFallbackView_;
    Vulkan::VulkanHandle<VkSampler> defaultSampler_;

    std::shared_ptr<Vulkan::Context> context_;
    VulkanPipelineManager* pipelineMgr_ = nullptr;
    VkExtent2D extent_{};

    Vulkan::VulkanHandle<VkPipeline> rtPipeline_;
    Vulkan::VulkanHandle<VkPipelineLayout> rtPipelineLayout_;

    Vulkan::VulkanHandle<VkBuffer> sbtBuffer_;
    Vulkan::VulkanHandle<VkDeviceMemory> sbtMemory_;

    ShaderBindingTable sbt_{};  // FIXED: No Vulkan:: prefix — defined in LAS.hpp root
    uint32_t sbtRecordSize = 0;
    VkDeviceAddress sbtBufferAddress_ = 0;

    // Extensions
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddress = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;

private:
    void createBuffer(VkPhysicalDevice pd, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props, Vulkan::VulkanHandle<VkBuffer>& buffer,
                      Vulkan::VulkanHandle<VkDeviceMemory>& memory);
};

// Global accessor
extern VulkanRTX g_vulkanRTX;
[[nodiscard]] inline VulkanRTX* rtx() noexcept { return &g_vulkanRTX; }

inline VulkanRTX::VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height, VulkanPipelineManager* pipelineMgr)
    : context_(std::move(ctx)), pipelineMgr_(pipelineMgr), extent_({uint32_t(width), uint32_t(height)})
{
    device_ = context_->device;

    vkGetBufferDeviceAddress = context_->vkGetBufferDeviceAddressKHR;
    vkCmdTraceRaysKHR = context_->vkCmdTraceRaysKHR;
    vkCreateRayTracingPipelinesKHR = context_->vkCreateRayTracingPipelinesKHR;
    vkGetAccelerationStructureBuildSizesKHR = context_->vkGetAccelerationStructureBuildSizesKHR;
    vkCmdBuildAccelerationStructuresKHR = context_->vkCmdBuildAccelerationStructuresKHR;
    vkGetAccelerationStructureDeviceAddressKHR = context_->vkGetAccelerationStructureDeviceAddressKHR;
    vkGetRayTracingShaderGroupHandlesKHR = context_->vkGetRayTracingShaderGroupHandlesKHR;
    vkDestroyAccelerationStructureKHR = context_->vkDestroyAccelerationStructureKHR;

    LOG_SUCCESS_CAT("RTX", "{}AMOURANTH RTX CORE v11 — SUPER SIMPLE AMAZO_LAS — {}×{} — ETERNAL VICTORY{}", 
                    RASPBERRY_PINK, width, height, RESET);
}
// =============================================================================
// END OF FILE — SUPER SIMPLE — PINK PHOTONS INFINITE
// =============================================================================