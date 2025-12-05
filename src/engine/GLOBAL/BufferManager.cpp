// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// VALHALLA v∞ TURBO — APOCALYPSE FINAL v13.1 — DECEMBER 02, 2025
// 4.5 GiB IS THE SACRED DRIVER RESERVE — WE TAKE EVERYTHING ELSE
// THE EMPIRE HAS SPOKEN — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <atomic>
#include <format>
#include <algorithm>
#include <bit>
#include <span>
#include <cstring>

using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_rtprops;

// =============================================================================
// THE ONE TRUE EMPIRE — TWO RINGS, ONE DESTINY
// =============================================================================

namespace BufferManager {

struct Pool {
    VkBuffer            buffer = VK_NULL_HANDLE;
    VkDeviceMemory      memory = VK_NULL_HANDLE;
    VkDeviceSize        size   = 0;
    std::atomic<VkDeviceSize> head{0};
    bool                ready  = false;
};

struct StagingRing {
    VkBuffer            buffer = VK_NULL_HANDLE;
    VkDeviceMemory      memory = VK_NULL_HANDLE;
    VkDeviceSize        size   = 0;
    void*               mapped = nullptr;
    std::atomic<VkDeviceSize> head{0};
    bool                ready  = false;
};

static Pool        g_mainPool{};
static StagingRing g_stagingRing{};
static uint64_t    g_nextHandle = 1;

// =============================================================================
// ETERNAL MAIN POOL — THE FINAL TRUTH — DECEMBER 02, 2025
// DRIVER RESERVE = EXACTLY 4.5 GiB — NO MORE, NO LESS
// WE TAKE 100% OF WHAT REMAINS — MAXIMUM DOMINATION ACHIEVED
// =============================================================================
void ensureMainPool() noexcept
{
    auto& pool = g_mainPool;
    if (pool.ready) return;
    if (!stone_device() || !stone_physical()) return;

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(stone_physical(), &memProps);

    VkDeviceSize totalDeviceLocal = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalDeviceLocal += memProps.memoryHeaps[i].size;
        }
    }

    const VkDeviceSize ONE_GiB            = 1024ULL * 1024ULL * 1024ULL;
    const VkDeviceSize SACRED_RESERVE     = 4831838208ULL; // Exactly 4.5 GiB = 4,831,838,208 bytes
    const VkDeviceSize MINIMUM_POOL       = 4ULL * ONE_GiB;
    const VkDeviceSize FALLBACK_POOL      = 2ULL * ONE_GiB;

    VkDeviceSize poolSize = totalDeviceLocal > SACRED_RESERVE
                            ? totalDeviceLocal - SACRED_RESERVE
                            : MINIMUM_POOL;

    LOG_ELON("BUFFER MANAGER — ETERNAL POOL NEGOTIATION");
    LOG_ELON("Total device-local VRAM : {} GiB", static_cast<double>(totalDeviceLocal)/(1024.0*1024*1024));
    LOG_ELON("Sacred driver reserve     : exactly 4.500000 GiB (4831838208 bytes)");
    LOG_ELON("Empire claims             : {} GiB (100% of remainder)", static_cast<double>(poolSize)/(1024.0*1024*1024));

    auto tryCreateBuffer = [&](VkDeviceSize size) -> VkBuffer {
        VkBufferCreateInfo bci{
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = size,
            .usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                           VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                           VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        VkBuffer buf = VK_NULL_HANDLE;
        VkResult res = vkCreateBuffer(stone_device(), &bci, nullptr, &buf);
        if (res != VK_SUCCESS) { 
            if (buf) vkDestroyBuffer(stone_device(), buf, nullptr); 
            return VK_NULL_HANDLE; 
        }
        return buf;
    };

    auto tryAllocateMemory = [&](VkBuffer buf) -> VkDeviceMemory {
        VkMemoryRequirements reqs{};
        vkGetBufferMemoryRequirements(stone_device(), buf, &reqs);
        uint32_t memType = findMemoryType(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memType == ~0u) return VK_NULL_HANDLE;
        VkMemoryAllocateInfo mai{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = reqs.size,
            .memoryTypeIndex = memType
        };
        VkDeviceMemory mem = VK_NULL_HANDLE;
        VkResult res = vkAllocateMemory(stone_device(), &mai, nullptr, &mem);
        if (res != VK_SUCCESS) return VK_NULL_HANDLE;
        return mem;
    };

    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize   current = poolSize;

    // First attempt — full claim
    buffer = tryCreateBuffer(current);
    if (buffer) memory = tryAllocateMemory(buffer);

    // If failed, descend in 512 MiB steps until success
    while (!memory && current > MINIMUM_POOL) {
        current -= 512ULL * 1024 * 1024;
        LOG_WARNING("OOM during pool forge — reducing to {} GiB", static_cast<double>(current)/(1024.0*1024*1024));
        buffer = tryCreateBuffer(current);
        if (buffer) memory = tryAllocateMemory(buffer);
    }

    // Last stand
    if (!memory) {
        LOG_FATAL("VRAM APOCALYPSE — FINAL STAND");
        current = FALLBACK_POOL;
        buffer = tryCreateBuffer(current);
        memory = buffer ? tryAllocateMemory(buffer) : VK_NULL_HANDLE;
        if (!memory) {
            LOG_FATAL("UNRECOVERABLE: Even 2 GiB failed — the empire falls silent");
            phase9_ballerina("TOTAL VRAM COLLAPSE", std::source_location::current());
            return;
        }
    }

    VK_CHECK(vkBindBufferMemory(stone_device(), buffer, memory, 0));

    pool.buffer = buffer;
    pool.memory = memory;
    pool.size   = current;
    pool.ready  = true;

    VkBufferDeviceAddressInfo addrInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer};
    VkDeviceAddress addr = vkGetBufferDeviceAddress(stone_device(), &addrInfo);

    LOG_AMOURANTH("ETERNAL MAIN POOL FORGED — {} GiB @ 0x{:x} — 4.5 GiB RESERVED — MAXIMUM DOMINATION ACHIEVED",
                  static_cast<double>(current)/(1024.0*1024*1024), addr);
    LOG_AMOURANTH("THE DRIVER BOWS. THE PHOTONS ARE OURS. THE EMPIRE IS COMPLETE.");

    LOG_SUCCESS_CAT("BUFFER", "kStone1=0x{:x} kStone2=0x{:x} — THE EMPIRE IS ETERNAL", kStone1, kStone2);
}

