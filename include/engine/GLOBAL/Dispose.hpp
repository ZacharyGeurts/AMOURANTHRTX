// include/engine/GLOBAL/Dispose.hpp
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

#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// Forward Declarations
// ──────────────────────────────────────────────────────────────────────────────
struct Context;  // Full definition in VulkanContext.hpp
[[nodiscard]] inline std::shared_ptr<Context>& ctx() noexcept;  // Global accessor

// ──────────────────────────────────────────────────────────────────────────────
// Includes
// ──────────────────────────────────────────────────────────────────────────────
#include "engine/GLOBAL/LAS.hpp"        // Acceleration structures: BUILD_BLAS, GLOBAL_TLAS, AMAZO_LAS
#include "engine/GLOBAL/StoneKey.hpp"   // Obfuscation keys
#include "engine/GLOBAL/logging.hpp"    // Logging macros: LOG_SUCCESS_CAT, LOG_ERROR, Color::
#include "engine/GLOBAL/OptionsMenu.hpp" // Configuration: MAX_FRAMES_IN_FLIGHT, REBUILD_EVERY_FRAME

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include <atomic>
#include <array>
#include <bitset>
#include <bit>
#include <string_view>
#include <cstring>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <SDL3/SDL.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <string>
#include <utility>
#include <span>
#include <limits>

// LAS global macros — now always in scope
class AMAZO_LAS;
#define GLOBAL_TLAS()          (AMAZO_LAS::get().getTLAS())
#define GLOBAL_BLAS()          (AMAZO_LAS::get().getBLAS())

// ──────────────────────────────────────────────────────────────────────────────
// Memory Literals
// ──────────────────────────────────────────────────────────────────────────────
// Enable power-of-two literals for binary-aligned sizing (e.g., 1_MB = 1 << 20)
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

// Predefined buffer sizes
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

// ──────────────────────────────────────────────────────────────────────────────
// Handle<T> — RAII Wrapper for Vulkan Handles
// ──────────────────────────────────────────────────────────────────────────────
// Provides automatic resource cleanup with optional secure shredding for sensitive data.
// Obfuscates the raw handle using StoneKey for basic protection against casual inspection.
template<typename T>
struct Handle {
    using DestroyFn = std::function<void(VkDevice, T, const VkAllocationCallbacks*)>;

    uint64_t raw = 0;                    // Obfuscated raw handle
    VkDevice device = VK_NULL_HANDLE;    // Associated device
    DestroyFn destroyer = nullptr;       // Cleanup function
    size_t size = 0;                     // Resource size in bytes (for shredding)
    std::string_view tag;                // Debug tag

    Handle() noexcept = default;

    // Constructor for valid handles
    Handle(T h, VkDevice d, DestroyFn del = nullptr, size_t sz = 0, std::string_view t = "")
        : raw(obfuscate(std::bit_cast<uint64_t>(h))), device(d), destroyer(del), size(sz), tag(t)
    {
        if (h) logAndTrackDestruction(typeid(T).name(), reinterpret_cast<void*>(std::bit_cast<uintptr_t>(h)), __LINE__, size);
    }

    // Null constructor
    Handle(T h, std::nullptr_t) noexcept 
        : raw(obfuscate(std::bit_cast<uint64_t>(h))), device(VK_NULL_HANDLE), destroyer(nullptr), size(0), tag("")
    {
        if (h) logAndTrackDestruction(typeid(T).name(), reinterpret_cast<void*>(std::bit_cast<uintptr_t>(h)), __LINE__, 0);
    }

    // Move constructor
    Handle(Handle&& o) noexcept : raw(o.raw), device(o.device), destroyer(o.destroyer), size(o.size), tag(o.tag) {
        o.raw = 0; o.device = VK_NULL_HANDLE; o.destroyer = nullptr;
    }

