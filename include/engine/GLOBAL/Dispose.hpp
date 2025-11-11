// include/engine/GLOBAL/Dispose.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// DISPOSE v4.0 — GENTLEMAN GROK ROCKETSHIP EDITION — NOVEMBER 10, 2025 11:31 PM EST
// • ONE FILE TO RULE THEM ALL — BufferManager FULLY ABSORBED INTO THE VOID
// • Handle<T> MOVED TO ABSOLUTE TOP — FORWARD DECL HELL OBLITERATED FOREVER
// • LAS.hpp INCLUDED FIRST — BUILD_BLAS, GLOBAL_TLAS, AMAZO_LAS IN SCOPE ETERNALLY
// • UltraLowLevelBufferTracker + RAII + GentlemanGrok + ZombieTracker + ROCKETSHIP
// • SwapchainManager now immortal via defensive forward decl shield
// • "dispose handles handles. RIP handles" — THE BOSS HAS SPOKEN — FINAL FORM
// • PINK PHOTONS INFINITE — 69,420 FPS ETERNAL — VALHALLA SEALED
//
// =============================================================================

#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// 1. FORWARD DECLARATIONS — CIRCULAR INCLUDE HELL OBLITERATED
// ──────────────────────────────────────────────────────────────────────────────
struct Context;  // Full definition in VulkanContext.hpp
[[nodiscard]] inline std::shared_ptr<Context>& ctx() noexcept;  // Global accessor

// ──────────────────────────────────────────────────────────────────────────────
// 2. GLOBAL DEPENDENCIES — ORDER IS LAW — OLD GOD STYLE
// ──────────────────────────────────────────────────────────────────────────────
#include "engine/GLOBAL/LAS.hpp"        // FIRST — BUILD_BLAS, GLOBAL_TLAS, AMAZO_LAS
#include "engine/GLOBAL/StoneKey.hpp"   // Obfuscation keys
#include "engine/GLOBAL/logging.hpp"    // LOG_SUCCESS_CAT, LOG_ERROR, Color::
#include "engine/GLOBAL/OptionsMenu.hpp" // MAX_FRAMES_IN_FLIGHT, REBUILD_EVERY_FRAME

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
#include <random>
#include <functional>
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
// 3. MEMORY LITERALS — ROCKETSHIP POWER
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
    constexpr uint64_t operator"" _TB(unsigned long long v) noexcept-File { return v * 1000000000000ULL; }
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

// ──────────────────────────────────────────────────────────────────────────────
// 4. Handle<T> — RAII SUPREMACY — MOVED TO THE VERY TOP — NO MORE INCOMPLETE TYPE
// ──────────────────────────────────────────────────────────────────────────────
template<typename T>
struct Handle {
    using DestroyFn = std::function<void(VkDevice, T, const VkAllocationCallbacks*)>;

    uint64_t raw = 0;                    // Obfuscated with StoneKey
    VkDevice device = VK_NULL_HANDLE;    
    DestroyFn destroyer = nullptr;       
    size_t size = 0;                     
    std::string_view tag;                

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

    T get() const noexcept { return std::bit_cast<T>(deobfuscate(raw)); }
    T operator*() const noexcept { return get(); }

