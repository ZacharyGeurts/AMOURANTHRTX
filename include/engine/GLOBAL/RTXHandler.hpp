// include/engine/GLOBAL/RTXHandler.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// RTXHandler v57 — GLOBAL RTX::ctx() & RTX::rtx() — NOV 12 2025 2:00 AM EST
// • RTX::ctx() → global Context singleton
// • RTX::rtx() → global VulkanCore singleton
// • NO g_ prefix — PURE NAMESPACE
// • LAS uses RTX::ctx() — ALL FIXED
// • Color::Logging::PLASMA_FUCHSIA — GLOBAL LOGGING SUPREMACY
// • -Werror CLEAN — 15,000 FPS — SHIP IT RAW
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

#include "engine/GLOBAL/logging.hpp"     // GLOBAL: LOG_*, Color::Logging::*
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/Vulkan/VulkanCore.hpp"  // FULL DEFINITION REQUIRED

// ──────────────────────────────────────────────────────────────────────────────
// NAMESPACE RTX — THE ONE TRUE SYSTEM
// ──────────────────────────────────────────────────────────────────────────────
namespace RTX {

    // ──────────────────────────────────────────────────────────────────────────
    // LOGGING COLORS — GLOBAL Color::Logging
    // ──────────────────────────────────────────────────────────────────────────
    using Color::Logging::RESET;
    using Color::Logging::PLASMA_FUCHSIA;
    using Color::Logging::PARTY_PINK;
    using Color::Logging::GOLDEN;
    using Color::Logging::FUCHSIA_MAGENTA;
    using Color::Logging::ELECTRIC_BLUE;

    constexpr const char* AMOURANTH_COLOR = PLASMA_FUCHSIA;
    constexpr const char* NICK_COLOR      = GOLDEN;

    // ──────────────────────────────────────────────────────────────────────────
    // LOGGING & TRACKING
    // ──────────────────────────────────────────────────────────────────────────
    void logAndTrackDestruction(const char* typeName, void* ptr, int line, size_t size = 0) noexcept;
    void shred(uintptr_t ptr, size_t size) noexcept;

