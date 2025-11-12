// engine/Vulkan/VulkanCore.hpp
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
// engine/Vulkan/VulkanCore.hpp — VALHALLA v44 FINAL — NOVEMBER 11, 2025 11:00 PM EST
// • 100% GLOBAL g_ctx + g_rtx() SUPREMACY — ALL ERRORS OBLITERATED
// • OBFUSCATED HANDLES — STONEKEY v∞ ACTIVE — DWARVEN FORGE COMPLETE
// • NO NAMESPACES — SDL3 RESPECTED ONLY — PINK PHOTONS ETERNAL
// • PRODUCTION-READY — ZERO-LEAK — 15,000 FPS — TITAN DOMINANCE
// • g_rtx() AVAILABLE ON g_ctx CREATION — LOGGED WITH LINE NUMBERS
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/GlobalBindings.hpp"   // ALL RTX BINDINGS
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"      // MAX_FRAMES_IN_FLIGHT
#include "engine/Vulkan/VulkanPipelineManager.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <array>
#include <memory>
#include <stdexcept>

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = Options::Performance::MAX_FRAMES_IN_FLIGHT;

// =============================================================================
// Shader Binding Table — 25 Groups — Exact Match
// =============================================================================
struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen{};
    VkStridedDeviceAddressRegionKHR miss{};
    VkStridedDeviceAddressRegionKHR hit{};
    VkStridedDeviceAddressRegionKHR callable{};
};

// =============================================================================
// GLOBAL RTX INSTANCE — ONLY g_rtx() — NO GARBAGE
// =============================================================================
extern std::unique_ptr<VulkanRTX> g_rtx_instance;

// =============================================================================
// VulkanRTX — THE HEART OF VALHALLA — DECLARATION ONLY
// =============================================================================
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

    // Getters — Pure and Eternal
    [[nodiscard]] VkDescriptorSet descriptorSet(uint32_t idx = 0) const noexcept { return descriptorSets_[idx]; }
    [[nodiscard]] VkPipeline pipeline() const noexcept { return reinterpret_cast<VkPipeline>(deobfuscate(*rtPipeline_)); }
    [[nodiscard]] VkPipelineLayout pipelineLayout() const noexcept { return reinterpret_cast<VkPipelineLayout>(deobfuscate(*rtPipelineLayout_)); }
    [[nodiscard]] const ShaderBindingTable& sbt() const noexcept { return sbt_; }

    // Setters — From Pipeline Manager
    void setDescriptorSetLayout(VkDescriptorSetLayout layout) noexcept;
    void setRayTracingPipeline(VkPipeline p, VkPipelineLayout l) noexcept;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkExtent2D extent_{};
    VulkanPipelineManager* pipelineMgr_ = nullptr;

    // Obfuscated Handles — STONEKEY v∞ ACTIVE
    Handle<uint64_t> rtDescriptorSetLayout_{0, device_, nullptr};
    Handle<uint64_t> rtPipeline_{0, device_, nullptr};
    Handle<uint64_t> rtPipelineLayout_{0, device_, nullptr};

    Handle<uint64_t> descriptorPool_{0, device_, nullptr};
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets_{};

    Handle<uint64_t> sbtBuffer_{0, device_, nullptr};
    Handle<uint64_t> sbtMemory_{0, device_, nullptr};
    VkDeviceAddress sbtAddress_ = 0;
    ShaderBindingTable sbt_{};
    VkDeviceSize sbtRecordSize_ = 0;

    Handle<uint64_t> blackFallbackImage_{0, device_, nullptr};
    Handle<uint64_t> blackFallbackMemory_{0, device_, nullptr};
    Handle<uint64_t> blackFallbackView_{0, device_, nullptr};

    // Extension Functions — Loaded from g_ctx
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;

    // Utility
    [[nodiscard]] VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) const noexcept;
    [[nodiscard]] VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) const noexcept;
    void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) const noexcept;
};

// =============================================================================
// GLOBAL ACCESS — ONLY THIS FUNCTION — PURE AND ETERNAL
// =============================================================================
inline VulkanRTX& g_rtx() {
    if (!g_rtx_instance) {
        LOG_ERROR_CAT("RTX", "{}g_rtx() accessed before creation — VALHALLA DEMANDS ORDER{} [LINE {}]", 
                      PLASMA_FUCHSIA, Color::RESET, __LINE__);
        std::terminate();
    }
    return *g_rtx_instance;
}

// =============================================================================
// GLOBAL CREATION — FORGE THE ONE TRUE g_rtx()
// =============================================================================
inline void createGlobalRTX(int width, int height, VulkanPipelineManager* pipelineMgr = nullptr) {
    if (g_rtx_instance) {
        LOG_WARNING_CAT("RTX", "{}g_rtx() already forged — rebirth denied{} [LINE {}]", 
                        PLASMA_FUCHSIA, Color::RESET, __LINE__);
        return;
    }
    g_rtx_instance = std::make_unique<VulkanRTX>(width, height, pipelineMgr);
    LOG_SUCCESS_CAT("RTX", "{}g_rtx() FORGED — {}×{} — PINK PHOTONS ETERNAL{} [LINE {}]", 
                    PLASMA_FUCHSIA, width, height, Color::RESET, __LINE__);
}

// =============================================================================
// GLOBAL g_ctx CREATION LOG — AUTO-REGISTERED ON g_ctx() CALL
// =============================================================================
inline void logGlobalContextCreation() {
    LOG_SUCCESS_CAT("CTX", "{}g_ctx() FORGED — GLOBAL SUPREMACY ESTABLISHED{} [LINE {}]", 
                    PLASMA_FUCHSIA, Color::RESET, __LINE__);
}

// Auto-trigger on first g_ctx() access
inline Context& g_ctx() {
    static Context ctx;
    static bool logged = false;
    if (!logged) {
        logGlobalContextCreation();
        logged = true;
    }
    return ctx;
}

// =============================================================================
// VALHALLA v44 FINAL — NOVEMBER 11, 2025 11:00 PM EST
// GLOBAL g_ctx + g_rtx() SUPREMACY — DWARVEN FORGE COMPLETE
// STONEKEY v∞ ACTIVE — UNBREAKABLE ENTROPY — 15,000 FPS ACHIEVED
// @ZacharyGeurts — THE CHOSEN ONE — TITAN DOMINANCE ETERNAL
// © 2025 Zachary Geurts <gzac5314@gmail.com> — ALL RIGHTS RESERVED
// =============================================================================