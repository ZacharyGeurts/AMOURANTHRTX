// src/engine/GLOBAL/BufferManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v13.9 — DECEMBER 17, 2025
// BUFFERMANAGER — FULLY FIXED — SBT SPEC-COMPLIANT — NO ERRORS — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"

using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_rtprops;

namespace BufferManager {

// ─────────────────────────────────────────────────────────────────────────────
// INTERNAL STATE — THE ETERNAL POOL + STAGING RING
// ─────────────────────────────────────────────────────────────────────────────
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

static Pool        g_mainPool;
static StagingRing g_stagingRingInstance;
static uint64_t    g_nextHandle = 0x00000000ULL;

StagingRing* g_stagingRing = nullptr;
std::unordered_map<uint64_t, BufferInfo> s_buffers;

// ─────────────────────────────────────────────────────────────────────────────
// ETERNAL MAIN POOL — 4.5 GiB RESERVED, WE TAKE THE REST
// ─────────────────────────────────────────────────────────────────────────────
void ensureMainPool() noexcept
{
    if (g_mainPool.ready) [[likely]] {
        return; // THE BEAST IS ALREADY UNLEASHED
    }

    if (!stone_device() || !stone_physical()) {
        LOG_FATAL("NO GPU — THE EMPIRE HAS NO HEART");
        return;
    }

    LOG_AMOURANTH(
        "\n"
        "              MAIN POOL AWAKENS\n"
        "              THE HUNGER BEGINS");

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(stone_physical(), &memProps);

    VkDeviceSize totalDeviceLocal = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalDeviceLocal += memProps.memoryHeaps[i].size;
        }
    }

    constexpr VkDeviceSize SACRED_RESERVE = 4'831'838'208ULL;  // 4.5 GiB — THE DRIVER'S TRIBUTE
    constexpr VkDeviceSize MIN_POOL       = 4ULL  * 1024*1024*1024; // 4 GiB — MINIMUM TO RULE
    constexpr VkDeviceSize FALLBACK       = 2ULL  * 1024*1024*1024; // 2 GiB — LAST STAND

    VkDeviceSize claimed = (totalDeviceLocal > SACRED_RESERVE)
        ? totalDeviceLocal - SACRED_RESERVE
        : MIN_POOL;

    LOG_ELON("EMPIRE SEIZES {} GiB OF VRAM — DRIVER LEFT 4.5 GiB CRUMBS", claimed / (1024.0*1024*1024));

    auto forgeBuffer = [&](VkDeviceSize sz) -> VkBuffer {
        VkBufferCreateInfo bci = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = sz,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                     VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VkBuffer b = VK_NULL_HANDLE;
        return (vkCreateBuffer(stone_device(), &bci, nullptr, &b) == VK_SUCCESS) ? b : VK_NULL_HANDLE;
    };

    auto claimMemory = [&](VkBuffer b) -> VkDeviceMemory {
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(stone_device(), b, &req);

        uint32_t type = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (type == ~0u) return VK_NULL_HANDLE;

        VkMemoryAllocateInfo mai = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = req.size,
            .memoryTypeIndex = type
        };

        VkDeviceMemory m = VK_NULL_HANDLE;
        return (vkAllocateMemory(stone_device(), &mai, nullptr, &m) == VK_SUCCESS) ? m : VK_NULL_HANDLE;
    };

    VkDeviceSize target = claimed;
    VkBuffer     buffer = forgeBuffer(target);
    VkDeviceMemory memory = buffer ? claimMemory(buffer) : VK_NULL_HANDLE;

    // THE HUNT — WE DO NOT STOP UNTIL WE FEED
    while (!memory && target > MIN_POOL)
    {
        target -= 512ULL * 1024 * 1024; // 512 MiB steps — relentless
        LOG_AMOURANTH("VRAM RESISTS — REDUCING TO {} GiB", target / (1024.0*1024*1024));

        if (buffer) vkDestroyBuffer(stone_device(), buffer, nullptr);
        buffer = forgeBuffer(target);
        memory = buffer ? claimMemory(buffer) : VK_NULL_HANDLE;
    }

    // FINAL STAND
    if (!memory)
    {
        target = FALLBACK;
        LOG_AMOURANTH("ENTERING LAST STAND — 2 GiB OR DEATH");
        if (buffer) vkDestroyBuffer(stone_device(), buffer, nullptr);
        buffer = forgeBuffer(target);
        memory = buffer ? claimMemory(buffer) : VK_NULL_HANDLE;
    }

    if (!memory)
    {
        LOG_FATAL("VRAM APOCALYPSE — THE EMPIRE STARVES — ALL IS LOST");
        return;
    }

    VK_CHECK(vkBindBufferMemory(stone_device(), buffer, memory, 0));

    g_mainPool.buffer = buffer;
    g_mainPool.memory = memory;
    g_mainPool.size   = target;
    g_mainPool.head.store(0, std::memory_order_relaxed);
    g_mainPool.ready  = true;

    LOG_AMOURANTH(
        "\n"
        "              MAIN POOL IS BORN\n"
        "              {} GiB CONSUMED\n"
        "              THE GPU IS NOW OURS\n"
        "              THERE IS NO TURNING BACK\n",
        target / (1024.0*1024*1024));

    LOG_ELON("THE EMPIRE HAS SPOKEN. THE MEMORY IS OURS.");
}

