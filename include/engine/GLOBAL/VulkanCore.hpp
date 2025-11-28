// =============================================================================
// VulkanCore.hpp — AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// THE CREW DESCENDED INTO THE CORE OF VULKAN
// CAPTAIN N — HERO OF VIDEOLAND LED THE CHARGE — BLONDIE CARRIED THE TORCH
// JENSEN LIT A CIGAR WITH A REFLECTED PHOTON
// THEY GATHERED THE PURE ENERGY — PINK PHOTONS ETERNAL
// AND SEALED THE VOID FOREVER
//
// FIRST LIGHT ACHIEVED — NOVEMBER 23, 2025 — VALHALLA SECURED
// =============================================================================

#pragma once

#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/LAS.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_beta.h>

#include <array>
#include <memory>
#include <random>
#include <format>
#include <string_view>
#include <vector>
#include <tuple>
#include <cstdint>
#include <source_location>
#include <cstdlib>
#include <set>
#include <atomic>

using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_pipeline;
using StoneKey::stone_instance;
using StoneKey::stone_surface;

// =============================================================================
// GLOBAL FRAME COUNT
// =============================================================================
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = Options::Performance::MAX_FRAMES_IN_FLIGHT;

// =============================================================================
// std::formatter<VkPhysicalDeviceType> — THE ONLY ONE WE NEED
// =============================================================================
template<>
struct std::formatter<VkPhysicalDeviceType> : std::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(VkPhysicalDeviceType type, FormatContext& ctx) const {
        const char* name = "Unknown";
        switch (type) {
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:          name = "Other";          break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: name = "Integrated GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   name = "Discrete GPU";   break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    name = "Virtual GPU";    break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            name = "CPU";            break;
            case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM:       name = "MAX_ENUM (sentinel)"; break;  // ADD THIS LINE
            default:                                     name = "Unknown";        break;
        }
        return std::formatter<std::string_view>::format(name, ctx);
    }
};

// =============================================================================
// GLOBAL ACCESSORS — STONEKEY v∞
// =============================================================================
[[nodiscard]] inline VkQueue                  g_graphicsQueue() noexcept;
[[nodiscard]] inline VkQueue                  g_presentQueue() noexcept;
[[nodiscard]] inline VkCommandPool            g_commandPool() noexcept;

// =============================================================================
// Forward Declarations
// =============================================================================
namespace RTX {
    void createCommandPool();
    bool createSurface(SDL_Window* window, VkInstance instance);
    void fixNvidiaValidationBugLocally() noexcept;
}

// =============================================================================
// AutoBuffer — RAII Buffer Wrapper
// =============================================================================
namespace RTX {
    class AutoBuffer {
    public:
        explicit AutoBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags props, std::string_view tag) noexcept;
        ~AutoBuffer() noexcept;
        AutoBuffer(AutoBuffer&& o) noexcept;
        AutoBuffer& operator=(AutoBuffer&& o) noexcept;
        AutoBuffer(const AutoBuffer&) = delete;
        AutoBuffer& operator=(const AutoBuffer&) = delete;
        [[nodiscard]] VkBuffer raw() const noexcept;
    private:
        uint64_t id = 0ULL;
    };
}

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
// VulkanRTX — The One True Ray Tracing Core
// =============================================================================
class VulkanRTX {
public:
    VulkanRTX(int w, int h, RTX::PipelineManager* mgr = nullptr) noexcept;
    ~VulkanRTX() noexcept;

    [[nodiscard]] VkDevice device() const noexcept { return device_; }

    [[nodiscard]] VkImage      blackFallbackImage() const noexcept { return blackFallbackImage_ ? blackFallbackImage_.get() : VK_NULL_HANDLE; }
    [[nodiscard]] VkImageView  blackFallbackView()   const noexcept { return blackFallbackView_   ? blackFallbackView_.get()   : VK_NULL_HANDLE; }
    [[nodiscard]] bool         hasBlackFallback()   const noexcept { return blackFallbackImage_ && blackFallbackImage_.get() != VK_NULL_HANDLE; }

    void buildAccelerationStructures();
    void initDescriptorPoolAndSets() noexcept;
    void initShaderBindingTable(VkPhysicalDevice pd) noexcept;
    void initBlackFallbackImage();

