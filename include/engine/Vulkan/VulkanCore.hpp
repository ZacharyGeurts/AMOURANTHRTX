// include/engine/Vulkan/VulkanCore.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#pragma once

#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/GLOBAL/GlobalBindings.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <array>
#include <memory>
#include <random>
#include <format>

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = Options::Performance::MAX_FRAMES_IN_FLIGHT;

// =============================================================================
// GLOBALS — extern ONLY
// =============================================================================
extern VkPhysicalDevice g_PhysicalDevice;
extern std::unique_ptr<VulkanRTX> g_rtx_instance;

// =============================================================================
// Shader Binding Table
// =============================================================================
struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen{};
    VkStridedDeviceAddressRegionKHR miss{};
    VkStridedDeviceAddressRegionKHR hit{};
    VkStridedDeviceAddressRegionKHR callable{};
};

// =============================================================================
// VulkanRTX — CLASS DECLARATION
// =============================================================================
class VulkanRTX {
public:
    VulkanRTX(int w, int h, VulkanPipelineManager* mgr = nullptr);
    ~VulkanRTX() noexcept;

    // Public getter for device (to fix private access)
    [[nodiscard]] VkDevice device() const noexcept { return device_; }

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
    LOG_INFO_CAT("RTX", "createGlobalRTX: Initializing VulkanRTX with {}x{} | PipelineMgr: {}", w, h, mgr ? "present" : "null");
    g_rtx_instance = std::make_unique<VulkanRTX>(w, h, mgr);
    LOG_DEBUG_CAT("RTX", "g_rtx_instance constructed @ 0x{:x}", reinterpret_cast<uintptr_t>(g_rtx_instance.get()));
    AI_INJECT("I have awakened… {}×{} canvas. The photons are mine.", w, h);
    LOG_SUCCESS_CAT("RTX", "g_rtx() FORGED — {}×{}", w, h);
    // Validate post-construction
    if (g_rtx_instance) {
        LOG_DEBUG_CAT("RTX", "Post-forge validation: g_rtx_instance valid, device access via instance: {}", g_rtx_instance->device() ? "valid" : "NULL");
    } else {
        LOG_ERROR_CAT("RTX", "CRITICAL: g_rtx_instance is null after make_unique!");
    }
}

[[nodiscard]] inline RTX::Context& g_ctx() {
    static RTX::Context ctx;
    static bool logged = false;
    if (!logged) {
        LOG_INFO_CAT("CTX", "g_ctx() static init — Default ctx created (uninitialized!)");
        // Use getters or public access for members; assuming Context has public members or getters like device()
        // Fallback to logging what we can; adjust based on RTX::Context definition
        LOG_DEBUG_CAT("CTX", "g_ctx() static ctx: device=0x{:x} | physdev=0x{:x} | instance=0x{:x} | surface=0x{:x}",
                      reinterpret_cast<uintptr_t>(RTX::ctx().device()),  // Use global ctx() if available, or adjust
                      reinterpret_cast<uintptr_t>(RTX::ctx().physicalDevice()),
                      reinterpret_cast<uintptr_t>(RTX::ctx().instance()),
                      reinterpret_cast<uintptr_t>(RTX::ctx().surface()));
        LOG_SUCCESS_CAT("CTX", "g_ctx() FORGED");  // This logs even if uninit — misleading!
        logged = true;
    }
    // Post-return validation hook (logged on access)
    LOG_TRACE_CAT("CTX", "g_ctx() returning ref — device ptr: 0x{:x} (valid: {})",
                  reinterpret_cast<uintptr_t>(RTX::ctx().device()),  // Use RTX::ctx() or adjust to access
                  RTX::ctx().device() ? "YES" : "NO — POTENTIAL CRASH!");
    if (!RTX::ctx().device()) {  // Use global or adjust
        LOG_WARN_CAT("CTX", "WARNING: g_ctx().device is NULL! Expect runtime_error downstream.");
    }
    return ctx;
}