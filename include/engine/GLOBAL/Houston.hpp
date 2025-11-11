// include/engine/GLOBAL/Houston.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// HOUSTON v9.5 — FINAL & CLEAN — NOV 11 2025 1:46 PM EST
// • All make_* helpers declared + implemented at bottom
// • No duplicate SIZE_* constants
// • Full power-of-two literals + make_* suite
// • C++23, -Werror clean, zero leaks, Valhalla sealed
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

// ──────────────────────────────────────────────────────────────────────────────
// Includes — MOVED EARLY FOR DEPENDENCIES
// ──────────────────────────────────────────────────────────────────────────────
#include "engine/GLOBAL/StoneKey.hpp"  // For obfuscate, deobfuscate, kStone1, kStone2
#include "engine/GLOBAL/logging.hpp"   // For LOG_DEBUG_CAT, ENABLE_DEBUG
#include "engine/GLOBAL/GlobalContext.hpp"

// ──────────────────────────────────────────────────────────────────────────────
// Forward Declarations 
// ──────────────────────────────────────────────────────────────────────────────
struct Context;
class VulkanRenderer;
class Camera;

// ──────────────────────────────────────────────────────────────────────────────
// Logging & Tracking — DECLARED EARLY
// ──────────────────────────────────────────────────────────────────────────────
#define INLINE_FREE(dev, mem, sz, tag) do { \
    vkFreeMemory(dev, mem, nullptr); \
    logAndTrackDestruction("VkDeviceMemory", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(mem)), __LINE__, sz); \
} while (0)

void logAndTrackDestruction(const char* typeName, void* ptr, int line, size_t size = 0) noexcept {
    if (ptr && ENABLE_DEBUG) {
        LOG_DEBUG_CAT("Houston", "Destroyed %s @ %p (line %d, size %zu)", typeName, ptr, line, size);
    }
}

void shred(uintptr_t ptr, size_t size) noexcept;

// ──────────────────────────────────────────────────────────────────────────────
// UltraLowLevelBufferTracker — STRUCT DEF EARLY
// ──────────────────────────────────────────────────────────────────────────────
struct BufferData {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    std::string tag;
};

