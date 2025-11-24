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

// =============================================================================
// PINK PHOTON LITERALS — MUST BE BEFORE ANYTHING USES _MB/_GB
// =============================================================================
constexpr uint64_t operator""_KB(unsigned long long v) noexcept { return v * 1024ULL; }
constexpr uint64_t operator""_MB(unsigned long long v) noexcept { return v * 1024ULL * 1024ULL; }
constexpr uint64_t operator""_GB(unsigned long long v) noexcept { return v * 1024ULL * 1024ULL * 1024ULL; }

// =============================================================================
// FINAL FIX: Only specialize for Vulkan opaque handles — NEVER conflict with std
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
} // namespace RTX

using namespace Logging::Color;
using namespace StoneKey;

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

namespace RTX {
    struct Context;
    void recreateSwapchain(uint32_t w, uint32_t h) noexcept;
    void forgeSwapchain(SDL_Window* window, int width, int height) noexcept;
}

using namespace Logging::Color;

// =============================================================================
// ULTRA LOW LEVEL BUFFER TRACKER — FINAL ETERNAL STONE v∞ — DECLARED FIRST
// =============================================================================

struct UltraLowLevelBufferTracker {
    struct BufferData {
        VkBuffer       buffer  = VK_NULL_HANDLE;
        VkDeviceMemory memory  = VK_NULL_HANDLE;
        VkDeviceSize   size    = 0;
        VkDeviceSize   aligned = 0;
        VkBufferUsageFlags usage = 0;
        std::string    tag;
    };

    [[nodiscard]] static UltraLowLevelBufferTracker& get() noexcept;

    // Called once after device creation
    static void initialize(VkDevice dev, VkPhysicalDevice phys) noexcept;

    [[nodiscard]] uint64_t create(VkDeviceSize size,
                                  VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags props,
                                  std::string_view tag = "");

    void     destroy(uint64_t handle) noexcept;
    void*    map(uint64_t handle) noexcept;
    void     unmap(uint64_t handle) noexcept;
    void     purge_all() noexcept;

    [[nodiscard]] BufferData*       getData(uint64_t handle) noexcept;
    [[nodiscard]] const BufferData* getData(uint64_t handle) const noexcept;

