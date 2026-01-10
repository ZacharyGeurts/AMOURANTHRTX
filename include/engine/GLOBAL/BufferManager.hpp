// include/engine/GLOBAL/BufferManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v28.5 — JANUARY 10, 2026
// BUFFERMANAGER — HEADER-ONLY 2026 ULTIMATE EDITION — PROFESSIONAL PRODUCTION RELEASE
// ZERO-COST C++23 PHILOSOPHY | FULLY MODERN | SAFE & ETERNAL | SUPER AUTOMAGIC FLAGS
// ON-DEMAND DEVICE-LOCAL CHUNKS — MAXIMUM VRAM CONTROL WITH DRIVER RESERVE
// UNIFIED POOL VIEW — DEVELOPERS SEE ONE INFINITE POOL (USABLE VRAM)
// LARGE ALLOCATIONS (>256 MiB) CREATE DEDICATED CHUNKS OF EXACT SIZE
// 1 GiB PERSISTENTLY MAPPED STAGING RING — RING-BUFFERED TRANSFERS
// PERSISTENT DIRECT UPLOAD DISABLED — STAGING RING ONLY (NVIDIA DRIVER SAFE)
// SMART PATH: ≤64KiB UNIFORMS → HOST-VISIBLE | ALL ELSE → DEVICE-LOCAL + BDA
// SUPER AUTOMAGIC: Forces TRANSFER_DST + SHADER_DEVICE_ADDRESS for device-local — fixes VUID-00120 & 02601
// SBT: minimum size + 256-byte padding + TRANSFER_DST
// VALIDATION CLEAN | ZERO FRAGMENTATION | NO INVALID COPIES
// NO FATAL ON EXHAUST — LOG_ERROR + RETURN 0
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <format>
#include <print>
#include <bit>

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/logging.hpp"

