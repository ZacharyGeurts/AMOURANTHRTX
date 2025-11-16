// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RTXHandler.hpp — HEADER-ONLY DECLARATIONS + SAFE CONTEXT INIT
// • NO INLINE IMPLEMENTATIONS (moved to .cpp)
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
extern uint64_t get_kHandleObfuscator() noexcept;

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

// -----------------------------------------------------------------------------
// User-defined literals
// -----------------------------------------------------------------------------
constexpr uint64_t operator"" _KB(unsigned long long v) noexcept { return v << 10; }
constexpr uint64_t operator"" _MB(unsigned long long v) noexcept { return v << 20; }
constexpr uint64_t operator"" _GB(unsigned long long v) noexcept { return v << 30; }
constexpr uint64_t operator"" _TB(unsigned long long v) noexcept { return v << 40; }

// =============================================================================
// NAMESPACE RTX
// =============================================================================
namespace RTX {
    // =============================================================================
    // FIXED: SDL3 2024+ — CREATE INSTANCE + OVERLOAD FOR initContext
    // =============================================================================
    [[nodiscard]] VkInstance createVulkanInstanceWithSDL(SDL_Window* window, bool enableValidation);  // UPDATED: Added SDL_Window* window
    void initContext(VkInstance instance, SDL_Window* window, int width, int height);

