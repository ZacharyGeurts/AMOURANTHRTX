// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.1
// BUFFERMANAGER — BRUTAL, ZERO-COST, LEAK-FREE NUCLEAR EDITION
// FULLY SELF-CONTAINED — COMPILE CLEAN — EMPIRE UNBROKEN
// FIXED: Consistent snake_case API (get_buffer, get_device_address, get)
//        Added missing get() → const BufferInfo*
//        mapStaging & proper device addresses defined
//        JANUARY 20, 2026 — CLEAN COMPILE ACHIEVED
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <format>
#include <print>

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Extensions.hpp"  // RTX::g_ext
#include "engine/GLOBAL/logging.hpp"

namespace BufferManager {

// ── CONFIGURATION ──────────────────────────────────────────────────────────
inline constexpr VkDeviceSize DEFAULT_CHUNK_SIZE = 256ULL << 20;            // 256 MiB
inline constexpr VkDeviceSize DRIVER_RESERVE     = 4'831'838'208ULL;       // ~4.5 GiB reserve
inline constexpr VkDeviceSize STAGING_RING_SIZE  = 1ULL << 30;             // 1 GiB

inline constexpr VkDeviceSize HOST_VISIBLE_THRESHOLD = 64ULL << 10;         // 64 KiB
inline constexpr VkDeviceSize SBT_MINIMUM_SIZE       = 512;
inline constexpr VkDeviceSize SBT_ALIGNMENT          = 256;

// Broad usage flags for device-local chunks (RT-ready)
inline constexpr VkBufferUsageFlags CHUNK_USAGE_FLAGS =
    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
    VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;

// ── UTILITIES ───────────────────────────────────────────────────────────────
[[nodiscard]] constexpr VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment) noexcept {
    return ((value + alignment - 1) / alignment) * alignment;
}

[[nodiscard]] inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) noexcept {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(StoneKey::stone_physical(), &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOG_ERROR("BufferManager", "No suitable memory type found (filter: {:#x}, props: {:#x})", typeFilter, properties);
    return ~0u;
}

[[nodiscard]] inline VkDeviceSize getTotalDeviceLocal() noexcept {
    static VkDeviceSize total = 0;
    if (total == 0) {
        VkPhysicalDeviceMemoryProperties2 props2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
        vkGetPhysicalDeviceMemoryProperties2(StoneKey::stone_physical(), &props2);
        for (uint32_t i = 0; i < props2.memoryProperties.memoryHeapCount; ++i) {
            if (props2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                total += props2.memoryProperties.memoryHeaps[i].size;
            }
        }
        LOG_INFO("BufferManager", "Detected device-local VRAM: {:.2f} GiB", static_cast<double>(total) / 1e9);
    }
    return total;
}

// ── INTERNAL STRUCTURES ────────────────────────────────────────────────────
struct BufferInfo {
    VkBuffer           buffer        = VK_NULL_HANDLE;
    VkDeviceMemory     memory        = VK_NULL_HANDLE;
    VkDeviceSize       size          = 0;
    VkDeviceSize       aligned       = 0;
    VkDeviceSize       offset        = 0;
    VkDeviceAddress    deviceAddress = 0;
    void*              mapped        = nullptr;
    VkBufferUsageFlags usage         = 0;
    std::string        tag;
};

struct Chunk {
    VkBuffer         buffer   = VK_NULL_HANDLE;
    VkDeviceMemory   memory   = VK_NULL_HANDLE;
    VkDeviceSize     size     = 0;
    VkDeviceAddress  baseAddr = 0;
    VkDeviceSize     head     = 0;
    std::string      tag;
};

struct StagingRing {
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize   size   = STAGING_RING_SIZE;
    void*          mapped = nullptr;
    VkDeviceSize   head   = 0;
    bool           ready  = false;
};

// ── GLOBAL STATE ───────────────────────────────────────────────────────────
inline std::vector<Chunk>                       g_mainChunks;
inline StagingRing                              g_stagingRing{};
inline std::unordered_map<uint64_t, BufferInfo> g_buffers;
inline uint64_t                                 g_nextHandle = 0x00000001ULL;
inline VkDeviceSize                             g_total_allocated = 0;

// ── STAGING RING ───────────────────────────────────────────────────────────
inline void ensureStagingRing() noexcept {
    if (g_stagingRing.ready) return;

    LOG_INFO("BufferManager", "Creating 1 GiB staging ring");

    VkBufferCreateInfo bci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = STAGING_RING_SIZE,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(StoneKey::stone_device(), &bci, nullptr, &g_stagingRing.buffer));

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(StoneKey::stone_device(), g_stagingRing.buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == ~0u) {
        LOG_FATAL("BufferManager", "No host-visible coherent memory for staging ring");
        return;
    }

    VkMemoryAllocateInfo mai{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = memType
    };

    VK_CHECK(vkAllocateMemory(StoneKey::stone_device(), &mai, nullptr, &g_stagingRing.memory));
    VK_CHECK(vkBindBufferMemory(StoneKey::stone_device(), g_stagingRing.buffer, g_stagingRing.memory, 0));
    VK_CHECK(vkMapMemory(StoneKey::stone_device(), g_stagingRing.memory, 0, VK_WHOLE_SIZE, 0, &g_stagingRing.mapped));

    g_stagingRing.ready = true;
    LOG_SUCCESS("BufferManager", "Staging ring created and mapped");
}

