// include/engine/GLOBAL/RTXHandler.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RTXHandler.hpp — HEADER-ONLY DECLARATIONS + SAFE CONTEXT INIT
// • NO INLINE IMPLEMENTATIONS (moved to .cpp)
// • VULKAN 1.4 READY: Core promotions, no broken PFNs, pure AAAAA glory
// • Pink photons locked and loaded — stutter-free, leak-proof, HDR supreme
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
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
#include "engine/GLOBAL/StoneKey.hpp"  // For secure handle accessors (g_device, g_instance, etc.)

// Forward declarations
class VulkanRTX;
class VulkanRenderer;
struct Camera;

using namespace Logging::Color;

// Forward-declare StoneKey funcs (no include needed—defined in main.cpp TU)
extern uint64_t kObfuscator() noexcept;

inline const char* getPlatformSurfaceExtension()
{
#if defined(__linux__)
    return VK_KHR_SURFACE_EXTENSION_NAME;     // Most Linux
    // return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME; // Uncomment if using Wayland
#elif defined(_WIN32)
    return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#elif defined(__APPLE__)
    return VK_EXT_METAL_SURFACE_EXTENSION_NAME;
#else
    return VK_KHR_XCB_SURFACE_EXTENSION_NAME;
#endif
}

extern const char* extra_extensions[];

// -----------------------------------------------------------------------------
// User-defined literals
// -----------------------------------------------------------------------------
constexpr uint64_t operator"" _KB(unsigned long long v) noexcept { return v << 10; }
constexpr uint64_t operator"" _MB(unsigned long long v) noexcept { return v << 20; }
constexpr uint64_t operator"" _GB(unsigned long long v) noexcept { return v << 30; }
constexpr uint64_t operator"" _TB(unsigned long long v) noexcept { return v << 40; }

// =============================================================================
// OFFICIAL RTX BUFFER MACROS — THE ONE TRUE SOURCE — EMPIRE-WIDE 2025 EDITION
// FULLY VALIDATION-SAFE | RTX-AWARE | STONEKEY v∞ COMPLIANT | NO VUID-09499 EVER
// FIRST LIGHT ACHIEVED — NOVEMBER 20, 2025 — SHE IS PLEASED
// =============================================================================

#define BUFFER(handle) uint64_t handle = 0ULL


[[nodiscard]] inline std::string getDeviceName(VkPhysicalDevice dev)
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(dev, &props);
    return std::string(props.deviceName);
}

