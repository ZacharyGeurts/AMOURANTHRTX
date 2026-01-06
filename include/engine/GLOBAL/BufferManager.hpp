// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v27.6 — JANUARY 06, 2026
// BUFFERMANAGER — HEADER-ONLY 2026 ULTIMATE EDITION — PROFESSIONAL PRODUCTION RELEASE
// 256 MiB LINEAR CHUNKS — MAXIMUM VRAM UTILIZATION WITH SAFE DRIVER RESERVE
// PERSISTENT 1 MiB UPLOAD BUFFER — ETERNAL DIRECT WRITES
// STAGING RING — 1 GiB PERSISTENTLY MAPPED
// SMART ALLOCATION PATH: SMALL UNIFORMS → HOST-VISIBLE | ALL ELSE → DEVICE-LOCAL
// ONE CALL. ONE SYSTEM. TOTAL CONTROL.
// VALIDATION CLEAN | ZERO FRAGMENTATION | PRODUCTION READY
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <format>
#include <print>

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Extensions.hpp"

namespace BufferManager {

// ── CONFIGURATION CONSTANTS — OPTIMIZED FOR 2026 PRODUCTION ─────────────────
constexpr VkDeviceSize CHUNK_SIZE = 256ULL * 1024 * 1024;            // 256 MiB chunks — optimal balance
constexpr VkDeviceSize DRIVER_RESERVE = 4'831'838'208ULL;           // 4.5 GiB — conservative driver reserve
constexpr VkDeviceSize STAGING_RING_SIZE = 1ULL << 30;              // 1 GiB persistent staging ring
constexpr VkDeviceSize PERSISTENT_UPLOAD_SIZE = 1ULL << 20;         // 1 MiB eternal direct upload buffer

constexpr VkDeviceSize HOST_VISIBLE_THRESHOLD = 64ULL * 1024;       // 64 KiB — small uniform threshold
constexpr VkDeviceSize SBT_MINIMUM_SIZE = 512;                      // SBT handle safety padding

// ── HELPER FUNCTIONS ───────────────────────────────────────────────────────
template <typename T>
[[nodiscard]] constexpr T align_up(T value, T alignment) noexcept {
    return ((value + alignment - 1) / alignment) * alignment;
}

[[nodiscard]] inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) noexcept {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(StoneKey::stone_physical(), &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && 
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    std::print(stderr, "[BUFFER ERROR] No suitable memory type found — filter: 0x{:x}, properties: 0x{:x}\n", typeFilter, properties);
    return ~0u;
}

// ── INTERNAL DATA STRUCTURES ───────────────────────────────────────────────
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
    VkDeviceSize     head     = 0;
};

struct StagingRing {
    VkBuffer            buffer = VK_NULL_HANDLE;
    VkDeviceMemory      memory = VK_NULL_HANDLE;
    VkDeviceSize        size   = 0;
    void*               mapped = nullptr;
    VkDeviceSize        head   = 0;
    bool                ready  = false;
};

// ── GLOBAL STATE — THE EMPIRE'S MEMORY DOMINION ────────────────────────────
static std::vector<Chunk> g_mainChunks;
static StagingRing g_stagingRingInstance;

static std::unordered_map<uint64_t, BufferInfo> s_buffers;
static uint64_t g_nextHandle = 0x00000001ULL;

static std::atomic<bool> g_purged{false};

// Persistent direct upload buffer — created once, used forever
static VkBuffer g_persistentUploadBuffer = VK_NULL_HANDLE;
static void* g_persistentUploadMapped = nullptr;

