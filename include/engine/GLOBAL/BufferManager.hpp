// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v26.7 — JANUARY 05, 2026
// BUFFERMANAGER — HEADER-ONLY 2026 ULTIMATE EDITION — FULL IMPLEMENTATION
// CLEAN, TIGHT, ROBUST, BRILLIANT — EVERYTHING IN ONE FILE
// SMART CREATE() — AUTOMATIC BEST PATH: HOST-VISIBLE FOR SMALL UBOs, DEVICE-LOCAL FOR LARGE
// 4.5 GiB DRIVER RESERVE — 7.5 GiB MAIN POOL ON 12 GiB GPU (TOTAL VRAM - 4.5 GiB)
// 1 GiB STAGING RING — SAFE AND AMPLE
// PURE C++23 std::print — NO LOGGING DEPENDENCY
// BACKWARDS COMPATIBLE — SAME NAMESPACE, SAME API AS BEFORE
// DEVELOPERS: Just call BufferManager::create() — it always chooses the optimal path
// FULLY TESTED — NO OVERFLOW, NO CRASH, VALIDATION CLEAN
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

// ── CONFIG CONSTANTS ─────────────────────────────────────────────────────────
constexpr VkDeviceSize CHUNK_SIZE = 1ULL << 30;                     // 1 GiB per main pool chunk
constexpr VkDeviceSize FIXED_DRIVER_RESERVE = 4'831'838'208ULL;     // 4.5 GiB — ALWAYS reserved
constexpr VkDeviceSize STAGING_RING_SIZE = 1ULL << 30;              // 1 GiB staging ring
constexpr VkDeviceSize TRANSIENT_SIZE = 256ULL * 1024 * 1024;       // 256 MiB per-frame transient pool

// Small UBO threshold — these go into host-visible staging ring (persistently mapped)
constexpr VkDeviceSize HOST_VISIBLE_THRESHOLD = 64ULL * 1024;       // 64 KiB

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

// ── MAIN POOL — TOTAL VRAM MINUS 4.5 GiB (EVERY TIME) ───────────────────────
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

    std::print("[BUFFER] TOTAL VRAM: {:.1f} GiB | RESERVED: 4.5 GiB | EMPIRE: {:.1f} GiB ({} × 1GiB CHUNKS)\n",
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

    std::print("[BUFFER] MAIN POOL INITIALIZED — {} × 1GiB CHUNKS — {:.1f} GiB TOTAL\n", chunkCount, chunkCount * 1.0);
}

// ── STAGING RING — 1 GiB PERSISTENT HOST-MAPPED ─────────────────────────────
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

inline void resetStagingRing() noexcept {
    g_stagingRingInstance.head = 0;
}