// ─────────────────────────────────────────────────────────────────────────────
// BUFFER_CREATE — THE SACRED ONE (AUTOMATICALLY STRIPS RTX FLAGS WHEN UNSAFE)
// ─────────────────────────────────────────────────────────────────────────────
#define BUFFER_CREATE(handle, size, usage, props, tag)                          \
    do {                                                                        \
        LOG_INFO_CAT("RTX", "BUFFER_CREATE: {} | Size: {} bytes | Tag: {}",     \
                     #handle, (VkDeviceSize)(size), (tag));                     \
                                                                                \
        VkBufferUsageFlags safeUsage = (usage);                                 \
                                                                                \
        /* STRIP RTX-ONLY FLAGS IF FULL RTX IS NOT ENABLED (validation active) */ \
        if (!g_ctx().hasFullRTX()) {                                       \
            safeUsage &= ~(                                                     \
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT                  |   \
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | \
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR     |   \
                VK_BUFFER_USAGE_RAY_TRACING_BIT_NV                                 \
            );                                                                  \
            LOG_DEBUG_CAT("RTX", "Validation active → RTX flags stripped from buffer '{}'", (tag)); \
        }                                                                       \
                                                                                \
        (handle) = RTX::UltraLowLevelBufferTracker::get().create(               \
            (VkDeviceSize)(size), safeUsage, (props), (tag));                   \
                                                                                \
        LOG_DEBUG_CAT("RTX", "Buffer forged: obf=0x{:x} | Size: {} | Tag: {}",   \
                      (handle), (VkDeviceSize)(size), (tag));                   \
    } while (0)

// ─────────────────────────────────────────────────────────────────────────────
// RAW ACCESS — UNCHANGED, PURE, LIGHTNING FAST
// ─────────────────────────────────────────────────────────────────────────────
#define RAW_BUFFER(handle)                                                      \
    ((handle) != 0ULL                                                           \
        ? (RTX::UltraLowLevelBufferTracker::get().getData((handle))            \
            ? RTX::UltraLowLevelBufferTracker::get().getData((handle))->buffer \
            : VK_NULL_HANDLE)                                                   \
        : VK_NULL_HANDLE)

#define BUFFER_MEMORY(handle)                                                   \
    ((handle) != 0ULL                                                           \
        ? (RTX::UltraLowLevelBufferTracker::get().getData((handle))            \
            ? RTX::UltraLowLevelBufferTracker::get().getData((handle))->memory \
            : VK_NULL_HANDLE)                                                   \
        : VK_NULL_HANDLE)

// ─────────────────────────────────────────────────────────────────────────────
// MAPPING — SAFE AND ELEGANT
// ─────────────────────────────────────────────────────────────────────────────
#define BUFFER_MAP(handle, mapped)                                              \
    do {                                                                        \
        if ((handle) != 0ULL) {                                                 \
            (mapped) = RTX::UltraLowLevelBufferTracker::get().map(handle);     \
        } else {                                                                \
            (mapped) = nullptr;                                                 \
        }                                                                       \
    } while (0)

#define BUFFER_UNMAP(handle)                                                    \
    do { if ((handle) != 0ULL) RTX::UltraLowLevelBufferTracker::get().unmap(handle); } while (0)

// ─────────────────────────────────────────────────────────────────────────────
// DESTRUCTION — WITH FULL LOGGING AND STONEKEY RITUAL
// ─────────────────────────────────────────────────────────────────────────────
#define BUFFER_DESTROY(handle)                                                  \
    do {                                                                        \
        if ((handle) != 0ULL) {                                                 \
            auto* data = RTX::UltraLowLevelBufferTracker::get().getData(handle); \
            const char* tagStr = data ? data->tag.c_str() : "unknown";          \
            LOG_INFO_CAT("RTX", "BUFFER_DESTROY: obf=0x{:x} | Tag: {}",         \
                         (handle), tagStr);                                     \
            RTX::UltraLowLevelBufferTracker::get().destroy(handle);            \
            (handle) = 0ULL;                                                    \
        }                                                                       \
    } while (0)

// =============================================================================
// NAMESPACE RTX
// =============================================================================
namespace RTX {
    // =============================================================================
    // FIXED: SDL3 2024+ — CREATE INSTANCE + OVERLOAD for initContext
    // =============================================================================
    [[nodiscard]] VkInstance createVulkanInstanceWithSDL(bool enableValidation);  // UPDATED: Added SDL_Window* window
    void initContext(VkInstance instance, SDL_Window* window, int width, int height);

    // =============================================================================
    // Helpers (declarations only) — MOVED UP FOR TEMPLATE VISIBILITY
    // =============================================================================
    void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size);

    // Internal sub-functions for stepwise initialization (declared here for modularity; defined in RTXHandler.cpp)
    [[nodiscard]] VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    [[nodiscard]] VkDevice         createLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    void createCommandPool();
    void loadRayTracingExtensions();
    void retrieveQueues() noexcept;
	
    // =============================================================================
    // Handle<T> — FIXED: FULL INLINE IMPLEMENTATIONS FOR TEMPLATE
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

        Handle& operator=(std::nullptr_t) noexcept {
            reset();
            return *this;
        }

        explicit operator bool() const noexcept { return valid(); }

        T get() const noexcept { return raw; }

        T operator*() const noexcept { return raw; }

        [[nodiscard]] bool valid() const noexcept {
            return raw != T{} && device != VK_NULL_HANDLE;
        }

        void reset() noexcept {
            if (valid()) {
                LOG_INFO_CAT("RTX", "Handle reset: {} @ 0x{:x} | Tag: {}", 
                             typeid(T).name(), reinterpret_cast<uint64_t>(raw), tag);
                if (destroyer && device) {
                    // CRITICAL: Destroy FIRST with original raw handle
                    destroyer(device, raw, nullptr);
                    
                    // THEN shred the local raw value if not too large (simple poison, StoneKey protects in-flight)
                    constexpr size_t threshold = 16 * 1024 * 1024;
                    if (size >= threshold) {
                        LOG_DEBUG_CAT("RTX", "Skipping shred for large allocation ({}MB): {}", 
                                      size / (1024 * 1024), tag.empty() ? "" : tag.c_str());
                    } else {
                        std::memset(&raw, 0xCD, sizeof(T));
                    }
                }
                logAndTrackDestruction(tag.empty() ? typeid(T).name() : tag.c_str(), 
                                       reinterpret_cast<void*>(raw), __LINE__, size);
                raw = T{}; device = VK_NULL_HANDLE; destroyer = nullptr; size = 0;
            }
        }

        ~Handle() {
            reset();
        }
    };

    template<typename T, typename... Args>
    [[nodiscard]] auto MakeHandle(T h, VkDevice d, Args&&... args) {
        using H = Handle<T>;
        return H(h, d, std::forward<Args>(args)...);
    }

    // =============================================================================
    // MACROS
    // =============================================================================
    #define HANDLE_CREATE(var, raw, dev, destroyer, size, tag) \
        do { LOG_INFO_CAT("RTX", "HANDLE_CREATE: {} | Tag: {}", #var, tag); (var) = RTX::MakeHandle((raw), (dev), (destroyer), (size), (tag)); } while(0)
    #define HANDLE_GET(var) ((var).get())
    #define HANDLE_RESET(var) do { LOG_INFO_CAT("RTX", "HANDLE_RESET: {}", #var); (var).reset(); } while(0)

// =============================================================================
// Context — Async Compute + Ready Flag + Full Cleanup + Safe Accessors + renderPass + physProps for alignment
// =============================================================================
// =============================================================================
// Context — C++23 EDITION — PURE STANDARD LIBRARY — NO FMT — FIRST LIGHT 2025
// =============================================================================
struct Context {
public:
    // CORE HANDLES
    VkInstance       instance_       = VK_NULL_HANDLE;
    VkSurfaceKHR     surface_        = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice         device_         = VK_NULL_HANDLE;

    VkQueue          graphicsQueue_  = VK_NULL_HANDLE;
    VkQueue          presentQueue_   = VK_NULL_HANDLE;
    VkQueue          computeQueue_   = VK_NULL_HANDLE;
    VkQueue          transferQueue_  = VK_NULL_HANDLE;

    VkCommandPool    commandPool_          = VK_NULL_HANDLE;
    VkCommandPool    computeCommandPool_   = VK_NULL_HANDLE;
    VkCommandPool    transferCommandPool_  = VK_NULL_HANDLE;
    VkPipelineCache  pipelineCache_        = VK_NULL_HANDLE;

    // QUEUE FAMILIES
    std::optional<uint32_t> graphicsFamily_;
    std::optional<uint32_t> presentFamily_;
    std::optional<uint32_t> computeFamily_;
    std::optional<uint32_t> transferFamily_;

	bool bufferDeviceAddressEnabled_ = false;          // ← THE ONE TRUE FLAG
    bool bufferDeviceAddressExtensionPresent_ = false; // For diagnostics

    // PHYSICAL DEVICE PROPERTIES — THE MAGIC SCROLL'S HOLY TRINITY
    VkPhysicalDeviceProperties               physicalDeviceProperties_{};
    VkPhysicalDeviceFeatures                 physicalDeviceFeatures_{};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProps_{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    VkFormat         hdr_format      = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR  hdr_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties_{};

    // RTX CAPABILITIES
    bool hasFullRTX_    = false;
    bool hasRayQuery_   = false;
    bool hasMeshShading_= false;

    // WINDOW & STATE
    SDL_Window* window = nullptr;
    int         width  = 0;
    int         height = 0;
    bool        valid_ = false;
    mutable std::atomic<bool> ready_{false};


    // CUSTOM HANDLES
    Handle<VkImageView>  blueNoiseView_;
    Handle<VkRenderPass> renderPass_;
    uint64_t             sharedStagingEnc_ = 0;

    // =============================================================================
    // C++23 PURE ACCESSORS — NO FMT, NO COMPROMISE, ONLY BEAUTY
    // =============================================================================
    [[nodiscard]] constexpr bool hdrEnabled() const noexcept { return hdr_format != VK_FORMAT_UNDEFINED; }
    [[nodiscard]] constexpr VkFormat hdrFormat() const noexcept { return hdr_format; }
    [[nodiscard]] constexpr VkColorSpaceKHR hdrColorSpace() const noexcept { return hdr_color_space; }
	[[nodiscard]] constexpr VkCommandPool commandPool() const noexcept { return commandPool_; }
	[[nodiscard]] constexpr const auto& rayTracingProps() const noexcept { return rayTracingProps_; }

    [[nodiscard]] constexpr uint32_t graphicsFamily() const noexcept { return graphicsFamily_.value(); }
    [[nodiscard]] constexpr uint32_t presentFamily()  const noexcept { return presentFamily_.value(); }
    [[nodiscard]] constexpr uint32_t computeFamily()  const noexcept { return computeFamily_.value_or(graphicsFamily()); }
    [[nodiscard]] constexpr uint32_t transferFamily() const noexcept { return transferFamily_.value_or(graphicsFamily()); }

    [[nodiscard]] constexpr VkQueue graphicsQueue() const noexcept { return graphicsQueue_; }
    [[nodiscard]] constexpr VkQueue presentQueue()  const noexcept { return presentQueue_; }
    [[nodiscard]] constexpr VkQueue computeQueue()  const noexcept { return computeQueue_; }
    [[nodiscard]] constexpr VkQueue transferQueue() const noexcept { return transferQueue_; }

    [[nodiscard]] constexpr VkDevice device() const noexcept { return device_; }
    [[nodiscard]] constexpr VkPhysicalDevice physicalDevice() const noexcept { return physicalDevice_; }

    [[nodiscard]] constexpr VkRenderPass renderPass() const noexcept 
    { return renderPass_.valid() ? renderPass_.get() : VK_NULL_HANDLE; }

    // =============================================================================
    // THE MAGIC SCROLL — C++23 std::format ONLY — PURE, CLEAN, ETERNAL
    // =============================================================================
    [[nodiscard]] constexpr const char* deviceName() const noexcept 
    { return physicalDeviceProperties_.deviceName; }

    [[nodiscard]] float vramGB() const noexcept 
    {
        for (uint32_t i = 0; i < physicalDeviceMemoryProperties_.memoryHeapCount; ++i) {
            if (physicalDeviceMemoryProperties_.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                return static_cast<float>(physicalDeviceMemoryProperties_.memoryHeaps[i].size) / (1024.0f * 1024.0f * 1024.0f);
            }
        }
        return 0.0f;
    }

    [[nodiscard]] constexpr bool bufferDeviceAddressEnabled() const noexcept 
    { 
        return bufferDeviceAddressEnabled_; 
    }

    [[nodiscard]] constexpr bool bufferDeviceAddressExtPresent() const noexcept 
    { 
        return bufferDeviceAddressExtensionPresent_; 
    }

    [[nodiscard]] std::string vendorName() const 
    {
        switch (physicalDeviceProperties_.vendorID) {
            case 0x10DE: return "NVIDIA";
            case 0x1002: return "AMD";
            case 0x8086: return "Intel";
            case 0x13B5: return "ARM";
            case 0x5143: return "Qualcomm";
            case 0x1AE0: return "Google";
            default:     
                return std::format("Vendor 0x{:04X}", physicalDeviceProperties_.vendorID);
        }
    }

    // =============================================================================
    // EXTENSION PFNS — STILL NEEDED UNTIL VULKAN 1.5
    // =============================================================================
    PFN_vkCmdTraceRaysKHR                    vkCmdTraceRaysKHR_                    = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR       vkCreateRayTracingPipelinesKHR_       = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR_ = nullptr;
    PFN_vkCreateAccelerationStructureKHR     vkCreateAccelerationStructureKHR_     = nullptr;
    PFN_vkDestroyAccelerationStructureKHR    vkDestroyAccelerationStructureKHR_    = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR_ = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR  vkCmdBuildAccelerationStructuresKHR_  = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR_ = nullptr;
    PFN_vkCmdDrawMeshTasksEXT                vkCmdDrawMeshTasksEXT_                = nullptr;
    PFN_vkCmdPushDescriptorSetKHR            vkCmdPushDescriptorSetKHR_            = nullptr;

    // PFN ACCESSORS
    [[nodiscard]] constexpr auto vkCmdTraceRaysKHR() const noexcept                    { return vkCmdTraceRaysKHR_; }
    [[nodiscard]] constexpr auto vkCreateRayTracingPipelinesKHR() const noexcept       { return vkCreateRayTracingPipelinesKHR_; }
    [[nodiscard]] constexpr auto vkGetRayTracingShaderGroupHandlesKHR() const noexcept { return vkGetRayTracingShaderGroupHandlesKHR_; }
    [[nodiscard]] constexpr auto vkCreateAccelerationStructureKHR() const noexcept     { return vkCreateAccelerationStructureKHR_; }
    [[nodiscard]] constexpr auto vkDestroyAccelerationStructureKHR() const noexcept    { return vkDestroyAccelerationStructureKHR_; }
    [[nodiscard]] constexpr auto vkGetAccelerationStructureBuildSizesKHR() const noexcept { return vkGetAccelerationStructureBuildSizesKHR_; }
    [[nodiscard]] constexpr auto vkCmdBuildAccelerationStructuresKHR() const noexcept  { return vkCmdBuildAccelerationStructuresKHR_; }
    [[nodiscard]] constexpr auto vkGetAccelerationStructureDeviceAddressKHR() const noexcept { return vkGetAccelerationStructureDeviceAddressKHR_; }
    [[nodiscard]] constexpr auto vkCmdDrawMeshTasksEXT() const noexcept                { return vkCmdDrawMeshTasksEXT_; }
    [[nodiscard]] constexpr auto vkCmdPushDescriptorSetKHR() const noexcept            { return vkCmdPushDescriptorSetKHR_; }

    // =============================================================================
    // LIFECYCLE
    // =============================================================================
    void createLogicalDevice();
    void init(SDL_Window* window, int width, int height);
    void forgeSwapchain(SDL_Window* window, int width, int height);
    void cleanup() noexcept;

    [[nodiscard]] constexpr bool isValid() const noexcept { return valid_; }
    [[nodiscard]] constexpr bool isReady() const noexcept { return ready_.load(); }
    constexpr void markReady() noexcept { ready_.store(true); }
    [[nodiscard]] constexpr bool hasFullRTX() const noexcept { return hasFullRTX_; }
    [[nodiscard]] constexpr bool hasRayQuery() const noexcept { return hasRayQuery_; }
};

    // =============================================================================
    // GLOBAL ACCESSORS — FIXED: ctx() == g_ctx() + NULL GUARD
    // =============================================================================
    // Global context instance declaration
	extern Context g_context_instance;

    // =============================================================================
    // UltraLowLevelBufferTracker
    // =============================================================================
    struct BufferData {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;  // Original requested size
        VkDeviceSize alignedSize = 0;  // Aligned allocation size (NEW)
        VkBufferUsageFlags usage = 0;
        std::string tag;
    };

    constexpr VkDeviceSize SIZE_64MB  =  64_MB;
    constexpr VkDeviceSize SIZE_128MB = 128_MB;
    constexpr VkDeviceSize SIZE_256MB = 256_MB;
    constexpr VkDeviceSize SIZE_420MB = 420_MB;
    constexpr VkDeviceSize SIZE_512MB = 512_MB;
    constexpr VkDeviceSize SIZE_1GB   =   1_GB;
    constexpr VkDeviceSize SIZE_2GB   =   2_GB;
    constexpr VkDeviceSize SIZE_4GB   =   4_GB;
    constexpr VkDeviceSize SIZE_8GB   =   8_GB;

    struct UltraLowLevelBufferTracker {
        static UltraLowLevelBufferTracker& get() noexcept;
        uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, std::string_view tag);  // FIXED: Removed noexcept — allows throws
        void destroy(uint64_t handle) noexcept;
        BufferData* getData(uint64_t handle) noexcept;
        const BufferData* getData(uint64_t handle) const noexcept;
        void* map(uint64_t handle) noexcept;
        void unmap(uint64_t handle) noexcept;
        void init(VkDevice dev, VkPhysicalDevice phys) noexcept;
        void purge_all() noexcept;
        uint64_t make_64M (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        uint64_t make_128M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        uint64_t make_256M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        uint64_t make_420M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        uint64_t make_512M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        uint64_t make_1G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        uint64_t make_2G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        uint64_t make_4G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        uint64_t make_8G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;

        // NEW: Alignment helper (use in .cpp create: VkDeviceSize align = g_ctx().getBufferAlignment(usage); aligned = ((size + align - 1)/align)*align;)
        // This ensures requested sizes like 96 bytes are padded to 256 (common minStorageBufferOffsetAlignment on RTX)

        // --- INLINE IMPLEMENTATION OF findMemoryType ---
        static uint32_t findMemoryType(VkPhysicalDevice physDev, uint32_t typeFilter, VkMemoryPropertyFlags props) noexcept {
            VkPhysicalDeviceMemoryProperties memProps;
            vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);
            for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
                if ((typeFilter & (1 << i)) &&
                    (memProps.memoryTypes[i].propertyFlags & props) == props) {
                    return i;
                }
            }
            return UINT32_MAX;
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<uint64_t, BufferData> map_;
        std::atomic<uint64_t> counter_{0};
        VkDevice device_{VK_NULL_HANDLE};
        VkPhysicalDevice physDev_{VK_NULL_HANDLE};
        uint64_t obfuscate(uint64_t raw) const noexcept;
        uint64_t deobfuscate(uint64_t obf) const noexcept;
    };

    // =============================================================================
    // GLOBAL g_swapchain() + LAS
    // =============================================================================
    Handle<VkSwapchainKHR>& swapchain();
    std::vector<VkImage>& swapchainImages();
    std::vector<Handle<VkImageView>>& swapchainImageViews();
    VkFormat& swapchainFormat();
    VkExtent2D& swapchainExtent();
    Handle<VkAccelerationStructureKHR>& blas();
    Handle<VkAccelerationStructureKHR>& tlas();
    Handle<VkRenderPass>& renderPass();  // FIXED: Now returns ref to ctx.renderPass_

    // =============================================================================
    // RENDERER + FRAME
    // =============================================================================
    [[nodiscard]] VulkanRenderer& renderer();
    void initRenderer(int w, int h);
    void renderFrame(const Camera& camera, float deltaTime) noexcept;
    void shutdown() noexcept;
    void createSwapchain(VkInstance inst, VkPhysicalDevice phys, VkDevice dev, VkSurfaceKHR surf, uint32_t w, uint32_t h);
    void recreateSwapchain(uint32_t w, uint32_t h) noexcept;
    void buildBLAS(uint64_t vertexBuf, uint64_t indexBuf, uint32_t vertexCount, uint32_t indexCount) noexcept;
    void buildTLAS(const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances) noexcept;
    void cleanupAll() noexcept;
    void createGlobalRenderPass();  // FIXED: Added declaration

    // stonekey_xor_spirv → MOVED TO .cpp (uses Options::Shader::STONEKEY_1)
    void stonekey_xor_spirv(std::vector<uint32_t>& data, bool encrypt = true);


// Helper
[[nodiscard]] inline bool isDeviceExtensionPresent(VkPhysicalDevice phys, const char* name) noexcept {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(phys, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(phys, nullptr, &count, exts.data());
    for (const auto& e : exts)
        if (strcmp(e.extensionName, name) == 0)
            return true;
    return false;
}

} // namespace RTX

static constexpr std::array<const char*, 28> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,   // THIS ONE WAS MISSING — REQUIRED
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
// PINK PHOTONS ETERNAL — DOMINANCE ETERNAL
// =============================================================================
// =============================================================================
// GLOBAL g_ctx() ALIAS — LEGACY SUPPORT — WORKS EVERYWHERE — PINK PHOTONS ETERNAL
// =============================================================================
[[nodiscard]] inline RTX::Context& g_ctx() noexcept { return RTX::g_context_instance; }