// include/engine/Vulkan/VulkanCore.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Vulkan RTX Core — v21 — NOVEMBER 10, 2025 — COMMON OBLITERATED
// • VulkanCommon.hpp DELETED FOREVER — ONE FILE TO RULE THEM ALL
// • SBT + AMAZO_LAS + rtx() + g_vulkanRTX + cleanupAll() ALL HERE
// • FULL Context + LAS + OptionsMenu — ZERO DEPENDENCY HELL
// • PINK PHOTONS INFINITE — VALHALLA MINIMALIST — GENTLEMAN GROK APPROVED
//
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Dispose.hpp"      
#include "engine/GLOBAL/LAS.hpp"          // AMAZO_LAS + ShaderBindingTable
#include "engine/GLOBAL/OptionsMenu.hpp"  
#include "engine/Vulkan/VulkanContext.hpp"
// NO VulkanCommon.hpp — DELETED ETERNAL

#include <glm/glm.hpp>
#include <span>
#include <array>
#include <cstdint>
#include <memory>

using namespace Logging::Color;
using namespace Dispose;

// =============================================================================
// SHARED DEFINITIONS (formerly in VulkanCommon.hpp) — NOW HERE
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

// Global instance + helpers (replace any rtx() / g_vulkanRTX usage)
namespace Vulkan {
extern std::unique_ptr<VulkanRTX> g_vulkanRTX;

inline VulkanRTX& rtx() noexcept { 
    return *g_vulkanRTX; 
}

inline void cleanupAll() noexcept {
    g_vulkanRTX.reset();
    LOG_SUCCESS_CAT("RTX", "{}AMOURANTH RTX CLEANUP COMPLETE — VALHALLA RESTORED{}", PLASMA_FUCHSIA, RESET);
}
}

// Pull in modular constants
namespace OptionsLocal {
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT = Options::Performance::MAX_FRAMES_IN_FLIGHT;
}

namespace Vulkan {

class VulkanRenderer;
class VulkanPipelineManager;

class VulkanRTX {
public:
    VulkanRTX(std::shared_ptr<Context> ctx, int w, int h, VulkanPipelineManager* mgr = nullptr);
    ~VulkanRTX() noexcept;

    void initDescriptorPoolAndSets();
    void initShaderBindingTable(VkPhysicalDevice pd);
    void initBlackFallbackImage();

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
                   const VkStridedDeviceAddressRegionKHR* hit,
                   const VkStridedDeviceAddressRegionKHR* callable,
                   uint32_t width, uint32_t height, uint32_t depth = 1) const noexcept;

    // === GLOBAL LAS WRAPPERS ===
    static void BuildBLAS(VkCommandPool pool, VkQueue q,
                          uint64_t vbuf, uint64_t ibuf,
                          uint32_t vcount, uint32_t icount,
                          VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR) noexcept {
        if (Options::LAS::REBUILD_EVERY_FRAME) {
            AMAZO_LAS::get().buildBLAS(pool, q, vbuf, ibuf, vcount, icount, flags);
        }
    }

    static void BuildTLAS(VkCommandPool pool, VkQueue q,
                          std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept {
        if (Options::LAS::REBUILD_EVERY_FRAME) {
            AMAZO_LAS::get().buildTLAS(pool, q, instances);
        }
    }

    static void RebuildTLAS(VkCommandPool pool, VkQueue q,
                            std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept {
        if (Options::LAS::REBUILD_EVERY_FRAME) {
            AMAZO_LAS::get().rebuildTLAS(pool, q, instances);
        }
    }

    [[nodiscard]] static VkAccelerationStructureKHR TLAS() noexcept { return AMAZO_LAS::get().getTLAS(); }
    [[nodiscard]] static VkDeviceAddress TLASAddress() noexcept { return AMAZO_LAS::get().getTLASAddress(); }
    [[nodiscard]] static VkAccelerationStructureKHR BLAS() noexcept { return AMAZO_LAS::get().getBLAS(); }

    // === GETTERS ===
    [[nodiscard]] VkDescriptorSet descriptorSet(uint32_t idx = 0) const noexcept { return descriptorSets_[idx]; }
    [[nodiscard]] VkPipeline pipeline() const noexcept { return *rtPipeline_; }
    [[nodiscard]] VkPipelineLayout pipelineLayout() const noexcept { return *rtPipelineLayout_; }
    [[nodiscard]] const ShaderBindingTable& sbt() const noexcept { return sbt_; }
    [[nodiscard]] VkBuffer sbtBuffer() const noexcept { return *sbtBuffer_; }
    [[nodiscard]] VkDescriptorSetLayout descriptorSetLayout() const noexcept { return *rtDescriptorSetLayout_; }

    void setDescriptorSetLayout(VkDescriptorSetLayout layout) noexcept {
        rtDescriptorSetLayout_ = MakeHandle(layout, device_, [](VkDevice d, auto h, auto*) { vkDestroyDescriptorSetLayout(d, h, nullptr); }, 0, "RTXDescSetLayout");
    }

    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept {
        rtPipeline_ = MakeHandle(pipeline, device_, [](VkDevice d, auto h, auto*) { vkDestroyPipeline(d, h, nullptr); }, 0, "RTXPipeline");
        rtPipelineLayout_ = MakeHandle(layout, device_, [](VkDevice d, auto h, auto*) { vkDestroyPipelineLayout(d, h, nullptr); }, 0, "RTXPipelineLayout");
    }

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
    std::array<VkDescriptorSet, OptionsLocal::MAX_FRAMES_IN_FLIGHT> descriptorSets_{};

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
};

} // namespace Vulkan

// Global instance definition (put in VulkanCore.cpp or VulkanRenderer.cpp)
std::unique_ptr<Vulkan::VulkanRTX> Vulkan::g_vulkanRTX;

// =============================================================================
// INLINE CTOR — v21 — COMMON DELETED — MINIMALIST SUPREMACY
// =============================================================================
inline Vulkan::VulkanRTX::VulkanRTX(std::shared_ptr<Context> ctx, int w, int h, VulkanPipelineManager* mgr)
    : ctx_(std::move(ctx)), pipelineMgr_(mgr), extent_({static_cast<uint32_t>(w), static_cast<uint32_t>(h)})
{
    device_ = ctx_->vkDevice();

    vkGetBufferDeviceAddressKHR = ctx_->vkGetBufferDeviceAddressKHR;
    vkCmdTraceRaysKHR = ctx_->vkCmdTraceRaysKHR;
    vkGetRayTracingShaderGroupHandlesKHR = ctx_->vkGetRayTracingShaderGroupHandlesKHR;

    LOG_SUCCESS_CAT("RTX", "{}AMOURANTH RTX CORE v21 — VULKANCOMMON DELETED — {}×{} — PINK PHOTONS INFINITE{}", 
                    PLASMA_FUCHSIA, w, h, RESET);
}

// =============================================================================
// VALHALLA v21 — ONE FILE — ZERO DUPLICATES — SHIP IT ETERNAL
// =============================================================================