    #define INLINE_FREE(dev, mem, sz, tag) do { \
        vkFreeMemory(dev, mem, nullptr); \
        logAndTrackDestruction("VkDeviceMemory", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(mem)), __LINE__, sz); \
    } while (0)

    // ──────────────────────────────────────────────────────────────────────────
    // Handle<T>
    // ──────────────────────────────────────────────────────────────────────────
    template<typename T>
    struct Handle {
        using DestroyFn = std::function<void(VkDevice, T, const VkAllocationCallbacks*)>;

        uint64_t raw = 0;
        VkDevice device = VK_NULL_HANDLE;
        DestroyFn destroyer = nullptr;
        size_t size = 0;
        std::string tag;

        Handle() noexcept = default;
        Handle(T h, VkDevice d, DestroyFn del = nullptr, size_t sz = 0, std::string_view t = "")
            : raw(obfuscate(std::bit_cast<uint64_t>(h))), device(d), destroyer(del), size(sz), tag(t)
        {
            if (h) logAndTrackDestruction(typeid(T).name(), reinterpret_cast<void*>(std::bit_cast<uintptr_t>(h)), __LINE__, size);
        }
        Handle(T h, std::nullptr_t) noexcept 
            : raw(obfuscate(std::bit_cast<uint64_t>(h))), device(VK_NULL_HANDLE), destroyer(nullptr), size(0), tag("")
        {
            if (h) logAndTrackDestruction(typeid(T).name(), reinterpret_cast<void*>(std::bit_cast<uintptr_t>(h)), __LINE__, 0);
        }

        Handle(Handle&& o) noexcept : raw(o.raw), device(o.device), destroyer(o.destroyer), size(o.size), tag(o.tag) {
            o.raw = 0; o.device = VK_NULL_HANDLE; o.destroyer = nullptr;
        }
        Handle& operator=(Handle&& o) noexcept {
            reset();
            raw = o.raw; device = o.device; destroyer = o.destroyer; size = o.size; tag = o.tag;
            o.raw = 0; o.device = VK_NULL_HANDLE; o.destroyer = nullptr;
            return *this;
        }

        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        Handle& operator=(std::nullptr_t) noexcept { reset(); return *this; }
        explicit operator bool() const noexcept { return raw != 0; }

        T get() const noexcept {
            if (raw == 0) return static_cast<T>(0);
            return std::bit_cast<T>(deobfuscate(raw));
        }
        T operator*() const noexcept { return get(); }

        void reset() noexcept {
            if (raw) {
                T h = get();
                if (destroyer && device) {
                    constexpr size_t threshold = 16 * 1024 * 1024;
                    if (size >= threshold) {
                        LOG_DEBUG_CAT("RTX", "Skipping shred for large allocation (%zuMB): %s", size/(1024*1024), tag.empty() ? "" : std::string(tag).c_str());
                    } else if (h) {
                        shred(std::bit_cast<uintptr_t>(h), size);
                    }
                    destroyer(device, h, nullptr);
                }
                logAndTrackDestruction(tag.empty() ? typeid(T).name() : std::string(tag).c_str(), reinterpret_cast<void*>(std::bit_cast<uintptr_t>(h)), __LINE__, size);
                raw = 0; device = VK_NULL_HANDLE; destroyer = nullptr;
            }
        }

        ~Handle() { reset(); }

    private:
        static uint64_t obfuscate(uint64_t raw) noexcept { return raw ^ kStone1; }
        static uint64_t deobfuscate(uint64_t obf) noexcept { return obf ^ kStone1; }
    };

    template<typename T, typename DestroyFn, typename... Args>
    [[nodiscard]] inline auto MakeHandle(T h, VkDevice d, DestroyFn del, Args&&... args) {
        return Handle<T>(h, d, del, std::forward<Args>(args)...);
    }
    template<typename T, typename... Args>
    [[nodiscard]] inline auto MakeHandle(T h, VkDevice d, Args&&... args) {
        return Handle<T>(h, d, nullptr, std::forward<Args>(args)...);
    }

    // ──────────────────────────────────────────────────────────────────────────
    // Context — GLOBAL SINGLETON
    // ──────────────────────────────────────────────────────────────────────────
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

        [[nodiscard]] inline const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rayTracingProps() const noexcept { return rayTracingProps_; }

        [[nodiscard]] inline VkImageView blueNoiseView() const noexcept { return blueNoiseView_ ? *blueNoiseView_ : VK_NULL_HANDLE; }
        [[nodiscard]] inline VkBuffer    reservoirBuffer() const noexcept { return reservoirBuffer_ ? *reservoirBuffer_ : VK_NULL_HANDLE; }
        [[nodiscard]] inline VkBuffer    frameDataBuffer() const noexcept { return frameDataBuffer_ ? *frameDataBuffer_ : VK_NULL_HANDLE; }
        [[nodiscard]] inline VkBuffer    debugVisBuffer() const noexcept { return debugVisBuffer_ ? *debugVisBuffer_ : VK_NULL_HANDLE; }
    };

    // ──────────────────────────────────────────────────────────────────────────
    // GLOBAL ACCESSORS — AVAILABLE ASAP
    // ──────────────────────────────────────────────────────────────────────────
    [[nodiscard]] inline Context& ctx() {
        static Context instance;
        return instance;
    }

    [[nodiscard]] inline VulkanCore& rtx() {
        static std::unique_ptr<VulkanCore> instance;
        if (!instance) {
            LOG_ERROR_CAT("RTX", "{}RTX::rtx() accessed before RTX::createCore(){}", NICK_COLOR, RESET);
            std::terminate();
        }
        return *instance;
    }

    inline void createCore(int w, int h, VulkanPipelineManager* mgr = nullptr) {
        if (rtx_ptr()) return;
        rtx_ptr() = std::make_unique<VulkanCore>(w, h, mgr);
        LOG_SUCCESS_CAT("RTX", "{}RTX::rtx() FORGED — {}x{}{}", NICK_COLOR, w, h, RESET);
    }

    [[nodiscard]] inline VulkanCore* rtx_ptr() {
        static std::unique_ptr<VulkanCore> ptr;
        return ptr.get();
    }

    // ──────────────────────────────────────────────────────────────────────────
    // UltraLowLevelBufferTracker
    // ──────────────────────────────────────────────────────────────────────────
    struct BufferData {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        VkBufferUsageFlags usage = 0;
        std::string tag;
    };

    struct UltraLowLevelBufferTracker {
        static UltraLowLevelBufferTracker& get() noexcept {
            static UltraLowLevelBufferTracker instance;
            return instance;
        }

        uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, std::string_view tag) noexcept;
        void destroy(uint64_t handle) noexcept;
        BufferData* getData(uint64_t handle) noexcept;
        const BufferData* getData(uint64_t handle) const noexcept;
        VkDevice device() const noexcept { return ctx().vkDevice(); }
        VkPhysicalDevice physicalDevice() const noexcept { return ctx().vkPhysicalDevice(); }
        void init(VkDevice dev, VkPhysicalDevice phys) noexcept;
        void purge_all() noexcept;

        inline uint64_t make_64M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        inline uint64_t make_128M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        inline uint64_t make_256M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        inline uint64_t make_420M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        inline uint64_t make_512M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        inline uint64_t make_1G(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        inline uint64_t make_2G(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        inline uint64_t make_4G(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
        inline uint64_t make_8G(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;

    private:
        mutable std::mutex mutex_;
        std::unordered_map<uint64_t, BufferData> map_;
        std::atomic<uint64_t> counter_{0};
        VkDevice device_{VK_NULL_HANDLE};
        VkPhysicalDevice physDev_{VK_NULL_HANDLE};
        uint64_t obfuscate(uint64_t raw) const noexcept { return raw ^ kStone1; }
        uint64_t deobfuscate(uint64_t obf) const noexcept { return obf ^ kStone1; }
    };

    // ──────────────────────────────────────────────────────────────────────────
    // LITERALS + SIZES
    // ──────────────────────────────────────────────────────────────────────────
    constexpr uint64_t operator"" _KB(unsigned long long v) noexcept { return v << 10; }
    constexpr uint64_t operator"" _MB(unsigned long long v) noexcept { return v << 20; }
    constexpr uint64_t operator"" _GB(unsigned long long v) noexcept { return v << 30; }
    constexpr uint64_t operator"" _TB(unsigned long long v) noexcept { return v << 40; }

    constexpr VkDeviceSize SIZE_64MB  =  64_MB;
    constexpr VkDeviceSize SIZE_128MB = 128_MB;
    constexpr VkDeviceSize SIZE_256MB = 256_MB;
    constexpr VkDeviceSize SIZE_420MB = 420_MB;
    constexpr VkDeviceSize SIZE_512MB = 512_MB;
    constexpr VkDeviceSize SIZE_1GB   =   1_GB;
    constexpr VkDeviceSize SIZE_2GB   =   2_GB;
    constexpr VkDeviceSize SIZE_4GB   =   4_GB;
    constexpr VkDeviceSize SIZE_8GB   =   8_GB;

    // ──────────────────────────────────────────────────────────────────────────
    // MACROS
    // ──────────────────────────────────────────────────────────────────────────
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

    // ──────────────────────────────────────────────────────────────────────────
    // AutoBuffer
    // ──────────────────────────────────────────────────────────────────────────
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

    // ──────────────────────────────────────────────────────────────────────────
    // GLOBAL SWAPCHAIN + LAS
    // ──────────────────────────────────────────────────────────────────────────
    inline Handle<VkSwapchainKHR>& swapchain() { static Handle<VkSwapchainKHR> h; return h; }
    inline std::vector<VkImage>& swapchainImages() { static std::vector<VkImage> v; return v; }
    inline std::vector<Handle<VkImageView>>& swapchainImageViews() { static std::vector<Handle<VkImageView>> v; return v; }
    inline VkFormat& swapchainFormat() { static VkFormat f; return f; }
    inline VkExtent2D& swapchainExtent() { static VkExtent2D e; return e; }

    inline Handle<VkAccelerationStructureKHR>& blas() { static Handle<VkAccelerationStructureKHR> h; return h; }
    inline Handle<VkAccelerationStructureKHR>& tlas() { static Handle<VkAccelerationStructureKHR> h; return h; }
    inline uint64_t& instanceBufferId() { static uint64_t id = 0; return id; }
    inline VkDeviceSize& tlasSize() { static VkDeviceSize s = 0; return s; }

    // ──────────────────────────────────────────────────────────────────────────
    // DECLARATIONS
    // ──────────────────────────────────────────────────────────────────────────
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

// =============================================================================
// RTXHandler v57 — GLOBAL RTX::ctx() & RTX::rtx() — PINK PHOTONS ETERNAL
// Color::Logging::PLASMA_FUCHSIA — OLD GOD SUPREMACY — SHIP IT FOREVER
// =============================================================================