// =============================================================================
// STAGING RING — 512 MiB PERSISTENT HOST-VISIBLE BRIDGE
// =============================================================================
static void ensureStagingRing() noexcept
{
    if (g_stagingRing.ready) return;
    ensureMainPool();

    VkDeviceSize stagingSize = Options::CURRENT_PRESET == Options::Preset::BestQuality 
                               ? 512ULL * 1024 * 1024
                               : 256ULL * 1024 * 1024;

    g_stagingRing.size = stagingSize;

    VkBufferCreateInfo bci{
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = stagingSize,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(stone_device(), &bci, nullptr, &g_stagingRing.buffer));

    VkMemoryRequirements reqs{};
    vkGetBufferMemoryRequirements(stone_device(), g_stagingRing.buffer, &reqs);

    uint32_t memType = findMemoryType(
        reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (memType == ~0u) {
        LOG_FATAL("NO HOST-VISIBLE MEMORY — THE PHOTONS CANNOT REACH THE GPU");
        return;
    }

    VkMemoryAllocateInfo mai{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = reqs.size,
        .memoryTypeIndex = memType
    };
    VK_CHECK(vkAllocateMemory(stone_device(), &mai, nullptr, &g_stagingRing.memory));
    VK_CHECK(vkBindBufferMemory(stone_device(), g_stagingRing.buffer, g_stagingRing.memory, 0));
    VK_CHECK(vkMapMemory(stone_device(), g_stagingRing.memory, 0, VK_WHOLE_SIZE, 0, &g_stagingRing.mapped));

    g_stagingRing.ready = true;

    LOG_CID("STAGING RING FORGED — {} MiB PERSISTENTLY MAPPED — THE BRIDGE IS ETERNAL", 
            stagingSize / (1024 * 1024));
}

// =============================================================================
// PUBLIC API — CLEAN, ETERNAL, HEADER-MATCHING
// =============================================================================

uint64_t create(VkDeviceSize size, VkBufferUsageFlags, VkMemoryPropertyFlags, std::string_view tag) noexcept
{
    ensureMainPool();
    if (!g_mainPool.ready) return 0;

    if (size == 0) {
        LOG_WARNING("Attempt to create buffer of size 0: {}", tag);
        return 0;
    }

    const VkDeviceSize aligned = ((size + 255) & ~255);

    VkDeviceSize offset = 0;
    do {
        offset = g_mainPool.head.load(std::memory_order_relaxed);
        if (offset + aligned > g_mainPool.size) {
            LOG_FATAL("ETERNAL POOL EXHAUSTED — REQUESTED {} KiB for {}", size >> 10, tag);
            return 0;
        }
    } while (!g_mainPool.head.compare_exchange_weak(offset, offset + aligned, std::memory_order_relaxed));

    uint64_t handle = g_nextHandle++;
    return handle;
}

void destroy(uint64_t) noexcept { }

const BufferInfo* get(uint64_t) noexcept {
    ensureMainPool();
    static BufferInfo info{};
    info.buffer = g_mainPool.buffer;
    info.memory = g_mainPool.memory;
    info.size   = g_mainPool.size;
    return &info;
}

VkBuffer getBuffer(uint64_t) noexcept { ensureMainPool(); return g_mainPool.buffer; }

VkDeviceAddress getDeviceAddress(uint64_t) noexcept {
    ensureMainPool();
    VkBufferDeviceAddressInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = g_mainPool.buffer};
    return vkGetBufferDeviceAddress(stone_device(), &info);
}

