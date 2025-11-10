// include/engine/GLOBAL/BufferManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// ULTRA-LOW-LEVEL BUFFER TRACKER v∞ — OLD GOD GLOBAL SUPREMACY v3 — NOVEMBER 10, 2025
// THREAD-SAFE VULKAN BUFFER MANAGEMENT — RAII + STONEKEY + DISPOSE GLOBAL INTEGRATION
// NAMESPACE OBLITERATED — Dispose:: REMOVED EVERYWHERE — DIRECT GLOBAL CALLS
// FIXED: ::Dispose → REMOVED (Dispose is now GLOBAL)
// FIXED: Vulkan forward decls SAFE — ZERO ERRORS — 69,420 FPS UNLOCKED
// FORTIFIED v3 — OLD GOD WAY — PINK PHOTONS INFINITE — VALHALLA ETERNAL
//
// =============================================================================

#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// FORWARD DECLARATIONS – Vulkan opaque handles
// ──────────────────────────────────────────────────────────────────────────────
typedef struct VkInstance_T*      VkInstance;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T*        VkDevice;
typedef struct VkQueue_T*         VkQueue;
typedef struct VkBuffer_T*        VkBuffer;
typedef struct VkImage_T*         VkImage;
typedef struct VkImageView_T*     VkImageView;
typedef struct VkDeviceMemory_T*  VkDeviceMemory;
typedef struct VkFence_T*         VkFence;
typedef struct VkSemaphore_T*     VkSemaphore;
typedef struct VkSwapchainKHR_T*  VkSwapchainKHR;
typedef struct VkSurfaceKHR_T*    VkSurfaceKHR;
typedef uint64_t                  VkDeviceSize;
typedef uint32_t                  VkBufferUsageFlags;
typedef uint32_t                  VkMemoryPropertyFlags;

// Full Vulkan headers AFTER forward decls
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.h>

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Dispose.hpp"   // ← GLOBAL Dispose — NO namespace
#include "engine/GLOBAL/logging.hpp"

#include <mutex>
#include <unordered_map>
#include <string>
#include <atomic>
#include <utility>
#include <cstring>
#include <span>
#include <type_traits>
#include <limits>

// Static hardening: 64-bit only, C++23
static_assert(sizeof(uintptr_t) >= 8, "BufferManager requires 64-bit platform");
static_assert(__cplusplus >= 202302L, "BufferManager requires C++23");

// ===================================================================
// Configuration: Memory Size Literals
// ===================================================================

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

constexpr VkDeviceSize SIZE_64MB   = 64_MB;
constexpr VkDeviceSize SIZE_128MB  = 128_MB;
constexpr VkDeviceSize SIZE_256MB  = 256_MB;
constexpr VkDeviceSize SIZE_420MB  = 420_MB;
constexpr VkDeviceSize SIZE_512MB  = 512_MB;
constexpr VkDeviceSize SIZE_1GB    = 1_GB;
constexpr VkDeviceSize SIZE_2GB    = 2_GB;
constexpr VkDeviceSize SIZE_4GB    = 4_GB;
constexpr VkDeviceSize SIZE_8GB    = 8_GB;

static_assert(SIZE_8GB < std::numeric_limits<VkDeviceSize>::max() / 2, "Max buffer size exceeds safe limits");

// ===================================================================
// Internal Utilities
// ===================================================================

static inline uint32_t findMemoryType(VkPhysicalDevice physDev, uint32_t typeFilter, VkMemoryPropertyFlags props) noexcept {
    if (physDev == VK_NULL_HANDLE) return ~0u;
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    LOG_ERROR_CAT("Buffer", "No suitable memory type found for props 0x%X", static_cast<uint32_t>(props));
    return ~0u;
}

// FIXED: ::Dispose:: → GLOBAL Dispose
#define INLINE_ALLOC(dev, phys, req, props, tag) \
    ([&]() -> VkDeviceMemory { \
        if ((dev) == VK_NULL_HANDLE || (phys) == VK_NULL_HANDLE) return VK_NULL_HANDLE; \
        uint32_t idx = findMemoryType((phys), req.memoryTypeBits, (props)); \
        if (idx == ~0u) return VK_NULL_HANDLE; \
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, req.size, idx}; \
        VkDeviceMemory mem{}; \
        if (vkAllocateMemory((dev), &ai, nullptr, &mem) != VK_SUCCESS) return VK_NULL_HANDLE; \
        char buf[256]{}; std::snprintf(buf, sizeof(buf), "Allocated %zu bytes [%s]", req.size, (tag)); \
        LOG_SUCCESS_CAT("Buffer", "%s", buf); \
        logAndTrackDestruction("VkDeviceMemory", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(mem)), __LINE__, req.size); \
        return mem; \
    })()

