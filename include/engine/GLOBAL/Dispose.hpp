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
// Dispose — RAII BOSS — HEART OF THE ENGINE — PINK PHOTONS ETERNAL
// • Owns Handle<T>, ctx(), UltraLowLevelBufferTracker, cleanupAll()
// • VulkanCore is SPINE — uses Dispose globals only
// • NO REDEFINITION — #pragma once + include guards
// • C++23, -Werror clean, zero leaks, Valhalla sealed
// • Handle<T> declared BEFORE ANY INCLUDES
// • SwapchainManager, VulkanCore, LAS — all obey Dispose
//
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

// ──────────────────────────────────────────────────────────────────────────────
// Forward Declarations — ctx() is eternal
// ──────────────────────────────────────────────────────────────────────────────
struct Context;
[[nodiscard]] inline std::shared_ptr<Context>& ctx() noexcept;

// ──────────────────────────────────────────────────────────────────────────────
// Handle<T> — RAII RAWR — ONLY DEFINED HERE — BEFORE ANY INCLUDES
// ──────────────────────────────────────────────────────────────────────────────
template<typename T>
struct Handle {
    using DestroyFn = std::function<void(VkDevice, T, const VkAllocationCallbacks*)>;

    uint64_t raw = 0;
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
                constexpr size_t threshold = 16 * 1024 * 1024;
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

// Factory
template<typename T, typename DestroyFn, typename... Args>
[[nodiscard]] inline auto MakeHandle(T h, VkDevice d, DestroyFn del, Args&&... args) {
    return Handle<T>(h, d, del, std::forward<Args>(args)...);
}
template<typename T, typename... Args>
[[nodiscard]] inline auto MakeHandle(T h, VkDevice d, Args&&... args) {
    return Handle<T>(h, d, nullptr, std::forward<Args>(args)...);
}

// ──────────────────────────────────────────────────────────────────────────────
// Secure Shredding
// ──────────────────────────────────────────────────────────────────────────────
inline void shred(uintptr_t ptr, size_t size) noexcept {
    if (!ptr || !size) return;
    constexpr size_t threshold = 16 * 1024 * 1024;
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

// ──────────────────────────────────────────────────────────────────────────────
// Includes — ORDER IS LAW — AFTER Handle<T>
// ──────────────────────────────────────────────────────────────────────────────
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/LAS.hpp"           // LAS macros — uses ctx(), no Handle needed here

// ──────────────────────────────────────────────────────────────────────────────
// Memory Literals
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
// UltraLowLevelBufferTracker — Dispose owns it
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
        LOG_SUCCESS_CAT("Buffer", "UltraLowLevelBufferTracker initialized");
    }

    VkDevice device() const noexcept { return device_; }
    VkPhysicalDevice physicalDevice() const noexcept { return physDev_; }

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
// RAII AutoBuffer + Macros
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
// Global Cleanup
// ──────────────────────────────────────────────────────────────────────────────
inline void cleanupAll() noexcept {
    UltraLowLevelBufferTracker::get().purge_all();
    std::thread([] { SDL_Quit(); }).detach();
    LOG_SUCCESS_CAT("Dispose", "Global cleanup complete");
}

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