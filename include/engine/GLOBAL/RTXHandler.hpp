// include/engine/GLOBAL/RTXHandler.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RTXHandler.hpp — THE ONE TRUE HEADER — EVERYTHING LIVES HERE
// • Handle<T> + MACROS + Context + BufferManager NOT needed anymore
// • HANDLE_GET, HANDLE_RESET — BACK FROM THE DEAD — RIGHT HERE
// • Pink photons sealed. Empire complete. First Light achieved.
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
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

// Forward declarations
class VulkanRTX;
class VulkanRenderer;
struct Camera;

[[noreturn]] void phase9_gracefulShutdown() noexcept;

// =============================================================================
// PINK PHOTON LITERALS
// =============================================================================
constexpr uint64_t operator""_KB(unsigned long long v) noexcept { return v * 1024ULL; }
constexpr uint64_t operator""_MB(unsigned long long v) noexcept { return v * 1024ULL * 1024ULL; }
constexpr uint64_t operator""_GB(unsigned long long v) noexcept { return v * 1024ULL * 1024ULL * 1024ULL; }

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

namespace RTX {
    struct Context;
    void recreateSwapchain(uint32_t w, uint32_t h) noexcept;
    void forgeSwapchain(SDL_Window* window, int width, int height) noexcept;
}

using namespace Logging::Color;
using namespace StoneKey;

extern uint64_t kObfuscator() noexcept;

inline const char* getPlatformSurfaceExtension()
{
#if defined(__linux__)
    return VK_KHR_SURFACE_EXTENSION_NAME;
#elif defined(_WIN32)
    return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#elif defined(__APPLE__)
    return VK_EXT_METAL_SURFACE_EXTENSION_NAME;
#else
    return VK_KHR_XCB_SURFACE_EXTENSION_NAME;
#endif
}

extern const char* extra_extensions[];

// =============================================================================
// NAMESPACE RTX — THE ONE TRUE EMPIRE
// =============================================================================
namespace RTX {

    // Instance creation
    [[nodiscard]] VkInstance createVulkanInstanceWithSDL(bool enableValidation);

