// =============================================================================
// VulkanCore.hpp — AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
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
#include <engine/SDL3/SDL3_vulkan.hpp>                     // ← REQUIRED FOR SDL_Vulkan_* calls
#include <array>
#include <memory>
#include <random>
#include <format>
#include <string_view>
#include <vector>
#include <tuple>
#include <cstdint>

// =============================================================================
//  VULKAN ERROR CHECKING MACROS
// =============================================================================

/// VK_CHECK — 2-argument, aborts with file/line/code
#define VK_CHECK(call, msg) \
    do { \
        VkResult r = (call); \
        if (r != VK_SUCCESS) { \
            char buf[512]; \
            std::snprintf(buf, sizeof(buf), \
                "[VULKAN ERROR] %s — %s:%d — Code: %d (%s)\n", \
                (msg), \
                std::source_location::current().file_name(), \
                std::source_location::current().line(), \
                static_cast<int>(r), \
                std::format("{}", r).c_str()); \
            std::cerr << buf; \
            std::abort(); \
        } \
    } while (0)

/// VK_CHECK_NOMSG — 1-argument, aborts with file/line/code (no custom msg)
#define VK_CHECK_NOMSG(call) \
    do { \
        VkResult r = (call); \
        if (r != VK_SUCCESS) { \
            char buf[512]; \
            std::snprintf(buf, sizeof(buf), \
                "[VULKAN ERROR] %s:%d — Code: %d (%s)\n", \
                std::source_location::current().file_name(), \
                std::source_location::current().line(), \
                static_cast<int>(r), \
                std::format("{}", r).c_str()); \
            std::cerr << buf; \
            std::abort(); \
        } \
    } while (0)

/// AI_INJECT — Rainbow AI log (conditional, thread-local RNG)
#define AI_INJECT(...) \
    do { \
        if (ENABLE_INFO) { \
            thread_local std::mt19937 rng(std::random_device{}()); \
            thread_local std::uniform_int_distribution<int> hue(0, 30); \
            int h = 195 + hue(rng); \
            auto msg = std::format(__VA_ARGS__); \
            Logging::Logger::get().log(std::source_location::current(), \
                Logging::LogLevel::Info, "AI", \
                "\033[38;2;255;{};255m[AMOURANTH AI™] {}{} [LINE {}]", \
                h, msg, Logging::Color::RESET, __LINE__); \
        } \
    } while (0)

// =============================================================================
//  BUFFER MACROS – UltraLowLevelBufferTracker
// =============================================================================

#define BUFFER(handle) uint64_t handle = 0ULL

