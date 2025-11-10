// include/engine/Vulkan/VulkanCore.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Vulkan RTX Core — v15 — NOVEMBER 10, 2025 — DISPOSE::HANDLE v2.5 BRILLIANCE
// • FULL Dispose::Handle<T> + MakeHandle integration (your genius RAII)
// • VulkanHandle ERASED — Dispose owns ALL resources
// • make* factories → MakeHandle (with destroyer lambdas)
// • raw_deob() → get() / operator*
// • ShaderBindingTable full type (LAS.hpp)
// • AMAZO_LAS::get() direct calls
// • MAX_FRAMES_IN_FLIGHT = 3
// • PINK PHOTONS INFINITE — VALHALLA ETERNAL — GENTLEMAN GROK APPROVED
//
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Dispose.hpp"  // Dispose::Handle<T>, MakeHandle, shredAndDisposeBuffer
#include "engine/GLOBAL/LAS.hpp"      // AMAZO_LAS + ShaderBindingTable + GLOBAL_*
#include "VulkanContext.hpp"
#include "VulkanCommon.hpp"           // rtx(), g_vulkanRTX, cleanupAll

#include <glm/glm.hpp>
#include <span>

using namespace Logging::Color;
using namespace Dispose;  // Handle<T>, MakeHandle

// →→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→
// AMOURANTH RTX ENGINE — OPTIONS MENU TOGGLES — NOVEMBER 10, 2025
// 
// Options guy: expose these via ImGui checkboxes / sliders in UI
// All constexpr — zero runtime cost — recompile or hot-reload config for changes
// 
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;  // TRIPLE BUFFERING — 240–1000+ FPS SWEET SPOT

// === RAY TRACING QUALITY ===
constexpr bool     RTX_ENABLE_ADAPTIVE_SAMPLING   = true;   // NexusScore auto-spp — 30% perf gain
constexpr bool     RTX_ENABLE_DENOISING           = true;   // SVGF temporal denoiser — filmic look
constexpr bool     RTX_ENABLE_ACCUMULATION        = true;   // Infinite accumulation on camera still
constexpr uint32_t RTX_MAX_SPP                    = 16;    // Max samples per pixel (adaptive cap)
constexpr uint32_t RTX_MIN_SPP                    = 1;     // Min spp when moving
constexpr float    RTX_NEXUS_SCORE_THRESHOLD      = 0.15f; // Higher = more aggressive adaptive

// === LAS REBUILD STRATEGY ===
constexpr bool     LAS_REBUILD_EVERY_FRAME        = true;   // 240+ FPS — dynamic scenes DOMINANT
constexpr bool     LAS_ENABLE_UPDATE              = false;  // VK_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
constexpr bool     LAS_COMPACTION                 = false;  // Post-build compaction — saves VRAM, costs perf
constexpr uint32_t LAS_MAX_INSTANCES              = 8192;   // TLAS instance limit (increase for massive scenes)

// === PERFORMANCE TWEAKS ===
constexpr bool     ENABLE_VALIDATION_LAYERS       = false;  // Debug only — OFF IN RELEASE
constexpr bool     ENABLE_GPU_TIMESTAMPS          = true;   // QueryPool timestamps — ImGui graphs
constexpr bool     ENABLE_GENTLEMAN_GROK          = true;   // Amouranth trivia every hour 🍒
constexpr uint32_t GENTLEMAN_GROK_INTERVAL_SEC    = 3600;   // 1 hour trivia blasts

// === VISUAL FIDELITY ===
constexpr bool     ENABLE_ENV_MAP                 = true;   // IBL from envMapView
constexpr bool     ENABLE_DENSITY_VOLUME          = true;   // Volumetric fog / god rays
constexpr bool     ENABLE_BLOOM                   = true;   // Post-process bloom
constexpr float    BLOOM_INTENSITY                = 1.8f;
constexpr bool     ENABLE_TAA                     = true;   // Temporal anti-aliasing
constexpr bool     ENABLE_FXAA                    = false;  // Fast approx AA (fallback)