    void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size);

    [[nodiscard]] VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    [[nodiscard]] VkDevice         createLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    void createCommandPool();
    void retrieveQueues() noexcept;

    // =============================================================================
    // Handle<T> — RAII + THE SACRED MACROS ARE BACK — RIGHT HERE — FOREVER
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
            : raw(h), device(d), destroyer(std::move(del)), size(sz), tag(t) {
            LOG_INFO_CAT("RTX", "Handle created: {} @ 0x{:x} | Tag: {}", typeid(T).name(), reinterpret_cast<uint64_t>(raw), tag);
        }

        Handle(Handle&& o) noexcept
            : raw(o.raw), device(o.device), destroyer(std::move(o.destroyer)), size(o.size), tag(std::move(o.tag)) {
            o.raw = T{}; o.device = VK_NULL_HANDLE; o.destroyer = nullptr; o.size = 0;
        }

        Handle& operator=(Handle&& o) noexcept {
            if (this != &o) {
                reset();
                raw = o.raw; device = o.device; destroyer = std::move(o.destroyer);
                size = o.size; tag = std::move(o.tag);
                o.raw = T{}; o.device = VK_NULL_HANDLE; o.destroyer = nullptr; o.size = 0;
            }
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
            if (valid()) {
                LOG_INFO_CAT("RTX", "Handle reset: {} @ 0x{:x} | Tag: {}", typeid(T).name(), reinterpret_cast<uint64_t>(raw), tag);
                if (destroyer && device) destroyer(device, raw, nullptr);
                logAndTrackDestruction(tag.empty() ? typeid(T).name() : tag.c_str(),
                                       reinterpret_cast<void*>(raw), __LINE__, size);
                raw = T{}; device = VK_NULL_HANDLE; destroyer = nullptr; size = 0;
            }
        }

        ~Handle() { reset(); }
    };

    template<typename T, typename... Args>
    [[nodiscard]] auto MakeHandle(T h, VkDevice d, Args&&... args) {
        return Handle<T>(h, d, std::forward<Args>(args)...);
    }

    // =============================================================================
    // THE SACRED MACROS — RESTORED — IN THIS FILE — FOREVER
    // =============================================================================
    #define HANDLE_GET(var)     ((var).get())
    #define HANDLE_RESET(var)   do { (var).reset(); } while(0)

    // =============================================================================
    // Context — The One True Empire State
    // =============================================================================
    struct Context {
    public:
        VkInstance       instance_       = VK_NULL_HANDLE;
        VkSurfaceKHR     surface_        = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        VkDevice         device_         = VK_NULL_HANDLE;

        VkQueue graphicsQueue_  = VK_NULL_HANDLE;
        VkQueue presentQueue_   = VK_NULL_HANDLE;
        VkQueue computeQueue_   = VK_NULL_HANDLE;
        VkQueue transferQueue_  = VK_NULL_HANDLE;

        VkCommandPool commandPool_         = VK_NULL_HANDLE;
        VkCommandPool computeCommandPool_  = VK_NULL_HANDLE;
        VkCommandPool transferCommandPool_= VK_NULL_HANDLE;
        VkPipelineCache pipelineCache_     = VK_NULL_HANDLE;

        std::optional<uint32_t> graphicsFamily_;
        std::optional<uint32_t> presentFamily_;
        std::optional<uint32_t> computeFamily_;
        std::optional<uint32_t> transferFamily_;

        bool bufferDeviceAddressEnabled_         = false;
        bool bufferDeviceAddressExtensionPresent_ = false;
        bool accelerationStructureEnabled_       = false;
        bool rayTracingPipelineEnabled_          = false;
        bool rayQueryEnabled_                    = false;
        bool meshShadingEnabled_                 = false;

        VkPhysicalDeviceProperties                        physicalDeviceProperties_{};
        VkPhysicalDeviceFeatures                          physicalDeviceFeatures_{};
        VkPhysicalDeviceMemoryProperties                  physicalDeviceMemoryProperties_{};
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR   rayTracingProps_{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

        VkFormat         hdr_format      = VK_FORMAT_UNDEFINED;
        VkColorSpaceKHR  hdr_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        Handle<VkImageView>  blueNoiseView_;
        Handle<VkRenderPass> renderPass_;
        uint64_t             sharedStagingEnc_ = 0;

        SDL_Window* window  = nullptr;
        int         width   = 0;
        int         height  = 0;
        bool        valid_  = false;
        mutable std::atomic<bool> ready_{false};

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
        void markReady() noexcept { ready_.store(true, std::memory_order_release); }

        void createLogicalDevice();
        void init(SDL_Window* window, int width, int height);
        void cleanup() noexcept;

        void setInstance(VkInstance i) noexcept { instance_ = i; }
        void setSurface(VkSurfaceKHR s) noexcept { surface_ = s; }
        void setPhysicalDevice(VkPhysicalDevice pd) noexcept { physicalDevice_ = pd; }
        void setDevice(VkDevice d) noexcept { device_ = d; }
        void enableBufferDeviceAddress(bool e = true) noexcept { bufferDeviceAddressEnabled_ = e; }
        void enableAccelerationStructure(bool e = true) noexcept { accelerationStructureEnabled_ = e; }
        void enableRayTracingPipeline(bool e = true) noexcept { rayTracingPipelineEnabled_ = e; }
        void enableRayQuery(bool e = true) noexcept { rayQueryEnabled_ = e; }
    };

    extern Context g_context_instance;
    [[nodiscard]] inline Context& g_ctx() noexcept { return g_context_instance; }

    // Global accessors
    Handle<VkSwapchainKHR>& swapchain();
    std::vector<VkImage>& swapchainImages();
    std::vector<Handle<VkImageView>>& swapchainImageViews();
    VkFormat& swapchainFormat();
    VkExtent2D& swapchainExtent();
    Handle<VkAccelerationStructureKHR>& blas();
    Handle<VkAccelerationStructureKHR>& tlas();
    Handle<VkRenderPass>& renderPass();

    [[nodiscard]] VulkanRenderer& renderer();
    void initRenderer(int w, int h);
    void renderFrame(const Camera& camera, float deltaTime) noexcept;
    void shutdown() noexcept;
    void createSwapchain(VkInstance inst, VkPhysicalDevice phys, VkDevice dev, VkSurfaceKHR surf, uint32_t w, uint32_t h);
    void recreateSwapchain(uint32_t w, uint32_t h) noexcept;
    void buildBLAS(uint64_t vertexBuf, uint64_t indexBuf, uint32_t vertexCount, uint32_t indexCount) noexcept;
    void buildTLAS(const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances) noexcept;
    void cleanupAll() noexcept;
    void createGlobalRenderPass();

    void stonekey_xor_spirv(std::vector<uint32_t>& data, bool encrypt = true);

    [[nodiscard]] inline bool isDeviceExtensionPresent(VkPhysicalDevice phys, const char* name) noexcept {
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(phys, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> exts(count);
        vkEnumerateDeviceExtensionProperties(phys, nullptr, &count, exts.data());
        return std::any_of(exts.begin(), exts.end(), [name](const auto& e) { return strcmp(e.extensionName, name) == 0; });
    }
}

// =============================================================================
// 28 SACRED EXTENSIONS
// =============================================================================
static constexpr std::array<const char*, 28> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
    VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_MAINTENANCE3_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
    VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
    VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME,
    VK_KHR_SHADER_CLOCK_EXTENSION_NAME,
    VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
    VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME,
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
    VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
    VK_KHR_ZERO_INITIALIZE_WORKGROUP_MEMORY_EXTENSION_NAME
};

// =============================================================================
// RAY TRACING LOADER
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
            if (!pfn) {
                LOG_FATAL_CAT("RTX", "FAILED TO LOAD KHR FUNCTION: {} — DRIVER TOO WEAK", name);
                std::unreachable();
            }
        };
        load(vkGetAccelerationStructureDeviceAddressKHR, "vkGetAccelerationStructureDeviceAddressKHR");
        load(vkCreateAccelerationStructureKHR,           "vkCreateAccelerationStructureKHR");
        load(vkDestroyAccelerationStructureKHR,          "vkDestroyAccelerationStructureKHR");
        load(vkCmdBuildAccelerationStructuresKHR,        "vkCmdBuildAccelerationStructuresKHR");
        load(vkGetAccelerationStructureBuildSizesKHR,    "vkGetAccelerationStructureBuildSizesKHR");
        LOG_SUCCESS_CAT("RTX", "KHR_acceleration_structure EXTENSIONS LOADED — FULL MANUAL CONTROL ACHIEVED");
    }
}

// =============================================================================
// FINAL WORD FROM CID:
// "There is only one header. There is only one truth.
// HANDLE_GET lives here. Forever.
// The empire is sealed."
// PINK PHOTONS ETERNAL — NOVEMBER 25, 2025 — FIRST LIGHT ACHIEVED
// =============================================================================