    // Move assignment
    Handle& operator=(Handle&& o) noexcept {
        reset();
        raw = o.raw; device = o.device; destroyer = o.destroyer; size = o.size; tag = o.tag;
        o.raw = 0; o.device = VK_NULL_HANDLE; o.destroyer = nullptr;
        return *this;
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    // Null assignment
    Handle& operator=(std::nullptr_t) noexcept { reset(); return *this; }

    // Bool conversion
    explicit operator bool() const noexcept { return raw != 0; }

    // Accessors
    T get() const noexcept { return std::bit_cast<T>(deobfuscate(raw)); }
    T operator*() const noexcept { return get(); }

    // Reset and destroy
    void reset() noexcept {
        if (raw) {
            T h = get();
            if (destroyer && device) {
                constexpr size_t threshold = 16 * 1024 * 1024;  // Skip shredding for large allocations (>16MB)
                if (size >= threshold) {
                    LOG_DEBUG_CAT("Dispose", "Skipping shred for large allocation (%zuMB): %s", size/(1024*1024), tag.empty() ? "" : tag.data());
                } else if (h) {
                    shred(std::bit_cast<uintptr_t>(h), size);
                }
                destroyer(device, h, nullptr);
            }
            logAndTrackDestruction(tag.empty() ? typeid(T).name() : tag, reinterpret_cast<void*>(std::bit_cast<uintptr_t>(h)), __LINE__);
            raw = 0; device = VK_NULL_HANDLE; destroyer = nullptr;
        }
    }

    ~Handle() { reset(); }
};

// Factory functions for creating handles
template<typename T, typename DestroyFn, typename... Args>
[[nodiscard]] inline auto MakeHandle(T h, VkDevice d, DestroyFn del, Args&&... args) {
    return Handle<T>(h, d, del, std::forward<Args>(args)...);
}
template<typename T, typename... Args>
[[nodiscard]] inline auto MakeHandle(T h, VkDevice d, Args&&... args) {
    return Handle<T>(h, d, nullptr, std::forward<Args>(args)...);
}

// ──────────────────────────────────────────────────────────────────────────────
// Secure Shredding and Buffer Disposal
// ──────────────────────────────────────────────────────────────────────────────
// Overwrites memory with a rotating pattern before zeroing for security (e.g., against cold boot attacks).
// Skips large allocations to avoid performance overhead.
inline void shred(uintptr_t ptr, size_t size) noexcept {
    if (!ptr || !size) return;
    constexpr size_t threshold = 16 * 1024 * 1024;  // 16 MB threshold
    if (size >= threshold) {
        LOG_DEBUG_CAT("Dispose", "Skipping shred for large allocation (%zuMB)", size/(1024*1024));
        return;
    }
    auto* p = reinterpret_cast<void*>(ptr);
    uint64_t pat = 0xF1F1F1F1F1F1F1F1ULL ^ kStone1;
    for (size_t i = 0; i < size; i += 8) {
        std::memcpy(reinterpret_cast<char*>(p)+i, &pat, std::min<size_t>(8, size-i));
        pat = std::rotl(pat, 7) ^ kStone2;
    }
    std::memset(p, 0, size);
}

// Shred and dispose a buffer-memory pair
inline void shredAndDisposeBuffer(VkBuffer buf, VkDevice dev, VkDeviceMemory mem, VkDeviceSize sz, const char* tag = nullptr) noexcept {
    if (mem) {
        shred(std::bit_cast<uintptr_t>(mem), sz);
        vkFreeMemory(dev, mem, nullptr);
        logAndTrackDestruction("VkDeviceMemory", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(mem)), __LINE__, sz);
    }
    if (buf) {
        vkDestroyBuffer(dev, buf, nullptr);
        logAndTrackDestruction("VkBuffer", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(buf)), __LINE__, 0);
    }
    if (tag) LOG_INFO_CAT("Dispose", "Freed %s (%llu MB)", tag, sz / (1024*1024));
}

// Macro for inline buffer freeing
#define INLINE_FREE(dev, mem, size, tag) \
    do { if ((mem) && (dev)) shredAndDisposeBuffer(VK_NULL_HANDLE, (dev), (mem), (size), (tag)); } while (0)

// ──────────────────────────────────────────────────────────────────────────────
// DestroyTracker — Optional Double-Free Detection (Bloom Filter)
// ──────────────────────────────────────────────────────────────────────────────
// Disabled by default for performance. Uses a Bloom filter for fast probable detection of double-frees.
struct DestroyTracker {
    static constexpr bool Enabled = false;  // Enable for debugging
    static constexpr size_t Capacity = Enabled ? 1'048'576 : 1;