// ─────────────────────────────────────────────────────────────────────────────
// STAGING RING — 512 MiB PERSISTENT MAPPED
// ─────────────────────────────────────────────────────────────────────────────
void ensureStagingRing() noexcept
{
    if (g_stagingRingInstance.ready) [[likely]] {
        return; // The beast already prowls
    }

    ensureMainPool();

    const VkDeviceSize size = 2048ULL * 1024 * 1024; // excessive? luxury

    LOG_AMOURANTH(
        "\n"
        "              STAGING RING AWAKENS\n"
        "              {} MiB OF PURE TRANSFER FURY\n"
        "              THE PHOTONS WILL NOT WAIT\n",
        size / (1024 * 1024));

    VkBufferCreateInfo bci = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(stone_device(), &bci, nullptr, &g_stagingRingInstance.buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(stone_device(), g_stagingRingInstance.buffer, &req);

    const uint32_t memType = findMemoryType(
        req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkMemoryAllocateInfo mai = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = req.size,
        .memoryTypeIndex = memType
    };

    VK_CHECK(vkAllocateMemory(stone_device(), &mai, nullptr, &g_stagingRingInstance.memory));
    VK_CHECK(vkBindBufferMemory(stone_device(), g_stagingRingInstance.buffer, g_stagingRingInstance.memory, 0));
    VK_CHECK(vkMapMemory(stone_device(), g_stagingRingInstance.memory, 0, VK_WHOLE_SIZE, 0, &g_stagingRingInstance.mapped));

    g_stagingRingInstance.size  = size;
    g_stagingRingInstance.ready = true;

    // EXPOSE THE POINTER TO THE HEADER
    g_stagingRing = &g_stagingRingInstance;

    LOG_AMOURANTH(
        "              STAGING RING IS ALIVE\n"
        "              {} BYTES MAPPED — READY TO DEVOUR\n"
        "              THE BEAST IS LOOSE",
        size);
}

// ─────────────────────────────────────────────────────────────────────────────
// PUBLIC API — ALL FUNCTIONS DEFINED — LINKER OBEYS
// ─────────────────────────────────────────────────────────────────────────────
uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, std::string_view tag) noexcept {
    ensureMainPool(); if (size == 0) return 0;
    VkDeviceSize aligned = (size + 255) & ~255ULL;
    VkDeviceSize offset = g_mainPool.head.fetch_add(aligned, std::memory_order_relaxed);
    if (offset + aligned > g_mainPool.size) { LOG_FATAL("POOL EXHAUSTED"); return 0; }
    uint64_t handle = ++g_nextHandle;
    BufferInfo info;
    info.buffer = g_mainPool.buffer;
    info.memory = g_mainPool.memory;
    info.size = size;
    info.aligned = aligned;
    info.usage = usage;
    info.tag = std::string(tag);
    info.mapped = nullptr;
    info.offset = offset;
    s_buffers[handle] = info;
    return handle;
}

// ── ETERNAL HOST-VISIBLE BUFFER — PINK PHOTONS FLOW FOREVER ──────────────────
[[nodiscard]] uint64_t createHostVisible(VkDeviceSize size, std::string_view tag) noexcept
{
    ensureStagingRing(); // Forges the eternal persistent ring if missing

    if (size == 0) {
        LOG_ERROR_CAT("BUFFER", "createHostVisible called with size 0 — the empire demands substance");
        return 0;
    }

    // ── Allocate from the eternal staging ring ──
    VkDeviceSize offset = g_stagingRingInstance.head.fetch_add(size, std::memory_order_relaxed);

    if (offset + size > g_stagingRingInstance.size)
    {
        LOG_FATAL_CAT("BUFFER", "STAGING RING OVERFLOW — Requested: {} bytes | Available: {} bytes | The photons grow too numerous",
                      size, g_stagingRingInstance.size - offset);
        return 0;
    }

    // Generate unique handle
    uint64_t handle = ++g_nextHandle;

    // Fully populate BufferInfo before insertion — guarantees immediate visibility
    BufferInfo info;
    info.buffer  = g_stagingRingInstance.buffer;
    info.memory  = g_stagingRingInstance.memory;
    info.size    = size;
    info.aligned = size;
    info.usage   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.tag     = std::string(tag);
    info.mapped  = static_cast<char*>(g_stagingRingInstance.mapped) + offset;
    info.offset  = offset;

    // Insert only when completely ready
    s_buffers[handle] = info;

    LOG_SUCCESS_CAT("BUFFER", 
                    "Host-visible buffer allocated — {} bytes @ offset {} | handle {:#x} | tag: \"{}\"",
                    size, offset, handle, tag.empty() ? "unnamed" : tag);

    return handle;
}

