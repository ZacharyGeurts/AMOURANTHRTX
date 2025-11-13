// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RTXHandler.hpp — HEADER-ONLY DECLARATIONS
// • NO INLINE IMPLEMENTATIONS (moved to .cpp)
// • Options::Shader::STONEKEY_1 → .cpp only
// • LOG_FATAL_CAT defined
// • VkFormat formatter specialization
// • g_ctx() guard
// • PINK PHOTONS ETERNAL
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
// =============================================================================

#pragma once

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
#include <format>
#include <source_location>
#include <functional>
#include <queue>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "engine/GLOBAL/logging.hpp"

// Forward declarations
class VulkanRTX;
class VulkanPipelineManager;
class VulkanRenderer;
struct Camera;

using namespace Logging::Color;

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
    // Handle<T>
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
        Handle(T h, VkDevice d, DestroyFn del = nullptr, size_t sz = 0, std::string_view t = "");
        Handle(Handle&& o) noexcept;
        Handle& operator=(Handle&& o) noexcept;
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        Handle& operator=(std::nullptr_t) noexcept;
        explicit operator bool() const noexcept;
        T get() const noexcept { return raw; }
        T operator*() const noexcept { return raw; }
        [[nodiscard]] bool valid() const noexcept;
        void reset() noexcept;
        ~Handle();
    };

    template<typename T, typename... Args>
    [[nodiscard]] auto MakeHandle(T h, VkDevice d, Args&&... args);

    // =============================================================================
    // MACROS
    // =============================================================================
    #define HANDLE_CREATE(var, raw, dev, destroyer, size, tag) \
        do { LOG_INFO_CAT("RTX", "HANDLE_CREATE: {} | Tag: {}", #var, tag); (var) = RTX::MakeHandle((raw), (dev), (destroyer), (size), (tag)); } while(0)
    #define HANDLE_GET(var) ((var).get())
    #define HANDLE_RESET(var) do { LOG_INFO_CAT("RTX", "HANDLE_RESET: {}", #var); (var).reset(); } while(0)

    // =============================================================================
    // Context
    // =============================================================================
    struct Context {
        VkInstance       instance_       = VK_NULL_HANDLE;
        VkSurfaceKHR     surface_        = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        VkDevice         device_         = VK_NULL_HANDLE;
        VkQueue          graphicsQueue_  = VK_NULL_HANDLE;
        VkQueue          presentQueue_   = VK_NULL_HANDLE;
        VkCommandPool    commandPool_    = VK_NULL_HANDLE;
        VkPipelineCache  pipelineCache_  = VK_NULL_HANDLE;

        uint32_t graphicsFamily_ = UINT32_MAX;
        uint32_t presentFamily_  = UINT32_MAX;

        // Ray Tracing Extensions
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

        Handle<VkImageView> blueNoiseView_;
        Handle<VkBuffer>    reservoirBuffer_;
        Handle<VkBuffer>    frameDataBuffer_;
        Handle<VkBuffer>    debugVisBuffer_;

        void init(SDL_Window* window, int width, int height);
        [[nodiscard]] bool isValid() const noexcept;
        [[nodiscard]] VkDevice          device()         const noexcept;
        [[nodiscard]] VkPhysicalDevice  physicalDevice() const noexcept;
        [[nodiscard]] VkInstance        instance()       const noexcept;
        [[nodiscard]] VkSurfaceKHR      surface()        const noexcept;
        [[nodiscard]] VkDevice         vkDevice() const noexcept { return device(); }
        [[nodiscard]] VkPhysicalDevice vkPhysicalDevice() const noexcept { return physicalDevice(); }
        [[nodiscard]] VkSurfaceKHR     vkSurface() const noexcept { return surface(); }
        [[nodiscard]] uint32_t         graphicsFamily() const noexcept { return graphicsFamily_; }
        [[nodiscard]] uint32_t         presentFamily() const noexcept { return presentFamily_; }
        [[nodiscard]] VkCommandPool    commandPool() const noexcept { return commandPool_; }
        [[nodiscard]] VkQueue          graphicsQueue() const noexcept { return graphicsQueue_; }
        [[nodiscard]] VkQueue          presentQueue() const noexcept { return presentQueue_; }
        [[nodiscard]] VkPipelineCache  pipelineCacheHandle() const noexcept { return pipelineCache_; }
        [[nodiscard]] PFN_vkCmdTraceRaysKHR                         vkCmdTraceRaysKHR() const noexcept { return vkCmdTraceRaysKHR_; }
        [[nodiscard]] PFN_vkGetRayTracingShaderGroupHandlesKHR      vkGetRayTracingShaderGroupHandlesKHR() const noexcept { return vkGetRayTracingShaderGroupHandlesKHR_; }
        [[nodiscard]] PFN_vkCreateAccelerationStructureKHR          vkCreateAccelerationStructureKHR() const noexcept { return vkCreateAccelerationStructureKHR_; }
        [[nodiscard]] PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR() const noexcept { return vkGetAccelerationStructureDeviceAddressKHR_; }
        [[nodiscard]] PFN_vkCreateRayTracingPipelinesKHR            vkCreateRayTracingPipelinesKHR() const noexcept { return vkCreateRayTracingPipelinesKHR_; }
        [[nodiscard]] PFN_vkGetBufferDeviceAddressKHR               vkGetBufferDeviceAddressKHR() const noexcept { return vkGetBufferDeviceAddressKHR_; }
        [[nodiscard]] const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rayTracingProps() const noexcept { return rayTracingProps_; }
        [[nodiscard]] VkImageView blueNoiseView() const noexcept { return blueNoiseView_ ? *blueNoiseView_ : VK_NULL_HANDLE; }
        [[nodiscard]] VkBuffer    reservoirBuffer() const noexcept { return reservoirBuffer_ ? *reservoirBuffer_ : VK_NULL_HANDLE; }
        [[nodiscard]] VkBuffer    frameDataBuffer() const noexcept { return frameDataBuffer_ ? *frameDataBuffer_ : VK_NULL_HANDLE; }
        [[nodiscard]] VkBuffer    debugVisBuffer() const noexcept { return debugVisBuffer_ ? *debugVisBuffer_ : VK_NULL_HANDLE; }
        [[nodiscard]] uint32_t currentFrame() const noexcept { return 0; }
    };

    // =============================================================================
    // GLOBAL ACCESSORS
    // =============================================================================
    [[nodiscard]] Context& ctx();
    [[nodiscard]] Context& g_ctx();

    // =============================================================================
    // UltraLowLevelBufferTracker
    // =============================================================================
    struct BufferData {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
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
        uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, std::string_view tag) noexcept;
        void destroy(uint64_t handle) noexcept;
        BufferData* getData(uint64_t handle) noexcept;
        const BufferData* getData(uint64_t handle) const noexcept;
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
    // MACROS
    // =============================================================================
    #define BUFFER(handle) uint64_t handle = 0ULL
    #define BUFFER_CREATE(handle, size, usage, props, tag) \
        do { LOG_INFO_CAT("RTX", "BUFFER_CREATE: {} | Size {} | Tag: {}", #handle, size, tag); (handle) = RTX::UltraLowLevelBufferTracker::get().create((size), (usage), (props), (tag)); } while (0)
    #define BUFFER_MAP(h, ptr) \
        do { (ptr) = nullptr; auto* d = RTX::UltraLowLevelBufferTracker::get().getData((h)); \
             if (d) { void* p{}; LOG_INFO_CAT("RTX", "Mapping buffer: 0x{:x} | Size {}", reinterpret_cast<uint64_t>(d->buffer), d->size); if (vkMapMemory(RTX::g_ctx().device(), d->memory, 0, d->size, 0, &p) == VK_SUCCESS) (ptr) = p; } } while (0)
    #define BUFFER_UNMAP(h) \
        do { auto* d = RTX::UltraLowLevelBufferTracker::get().getData((h)); if (d) { LOG_INFO_CAT("RTX", "Unmapping buffer: 0x{:x}", reinterpret_cast<uint64_t>(d->buffer)); vkUnmapMemory(RTX::g_ctx().device(), d->memory); } } while (0)
    #define BUFFER_DESTROY(handle) \
        do { LOG_INFO_CAT("RTX", "BUFFER_DESTROY: {}", #handle); RTX::UltraLowLevelBufferTracker::get().destroy((handle)); } while (0)
    #define RAW_BUFFER(handle) \
        (RTX::UltraLowLevelBufferTracker::get().getData((handle)) ? RTX::UltraLowLevelBufferTracker::get().getData((handle))->buffer : VK_NULL_HANDLE)
    #define BUFFER_MEMORY(handle) \
        (RTX::UltraLowLevelBufferTracker::get().getData((handle)) ? RTX::UltraLowLevelBufferTracker::get().getData((handle))->memory : VK_NULL_HANDLE)

    // =============================================================================
    // AutoBuffer
    // =============================================================================
    struct AutoBuffer {
        uint64_t id{0ULL};
        AutoBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::string_view tag = "AutoBuffer") noexcept;
        ~AutoBuffer() noexcept;
        AutoBuffer(AutoBuffer&& o) noexcept;
        AutoBuffer& operator=(AutoBuffer&& o) noexcept;
        VkBuffer raw() const noexcept;
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

    // =============================================================================
    // Helpers (declarations only)
    // =============================================================================
    void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size);

    // stonekey_xor_spirv → MOVED TO .cpp (uses Options::Shader::STONEKEY_1)
    void stonekey_xor_spirv(std::vector<uint32_t>& data, bool encrypt = true);

} // namespace RTX