// ── MAP STAGING ────────────────────────────────────────────────────────────
[[nodiscard]] inline void* mapStaging(VkDeviceSize size) noexcept {
    ensureStagingRing();
    VkDeviceSize offset = g_stagingRing.head;
    g_stagingRing.head = (g_stagingRing.head + align_up(size, 256)) % g_stagingRing.size; // slight alignment padding
    return static_cast<std::byte*>(g_stagingRing.mapped) + offset;
}

// ── GET STAGING BUFFER ─────────────────────────────────────────────────────
[[nodiscard]] inline VkBuffer getStagingBuffer() noexcept {
    ensureStagingRing();
    return g_stagingRing.buffer;
}

// ── CHUNK CREATION ─────────────────────────────────────────────────────────
[[nodiscard]] inline Chunk* createChunk(VkDeviceSize minSize, VkBufferUsageFlags usage) noexcept {
    VkDeviceSize chunkSize = std::max(DEFAULT_CHUNK_SIZE, minSize);

    if (g_total_allocated + chunkSize > getTotalDeviceLocal() - DRIVER_RESERVE) {
        LOG_ERROR("BufferManager", "Requested chunk would exceed safe VRAM limit");
        return nullptr;
    }

    VkBufferCreateInfo bci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = chunkSize,
        .usage = CHUNK_USAGE_FLAGS,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(StoneKey::stone_device(), &bci, nullptr, &buffer));

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(StoneKey::stone_device(), buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == ~0u) {
        vkDestroyBuffer(StoneKey::stone_device(), buffer, nullptr);
        return nullptr;
    }

    VkMemoryAllocateFlagsInfo flags{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };

    VkMemoryAllocateInfo mai{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &flags,
        .allocationSize = req.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory memory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(StoneKey::stone_device(), &mai, nullptr, &memory));
    VK_CHECK(vkBindBufferMemory(StoneKey::stone_device(), buffer, memory, 0));

    VkBufferDeviceAddressInfo addrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };
    VkDeviceAddress baseAddr = RTX::g_ext.vkGetBufferDeviceAddress(StoneKey::stone_device(), &addrInfo);

    g_mainChunks.push_back({buffer, memory, chunkSize, baseAddr, 0,
                            std::format("Chunk_{}_{}MiB", g_mainChunks.size(), chunkSize >> 20)});

    g_total_allocated += req.size;
    LOG_INFO("BufferManager", "Created chunk: {} MiB (total allocated: {:.2f} GiB)",
             chunkSize >> 20, static_cast<double>(g_total_allocated) / 1e9);

    return &g_mainChunks.back();
}