    // =============================================================================
    // Helpers (declarations only) — MOVED UP FOR TEMPLATE VISIBILITY
    // =============================================================================
    void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size);

    // Internal sub-functions for stepwise initialization (declared here for modularity; defined in RTXHandler.cpp)
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();
    void loadRayTracingExtensions();

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
    // Context — FINAL: Async Compute + Ready Flag + Full Cleanup + Safe Accessors + renderPass + physProps for alignment
    // =============================================================================
    struct Context {
    public:
        // Core Vulkan Handles (raw for local use; access via secure getters)
        VkInstance       instance_       = VK_NULL_HANDLE;
        VkSurfaceKHR     surface_        = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        VkDevice         device_         = VK_NULL_HANDLE;
        VkQueue          graphicsQueue_  = VK_NULL_HANDLE;
        VkQueue          presentQueue_   = VK_NULL_HANDLE;
        VkCommandPool    commandPool_    = VK_NULL_HANDLE;
        VkPipelineCache  pipelineCache_  = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
		VkFormat        hdr_format       = VK_FORMAT_UNDEFINED;
        VkColorSpaceKHR hdr_color_space  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        // Device Properties for Alignment (NEW: Fixes driver min size warnings)
        VkPhysicalDeviceProperties physProps_{};

        // Window and Dimensions
        SDL_Window*      window   = nullptr;
        int              width    = 0;
        int              height   = 0;

        // Validity and Readiness Flags
        bool             valid_   = false;  // For ctx() guard during init

        mutable std::atomic<bool> ready_{false};  // Thread-safe for renderer

        // Async Compute Support
        uint32_t         computeFamily_      = UINT32_MAX;
        VkQueue          computeQueue_       = VK_NULL_HANDLE;
        VkCommandPool    computeCommandPool_ = VK_NULL_HANDLE;

        uint32_t         graphicsFamily_    = UINT32_MAX;
        uint32_t         presentFamily_     = UINT32_MAX;

        // Ray Tracing Extensions (Function Pointers) — Public for direct access in LAS.hpp et al.
        PFN_vkGetBufferDeviceAddressKHR               vkGetBufferDeviceAddressKHR_               = nullptr;
        PFN_vkCmdTraceRaysKHR                         vkCmdTraceRaysKHR_                         = nullptr;
        PFN_vkGetRayTracingShaderGroupHandlesKHR      vkGetRayTracingShaderGroupHandlesKHR_      = nullptr;
        PFN_vkCreateAccelerationStructureKHR          vkCreateAccelerationStructureKHR_          = nullptr;
        PFN_vkDestroyAccelerationStructureKHR         vkDestroyAccelerationStructureKHR_         = nullptr;
        PFN_vkGetAccelerationStructureBuildSizesKHR  vkGetAccelerationStructureBuildSizesKHR_  = nullptr;
        PFN_vkCmdBuildAccelerationStructuresKHR       vkCmdBuildAccelerationStructuresKHR_       = nullptr;
        PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR_ = nullptr;
        PFN_vkCreateRayTracingPipelinesKHR            vkCreateRayTracingPipelinesKHR_            = nullptr;

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProps_{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
        };

        // Custom Buffers/Views (Wrapped Handles)
        Handle<VkImageView> blueNoiseView_;
        Handle<VkBuffer>    reservoirBuffer_;
        Handle<VkBuffer>    frameDataBuffer_;
        Handle<VkBuffer>    debugVisBuffer_;
        Handle<VkRenderPass> renderPass_;  // FIXED: Added renderPass_ member

		// STONEKEY v∞ — THE ONE TRUE SHARED STAGING BUFFER
		uint64_t sharedStagingEnc_ = 0;   // Obfuscated handle — eternal, protected, unbreakable

        // Initialization and Cleanup
        void init(SDL_Window* window, int width, int height);
        void cleanup() noexcept;

        // Validity and Readiness Accessors
        [[nodiscard]] bool isValid() const noexcept {
            // Flag enables during init; full check enforces handles post-setup
            return valid_ &&
                   instance_ != VK_NULL_HANDLE &&
                   surface_ != VK_NULL_HANDLE &&
                   physicalDevice_ != VK_NULL_HANDLE &&
                   device_ != VK_NULL_HANDLE;
        }
        [[nodiscard]] bool isReady() const noexcept { return ready_.load(std::memory_order_acquire); }
        void markReady() noexcept { ready_.store(true, std::memory_order_release); }
        void setValid(bool v) noexcept { valid_ = v; }

        // NEW: Buffer Alignment Helper (for UltraLowLevelBufferTracker::create to fix min size warnings)
        [[nodiscard]] VkDeviceSize getBufferAlignment(VkBufferUsageFlags usage) const noexcept {
            VkDeviceSize alignment = physProps_.limits.nonCoherentAtomSize;
            if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
                alignment = physProps_.limits.minUniformBufferOffsetAlignment;
            } else if (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
                alignment = physProps_.limits.minStorageBufferOffsetAlignment;
            }
            // For AS storage, treat as storage buffer
            if (usage & VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR) {
                alignment = physProps_.limits.minStorageBufferOffsetAlignment;
            }
            return alignment;
        }

        // Core Vulkan Accessors — SECURE VIA STONEKEY
        [[nodiscard]] VkDevice          device()         const noexcept { return g_device(); }
        [[nodiscard]] VkPhysicalDevice  physicalDevice() const noexcept { return g_PhysicalDevice(); }
        [[nodiscard]] VkInstance        instance()       const noexcept { return g_instance(); }
        [[nodiscard]] VkSurfaceKHR      surface()        const noexcept { return g_surface(); }

        // FIXED: Added renderPass() accessor
        [[nodiscard]] VkRenderPass renderPass() const noexcept { return renderPass_.valid() ? renderPass_.get() : VK_NULL_HANDLE; }

        // Legacy Aliases
        [[nodiscard]] VkDevice         vkDevice() const noexcept { return device(); }
        [[nodiscard]] VkPhysicalDevice vkPhysicalDevice() const noexcept { return physicalDevice(); }
        [[nodiscard]] VkSurfaceKHR     vkSurface() const noexcept { return surface(); }

        // Queue Family Accessors
        [[nodiscard]] uint32_t         graphicsFamily() const noexcept { return graphicsFamily_; }
        [[nodiscard]] uint32_t         presentFamily()  const noexcept { return presentFamily_; }
        [[nodiscard]] uint32_t         computeFamily()  const noexcept { return computeFamily_; }

        // Command Pool Accessors
        [[nodiscard]] VkCommandPool    commandPool()       const noexcept { return commandPool_; }
        [[nodiscard]] VkCommandPool    computeCommandPool()const noexcept { return computeCommandPool_; }

        // Queue Accessors
        [[nodiscard]] VkQueue          graphicsQueue() const noexcept { return graphicsQueue_; }
        [[nodiscard]] VkQueue          presentQueue()  const noexcept { return presentQueue_; }
        [[nodiscard]] VkQueue          computeQueue()   const noexcept { return computeQueue_; }

        // Pipeline Cache
        [[nodiscard]] VkPipelineCache  pipelineCacheHandle() const noexcept { return pipelineCache_; }

        // Ray Tracing Function Pointer Accessors (Direct access via members; accessors for consistency)
        [[nodiscard]] PFN_vkCmdTraceRaysKHR                         vkCmdTraceRaysKHR() const noexcept { return vkCmdTraceRaysKHR_; }
        [[nodiscard]] PFN_vkGetRayTracingShaderGroupHandlesKHR      vkGetRayTracingShaderGroupHandlesKHR() const noexcept { return vkGetRayTracingShaderGroupHandlesKHR_; }
        [[nodiscard]] PFN_vkCreateAccelerationStructureKHR          vkCreateAccelerationStructureKHR() const noexcept { return vkCreateAccelerationStructureKHR_; }
        [[nodiscard]] PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR() const noexcept { return vkGetAccelerationStructureDeviceAddressKHR_; }
        [[nodiscard]] PFN_vkCreateRayTracingPipelinesKHR            vkCreateRayTracingPipelinesKHR() const noexcept { return vkCreateRayTracingPipelinesKHR_; }
        [[nodiscard]] PFN_vkGetBufferDeviceAddressKHR               vkGetBufferDeviceAddressKHR() const noexcept { return vkGetBufferDeviceAddressKHR_; }
        [[nodiscard]] PFN_vkGetAccelerationStructureBuildSizesKHR   vkGetAccelerationStructureBuildSizesKHR() const noexcept { return vkGetAccelerationStructureBuildSizesKHR_; }
        [[nodiscard]] PFN_vkCmdBuildAccelerationStructuresKHR       vkCmdBuildAccelerationStructuresKHR() const noexcept { return vkCmdBuildAccelerationStructuresKHR_; }
        [[nodiscard]] PFN_vkDestroyAccelerationStructureKHR         vkDestroyAccelerationStructureKHR() const noexcept { return vkDestroyAccelerationStructureKHR_; }

        [[nodiscard]] const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rayTracingProps() const noexcept { return rayTracingProps_; }

        // Custom Buffer/View Accessors
        [[nodiscard]] VkImageView blueNoiseView() const noexcept { return blueNoiseView_ ? *blueNoiseView_ : VK_NULL_HANDLE; }
        [[nodiscard]] VkBuffer    reservoirBuffer() const noexcept { return reservoirBuffer_ ? *reservoirBuffer_ : VK_NULL_HANDLE; }
        [[nodiscard]] VkBuffer    frameDataBuffer() const noexcept { return frameDataBuffer_ ? *frameDataBuffer_ : VK_NULL_HANDLE; }
        [[nodiscard]] VkBuffer    debugVisBuffer() const noexcept { return debugVisBuffer_ ? *debugVisBuffer_ : VK_NULL_HANDLE; }

        // Utility
        [[nodiscard]] uint32_t currentFrame() const noexcept { return 0; }  // Placeholder
    };

    // =============================================================================
    // GLOBAL ACCESSORS — FIXED: ctx() == g_ctx() + NULL GUARD
    // =============================================================================
    // Global context instance declaration
    extern Context g_context_instance;

    [[nodiscard]] Context& g_ctx() noexcept;

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
    // GLOBAL SWAPCHAIN + LAS
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
    void handleResize(int w, int h);
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

} // namespace RTX

// =============================================================================
// PINK PHOTONS ETERNAL — DOMINANCE ETERNAL
// =============================================================================