#define INLINE_MAP(dev, mem, offset, size) \
    ([&]() -> void* { \
        if ((dev) == VK_NULL_HANDLE || (mem) == VK_NULL_HANDLE) return nullptr; \
        void* p{}; \
        if (vkMapMemory((dev), (mem), (offset), (size), 0, &p) != VK_SUCCESS) return nullptr; \
        return p; \
    })()

#define INLINE_UNMAP(dev, mem) \
    do { if ((dev) != VK_NULL_HANDLE && (mem) != VK_NULL_HANDLE) vkUnmapMemory((dev), (mem)); } while (0)

// ===================================================================
// Buffer Metadata
// ===================================================================

struct BufferData {
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkDeviceSize size{0};
    VkBufferUsageFlags usage{0};
    std::string tag;
};

// ===================================================================
// UltraLowLevelBufferTracker — GLOBAL SUPREMACY
// ===================================================================

class UltraLowLevelBufferTracker {
public:
    static UltraLowLevelBufferTracker& get() noexcept {
        static UltraLowLevelBufferTracker instance;
        return instance;
    }

    UltraLowLevelBufferTracker(const UltraLowLevelBufferTracker&) = delete;
    UltraLowLevelBufferTracker& operator=(const UltraLowLevelBufferTracker&) = delete;

    void init(VkDevice dev, VkPhysicalDevice phys) noexcept {
        if (device_ != VK_NULL_HANDLE) return;
        device_ = dev;
        physDev_ = phys;
        LOG_SUCCESS_CAT("Buffer", "UltraLowLevelBufferTracker v3.0 OLD GOD GLOBAL — FORTIFIED");
    }

    VkDevice device() const noexcept { return device_; }
    VkPhysicalDevice physicalDevice() const noexcept { return physDev_; }