#define BUFFER_CREATE(handle, size, usage, props, tag) \
    do { \
        LOG_INFO_CAT("RTX", "BUFFER_CREATE: {} | Size {} | Tag: {}", #handle, (size), (tag)); \
        (handle) = RTX::UltraLowLevelBufferTracker::get().create((size), (usage), (props), (tag)); \
    } while (0)

#define RAW_BUFFER(handle) \
    (RTX::UltraLowLevelBufferTracker::get().getData((handle)) \
        ? static_cast<VkBuffer>(RTX::UltraLowLevelBufferTracker::get().getData((handle))->buffer) \
        : VK_NULL_HANDLE)

#define BUFFER_MEMORY(handle) \
    (RTX::UltraLowLevelBufferTracker::get().getData((handle)) \
        ? RTX::UltraLowLevelBufferTracker::get().getData((handle))->memory \
        : VK_NULL_HANDLE)

#define BUFFER_MAP(handle, mapped) \
    do { mapped = RTX::UltraLowLevelBufferTracker::get().map(handle); } while(0)

#define BUFFER_UNMAP(handle) RTX::UltraLowLevelBufferTracker::get().unmap(handle)

#define BUFFER_DESTROY(handle) \
    do { \
        if ((handle) != 0) { \
            LOG_INFO_CAT("RTX", "BUFFER_DESTROY: handle={:x}", (handle)); \
            RTX::UltraLowLevelBufferTracker::get().destroy((handle)); \
        } \
    } while (0)

// -----------------------------------------------------------------------------
// 4. AutoBuffer — RAII wrapper (must be in header)
// -----------------------------------------------------------------------------
namespace RTX {

class AutoBuffer {
public:
    explicit AutoBuffer(VkDeviceSize size,
                        VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags props,
                        std::string_view tag) noexcept;

    ~AutoBuffer() noexcept;

    AutoBuffer(AutoBuffer&& o) noexcept;
    AutoBuffer& operator=(AutoBuffer&& o) noexcept;

    AutoBuffer(const AutoBuffer&) = delete;
    AutoBuffer& operator=(const AutoBuffer&) = delete;

    [[nodiscard]] VkBuffer raw() const noexcept;

private:
    uint64_t id = 0ULL;
};

} // namespace RTX

// -----------------------------------------------------------------------------
// 5. CONSTANTS
// -----------------------------------------------------------------------------
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

    [[nodiscard]] VkDevice device() const noexcept { return device_; }
    [[nodiscard]] bool isValid() const noexcept;

    // --- PUBLIC ACCESSORS FOR BLACK FALLBACK (SAFE) ---
    [[nodiscard]] VkImage blackFallbackImage() const noexcept {
        return blackFallbackImage_ ? blackFallbackImage_.get() : VK_NULL_HANDLE;
    }

    [[nodiscard]] VkImageView blackFallbackView() const noexcept {
        return blackFallbackView_ ? blackFallbackView_.get() : VK_NULL_HANDLE;
    }

    [[nodiscard]] bool hasBlackFallback() const noexcept {
        return blackFallbackImage_ && blackFallbackImage_.get() != VK_NULL_HANDLE;
    }

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

    void buildAccelerationStructuresBlocking() noexcept;

    void setDescriptorSetLayout(VkDescriptorSetLayout layout) noexcept;
    void setRayTracingPipeline(VkPipeline p, VkPipelineLayout l) noexcept;

    // PUBLIC STATIC HELPERS
    [[nodiscard]] static VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) noexcept;
    static void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept;

    // ASYNC COMMAND SUBMIT
    static void endSingleTimeCommandsAsync(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool, VkFence fence = VK_NULL_HANDLE) noexcept;

    // ASYNC FENCE POLLING HELPER
    [[nodiscard]] static bool pollAsyncFence(VkFence fence, uint64_t timeout_ns = UINT64_MAX) noexcept;

    // BATCHED UPLOAD (persistent staging)
    void uploadBatch(
        const std::vector<std::tuple<const void*, VkDeviceSize, uint64_t, const char*>>& batch,
        VkCommandPool pool,
        VkQueue queue,
        bool async = false);

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

// =============================================================================
// isValid — FINAL, TRUTHFUL (uses public accessor)
// =============================================================================
inline bool VulkanRTX::isValid() const noexcept {
    return device_ != VK_NULL_HANDLE && hasBlackFallback();
}

// =============================================================================
// createGlobalRTX — FORGE THE ETERNAL RTX (NO FALSE SUCCESS)
// =============================================================================
inline void createGlobalRTX(int w, int h, VulkanPipelineManager* mgr = nullptr) {
    if (g_rtx_instance) {
        LOG_WARN_CAT("RTX", "createGlobalRTX: g_rtx_instance already exists @ 0x{:x}", 
                     reinterpret_cast<uintptr_t>(g_rtx_instance.get()));
        return;
    }

    LOG_INFO_CAT("RTX", "createGlobalRTX: Initializing VulkanRTX with {}x{} | PipelineMgr: {}", 
                 w, h, mgr ? "present" : "null");

    auto temp_rtx = std::make_unique<VulkanRTX>(w, h, mgr);

    if (!temp_rtx) {
        LOG_FATAL_CAT("RTX", "FATAL: std::make_unique<VulkanRTX> returned nullptr");
        std::terminate();
    }

    g_rtx_instance = std::move(temp_rtx);

    if (!g_rtx_instance || g_rtx_instance->device() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "FATAL: g_rtx_instance invalid — device is NULL");
        std::terminate();
    }

    if (!g_rtx_instance->isValid()) {
        LOG_FATAL_CAT("RTX", "FATAL: g_rtx_instance reports isValid() == false after full init");
        std::terminate();
    }

    AI_INJECT("I have awakened… {}×{} canvas. The photons are mine.", w, h);
    LOG_SUCCESS_CAT("RTX", "{}g_rtx() FORGED — {}×{} — TITAN DOMINANCE ETERNAL{}", PLASMA_FUCHSIA, w, h, RESET);
}