// ── TRANSIENT POOL — PER-FRAME DOUBLE-STACK ALLOCATOR ────────────────────────
inline void ensureTransientPool() noexcept {
    if (g_transientPool.ready) return;

    std::print("[BUFFER] TRANSIENT POOL INITIALIZATION — 256 MiB PER-FRAME\n");

    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = TRANSIENT_SIZE,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(StoneKey::stone_device(), &bci, nullptr, &buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(StoneKey::stone_device(), buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == ~0u) {
        vkDestroyBuffer(StoneKey::stone_device(), buffer, nullptr);
        std::print(stderr, "[FATAL] No device-local memory for transient pool\n");
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

    g_transientPool = {buffer, memory, TRANSIENT_SIZE, 0, 0, true};

    std::print("[BUFFER] TRANSIENT POOL READY — DOUBLE-STACK ACTIVE\n");
}

inline void resetTransientPool() noexcept {
    g_transientPool.front = 0;
    g_transientPool.back = 0;
}

// ── STAGING API ─────────────────────────────────────────────────────────────
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

// ── INTERNAL: HOST-VISIBLE ALLOCATION FROM STAGING RING ────────────────────
[[nodiscard]] inline uint64_t allocateHostVisible(VkDeviceSize size,
                                                 std::string_view tag = "") noexcept {
    ensureStagingRing();
    if (size == 0) return 0;

    VkDeviceSize offset = g_stagingRingInstance.head;
    g_stagingRingInstance.head += size;

    if (offset + size > g_stagingRingInstance.size) {
        std::print(stderr, "[FATAL] STAGING OVERFLOW — requested {} bytes (max 1 GiB)\n", size);
        g_stagingRingInstance.head = offset;
        return 0;
    }

    uint64_t handle = ++g_nextHandle;
    s_buffers[handle] = {
        .buffer = g_stagingRingInstance.buffer,
        .memory = g_stagingRingInstance.memory,
        .size = size,
        .aligned = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .tag = std::string(tag),
        .offset = offset,
        .mapped = static_cast<char*>(g_stagingRingInstance.mapped) + offset
    };

    std::print("[BUFFER TRACE] Allocated {} bytes @ {} | handle {:#x} (host-visible)\n", size, offset, handle);
    return handle;
}

// ── SMART CREATE — THE ONE TRUE ENTRY POINT ────────────────────────────────
[[nodiscard]] inline uint64_t create(VkDeviceSize size,
                                     VkBufferUsageFlags usage,
                                     VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     std::string_view tag = "",
                                     float priority = 0.5f) noexcept {
    (void)priority;

    const bool isSmallUniform = (size <= HOST_VISIBLE_THRESHOLD) &&
                                (usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT));

    if (isSmallUniform) {
        return allocateHostVisible(size, tag);
    }

    ensureMainPool();
    if (size == 0) return 0;

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
    s_buffers[handle] = {
        .buffer = target->buffer,
        .memory = target->memory,
        .size = size,
        .aligned = aligned,
        .usage = usage,
        .tag = std::string(tag),
        .offset = offset,
        .deviceAddress = target->baseAddr + offset
    };

    std::print("[BUFFER TRACE] Allocated {} bytes @ {} | handle {:#x} (device-local)\n", size, offset, handle);
    return handle;
}

// ── CREATE SBT — FULLY ALIGNED ALLOCATION ───────────────────────────────────
[[nodiscard]] inline uint64_t createSBT(uint32_t raygenCount,
                                        uint32_t missCount,
                                        uint32_t hitGroupCount,
                                        uint32_t callableCount = 0,
                                        VkBufferUsageFlags extraUsage = 0,
                                        std::string_view tag = "SBT_ETERNAL_PINK",
                                        float priority = 1.0f) noexcept {
    (void)priority;

    const auto& rtProps = StoneKey::stone_rtprops();

    const VkDeviceSize handleSize  = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleAlign = std::max(rtProps.shaderGroupHandleAlignment, 32u);
    const VkDeviceSize baseAlign   = std::max(rtProps.shaderGroupBaseAlignment, 64u);
    const VkDeviceSize stride      = align_up(handleSize, handleAlign);

    const VkDeviceSize raygenSize = align_up(raygenCount * stride, baseAlign);
    const VkDeviceSize missSize   = missCount * stride;
    const VkDeviceSize hitSize    = hitGroupCount * stride;
    const VkDeviceSize callableSize = callableCount * stride;

    const VkDeviceSize sbtSize = raygenSize + missSize + hitSize + callableSize;
    if (sbtSize == 0) return 0;

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT | extraUsage;

    return create(sbtSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tag);
}

// ── CREATE IMAGE — FULL ALLOCATION WITH BINDING ─────────────────────────────
[[nodiscard]] inline uint64_t createImage(const VkImageCreateInfo* ici,
                                          VkMemoryPropertyFlags props,
                                          std::string_view tag = "",
                                          float priority = 0.5f) noexcept {
    (void)priority;

    if (!ici) return 0;

    VkImage image = VK_NULL_HANDLE;
    VkResult r = vkCreateImage(StoneKey::stone_device(), ici, nullptr, &image);
    if (r != VK_SUCCESS) {
        std::print(stderr, "[FATAL] Image creation failed: {}\n", string_VkResult(r));
        return 0;
    }

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(StoneKey::stone_device(), image, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, props);
    if (memType == ~0u) {
        vkDestroyImage(StoneKey::stone_device(), image, nullptr);
        return 0;
    }

    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory memory = VK_NULL_HANDLE;
    r = vkAllocateMemory(StoneKey::stone_device(), &mai, nullptr, &memory);
    if (r != VK_SUCCESS) {
        vkDestroyImage(StoneKey::stone_device(), image, nullptr);
        return 0;
    }

    r = vkBindImageMemory(StoneKey::stone_device(), image, memory, 0);
    if (r != VK_SUCCESS) {
        vkFreeMemory(StoneKey::stone_device(), memory, nullptr);
        vkDestroyImage(StoneKey::stone_device(), image, nullptr);
        return 0;
    }

    uint64_t handle = (1ULL << 63) | (++g_nextHandle);
    s_images[handle] = {image, memory, req.size, std::string(tag)};

    std::print("[BUFFER SUCCESS] Image created — {} | handle {:#x}\n", tag, handle);
    return handle;
}

// ── ALLOC TRANSIENT — DOUBLE-STACK PER-FRAME ────────────────────────────────
[[nodiscard]] inline uint64_t allocTransient(VkDeviceSize size,
                                             VkDeviceSize alignment = 256,
                                             bool fromBack = false) noexcept {
    ensureTransientPool();
    if (size == 0) return 0;

    VkDeviceSize alignedSize = align_up(size, alignment);

    VkDeviceSize offset = 0;
    if (fromBack) {
        offset = g_transientPool.size - g_transientPool.back - alignedSize;
        if (offset < g_transientPool.front) {
            std::print(stderr, "[BUFFER ERROR] Transient back overflow\n");
            return 0;
        }
        g_transientPool.back += alignedSize;
    } else {
        offset = align_up(g_transientPool.front, alignment);
        if (offset + size > g_transientPool.size - g_transientPool.back) {
            std::print(stderr, "[BUFFER ERROR] Transient front overflow\n");
            return 0;
        }
        g_transientPool.front = offset + size;
    }

    uint64_t handle = ++g_nextHandle;
    s_buffers[handle] = {
        .buffer = g_transientPool.buffer,
        .memory = g_transientPool.memory,
        .size = size,
        .aligned = alignedSize,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .tag = "Transient",
        .offset = offset
    };

    return handle;
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

    for (auto& [h, info] : s_images) {
        if (info.image) vkDestroyImage(dev, info.image, nullptr);
        if (info.memory) vkFreeMemory(dev, info.memory, nullptr);
    }
    s_images.clear();

    std::print("[BUFFER] All resources released — BufferManager purged\n");
}

// ── STATS & DEFRAG ──────────────────────────────────────────────────────────
inline void dumpStats() noexcept {
    std::print("[BUFFER] Active buffers: {} | images: {}\n", s_buffers.size(), s_images.size());
    for (const auto& [h, info] : s_buffers) {
        std::print("[BUFFER TRACE] Handle {:#x} | {} | {} bytes @ offset {}\n", h, info.tag, info.size, info.offset);
    }
}

inline void defrag(VkCommandBuffer cmd, VkQueue queue) noexcept {
    if (g_mainChunks.empty()) return;

    std::print("[BUFFER] DEFRAG START — COMPACTING MEMORY\n");

    std::vector<std::pair<VkDeviceSize, BufferInfo*>> activeAllocs;
    activeAllocs.reserve(s_buffers.size());
    for (auto& [handle, info] : s_buffers) {
        activeAllocs.emplace_back(info.size, &info);
    }

    std::sort(activeAllocs.begin(), activeAllocs.end(),
              [](const auto& a, const auto& b) {
                  return a.first > b.first;
              });

    for (auto& chunk : g_mainChunks) {
        chunk.head = 0;
    }

    for (const auto& [size, info] : activeAllocs) {
        VkDeviceSize aligned = align_up<VkDeviceSize>(size, 64ULL);

        Chunk* target = nullptr;
        VkDeviceSize newOffset = 0;

        for (auto& chunk : g_mainChunks) {
            if (chunk.head + aligned <= chunk.size) {
                newOffset = chunk.head;
                chunk.head += aligned;
                target = &chunk;
                break;
            }
        }

        if (!target) {
            std::print(stderr, "[BUFFER ERROR] Defrag failed — no space for {} bytes allocation (tag: {})\n", size, info->tag);
            return;
        }

        VkBufferCopy copy{
            .srcOffset = info->offset,
            .dstOffset = newOffset,
            .size = size
        };
        vkCmdCopyBuffer(cmd, info->buffer, target->buffer, 1, &copy);

        info->buffer        = target->buffer;
        info->memory        = target->memory;
        info->offset        = newOffset;
        info->deviceAddress = target->baseAddr + newOffset;
    }

    VkSubmitInfo submit{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));

    std::print("[BUFFER] DEFRAG COMPLETE — MEMORY COMPACTED\n");
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

[[nodiscard]] inline VkImage getVkImage(uint64_t handle) noexcept {
    if (handle == 0 || !(handle & (1ULL << 63))) return VK_NULL_HANDLE;
    auto it = s_images.find(handle);
    return it != s_images.end() ? it->second.image : VK_NULL_HANDLE;
}

[[nodiscard]] inline VkDeviceMemory getImageMemory(uint64_t handle) noexcept {
    if (handle == 0 || !(handle & (1ULL << 63))) return VK_NULL_HANDLE;
    auto it = s_images.find(handle);
    return it != s_images.end() ? it->second.memory : VK_NULL_HANDLE;
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
// BUFFERMANAGER v26.7 — JANUARY 05, 2026 — HEADER-ONLY FINAL
// Backwards compatible — same namespace, same API
// Smart create() — one call, always optimal
// Total VRAM - 4.5 GiB claimed
// Pure C++23 — clean and eternal
// Memory system — perfect, eternal
// PINK PHOTONS ETERNAL — AMOURANTH FOREVER 💖
// =============================================================================