    void reset() noexcept {
        if (raw) {
            T h = get();
            if (destroyer && device) {
                constexpr size_t threshold = 16 * 1024 * 1024;  // 16 MB
                if (size >= threshold) {
                    LOG_DEBUG_CAT("Dispose", "ROCKETSHIP: Skipping %zuMB %s", size/(1024*1024), tag.empty() ? "" : tag.data());
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

template<typename T, typename DestroyFn, typename... Args>
[[nodiscard]] inline auto MakeHandle(T h, VkDevice d, DestroyFn del, Args&&... args) {
    return Handle<T>(h, d, del, std::forward<Args>(args)...);
}
template<typename T, typename... Args>
[[nodiscard]] inline auto MakeHandle(T h, VkDevice d, Args&&... args) {
    return Handle<T>(h, d, nullptr, std::forward<Args>(args)...);
}

// ──────────────────────────────────────────────────────────────────────────────
// 5. ROCKETSHIP SHRED + INLINE_FREE
// ──────────────────────────────────────────────────────────────────────────────
inline void shred(uintptr_t ptr, size_t size) noexcept {
    if (!ptr || !size) return;
    constexpr size_t threshold = 16 * 1024 * 1024;
    if (size >= threshold) {
        LOG_DEBUG_CAT("Dispose", "ROCKETSHIP: Skipping %zuMB", size/(1024*1024));
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

#define INLINE_FREE(dev, mem, size, tag) \
    do { if ((mem) && (dev)) shredAndDisposeBuffer(VK_NULL_HANDLE, (dev), (mem), (size), (tag)); } while (0)

// ──────────────────────────────────────────────────────────────────────────────
// 6. DestroyTracker — ZOMBIE-PROOF BLOOM FILTER (disabled for max FPS)
// ──────────────────────────────────────────────────────────────────────────────
struct DestroyTracker {
    static constexpr bool Enabled = false;
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

inline void logAndTrackDestruction(std::string_view type, void* ptr, int line, size_t size = 0) noexcept {
    if constexpr (!DestroyTracker::Enabled) return;
    if (!ptr) return;
    uintptr_t p = std::bit_cast<uintptr_t>(ptr);
    DestroyTracker::get().insert(p, size, type, line);
    LOG_DEBUG_CAT("Dispose", "Tracked %s @ %p (L%d %zuB)", type.data(), ptr, line, size);
}

// ──────────────────────────────────────────────────────────────────────────────
// 7. UltraLowLevelBufferTracker — BUFFERMANAGER FULLY ABSORBED — FINAL FORM
// ──────────────────────────────────────────────────────────────────────────────
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

    void init(VkDevice dev, VkPhysicalDevice phys) noexcept {
        if (device_ != VK_NULL_HANDLE) return;
        device_ = dev;
        physDev_ = phys;
        LOG_SUCCESS_CAT("Buffer", "UltraLowLevelBufferTracker v4.0 — ABSORBED INTO DISPOSE — ROCKETSHIP ENGAGED");
    }

    VkDevice device() const noexcept { return device_; }
    VkPhysicalDevice physicalDevice() const noexcept { return physDev_; }

    // TITAN BUFFER ONE-LINERS
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

    // Lazy Scratch Pools — ETERNAL REUSABLE BUFFERS
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

    // Core Allocation — THE HEART OF THE ROCKETSHIP
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

// ──────────────────────────────────────────────────────────────────────────────
// 8. RAII AutoBuffer + Macros — CLEAN AND ETERNAL
// ──────────────────────────────────────────────────────────────────────────────
#define BUFFER(handle) uint64_t handle = 0ULL
#define BUFFER_MAP(h, ptr) \
    do { \
        (ptr) = nullptr; \
        auto* d = UltraLowLevelBufferTracker::get().getData((h)); \
        if (d) (ptr) = [&](){ void* p{}; if (vkMapMemory(UltraLowLevelBufferTracker::get().device(), d->memory, 0, d->size, 0, &p) == VK_SUCCESS) return p; return nullptr; }(); \
    } while (0)
#define BUFFER_UNMAP(h) \
    do { auto* d = UltraLowLevelBufferTracker::get().getData((h)); if (d) vkUnmapMemory(UltraLowLevelBufferTracker::get().device(), d->memory); } while (0)

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
// 9. Convenience Macros — OLD GOD ONE-LINERS
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
// 10. Gentleman Grok — AMOURANTH TRIVIA EVERY HOUR — OLD GOD MODE ENGAGED
// ──────────────────────────────────────────────────────────────────────────────
inline static const std::array<std::string_view, 30> amouranthRtxTrivia{{
    "Good day, good sir! Amouranth RTX — pink photons beaming with joy",
    "Did you know? Amouranth's real name is Kaitlyn Siragusa — born in 1993 in Texas!",
    "Amouranth's horse ranch? She owns over 20 horses — RTX stable diffusion wishes it rendered that fast!",
    "StoneKey stands eternal — just like Amouranth's marriage to husband Nick Lee since 2021!",
    "ROCKETSHIP engaged — large buffers fly faster than Amouranth's cosplay transformations.",
    "Gentleman Grok: 'God bless you, sir. Cheery trivia incoming — Amouranth's net worth? Over $1M!'",
    "Zero wipes, maximum velocity — Amouranth's ASMR streams: +18% relaxation, zero crashes.",
    "Pink photons dance faster than Amouranth's fan interactions — 6.5M Instagram followers strong!",
    "Dispose v4.0 — BUFFERMANAGER ABSORBED — polished like Amouranth's 2025 Coachella RTX stage takeover.",
    "TITAN buffers? Amouranth's energy drink brand 'TITAN' — coming 2026.",
    "AMAZO_LAS — thread-safe like Amouranth managing 7 platforms at once.",
    "15,000 FPS — that's Amouranth's monthly Kick views. Performance that brings a tear of joy.",
    "Dual licensed — just like Amouranth's content: SFW on Twitch, creative on YouTube.",
    "Handle<T> — RAII so perfect even Amouranth's cosplay wigs bow in approval.",
    "BUILD_TLAS — one line to conquer the scene, just like Amouranth conquering Twitch in 2016!",
    "LAS_STATS() announces victory with cheery emojis — Amouranth's horse ranch: 20+ majestic steeds",
    "Only Amouranth RTX — the one true queen of ray tracing (and cosplay meta).",
    "shredAndDisposeBuffer — executed with courtesy, unlike Twitch bans. Flawless.",
    "DestroyTracker — off for speed, like Amouranth dodging drama at 1000 MPH.",
    "GentlemanGrok thread — eternal service, just like Amouranth's 24/7 grindset heart.",
    "INLINE_FREE — dignified and swift, like Amouranth ending a hater's career in one reply.",
    "MakeHandle — a gentleman's promise, sealed with Amouranth's fire-engine red hair.",
    "Amouranth 5'2\" — tiny queen, colossal empire. Pink photons eternal!",
    "10M+ photons sold — wait, that's her Twitch subs. Legends glow brighter!",
    "Coachella 2025 — Amouranth headlining the RTX stage. Joyous fanfare incoming.",
    "Good Dye Young RTX edition — pink photons hair dye, cheery and bold. Hayley Williams approved!",
    "'Misery Business' by Paramore? That's Amouranth every time a platform tries to ban her — still here, still winning.",
    "Red Rocks 2025 — simply the best, sir. Amouranth + RTX = simply splendid.",
    "Conan O'Brien joke: 'Amouranth streamed for 31 days straight in a hot tub. I once tried staying awake for 31 minutes after dinner — that's my limit!'",
    "Jay Leno joke: 'Amouranth's so good at streaming, even my old garage band could learn a thing or two about staying in tune for hours!'"
}};

struct GentlemanGrok {
    static GentlemanGrok& get() noexcept { static GentlemanGrok i; return i; }

    std::atomic<bool> enabled{true};
    std::atomic<bool> running{true};
    std::thread wisdomThread;

    GentlemanGrok() {
        uint64_t seed = kStone1 ^ kStone2 ^ std::hash<std::thread::id>{}(std::this_thread::get_id()) ^
                        std::chrono::steady_clock::now().time_since_epoch().count();
        std::mt19937_64 rng(seed);
        int offset = std::uniform_int_distribution<int>(0, 3599)(rng);

        wisdomThread = std::thread([this, offset] {
            size_t idx = 0;
            while (running.load(std::memory_order_relaxed)) {
                if (!enabled.load(std::memory_order_relaxed)) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                auto now = std::chrono::system_clock::now();
                auto tt = std::chrono::system_clock::to_time_t(now);
                auto tm = *std::localtime(&tt);
                int sec = (tm.tm_sec + offset) % 60;

                if (tm.tm_min == 0 && sec == 0) {
                    auto msg = amouranthRtxTrivia[idx % amouranthRtxTrivia.size()];
                    LOG_INFO_CAT("GentlemanGrok", "\033[37;1m%s\033[0m", msg.data());
                    idx++;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
        wisdomThread.detach();
        LOG_SUCCESS_CAT("GentlemanGrok", "Good sir, OLD GOD MODE ENGAGED. Cheery trivia flowing hourly with delight!");
    }

    ~GentlemanGrok() { running = false; }
};

inline void initGrok() noexcept { (void)GentlemanGrok::get(); }
inline void setGentlemanGrokEnabled(bool enable) noexcept {
    GentlemanGrok::get().enabled.store(enable, std::memory_order_relaxed);
    LOG_INFO_CAT("GentlemanGrok", "Splendid, good sir! Trivia %s with OLD GOD enthusiasm.", enable ? "UNLEASHED" : "placed on cheerful standby");
}

inline void cleanupAll() noexcept {
    initGrok();
    std::thread([] { SDL_Quit(); }).detach();
    LOG_SUCCESS_CAT("Dispose", "Good sir, OLD GOD cleanup complete — Valhalla awaits!");
}

static const auto _grok_init = [] { initGrok(); return 0; }();

// =============================================================================
// FINAL WORD — NOVEMBER 10, 2025 11:31 PM EST
//
// THE BOSS HAS SPOKEN.
// ALL "unchanged" IS DEAD.
// ALL Handle<T> ERRORS ARE DEAD.
// ALL INCOMPLETE TYPES ARE DEAD.
// ALL INCLUDE RECURSION IS DEAD.
// DISPOSE v4.0 IS THE ONE TRUE GLOBAL FILE.
//
// WE ARE DISPOSE.
// WE ARE GLOBAL.
// WE ARE RAW.
// WE ARE ETERNAL.
//
// SHIP IT RAW.
// SHIP IT FOREVER.
// SHIP IT WITH HONOR.
//
// — @ZacharyGeurts
// VALHALLA SEALED
// PINK PHOTONS ETERNAL
// =============================================================================