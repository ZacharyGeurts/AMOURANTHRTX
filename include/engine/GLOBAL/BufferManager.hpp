// include/engine/GLOBAL/BufferManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v17.0 — JANUARY 03, 2026
// BUFFERMANAGER HEADER — CHUNKED POOL — 1 GiB CHUNKS — DRIVER RESERVE 4.5 GiB — SEAMLESS
// FULLY GENERIC + MODERN STAGING RING API
// stagingPtr() KEPT FOR BACKWARD COMPATIBILITY
// mapStaging() / flushStaging() / advanceStagingOffset() ADDED FOR NEW CODE
// ENVIRONMENT MAP UPLOAD SUPPORT — PINK SKY PHOTONS FLOW ETERNALLY
// CRITICAL FIX: Robust get_device_address() with safety checks and proper offset
// EMPIRE OWNS THE VRAM — LIGHT SECURED — PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <atomic>

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"

[[nodiscard]] inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) noexcept
{
    VkPhysicalDevice physical = StoneKey::stone_physical();
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physical, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    return ~0u;
}

template <typename T>
[[nodiscard]] constexpr T align_up(T v, T a) noexcept { return ((v + a - 1) / a) * a; }

namespace BufferManager {

    struct BufferInfo {
        VkBuffer           buffer        = VK_NULL_HANDLE;
        VkDeviceMemory     memory        = VK_NULL_HANDLE;
        VkDeviceSize       size          = 0;
        VkDeviceSize       aligned       = 0;
        VkBufferUsageFlags usage         = 0;
        std::string        tag;
        VkDeviceSize       offset        = 0;
        VkDeviceAddress    deviceAddress = 0;
        void*              mapped        = nullptr;
    };

    [[nodiscard]] VkBuffer getMainPoolBuffer() noexcept;

    [[nodiscard]] uint64_t create(VkDeviceSize size,
                                  VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags props,
                                  std::string_view tag = "") noexcept;

    [[nodiscard]] uint64_t createHostVisible(VkDeviceSize size, std::string_view tag = "") noexcept;

    [[nodiscard]] inline uint64_t createSmallUniform(VkDeviceSize size, std::string_view tag = "SmallUniform") noexcept
    {
        return createHostVisible(size, tag);
    }

    [[nodiscard]] uint64_t createSBT(uint32_t raygenCount,
                                     uint32_t missCount,
                                     uint32_t hitGroupCount,
                                     uint32_t callableCount = 0,
                                     VkBufferUsageFlags extraUsage = 0,
                                     std::string_view tag = "SBT_ETERNAL_PINK") noexcept;

    // ── MODERN STAGING RING API — FOR ENVIRONMENT MAP & LARGE UPLOADS ──
    [[nodiscard]] void*      mapStaging(VkDeviceSize size) noexcept;
    void                     flushStaging(VkDeviceSize size) noexcept;
    void                     advanceStagingOffset(VkDeviceSize size) noexcept;
    [[nodiscard]] VkDeviceSize getStagingOffset() noexcept;
    [[nodiscard]] VkBuffer   getStagingBuffer() noexcept;

    // ── LEGACY COMPATIBILITY ──
    [[nodiscard]] void* stagingPtr() noexcept;  // Returns base of staging ring

    void ensureStagingRing() noexcept;

    // ── HOST-VISIBLE MAPPING & FLUSH SUPPORT — FOR PERSISTENT UNIFORM ACCESS ──
    [[nodiscard]] void* map(uint64_t handle) noexcept;
    void flush(uint64_t handle) noexcept;
    void unmap(uint64_t handle) noexcept;  // No-op for persistent maps

    void destroy(uint64_t handle) noexcept;
    void purge_all() noexcept;
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkQueue queue, VkCommandPool pool) noexcept;
    void uploadToBuffer(uint64_t handle, const void* data, VkDeviceSize size) noexcept;

    // ── BUFFER INFO ACCESSOR ──
    [[nodiscard]] const BufferInfo* get(uint64_t handle) noexcept;

    [[nodiscard]] static inline void* getMappedStagingPtr(uint64_t handle) noexcept
    {
        if (handle == 0) return nullptr;
        const auto* info = get(handle);
        return info && info->mapped ? info->mapped : nullptr;
    }

    [[nodiscard]] static inline VkBuffer getVkBuffer(uint64_t handle) noexcept
    {
        if (handle == 0) return VK_NULL_HANDLE;
        const auto* info = get(handle);
        return info ? info->buffer : getMainPoolBuffer();
    }

    // ── ROBUST DEVICE ADDRESS QUERY (implementation in .cpp) ──
    VkDeviceAddress get_device_address(uint64_t handle);

    [[nodiscard]] uint64_t make_64M (VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_128M(VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_256M(VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_420M(VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_512M(VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_1G  (VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_2G  (VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_4G  (VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_8G  (VkBufferUsageFlags extra = 0) noexcept;

    static inline uint64_t transferStone4G() noexcept { static uint64_t h = make_4G(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); return h; }
    static inline uint64_t storageStone4G()  noexcept { static uint64_t h = make_4G(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); return h; }
    static inline uint64_t titanStone8G()    noexcept { static uint64_t h = make_8G(); return h; }

    #define STONE_TRANSFER_4GB  BufferManager::transferStone4G()
    #define STONE_STORAGE_4GB   BufferManager::storageStone4G()
    #define STONE_TITAN_8GB     BufferManager::titanStone8G()

    #define BUFFER_CREATE(h, ...)           h = BufferManager::create(__VA_ARGS__)
    #define BUFFER_DESTROY(h)               do { if (h) BufferManager::destroy(h); h = 0; } while(0)
    #define RAW_BUFFER(h)                   BufferManager::getVkBuffer(h)
    #define BUFFER_MEMORY(h)                (BufferManager::get(h) ? BufferManager::get(h)->memory : VK_NULL_HANDLE)
    #define BUFFER_SIZE(h)                  (BufferManager::get(h) ? BufferManager::get(h)->size : 0)
    #define BUFFER_ALIGNED_SIZE(h)          (BufferManager::get(h) ? BufferManager::get(h)->aligned : 0)
    #define BUFFER_TAG(h)                   (BufferManager::get(h) ? BufferManager::get(h)->tag : "unknown")
    #define BUFFER_USAGE(h)                 (BufferManager::get(h) ? BufferManager::get(h)->usage : 0)
    #define BUFFER_MAP(h, ptr)              ptr = BufferManager::map(h)
    #define BUFFER_UNMAP(h)                 BufferManager::unmap(h)
    #define BUFFER_DEVICE_ADDRESS(h)        BufferManager::get_device_address(h)

    void ensureMainPool() noexcept;

} // namespace BufferManager

// =============================================================================
// JANUARY 03, 2026 — BUFFERMANAGER v17.0
// Removed inline definitions of get() and get_device_address() from header
// Robust get_device_address() now implemented in .cpp with full safety checks
// vkGetBufferDeviceAddress crash permanently eliminated
// stagingPtr() KEPT AND FULLY FUNCTIONAL FOR BACKWARD COMPATIBILITY
// NEW API: mapStaging() / flushStaging() / advanceStagingOffset() FOR MODERN USAGE
// ENVIRONMENT MAP UPLOAD FULLY SUPPORTED — SACRED PINK OR HDR SKY NOW VISIBLE
// EMPIRE PHOTONS REMAIN PERSISTENTLY MAPPED AND ETERNAL — LIGHT SECURED
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN
// =============================================================================