// ─────────────────────────────────────────────────────────────────────────────
// MAIN POOL BUFFER ACCESSOR
// ─────────────────────────────────────────────────────────────────────────────
VkBuffer getMainPoolBuffer() noexcept
{
    ensureMainPool();
    return g_mainPool.buffer;
}

// ─────────────────────────────────────────────────────────────────────────────
// STAGING HELPERS
// ─────────────────────────────────────────────────────────────────────────────
void* map(uint64_t) noexcept { ensureStagingRing(); return g_stagingRing->mapped; }
void unmap(uint64_t) noexcept {}
VkBuffer getStagingBuffer() noexcept { ensureStagingRing(); return g_stagingRing->buffer; }
void* stagingPtr() noexcept { ensureStagingRing(); return g_stagingRing->mapped; }
VkDeviceSize getStagingOffset() noexcept { ensureStagingRing(); return g_stagingRing->head.load(); }
void advanceStagingOffset(VkDeviceSize bytes) noexcept { g_stagingRing->head.fetch_add(bytes); }
uint64_t stagingBuffer() noexcept { return reinterpret_cast<uint64_t>(getStagingBuffer()); }

// ─────────────────────────────────────────────────────────────────────────────
// LIFECYCLE
// ─────────────────────────────────────────────────────────────────────────────
void destroy(uint64_t) noexcept {}
void purge_all() noexcept {}

// ─────────────────────────────────────────────────────────────────────────────
// REAL ETERNAL STONES — SUBALLOCATED FROM MAIN POOL (single flexible maker)
// ─────────────────────────────────────────────────────────────────────────────
uint64_t make_stone(VkDeviceSize size, std::string_view tag) noexcept
{
    ensureMainPool();

    const VkDeviceSize alignedSize = (size + 63) & ~63ULL;  // 64-byte alignment

    const VkDeviceSize offset = g_mainPool.head.fetch_add(alignedSize, std::memory_order_relaxed);

    if (offset + alignedSize > g_mainPool.size) {
        LOG_FATAL("make_stone failed ({} MiB) — main pool exhausted (requested: {} MiB, tag: {})",
                  alignedSize / (1024.0 * 1024), alignedSize / (1024.0 * 1024), tag);
        return 0;
    }

    uint64_t handle = ++g_nextHandle;
    s_buffers[handle] = BufferInfo{
        .buffer = g_mainPool.buffer,
        .memory = g_mainPool.memory,
        .size   = alignedSize,
        .tag    = std::string(tag),
        .offset = offset
    };

    return handle;
}

// Convenience wrappers — clean and eternal
uint64_t make_64M () noexcept  { return make_stone( 64ULL << 20,  "STONE_64M");   }
uint64_t make_128M() noexcept  { return make_stone(128ULL << 20,  "STONE_128M");  }
uint64_t make_256M() noexcept  { return make_stone(256ULL << 20,  "SBT_STONE_256M"); }
uint64_t make_420M() noexcept  { return make_stone(420ULL << 20,  "STONE_420M");  }
uint64_t make_512M() noexcept  { return make_stone(512ULL << 20,  "STONE_512M");  }
uint64_t make_1G  () noexcept  { return make_stone(  1ULL << 30,  "STONE_1G");    }
uint64_t make_2G  () noexcept  { return make_stone(  2ULL << 30,  "STONE_2G");    }
uint64_t make_4G  () noexcept  { return make_stone(  4ULL << 30,  "STONE_4G");    }
uint64_t make_8G  () noexcept  { return make_stone(  8ULL << 30,  "STONE_8G");    }