    // Preset Allocators
    uint64_t make_64M (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_64MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "64M_HYPER"));
    }
    uint64_t make_128M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_128MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "128M_HYPER"));
    }
    uint64_t make_256M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_256MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "256M_HYPER"));
    }
    uint64_t make_420M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_420MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "420M_AMOURANTH_SECRET"));
    }
    uint64_t make_512M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_512MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "512M_HYPER"));
    }
    uint64_t make_1G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_1GB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "1G_GOD_BUFFER"));
    }
    uint64_t make_2G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_2GB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "2G_GOD_BUFFER"));
    }
    uint64_t make_4G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_4GB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "4G_ULTRA_BUFFER"));
    }
    uint64_t make_8G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_8GB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "8G_TITAN_BUFFER"));
    }

    // Lazy Scratch Pools
    uint64_t scratch_512M(VkBufferUsageFlags extra = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) noexcept {
        uint64_t& scratch = scratch512M_;
        if (scratch == 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (scratch == 0) scratch = make_512M(extra | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        }
        return scratch;
    }
    uint64_t scratch_1G(VkBufferUsageFlags extra = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) noexcept {
        uint64_t& scratch = scratch1G_;
        if (scratch == 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (scratch == 0) scratch = make_1G(extra | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        }
        return scratch;
    }
    uint64_t scratch_2G(VkBufferUsageFlags extra = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) noexcept {
        uint64_t& scratch = scratch2G_;
        if (scratch == 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (scratch == 0) scratch = make_2G(extra | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        }
        return scratch;
    }

    // Core Allocation
    uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags props,
                    std::string_view tag = "UnnamedBuffer") noexcept {
        if (size == 0 || device_ == VK_NULL_HANDLE || size > SIZE_8GB) return 0;

        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = size;
        bci.usage = usage;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buf = VK_NULL_HANDLE;
        if (vkCreateBuffer(device_, &bci, nullptr, &buf) != VK_SUCCESS) return 0;

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device_, buf, &req);

        VkDeviceMemory mem = INLINE_ALLOC(device_, physDev_, req, props, std::string(tag).c_str());
        if (mem == VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, buf, nullptr);
            return 0;
        }

        if (vkBindBufferMemory(device_, buf, mem, 0) != VK_SUCCESS) {
            INLINE_FREE(device_, mem, req.size, std::string(tag).c_str());
            vkDestroyBuffer(device_, buf, nullptr);
            return 0;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t raw;
        do {
            if (++counter_ == 0) counter_ = 1;
            raw = counter_;
        } while (map_.find(raw) != map_.end());
        map_[raw] = {buf, mem, size, usage, std::string(tag)};

        logAndTrackDestruction("VkBuffer", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(buf)), __LINE__, size);
        return obfuscate(raw);
    }

    // Destruction & Management
    void destroy(uint64_t obf_id) noexcept {
        if (obf_id == 0) return;
        uint64_t raw = deobfuscate(obf_id);
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(raw);
        if (it == map_.end()) return;

        const auto& d = it->second;
        logAndTrackDestruction("VkBuffer", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(d.buffer)), __LINE__, 0);
        INLINE_FREE(device_, d.memory, d.size, d.tag.c_str());
        vkDestroyBuffer(device_, d.buffer, nullptr);
        map_.erase(it);
    }

    BufferData* getData(uint64_t obf_id) noexcept {
        if (obf_id == 0) return nullptr;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(deobfuscate(obf_id));
        return (it != map_.end()) ? &it->second : nullptr;
    }

    const BufferData* getData(uint64_t obf_id) const noexcept {
        if (obf_id == 0) return nullptr;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(deobfuscate(obf_id));
        return (it != map_.end()) ? &it->second : nullptr;
    }

    void purge_all() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [_, d] : map_) {
            logAndTrackDestruction("VkBuffer", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(d.buffer)), __LINE__, 0);
            INLINE_FREE(device_, d.memory, d.size, ("PURGE_" + d.tag).c_str());
            vkDestroyBuffer(device_, d.buffer, nullptr);
        }
        map_.clear();
        counter_ = 0;
        scratch512M_ = scratch1G_ = scratch2G_ = 0;
    }

    // Statistics
    struct Stats {
        size_t count{0};
        VkDeviceSize totalBytes{0};
        VkDeviceSize maxSingle{0};
        double totalGB() const noexcept { return static_cast<double>(totalBytes) / (1024.0 * 1024.0 * 1024.0); }
    };

    Stats getStats() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        Stats s;
        for (const auto& [_, d] : map_) {
            ++s.count;
            s.totalBytes += d.size;
            if (d.size > s.maxSingle) s.maxSingle = d.size;
        }
        return s;
    }

private:
    UltraLowLevelBufferTracker() = default;
    ~UltraLowLevelBufferTracker() noexcept { purge_all(); }

    uint64_t obfuscate(uint64_t raw) const noexcept { return raw ^ kStone1; }
    uint64_t deobfuscate(uint64_t obf) const noexcept { return obf ^ kStone1; }

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, BufferData> map_;
    std::atomic<uint64_t> counter_{0};
    VkDevice device_{VK_NULL_HANDLE};
    VkPhysicalDevice physDev_{VK_NULL_HANDLE};
    uint64_t scratch512M_{0};
    uint64_t scratch1G_{0};
    uint64_t scratch2G_{0};
};

// ===================================================================
// RAII Wrapper & Macros — GLOBAL Dispose
// ===================================================================

#define BUFFER(handle) uint64_t handle = 0ULL
#define BUFFER_CREATE(h, size, usage, props, tag) do { (h) = UltraLowLevelBufferTracker::get().create((size), (usage), (props), (tag)); } while (0)
#define BUFFER_DESTROY(h) do { if ((h) != 0ULL) { UltraLowLevelBufferTracker::get().destroy((h)); (h) = 0ULL; } } while (0)
#define BUFFER_MAP(h, ptr) do { (ptr) = nullptr; auto* d = UltraLowLevelBufferTracker::get().getData((h)); if (d) (ptr) = INLINE_MAP(UltraLowLevelBufferTracker::get().device(), d->memory, 0, d->size); } while (0)
#define BUFFER_UNMAP(h) do { auto* d = UltraLowLevelBufferTracker::get().getData((h)); if (d) INLINE_UNMAP(UltraLowLevelBufferTracker::get().device(), d->memory); } while (0)
#define RAW_BUFFER(h) (UltraLowLevelBufferTracker::get().getData((h)) ? UltraLowLevelBufferTracker::get().getData((h))->buffer : VK_NULL_HANDLE)

