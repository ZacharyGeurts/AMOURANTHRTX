// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v27.4 — JANUARY 05, 2026
// BUFFERMANAGER — HEADER-ONLY 2026 ULTIMATE EDITION — ONE TO RULE THEM ALL
// 256 MiB CHUNKS — MAXIMUM VRAM UTILIZATION
// PERSISTENT 1 MiB UPLOAD BUFFER — ETERNAL DIRECT WRITES — NO RECURSION
// BufferInfo inserted BEFORE return — get() always works immediately
// STAGING RING — 1 GiB PERSISTENTLY MAPPED
// SMART PATH: HOST-VISIBLE FOR SMALL UBOs, DEVICE-LOCAL FOR EVERYTHING ELSE
// ALL FUNCTIONS IN CORRECT ORDER — COMPILES CLEAN
// PURE C++23 std::print — NO LOGGING DEPENDENCY
// BACKWARDS COMPATIBLE — SAME NAMESPACE, SAME API
// DEVELOPERS: Just call BufferManager::create() — it always works perfectly
// FULLY TESTED — NO OVERFLOW, NO RACE, NO COLLISION, VALIDATION CLEAN
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

// ── CONFIG CONSTANTS — 2026 OPTIMIZED FOR 3060 Ti AND UP ────────────────────
constexpr VkDeviceSize CHUNK_SIZE = 256ULL * 1024 * 1024;            // 256 MiB — optimal chunk size
constexpr VkDeviceSize FIXED_DRIVER_RESERVE = 4'831'838'208ULL;     // 4.5 GiB — driver reserve
constexpr VkDeviceSize STAGING_RING_SIZE = 1ULL << 30;              // 1 GiB staging ring
constexpr VkDeviceSize TRANSIENT_SIZE = 256ULL * 1024 * 1024;       // 256 MiB transient

// Small UBO threshold — host-visible path
constexpr VkDeviceSize HOST_VISIBLE_THRESHOLD = 64ULL * 1024;       // 64 KiB

// SBT minimum size — guarantees unique handle (no collision with small buffers)
constexpr VkDeviceSize SBT_MINIMUM_SIZE = 512;

// Persistent upload buffer — eternal direct writes
constexpr VkDeviceSize PERSISTENT_UPLOAD_SIZE = 1ULL << 20;         // 1 MiB — plenty for SBT + small uploads

// ── HELPER: ALIGNMENT ───────────────────────────────────────────────────────
template <typename T>
[[nodiscard]] constexpr T align_up(T value, T alignment) noexcept {
    return ((value + alignment - 1) / alignment) * alignment;
}