struct UltraLowLevelBufferTracker {
    static UltraLowLevelBufferTracker& get() noexcept;
    uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, std::string_view tag) noexcept;
    void destroy(uint64_t handle) noexcept;
    BufferData* getData(uint64_t handle) noexcept;
    const BufferData* getData(uint64_t handle) const noexcept;
    VkDevice device() const noexcept;
    VkPhysicalDevice physicalDevice() const noexcept;
    void init(VkDevice dev, VkPhysicalDevice phys) noexcept;
    void purge_all() noexcept;

    // make_* helpers — DECLARED HERE
    inline uint64_t make_64M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
    inline uint64_t make_128M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
    inline uint64_t make_256M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
    inline uint64_t make_512M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
    inline uint64_t make_1G(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
    inline uint64_t make_2G(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
    inline uint64_t make_4G(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;
    inline uint64_t make_8G(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept;

    inline uint64_t makeScratch64M(VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    inline uint64_t makeScratch128M(VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    inline uint64_t makeScratch256M(VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    inline uint64_t makeScratch512M(VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    inline uint64_t makeScratch1G(VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    inline uint64_t makeScratch2G(VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;

    inline uint64_t makeStaging64M() noexcept;
    inline uint64_t makeStaging128M() noexcept;
    inline uint64_t makeStaging256M() noexcept;
    inline uint64_t makeStaging512M() noexcept;
    inline uint64_t makeStaging1G() noexcept;

    inline uint64_t makeUniform64M() noexcept;
    inline uint64_t makeUniform128M() noexcept;
    inline uint64_t makeUniform256M() noexcept;
    inline uint64_t makeUniform512M() noexcept;
    inline uint64_t makeUniform1G() noexcept;

    inline uint64_t makeDynamic64M() noexcept;
    inline uint64_t makeDynamic128M() noexcept;
    inline uint64_t makeDynamic256M() noexcept;

private:
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, BufferData> map_;
    std::atomic<uint64_t> counter_{0};
    VkDevice device_{VK_NULL_HANDLE};
    VkPhysicalDevice physDev_{VK_NULL_HANDLE};
    uint64_t scratch512M_{0};
    uint64_t scratch1G_{0};
    uint64_t scratch2G_{0};
    uint64_t obfuscate(uint64_t raw) const noexcept;
    uint64_t deobfuscate(uint64_t obf) const noexcept;
};

// ──────────────────────────────────────────────────────────────────────────────
// Handle<T> — RAII BOSS
// ──────────────────────────────────────────────────────────────────────────────
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
        if (raw == 0) return nullptr;
        return std::bit_cast<T>(deobfuscate(raw));
    }
    T operator*() const noexcept { return get(); }

    void reset() noexcept {
        if (raw) {
            T h = get();
            if (destroyer && device) {
                constexpr size_t threshold = 16 * 1024 * 1024;
                if (size >= threshold) {
                    LOG_DEBUG_CAT("Houston", "Skipping shred for large allocation (%zuMB): %s", size/(1024*1024), tag.empty() ? "" : std::string(tag).c_str());
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
};

template<typename T, typename DestroyFn, typename... Args>
[[nodiscard]] inline auto MakeHandle(T h, VkDevice d, DestroyFn del, Args&&... args) {
    return Handle<T>(h, d, del, std::forward<Args>(args)...);
}
template<typename T, typename... Args>
[[nodiscard]] inline auto MakeHandle(T h, VkDevice d, Args&&... args) {
    return Handle<T>(h, d, nullptr, std::forward<Args>(args)...);
}

// ──────────────────────────────────────────────────────────────────────────────
// GLOBALS
// ──────────────────────────────────────────────────────────────────────────────
extern std::unique_ptr<VulkanRenderer> g_vulkanRenderer;
extern std::shared_ptr<Context> g_context;

extern Handle<VkSwapchainKHR> g_swapchain;
extern std::vector<VkImage> g_swapchainImages;
extern std::vector<Handle<VkImageView>> g_swapchainImageViews;
extern VkFormat g_swapchainFormat;
extern VkExtent2D g_swapchainExtent;

extern Handle<VkAccelerationStructureKHR> g_blas;
extern Handle<VkAccelerationStructureKHR> g_tlas;
extern uint64_t g_instanceBufferId;
extern VkDeviceSize g_tlasSize;

// ──────────────────────────────────────────────────────────────────────────────
// AMOURANTH & NICK — PERSONALITY COLORS
// ──────────────────────────────────────────────────────────────────────────────
inline constexpr std::string_view AMOURANTH_COLOR = "\033[1;38;5;208m";  // FIERY_ORANGE
inline constexpr std::string_view NICK_COLOR      = "\033[1;38;5;220m";  // COSMIC_GOLD

// ──────────────────────────────────────────────────────────────────────────────
// AmouranthMessage
// ──────────────────────────────────────────────────────────────────────────────
struct AmouranthMessage {
    enum class Type { INIT_RENDERER, HANDLE_RESIZE, SHUTDOWN, RENDER_FRAME, RECREATE_SWAPCHAIN, BUILD_BLAS, BUILD_TLAS, CUSTOM };
    Type type;
    int width = 0, height = 0;
    const Camera* camera = nullptr;
    float deltaTime = 0.0f;
    std::function<void()> custom;
    uint64_t vertexBuf = 0, indexBuf = 0;
    uint32_t vertexCount = 0, indexCount = 0;
    std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>> instances;

    AmouranthMessage(Type t) : type(t) {}
    AmouranthMessage(Type t, int w, int h) : type(t), width(w), height(h) {}
    AmouranthMessage(Type t, const Camera* cam, float dt) : type(t), camera(cam), deltaTime(dt) {}
    AmouranthMessage(std::function<void()> fn) : type(Type::CUSTOM), custom(std::move(fn)) {}
    AmouranthMessage(Type t, uint64_t vbuf, uint64_t ibuf, uint32_t vc, uint32_t ic)
        : type(t), vertexBuf(vbuf), indexBuf(ibuf), vertexCount(vc), indexCount(ic) {}
    AmouranthMessage(Type t, std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>> insts)
        : type(t), instances(std::move(insts)) {}
};

// ──────────────────────────────────────────────────────────────────────────────
// NICK — DECLARATIONS
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] inline VulkanRenderer& getRenderer();
inline void initRenderer(int w, int h);
inline void handleResize(int w, int h);
inline void renderFrame(const Camera& camera, float deltaTime) noexcept;
inline void shutdown() noexcept;

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL SWAPCHAIN — DECLARATIONS
// ──────────────────────────────────────────────────────────────────────────────
inline void createSwapchain(VkInstance inst, VkPhysicalDevice phys, VkDevice dev,
                            VkSurfaceKHR surf, uint32_t w, uint32_t h);
inline void recreateSwapchain(uint32_t w, uint32_t h) noexcept;

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL LAS — DECLARATIONS
// ──────────────────────────────────────────────────────────────────────────────
inline void buildBLAS(uint64_t vertexBuf, uint64_t indexBuf, uint32_t vertexCount, uint32_t indexCount) noexcept;
inline void buildTLAS(const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances) noexcept;

// ──────────────────────────────────────────────────────────────────────────────
// RAII AutoBuffer + Macros
// ──────────────────────────────────────────────────────────────────────────────
#define BUFFER(handle) uint64_t handle = 0ULL

#define BUFFER_CREATE(handle, size, usage, props, tag) \
    do { (handle) = UltraLowLevelBufferTracker::get().create((size), (usage), (props), (tag)); } while (0)

#define BUFFER_MAP(h, ptr) \
    do { (ptr) = nullptr; auto* d = UltraLowLevelBufferTracker::get().getData((h)); \
         if (d) { void* p{}; if (vkMapMemory(UltraLowLevelBufferTracker::get().device(), d->memory, 0, d->size, 0, &p) == VK_SUCCESS) (ptr) = p; } } while (0)

#define BUFFER_UNMAP(h) \
    do { auto* d = UltraLowLevelBufferTracker::get().getData((h)); if (d) vkUnmapMemory(UltraLowLevelBufferTracker::get().device(), d->memory); } while (0)

#define BUFFER_DESTROY(handle) \
    do { UltraLowLevelBufferTracker::get().destroy((handle)); } while (0)

#define RAW_BUFFER(handle) \
    (UltraLowLevelBufferTracker::get().getData((handle)) ? UltraLowLevelBufferTracker::get().getData((handle))->buffer : VK_NULL_HANDLE)

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

// ──────────────────────────────────────────────────────────────────────────────
// Includes — AFTER Handle<T> and Macros
// ──────────────────────────────────────────────────────────────────────────────
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

// ──────────────────────────────────────────────────────────────────────────────
// Memory Literals — GLOBAL
// ──────────────────────────────────────────────────────────────────────────────
#define USE_POWER_OF_TWO_LITERALS
#ifdef USE_POWER_OF_TWO_LITERALS
    constexpr uint64_t operator"" _KB(unsigned long long v) noexcept { return v << 10; }
    constexpr uint64_t operator"" _MB(unsigned long long v) noexcept { return v << 20; }
    constexpr uint64_t operator"" _GB(unsigned long long v) noexcept { return v << 30; }
    constexpr uint64_t operator"" _TB(unsigned long long v) noexcept { return v << 40; }
#else
    constexpr uint64_t operator"" _KB(unsigned long long v) noexcept { return v * 1000ULL; }
    constexpr uint64_t operator"" _MB(unsigned long long v) noexcept { return v * 1000000ULL; }
    constexpr uint64_t operator"" _GB(unsigned long long v) noexcept { return v * 1000000000ULL; }
    constexpr uint64_t operator"" _TB(unsigned long long v) noexcept { return v * 1000000000000ULL; }
#endif

// ──────────────────────────────────────────────────────────────────────────────
// Global Cleanup
// ──────────────────────────────────────────────────────────────────────────────
inline void cleanupAll() noexcept;

// ──────────────────────────────────────────────────────────────────────────────
// POWER-OF-TWO SIZES (UNIQUE, NO DUPLICATES)
// ──────────────────────────────────────────────────────────────────────────────
constexpr VkDeviceSize SIZE_64MB  =  64_MB;
constexpr VkDeviceSize SIZE_128MB = 128_MB;
constexpr VkDeviceSize SIZE_256MB = 256_MB;
constexpr VkDeviceSize SIZE_420MB = 420_MB;
constexpr VkDeviceSize SIZE_512MB = 512_MB;
constexpr VkDeviceSize SIZE_1GB   =   1_GB;
constexpr VkDeviceSize SIZE_2GB   =   2_GB;
constexpr VkDeviceSize SIZE_4GB   =   4_GB;
constexpr VkDeviceSize SIZE_8GB   =   8_GB;

// ────────────────────── CORE MAKE HELPERS — IMPLEMENTED AT BOTTOM ──────────────────────
inline uint64_t UltraLowLevelBufferTracker::make_64M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept {
    return create(SIZE_64MB,  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "64M");
}
inline uint64_t UltraLowLevelBufferTracker::make_128M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept {
    return create(SIZE_128MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "128M");
}
inline uint64_t UltraLowLevelBufferTracker::make_256M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept {
    return create(SIZE_256MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "256M");
}
inline uint64_t UltraLowLevelBufferTracker::make_512M(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept {
    return create(SIZE_512MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "512M");
}
inline uint64_t UltraLowLevelBufferTracker::make_1G(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept {
    return create(SIZE_1GB,   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "1G");
}
inline uint64_t UltraLowLevelBufferTracker::make_2G(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept {
    return create(SIZE_2GB,   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "2G");
}
inline uint64_t UltraLowLevelBufferTracker::make_4G(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept {
    return create(SIZE_4GB,   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "4G");
}
inline uint64_t UltraLowLevelBufferTracker::make_8G(VkBufferUsageFlags extra, VkMemoryPropertyFlags props) noexcept {
    return create(SIZE_8GB,   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "8G");
}

// ────────────────────── FAST SCRATCH (DEVICE_LOCAL) ──────────────────────
inline uint64_t UltraLowLevelBufferTracker::makeScratch64M(VkMemoryPropertyFlags props) noexcept {
    return make_64M(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, props);
}
inline uint64_t UltraLowLevelBufferTracker::makeScratch128M(VkMemoryPropertyFlags props) noexcept {
    return make_128M(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, props);
}
inline uint64_t UltraLowLevelBufferTracker::makeScratch256M(VkMemoryPropertyFlags props) noexcept {
    return make_256M(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, props);
}
inline uint64_t UltraLowLevelBufferTracker::makeScratch512M(VkMemoryPropertyFlags props) noexcept {
    return make_512M(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, props);
}
inline uint64_t UltraLowLevelBufferTracker::makeScratch1G(VkMemoryPropertyFlags props) noexcept {
    return make_1G(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, props);
}
inline uint64_t UltraLowLevelBufferTracker::makeScratch2G(VkMemoryPropertyFlags props) noexcept {
    return make_2G(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, props);
}

// ────────────────────── STAGING (HOST_VISIBLE + COHERENT) ──────────────────────
inline uint64_t UltraLowLevelBufferTracker::makeStaging64M() noexcept {
    return create(SIZE_64MB, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Staging64M");
}
inline uint64_t UltraLowLevelBufferTracker::makeStaging128M() noexcept {
    return create(SIZE_128MB, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Staging128M");
}
inline uint64_t UltraLowLevelBufferTracker::makeStaging256M() noexcept {
    return create(SIZE_256MB, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Staging256M");
}
inline uint64_t UltraLowLevelBufferTracker::makeStaging512M() noexcept {
    return create(SIZE_512MB, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Staging512M");
}
inline uint64_t UltraLowLevelBufferTracker::makeStaging1G() noexcept {
    return create(SIZE_1GB, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Staging1G");
}

// ────────────────────── UNIFORM (HOST_VISIBLE, READ-ONLY ON GPU) ──────────────────────
inline uint64_t UltraLowLevelBufferTracker::makeUniform64M() noexcept {
    return create(SIZE_64MB, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Uniform64M");
}
inline uint64_t UltraLowLevelBufferTracker::makeUniform128M() noexcept {
    return create(SIZE_128MB, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Uniform128M");
}
inline uint64_t UltraLowLevelBufferTracker::makeUniform256M() noexcept {
    return create(SIZE_256MB, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Uniform256M");
}
inline uint64_t UltraLowLevelBufferTracker::makeUniform512M() noexcept {
    return create(SIZE_512MB, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Uniform512M");
}
inline uint64_t UltraLowLevelBufferTracker::makeUniform1G() noexcept {
    return create(SIZE_1GB, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Uniform1G");
}

// ────────────────────── DYNAMIC (HOST_VISIBLE + FREQUENT UPDATE) ──────────────────────
inline uint64_t UltraLowLevelBufferTracker::makeDynamic64M() noexcept {
    return create(SIZE_64MB, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Dynamic64M");
}
inline uint64_t UltraLowLevelBufferTracker::makeDynamic128M() noexcept {
    return create(SIZE_128MB, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Dynamic128M");
}
inline uint64_t UltraLowLevelBufferTracker::makeDynamic256M() noexcept {
    return create(SIZE_256MB, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Dynamic256M");
}

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================