    void updateRTXDescriptors(
        uint32_t frameIdx,
        VkBuffer cameraBuf, VkBuffer materialBuf, VkBuffer dimensionBuf,
        VkImageView storageView, VkImageView accumView,
        VkImageView envMapView, VkSampler envSampler,
        VkImageView densityVol = VK_NULL_HANDLE,
        VkImageView gDepth = VK_NULL_HANDLE,
        VkImageView gNormal = VK_NULL_HANDLE);

    void recordRayTrace(VkCommandBuffer cmd, VkExtent2D extent, VkImage outputImage, VkImageView outputView) noexcept;
    void recordRayTraceAdaptive(VkCommandBuffer cmd, VkExtent2D extent, VkImage outputImage, VkImageView outputView, float nexusScore);
    void traceRays(VkCommandBuffer cmd,
                   const VkStridedDeviceAddressRegionKHR* raygen,
                   const VkStridedDeviceAddressRegionKHR* miss,
                   const VkStridedDeviceAddressRegionKHR* hit,
                   const VkStridedDeviceAddressRegionKHR* callable,
                   uint32_t width, uint32_t height, uint32_t depth) const noexcept;

    [[nodiscard]] VkDescriptorSet descriptorSet(uint32_t idx = 0) const noexcept {
        return (idx < descriptorSets_.size()) ? descriptorSets_[idx] : VK_NULL_HANDLE;
    }
    [[nodiscard]] VkPipeline       pipeline()       const noexcept { return HANDLE_GET(rtPipeline_); }
    [[nodiscard]] VkPipelineLayout pipelineLayout() const noexcept { return HANDLE_GET(rtPipelineLayout_); }
    [[nodiscard]] const ShaderBindingTable& sbt()   const noexcept { return sbt_; }

    void buildAccelerationStructuresBlocking() noexcept;
    void setDescriptorSetLayout(VkDescriptorSetLayout layout) noexcept;
    void setRayTracingPipeline(VkPipeline p, VkPipelineLayout l) noexcept;

    static VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) noexcept;
    static void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept;
    static bool pollAsyncFence(VkFence fence, uint64_t timeout_ns = UINT64_MAX) noexcept;

    void uploadBatch(
        const std::vector<std::tuple<const void*, VkDeviceSize, uint64_t, const char*>>& batch,
        VkCommandPool pool, VkQueue queue, bool async = false);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkExtent2D extent_{};
    RTX::PipelineManager* pipelineMgr_ = nullptr;

    RTX::Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    RTX::Handle<VkPipeline> rtPipeline_;
    RTX::Handle<VkPipelineLayout> rtPipelineLayout_;

    RTX::Handle<VkDescriptorPool> descriptorPool_;
    std::vector<VkDescriptorSet> descriptorSets_;

    RTX::Handle<VkBuffer> sbtBuffer_;
    RTX::Handle<VkDeviceMemory> sbtMemory_;
    VkDeviceAddress sbtAddress_ = 0;
    ShaderBindingTable sbt_{};
    VkDeviceSize sbtRecordSize_ = 0;

    RTX::Handle<VkImage> blackFallbackImage_;
    RTX::Handle<VkDeviceMemory> blackFallbackMemory_;
    RTX::Handle<VkImageView> blackFallbackView_;

    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;
    PFN_vkCmdTraceRaysKHR rtCmdTraceRaysKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR rtGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR rtGetAccelerationStructureDeviceAddressKHR = nullptr;

    [[nodiscard]] VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) const noexcept;
};

// =============================================================================
// g_rtx() — THE ONE TRUE SAFE ACCESSOR — FIRST LIGHT ETERNAL
// =============================================================================
inline std::unique_ptr<VulkanRTX> g_rtx_instance;
inline VulkanRTX g_rtx_dummy(1, 1, nullptr);

[[nodiscard]] inline VulkanRTX& g_rtx() noexcept
{
    if (!g_rtx_instance) {
        static bool warned_once = false;
        if (!warned_once && stone_device() != VK_NULL_HANDLE) {
            warned_once = true;
            LOG_WARN_CAT("RTX", 
                "{}g_rtx() called before forge — safe dummy active (pre-Phase 7){}", 
                YELLOW, RESET);
        }
        return g_rtx_dummy;
    }
    return *g_rtx_instance;
}