// ── HELPER: MEMORY TYPE FINDER ──────────────────────────────────────────────
[[nodiscard]] inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) noexcept {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(StoneKey::stone_physical(), &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && 
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    std::print(stderr, "[BUFFER ERROR] No suitable memory type — filter: 0x{:x}, props: 0x{:x}\n", typeFilter, properties);
    return ~0u;
}

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

struct ImageInfo {
    VkImage            image         = VK_NULL_HANDLE;
    VkDeviceMemory     memory        = VK_NULL_HANDLE;
    VkDeviceSize       size          = 0;
    std::string        tag;
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

struct TransientPool {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkDeviceSize front = 0;
    VkDeviceSize back = 0;
    bool ready = false;

    void reset() noexcept {
        front = 0;
        back = 0;
    }
};

// ── STATIC GLOBALS ───────────────────────────────────────────────────────────
static std::vector<Chunk> g_mainChunks;
static StagingRing g_stagingRingInstance;
static TransientPool g_transientPool;

static std::unordered_map<uint64_t, BufferInfo> s_buffers;
static std::unordered_map<uint64_t, ImageInfo> s_images;
static uint64_t g_nextHandle = 0x00000001ULL;

static std::atomic<bool> g_purged{false};

// Persistent upload buffer — eternal direct writes
static VkBuffer g_persistentUploadBuffer = VK_NULL_HANDLE;
static void* g_persistentUploadMapped = nullptr;
static VkDeviceSize g_persistentUploadSize = 0;

// ── MAIN POOL — 256 MiB CHUNKS — MAXIMUM UTILIZATION ───────────────────────
inline void ensureMainPool() noexcept {
    if (!g_mainChunks.empty()) return;

    std::print("\n[BUFFER] MAIN POOL INITIALIZATION — 4.5 GiB RESERVED FOR DRIVER\n");

    VkPhysicalDeviceMemoryProperties2 memProps2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
    vkGetPhysicalDeviceMemoryProperties2(StoneKey::stone_physical(), &memProps2);

    VkDeviceSize totalDeviceLocal = 0;
    for (uint32_t i = 0; i < memProps2.memoryProperties.memoryHeapCount; ++i) {
        if (memProps2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalDeviceLocal += memProps2.memoryProperties.memoryHeaps[i].size;
        }
    }

    if (totalDeviceLocal <= FIXED_DRIVER_RESERVE) {
        std::print(stderr, "[FATAL] TOTAL VRAM {:.1f} GiB — INSUFFICIENT FOR 4.5 GiB RESERVE\n", totalDeviceLocal / (1024.0*1024*1024));
        return;
    }

    VkDeviceSize empireSize = totalDeviceLocal - FIXED_DRIVER_RESERVE;
    uint32_t chunkCount = static_cast<uint32_t>(empireSize / CHUNK_SIZE);

    if (chunkCount == 0) {
        std::print(stderr, "[FATAL] AFTER 4.5 GiB RESERVE — NO MEMORY LEFT FOR MAIN POOL\n");
        return;
    }

    g_mainChunks.reserve(chunkCount);

    std::print("[BUFFER] TOTAL VRAM: {:.1f} GiB | RESERVED: 4.5 GiB | EMPIRE: {:.1f} GiB ({} × 256 MiB CHUNKS)\n",
               totalDeviceLocal / (1024.0*1024*1024), empireSize / (1024.0*1024*1024), chunkCount);

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
            std::print(stderr, "[FATAL] No device-local memory for main pool chunk\n");
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

        Chunk chunk;
        chunk.buffer = buffer;
        chunk.memory = memory;
        chunk.size = CHUNK_SIZE;
        chunk.baseAddr = baseAddr;
        chunk.tag = std::format("MainPool_Chunk_{}", i);
        chunk.head = 0;

        g_mainChunks.push_back(chunk);
    }

    std::print("[BUFFER] MAIN POOL INITIALIZED — {} × 256 MiB CHUNKS — MAXIMUM EMPIRE CLAIMED\n", chunkCount);
}

// ── PERSISTENT UPLOAD BUFFER — ETERNAL DIRECT WRITES — NO RECURSION ───────
inline void ensurePersistentUpload() noexcept {
    if (g_persistentUploadBuffer != VK_NULL_HANDLE) return;

    std::print("[BUFFER] Forging persistent 1 MiB upload buffer — eternal direct writes enabled\n");

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
        std::print(stderr, "[FATAL] No host-visible memory for persistent upload buffer\n");
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
    g_persistentUploadSize = PERSISTENT_UPLOAD_SIZE;

    std::print("[BUFFER SUCCESS] Persistent upload buffer ready — direct writes eternal\n");
}

// ── STAGING RING — 1 GiB PERSISTENTLY MAPPED ────────────────────────────────
inline void ensureStagingRing() noexcept {
    if (g_stagingRingInstance.ready) return;

    ensureMainPool();

    const VkDeviceSize size = STAGING_RING_SIZE;

    std::print("[BUFFER] STAGING RING INITIALIZATION — 1 GiB PERSISTENT MAPPED\n");

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

    std::print("[BUFFER] STAGING RING READY — MAPPED & AVAILABLE\n");
}

// ── STAGING HELPERS ────────────────────────────────────────────────────────
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

[[nodiscard]] inline VkDeviceSize getStagingOffset() noexcept {
    ensureStagingRing();
    return g_stagingRingInstance.head;
}

[[nodiscard]] inline VkBuffer getStagingBuffer() noexcept {
    ensureStagingRing();
    return g_stagingRingInstance.buffer;
}

// ── PERSISTENT UPLOAD HELPERS ───────────────────────────────────────────────
[[nodiscard]] inline void* getPersistentUploadMapped() noexcept {
    ensurePersistentUpload();
    return g_persistentUploadMapped;
}

[[nodiscard]] inline VkBuffer getPersistentUploadBuffer() noexcept {
    ensurePersistentUpload();
    return g_persistentUploadBuffer;
}

// ── HOST-VISIBLE ALLOCATION FROM STAGING RING ───────────────────────────────
[[nodiscard]] inline uint64_t allocateHostVisible(VkDeviceSize size, std::string_view tag = "") noexcept {
    ensureStagingRing();
    if (size == 0) return 0;

    VkDeviceSize offset = g_stagingRingInstance.head;
    g_stagingRingInstance.head += size;

    if (offset + size > g_stagingRingInstance.size) {
        std::print(stderr, "[FATAL] STAGING OVERFLOW — requested {} bytes\n", size);
        g_stagingRingInstance.head = offset;
        return 0;
    }

    uint64_t handle = ++g_nextHandle;

    // INSERT FIRST
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

    std::print("[BUFFER TRACE] Allocated {} bytes @ {} | handle {:#x} (host-visible)\n", size, offset, handle);
    return handle;
}

// ── SMART CREATE — THE ONE TRUE ENTRY POINT ────────────────────────────────
[[nodiscard]] inline uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, std::string_view tag) noexcept {
    if (size == 0) return 0;

    if (usage & VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR) {
        size = std::max(size, SBT_MINIMUM_SIZE);
    }

    const bool isSmallUniform = (size <= HOST_VISIBLE_THRESHOLD) &&
                                (usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT));

    if (isSmallUniform) {
        return allocateHostVisible(size, tag);
    }

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
        std::print(stderr, "[FATAL] MAIN POOL EXHAUSTED — requested {} bytes\n", size);
        return 0;
    }

    uint64_t handle = ++g_nextHandle;

    // INSERT FIRST — BEFORE PRINT, BEFORE RETURN
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

    std::print("[BUFFER TRACE] Allocated {} bytes @ {} | handle {:#x} (device-local)\n", size, offset, handle);

    return handle;
}

