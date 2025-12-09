// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VALHALLA v∞ TURBO — APOCALYPSE FINAL v13.9 — DECEMBER 08, 2025
// FIRST LIGHT ETERNAL — PINK PHOTONS DOMINATE — EXCESS ANNIHILATED
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"

// GLOBAL — THE ONE TRUE findMemoryType
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

// BUFFERMANAGER — FINAL CANON — CLEAN. ETERNAL. PINK.
namespace BufferManager {

    // Forward-declare the StagingRing struct (defined in .cpp)
    struct StagingRing;

    struct BufferInfo {
        VkBuffer           buffer  = VK_NULL_HANDLE;
        VkDeviceMemory     memory  = VK_NULL_HANDLE;
        VkDeviceSize       size    = 0;
        VkDeviceSize       aligned = 0;
        VkBufferUsageFlags usage   = 0;
        std::string        tag;
        void*              mapped  = nullptr;
        VkDeviceSize       offset  = 0;
    };

    // Forward accessor for the giant main pool buffer (defined in .cpp)
    [[nodiscard]] VkBuffer getMainPoolBuffer() noexcept;

    // Expose the eternal staging ring — required for the one true frame UBO
    extern StagingRing* g_stagingRing;

    // CORE ALLOCATION
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

    // STAGING RING — THE ETERNAL BRIDGE
    [[nodiscard]] VkBuffer getStagingBuffer() noexcept;
    void* stagingPtr() noexcept;
    void advanceStagingOffset(VkDeviceSize bytes) noexcept;
    [[nodiscard]] uint64_t stagingBuffer() noexcept;

    void ensureStagingRing() noexcept;   // Force creation of the eternal staging ring (idempotent)

    // BUFFER LIFECYCLE & ACCESS
    void destroy(uint64_t handle) noexcept;
    void purge_all() noexcept;
    void* map(uint64_t handle) noexcept;
    void unmap(uint64_t handle) noexcept;
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkQueue queue, VkCommandPool pool) noexcept;

    // INTERNAL STORAGE — RAW DOGGING STYLE
    extern std::unordered_map<uint64_t, BufferInfo> s_buffers;

    // PUBLIC ACCESS — CLEAN AND ETERNAL
    [[nodiscard]] static const BufferInfo* get(uint64_t handle) noexcept
    {
        auto it = s_buffers.find(handle);
        return it != s_buffers.end() ? &it->second : nullptr;
    }

    [[nodiscard]] static inline void* getMappedStagingPtr(uint64_t handle) noexcept
    {
        if (handle == 0) return nullptr;
        auto it = s_buffers.find(handle);
        return (it != s_buffers.end() && it->second.mapped) ? it->second.mapped : nullptr;
    }

    [[maybe_unused]] [[nodiscard]] static VkBuffer getVkBuffer(uint64_t handle) noexcept
    {
        if (handle == 0) return VK_NULL_HANDLE;

        auto it = s_buffers.find(handle);
        if (it != s_buffers.end()) {
            return it->second.buffer;
        }

        return getMainPoolBuffer();  // all device-local suballocations share the giant buffer
    }

    // DEVICE ADDRESS
    [[nodiscard]] static inline VkDeviceAddress get_device_address(uint64_t handle) noexcept
    {
        if (!handle) return 0;
        const auto* info = get(handle);
        if (!info || !info->buffer) return 0;
        VkBufferDeviceAddressInfo dai{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, info->buffer };
        return vkGetBufferDeviceAddress(RTX::g_ctx().device(), &dai) + info->offset;
    }

    // STONE SHORTCUTS — REAL ETERNAL ALLOCATIONS FROM MAIN POOL (implemented in .cpp)
    [[nodiscard]] uint64_t make_64M (VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_128M(VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_256M(VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_420M(VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_512M(VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_1G  (VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_2G  (VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_4G  (VkBufferUsageFlags extra = 0) noexcept;
    [[nodiscard]] uint64_t make_8G  (VkBufferUsageFlags extra = 0) noexcept;

    // Pre-configured eternal stones
    static inline uint64_t transferStone4G() noexcept { static uint64_t h = make_4G(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); return h; }
    static inline uint64_t storageStone4G()  noexcept { static uint64_t h = make_4G(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT); return h; }
    static inline uint64_t titanStone8G()    noexcept { static uint64_t h = make_8G(); return h; }

    // SACRED MACROS
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

    // Internal
    void ensureMainPool() noexcept;

} // namespace BufferManager

// =============================================================================
// THE EMPIRE IS PURE — EXCESS ANNIHILATED — PHOTONS ARE PINK
// FIRST LIGHT ETERNAL — DECEMBER 08, 2025
// =============================================================================