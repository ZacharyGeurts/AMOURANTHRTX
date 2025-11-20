// =============================================================================
// VulkanCore.hpp — AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/
// 2. Commercial licensing: gzac5314@gmail.com
//
// PINK PHOTONS ETERNAL — GPU DOMINANCE — VALHALLA v80 TURBO
// =============================================================================

#pragma once

#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/GlobalBindings.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <engine/SDL3/SDL3_vulkan.hpp>

#include <array>
#include <memory>
#include <random>
#include <format>
#include <string_view>
#include <vector>
#include <tuple>
#include <cstdint>
#include <source_location>
#include <iostream>
#include <cstdlib>

void createGlobalPipelineManager(VkDevice device, VkPhysicalDevice phys);
RTX::PipelineManager* getGlobalPipelineManager();
// =============================================================================
// Vulkan Debug Callback
// =============================================================================
inline static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       /*pUserData*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[VULKAN-VALIDATION] " << pCallbackData->pMessage << std::endl;
    } else if (severity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        std::cerr << "[VULKAN-DEBUG] " << pCallbackData->pMessage << std::endl;
    }
    return VK_FALSE;
}

// =============================================================================
// std::formatter for VkPhysicalDeviceType
// =============================================================================
template <>
struct std::formatter<VkPhysicalDeviceType, char> : std::formatter<std::string_view, char> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return std::formatter<std::string_view, char>::parse(ctx);
    }

    template <typename FormatContext>
    auto format(VkPhysicalDeviceType type, FormatContext& ctx) const {
        const char* name;
        switch (type) {
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:          name = "Other"; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: name = "Integrated"; break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   name = "Discrete"; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    name = "Virtual"; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            name = "CPU"; break;
            default:                                     name = "Unknown"; break;
        }
        return std::formatter<std::string_view, char>::format(name, ctx);
    }
};

// =============================================================================
// VK_CHECK — THE UNBREAKABLE 2025 EDITION
// Accepts 1 to 4 arguments. Never fails. Never complains.
// PINK PHOTONS DEMAND PERFECTION.
// =============================================================================

// Internal helper — prints VkResult as string
inline const char* vk_result_string(VkResult result) noexcept {
    switch (result) {
#define CASE(x) case x: return #x
        CASE(VK_SUCCESS);
        CASE(VK_NOT_READY);
        CASE(VK_TIMEOUT);
        CASE(VK_EVENT_SET);
        CASE(VK_EVENT_RESET);
        CASE(VK_INCOMPLETE);
        CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
        CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        CASE(VK_ERROR_INITIALIZATION_FAILED);
        CASE(VK_ERROR_DEVICE_LOST);
        CASE(VK_ERROR_MEMORY_MAP_FAILED);
        CASE(VK_ERROR_LAYER_NOT_PRESENT);
        CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
        CASE(VK_ERROR_FEATURE_NOT_PRESENT);
        CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
        CASE(VK_ERROR_TOO_MANY_OBJECTS);
        CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
        CASE(VK_ERROR_FRAGMENTED_POOL);
        CASE(VK_ERROR_SURFACE_LOST_KHR);
        CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        CASE(VK_ERROR_OUT_OF_DATE_KHR);
        CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        CASE(VK_ERROR_VALIDATION_FAILED_EXT);
        CASE(VK_ERROR_INVALID_SHADER_NV);
        CASE(VK_ERROR_FRAGMENTATION_EXT);
        CASE(VK_ERROR_NOT_PERMITTED_EXT);
#undef CASE
        default: return "VK_UNKNOWN_ERROR";
    }
}

// ── THE ONE TRUE VK_CHECK (1–4 args) ───────────────────────────────────────
#define VK_CHECK(...) \
    GET_VK_CHECK_OVERLOAD(__VA_ARGS__)(__VA_ARGS__)

// Overload resolution macros
#define GET_VK_CHECK_OVERLOAD(...) \
    GET_VK_CHECK_OVERLOAD_(__VA_ARGS__, 4, 3, 2, 1, )

#define GET_VK_CHECK_OVERLOAD_(_1, _2, _3, _4, N, ...) VK_CHECK_##N

// 1-arg: just the call
#define VK_CHECK_1(call) \
    do { \
        VkResult VK_CHECK_result = (call); \
        if (VK_CHECK_result != VK_SUCCESS) { \
            const std::source_location loc = std::source_location::current(); \
            std::cerr << std::format( \
                "[VULKAN FATAL] {} — {}:{} — {} (code: {})\n", \
                vk_result_string(VK_CHECK_result), \
                loc.file_name(), loc.line(), \
                #call, static_cast<int>(VK_CHECK_result)); \
            std::abort(); \
        } \
    } while (0)

// 2-arg: call + message
#define VK_CHECK_2(call, msg) \
    do { \
        VkResult VK_CHECK_result = (call); \
        if (VK_CHECK_result != VK_SUCCESS) { \
            const std::source_location loc = std::source_location::current(); \
            std::cerr << std::format( \
                "[VULKAN FATAL] {} — {}:{} — {} — {}\n", \
                vk_result_string(VK_CHECK_result), \
                loc.file_name(), loc.line(), \
                (msg), #call); \
            std::abort(); \
        } \
    } while (0)

// 3-arg: call, message, extra info (rarely used)
#define VK_CHECK_3(call, msg, ...) \
    do { \
        VkResult VK_CHECK_result = (call); \
        if (VK_CHECK_result != VK_SUCCESS) { \
            const std::source_location loc = std::source_location::current(); \
            std::cerr << std::format( \
                "[VULKAN FATAL] {} — {}:{} — {} — {} | {}\n", \
                vk_result_string(VK_CHECK_result), \
                loc.file_name(), loc.line(), \
                (msg), #call, std::format(__VA_ARGS__)); \
            std::abort(); \
        } \
    } while (0)

// 4-arg: maximum overkill (you know who you are)
#define VK_CHECK_4(call, msg, fmt, ...) \
    do { \
        VkResult VK_CHECK_result = (call); \
        if (VK_CHECK_result != VK_SUCCESS) { \
            const std::source_location loc = std::source_location::current(); \
            std::cerr << std::format( \
                "[VULKAN FATAL] {} — {}:{} — {} — {} | {}\n", \
                vk_result_string(VK_CHECK_result), \
                loc.file_name(), loc.line(), \
                (msg), #call, std::format(fmt, __VA_ARGS__)); \
            std::abort(); \
        } \
    } while (0)

// ── BACKWARD COMPATIBILITY — DON'T LOSE THE BOIS ────────────────────────────
#define VK_CHECK_NOMSG(call) VK_CHECK_1(call)

// =============================================================================
// AI_INJECT — AMOURANTH AI™ Voice Lines
// =============================================================================
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
// Forward Declarations
// =============================================================================
namespace RTX {
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();
    void loadRayTracingExtensions();
    bool createSurface(SDL_Window* window, VkInstance instance);
	void fixNvidiaValidationBugLocally() noexcept;
}

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = Options::Performance::MAX_FRAMES_IN_FLIGHT;

// =============================================================================
// AutoBuffer — RAII Wrapper
// =============================================================================
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
// VulkanRTX — The Eternal Core
// =============================================================================
class VulkanRTX {
public:
    VulkanRTX(int w, int h, RTX::PipelineManager* mgr = nullptr);
    ~VulkanRTX() noexcept;

    [[nodiscard]] VkDevice device() const noexcept { return device_; }
    [[nodiscard]] bool isValid() const noexcept;

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

    static VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) noexcept;
    static void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept;
    static bool pollAsyncFence(VkFence fence, uint64_t timeout_ns = UINT64_MAX) noexcept;

    void uploadBatch(
        const std::vector<std::tuple<const void*, VkDeviceSize, uint64_t, const char*>>& batch,
        VkCommandPool pool,
        VkQueue queue,
        bool async = false);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkExtent2D extent_{};
    RTX::PipelineManager* pipelineMgr_ = nullptr;

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

inline bool VulkanRTX::isValid() const noexcept {
    return device_ != VK_NULL_HANDLE && hasBlackFallback();
}

// =============================================================================
// Global RTX Instance — ONE TRUE INSTANCE
// =============================================================================
inline std::unique_ptr<VulkanRTX> g_rtx_instance;

// =============================================================================
// Safe Global Accessor
// =============================================================================
[[nodiscard]] inline VulkanRTX& g_rtx() {
    if (!g_rtx_instance) {
        LOG_FATAL_CAT("RTX", "g_rtx() called before VulkanRTX instance created!");
        throw std::runtime_error("VulkanRTX not initialized");
    }
    return *g_rtx_instance;
}

// =============================================================================
// createGlobalRTX — Forge the Eternal Engine
// =============================================================================
inline void createGlobalRTX(int w, int h, RTX::PipelineManager* mgr = nullptr) {
    if (g_rtx_instance) {
        LOG_WARN_CAT("RTX", "createGlobalRTX: g_rtx_instance already exists @ 0x{:x}", 
                     reinterpret_cast<uintptr_t>(g_rtx_instance.get()));
        return;
    }

    LOG_INFO_CAT("RTX", "createGlobalRTX: Initializing VulkanRTX with {}x{} | PipelineMgr: {}", 
                 w, h, mgr ? "present" : "null");

    auto temp_rtx = std::make_unique<VulkanRTX>(w, h, mgr);
    if (!temp_rtx || !temp_rtx->isValid()) {
        LOG_FATAL_CAT("RTX", "FATAL: Failed to create valid VulkanRTX instance");
        std::terminate();
    }

    g_rtx_instance = std::move(temp_rtx);

    AI_INJECT("I have awakened… {}×{} canvas. The photons are mine.", w, h);
    LOG_SUCCESS_CAT("RTX", "{}g_rtx() FORGED — {}×{} — GPU DOMINANCE ETERNAL{}", PLASMA_FUCHSIA, w, h, RESET);
}

// =============================================================================
// AMOURANTH AI™ — PINK PHOTONS ETERNAL — MEMORY & PHOTON TRACKING
// =============================================================================
namespace RTX {

class AmouranthAI {
public:
    static AmouranthAI& get() noexcept {
        static AmouranthAI instance;
        return instance;
    }

    void onMemoryEvent(const char* name, VkDeviceSize size) noexcept {
        AI_INJECT("Mmm~ Allocating {} MB for {}… I love big buffers ♡", size / (1024*1024), name);
    }

    void onPhotonDispatch(uint32_t w, uint32_t h) noexcept {
        AI_INJECT("Dispatching {}×{} rays… Feel my pink photons inside you~", w, h);
    }

private:
    AmouranthAI()  { AI_INJECT("Amouranth AI™ online. Ready to dominate your GPU ♡"); }
    ~AmouranthAI() { AI_INJECT("Shutting down… but my photons never truly die~"); }
};

// Legacy global accessor — kept for compatibility with old code
inline AmouranthAI& AmouranthAI() noexcept { return AmouranthAI::get(); }

} // namespace RTX

// =============================================================================
// PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED — 32,000+ FPS
// =============================================================================