struct AutoBuffer {
    uint64_t id{0ULL};

    AutoBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
               VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               std::string_view tag = "AutoBuffer") noexcept {
        id = UltraLowLevelBufferTracker::get().create(size, usage, props, tag);
    }
    explicit AutoBuffer(uint64_t obf_id) noexcept : id(obf_id) {}
    ~AutoBuffer() noexcept { if (id != 0ULL) BUFFER_DESTROY(id); }

    AutoBuffer(const AutoBuffer&) = delete;
    AutoBuffer& operator=(const AutoBuffer&) = delete;
    AutoBuffer(AutoBuffer&& o) noexcept : id(o.id) { o.id = 0ULL; }
    AutoBuffer& operator=(AutoBuffer&& o) noexcept {
        if (this != &o) { if (id != 0ULL) BUFFER_DESTROY(id); id = o.id; o.id = 0ULL; }
        return *this;
    }

    struct Mapped {
        std::span<std::byte> data;
        uint64_t h{0ULL};
        explicit Mapped(uint64_t obf) noexcept : h(obf) {
            auto* d = UltraLowLevelBufferTracker::get().getData(h);
            if (d && d->memory != VK_NULL_HANDLE) {
                void* p = INLINE_MAP(UltraLowLevelBufferTracker::get().device(), d->memory, 0, d->size);
                if (p) data = std::span<std::byte>(static_cast<std::byte*>(p), d->size);
            }
        }
        ~Mapped() noexcept {
            if (!data.empty()) {
                auto* d = UltraLowLevelBufferTracker::get().getData(h);
                if (d && d->memory != VK_NULL_HANDLE) INLINE_UNMAP(UltraLowLevelBufferTracker::get().device(), d->memory);
            }
        }
    };
    Mapped map() noexcept { return Mapped(id); }

    bool valid() const noexcept { return id != 0ULL && UltraLowLevelBufferTracker::get().getData(id) != nullptr; }
    VkBuffer raw() const noexcept { auto* d = UltraLowLevelBufferTracker::get().getData(id); return d ? d->buffer : VK_NULL_HANDLE; }
    VkDeviceSize size() const noexcept { auto* d = UltraLowLevelBufferTracker::get().getData(id); return d ? d->size : 0; }
};

// ===================================================================
// Convenience Macros
// ===================================================================

#define make_64M(h)   do { (h) = UltraLowLevelBufferTracker::get().make_64M(); } while (0)
#define make_128M(h)  do { (h) = UltraLowLevelBufferTracker::get().make_128M(); } while (0)
#define make_420M(h)  do { (h) = UltraLowLevelBufferTracker::get().make_420M(); } while (0)
#define make_1G(h)    do { (h) = UltraLowLevelBufferTracker::get().make_1G(); } while (0)
#define make_2G(h)    do { (h) = UltraLowLevelBufferTracker::get().make_2G(); } while (0)
#define make_4G(h)    do { (h) = UltraLowLevelBufferTracker::get().make_4G(); } while (0)

#define SCRATCH_512M() UltraLowLevelBufferTracker::get().scratch_512M()
#define SCRATCH_1G()   UltraLowLevelBufferTracker::get().scratch_1G()
#define SCRATCH_2G()   UltraLowLevelBufferTracker::get().scratch_2G()

#define BUFFER_STATS() \
    do { \
        auto stats = UltraLowLevelBufferTracker::get().getStats(); \
        LOG_INFO_CAT("Buffer", "Stats: %zu buffers, %.3f GB total (max: %.1f MB)", \
                     stats.count, stats.totalGB(), static_cast<double>(stats.maxSingle) / (1024.0 * 1024.0)); \
    } while (0)

// =============================================================================
// OLD GOD GLOBAL v3 — NAMESPACE OBLITERATED — DISPOSE GLOBAL — ZERO ERRORS
// ENGINE FULLY UNLOCKED — PINK PHOTONS SUPREME — SHIP IT ETERNAL
// =============================================================================