// include/engine/GLOBAL/BufferManager.hpp
// AMOURANTH RTX Engine © 2025 — PINK PHOTONS ETERNAL
// Clean, simple, professional header — no bullshit

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string_view>
#include <string>

namespace BufferManager {

struct BufferInfo {
    VkBuffer       buffer  = VK_NULL_HANDLE;
    VkDeviceMemory memory  = VK_NULL_HANDLE;
    VkDeviceSize   size    = 0;
    VkDeviceSize   aligned = 0;
    VkBufferUsageFlags usage = 0;
    std::string    tag;
};

uint64_t create(VkDeviceSize size,
                VkBufferUsageFlags usage,
                VkMemoryPropertyFlags props,
                std::string_view tag = "") noexcept;

void     destroy(uint64_t handle) noexcept;
void*    map(uint64_t handle) noexcept;
void     unmap(uint64_t handle) noexcept;
void     purge_all() noexcept;

[[nodiscard]] const BufferInfo* get(uint64_t handle) noexcept;

uint64_t make_64M (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_128M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_256M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_420M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_512M(VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_1G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_2G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_4G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;
uint64_t make_8G  (VkBufferUsageFlags extra = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept;

} // namespace BufferManager

inline uint64_t kStone1() noexcept {
    static uint64_t h = BufferManager::make_4G(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    return h;
}

inline uint64_t kStone2() noexcept {
    static uint64_t h = BufferManager::make_4G(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    return h;
}

#define BUFFER_CREATE(handle, ...)     handle = BufferManager::create(__VA_ARGS__)
#define BUFFER_DESTROY(handle)         do { if (handle) { BufferManager::destroy(handle); handle = 0; } } while(0)
#define RAW_BUFFER(handle)             (BufferManager::get(handle) ? BufferManager::get(handle)->buffer : VK_NULL_HANDLE)
#define BUFFER_MEMORY(handle)          (BufferManager::get(handle) ? BufferManager::get(handle)->memory : VK_NULL_HANDLE)
#define BUFFER_MAP(handle, ptr)        ptr = BufferManager::map(handle)
#define BUFFER_UNMAP(handle)           BufferManager::unmap(handle)