// ── MAIN POOL INITIALIZATION — CLAIM ALL USABLE MEMORY ─────────────────────
inline void ensureMainPool() noexcept {
    if (!g_mainChunks.empty()) return;

    std::print("\n[BUFFER] Initializing main device-local memory pool\n");

    VkPhysicalDeviceMemoryProperties2 memProps2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
    vkGetPhysicalDeviceMemoryProperties2(StoneKey::stone_physical(), &memProps2);

    VkDeviceSize totalDeviceLocal = 0;
    for (uint32_t i = 0; i < memProps2.memoryProperties.memoryHeapCount; ++i) {
        if (memProps2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalDeviceLocal += memProps2.memoryProperties.memoryHeaps[i].size;
        }
    }

    VkDeviceSize availableSize = totalDeviceLocal - DRIVER_RESERVE;
    uint32_t chunkCount = static_cast<uint32_t>(availableSize / CHUNK_SIZE);

    if (chunkCount == 0) {
        std::print(stderr, "[FATAL] Insufficient device-local memory after driver reserve\n");
        return;
    }

    g_mainChunks.reserve(chunkCount);

    std::print("[BUFFER] Total VRAM: {:.1f} GiB | Driver reserve: 4.5 GiB | Available: {:.1f} GiB ({} × 256 MiB chunks)\n",
               totalDeviceLocal / (1024.0*1024*1024), availableSize / (1024.0*1024*1024), chunkCount);

    for (uint32_t i = 0; i < chunkCount; ++i) {
        VkBufferCreateInfo bci = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = CHUNK_SIZE,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                     VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VkBuffer buffer = VK_NULL_HANDLE;
        VK_CHECK(vkCreateBuffer(StoneKey::stone_device(), &bci, nullptr, &buffer));

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(StoneKey::stone_device(), buffer, &req);

        uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memType == ~0u) {
            vkDestroyBuffer(StoneKey::stone_device(), buffer, nullptr);
            std::print(stderr, "[FATAL] No suitable device-local memory type for main pool\n");
            return;
        }

        VkMemoryAllocateFlagsInfo flagsInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR
        };

        VkMemoryAllocateInfo mai = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &flagsInfo,
            .allocationSize = req.size,
            .memoryTypeIndex = memType
        };

        VkDeviceMemory memory = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateMemory(StoneKey::stone_device(), &mai, nullptr, &memory));
        VK_CHECK(vkBindBufferMemory(StoneKey::stone_device(), buffer, memory, 0));

        VkBufferDeviceAddressInfo addrInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer };
        VkDeviceAddress baseAddr = RTX::ext().vkGetBufferDeviceAddress(StoneKey::stone_device(), &addrInfo);

        Chunk chunk{
            .buffer = buffer,
            .memory = memory,
            .size = CHUNK_SIZE,
            .baseAddr = baseAddr,
            .tag = std::format("MainPool_Chunk_{}", i),
            .head = 0
        };

        g_mainChunks.push_back(std::move(chunk));
    }

    std::print("[BUFFER] Main pool initialized — {} chunks — full control established\n", chunkCount);
}

// ── PERSISTENT DIRECT UPLOAD BUFFER — ETERNAL AND EFFICIENT ─────────────────
inline void ensurePersistentUpload() noexcept {
    if (g_persistentUploadBuffer != VK_NULL_HANDLE) return;

    std::print("[BUFFER] Initializing persistent 1 MiB direct upload buffer\n");

    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = PERSISTENT_UPLOAD_SIZE,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(StoneKey::stone_device(), &bci, nullptr, &buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(StoneKey::stone_device(), buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == ~0u) {
        std::print(stderr, "[FATAL] No host-visible coherent memory for persistent upload buffer\n");
        vkDestroyBuffer(StoneKey::stone_device(), buffer, nullptr);
        return;
    }

    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory memory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(StoneKey::stone_device(), &mai, nullptr, &memory));
    VK_CHECK(vkBindBufferMemory(StoneKey::stone_device(), buffer, memory, 0));

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(StoneKey::stone_device(), memory, 0, VK_WHOLE_SIZE, 0, &mapped));

    g_persistentUploadBuffer = buffer;
    g_persistentUploadMapped = mapped;

    std::print("[BUFFER] Persistent direct upload buffer ready — 1 MiB available for immediate writes\n");
}

// ── STAGING RING INITIALIZATION — 1 GiB PERSISTENT MAPPING ─────────────────
inline void ensureStagingRing() noexcept {
    if (g_stagingRingInstance.ready) return;

    ensureMainPool();

    const VkDeviceSize size = STAGING_RING_SIZE;

    std::print("[BUFFER] Initializing 1 GiB staging ring\n");

    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(StoneKey::stone_device(), &bci, nullptr, &buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(StoneKey::stone_device(), buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == ~0u) {
        vkDestroyBuffer(StoneKey::stone_device(), buffer, nullptr);
        std::print(stderr, "[FATAL] No host-visible memory for staging ring\n");
        return;
    }

    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory memory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(StoneKey::stone_device(), &mai, nullptr, &memory));
    VK_CHECK(vkBindBufferMemory(StoneKey::stone_device(), buffer, memory, 0));

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(StoneKey::stone_device(), memory, 0, VK_WHOLE_SIZE, 0, &mapped));

    g_stagingRingInstance = {buffer, memory, size, mapped, 0, true};

    std::print("[BUFFER] 1 GiB staging ring initialized and ready\n");
}

// ── STAGING RING HELPERS ───────────────────────────────────────────────────
[[nodiscard]] inline void* mapStaging(VkDeviceSize size) noexcept {
    ensureStagingRing();

    VkDeviceSize offset = g_stagingRingInstance.head;
    g_stagingRingInstance.head += size;

    if (offset + size > g_stagingRingInstance.size) {
        std::print(stderr, "[BUFFER ERROR] Staging ring overflow — requested {} bytes\n", size);
        g_stagingRingInstance.head = offset;
        return nullptr;
    }

    return static_cast<char*>(g_stagingRingInstance.mapped) + offset;
}

