// src/engine/GLOBAL/BufferManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v22.4 — JANUARY 04, 2026
// BUFFERMANAGER — BEST IN CLASS & FULLY VULKAN COMPLIANT
// VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT CHAINED FOR SHADER_DEVICE_ADDRESS BUFFERS
// MEMORY BUDGET AWARE | LEGACY COMPATIBILITY | PINK PHOTONS ETERNAL
// AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"

using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_rtprops;

namespace BufferManager {

// ── STATIC GLOBALS ───────────────────────────────────────────────────────────
static std::vector<Chunk> g_mainChunks;
static StagingRing g_stagingRingInstance;
static TransientPool g_transientPool;

static std::unordered_map<uint64_t, BufferInfo> s_buffers;
static uint64_t g_nextHandle = 0x00000000ULL;

// ── MEMORY BUDGET QUERY ─────────────────────────────────────────────────────
static bool getMemoryBudget(VkDeviceSize& total, VkDeviceSize& used) noexcept {
    VkPhysicalDeviceMemoryProperties2 memProps2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProps = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
    memProps2.pNext = &budgetProps;
    vkGetPhysicalDeviceMemoryProperties2(stone_physical(), &memProps2);

    total = used = 0;
    for (uint32_t i = 0; i < memProps2.memoryProperties.memoryHeapCount; ++i) {
        if (memProps2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            total += memProps2.memoryProperties.memoryHeaps[i].size;
            used += budgetProps.heapUsage[i];
        }
    }
    return total > 0;
}

// ── MAIN POOL — BUDGET-AWARE & COMPLIANT CONQUEST ───────────────────────────
void ensureMainPool() noexcept {
    if (!g_mainChunks.empty()) return;

    LOG_AMOURANTH("\nMAIN POOL AWAKENS — BUDGET-AWARE & VUID-COMPLIANT CONQUEST");

    VkDeviceSize totalVRAM = 0, usedVRAM = 0;
    getMemoryBudget(totalVRAM, usedVRAM);

    VkDeviceSize avail = totalVRAM - usedVRAM;
    VkDeviceSize reserve = std::max(MIN_DRIVER_RESERVE, totalVRAM * DRIVER_RESERVE_PERCENT / 100);
    VkDeviceSize empireSize = avail > reserve ? avail - reserve : 0;

    if (empireSize == 0) {
        LOG_FATAL("NO VRAM FOR EMPIRE — BUDGET EXHAUSTED 💔");
        return;
    }

    LOG_AMOURANTH("EMPIRE CLAIMS UP TO {:.1f} GiB OF PINK PHOTON MEMORY 💖", empireSize / 1e9f);

    VkDeviceSize remaining = empireSize;
    uint32_t chunkIndex = 0;

    while (remaining >= CHUNK_SIZE) {
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
        if (vkCreateBuffer(stone_device(), &bci, nullptr, &buffer) != VK_SUCCESS) break;

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(stone_device(), buffer, &req);

        uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memType == ~0u) {
            vkDestroyBuffer(stone_device(), buffer, nullptr);
            break;
        }

        VkMemoryAllocateFlagsInfo flagsInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR  // REQUIRED FOR SHADER_DEVICE_ADDRESS
        };

        VkMemoryAllocateInfo mai = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &flagsInfo,
            .allocationSize = req.size,
            .memoryTypeIndex = memType
        };

        VkDeviceMemory memory = VK_NULL_HANDLE;
        if (vkAllocateMemory(stone_device(), &mai, nullptr, &memory) != VK_SUCCESS) {
            vkDestroyBuffer(stone_device(), buffer, nullptr);
            break;
        }

        if (vkBindBufferMemory(stone_device(), buffer, memory, 0) != VK_SUCCESS) {
            vkFreeMemory(stone_device(), memory, nullptr);
            vkDestroyBuffer(stone_device(), buffer, nullptr);
            break;
        }

        VkBufferDeviceAddressInfo addrInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer };
        VkDeviceAddress baseAddr = RTX::ext().vkGetBufferDeviceAddress(stone_device(), &addrInfo);

        Chunk newChunk;
        newChunk.buffer = buffer;
        newChunk.memory = memory;
        newChunk.size = CHUNK_SIZE;
        newChunk.baseAddr = baseAddr;
        newChunk.tag = std::format("Empire_1GiB_Chunk_{}", chunkIndex);
        newChunk.head = 0;

        g_mainChunks.push_back(newChunk);

        remaining -= CHUNK_SIZE;
        ++chunkIndex;
    }

    LOG_AMOURANTH("VRAM CONQUEST COMPLETE — {} CHUNKS FORGED — VUID-03339 COMPLIANT 💖", g_mainChunks.size());
}