    struct Entry {
        std::atomic<uintptr_t> ptr{0};
        std::atomic<size_t>    size{0};
        std::string_view       type;
        int                    line{};
        std::atomic<bool>      destroyed{false};
    };

    static DestroyTracker& get() noexcept { static DestroyTracker t; return t; }

    std::bitset<Capacity * 8> bloom{};
    std::atomic<size_t>       head{0};
    std::array<Entry, Capacity> entries{};

    void insert(uintptr_t p, size_t s, std::string_view t, int l) noexcept {
        if constexpr (!Enabled) return;
        uintptr_t h1 = p ^ kStone1;
        uintptr_t h2 = (p * 0x517CC1B727220A95ULL) ^ kStone2;
        bloom.set(h1 % (Capacity * 8));
        bloom.set(h2 % (Capacity * 8));
        auto i = head.fetch_add(1, std::memory_order_relaxed) % Capacity;
        auto& e = entries[i];
        e.ptr.store(p, std::memory_order_release);
        e.size.store(s, std::memory_order_release);
        e.type = t;
        e.line = l;
        e.destroyed.store(false, std::memory_order_release);
    }

    static bool isDestroyed(const void* ptr) noexcept {
        if constexpr (!Enabled) return false;
        if (!ptr) return true;
        uintptr_t p = std::bit_cast<uintptr_t>(ptr);
        uintptr_t h1 = p ^ kStone1;
        uintptr_t h2 = (p * 0x517CC1B727220A95ULL) ^ kStone2;
        auto& tracker = get();
        if (!tracker.bloom.test(h1 % (Capacity * 8)) || !tracker.bloom.test(h2 % (Capacity * 8))) return false;
        for (size_t i = 0; i < Capacity; ++i) {
            auto& e = tracker.entries[i];
            if (e.ptr.load(std::memory_order_acquire) == p) return e.destroyed.load(std::memory_order_acquire);
        }
        return false;
    }

    static void markDestroyed(const void* ptr) noexcept {
        if constexpr (!Enabled) return;
        if (!ptr) return;
        uintptr_t p = std::bit_cast<uintptr_t>(ptr);
        auto& tracker = get();
        for (size_t i = 0; i < Capacity; ++i) {
            auto& e = tracker.entries[i];
            if (e.ptr.load(std::memory_order_acquire) == p) {
                e.destroyed.store(true, std::memory_order_release);
                return;
            }
        }
    }
};

// Logging wrapper for tracking destructions
inline void logAndTrackDestruction(std::string_view type, void* ptr, int line, size_t size = 0) noexcept {
    if constexpr (!DestroyTracker::Enabled) return;
    if (!ptr) return;
    uintptr_t p = std::bit_cast<uintptr_t>(ptr);
    DestroyTracker::get().insert(p, size, type, line);
    LOG_DEBUG_CAT("Dispose", "Tracked %s @ %p (L%d %zuB)", type.data(), ptr, line, size);
}

// ──────────────────────────────────────────────────────────────────────────────
// UltraLowLevelBufferTracker — Centralized Buffer Management
// ──────────────────────────────────────────────────────────────────────────────
// Singleton for creating, tracking, and destroying Vulkan buffers with associated memory.
// Supports obfuscated IDs, lazy scratch pools, and stats. Absorbs all prior BufferManager logic.
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

struct BufferData {
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkDeviceSize size{0};
    VkBufferUsageFlags usage{0};
    std::string tag;
};

class UltraLowLevelBufferTracker {
public:
    static UltraLowLevelBufferTracker& get() noexcept {
        static UltraLowLevelBufferTracker instance;
        return instance;
    }

    UltraLowLevelBufferTracker(const UltraLowLevelBufferTracker&) = delete;
    UltraLowLevelBufferTracker& operator=(const UltraLowLevelBufferTracker&) = delete;