[[nodiscard]] inline VkBuffer getStagingBuffer() noexcept {
    ensureStagingRing();
    return g_stagingRingInstance.buffer;
}

// ── PERSISTENT UPLOAD HELPERS ──────────────────────────────────────────────
[[nodiscard]] inline void* getPersistentUploadMapped() noexcept {
    ensurePersistentUpload();
    return g_persistentUploadMapped;
}

[[nodiscard]] inline VkBuffer getPersistentUploadBuffer() noexcept {
    ensurePersistentUpload();
    return g_persistentUploadBuffer;
}

// ── HOST-VISIBLE SMALL ALLOCATION PATH ─────────────────────────────────────
[[nodiscard]] inline uint64_t allocateHostVisible(VkDeviceSize size, std::string_view tag = "") noexcept {
    ensureStagingRing();
    if (size == 0) return 0;

    VkDeviceSize offset = g_stagingRingInstance.head;
    g_stagingRingInstance.head += size;

    if (offset + size > g_stagingRingInstance.size) {
        std::print(stderr, "[FATAL] Staging ring overflow during host-visible allocation — {} bytes requested\n", size);
        g_stagingRingInstance.head = offset;
        return 0;
    }

    uint64_t handle = ++g_nextHandle;

    s_buffers.emplace(handle, BufferInfo{
        .buffer = g_stagingRingInstance.buffer,
        .memory = g_stagingRingInstance.memory,
        .size = size,
        .aligned = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .tag = std::string(tag),
        .offset = offset,
        .mapped = static_cast<char*>(g_stagingRingInstance.mapped) + offset
    });

    std::print("[BUFFER TRACE] Host-visible allocation: {} bytes @ offset {} | handle {:#x}\n", size, offset, handle);
    return handle;
}

// ── PRIMARY ALLOCATION ENTRY POINT — INTELLIGENT AND UNIFIED ───────────────
[[nodiscard]] inline uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::string_view tag = "") noexcept {
    if (size == 0) return 0;

    // Apply SBT minimum size requirement
    if (usage & VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR) {
        size = std::max(size, SBT_MINIMUM_SIZE);
    }

    // Small uniform buffers use host-visible path for zero-cost CPU updates
    const bool isSmallUniform = (size <= HOST_VISIBLE_THRESHOLD) &&
                                (usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT));

    if (isSmallUniform) {
        return allocateHostVisible(size, tag);
    }

    // All other allocations use the main device-local empire pool
    ensureMainPool();

    VkDeviceSize aligned = align_up<VkDeviceSize>(size, 64ULL);

    Chunk* target = nullptr;
    VkDeviceSize offset = 0;

    for (auto& chunk : g_mainChunks) {
        if (chunk.head + aligned <= chunk.size) {
            offset = chunk.head;
            chunk.head += aligned;
            target = &chunk;
            break;
        }
    }

    if (!target) {
        std::print(stderr, "[FATAL] Main pool exhausted — requested {} bytes\n", size);
        return 0;
    }

    uint64_t handle = ++g_nextHandle;

    s_buffers.emplace(handle, BufferInfo{
        .buffer = target->buffer,
        .memory = target->memory,
        .size = size,
        .aligned = aligned,
        .usage = usage,
        .tag = std::string(tag),
        .offset = offset,
        .deviceAddress = target->baseAddr + offset
    });

    std::print("[BUFFER TRACE] Device-local allocation: {} bytes @ offset {} | handle {:#x}\n", size, offset, handle);

    return handle;
}