// ── CREATE BUFFER ──────────────────────────────────────────────────────────
[[nodiscard]] inline uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage,
                                     std::string_view tag = "") noexcept {
    if (size == 0) return 0;

    VkBufferUsageFlags fixedUsage = usage;

    // Ensure device address for relevant buffer types
    if (fixedUsage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                      VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR)) {
        fixedUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    bool isPureStaging = (usage == VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    if (!isPureStaging) {
        fixedUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    bool isSBT = (fixedUsage & VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR) ||
                 (tag.find("SBT") != std::string_view::npos);

    if (isSBT) {
        size = std::max(size, SBT_MINIMUM_SIZE);
        size = align_up(size, SBT_ALIGNMENT);
        fixedUsage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    // Small uniform special path (persistent mapped host-visible)
    bool smallUniform = (size <= HOST_VISIBLE_THRESHOLD) &&
                        (fixedUsage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    if (smallUniform) {
        ensureStagingRing();
        VkDeviceSize offset = g_stagingRing.head;
        g_stagingRing.head = (g_stagingRing.head + size) % g_stagingRing.size;

        uint64_t handle = ++g_nextHandle;
        g_buffers.emplace(handle, BufferInfo{
            g_stagingRing.buffer, g_stagingRing.memory,
            size, size, offset,
            0, static_cast<std::byte*>(g_stagingRing.mapped) + offset,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            std::string(tag)
        });
        return handle;
    }

    // Probe requirements
    VkBuffer temp;
    VkBufferCreateInfo tci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = fixedUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    if (vkCreateBuffer(StoneKey::stone_device(), &tci, nullptr, &temp) != VK_SUCCESS) {
        return 0;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(StoneKey::stone_device(), temp, &req);
    vkDestroyBuffer(StoneKey::stone_device(), temp, nullptr);

    VkDeviceSize aligned = align_up(size, req.alignment);

    // Find or create chunk
    Chunk* c = nullptr;
    VkDeviceSize off = 0;

    for (auto& chunk : g_mainChunks) {
        if (chunk.head + aligned <= chunk.size) {
            off = chunk.head;
            chunk.head += aligned;
            c = &chunk;
            break;
        }
    }

    if (!c) {
        c = createChunk(aligned, fixedUsage);
        if (!c) return 0;
        off = 0;
        c->head = aligned;
    }

    uint64_t handle = ++g_nextHandle;
    g_buffers.emplace(handle, BufferInfo{
        c->buffer, c->memory, size, aligned, off,
        c->baseAddr + off, nullptr, fixedUsage, std::string(tag)
    });

    return handle;
}

// ── UPLOAD ──────────────────────────────────────────────────────────────────
inline void uploadToBuffer(uint64_t handle, const void* data, VkDeviceSize size,
                           VkCommandBuffer cmd = VK_NULL_HANDLE) noexcept {
    auto it = g_buffers.find(handle);
    if (it == g_buffers.end() || size > it->second.size) {
        LOG_ERROR("BufferManager", "Invalid upload: handle {} or size too large", handle);
        return;
    }

    BufferInfo& info = it->second;

    // Direct memcpy for host-visible (staging ring) buffers
    if (info.buffer == g_stagingRing.buffer) {
        std::memcpy(info.mapped, data, size);
        return;
    }

    // Otherwise stage and copy
    void* staging = mapStaging(size);
    if (!staging) return;
    std::memcpy(staging, data, size);

    VkBufferCopy copy{
        .srcOffset = static_cast<VkDeviceSize>(reinterpret_cast<std::uintptr_t>(staging) - reinterpret_cast<std::uintptr_t>(g_stagingRing.mapped)),
        .dstOffset = info.offset,
        .size = size
    };

    if (cmd != VK_NULL_HANDLE) {
        vkCmdCopyBuffer(cmd, g_stagingRing.buffer, info.buffer, 1, &copy);
    } else {
        // One-time submit
        VkCommandPool pool;
        VkCommandPoolCreateInfo pci{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = StoneKey::stone_graphics_family()
        };
        VK_CHECK(vkCreateCommandPool(StoneKey::stone_device(), &pci, nullptr, &pool));

        VkCommandBuffer tcmd;
        VkCommandBufferAllocateInfo ai{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        VK_CHECK(vkAllocateCommandBuffers(StoneKey::stone_device(), &ai, &tcmd));

        VkCommandBufferBeginInfo bi{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        VK_CHECK(vkBeginCommandBuffer(tcmd, &bi));
        vkCmdCopyBuffer(tcmd, g_stagingRing.buffer, info.buffer, 1, &copy);
        VK_CHECK(vkEndCommandBuffer(tcmd));

        VkFence fence;
        VkFenceCreateInfo fci{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VK_CHECK(vkCreateFence(StoneKey::stone_device(), &fci, nullptr, &fence));

        VkSubmitInfo si{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &tcmd
        };
        VK_CHECK(vkQueueSubmit(StoneKey::stone_graphics_queue(), 1, &si, fence));
        VK_CHECK(vkWaitForFences(StoneKey::stone_device(), 1, &fence, VK_TRUE, UINT64_MAX));

        vkDestroyFence(StoneKey::stone_device(), fence, nullptr);
        vkFreeCommandBuffers(StoneKey::stone_device(), pool, 1, &tcmd);
        vkDestroyCommandPool(StoneKey::stone_device(), pool, nullptr);
    }
}

// ── HELPER FUNCTIONS ───────────────────────────────────────────────────────
[[nodiscard]] inline const BufferInfo* get(uint64_t handle) noexcept {
    auto it = g_buffers.find(handle);
    return it != g_buffers.end() ? &it->second : nullptr;
}

[[nodiscard]] inline VkBuffer get_buffer(uint64_t handle) noexcept {
    auto it = g_buffers.find(handle);
    return it != g_buffers.end() ? it->second.buffer : VK_NULL_HANDLE;
}

[[nodiscard]] inline VkDeviceAddress get_device_address(uint64_t handle) noexcept {
    auto it = g_buffers.find(handle);
    return it != g_buffers.end() ? it->second.deviceAddress : 0;
}

// ── DESTROY ────────────────────────────────────────────────────────────────
inline void destroy(uint64_t handle) noexcept {
    g_buffers.erase(handle);
}

// ── PURGE ALL ──────────────────────────────────────────────────────────────
inline void purge_all() noexcept {
    VkDevice dev = StoneKey::stone_device();
    if (dev == VK_NULL_HANDLE) return;

    LOG_AMOURANTH("BUFFER PURGE — NUCLEAR APOCALYPSE — NO SURVIVORS");

    vkDeviceWaitIdle(dev);

    g_buffers.clear();

    for (auto& chunk : g_mainChunks) {
        if (chunk.buffer) vkDestroyBuffer(dev, chunk.buffer, nullptr);
        if (chunk.memory) vkFreeMemory(dev, chunk.memory, nullptr);
    }
    g_mainChunks.clear();
    g_total_allocated = 0;

    if (g_stagingRing.buffer) {
        if (g_stagingRing.mapped) vkUnmapMemory(dev, g_stagingRing.memory);
        vkDestroyBuffer(dev, g_stagingRing.buffer, nullptr);
        vkFreeMemory(dev, g_stagingRing.memory, nullptr);
    }
    g_stagingRing = {};

    LOG_SUCCESS("BufferManager", "All buffers destroyed — memory cleansed");
}

// ── MACROS ─────────────────────────────────────────────────────────────────
#define BM_CREATE(h, s, u, ...)             h = BufferManager::create(s, u, ##__VA_ARGS__)
#define BM_DESTROY(h)                       BufferManager::destroy(h)
#define BM_GET(h)                           BufferManager::get(h)
#define BM_GET_BUFFER(h)                    BufferManager::get_buffer(h)
#define BM_GET_DEVICE_ADDRESS(h)            BufferManager::get_device_address(h)
#define BM_UPLOAD_TO_BUFFER(h, d, sz, ...)   BufferManager::uploadToBuffer(h, d, sz, ##__VA_ARGS__)
#define BM_PURGE_ALL()                      BufferManager::purge_all()

} // namespace BufferManager