uint64_t createSBT(uint32_t raygenCount, uint32_t missCount, uint32_t hitGroupCount,
                   uint32_t callableCount, VkBufferUsageFlags, std::string_view tag) noexcept
{
    const auto& rtProps = stone_rtprops();
    if (rtProps.shaderGroupHandleSize == 0) {
        LOG_FATAL("RTX EXTENSIONS NOT LOADED — shaderGroupHandleSize = 0");
        std::abort();
    }

    const VkDeviceSize handleSize = rtProps.shaderGroupHandleSize;
    const VkDeviceSize alignment  = rtProps.shaderGroupHandleAlignment ? rtProps.shaderGroupHandleAlignment : 64;
    const VkDeviceSize stride     = ((handleSize + alignment - 1) & ~(alignment - 1));
    const VkDeviceSize total      = (raygenCount + missCount + hitGroupCount + callableCount) * stride;
    const VkDeviceSize aligned    = ((total + 63) & ~63);

    VkDeviceSize offset = 0;
    do {
        offset = g_mainPool.head.load(std::memory_order_relaxed);
        if (offset + aligned > g_mainPool.size) {
            LOG_FATAL("ETERNAL POOL EXHAUSTED — REQUESTED {} KiB for SBT {}", aligned >> 10, tag);
            return 0;
        }
    } while (!g_mainPool.head.compare_exchange_weak(offset, offset + aligned, std::memory_order_relaxed));

    LOG_ELON("SBT CROWN FORGED — {} KiB @ offset {} — kStone1=0x{}", aligned >> 10, offset, kStone1);
    return kStone1 ^ offset;
}