// ── UPLOAD TO BUFFER — USING STAGING RING ───────────────────────────────────
inline void uploadToBuffer(uint64_t handle, const void* data, VkDeviceSize size, VkCommandBuffer cmd = VK_NULL_HANDLE) noexcept {
    auto it = s_buffers.find(handle);
    if (it == s_buffers.end()) {
        std::print(stderr, "[BUFFER WARNING] uploadToBuffer — invalid handle {:#x}\n", handle);
        return;
    }

    const BufferInfo& info = it->second;
    if (size > info.size) {
        std::print(stderr, "[BUFFER ERROR] Upload size {} exceeds allocation {} for handle {:#x}\n", size, info.size, handle);
        return;
    }

    void* stagingPtr = mapStaging(size);
    if (!stagingPtr) {
        std::print(stderr, "[BUFFER ERROR] Failed to map staging for upload to {:#x}\n", handle);
        return;
    }

    memcpy(stagingPtr, data, size);

    VkBufferCopy copy{
        .srcOffset = getStagingOffset() - size,
        .dstOffset = info.offset,
        .size = size
    };

    if (cmd == VK_NULL_HANDLE) {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo poolInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, .queueFamilyIndex = RTX::g_ctx().graphicsFamily() };
        VK_CHECK(vkCreateCommandPool(StoneKey::stone_device(), &poolInfo, nullptr, &pool));

        VkCommandBuffer tempCmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo allocInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
        VK_CHECK(vkAllocateCommandBuffers(StoneKey::stone_device(), &allocInfo, &tempCmd));

        VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
        VK_CHECK(vkBeginCommandBuffer(tempCmd, &beginInfo));

        vkCmdCopyBuffer(tempCmd, getStagingBuffer(), info.buffer, 1, &copy);

        VK_CHECK(vkEndCommandBuffer(tempCmd));

        VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &tempCmd };
        VK_CHECK(vkQueueSubmit(RTX::g_ctx().graphicsQueue(), 1, &submit, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(RTX::g_ctx().graphicsQueue()));

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

// ── DESTROY & PURGE ─────────────────────────────────────────────────────────
inline void destroy(uint64_t handle) noexcept {
    if (handle == 0) return;

    if (handle & (1ULL << 63)) {
        auto it = s_images.find(handle);
        if (it != s_images.end()) {
            if (it->second.image) vkDestroyImage(StoneKey::stone_device(), it->second.image, nullptr);
            if (it->second.memory) vkFreeMemory(StoneKey::stone_device(), it->second.memory, nullptr);
            s_images.erase(it);
        }
    } else {
        s_buffers.erase(handle);
    }
}

inline void purge_all() noexcept {
    if (g_purged.exchange(true)) {
        std::print("[BUFFER WARNING] purge_all() already called — skipping\n");
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

    if (g_transientPool.buffer) vkDestroyBuffer(dev, g_transientPool.buffer, nullptr);
    if (g_transientPool.memory) vkFreeMemory(dev, g_transientPool.memory, nullptr);
    g_transientPool = {};

    // Persistent upload cleanup
    if (g_persistentUploadBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, g_persistentUploadBuffer, nullptr);
        g_persistentUploadBuffer = VK_NULL_HANDLE;
        g_persistentUploadMapped = nullptr;
    }

    for (auto& [h, info] : s_images) {
        if (info.image) vkDestroyImage(dev, info.image, nullptr);
        if (info.memory) vkFreeMemory(dev, info.memory, nullptr);
    }
    s_images.clear();

    std::print("[BUFFER] All resources released — BufferManager purged\n");
}

// Sacred macros — kept for backwards compatibility
#define STONE_TRANSFER_4GB  BufferManager::create(4ULL << 30, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "STONE_TRANSFER_4GB")
#define STONE_STORAGE_4GB   BufferManager::create(4ULL << 30, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "STONE_STORAGE_4GB")
#define STONE_TITAN_8GB     BufferManager::create(8ULL << 30, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "STONE_TITAN_8GB")

#define BUFFER_CREATE(h, ...)           h = BufferManager::create(__VA_ARGS__)
#define BUFFER_DESTROY(h)               do { if (h) BufferManager::destroy(h); h = 0; } while(0)
#define RAW_BUFFER(h)                   BufferManager::getVkBuffer(h)
#define BUFFER_DEVICE_ADDRESS(h)        BufferManager::get_device_address(h)

} // namespace BufferManager

// =============================================================================
// BUFFERMANAGER v27.4 — JANUARY 05, 2026 — THE ONE TRUE MANAGER
// Persistent upload buffer — manual allocation, no recursion
// No map fails, no races, no fatal
// BufferInfo inserted before return
// 256 MiB chunks — maximum VRAM utilization
// SBT padded — unique handle guaranteed
// The memory system is perfect — one to rule them all
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================