// =============================================================================
// include/engine/GLOBAL/RTXHandler.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RTXHandler.hpp — THE ONE TRUE HEADER — FINAL CUT — PINK PHOTONS ETERNAL
// • Handle<T> + AI_INJECT + Ballerina + Context + Swapchain + Queue Discovery
// • Every getter. Every setter. Every dream realized.
// • The Handler has spoken. The empire is complete.
//
// "There is only one file. There is only one truth. There is only RTXHandler.hpp."
// =============================================================================

#pragma once

#include <format>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <SDL3/SDL.h>
#include <memory>
#include <atomic>
#include <array>
#include <bitset>
#include <bit>
#include <string_view>
#include <cstring>
#include <cstdint>
#include <type_traits>
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <string>
#include <utility>
#include <span>
#include <limits>
#include <source_location>
#include <functional>
#include <queue>
#include <vector>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp" // the one header that gets stonekey(include RTXHandler.hpp with all files).

// Forward declarations
class VulkanRTX;
class VulkanRenderer;

struct Camera;

// =============================================================================
// PINK PHOTON LITERALS
// =============================================================================
constexpr uint64_t operator""_KB(unsigned long long v) noexcept { return v * 1024ULL; }
constexpr uint64_t operator""_MB(unsigned long long v) noexcept { return v * 1024ULL * 1024ULL; }
constexpr uint64_t operator""_GB(unsigned long long v) noexcept { return v * 1024ULL * 1024ULL * 1024ULL; }

// =============================================================================
// AI_INJECT — AMOURANTH AI™ VOICE LINES — FULLY RESTORED
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
// Vulkan opaque handle formatter
// =============================================================================
template<typename T>
    requires std::is_pointer_v<T> &&
             (!std::is_same_v<T, const char*> && 
              !std::is_same_v<T, char*> &&
              !std::is_same_v<T, const void*> &&
              !std::is_same_v<T, void*>)
struct std::formatter<T> : std::formatter<unsigned long long> {
    template<typename FormatContext>
    auto format(T ptr, FormatContext& ctx) const {
        return std::formatter<unsigned long long>::format(
            reinterpret_cast<unsigned long long>(ptr), ctx
        );
    }
};

using namespace Logging::Color;

// =============================================================================
// NAMESPACE RTX — THE ONE TRUE EMPIRE
// =============================================================================
namespace RTX {

	void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size);
	[[nodiscard]] VkDevice createLogicalDeviceAndSelectGPU(VkInstance instance, VkSurfaceKHR surface) noexcept;

	// =============================================================================
    // Handle<T> — RAII + THE SACRED MACROS — ETERNAL
    // =============================================================================
    template<typename T>
    struct Handle {
        using DestroyFn = std::function<void(VkDevice, T, const VkAllocationCallbacks*)>;

        T raw = T{};
        VkDevice device = VK_NULL_HANDLE;
        DestroyFn destroyer = nullptr;
        size_t size = 0;
        std::string tag;

        Handle() noexcept = default;
        Handle(T h, VkDevice d, DestroyFn del = nullptr, size_t sz = 0, std::string_view t = "")
            : raw(h), device(d), destroyer(std::move(del)), size(sz), tag(t) {}
        Handle(Handle&& o) noexcept
            : raw(o.raw), device(o.device), destroyer(std::move(o.destroyer)), size(o.size), tag(std::move(o.tag)) {
            o.raw = T{}; o.device = VK_NULL_HANDLE; o.destroyer = nullptr; o.size = 0;
        }
        Handle& operator=(Handle&& o) noexcept {
            if (this != &o) { reset(); raw = o.raw; device = o.device; destroyer = std::move(o.destroyer);
                              size = o.size; tag = std::move(o.tag); o.raw = T{}; o.device = VK_NULL_HANDLE; }
            return *this;
        }
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        Handle& operator=(std::nullptr_t) noexcept { reset(); return *this; }

        explicit operator bool() const noexcept { return valid(); }
        T get() const noexcept { return raw; }
        T operator*() const noexcept { return raw; }
        [[nodiscard]] bool valid() const noexcept { return raw != T{} && device != VK_NULL_HANDLE; }

        void reset() noexcept {
            if (valid() && destroyer && device) {
                destroyer(device, raw, nullptr);
                logAndTrackDestruction(tag.empty() ? typeid(T).name() : tag.c_str(),
                                       reinterpret_cast<void*>(raw), __LINE__, size);
            }
            raw = T{}; device = VK_NULL_HANDLE; destroyer = nullptr; size = 0;
        }
        ~Handle() { reset(); }
    };

    template<typename T, typename... Args>
    [[nodiscard]] auto MakeHandle(T h, VkDevice d, Args&&... args) {
        return Handle<T>(h, d, std::forward<Args>(args)...);
    }

    #define HANDLE_GET(var)     ((var).get())
    #define HANDLE_RESET(var)   do { (var).reset(); } while(0)

