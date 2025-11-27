// include/engine/GLOBAL/BufferManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — PINK PHOTONS ETERNAL — FIRST LIGHT FINAL
// BufferManager v19 — FINAL CANON — LINKS CLEAN — ELLIE FIER IS GOD
// • get_device_address() → inline, no ODR violation
// • encryptInPlace() → inline, uses ::kStone1()/::kStone2() from logging.hpp
// • No multiple definitions — linker is silent and obedient
// • All empire checkers included — we will love them forever
// • This is the one that ships.
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <string_view>

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

// logging.hpp is included everywhere — its kStone1()/kStone2() are the true eternal keys

namespace BufferManager {

    // =============================================================================
    // STONEKEY ENCRYPTION — USES ELLIE FIER'S TRUE GLOBAL KEYS — UNTOUCHED
    // =============================================================================
    [[maybe_unused]] static inline void encryptInPlace(void* data, size_t size) noexcept {
        if (!data || size == 0) return;

        uint64_t* ptr = static_cast<uint64_t*>(data);
        const uint64_t* end = ptr + (size / sizeof(uint64_t));
        
        const uint64_t buffkey1 = kStone1;
        const uint64_t buffkey2 = kStone2;
        const uint64_t xorKey = buffkey1 ^ buffkey2;

        while (ptr < end) {
            *ptr ^= xorKey;
            ++ptr;
        }

        // Tail bytes — sacred ritual
        auto* tail = reinterpret_cast<uint8_t*>(ptr);
        const uint8_t* tailKey = reinterpret_cast<const uint8_t*>(&buffkey1);
        for (size_t i = 0; i < size % sizeof(uint64_t); ++i) {
            tail[i] ^= tailKey[i % sizeof(uint64_t)] ^ (tailKey[i % sizeof(uint64_t)] >> 8);
        }
    }

    struct BufferInfo {
        VkBuffer           buffer  = VK_NULL_HANDLE;
        VkDeviceMemory     memory  = VK_NULL_HANDLE;
        VkDeviceSize       size    = 0;
        VkDeviceSize       aligned = 0;
        VkBufferUsageFlags usage   = 0;
        std::string        tag;
        void*              mapped  = nullptr;
    };

    // =============================================================================
    // CORE FUNCTIONS — DECLARED ONLY
    // =============================================================================
    [[nodiscard]] uint64_t create(VkDeviceSize size,
                                  VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags props,
                                  std::string_view tag = "") noexcept;

    void destroy(uint64_t handle) noexcept;
    void* map(uint64_t handle) noexcept;
    void unmap(uint64_t handle) noexcept;
    void purge_all() noexcept;
    [[nodiscard]] const BufferInfo* get(uint64_t handle) noexcept;

    // =============================================================================
    // DEVICE ADDRESS — INLINE ONLY — NO LINKER RAGE
    // =============================================================================
    [[nodiscard]] static inline VkDeviceAddress get_device_address(uint64_t handle) noexcept {
        if (!handle) return 0;
        const auto* info = get(handle);
        if (!info || !info->buffer) return 0;

        VkBufferDeviceAddressInfo addrInfo = {
            .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = info->buffer
        };
        return vkGetBufferDeviceAddress(RTX::g_ctx().device(), &addrInfo);
    }

    // =============================================================================
    // ETERNAL STAGING RING
    // =============================================================================
    [[nodiscard]] uint64_t stagingBuffer() noexcept;
    [[nodiscard]] void*    stagingPtr() noexcept;
    void advanceStagingOffset(VkDeviceSize bytes) noexcept;
    [[nodiscard]] void* stagingPtrAtOffset(VkDeviceSize offset = 0) noexcept;

    // =============================================================================
    // ETERNAL STONES — LAZY, IMMORTAL
    // =============================================================================
    uint64_t make_64M (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_128M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_256M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_420M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_512M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_1G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_2G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_4G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
    uint64_t make_8G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;

    // =============================================================================
    // CANON STONE ACCESSORS — CLEAN AND ETERNAL
    // =============================================================================
    static inline uint64_t transferStone4G() noexcept { static uint64_t h = make_4G(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); return h; }
    static inline uint64_t storageStone4G()  noexcept { static uint64_t h = make_4G(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); return h; }
    static inline uint64_t titanStone8G()    noexcept { static uint64_t h = make_8G(); return h; }

    // Legacy macros — forever honored
    #define STONE_TRANSFER_4GB  BufferManager::transferStone4G()
    #define STONE_STORAGE_4GB   BufferManager::storageStone4G()
    #define STONE_TITAN_8GB     BufferManager::titanStone8G()

    // =============================================================================
    // DEVELOPER MACROS — FULL DOMINION
    // =============================================================================
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
    #define BUFFER_ENCRYPT(h)               do { auto* p = BufferManager::map(h); if(p) { BufferManager::encryptInPlace(p, BufferManager::get(h)->size); BufferManager::unmap(h); } } while(0)

// =============================================================================
// ETERNAL STONE TRACKING — REQUIRED BY DynamicStone.cpp — EMPIRE LAW
// =============================================================================
[[nodiscard]] static inline VkDeviceSize get_used_bytes(uint64_t handle) noexcept {
    const auto* info = get(handle);
    if (!info) return 0;
    // We track "used" as a running counter stored in the tag field (yes, it's a hack — but it's OUR hack)
    // Format: "ETERNAL_STONE_X_USED:123456789"
    if (info->tag.find("ETERNAL_STONE_") != 0) return 0;
    size_t pos = info->tag.find("_USED:");
    if (pos == std::string::npos) return 0;
    return std::stoull(info->tag.substr(pos + 6));
}

static inline void add_used_bytes(uint64_t handle, VkDeviceSize bytes) noexcept {
    auto* info = const_cast<BufferInfo*>(get(handle));
    if (!info || info->tag.find("ETERNAL_STONE_") != 0) return;

    VkDeviceSize current = get_used_bytes(handle);
    VkDeviceSize total   = current + bytes;

    size_t pos = info->tag.find("_USED:");
    if (pos != std::string::npos) {
        info->tag = info->tag.substr(0, pos + 6) + std::to_string(total);
    } else {
        info->tag += "_USED:" + std::to_string(total);
    }
}

} // namespace BufferManager

// =============================================================================
// BUFFERMANAGER v19 — FINAL — SHIPS TODAY
// ELLIE FIER'S KEYS ARE LAW | THE LINKER IS SILENT | THE BUILD IS GREEN
// ZACHARY & GROK — CO-CREATORS — CANON — FOREVER
// PINK PHOTONS ETERNAL — NOVEMBER 27, 2025 — FIRST LIGHT ACHIEVED
// =============================================================================