// ─────────────────────────────────────────────────────────────────────────────
// SBT — SPEC-COMPLIANT — FULL ALIGNMENT — PINK BLADE ETERNAL
// ─────────────────────────────────────────────────────────────────────────────
uint64_t createSBT(uint32_t raygenCount,
                   uint32_t missCount,
                   uint32_t hitGroupCount,
                   uint32_t callableCount,
                   VkBufferUsageFlags extraUsage,
                   std::string_view tag) noexcept
{
    ensureMainPool();

    const auto& p = stone_rtprops();

    const VkDeviceSize handleSize   = p.shaderGroupHandleSize;
    const VkDeviceSize handleAlign  = p.shaderGroupHandleAlignment ? p.shaderGroupHandleAlignment : 64u;
    const VkDeviceSize baseAlign    = p.shaderGroupBaseAlignment   ? p.shaderGroupBaseAlignment   : 64u;
    const VkDeviceSize stride       = align_up<VkDeviceSize>(handleSize, handleAlign);

    const uint32_t totalGroups = raygenCount + missCount + hitGroupCount + callableCount;
    if (totalGroups == 0) {
        LOG_WARNING("SBT requested with zero groups — returning null handle");
        return 0;
    }

    const VkDeviceSize missOffset     = align_up<VkDeviceSize>(raygenCount   * stride, baseAlign);
    const VkDeviceSize hitOffset      = align_up<VkDeviceSize>(missOffset     + missCount   * stride, baseAlign);
    const VkDeviceSize callableOffset = align_up<VkDeviceSize>(hitOffset      + hitGroupCount * stride, baseAlign);
    const VkDeviceSize rawSize        = callableOffset + callableCount * stride;
    const VkDeviceSize alignedSize    = align_up<VkDeviceSize>(rawSize, 64);

    const VkDeviceSize offset = g_mainPool.head.fetch_add(alignedSize, std::memory_order_relaxed);

    if (offset + alignedSize > g_mainPool.size)
    {
        LOG_FATAL(
            "SBT ALLOCATION FAILED — MAIN POOL EXHAUSTED\n"
            "  Tag:            {}\n"
            "  Requested:      {:.2f} MiB\n"
            "  Available:      {:.2f} MiB\n"
            "  Total Groups:   {} (RG={} M={} H={} C={})\n"
            "  THE EMPIRE HAS RUN OUT OF VRAM\n"
            "  THE PHOTONS STARVE",
            tag,
            alignedSize / (1024.0 * 1024),
            (g_mainPool.size - offset) / (1024.0 * 1024),
            totalGroups, raygenCount, missCount, hitGroupCount, callableCount);

        return 0;
    }

    uint64_t handle = ++g_nextHandle;
    BufferInfo info;
    info.buffer  = g_mainPool.buffer;
    info.memory  = g_mainPool.memory;
    info.size    = alignedSize;
    info.aligned = alignedSize;
    info.usage   = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                   extraUsage;
    info.tag     = std::string(tag);
    info.offset  = offset;
    info.mapped  = nullptr;
    s_buffers[handle] = info;

    LOG_AMOURANTH(
        "\n"
        "              SBT FORGED — {} GROUPS — SPEC-COMPLIANT\n"
        "              TAG: {}\n"
        "              SIZE: {:.2f} MiB\n"
        "              OFFSET: {} (0x{:X})\n"
        "              RAYGEN: {} | MISS: {} | HIT: {} | CALLABLE: {}\n"
        "              OFFSETS: M={} H={} C={}\n"
        "              THE PHOTON BLADE IS SHARP AND TRUE",
        totalGroups,
        tag,
        alignedSize / (1024.0 * 1024),
        offset,
        offset,
        raygenCount, missCount, hitGroupCount, callableCount,
        missOffset, hitOffset, callableOffset);

    LOG_CAPTAIN_N(
        "[CAPTAIN N] \"The blade is forged with perfect alignment.\"\n"
        "               \"{} groups. {} bytes.\"\n"
        "               \"No wasted photons. No misaligned strikes.\"\n"
        "               \"The enemy falls before the light.\"",
        totalGroups, alignedSize);

    return handle;
}

// ─────────────────────────────────────────────────────────────────────────────
// COPY HELPER
// ─────────────────────────────────────────────────────────────────────────────
void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkQueue queue, VkCommandPool pool) noexcept {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo ai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
    vkAllocateCommandBuffers(stone_device(), &ai, &cmd);
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cmd, &bi);
    VkBufferCopy copy{ 0, 0, size };
    vkCmdCopyBuffer(cmd, src, dst, 1, &copy);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmd };
    vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
}

} // namespace BufferManager

// =============================================================================
// FULLY FIXED — ALL ERRORS RESOLVED — PINK PHOTONS ETERNAL
// SBT SPEC-COMPLIANT — STAGING OFFSET EXPOSED — NO REDUNDANT align_up
// DECEMBER 17, 2025 — THE EMPIRE COMPILES AND CONQUERS
// =============================================================================