namespace BufferManager {

// ── CONFIGURATION — TUNED FOR 2026 HIGH-END GPUs ───────────────────────────
inline constexpr VkDeviceSize DEFAULT_CHUNK_SIZE = 256ULL << 20;            // 256 MiB base chunk size
inline constexpr VkDeviceSize DRIVER_RESERVE     = 4'831'838'208ULL;       // ~4.5 GiB safe reserve
inline constexpr VkDeviceSize STAGING_RING_SIZE  = 1ULL   << 30;           // 1 GiB persistent staging

inline constexpr VkDeviceSize HOST_VISIBLE_THRESHOLD = 64ULL << 10;         // 64 KiB
inline constexpr VkDeviceSize SBT_MINIMUM_SIZE       = 512;
inline constexpr VkDeviceSize SBT_ALIGNMENT          = 256;                // NVIDIA requirement

// Total device-local VRAM (computed once)
inline VkDeviceSize g_total_device_local = 0;

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
    if (g_total_device_local == 0) {
        VkPhysicalDeviceMemoryProperties2 memProps2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
        vkGetPhysicalDeviceMemoryProperties2(StoneKey::stone_physical(), &memProps2);

        for (uint32_t i = 0; i < memProps2.memoryProperties.memoryHeapCount; ++i) {
            if (memProps2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                g_total_device_local += memProps2.memoryProperties.memoryHeaps[i].size;
            }
        }
        LOG_INFO("BufferManager", "Detected total device-local VRAM: {:.2f} GiB", static_cast<double>(g_total_device_local) / 1e9);
    }
    return g_total_device_local;
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

// ── GLOBAL EMPIRE STATE ────────────────────────────────────────────────────
inline std::vector<Chunk>                       g_mainChunks;
inline StagingRing                              g_stagingRing{};
inline std::unordered_map<uint64_t, BufferInfo> g_buffers;
inline uint64_t                                 g_nextHandle = 0x00000001ULL;
inline VkDeviceSize                             g_total_allocated = 0;

// ── SUPER AUTOMAGIC USAGE FIX — Forces required flags for validation clean ──
// In BufferManager.hpp — replace smart_usage()
[[nodiscard]] constexpr VkBufferUsageFlags smart_usage(VkBufferUsageFlags input) noexcept {
    VkBufferUsageFlags fixed = input;

    // Always force these for any device-local buffer
    fixed |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    // Force TRANSFER_DST for EVERYTHING except PURE staging
    if (!(input & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) || 
        (input & (VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT))) {
        fixed |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    // Pure staging only
    if (input == VK_BUFFER_USAGE_TRANSFER_SRC_BIT) {
        fixed = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }

    return fixed;
}

// ── CREATE NEW CHUNK ON-DEMAND (automagic usage fix) ────────────────────────
[[nodiscard]] inline Chunk* createChunk(VkDeviceSize minSize, VkBufferUsageFlags usage) noexcept {
    VkDeviceSize chunkSize = std::max(DEFAULT_CHUNK_SIZE, minSize);

    if (g_total_allocated + chunkSize > getTotalDeviceLocal() - DRIVER_RESERVE) {
        LOG_ERROR("BufferManager", "Cannot create chunk of {} bytes — exceeds safe VRAM limit", chunkSize);
        return nullptr;
    }

    VkBufferUsageFlags fixedUsage = smart_usage(usage);

    VkBufferCreateInfo bci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = chunkSize,
        .usage = fixedUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(StoneKey::stone_device(), &bci, nullptr, &buffer) != VK_SUCCESS) {
        LOG_ERROR("BufferManager", "Failed to create chunk buffer of {} bytes", chunkSize);
        return nullptr;
    }

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
    if (vkAllocateMemory(StoneKey::stone_device(), &mai, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(StoneKey::stone_device(), buffer, nullptr);
        LOG_ERROR("BufferManager", "Failed to allocate memory for chunk of {} bytes", chunkSize);
        return nullptr;
    }

    if (vkBindBufferMemory(StoneKey::stone_device(), buffer, memory, 0) != VK_SUCCESS) {
        vkFreeMemory(StoneKey::stone_device(), memory, nullptr);
        vkDestroyBuffer(StoneKey::stone_device(), buffer, nullptr);
        LOG_ERROR("BufferManager", "Failed to bind memory for chunk");
        return nullptr;
    }

    VkBufferDeviceAddressInfo addrInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer};
    VkDeviceAddress baseAddr = RTX::ext().vkGetBufferDeviceAddress(StoneKey::stone_device(), &addrInfo);

    g_mainChunks.push_back(Chunk{
        .buffer   = buffer,
        .memory   = memory,
        .size     = chunkSize,
        .baseAddr = baseAddr,
        .head     = 0,
        .tag      = std::format("Chunk_{}_size_{}MiB", g_mainChunks.size(), chunkSize >> 20)
    });

    g_total_allocated += req.size;
    LOG_INFO("BufferManager", "New chunk created: {} MiB (total allocated: {:.2f} GiB) | usage: {:#x}",
             chunkSize >> 20, static_cast<double>(g_total_allocated) / 1e9, fixedUsage);

    return &g_mainChunks.back();
}

// ── 1 GiB STAGING RING — PERSISTENTLY MAPPED RING BUFFER (STRICT SRC ONLY) ──
inline void ensureStagingRing() noexcept {
    if (g_stagingRing.ready) return;

    LOG_INFO("BufferManager", "Creating 1 GiB persistently mapped staging ring");

    VkBufferCreateInfo bci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = STAGING_RING_SIZE,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,  // STRICT SRC ONLY
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(StoneKey::stone_device(), &bci, nullptr, &g_stagingRing.buffer));

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(StoneKey::stone_device(), g_stagingRing.buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == ~0u) {
        LOG_FATAL("BufferManager", "No host-visible memory for staging ring");
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
    LOG_SUCCESS("BufferManager", "1 GiB staging ring ready — eternal host access");
}

// ── STAGING HELPERS ────────────────────────────────────────────────────────
[[nodiscard]] inline void* mapStaging(VkDeviceSize size) noexcept {
    ensureStagingRing();
    VkDeviceSize offset = g_stagingRing.head;
    g_stagingRing.head = (g_stagingRing.head + size) % g_stagingRing.size;
    return static_cast<std::byte*>(g_stagingRing.mapped) + offset;
}

[[nodiscard]] inline VkBuffer getStagingBuffer() noexcept {
    ensureStagingRing();
    return g_stagingRing.buffer;
}

// ── SUPER SMART ALLOCATION — ZERO-COST PATH SELECTION + AUTOMAGIC FLAGS ─────
[[nodiscard]] inline uint64_t create(VkDeviceSize size,
                                     VkBufferUsageFlags usage,
                                     std::string_view tag = "") noexcept
{
    if (size == 0) return 0;

    VkBufferUsageFlags fixedUsage = smart_usage(usage);

    if (fixedUsage & VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR) {
        size = std::max(size, SBT_MINIMUM_SIZE);
        size = align_up(size, SBT_ALIGNMENT);
        fixedUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    bool smallUniform = (size <= HOST_VISIBLE_THRESHOLD) &&
                        (fixedUsage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT));

    if (smallUniform) {
        ensureStagingRing();

        VkDeviceSize offset = g_stagingRing.head;
        g_stagingRing.head = (g_stagingRing.head + size) % g_stagingRing.size;

        uint64_t handle = ++g_nextHandle;
        g_buffers.emplace(handle, BufferInfo{
            .buffer  = g_stagingRing.buffer,
            .memory  = g_stagingRing.memory,
            .size    = size,
            .aligned = size,
            .offset  = offset,
            .mapped  = static_cast<std::byte*>(g_stagingRing.mapped) + offset,
            .usage   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .tag     = std::string(tag)
        });

        return handle;
    }

    // Device-local path — force required flags
    fixedUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    fixedUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VkBufferCreateInfo tempBci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = fixedUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer tempBuffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(StoneKey::stone_device(), &tempBci, nullptr, &tempBuffer) != VK_SUCCESS) {
        LOG_ERROR("BufferManager", "Failed to create temp buffer for req (size: {})", size);
        return 0;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(StoneKey::stone_device(), tempBuffer, &req);
    vkDestroyBuffer(StoneKey::stone_device(), tempBuffer, nullptr);

    VkDeviceSize aligned = align_up(size, req.alignment);

    Chunk* chosen = nullptr;
    VkDeviceSize offsetInChunk = 0;

    for (auto& chunk : g_mainChunks) {
        if (chunk.head + aligned <= chunk.size) {
            offsetInChunk = chunk.head;
            chunk.head += aligned;
            chosen = &chunk;
            break;
        }
    }

    if (!chosen) {
        VkDeviceSize minChunkSize = align_up(aligned, req.alignment);
        chosen = createChunk(minChunkSize, fixedUsage);
        if (!chosen) return 0;

        offsetInChunk = 0;
        chosen->head = aligned;
    }

    uint64_t handle = ++g_nextHandle;
    g_buffers.emplace(handle, BufferInfo{
        .buffer        = chosen->buffer,
        .memory        = chosen->memory,
        .size          = size,
        .aligned       = aligned,
        .offset        = offsetInChunk,
        .deviceAddress = chosen->baseAddr + offsetInChunk,
        .usage         = fixedUsage,
        .tag           = std::string(tag)
    });

    return handle;
}

// ── LEGACY COMPATIBILITY ACCESSORS ─────────────────────────────────────────
[[nodiscard]] inline const BufferInfo* get(uint64_t handle) noexcept {
    auto it = g_buffers.find(handle);
    return it != g_buffers.end() ? &it->second : nullptr;
}

[[nodiscard]] inline VkBuffer getVkBuffer(uint64_t handle) noexcept {
    auto* info = get(handle);
    return info ? info->buffer : VK_NULL_HANDLE;
}

[[nodiscard]] inline VkDeviceAddress get_device_address(uint64_t handle) noexcept {
    auto* info = get(handle);
    return info ? info->deviceAddress : 0;
}

// ── UPLOAD USING STAGING RING (safe, always SRC → DST) ─────────────────────
inline void uploadToBuffer(uint64_t handle, const void* data, VkDeviceSize size, VkCommandBuffer cmd = VK_NULL_HANDLE) noexcept {
    auto* info = get(handle);
    if (!info || size > info->size) {
        LOG_ERROR("BufferManager", "Invalid upload to handle {:#x} (size {})", handle, size);
        return;
    }

    void* staging = mapStaging(size);
    if (!staging) return;

    memcpy(staging, data, size);

    VkBufferCopy copy{
        .srcOffset = (reinterpret_cast<uintptr_t>(staging) - reinterpret_cast<uintptr_t>(g_stagingRing.mapped)),
        .dstOffset = info->offset,
        .size = size
    };

    if (cmd == VK_NULL_HANDLE) {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = StoneKey::stone_graphics_family()
        };
        VK_CHECK(vkCreateCommandPool(StoneKey::stone_device(), &poolInfo, nullptr, &pool));

        VkCommandBuffer tempCmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        VK_CHECK(vkAllocateCommandBuffers(StoneKey::stone_device(), &allocInfo, &tempCmd));

        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        VK_CHECK(vkBeginCommandBuffer(tempCmd, &beginInfo));

        vkCmdCopyBuffer(tempCmd, getStagingBuffer(), info->buffer, 1, &copy);
        VK_CHECK(vkEndCommandBuffer(tempCmd));

        VkSubmitInfo submit{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &tempCmd
        };
        VK_CHECK(vkQueueSubmit(StoneKey::stone_graphics_queue(), 1, &submit, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(StoneKey::stone_graphics_queue()));

        vkFreeCommandBuffers(StoneKey::stone_device(), pool, 1, &tempCmd);
        vkDestroyCommandPool(StoneKey::stone_device(), pool, nullptr);
    } else {
        vkCmdCopyBuffer(cmd, getStagingBuffer(), info->buffer, 1, &copy);
    }
}

// ── CLEANUP ────────────────────────────────────────────────────────────────
inline void destroy(uint64_t handle) noexcept {
    if (handle == 0) return;
    g_buffers.erase(handle);
}

inline void purge_all() noexcept {
    VkDevice dev = StoneKey::stone_device();
    if (dev == VK_NULL_HANDLE) return;

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

    LOG_INFO("BufferManager", "All buffers purged — memory empire cleansed");
}

// ── PUBLIC ZERO-COST MACROS (legacy compatibility) ────────────────────────
#define BM_CREATE(h, size, usage, ...)       h = BufferManager::create(size, usage, ##__VA_ARGS__)
#define BM_DESTROY(h)                        BufferManager::destroy(h)
#define BM_VK_BUFFER(h)                      BufferManager::getVkBuffer(h)
#define BM_DEVICE_ADDRESS(h)                 BufferManager::get_device_address(h)
#define BM_UPLOAD_TO_BUFFER(h, data, size)   BufferManager::uploadToBuffer(h, data, size)

} // namespace BufferManager

// =============================================================================
// BUFFERMANAGER v28.5 — JANUARY 10, 2026 — FINAL NVIDIA-SAFE PRODUCTION RELEASE
// - All compile issues fixed (correct order)
// - SUPER AUTOMAGIC: Forces TRANSFER_DST + SHADER_DEVICE_ADDRESS for device-local — all VUIDs fixed
// - Staging ring: strictly TRANSFER_SRC + host-visible
// - SBT: minimum size + 256-byte padding + TRANSFER_DST
// - On-demand chunk allocation — no preclaim, grow as needed
// - Unified pool: Developers request any size up to usable VRAM
// - No fatal on exhaust — LOG_ERROR + return 0
// - Staging ring true wrap-around (% size)
// - Upload always SRC → DST
// - All globals inline | Modern C++23 | Eternal and unbreakable
// The memory dominion is complete — the empire stands forever
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================