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
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 28, 2025
// GLOBAL findMemoryType — NO NAMESPACE — WORKS IN EVERY .cpp — FINAL FORM
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <string_view>

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"

// =============================================================================
// GLOBAL — NO NAMESPACE — ONE FUNCTION TO RULE THEM ALL
// EVERY .cpp CAN NOW JUST CALL: findMemoryType(...)
// NO vkh:: NO stone_pipeline()-> NO BULLSHIT
// =============================================================================

// =============================================================================
// GLOBAL — NO NAMESPACE — THE ONE TRUE findMemoryType — WORKS EVERYWHERE
// =============================================================================

[[nodiscard]] inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) noexcept
{
    VkPhysicalDevice physical = StoneKey::stone_physical();

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physical, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return ~0u;  // Let it fly free. No guard. No noise.
}

// =============================================================================
// BUFFERMANAGER v19 — FINAL CANON — NOW USES GLOBAL findMemoryType
// =============================================================================

namespace BufferManager {

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

    [[nodiscard]] uint64_t create(VkDeviceSize size,
                                  VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags props,
                                  std::string_view tag = "") noexcept;

    void destroy(uint64_t handle) noexcept;
    void* map(uint64_t handle) noexcept;
    void unmap(uint64_t handle) noexcept;
    void purge_all() noexcept;
    [[nodiscard]] const BufferInfo* get(uint64_t handle) noexcept;

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

    [[nodiscard]] uint64_t stagingBuffer() noexcept;
    [[nodiscard]] void*    stagingPtr() noexcept;
    void advanceStagingOffset(VkDeviceSize bytes) noexcept;
    [[nodiscard]] void* stagingPtrAtOffset(VkDeviceSize offset = 0) noexcept;

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
    #define BUFFER_ENCRYPT(h)               do { auto* p = BufferManager::map(h); if(p) { BufferManager::encryptInPlace(p, BufferManager::get(h)->size); BufferManager::unmap(h); } } while(0)

    [[nodiscard]] static inline VkDeviceSize get_used_bytes(uint64_t handle) noexcept {
        const auto* info = get(handle);
        if (!info) return 0;
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
// GLOBAL findMemoryType — IN THIS HEADER — WORKS IN EVERY .cpp THAT INCLUDES IT
// NO NAMESPACE — NO STONE_PIPELINE — JUST CALL findMemoryType(...)
// THIS IS THE FINAL FORM — NOVEMBER 28, 2025 — FIRST LIGHT ETERNAL
// =============================================================================