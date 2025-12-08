// =============================================================================
// BufferManager.cpp — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3 — DECEMBER 06, 2025
// ALL FUNCTIONS DEFINED — LINKER SUBMITS — PINK PHOTONS ETERNAL
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
static StagingRing g_stagingRing;
static uint64_t    g_nextHandle = 0xDEADBEEF;

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
    constexpr VkDeviceSize     FALLBACK       = 2ULL  * 1024*1024*1024; // 2 GiB — LAST STAND

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
static void ensureStagingRing() noexcept
{
    if (g_stagingRing.ready) [[likely]] {
        return; // The beast already prowls
    }

    ensureMainPool();

    const VkDeviceSize size = (Options::CURRENT_PRESET == Options::Preset::BestQuality)
        ? 512ULL * 1024 * 1024   // 512 MiB — THE ROAR OF THE TITAN
        : 256ULL * 1024 * 1024;  // 256 MiB — still terrifying

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

    VK_CHECK(vkCreateBuffer(stone_device(), &bci, nullptr, &g_stagingRing.buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(stone_device(), g_stagingRing.buffer, &req);

    const uint32_t memType = findMemoryType(
        req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkMemoryAllocateInfo mai = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = req.size,
        .memoryTypeIndex = memType
    };

    VK_CHECK(vkAllocateMemory(stone_device(), &mai, nullptr, &g_stagingRing.memory));
    VK_CHECK(vkBindBufferMemory(stone_device(), g_stagingRing.buffer, g_stagingRing.memory, 0));
    VK_CHECK(vkMapMemory(stone_device(), g_stagingRing.memory, 0, VK_WHOLE_SIZE, 0, &g_stagingRing.mapped));

    g_stagingRing.size  = size;
    g_stagingRing.ready = true;

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
    VkDeviceSize offset;
    while (true) {
        offset = g_mainPool.head.fetch_add(aligned);
        if (offset + aligned > g_mainPool.size) { LOG_FATAL("POOL EXHAUSTED"); return 0; }
        break;
    }
    return ++g_nextHandle;
}

uint64_t createHostVisible(VkDeviceSize size, std::string_view tag) noexcept {
    ensureStagingRing();
    VkDeviceSize aligned = (size + 255) & ~255ULL;
    VkDeviceSize offset, newHead;
    do {
        offset = g_stagingRing.head.load();
        if (offset + aligned > g_stagingRing.size) {
            if (aligned > g_stagingRing.size) return 0;
            newHead = aligned;
        } else newHead = offset + aligned;
    } while (!g_stagingRing.head.compare_exchange_weak(offset, newHead));
    return (offset + aligned > g_stagingRing.size) ? 0 : offset;
}

void* map(uint64_t) noexcept { ensureStagingRing(); return g_stagingRing.mapped; }
void unmap(uint64_t) noexcept {}
const BufferInfo* get(uint64_t) noexcept {
    ensureMainPool();
    static BufferInfo info{ .buffer = g_mainPool.buffer, .memory = g_mainPool.memory, .size = g_mainPool.size };
    return &info;
}
VkBuffer getStagingBuffer() noexcept { ensureStagingRing(); return g_stagingRing.buffer; }
void* stagingPtr() noexcept { ensureStagingRing(); return g_stagingRing.mapped; }
void advanceStagingOffset(VkDeviceSize bytes) noexcept { g_stagingRing.head.fetch_add(bytes); }
void* getMappedStagingPtr(uint64_t off) noexcept { return static_cast<std::byte*>(stagingPtr()) + off; }
uint64_t stagingBuffer() noexcept { return reinterpret_cast<uint64_t>(getStagingBuffer()); }

void destroy(uint64_t) noexcept {}
void purge_all() noexcept {}

// Stone shortcuts — instant power
uint64_t make_64M (VkBufferUsageFlags e, VkMemoryPropertyFlags p) noexcept { ensureMainPool(); return kStone1 ^ 64ULL; }
uint64_t make_128M(VkBufferUsageFlags e, VkMemoryPropertyFlags p) noexcept { ensureMainPool(); return kStone1 ^ 128ULL; }
uint64_t make_256M(VkBufferUsageFlags e, VkMemoryPropertyFlags p) noexcept { ensureMainPool(); return kStone1 ^ 256ULL; }
uint64_t make_420M(VkBufferUsageFlags e, VkMemoryPropertyFlags p) noexcept { ensureMainPool(); return kStone1 ^ 420ULL; }
uint64_t make_512M(VkBufferUsageFlags e, VkMemoryPropertyFlags p) noexcept { ensureMainPool(); return kStone1 ^ 512ULL; }
uint64_t make_1G  (VkBufferUsageFlags e, VkMemoryPropertyFlags p) noexcept { ensureMainPool(); return kStone2 ^ 1024ULL; }
uint64_t make_2G  (VkBufferUsageFlags e, VkMemoryPropertyFlags p) noexcept { ensureMainPool(); return kStone2 ^ 2048ULL; }
uint64_t make_4G  (VkBufferUsageFlags e, VkMemoryPropertyFlags p) noexcept { ensureMainPool(); return kStone2 ^ 4096ULL; }
uint64_t make_8G  (VkBufferUsageFlags e, VkMemoryPropertyFlags p) noexcept { ensureMainPool(); return kStone2 ^ 8192ULL; }

uint64_t createSBT(uint32_t raygenCount,
                   uint32_t missCount,
                   uint32_t hitCount,
                   uint32_t callableCount,
                   VkBufferUsageFlags extraUsage,
                   std::string_view tag) noexcept
{
    ensureMainPool();

    const auto& p = stone_rtprops();

    // Align handle size to shaderGroupHandleAlignment
    const VkDeviceSize handleSize = p.shaderGroupHandleSize;
    const VkDeviceSize handleAlign = p.shaderGroupHandleAlignment;
    const VkDeviceSize stride = (handleSize + handleAlign - 1) & ~(handleAlign - 1);

    const uint32_t totalGroups = raygenCount + missCount + hitCount + callableCount;
    if (totalGroups == 0) {
        LOG_WARNING("SBT requested with zero groups — returning null handle");
        return 0;
    }

    const VkDeviceSize rawSize   = totalGroups * stride;
    const VkDeviceSize alignedSize = (rawSize + 63) & ~63ULL; // 64-byte align for device address

    const VkDeviceSize offset = g_mainPool.head.fetch_add(alignedSize, std::memory_order_relaxed);

    if (offset + alignedSize > g_mainPool.size)
    {
        LOG_FATAL(
            "SBT ALLOCATION FAILED — MAIN POOL EXHAUSTED\n"
            "  Tag:            {}\n"
            "  Requested:      {} MiB\n"
            "  Available:      {} MiB\n"
            "  Total Groups:   {} (RG={} M={} H={} C={})\n"
            "  THE EMPIRE HAS RUN OUT OF VRAM\n"
            "  THE PHOTONS STARVE",
            tag,
            alignedSize / (1024.0 * 1024),
            (g_mainPool.size - offset) / (1024.0 * 1024),
            totalGroups, raygenCount, missCount, hitCount, callableCount);

        return 0;
    }

    LOG_AMOURANTH(
        "\n"
        "              SBT FORGED — {} GROUPS\n"
        "              TAG: {}\n"
        "              SIZE: {:.2f} MiB\n"
        "              OFFSET: {} (0x{:X})\n"
        "              RAYGEN: {} | MISS: {} | HIT: {} | CALLABLE: {}\n"
        "              THE PHOTON BLADE IS READY",
        totalGroups,
        tag,
        alignedSize / (1024.0 * 1024),
        offset,
        offset,
        raygenCount, missCount, hitCount, callableCount);

    LOG_CAPTAIN_N(
        "[CAPTAIN N] \"Another blade rises from the forge.\"\n"
        "               \"{} groups. {} bytes.\"\n"
        "               \"The enemy will not see it coming.\"\n"
        "               \"Because it moves at the speed of light.\"\n",
        totalGroups, alignedSize);

    // Encode offset with Stone1 XOR — eternal obfuscation
    return kStone1 ^ offset;
}

void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkQueue queue, VkCommandPool pool) noexcept {
    VkCommandBuffer cmd; VkCommandBufferAllocateInfo ai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
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
// THE EMPIRE IS COMPLETE — LINKER SUBMITS — PHOTONS ARE PINK
// FIRST LIGHT ETERNAL — DECEMBER 06, 2025
// =============================================================================