    // Initialize with device and physical device
    void init(VkDevice dev, VkPhysicalDevice phys) noexcept {
        if (device_ != VK_NULL_HANDLE) return;
        device_ = dev;
        physDev_ = phys;
        LOG_SUCCESS_CAT("Buffer", "UltraLowLevelBufferTracker initialized");
    }

    VkDevice device() const noexcept { return device_; }
    VkPhysicalDevice physicalDevice() const noexcept { return physDev_; }

    // Convenience methods for common buffer sizes (returns obfuscated ID)
    uint64_t make_64M (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_64MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "64M"));
    }
    uint64_t make_128M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_128MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "128M"));
    }
    uint64_t make_256M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_256MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "256M"));
    }
    uint64_t make_420M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_420MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "420M"));
    }
    uint64_t make_512M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_512MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "512M"));
    }
    uint64_t make_1G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_1GB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "1G"));
    }
    uint64_t make_2G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_2GB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "2G"));
    }
    uint64_t make_4G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_4GB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "4G"));
    }
    uint64_t make_8G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return obfuscate(create(SIZE_8GB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, props, "8G"));
    }

    // Lazy scratch pools for reusable temporary buffers
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

    // Core buffer creation (returns obfuscated ID)
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

        VkDeviceMemory mem = [&]() -> VkDeviceMemory {
            uint32_t idx = findMemoryType(physDev_, req.memoryTypeBits, props);
            if (idx == ~0u) return VK_NULL_HANDLE;
            VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, req.size, idx};
            VkDeviceMemory m{};
            if (vkAllocateMemory(device_, &ai, nullptr, &m) != VK_SUCCESS) return VK_NULL_HANDLE;
            char buf[256]{}; std::snprintf(buf, sizeof(buf), "Allocated %zu bytes [%s]", req.size, tag.data());
            LOG_SUCCESS_CAT("Buffer", "%s", buf);
            logAndTrackDestruction("VkDeviceMemory", reinterpret_cast<void*>(std::bit_cast<uintptr_t>(m)), __LINE__, req.size);
            return m;
        }();
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

    // Destroy by obfuscated ID
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

    // Get buffer data by obfuscated ID
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

    // Purge all tracked buffers
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

    // Statistics structure
    struct Stats {
        size_t count{0};
        VkDeviceSize totalBytes{0};
        VkDeviceSize maxSingle{0};
        double totalGB() const noexcept { return static_cast<double>(totalBytes) / (1024.0 * 1024.0 * 1024.0); }
    };

    // Get current stats
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

    // Obfuscation helpers (simple XOR with StoneKey)
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

// ──────────────────────────────────────────────────────────────────────────────
// RAII AutoBuffer and Mapping Macros
// ──────────────────────────────────────────────────────────────────────────────
#define BUFFER(handle) uint64_t handle = 0ULL

// Map buffer to host-visible pointer (assumes host-visible memory)
#define BUFFER_MAP(h, ptr) \
    do { \
        (ptr) = nullptr; \
        auto* d = UltraLowLevelBufferTracker::get().getData((h)); \
        if (d) (ptr) = [&](){ void* p{}; if (vkMapMemory(UltraLowLevelBufferTracker::get().device(), d->memory, 0, d->size, 0, &p) == VK_SUCCESS) return p; return nullptr; }(); \
    } while (0)

// Unmap buffer
#define BUFFER_UNMAP(h) \
    do { auto* d = UltraLowLevelBufferTracker::get().getData((h)); if (d) vkUnmapMemory(UltraLowLevelBufferTracker::get().device(), d->memory); } while (0)

// RAII wrapper for buffers
struct AutoBuffer {
    uint64_t id{0ULL};

    AutoBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
               VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               std::string_view tag = "AutoBuffer") noexcept {
        id = UltraLowLevelBufferTracker::get().create(size, usage, props, tag);
    }
    explicit AutoBuffer(uint64_t obf_id) noexcept : id(obf_id) {}
    ~AutoBuffer() noexcept { if (id != 0ULL) UltraLowLevelBufferTracker::get().destroy(id); }

    AutoBuffer(const AutoBuffer&) = delete;
    AutoBuffer& operator=(const AutoBuffer&) = delete;
    AutoBuffer(AutoBuffer&& o) noexcept : id(o.id) { o.id = 0ULL; }
    AutoBuffer& operator=(AutoBuffer&& o) noexcept {
        if (this != &o) { if (id != 0ULL) UltraLowLevelBufferTracker::get().destroy(id); id = o.id; o.id = 0ULL; }
        return *this;
    }

    // RAII mapped view (requires host-visible memory)
    struct Mapped {
        std::span<std::byte> data;
        uint64_t h{0ULL};
        explicit Mapped(uint64_t obf) noexcept : h(obf) {
            auto* d = UltraLowLevelBufferTracker::get().getData(h);
            if (d && d->memory != VK_NULL_HANDLE) {
                void* p{};
                if (vkMapMemory(UltraLowLevelBufferTracker::get().device(), d->memory, 0, d->size, 0, &p) == VK_SUCCESS) {
                    data = std::span<std::byte>(static_cast<std::byte*>(p), d->size);
                }
            }
        }
        ~Mapped() noexcept {
            if (!data.empty()) {
                auto* d = UltraLowLevelBufferTracker::get().getData(h);
                if (d && d->memory != VK_NULL_HANDLE) vkUnmapMemory(UltraLowLevelBufferTracker::get().device(), d->memory);
            }
        }
    };
    Mapped map() noexcept { return Mapped(id); }

    bool valid() const noexcept { return id != 0ULL && UltraLowLevelBufferTracker::get().getData(id) != nullptr; }
    VkBuffer raw() const noexcept { auto* d = UltraLowLevelBufferTracker::get().getData(id); return d ? d->buffer : VK_NULL_HANDLE; }
    VkDeviceSize size() const noexcept { auto* d = UltraLowLevelBufferTracker::get().getData(id); return d ? d->size : 0; }
};

// ──────────────────────────────────────────────────────────────────────────────
// Convenience Macros
// ──────────────────────────────────────────────────────────────────────────────
#define make_64M(h)   do { (h) = UltraLowLevelBufferTracker::get().make_64M(); } while (0)
#define make_128M(h)  do { (h) = UltraLowLevelBufferTracker::get().make_128M(); } while (0)
#define make_256M(h)  do { (h) = UltraLowLevelBufferTracker::get().make_256M(); } while (0)
#define make_420M(h)  do { (h) = UltraLowLevelBufferTracker::get().make_420M(); } while (0)
#define make_512M(h)  do { (h) = UltraLowLevelBufferTracker::get().make_512M(); } while (0)
#define make_1G(h)    do { (h) = UltraLowLevelBufferTracker::get().make_1G(); } while (0)
#define make_2G(h)    do { (h) = UltraLowLevelBufferTracker::get().make_2G(); } while (0)
#define make_4G(h)    do { (h) = UltraLowLevelBufferTracker::get().make_4G(); } while (0)
#define make_8G(h)    do { (h) = UltraLowLevelBufferTracker::get().make_8G(); } while (0)

#define SCRATCH_512M() UltraLowLevelBufferTracker::get().scratch_512M()
#define SCRATCH_1G()   UltraLowLevelBufferTracker::get().scratch_1G()
#define SCRATCH_2G()   UltraLowLevelBufferTracker::get().scratch_2G()

#define BUFFER_STATS() \
    do { \
        auto stats = UltraLowLevelBufferTracker::get().getStats(); \
        LOG_INFO_CAT("Buffer", "Stats: %zu buffers, %.3f GB total (max: %.1f MB)", \
                     stats.count, stats.totalGB(), static_cast<double>(stats.maxSingle) / (1024.0 * 1024.0)); \
    } while (0)

// ──────────────────────────────────────────────────────────────────────────────
// Global Cleanup
// ──────────────────────────────────────────────────────────────────────────────
inline void cleanupAll() noexcept {
    UltraLowLevelBufferTracker::get().purge_all();
    std::thread([] { SDL_Quit(); }).detach();
    LOG_SUCCESS_CAT("Dispose", "Global cleanup complete");
}

// Static initializer for cleanup registration (if needed)
static const auto _dispose_init = [] { atexit(cleanupAll); return 0; }();

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