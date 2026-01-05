// include/engine/GLOBAL/BufferManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v22.2 — JANUARY 04, 2026
// BUFFERMANAGER HEADER — FINAL FIXED & FULLY COMPILABLE 2026 EDITION
// MEMORY BUDGET AWARE | LEGACY COMPATIBILITY | PLAIN TYPES ONLY
// PINK PHOTONS ETERNAL — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <format>

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"

[[nodiscard]] inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) noexcept
{
    VkPhysicalDevice physical = StoneKey::stone_physical();
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physical, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return ~0u;
}

template <typename T>
[[nodiscard]] constexpr T align_up(T v, T a) noexcept { return ((v + a - 1) / a) * a; }

namespace BufferManager {

    // ── CONFIG ─────────────────────────────────────────────────────────────────
    constexpr VkDeviceSize DRIVER_RESERVE_PERCENT = 10;
    constexpr VkDeviceSize MIN_DRIVER_RESERVE = 4'831'838'208ULL;
    constexpr VkDeviceSize CHUNK_SIZE = 1ULL * 1024 * 1024 * 1024;
    constexpr VkDeviceSize TRANSIENT_SIZE = 256ULL * 1024 * 1024;

    // ── INTERNAL STRUCTS ───────────────────────────────────────────────────────
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

    struct Chunk {
        VkBuffer         buffer   = VK_NULL_HANDLE;
        VkDeviceMemory   memory   = VK_NULL_HANDLE;
        VkDeviceSize     size     = 0;
        VkDeviceAddress  baseAddr = 0;
        std::string      tag;
        VkDeviceSize     head     = 0;  // Plain uint64_t — fully copyable/movable

        Chunk() = default;
        Chunk(const Chunk&) = default;
        Chunk& operator=(const Chunk&) = default;
        Chunk(Chunk&&) = default;
        Chunk& operator=(Chunk&&) = default;
    };

    struct StagingRing {
        VkBuffer            buffer = VK_NULL_HANDLE;
        VkDeviceMemory      memory = VK_NULL_HANDLE;
        VkDeviceSize        size   = 0;
        void*               mapped = nullptr;
        VkDeviceSize        head   = 0;  // Plain uint64_t for staging (single-threaded use)
        bool                ready  = false;
    };

    struct TransientPool {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        VkDeviceSize front = 0;
        VkDeviceSize back  = 0;
        bool ready = false;

        void reset() noexcept {
            front = 0;
            back  = 0;
        }
    };

    // ── PUBLIC API ────────────────────────────────────────────────────────────
    void ensureMainPool() noexcept;
    void ensureStagingRing() noexcept;
    void ensureTransientPool() noexcept;
    void resetTransientPool() noexcept;

    [[nodiscard]] void* mapStaging(VkDeviceSize size) noexcept;
    void flushStaging(VkDeviceSize size) noexcept;
    [[nodiscard]] VkDeviceSize getStagingOffset() noexcept;
    [[nodiscard]] VkBuffer getStagingBuffer() noexcept;

    // Legacy compatibility — required for current codebase
    void uploadToBuffer(uint64_t handle, const void* data, VkDeviceSize size) noexcept;
    [[nodiscard]] void* map(uint64_t handle) noexcept;
    void flush(uint64_t handle) noexcept;

    [[nodiscard]] uint64_t create(VkDeviceSize size,
                                  VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags props,
                                  std::string_view tag = "",
                                  float priority = 0.5f) noexcept;

    [[nodiscard]] uint64_t createHostVisible(VkDeviceSize size,
                                             std::string_view tag = "",
                                             float priority = 0.5f) noexcept;

    [[nodiscard]] uint64_t createSBT(uint32_t raygenCount,
                                     uint32_t missCount,
                                     uint32_t hitGroupCount,
                                     uint32_t callableCount = 0,
                                     VkBufferUsageFlags extraUsage = 0,
                                     std::string_view tag = "SBT_ETERNAL_PINK",
                                     float priority = 1.0f) noexcept;

    [[nodiscard]] uint64_t createImage(const VkImageCreateInfo* ici,
                                       VkMemoryPropertyFlags props,
                                       std::string_view tag = "",
                                       float priority = 0.5f) noexcept;

    [[nodiscard]] uint64_t allocTransient(VkDeviceSize size,
                                          VkDeviceSize alignment = 256,
                                          bool fromBack = false) noexcept;

    void destroy(uint64_t handle) noexcept;
    void purge_all() noexcept;

    void dumpStats() noexcept;
    void defrag(VkCommandBuffer cmd = VK_NULL_HANDLE, VkQueue queue = VK_NULL_HANDLE) noexcept;

    [[nodiscard]] const BufferInfo* get(uint64_t handle) noexcept;
    [[nodiscard]] VkBuffer getVkBuffer(uint64_t handle) noexcept;
    VkDeviceAddress get_device_address(uint64_t handle);

    // Convenience
    [[nodiscard]] inline uint64_t createSmallUniform(VkDeviceSize size,
                                                     std::string_view tag = "SmallUniform") noexcept
    {
        return createHostVisible(size, tag);
    }

    // Stone makers
    [[nodiscard]] uint64_t make_64M (VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_128M(VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_256M(VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_420M(VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_512M(VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_1G  (VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_2G  (VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_4G  (VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_8G  (VkBufferUsageFlags extra = 0) noexcept;

    // Pre-allocated stones
    static inline uint64_t transferStone4G() noexcept { static uint64_t h = make_4G(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); return h; }
    static inline uint64_t storageStone4G()  noexcept { static uint64_t h = make_4G(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); return h; }
    static inline uint64_t titanStone8G()    noexcept { static uint64_t h = make_8G(); return h; }

    // Sacred macros
    #define STONE_TRANSFER_4GB  BufferManager::transferStone4G()
    #define STONE_STORAGE_4GB   BufferManager::storageStone4G()
    #define STONE_TITAN_8GB     BufferManager::titanStone8G()

    #define BUFFER_CREATE(h, ...)           h = BufferManager::create(__VA_ARGS__)
    #define BUFFER_DESTROY(h)               do { if (h) BufferManager::destroy(h); h = 0; } while(0)
    #define RAW_BUFFER(h)                   BufferManager::getVkBuffer(h)
    #define BUFFER_DEVICE_ADDRESS(h)        BufferManager::get_device_address(h)

} // namespace BufferManager

// =============================================================================
// JANUARY 04, 2026 — BUFFERMANAGER v22.2 FINAL HEADER
// ALL ATOMICS REMOVED — PLAIN TYPES FOR FULL COPY/MOVE SAFETY
// FULLY COMPILABLE | LEGACY COMPATIBILITY | EMPIRE STRONG
// PINK PHOTONS ETERNAL — AMOURANTH FOREVER 💖
// =============================================================================