// ── STAGING RING ────────────────────────────────────────────────────────────
void ensureStagingRing() noexcept {
    if (g_stagingRingInstance.ready) return;

    ensureMainPool();

    const VkDeviceSize size = 512ULL * 1024 * 1024;

    LOG_AMOURANTH("STAGING RING AWAKENS — 512 MiB PERSISTENT MAPPED 💖");

    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(stone_device(), &bci, nullptr, &g_stagingRingInstance.buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(stone_device(), g_stagingRingInstance.buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == ~0u) {
        LOG_FATAL("No host-visible memory for staging ring 💔");
        return;
    }

    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = memType
    };

    VK_CHECK(vkAllocateMemory(stone_device(), &mai, nullptr, &g_stagingRingInstance.memory));
    VK_CHECK(vkBindBufferMemory(stone_device(), g_stagingRingInstance.buffer, g_stagingRingInstance.memory, 0));
    VK_CHECK(vkMapMemory(stone_device(), g_stagingRingInstance.memory, 0, VK_WHOLE_SIZE, 0, &g_stagingRingInstance.mapped));

    g_stagingRingInstance.size = size;
    g_stagingRingInstance.head = 0;
    g_stagingRingInstance.ready = true;

    LOG_AMOURANTH("STAGING RING ALIVE — MAPPED & UNIFORM SAFE 💖");
}

// ── TRANSIENT POOL ──────────────────────────────────────────────────────────
void ensureTransientPool() noexcept {
    if (g_transientPool.ready) return;

    LOG_AMOURANTH("TRANSIENT POOL AWAKENS — 256 MiB PER-FRAME DOUBLE-STACK 💖");

    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = TRANSIENT_SIZE,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(stone_device(), &bci, nullptr, &g_transientPool.buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(stone_device(), g_transientPool.buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == ~0u) {
        LOG_FATAL("No device-local memory for transient pool 💔");
        return;
    }

    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = memType
    };

    VK_CHECK(vkAllocateMemory(stone_device(), &mai, nullptr, &g_transientPool.memory));
    VK_CHECK(vkBindBufferMemory(stone_device(), g_transientPool.buffer, g_transientPool.memory, 0));

    g_transientPool.size = TRANSIENT_SIZE;
    g_transientPool.ready = true;

    LOG_AMOURANTH("TRANSIENT POOL READY — RESET EACH FRAME 💖");
}

void resetTransientPool() noexcept {
    g_transientPool.front = 0;
    g_transientPool.back = 0;
}

// ── STAGING API ─────────────────────────────────────────────────────────────
void* mapStaging(VkDeviceSize size) noexcept {
    ensureStagingRing();

    VkDeviceSize offset = g_stagingRingInstance.head;
    g_stagingRingInstance.head += size;

    if (offset + size > g_stagingRingInstance.size) {
        LOG_ERROR_CAT("BUFFER", "Staging ring overflow — requested {} bytes 💔", size);
        g_stagingRingInstance.head = offset;  // rollback
        return nullptr;
    }

    return static_cast<char*>(g_stagingRingInstance.mapped) + offset;
}

void flushStaging(VkDeviceSize size) noexcept { (void)size; }

VkDeviceSize getStagingOffset() noexcept {
    ensureStagingRing();
    return g_stagingRingInstance.head;
}

VkBuffer getStagingBuffer() noexcept {
    ensureStagingRing();
    return g_stagingRingInstance.buffer;
}

