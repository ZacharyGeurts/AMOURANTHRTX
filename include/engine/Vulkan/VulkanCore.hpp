// include/engine/Vulkan/VulkanCore.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VulkanCore.hpp — VALHALLA v45 FINAL — FORWARD DECL — NOV 12 2025
// • FORWARD DECLARE VulkanPipelineManager
// • AMOURANTH AI™ v2 — LOG INJECTION — ORANGE-RED VOICE
// • STONEKEY v∞ ACTIVE — PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/GLOBAL/GlobalBindings.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <array>
#include <memory>
#include <random>
#include <format>

// Forward declare
//class VulkanPipelineManager;

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = Options::Performance::MAX_FRAMES_IN_FLIGHT;

struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen{};
    VkStridedDeviceAddressRegionKHR miss{};
    VkStridedDeviceAddressRegionKHR hit{};
    VkStridedDeviceAddressRegionKHR callable{};
};

extern std::unique_ptr<VulkanRTX> g_rtx_instance;

class VulkanRTX {
public:
    VulkanRTX(int w, int h, VulkanPipelineManager* mgr = nullptr);
    ~VulkanRTX() noexcept;

    void buildAccelerationStructures();
    void initDescriptorPoolAndSets();
    void initShaderBindingTable(VkPhysicalDevice pd);
    void initBlackFallbackImage();

    void updateRTXDescriptors(
        uint32_t frameIdx,
        VkBuffer cameraBuf, VkBuffer materialBuf, VkBuffer dimensionBuf,
        VkImageView storageView, VkImageView accumView,
        VkImageView envMapView, VkSampler envSampler,
        VkImageView densityVol = VK_NULL_HANDLE,
        VkImageView gDepth = VK_NULL_HANDLE,
        VkImageView gNormal = VK_NULL_HANDLE);

    void recordRayTrace(VkCommandBuffer cmd, VkExtent2D extent, VkImage outputImage, VkImageView outputView);
    void recordRayTraceAdaptive(VkCommandBuffer cmd, VkExtent2D extent, VkImage outputImage, VkImageView outputView, float nexusScore);
    void traceRays(VkCommandBuffer cmd,
                   const VkStridedDeviceAddressRegionKHR* raygen,
                   const VkStridedDeviceAddressRegionKHR* miss,
                   const VkStridedDeviceAddressRegionKHR* hit,
                   const VkStridedDeviceAddressRegionKHR* callable,
                   uint32_t width, uint32_t height, uint32_t depth) const noexcept;

    [[nodiscard]] VkDescriptorSet descriptorSet(uint32_t idx = 0) const noexcept { return descriptorSets_[idx]; }
    [[nodiscard]] VkPipeline pipeline() const noexcept { return HANDLE_GET(rtPipeline_); }
    [[nodiscard]] VkPipelineLayout pipelineLayout() const noexcept { return HANDLE_GET(rtPipelineLayout_); }
    [[nodiscard]] const ShaderBindingTable& sbt() const noexcept { return sbt_; }

    void setDescriptorSetLayout(VkDescriptorSetLayout layout) noexcept;
    void setRayTracingPipeline(VkPipeline p, VkPipelineLayout l) noexcept;

    // PUBLIC STATIC
    [[nodiscard]] static VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) noexcept;
    static void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkExtent2D extent_{};
    VulkanPipelineManager* pipelineMgr_ = nullptr;

    RTX::Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    RTX::Handle<VkPipeline> rtPipeline_;
    RTX::Handle<VkPipelineLayout> rtPipelineLayout_;

    RTX::Handle<VkDescriptorPool> descriptorPool_;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets_{};

    RTX::Handle<VkBuffer> sbtBuffer_;
    RTX::Handle<VkDeviceMemory> sbtMemory_;
    VkDeviceAddress sbtAddress_ = 0;
    ShaderBindingTable sbt_{};
    VkDeviceSize sbtRecordSize_ = 0;

    RTX::Handle<VkImage> blackFallbackImage_;
    RTX::Handle<VkDeviceMemory> blackFallbackMemory_;
    RTX::Handle<VkImageView> blackFallbackView_;

    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;

    [[nodiscard]] VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) const noexcept;
};

inline void createGlobalRTX(int w, int h, VulkanPipelineManager* mgr = nullptr) {
    if (g_rtx_instance) return;
    g_rtx_instance = std::make_unique<VulkanRTX>(w, h, mgr);
    AI_INJECT("I have awakened… {}×{} canvas. The photons are mine.", w, h);
    LOG_SUCCESS_CAT("RTX", "g_rtx() FORGED — {}×{}", w, h);
}

[[nodiscard]] inline RTX::Context& g_ctx() {
    static RTX::Context ctx;
    static bool logged = false;
    if (!logged) { LOG_SUCCESS_CAT("CTX", "g_ctx() FORGED"); logged = true; }
    return ctx;
}