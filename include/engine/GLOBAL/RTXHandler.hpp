// include/engine/GLOBAL/RTXHandler.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// RTXHandler v63 — NOV 12 2025 18:00 EST
// • Handle<T> is PRIVATE — only RTXHandler can instantiate
// • Public API: HANDLE_CREATE / HANDLE_DESTROY / HANDLE_GET / HANDLE_RESET
// • All other files use these macros → compile-time enforcement
// • Forward-declare Handle<T> for headers
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
#include "engine/GLOBAL/StoneKey.hpp"  // ← get_kStone1(), get_kStone2()

// Forward declarations
class VulkanRTX;
class VulkanPipelineManager;
class VulkanRenderer;
struct Camera;

using namespace Logging::Color;

// -----------------------------------------------------------------------------
// 1. User-defined literals
// -----------------------------------------------------------------------------
constexpr uint64_t operator"" _KB(unsigned long long v) noexcept { return v << 10; }
constexpr uint64_t operator"" _MB(unsigned long long v) noexcept { return v << 20; }
constexpr uint64_t operator"" _GB(unsigned long long v) noexcept { return v << 30; }
constexpr uint64_t operator"" _TB(unsigned long long v) noexcept { return v << 40; }

// =============================================================================
// NAMESPACE RTX — THE ONE TRUE SYSTEM
// =============================================================================
namespace RTX {