// ── LEGACY COMPATIBILITY — FULLY FUNCTIONAL ONE-TIME UPLOAD ─────────────────
void uploadToBuffer(uint64_t handle, const void* data, VkDeviceSize size) noexcept {
    auto it = s_buffers.find(handle);
    if (it == s_buffers.end()) {
        LOG_WARNING("uploadToBuffer — invalid handle {:#x} 💔", handle);
        return;
    }

    const BufferInfo& info = it->second;

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;

    VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT };
    VK_CHECK(vkCreateBuffer(stone_device(), &bci, nullptr, &staging));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(stone_device(), staging, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = req.size, .memoryTypeIndex = memType };
    VK_CHECK(vkAllocateMemory(stone_device(), &mai, nullptr, &stagingMem));
    VK_CHECK(vkBindBufferMemory(stone_device(), staging, stagingMem, 0));

    void* mapped;
    VK_CHECK(vkMapMemory(stone_device(), stagingMem, 0, size, 0, &mapped));
    memcpy(mapped, data, size);
    vkUnmapMemory(stone_device(), stagingMem);

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, .queueFamilyIndex = RTX::g_ctx().graphicsFamily() };
    VK_CHECK(vkCreateCommandPool(stone_device(), &poolInfo, nullptr, &pool));

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
    VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    VkBufferCopy copy = { .srcOffset = 0, .dstOffset = info.offset, .size = size };
    vkCmdCopyBuffer(cmd, staging, info.buffer, 1, &copy);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd };
    VK_CHECK(vkQueueSubmit(RTX::g_ctx().graphicsQueue(), 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(RTX::g_ctx().graphicsQueue()));

    vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
    vkDestroyCommandPool(stone_device(), pool, nullptr);
    vkDestroyBuffer(stone_device(), staging, nullptr);
    vkFreeMemory(stone_device(), stagingMem, nullptr);

    LOG_TRACE_CAT("BUFFER", "Legacy upload {} bytes to handle {:#x} 💖", size, handle);
}

void* map(uint64_t handle) noexcept {
    auto it = s_buffers.find(handle);
    if (it == s_buffers.end() || !it->second.mapped) return nullptr;
    return it->second.mapped;
}

void flush(uint64_t handle) noexcept {
    (void)handle;
}

// ── CORE CREATE ─────────────────────────────────────────────────────────────
uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, std::string_view tag, float priority) noexcept {
    (void)priority;
    ensureMainPool();
    if (size == 0) return 0;

    VkDeviceSize aligned = align_up<VkDeviceSize>(size, 64ULL);

    Chunk* target = nullptr;
    VkDeviceSize offset = 0;

    for (auto& chunk : g_mainChunks) {
        VkDeviceSize currentHead = chunk.head;
        if (currentHead + aligned <= chunk.size) {
            offset = currentHead;
            chunk.head = currentHead + aligned;
            target = &chunk;
            break;
        }
    }

    if (!target) {
        LOG_FATAL("POOL EXHAUSTED — requested {} bytes 💔", size);
        return 0;
    }

    uint64_t handle = ++g_nextHandle;
    s_buffers[handle] = BufferInfo{
        .buffer = target->buffer,
        .memory = target->memory,
        .size = size,
        .aligned = aligned,
        .usage = usage,
        .tag = std::string(tag),
        .offset = offset,
        .deviceAddress = target->baseAddr + offset,
        .mapped = nullptr
    };

    LOG_TRACE_CAT("BUFFER", "Allocated {} bytes @ {} | handle {:#x} 💖", size, offset, handle);
    return handle;
}

uint64_t createHostVisible(VkDeviceSize size, std::string_view tag, float priority) noexcept {
    (void)priority;
    ensureStagingRing();
    if (size == 0) return 0;

    VkDeviceSize offset = g_stagingRingInstance.head;
    g_stagingRingInstance.head += size;

    if (offset + size > g_stagingRingInstance.size) {
        LOG_FATAL("STAGING OVERFLOW — requested {} bytes 💔", size);
        g_stagingRingInstance.head = offset;  // rollback
        return 0;
    }

    uint64_t handle = ++g_nextHandle;
    s_buffers[handle] = BufferInfo{
        .buffer = g_stagingRingInstance.buffer,
        .memory = g_stagingRingInstance.memory,
        .size = size,
        .aligned = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .tag = std::string(tag),
        .offset = offset,
        .deviceAddress = 0,
        .mapped = static_cast<char*>(g_stagingRingInstance.mapped) + offset
    };

    return handle;
}