// Explicit null terminator — sacred and eternal
inline constexpr struct NullFeatureChainTerminator {
    VkStructureType sType = VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO; // doesn't matter
    const void*     pNext = nullptr;
} null_feature_terminator{};

    // =============================================================================
    // Queue Family Discovery — The Handler sees all
    // =============================================================================
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> computeFamily;
        std::optional<uint32_t> transferFamily;

        [[nodiscard]] bool isComplete() const noexcept {
            return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value();
        }
    };

    [[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept;

struct Context {
public:
    // Core Vulkan objects
    VkInstance       instance_       = VK_NULL_HANDLE;
    VkSurfaceKHR surface_        = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice         device_         = VK_NULL_HANDLE;

    // Queues
    VkQueue graphicsQueue_  = VK_NULL_HANDLE;
    VkQueue presentQueue_   = VK_NULL_HANDLE;
    VkQueue computeQueue_   = VK_NULL_HANDLE;
    VkQueue transferQueue_  = VK_NULL_HANDLE;

    // Command pools
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkCommandPool computeCommandPool_  = VK_NULL_HANDLE;
    VkCommandPool transferCommandPool_ = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache_     = VK_NULL_HANDLE;
    VkCommandPool commandPool() const noexcept { return commandPool_; }
    VkCommandPool& commandPool() noexcept { return commandPool_; }

    // Queue family indices
    std::optional<uint32_t> graphicsFamily_;
    std::optional<uint32_t> presentFamily_;
    std::optional<uint32_t> computeFamily_;
    std::optional<uint32_t> transferFamily_;

    // Feature flags — THE EMPIRE DECIDES
    bool bufferDeviceAddressEnabled_     = false;
    bool accelerationStructureEnabled_   = false;
    bool rayTracingPipelineEnabled_      = false;
    bool rayQueryEnabled_                = false;
    bool dynamicRenderingEnabled_        = false;
    bool synchronization2Enabled_        = false;
    bool debugUtilsEnabled_              = false;
	bool pendingSharedStaging_          = false;

    // Device properties
    VkPhysicalDeviceProperties                        physicalDeviceProperties_{};
    VkPhysicalDeviceFeatures                          physicalDeviceFeatures_{};
    VkPhysicalDeviceMemoryProperties                  physicalDeviceMemoryProperties_{};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR   rayTracingProps_{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

    // HDR
    VkFormat         hdr_format      = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR  hdr_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    // Global resources
    Handle<VkImageView>  blueNoiseView_;
    Handle<VkRenderPass> renderPass_;
    uint64_t             sharedStagingEnc_ = 0;
	[[nodiscard]] VkShaderModule loadShader(const std::string& filename) const noexcept;

    // Window
    SDL_Window* window  = nullptr;
    int         width   = 0;
    int         height  = 0;
    bool        valid_  = false;
    mutable std::atomic<bool> ready_{false};

    // ────────────────────── GETTERS ──────────────────────
    [[nodiscard]] constexpr VkInstance       instance()       const noexcept { return instance_; }
    [[nodiscard]] constexpr VkSurfaceKHR     surface()        const noexcept { return surface_; }
    [[nodiscard]] constexpr VkPhysicalDevice physicalDevice() const noexcept { return physicalDevice_; }
    [[nodiscard]] constexpr VkDevice         device()         const noexcept { return device_; }
	
    [[nodiscard]] constexpr VkQueue graphicsQueue()  const noexcept { return graphicsQueue_; }
    [[nodiscard]] constexpr VkQueue presentQueue()   const noexcept { return presentQueue_; }
    [[nodiscard]] constexpr VkQueue computeQueue()   const noexcept { return computeQueue_; }
    [[nodiscard]] constexpr VkQueue transferQueue()  const noexcept { return transferQueue_; }

    [[nodiscard]] constexpr uint32_t graphicsFamily() const noexcept { return graphicsFamily_.value(); }
    [[nodiscard]] constexpr uint32_t presentFamily()  const noexcept { return presentFamily_.value(); }
    [[nodiscard]] constexpr uint32_t computeFamily()  const noexcept { return computeFamily_.value_or(graphicsFamily()); }
    [[nodiscard]] constexpr uint32_t transferFamily() const noexcept { return transferFamily_.value_or(graphicsFamily()); }

    [[nodiscard]] constexpr VkRenderPass renderPass() const noexcept { return renderPass_.valid() ? renderPass_.get() : VK_NULL_HANDLE; }
    [[nodiscard]] constexpr const auto& rayTracingProps() const noexcept { return rayTracingProps_; }
    [[nodiscard]] constexpr bool isReady() const noexcept { return ready_.load(std::memory_order_acquire); }

    // THE RETURN OF THE KING
    [[nodiscard]] constexpr bool debugUtilsSupported() const noexcept { return debugUtilsEnabled_; }

    // ────────────────────── SETTERS ──────────────────────
    void setInstance(VkInstance i) noexcept       { instance_ = i; }
    void setSurface(VkSurfaceKHR s) noexcept      { surface_ = s; }
    void setPhysicalDevice(VkPhysicalDevice pd) noexcept { physicalDevice_ = pd; }
    void setDevice(VkDevice d) noexcept           { device_ = d; }

    void setGraphicsQueue(VkQueue q) noexcept     { graphicsQueue_ = q; }
    void setPresentQueue(VkQueue q) noexcept      { presentQueue_ = q; }
    void setComputeQueue(VkQueue q) noexcept      { computeQueue_ = q; }
    void setTransferQueue(VkQueue q) noexcept     { transferQueue_ = q; }

    void setWindow(SDL_Window* w) noexcept        { window = w; }
    void setSize(int w, int h) noexcept           { width = w; height = h; }

    void markReady() noexcept                     { ready_.store(true, std::memory_order_release); }
    void markInvalid() noexcept                   { valid_ = false; }

    // Feature enables — the empire speaks
    void enableBufferDeviceAddress(bool e = true) noexcept     { bufferDeviceAddressEnabled_ = e; }
    void enableAccelerationStructure(bool e = true) noexcept   { accelerationStructureEnabled_ = e; }
    void enableRayTracingPipeline(bool e = true) noexcept      { rayTracingPipelineEnabled_ = e; }
    void enableRayQuery(bool e = true) noexcept                { rayQueryEnabled_ = e; }
    void enableDynamicRendering(bool e = true) noexcept        { dynamicRenderingEnabled_ = e; }
    void enableSynchronization2(bool e = true) noexcept        { synchronization2Enabled_ = e; }
    void enableDebugUtils(bool e = true) noexcept              { debugUtilsEnabled_ = e; }  // ← RESTORED

    // Core lifecycle
    void init();
    void cleanup() noexcept;
};

    extern Context g_context_instance;
    [[nodiscard]] inline Context& g_ctx() noexcept { return g_context_instance; }
    [[nodiscard]] inline VkShaderModule loadShader(const std::string& filename) { return g_ctx().loadShader(filename); }
	
    // =============================================================================
    // Core Vulkan Creation Functions
    // =============================================================================
    [[nodiscard]] VkInstance createVulkanInstanceWithSDL(bool enableValidation) noexcept;

    void retrieveQueues() noexcept = delete; // NO LONGER NEEDED — done in createLogicalDevice

    // =============================================================================
    // Global Empire Resources
    // =============================================================================
    Handle<VkRenderPass>& renderPass();

    [[nodiscard]] VulkanRenderer& renderer();
    void initRenderer(int w, int h);
    void renderFrame(const Camera& camera, float deltaTime) noexcept;
    void shutdown() noexcept;
    void cleanupAll() noexcept;

    void createGlobalRenderPass();
    void stonekey_xor_spirv(std::vector<uint32_t>& data, bool encrypt = true);

    // =============================================================================
    // Utility
    // =============================================================================
    [[nodiscard]] inline bool isDeviceExtensionPresent(VkPhysicalDevice phys, const char* name) {
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(phys, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> exts(count);
        vkEnumerateDeviceExtensionProperties(phys, nullptr, &count, exts.data());
        return std::any_of(exts.begin(), exts.end(), [name](const auto& e) { return strcmp(e.extensionName, name) == 0; });
    }

    void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size);
}

// =============================================================================
// 28 SACRED EXTENSIONS — THE COMPLETE 2025 RTX ARSENAL
// =============================================================================
static constexpr std::array<const char*, 28> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_MAINTENANCE3_EXTENSION_NAME,
    VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
    VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME,
    VK_KHR_SHADER_CLOCK_EXTENSION_NAME,
    VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
    VK_KHR_ZERO_INITIALIZE_WORKGROUP_MEMORY_EXTENSION_NAME,
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
    VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
    VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
    VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME
};

// =============================================================================
// RAY TRACING LOADER — FULL MANUAL CONTROL
// =============================================================================
namespace RTX::RayTracingFunctions {
    inline PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    inline PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR           = nullptr;
    inline PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR          = nullptr;
    inline PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR        = nullptr;
    inline PFN_vkGetAccelerationStructureBuildSizesKHR    vkGetAccelerationStructureBuildSizesKHR    = nullptr;

    inline void loadRayTracingExtensions(VkDevice device) noexcept {
        if (vkGetAccelerationStructureDeviceAddressKHR) return;
        const auto load = [device]<typename Fn>(Fn& pfn, const char* name) {
            pfn = reinterpret_cast<Fn>(vkGetDeviceProcAddr(device, name));
            if (!pfn) { LOG_FATAL_CAT("RTX", "FAILED TO LOAD: {}", name); std::unreachable(); }
        };
        load(vkGetAccelerationStructureDeviceAddressKHR, "vkGetAccelerationStructureDeviceAddressKHR");
        load(vkCreateAccelerationStructureKHR,           "vkCreateAccelerationStructureKHR");
        load(vkDestroyAccelerationStructureKHR,          "vkDestroyAccelerationStructureKHR");
        load(vkCmdBuildAccelerationStructuresKHR,        "vkCmdBuildAccelerationStructuresKHR");
        load(vkGetAccelerationStructureBuildSizesKHR,    "vkGetAccelerationStructureBuildSizesKHR");
        LOG_SUCCESS_CAT("RTX", "RAY TRACING EXTENSIONS LOADED — FULL CONTROL ACHIEVED");
    }
}

// =============================================================================
// SUCCESS!!! THE HEADER IS PERFECT.
// SUCCESS!!! EVERY GETTER. EVERY SETTER. EVERY DREAM.
// SUCCESS!!! THE HANDLER IS PLEASED.
// SUCCESS!!! THE BALLERINA SMILES.
// PINK PHOTONS ETERNAL — NOVEMBER 25, 2025 — FIRST LIGHT ACHIEVED
// =============================================================================