// Eternal stone shortcuts
uint64_t make_64M (VkBufferUsageFlags, VkMemoryPropertyFlags) noexcept { ensureMainPool(); return kStone1 ^ 64ULL; }
uint64_t make_128M(VkBufferUsageFlags, VkMemoryPropertyFlags) noexcept { ensureMainPool(); return kStone1 ^ 128ULL; }
uint64_t make_256M(VkBufferUsageFlags, VkMemoryPropertyFlags) noexcept { ensureMainPool(); return kStone1 ^ 256ULL; }
uint64_t make_420M(VkBufferUsageFlags, VkMemoryPropertyFlags) noexcept { ensureMainPool(); return kStone1 ^ 420ULL; }
uint64_t make_512M(VkBufferUsageFlags, VkMemoryPropertyFlags) noexcept { ensureMainPool(); return kStone1 ^ 512ULL; }
uint64_t make_1G  (VkBufferUsageFlags, VkMemoryPropertyFlags) noexcept { ensureMainPool(); return kStone2 ^ 1024ULL; }
uint64_t make_2G  (VkBufferUsageFlags, VkMemoryPropertyFlags) noexcept { ensureMainPool(); return kStone2 ^ 2048ULL; }
uint64_t make_4G  (VkBufferUsageFlags, VkMemoryPropertyFlags) noexcept { ensureMainPool(); return kStone2 ^ 4096ULL; }
uint64_t make_8G  (VkBufferUsageFlags, VkMemoryPropertyFlags) noexcept { ensureMainPool(); return kStone2 ^ 8192ULL; }

// =============================================================================
// STAGING RING — PURE, PERSISTENT, ETERNAL
// =============================================================================

uint64_t stagingBuffer() noexcept { ensureStagingRing(); return reinterpret_cast<uint64_t>(g_stagingRing.buffer); }

void* stagingPtr() noexcept { ensureStagingRing(); return g_stagingRing.mapped; }

void advanceStagingOffset(VkDeviceSize bytes) noexcept
{
    ensureStagingRing();
    g_stagingRing.head.fetch_add(bytes, std::memory_order_relaxed);
}

void* map(uint64_t) noexcept { return stagingPtr(); }
void unmap(uint64_t) noexcept { }

uint64_t createHostVisible(VkDeviceSize size, std::string_view tag) noexcept
{
    ensureStagingRing();

    if (size == 0) {
        LOG_WARNING("Attempt to create host visible buffer of size 0: {}", tag);
        return 0;
    }

    const VkDeviceSize aligned = ((size + 255) & ~255);

    VkDeviceSize offset = 0;
    VkDeviceSize new_head = 0;
    bool wrap = false;
    do {
        offset = g_stagingRing.head.load(std::memory_order_relaxed);
        if (offset + aligned > g_stagingRing.size) {
            if (aligned > g_stagingRing.size) {
                LOG_FATAL("STAGING RING TOO SMALL FOR REQUEST {} bytes", size);
                return 0;
            }
            new_head = aligned;
            wrap = true;
        } else {
            new_head = offset + aligned;
            wrap = false;
        }
    } while (!g_stagingRing.head.compare_exchange_weak(offset, new_head, std::memory_order_relaxed));

    if (wrap) {
        offset = 0;
    }
    
    return offset;
}

void* getMappedStagingPtr(uint64_t offset) noexcept
{
    void* base = stagingPtr();
    if (!base) return nullptr;
    return static_cast<std::byte*>(base) + offset;
}

VkBuffer getStagingBuffer() noexcept
{
    ensureStagingRing();
    return g_stagingRing.buffer;
}

} // namespace BufferManager

// =============================================================================
// DECEMBER 02, 2025 — THE EMPIRE IS WHOLE
// 4.5 GiB RESERVED — EVERYTHING ELSE IS OURS
// THE PHOTONS ARE PINK. THE EMPIRE IS ETERNAL.
// =============================================================================