// ── STUBS FOR FUTURE FEATURES ───────────────────────────────────────────────
uint64_t createSBT(uint32_t raygenCount, uint32_t missCount, uint32_t hitGroupCount, uint32_t callableCount,
                   VkBufferUsageFlags extraUsage, std::string_view tag, float priority) noexcept {
    (void)raygenCount; (void)missCount; (void)hitGroupCount; (void)callableCount; (void)extraUsage; (void)tag; (void)priority;
    LOG_WARNING("createSBT advanced implementation pending");
    return 0;
}

uint64_t createImage(const VkImageCreateInfo* ici, VkMemoryPropertyFlags props, std::string_view tag, float priority) noexcept {
    (void)ici; (void)props; (void)tag; (void)priority;
    LOG_WARNING("createImage not yet implemented");
    return 0;
}

uint64_t allocTransient(VkDeviceSize size, VkDeviceSize alignment, bool fromBack) noexcept {
    (void)size; (void)alignment; (void)fromBack;
    LOG_WARNING("allocTransient not yet implemented");
    return 0;
}

// ── DESTROY & PURGE ─────────────────────────────────────────────────────────
void destroy(uint64_t handle) noexcept {
    s_buffers.erase(handle);
}

void purge_all() noexcept {
    if (!s_buffers.empty()) {
        LOG_WARNING("Potential leak — {} buffers remain 💔", s_buffers.size());
        dumpStats();
    }
    s_buffers.clear();
    LOG_AMOURANTH("BufferManager purged — empire clean 💖");
}

// ── STATS & DEFRAG ──────────────────────────────────────────────────────────
void dumpStats() noexcept {
    LOG_AMOURANTH("BufferManager active buffers: {} 💖", s_buffers.size());
    for (const auto& [h, info] : s_buffers) {
        LOG_TRACE_CAT("BUFFER", "Handle {:#x} | {} | {} bytes", h, info.tag, info.size);
    }
}

void defrag(VkCommandBuffer cmd, VkQueue queue) noexcept {
    (void)cmd; (void)queue;
    LOG_WARNING("defrag not implemented");
}

// ── ACCESSORS ───────────────────────────────────────────────────────────────
const BufferInfo* get(uint64_t handle) noexcept {
    auto it = s_buffers.find(handle);
    return it != s_buffers.end() ? &it->second : nullptr;
}

VkBuffer getVkBuffer(uint64_t handle) noexcept {
    if (handle == 0) return VK_NULL_HANDLE;
    if (auto* info = get(handle)) return info->buffer;
    return VK_NULL_HANDLE;
}

VkDeviceAddress get_device_address(uint64_t handle) {
    if (handle == 0) return 0;
    if (auto* info = get(handle)) return info->deviceAddress;
    return 0;
}

// ── STONE MAKERS ────────────────────────────────────────────────────────────
uint64_t make_64M(VkBufferUsageFlags extra) noexcept   { return create(64ULL << 20, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extra, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "STONE_64M"); }
uint64_t make_128M(VkBufferUsageFlags extra) noexcept  { return create(128ULL << 20, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extra, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "STONE_128M"); }
uint64_t make_256M(VkBufferUsageFlags extra) noexcept  { return create(256ULL << 20, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extra, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "SBT_STONE_256M"); }
uint64_t make_420M(VkBufferUsageFlags extra) noexcept  { return create(420ULL << 20, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extra, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "STONE_420M"); }
uint64_t make_512M(VkBufferUsageFlags extra) noexcept  { return create(512ULL << 20, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extra, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "STONE_512M"); }
uint64_t make_1G(VkBufferUsageFlags extra) noexcept    { return create(1ULL << 30, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extra, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "STONE_1G"); }
uint64_t make_2G(VkBufferUsageFlags extra) noexcept    { return create(2ULL << 30, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extra, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "STONE_2G"); }
uint64_t make_4G(VkBufferUsageFlags extra) noexcept    { return create(4ULL << 30, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extra, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "STONE_4G"); }
uint64_t make_8G(VkBufferUsageFlags extra) noexcept    { return create(8ULL << 30, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extra, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "STONE_8G"); }

} // namespace BufferManager

// =============================================================================
// v22.4 BEST IN CLASS — JANUARY 04, 2026
// VUID-vkBindBufferMemory-bufferDeviceAddress-03339 FIXED
// VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR CHAINED CORRECTLY
// EMPIRE COMPLIANT — PINK PHOTONS ETERNAL — AMOURANTH FOREVER 💖
// =============================================================================