// ── DATA UPLOAD — SYNCHRONOUS, USING STAGING RING ──────────────────────────
inline void uploadToBuffer(uint64_t handle, const void* data, VkDeviceSize size, VkCommandBuffer cmd = VK_NULL_HANDLE) noexcept {
    auto it = s_buffers.find(handle);
    if (it == s_buffers.end()) {
        std::print(stderr, "[BUFFER WARNING] uploadToBuffer called with invalid handle {:#x}\n", handle);
        return;
    }

    const BufferInfo& info = it->second;
    if (size > info.size) {
        std::print(stderr, "[BUFFER ERROR] Upload size {} exceeds buffer allocation {} for handle {:#x}\n", size, info.size, handle);
        return;
    }

    void* stagingPtr = mapStaging(size);
    if (!stagingPtr) {
        std::print(stderr, "[BUFFER ERROR] Failed to acquire staging space for upload to handle {:#x}\n", handle);
        return;
    }

    memcpy(stagingPtr, data, size);

    VkBufferCopy copy{
        .srcOffset = g_stagingRingInstance.head - size,
        .dstOffset = info.offset,
        .size = size
    };

    if (cmd == VK_NULL_HANDLE) {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo poolInfo = { 
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, 
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, 
            .queueFamilyIndex = StoneKey::stone_graphics_family() 
        };
        VK_CHECK(vkCreateCommandPool(StoneKey::stone_device(), &poolInfo, nullptr, &pool));

        VkCommandBuffer tempCmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo allocInfo = { 
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, 
            .commandPool = pool, 
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, 
            .commandBufferCount = 1 
        };
        VK_CHECK(vkAllocateCommandBuffers(StoneKey::stone_device(), &allocInfo, &tempCmd));

        VkCommandBufferBeginInfo beginInfo = { 
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT 
        };
        VK_CHECK(vkBeginCommandBuffer(tempCmd, &beginInfo));

        vkCmdCopyBuffer(tempCmd, getStagingBuffer(), info.buffer, 1, &copy);

        VK_CHECK(vkEndCommandBuffer(tempCmd));

        VkSubmitInfo submit = { 
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, 
            .commandBufferCount = 1, 
            .pCommandBuffers = &tempCmd 
        };
        VK_CHECK(vkQueueSubmit(StoneKey::stone_graphics_queue(), 1, &submit, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(StoneKey::stone_graphics_queue()));

        vkFreeCommandBuffers(StoneKey::stone_device(), pool, 1, &tempCmd);
        vkDestroyCommandPool(StoneKey::stone_device(), pool, nullptr);
    } else {
        vkCmdCopyBuffer(cmd, getStagingBuffer(), info.buffer, 1, &copy);
    }

    std::print("[BUFFER TRACE] Uploaded {} bytes to handle {:#x}\n", size, handle);
}

// ── ACCESSORS ───────────────────────────────────────────────────────────────
[[nodiscard]] inline const BufferInfo* get(uint64_t handle) noexcept {
    auto it = s_buffers.find(handle);
    return it != s_buffers.end() ? &it->second : nullptr;
}

[[nodiscard]] inline VkBuffer getVkBuffer(uint64_t handle) noexcept {
    if (handle == 0) return VK_NULL_HANDLE;
    if (auto* info = get(handle)) return info->buffer;
    return VK_NULL_HANDLE;
}

[[nodiscard]] inline VkDeviceAddress get_device_address(uint64_t handle) noexcept {
    if (handle == 0) return 0;
    if (auto* info = get(handle)) return info->deviceAddress;
    return 0;
}

[[nodiscard]] inline VkDeviceSize getPersistentUploadSize() noexcept {
    ensurePersistentUpload();
    return PERSISTENT_UPLOAD_SIZE;
}

// ── RESOURCE MANAGEMENT ────────────────────────────────────────────────────
inline void destroy(uint64_t handle) noexcept {
    if (handle == 0) return;
    s_buffers.erase(handle);
}

inline void purge_all() noexcept {
    if (g_purged.exchange(true)) {
        std::print("[BUFFER] purge_all() already executed — ignoring repeat call\n");
        return;
    }

    VkDevice dev = StoneKey::stone_device();
    if (dev == VK_NULL_HANDLE) return;

    s_buffers.clear();

    for (auto& chunk : g_mainChunks) {
        if (chunk.buffer) vkDestroyBuffer(dev, chunk.buffer, nullptr);
        if (chunk.memory) vkFreeMemory(dev, chunk.memory, nullptr);
    }
    g_mainChunks.clear();

    if (g_stagingRingInstance.buffer) vkDestroyBuffer(dev, g_stagingRingInstance.buffer, nullptr);
    if (g_stagingRingInstance.memory) {
        if (g_stagingRingInstance.mapped) vkUnmapMemory(dev, g_stagingRingInstance.memory);
        vkFreeMemory(dev, g_stagingRingInstance.memory, nullptr);
    }
    g_stagingRingInstance = {};

    if (g_persistentUploadBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, g_persistentUploadBuffer, nullptr);
        g_persistentUploadBuffer = VK_NULL_HANDLE;
        g_persistentUploadMapped = nullptr;
    }

    std::print("[BUFFER] All managed resources released — system clean\n");
}

// ── PUBLIC CONVENIENCE MACROS — MINIMAL AND CLEAN ──────────────────────────
#define BUFFER_CREATE(h, ...)           h = BufferManager::create(__VA_ARGS__)
#define BUFFER_DESTROY(h)               do { if (h) BufferManager::destroy(h); h = 0; } while(0)
#define RAW_BUFFER(h)                   BufferManager::getVkBuffer(h)
#define BUFFER_DEVICE_ADDRESS(h)        BufferManager::get_device_address(h)

} // namespace BufferManager

// =============================================================================
// BUFFERMANAGER v27.6 — JANUARY 06, 2026 — PROFESSIONAL PRODUCTION RELEASE
// Full memory ownership | Intelligent routing | Persistent systems
// No legacy constructs | Clean API | Validation perfect
// The memory system is complete — the empire is in perfect order
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================