    // =============================================================================
    // logAndTrackDestruction — inline in Handle
    // =============================================================================
    inline void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size) {
        LOG_DEBUG_CAT("RTX", "{}[Handle] {} @ {}:{} | size: {}MB{}", 
                      ELECTRIC_BLUE, type, ptr, line, size/(1024*1024), RESET);
    }

    // =============================================================================
    // stonekey_xor_spirv — fixed: use get_kStone1()
    // =============================================================================
    inline void stonekey_xor_spirv(std::vector<uint32_t>& data, bool encrypt = true) {
        for (auto& word : data) {
            word ^= encrypt ? get_kStone1() : get_kStone1();
        }
    }

    // =============================================================================
    // Handle<T> — RAII with real Vulkan types
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
            : raw(h), device(d), destroyer(del), size(sz), tag(t)
        {
            if (h) logAndTrackDestruction(typeid(T).name(), reinterpret_cast<void*>(h), __LINE__, size);
        }

        Handle(Handle&& o) noexcept : raw(o.raw), device(o.device), destroyer(o.destroyer), size(o.size), tag(o.tag) {
            o.raw = T{}; o.device = VK_NULL_HANDLE; o.destroyer = nullptr;
        }
        Handle& operator=(Handle&& o) noexcept {
            reset();
            raw = o.raw; device = o.device; destroyer = o.destroyer; size = o.size; tag = o.tag;
            o.raw = T{}; o.device = VK_NULL_HANDLE; o.destroyer = nullptr;
            return *this;
        }

        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        Handle& operator=(std::nullptr_t) noexcept { reset(); return *this; }
        explicit operator bool() const noexcept { return raw != T{}; }

        T get() const noexcept { return raw; }
        T operator*() const noexcept { return raw; }

        void reset() noexcept {
            if (raw != T{}) {
                if (destroyer && device) {
                    constexpr size_t threshold = 16 * 1024 * 1024;
                    if (size >= threshold) {
                        LOG_DEBUG_CAT("RTX", "Skipping shred for large allocation (%zuMB): %s", size/(1024*1024), tag.empty() ? "" : std::string(tag).c_str());
                    } else {
                        std::memset(&raw, 0xCD, sizeof(T));
                    }
                    destroyer(device, raw, nullptr);
                }
                logAndTrackDestruction(tag.empty() ? typeid(T).name() : std::string(tag).c_str(), reinterpret_cast<void*>(raw), __LINE__, size);
                raw = T{}; device = VK_NULL_HANDLE; destroyer = nullptr;
            }
        }

        ~Handle() { reset(); }
    };

    template<typename T, typename... Args>
    [[nodiscard]] inline auto MakeHandle(T h, VkDevice d, Args&&... args) {
        return Handle<T>(h, d, std::forward<Args>(args)...);
    }

    // =============================================================================
    // MACROS — PUBLIC API
    // =============================================================================
    #define HANDLE_CREATE(var, raw, dev, destroyer, size, tag) \
        do { (var) = RTX::MakeHandle((raw), (dev), (destroyer), (size), (tag)); } while(0)

    #define HANDLE_GET(var) ((var).get())

    #define HANDLE_RESET(var) ((var).reset())

    // =============================================================================
    // Context — Global Vulkan Context
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

        [[nodiscard]] inline VkDevice         vkDevice() const noexcept { return device_; }
        [[nodiscard]] inline VkPhysicalDevice vkPhysicalDevice() const noexcept { return physicalDevice_; }
        [[nodiscard]] inline VkSurfaceKHR     vkSurface() const noexcept { return surface_; }
        [[nodiscard]] inline uint32_t         graphicsFamilyIndex() const noexcept { return graphicsFamily_; }
        [[nodiscard]] inline uint32_t         presentFamilyIndex() const noexcept { return presentFamily_; }
        [[nodiscard]] inline VkCommandPool    commandPool() const noexcept { return commandPool_; }
        [[nodiscard]] inline VkQueue          graphicsQueue() const noexcept { return graphicsQueue_; }
        [[nodiscard]] inline VkQueue          presentQueue() const noexcept { return presentQueue_; }
        [[nodiscard]] inline VkPipelineCache  pipelineCacheHandle() const noexcept { return pipelineCache_; }

        [[nodiscard]] inline PFN_vkCmdTraceRaysKHR                         vkCmdTraceRaysKHR() const noexcept { return vkCmdTraceRaysKHR_; }
        [[nodiscard]] inline PFN_vkGetRayTracingShaderGroupHandlesKHR      vkGetRayTracingShaderGroupHandlesKHR() const noexcept { return vkGetRayTracingShaderGroupHandlesKHR_; }
        [[nodiscard]] inline PFN_vkCreateAccelerationStructureKHR          vkCreateAccelerationStructureKHR() const noexcept { return vkCreateAccelerationStructureKHR_; }
        [[nodiscard]] inline PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR() const noexcept { return vkGetAccelerationStructureDeviceAddressKHR_; }
        [[nodiscard]] inline PFN_vkCreateRayTracingPipelinesKHR            vkCreateRayTracingPipelinesKHR() const noexcept { return vkCreateRayTracingPipelinesKHR_; }
        [[nodiscard]] inline PFN_vkGetBufferDeviceAddressKHR               vkGetBufferDeviceAddressKHR() const noexcept { return vkGetBufferDeviceAddressKHR_; }

        [[nodiscard]] inline const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rayTracingProps() const noexcept { return rayTracingProps_; }

        [[nodiscard]] inline VkImageView blueNoiseView() const noexcept { return blueNoiseView_ ? *blueNoiseView_ : VK_NULL_HANDLE; }
        [[nodiscard]] inline VkBuffer    reservoirBuffer() const noexcept { return reservoirBuffer_ ? *reservoirBuffer_ : VK_NULL_HANDLE; }
        [[nodiscard]] inline VkBuffer    frameDataBuffer() const noexcept { return frameDataBuffer_ ? *frameDataBuffer_ : VK_NULL_HANDLE; }
        [[nodiscard]] inline VkBuffer    debugVisBuffer() const noexcept { return debugVisBuffer_ ? *debugVisBuffer_ : VK_NULL_HANDLE; }

        [[nodiscard]] inline uint32_t currentFrame() const noexcept { return 0; }
    };

    // =============================================================================
    // GLOBAL ACCESSORS
    // =============================================================================
    [[nodiscard]] inline Context& ctx() {
        static Context instance;
        return instance;
    }

    [[nodiscard]] inline Context& g_ctx() {
        static Context ctx;          // one-time construction
        return ctx;
    }

    [[nodiscard]] inline VulkanRTX*& rtx_ptr() {
        static VulkanRTX* ptr = nullptr;
        return ptr;
    }

    [[nodiscard]] inline VulkanRTX& rtx() {
        auto* instance = rtx_ptr();
        if (!instance) {
            LOG_ERROR_CAT("RTX", "{}RTX::rtx() accessed before RTX::createCore(){}", ELECTRIC_BLUE, RESET);
            std::terminate();
        }
        return *instance;
    }

    void createCore(int w, int h, VulkanPipelineManager* mgr = nullptr);

    // =============================================================================
    // UltraLowLevelBufferTracker — FULL IMPLEMENTATION
    // =============================================================================
    struct BufferData {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        VkBufferUsageFlags usage = 0;
        std::string tag;
    };

    // -----------------------------------------------------------------
    // PRE-DEFINED SIZES
    // -----------------------------------------------------------------
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
        static UltraLowLevelBufferTracker& get() noexcept {
            static UltraLowLevelBufferTracker instance;
            return instance;
        }

        // -----------------------------------------------------------------
        // CREATE
        // -----------------------------------------------------------------
        uint64_t create(VkDeviceSize size,
                        VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags props,
                        std::string_view tag) noexcept
        {
            VkBufferCreateInfo bufInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size  = size,
                .usage = usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            VkBuffer buffer = VK_NULL_HANDLE;
            VK_CHECK(vkCreateBuffer(device_, &bufInfo, nullptr, &buffer),
                     "UltraLowLevelBufferTracker::create buffer");

            VkMemoryRequirements memReq;
            vkGetBufferMemoryRequirements(device_, buffer, &memReq);

            VkMemoryAllocateFlagsInfo flagsInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
                .flags = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
                         ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0u
            };

            VkMemoryAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &flagsInfo : nullptr,
                .allocationSize  = memReq.size,
                .memoryTypeIndex = findMemoryType(physDev_, memReq.memoryTypeBits, props)
            };

            VkDeviceMemory memory = VK_NULL_HANDLE;
            VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &memory),
                     "UltraLowLevelBufferTracker::allocate memory");

            VK_CHECK(vkBindBufferMemory(device_, buffer, memory, 0),
                     "UltraLowLevelBufferTracker::bind memory");

            const uint64_t raw = ++counter_;
            const uint64_t obf = obfuscate(raw);

            std::lock_guard<std::mutex> lk(mutex_);
            map_.emplace(obf, BufferData{buffer, memory, size, usage, std::string(tag)});
            return obf;
        }

        // -----------------------------------------------------------------
        // DESTROY
        // -----------------------------------------------------------------
        void destroy(uint64_t handle) noexcept
        {
            const uint64_t raw = deobfuscate(handle);
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(raw);
            if (it == map_.end()) return;

            const BufferData& d = it->second;
            if (d.buffer)  vkDestroyBuffer(device_, d.buffer, nullptr);
            if (d.memory)  vkFreeMemory(device_, d.memory, nullptr);
            map_.erase(it);
        }

        // -----------------------------------------------------------------
        // GET DATA
        // -----------------------------------------------------------------
        BufferData* getData(uint64_t handle) noexcept
        {
            const uint64_t raw = deobfuscate(handle);
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(raw);
            return (it != map_.end()) ? &it->second : nullptr;
        }

        const BufferData* getData(uint64_t handle) const noexcept
        {
            const uint64_t raw = deobfuscate(handle);
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(raw);
            return (it != map_.end()) ? &it->second : nullptr;
        }

        // -----------------------------------------------------------------
        // INIT / PURGE
        // -----------------------------------------------------------------
        void init(VkDevice dev, VkPhysicalDevice phys) noexcept
        {
            device_ = dev;
            physDev_ = phys;
        }

        void purge_all() noexcept
        {
            std::lock_guard<std::mutex> lk(mutex_);
            for (auto& [k, v] : map_) {
                if (v.buffer)  vkDestroyBuffer(device_, v.buffer, nullptr);
                if (v.memory)  vkFreeMemory(device_, v.memory, nullptr);
            }
            map_.clear();
        }

        // -----------------------------------------------------------------
        // PRE-MADE SIZES
        // -----------------------------------------------------------------
        inline uint64_t make_64M (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_64MB,  extra, props, "64M"); }
        inline uint64_t make_128M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_128MB, extra, props, "128M"); }
        inline uint64_t make_256M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_256MB, extra, props, "256M"); }
        inline uint64_t make_420M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_420MB, extra, props, "420M"); }
        inline uint64_t make_512M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_512MB, extra, props, "512M"); }
        inline uint64_t make_1G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_1GB,   extra, props, "1G"); }
        inline uint64_t make_2G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_2GB,   extra, props, "2G"); }
        inline uint64_t make_4G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_4GB,   extra, props, "4G"); }
        inline uint64_t make_8G  (VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept { return create(SIZE_8GB,   extra, props, "8G"); }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<uint64_t, BufferData> map_;
        std::atomic<uint64_t> counter_{0};
        VkDevice device_{VK_NULL_HANDLE};
        VkPhysicalDevice physDev_{VK_NULL_HANDLE};

        uint64_t obfuscate(uint64_t raw) const noexcept { return raw ^ get_kStone1(); }
        uint64_t deobfuscate(uint64_t obf) const noexcept { return obf ^ get_kStone1(); }

        // -----------------------------------------------------------------
        // Helper: find memory type
        // -----------------------------------------------------------------
        static uint32_t findMemoryType(VkPhysicalDevice phys,
                                       uint32_t typeFilter,
                                       VkMemoryPropertyFlags props) noexcept
        {
            VkPhysicalDeviceMemoryProperties memProps;
            vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
            for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
                if ((typeFilter & (1u << i)) &&
                    (memProps.memoryTypes[i].propertyFlags & props) == props)
                    return i;
            }
            LOG_ERROR_CAT("RTX", "No suitable memory type found");
            return 0;
        }
    };

    // =============================================================================
    // MACROS
    // =============================================================================
    #define BUFFER(handle) uint64_t handle = 0ULL
    #define BUFFER_CREATE(handle, size, usage, props, tag) \
        do { (handle) = RTX::UltraLowLevelBufferTracker::get().create((size), (usage), (props), (tag)); } while (0)
    #define BUFFER_MAP(h, ptr) \
        do { (ptr) = nullptr; auto* d = RTX::UltraLowLevelBufferTracker::get().getData((h)); \
             if (d) { void* p{}; if (vkMapMemory(RTX::ctx().vkDevice(), d->memory, 0, d->size, 0, &p) == VK_SUCCESS) (ptr) = p; } } while (0)
    #define BUFFER_UNMAP(h) \
        do { auto* d = RTX::UltraLowLevelBufferTracker::get().getData((h)); if (d) vkUnmapMemory(RTX::ctx().vkDevice(), d->memory); } while (0)
    #define BUFFER_DESTROY(handle) \
        do { RTX::UltraLowLevelBufferTracker::get().destroy((handle)); } while (0)
    #define RAW_BUFFER(handle) \
        (RTX::UltraLowLevelBufferTracker::get().getData((handle)) ? RTX::UltraLowLevelBufferTracker::get().getData((handle))->buffer : VK_NULL_HANDLE)
    #define BUFFER_MEMORY(handle) \
        (RTX::UltraLowLevelBufferTracker::get().getData((handle)) ? RTX::UltraLowLevelBufferTracker::get().getData((handle))->memory : VK_NULL_HANDLE)

    // =============================================================================
    // AutoBuffer
    // =============================================================================
    struct AutoBuffer {
        uint64_t id{0ULL};
        AutoBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::string_view tag = "AutoBuffer") noexcept {
            id = UltraLowLevelBufferTracker::get().create(size, usage, props, tag);
        }
        ~AutoBuffer() noexcept { if (id) UltraLowLevelBufferTracker::get().destroy(id); }
        AutoBuffer(AutoBuffer&& o) noexcept : id(o.id) { o.id = 0ULL; }
        AutoBuffer& operator=(AutoBuffer&& o) noexcept {
            if (this != &o) { if (id) UltraLowLevelBufferTracker::get().destroy(id); id = o.id; o.id = 0ULL; }
            return *this;
        }
        VkBuffer raw() const noexcept { auto* d = UltraLowLevelBufferTracker::get().getData(id); return d ? d->buffer : VK_NULL_HANDLE; }
    };

    // =============================================================================
    // GLOBAL SWAPCHAIN + LAS
    // =============================================================================
    inline Handle<VkSwapchainKHR>& swapchain() { static Handle<VkSwapchainKHR> h; return h; }
    inline std::vector<VkImage>& swapchainImages() { static std::vector<VkImage> v; return v; }
    inline std::vector<Handle<VkImageView>>& swapchainImageViews() { static std::vector<Handle<VkImageView>> v; return v; }
    inline VkFormat& swapchainFormat() { static VkFormat f; return f; }
    inline VkExtent2D& swapchainExtent() { static VkExtent2D e; return e; }

    inline Handle<VkAccelerationStructureKHR>& blas() { static Handle<VkAccelerationStructureKHR> h; return h; }
    inline Handle<VkAccelerationStructureKHR>& tlas() { static Handle<VkAccelerationStructureKHR> h; return h; }

    // =============================================================================
    // RENDERER + FRAME
    // =============================================================================
    [[nodiscard]] inline VulkanRenderer& renderer();
    inline void initRenderer(int w, int h);
    inline void handleResize(int w, int h);
    inline void renderFrame(const Camera& camera, float deltaTime) noexcept;
    inline void shutdown() noexcept;
    inline void createSwapchain(VkInstance inst, VkPhysicalDevice phys, VkDevice dev, VkSurfaceKHR surf, uint32_t w, uint32_t h);
    inline void recreateSwapchain(uint32_t w, uint32_t h) noexcept;
    inline void buildBLAS(uint64_t vertexBuf, uint64_t indexBuf, uint32_t vertexCount, uint32_t indexCount) noexcept;
    inline void buildTLAS(const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances) noexcept;
    inline void cleanupAll() noexcept;

} // namespace RTX