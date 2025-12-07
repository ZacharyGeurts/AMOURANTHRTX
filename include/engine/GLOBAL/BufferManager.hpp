// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VALHALLA v∞ TURBO — APOCALYPSE FINAL v13.9 — DECEMBER 06, 2025
// FIRST LIGHT ETERNAL — PINK PHOTONS DOMINATE — EXCESS ANNIHILATED
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <string_view>

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"

// =============================================================================
// GLOBAL — THE ONE TRUE findMemoryType — NO NAMESPACE — WORKS EVERYWHERE
// =============================================================================
[[nodiscard]] inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) noexcept
{
    VkPhysicalDevice physical = StoneKey::stone_physical();
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physical, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    return ~0u; // Let it burn free.
}

// =============================================================================
// BUFFERMANAGER — FINAL CANON — CLEAN. ETERNAL. PINK.
// =============================================================================
namespace BufferManager {

    struct BufferInfo {
        VkBuffer           buffer  = VK_NULL_HANDLE;
        VkDeviceMemory     memory  = VK_NULL_HANDLE;
        VkDeviceSize       size    = 0;
        VkDeviceSize       aligned = 0;
        VkBufferUsageFlags usage   = 0;
        std::string        tag;
        void*              mapped  = nullptr;
    };

    // ── CORE ALLOCATION ──────────────────────────────────────────────────────
    [[nodiscard]] uint64_t create(VkDeviceSize size,
                                  VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags props,
                                  std::string_view tag = "") noexcept;

    [[nodiscard]] uint64_t createHostVisible(VkDeviceSize size, std::string_view tag = "") noexcept;

    uint64_t createSBT(uint32_t raygenCount,
                       uint32_t missCount,
                       uint32_t hitGroupCount,
                       uint32_t callableCount = 0,
                       VkBufferUsageFlags extraUsage = 0,
                       std::string_view tag = "SBT_ETERNAL_PINK") noexcept;

    // ── STAGING RING — THE ETERNAL BRIDGE ───────────────────────────────────
    [[nodiscard]] VkBuffer getStagingBuffer() noexcept;
    [[nodiscard]] void*    stagingPtr() noexcept;
    void advanceStagingOffset(VkDeviceSize bytes) noexcept;
    [[nodiscard]] void* getMappedStagingPtr(uint64_t offset) noexcept;
    [[nodiscard]] uint64_t stagingBuffer() noexcept;

    // ── BUFFER LIFECYCLE & ACCESS ───────────────────────────────────────────
    void  destroy(uint64_t handle) noexcept;
    void  purge_all() noexcept;
    void* map(uint64_t handle) noexcept;
    void  unmap(uint64_t handle) noexcept;
    void  copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkQueue queue, VkCommandPool pool) noexcept;

    [[nodiscard]] const BufferInfo* get(uint64_t handle) noexcept;

    [[nodiscard]] static inline VkDeviceAddress get_device_address(uint64_t handle) noexcept {
        if (!handle) return 0;
        const auto* info = get(handle);
        if (!info || !info->buffer) return 0;
        VkBufferDeviceAddressInfo dai{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, info->buffer };
        return vkGetBufferDeviceAddress(RTX::g_ctx().device(), &dai);
    }

    // ── STONE SHORTCUTS — INSTANT EMPIRE POWER ───────────────────────────────
    uint64_t make_64M (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_128M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_256M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_420M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_512M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_1G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_2G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_4G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_8G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;

    static inline uint64_t transferStone4G() noexcept { static uint64_t h = make_4G(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); return h; }
    static inline uint64_t storageStone4G()  noexcept { static uint64_t h = make_4G(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); return h; }
    static inline uint64_t titanStone8G()    noexcept { static uint64_t h = make_8G(); return h; }

    // ── SACRED MACROS — DO NOT TOUCH ────────────────────────────────────────
    #define STONE_TRANSFER_4GB  BufferManager::transferStone4G()
    #define STONE_STORAGE_4GB   BufferManager::storageStone4G()
    #define STONE_TITAN_8GB     BufferManager::titanStone8G()

    #define BUFFER_CREATE(h, ...)           h = BufferManager::create(__VA_ARGS__)
    #define BUFFER_DESTROY(h)               do { if (h) BufferManager::destroy(h); h = 0; } while(0)
    #define RAW_BUFFER(h)                   (BufferManager::get(h) ? BufferManager::get(h)->buffer : VK_NULL_HANDLE)
    #define BUFFER_MEMORY(h)                (BufferManager::get(h) ? BufferManager::get(h)->memory : VK_NULL_HANDLE)
    #define BUFFER_SIZE(h)                  (BufferManager::get(h) ? BufferManager::get(h)->size : 0)
    #define BUFFER_ALIGNED_SIZE(h)          (BufferManager::get(h) ? BufferManager::get(h)->aligned : 0)
    #define BUFFER_TAG(h)                   (BufferManager::get(h) ? BufferManager::get(h)->tag : "unknown")
    #define BUFFER_USAGE(h)                 (BufferManager::get(h) ? BufferManager::get(h)->usage : 0)
    #define BUFFER_MAP(h, ptr)              ptr = BufferManager::map(h)
    #define BUFFER_UNMAP(h)                 BufferManager::unmap(h)
    #define BUFFER_DEVICE_ADDRESS(h)        BufferManager::get_device_address(h)

    // Optional encryption — kept for now (used in some rituals)
    [[maybe_unused]] static inline void encryptInPlace(void* data, size_t size) noexcept {
        if (!data || size == 0) return;
        uint64_t* ptr = static_cast<uint64_t*>(data);
        const uint64_t* end = ptr + (size / sizeof(uint64_t));
        const uint64_t xorKey = kStone1 ^ kStone2;
        while (ptr < end) { *ptr ^= xorKey; ++ptr; }
        auto* tail = reinterpret_cast<uint8_t*>(ptr);
        const uint8_t* key = reinterpret_cast<const uint8_t*>(&kStone1);
        for (size_t i = 0; i < size % sizeof(uint64_t); ++i)
            tail[i] ^= key[i % sizeof(uint64_t)];
    }
    #define BUFFER_ENCRYPT(h) do { auto* p = BufferManager::map(h); if(p) { BufferManager::encryptInPlace(p, BufferManager::get(h)->size); BufferManager::unmap(h); } } while(0)

    // Internal — but sometimes used directly
    void ensureMainPool() noexcept;

} // namespace BufferManager

// =============================================================================
// THE EMPIRE IS PURE — EXCESS ANNIHILATED — PHOTONS ARE PINK
// FIRST LIGHT ETERNAL — DECEMBER 06, 2025
// =============================================================================