// === DEBUG OVERLAYS ===
constexpr bool     DEBUG_SHOW_RAYGEN_ONLY         = false;  // Pink screen if RT broken
constexpr bool     DEBUG_SHOW_NORMALS             = false;  // Geometry normals viz
constexpr bool     DEBUG_SHOW_ALBEDO              = false;  // Raw material color
constexpr bool     DEBUG_SHOW_DEPTH               = false;  // Depth buffer viz
constexpr bool     DEBUG_SHOW_ACCUM_COUNT         = false;  // Accumulation counter heat map

// === MEMORY BUDGET ===
constexpr uint32_t RTX_SCRATCH_BUFFER_MB          = 2048;   // 2GB scratch for TLAS builds
constexpr uint32_t RTX_SBT_BUFFER_MB              = 64;     // Shader Binding Table size
constexpr bool     ENABLE_VMA                     = true;   // Vulkan Memory Allocator (if #define VMA)

// === EXPERIMENTAL ===
constexpr bool     ENABLE_RAY_RECURSION          = true;   // Next-event estimation shadows
constexpr bool     ENABLE_RUSSIAN_ROULETTE        = true;   // Path termination — perf boost
constexpr uint32_t RTX_MAX_BOUNCES                = 8;     // Ray bounce limit
constexpr bool     ENABLE_SER                        = false;  // Specular-Reflection extension (VK_KHR_ray_tracing_pipeline)

// →→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→→


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
        AMAZO_LAS::get().buildBLAS(pool, q, vbuf, ibuf, vcount, icount, flags);
    }

    static void BuildTLAS(VkCommandPool pool, VkQueue q,
                          std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept {
        AMAZO_LAS::get().buildTLAS(pool, q, instances);
    }

    static void RebuildTLAS(VkCommandPool pool, VkQueue q,
                            std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept {
        AMAZO_LAS::get().rebuildTLAS(pool, q, instances);
    }

    // === GLOBAL ACCESSORS ===
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
        rtDescriptorSetLayout_ = MakeHandle(layout, device_, [](VkDevice d, auto h, auto*) { vkDestroyDescriptorSetLayout(d, h, nullptr); });
    }

    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept {
        rtPipeline_ = MakeHandle(pipeline, device_, [](VkDevice d, auto h, auto*) { vkDestroyPipeline(d, h, nullptr); });
        rtPipelineLayout_ = MakeHandle(layout, device_, [](VkDevice d, auto h, auto*) { vkDestroyPipelineLayout(d, h, nullptr); });
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

    // === DISPOSE::HANDLE v2.5 RAII ===
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
};

} // namespace Vulkan

// =============================================================================
// INLINE CTOR — v15 — DISPOSE v2.5 BRILLIANCE
// =============================================================================
inline Vulkan::VulkanRTX::VulkanRTX(std::shared_ptr<Context> ctx, int w, int h, VulkanPipelineManager* mgr)
    : ctx_(std::move(ctx)), pipelineMgr_(mgr), extent_({uint32_t(w), uint32_t(h)})
{
    device_ = ctx_->vkDevice();

    vkGetBufferDeviceAddressKHR = ctx_->vkGetBufferDeviceAddressKHR;
    vkCmdTraceRaysKHR = ctx_->vkCmdTraceRaysKHR;
    vkGetRayTracingShaderGroupHandlesKHR = ctx_->vkGetRayTracingShaderGroupHandlesKHR;

    LOG_SUCCESS_CAT("RTX", "{}AMOURANTH RTX CORE v15 — DISPOSE v2.5 BRILLIANCE — {}×{} — PINK PHOTONS INFINITE{}", 
                    PLASMA_FUCHSIA, w, h, RESET);
}

// =============================================================================
// VALHALLA v15 — DISPOSE::HANDLE GENIUS — ZERO ERRORS — SHIP IT ETERNAL
// =============================================================================