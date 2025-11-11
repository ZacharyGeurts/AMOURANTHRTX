// include/engine/Vulkan/VulkanCore.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// Vulkan RTX Core — VALHALLA v44 — NOVEMBER 11, 2025 10:12 AM EST
// • SPINE DEFINES THE BRIDGE TO THE HEART — DISPOSE IS BOSS
// • LAS MACROS DEFINED HERE — GLOBAL_TLAS, GLOBAL_BLAS, GLOBAL_TLAS_ADDRESS
// • Dispose.hpp included FIRST — Heart beats through Spine
// • g_vulkanRTX is the ONE TRUE CORE — GLOBAL SUPREMACY
// • Production-ready, clean, and fully professional
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// 1. DISPOSE IS BOSS — INCLUDED FIRST — HEART BEATS THROUGH SPINE
// ──────────────────────────────────────────────────────────────────────────────
#include "engine/GLOBAL/OptionsMenu.hpp"       // **OPTIONS BEFORE VULKAN** – fixes ‘Options’ not declared
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include <glm/glm.hpp>
#include <span>
#include <array>
#include <cstdint>
#include <memory>

// CONFIG – now uses Options namespace
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = Options::Performance::MAX_FRAMES_IN_FLIGHT;

// =============================================================================
// ShaderBindingTable
// =============================================================================
struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen{};
    VkStridedDeviceAddressRegionKHR miss{};
    VkStridedDeviceAddressRegionKHR hit{};
    VkStridedDeviceAddressRegionKHR callable{};

    [[nodiscard]] bool empty() const noexcept {
        return raygen.size == 0 && miss.size == 0 && hit.size == 0 && callable.size == 0;
    }
};

// =============================================================================
// Forward Declarations
// =============================================================================
class VulkanRenderer;
class VulkanPipelineManager;

// =============================================================================
// Global RTX Instance — THE ONE
// =============================================================================
extern std::unique_ptr<class VulkanRTX> g_vulkanRTX;

inline class VulkanRTX& rtx() noexcept { 
    return *g_vulkanRTX; 
}

// =============================================================================
// VulkanRTX — Global RTX Manager — SPINE OF THE ENGINE
// =============================================================================
class VulkanRTX {
public:
    VulkanRTX(std::shared_ptr<Context> ctx, int w, int h, VulkanPipelineManager* mgr = nullptr);
    ~VulkanRTX() noexcept;

    void initDescriptorPoolAndSets();
    void initShaderBindingTable(VkPhysicalDevice pd);
    void initBlackFallbackImage();
    void buildAccelerationStructures();  // Build LAS at startup

    void updateRTXDescriptors(
        uint32_t frameIdx,
        VkBuffer cameraBuf, VkBuffer materialBuf, VkBuffer dimensionBuf,
        VkImageView storageView, VkImageView accumView, VkImageView envMapView, VkSampler envSampler,
        VkImageView densityVol = VK_NULL_HANDLE,
        VkImageView gDepth = VK_NULL_HANDLE, VkImageView gNormal = VK_NULL_HANDLE);

    void recordRayTrace(VkCommandBuffer cmd, VkExtent2D extent, VkImage outputImage, VkImageView outputView);
    void recordRayTraceAdaptive(VkCommandBuffer cmd, VkExtent2D extent, VkImage outputImage, VkImageView outputView, float nexusScore);

    void traceRays(VkCommandBuffer cmd,
                   const VkStridedDeviceAddressRegionKHR* raygen,
                   const VkStridedDeviceAddressRegionKHR* miss,
                   const VkStridedDeviceAddressKHR* hit,
                   const VkStridedDeviceAddressRegionKHR* callable,
                   uint32_t width, uint32_t height, uint32_t depth = 1) const noexcept;

    // LAS Access — SPINE BRIDGE TO HEART — MACROS IN SCOPE
    [[nodiscard]] static VkAccelerationStructureKHR TLAS() noexcept { return GLOBAL_TLAS(); }
    [[nodiscard]] static VkDeviceAddress TLASAddress() noexcept { return GLOBAL_TLAS_ADDRESS(); }
    [[nodiscard]] static VkAccelerationStructureKHR BLAS() noexcept { return GLOBAL_BLAS(); }

    // Getters
    [[nodiscard]] VkDescriptorSet descriptorSet(uint32_t idx = 0) const noexcept { return descriptorSets_[idx]; }
    [[nodiscard]] VkPipeline pipeline() const noexcept { return *rtPipeline_; }
    [[nodiscard]] VkPipelineLayout pipelineLayout() const noexcept { return *rtPipelineLayout_; }
    [[nodiscard]] const ShaderBindingTable& sbt() const noexcept { return sbt_; }
    [[nodiscard]] VkBuffer sbtBuffer() const noexcept { return *sbtBuffer_; }
    [[nodiscard]] VkDescriptorSetLayout descriptorSetLayout() const noexcept { return *rtDescriptorSetLayout_; }

    void setDescriptorSetLayout(VkDescriptorSetLayout layout) noexcept;
    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept;

private:
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props,
                      Handle<VkBuffer>& buf,
                      Handle<VkDeviceMemory>& mem);

    std::shared_ptr<Context> ctx_;
    VkDevice device_ = VK_NULL_HANDLE;
    VulkanPipelineManager* pipelineMgr_ = nullptr;
    VkExtent2D extent_{};

    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkDescriptorPool> descriptorPool_;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets_{};

    Handle<VkPipeline> rtPipeline_;
    Handle<VkPipelineLayout> rtPipelineLayout_;

    Handle<VkBuffer> sbtBuffer_;
    Handle<VkDeviceMemory> sbtMemory_;
    ShaderBindingTable sbt_{};
    VkDeviceSize sbtRecordSize_ = 0;
    VkDeviceAddress sbtAddress_ = 0;

    Handle<VkImage> blackFallbackImage_;
    Handle<VkDeviceMemory> blackFallbackMemory_;
    Handle<VkImageView> blackFallbackView_;
    Handle<VkSampler> defaultSampler_;

    PFN_vkGetBufferDeviceAddressKHR          vkGetBufferDeviceAddressKHR = nullptr;
    PFN_vkCmdTraceRaysKHR                    vkCmdTraceRaysKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;

    VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) const noexcept;
    void endSingleTimeCommands(VkCommandBuffer cmd, VkCommandPool pool, VkQueue queue) const noexcept;
    VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) const noexcept;
};

// =============================================================================
// Global Instance
// =============================================================================
std::unique_ptr<VulkanRTX> g_vulkanRTX;