    // PUBLIC — needed by old code
    static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props) noexcept;

    // Convenience
    [[nodiscard]] uint64_t make_64M (VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    [[nodiscard]] uint64_t make_128M(VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    [[nodiscard]] uint64_t make_256M(VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    [[nodiscard]] uint64_t make_420M(VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    [[nodiscard]] uint64_t make_512M(VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    [[nodiscard]] uint64_t make_1G  (VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    [[nodiscard]] uint64_t make_2G  (VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    [[nodiscard]] uint64_t make_4G  (VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    [[nodiscard]] uint64_t make_8G  (VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;

private:
    static inline std::atomic<uint64_t> counter_{1};
    static inline std::mutex            mutex_;
    static inline std::unordered_map<uint64_t, BufferData> vault_;
    static inline VkDevice              device_ = VK_NULL_HANDLE;
    static inline VkPhysicalDevice      physicalDevice_ = VK_NULL_HANDLE;
};

// =============================================================================
// THE SACRED MACROS — NOW THEY SEE THE STRUCT — NO MORE "does not name a type"
// =============================================================================


[[nodiscard]] inline UltraLowLevelBufferTracker& g_bufferTracker() noexcept {
    return UltraLowLevelBufferTracker::get();


#define BUFFER_CREATE(handle, size, usage, props, ...) \
    handle = g_bufferTracker().create(size, usage, props, ##__VA_ARGS__)

#define BUFFER_DESTROY(handle) do { \
    if (handle) { g_bufferTracker().destroy(handle); handle = 0; } \
} while(0)

#define RAW_BUFFER(handle) \
    (g_bufferTracker().getData(handle) ? g_bufferTracker().getData(handle)->buffer : VK_NULL_HANDLE)

#define BUFFER_MEMORY(handle) \
    (g_bufferTracker().getData(handle) ? g_bufferTracker().getData(handle)->memory : VK_NULL_HANDLE)

#define BUFFER_MAP(handle, out_ptr) \
    out_ptr = g_bufferTracker().map(handle)

#define BUFFER_UNMAP(handle) \
    g_bufferTracker().unmap(handle)

} // namespace RTX

// =============================================================================
// NAMESPACE RTX
// =============================================================================
namespace RTX {
    // =============================================================================
    // FIXED: SDL3 2024+ — CREATE INSTANCE + OVERLOAD for initContext
    // =============================================================================
    [[nodiscard]] VkInstance createVulkanInstanceWithSDL(bool enableValidation);  // UPDATED: Added SDL_Window* window

    // =============================================================================
    // Helpers (declarations only) — MOVED UP FOR TEMPLATE VISIBILITY
    // =============================================================================
    void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size);

    // Internal sub-functions for stepwise initialization (declared here for modularity; defined in RTXHandler.cpp)
    [[nodiscard]] VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    [[nodiscard]] VkDevice         createLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    void createCommandPool();
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
    // ========================================================================
    // CORE HANDLES — THE HEART OF THE EMPIRE
    // ========================================================================
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

    // ========================================================================
    // QUEUE FAMILIES — THE BLOODLINES OF POWER
    // ========================================================================
    std::optional<uint32_t> graphicsFamily_;
    std::optional<uint32_t> presentFamily_;
    std::optional<uint32_t> computeFamily_;
    std::optional<uint32_t> transferFamily_;

    // ========================================================================
    // RTX ASCENSION — THE HOLY TRINITY OF FIRST LIGHT
    // ========================================================================
    bool bufferDeviceAddressEnabled_         = false;
    bool bufferDeviceAddressExtensionPresent_ = false;
    bool accelerationStructureEnabled_       = false;
    bool rayTracingPipelineEnabled_          = false;
    bool rayQueryEnabled_                    = false;
    bool meshShadingEnabled_                 = false;

    // ========================================================================
    // PHYSICAL DEVICE SCROLLS — WISDOM OF THE ANCIENTS
    // ========================================================================
    VkPhysicalDeviceProperties                        physicalDeviceProperties_{};
    VkPhysicalDeviceFeatures                          physicalDeviceFeatures_{};
    VkPhysicalDeviceMemoryProperties                  physicalDeviceMemoryProperties_{};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR   rayTracingProps_{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    VkFormat         hdr_format       = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR  hdr_color_space  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    // ========================================================================
    // CUSTOM HANDLES — THE CROWN JEWELS
    // ========================================================================
    Handle<VkImageView>     blueNoiseView_;
    Handle<VkRenderPass>    renderPass_;
    uint64_t                sharedStagingEnc_ = 0;

    // ========================================================================
    // STATE OF THE EMPIRE
    // ========================================================================
    SDL_Window* window  = nullptr;
    int         width  = 0;
    int         height = 0;
    bool        valid_ = false;
    mutable std::atomic<bool> ready_{false};

    // ========================================================================
    // C++23 PURE ACCESSORS — THE ONE TRUE WAY — NO MACROS, NO WEAKNESS
    // ========================================================================
    [[nodiscard]] constexpr VkInstance       instance()       const noexcept { return instance_; }
    [[nodiscard]] constexpr VkSurfaceKHR     surface()        const noexcept { return surface_; }
    [[nodiscard]] constexpr VkPhysicalDevice physicalDevice() const noexcept { return physicalDevice_; }
    [[nodiscard]] constexpr VkDevice         device()         const noexcept { return device_; }

    [[nodiscard]] constexpr VkQueue graphicsQueue()  const noexcept { return graphicsQueue_; }
    [[nodiscard]] constexpr VkQueue presentQueue()   const noexcept { return presentQueue_; }
    [[nodiscard]] constexpr VkQueue computeQueue()   const noexcept { return computeQueue_; }
    [[nodiscard]] constexpr VkQueue transferQueue()  const noexcept { return transferQueue_; }

    [[nodiscard]] constexpr VkCommandPool commandPool()        const noexcept { return commandPool_; }
    [[nodiscard]] constexpr VkCommandPool computeCommandPool() const noexcept { return computeCommandPool_; }
    [[nodiscard]] constexpr VkCommandPool transferCommandPool()const noexcept { return transferCommandPool_; }

    [[nodiscard]] constexpr uint32_t graphicsFamily() const noexcept { return graphicsFamily_.value(); }
    [[nodiscard]] constexpr uint32_t presentFamily()  const noexcept { return presentFamily_.value(); }
    [[nodiscard]] constexpr uint32_t computeFamily()  const noexcept { return computeFamily_.value_or(graphicsFamily()); }
    [[nodiscard]] constexpr uint32_t transferFamily() const noexcept { return transferFamily_.value_or(graphicsFamily()); }

    [[nodiscard]] constexpr VkRenderPass renderPass() const noexcept 
    { return renderPass_.valid() ? renderPass_.get() : VK_NULL_HANDLE; }

    [[nodiscard]] constexpr const auto& rayTracingProps() const noexcept { return rayTracingProps_; }
    [[nodiscard]] constexpr bool hdrEnabled()            const noexcept { return hdr_format != VK_FORMAT_UNDEFINED; }
    [[nodiscard]] constexpr VkFormat hdrFormat()         const noexcept { return hdr_format; }
    [[nodiscard]] constexpr VkColorSpaceKHR hdrColorSpace() const noexcept { return hdr_color_space; }

    // ========================================================================
    // RTX ASCENSION STATUS — THE FINAL TRUTH
    // ========================================================================
    [[nodiscard]] constexpr bool hasFullRTX()        const noexcept { return accelerationStructureEnabled_ && rayTracingPipelineEnabled_; }
    [[nodiscard]] constexpr bool hasRayQuery()       const noexcept { return rayQueryEnabled_; }
    [[nodiscard]] constexpr bool hasMeshShading()    const noexcept { return meshShadingEnabled_; }

    [[nodiscard]] constexpr bool bufferDeviceAddressEnabled()        const noexcept { return bufferDeviceAddressEnabled_; }
    [[nodiscard]] constexpr bool bufferDeviceAddressExtPresent()     const noexcept { return bufferDeviceAddressExtensionPresent_; }
    [[nodiscard]] constexpr bool accelerationStructureEnabled()     const noexcept { return accelerationStructureEnabled_; }
    [[nodiscard]] constexpr bool rayTracingPipelineEnabled()         const noexcept { return rayTracingPipelineEnabled_; }

    // ========================================================================
    // THE MAGIC SCROLL — KNOWLEDGE IS POWER
    // ========================================================================
    [[nodiscard]] constexpr const char* deviceName() const noexcept { return physicalDeviceProperties_.deviceName; }

    [[nodiscard]] float vramGB() const noexcept 
    {
        for (uint32_t i = 0; i < physicalDeviceMemoryProperties_.memoryHeapCount; ++i) {
            if (physicalDeviceMemoryProperties_.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                return static_cast<float>(physicalDeviceMemoryProperties_.memoryHeaps[i].size) / (1024.f * 1024.f * 1024.f);
            }
        }
        return 0.0f;
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
            default:     return std::format("Vendor 0x{:04X}", physicalDeviceProperties_.vendorID);
        }
    }

    // ========================================================================
    // STATE OF THE EMPIRE — READINESS IS ALL
    // ========================================================================
    [[nodiscard]] constexpr bool isValid() const noexcept { return valid_; }
    [[nodiscard]] constexpr bool isReady() const noexcept { return ready_.load(std::memory_order_acquire); }
    constexpr void markReady() noexcept { ready_.store(true, std::memory_order_release); }

    // ========================================================================
    // LIFECYCLE — THE RITUALS OF CREATION AND DISSOLUTION
    // ========================================================================
    void createLogicalDevice();
    void init(SDL_Window* window, int width, int height);
    void cleanup() noexcept;

    // ========================================================================
    // INTERNAL SETTERS — FOR THE FORGE ONLY
    // ========================================================================
    void setInstance(VkInstance i)       noexcept { instance_ = i; }
    void setSurface(VkSurfaceKHR s)      noexcept { surface_ = s; }
    void setPhysicalDevice(VkPhysicalDevice pd) noexcept { physicalDevice_ = pd; }
    void setDevice(VkDevice d)           noexcept { device_ = d; }

    void setGraphicsQueue(VkQueue q)     noexcept { graphicsQueue_ = q; }
    void setPresentQueue(VkQueue q)      noexcept { presentQueue_ = q; }
    void setComputeQueue(VkQueue q)      noexcept { computeQueue_ = q; }
    void setTransferQueue(VkQueue q)     noexcept { transferQueue_ = q; }

    void enableBufferDeviceAddress(bool enabled = true)     noexcept { bufferDeviceAddressEnabled_ = enabled; }
    void enableAccelerationStructure(bool enabled = true)  noexcept { accelerationStructureEnabled_ = enabled; }
    void enableRayTracingPipeline(bool enabled = true)     noexcept { rayTracingPipelineEnabled_ = enabled; }
    void enableRayQuery(bool enabled = true)                noexcept { rayQueryEnabled_ = enabled; }
    void enableMeshShading(bool enabled = true)             noexcept { meshShadingEnabled_ = enabled; }
};

    // =============================================================================
    // GLOBAL ACCESSORS — FIXED: ctx() == g_ctx() + NULL GUARD
    // =============================================================================
    // Global context instance declaration
	extern Context g_context_instance;

// =============================================================================
// GLOBAL stone_swapchain() + LAS
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

[[nodiscard]] inline RTX::Context& g_ctx() noexcept { return RTX::g_context_instance; }
// ──────────────────────────────────────────────────────────────────────────────
// RAY TRACING FUNCTION LOADER — THE ONE TRUE NAMESPACE
// ──────────────────────────────────────────────────────────────────────────────
namespace RTX::RayTracingFunctions {

    // THE SACRED PFNS — INLINE, ODR-SAFE, C++23 INITIALIZED
    inline PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    inline PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR           = nullptr;
    inline PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR          = nullptr;
    inline PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR        = nullptr;
    inline PFN_vkGetAccelerationStructureBuildSizesKHR    vkGetAccelerationStructureBuildSizesKHR    = nullptr;

    // THE ONLY CORRECT, BULLETPROOF, `-fpermissive`-FREE LOADER IN EXISTENCE
    inline void loadRayTracingExtensions(VkDevice device) noexcept
    {
        if (vkGetAccelerationStructureDeviceAddressKHR) [[unlikely]]
            return; // already loaded — ZUUL remembers

        // THIS IS THE CORRECT WAY — ACCEPTED BY EVERY COMPILER SINCE 2015
        const auto load = [device]<typename Fn>(Fn& pfn, const char* name) -> void {
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
        LOG_SUCCESS_CAT("RTX", "PINK PHOTONS ARMED — FIRST LIGHT IMMINENT — NOVEMBER 24, 2025");
    }
}
// =============================================================================
// END OF FILE — THE EMPIRE IS SEALED
// =============================================================================