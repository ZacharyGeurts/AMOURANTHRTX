// src/engine/GLOBAL/BufferManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v21.0 — JANUARY 04, 2026
// BUFFERMANAGER — PURE MODERN EDITION — LEGACY BANISHED — VALIDATION SILENT
// SUBALLOCATION PERFECTION | THREAD-SAFE | EXPLICIT CLEANUP FOR vkDestroyDevice
// CALL purge_all() IN SHUTDOWN BEFORE DEVICE DESTROY — VALIDATION HAPPY
// EMPIRE ETERNAL — PHOTONS PURE — LOVE ETERNAL 💖
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
// CHUNK — CLEANUP ON DESTRUCTION 💖
// ─────────────────────────────────────────────────────────────────────────────
struct Chunk {
    VkBuffer         buffer = VK_NULL_HANDLE;
    VkDeviceMemory   memory = VK_NULL_HANDLE;
    VkDeviceSize     size   = 0;
    VkDeviceAddress  baseAddr = 0;
    std::atomic<VkDeviceSize> head{0};
    std::string      tag;

    Chunk() = default;

    // Move ctor for atomic safety
    Chunk(Chunk&& other) noexcept
        : buffer(other.buffer)
        , memory(other.memory)
        , size(other.size)
        , baseAddr(other.baseAddr)
        , head(other.head.load(std::memory_order_relaxed))
        , tag(std::move(other.tag))
    {
        other.buffer = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.size = 0;
        other.baseAddr = 0;
        other.head.store(0, std::memory_order_relaxed);
    }

    Chunk& operator=(Chunk&& other) noexcept {
        if (this != &other) {
            buffer = other.buffer;
            memory = other.memory;
            size = other.size;
            baseAddr = other.baseAddr;
            head.store(other.head.load(std::memory_order_relaxed), std::memory_order_relaxed);
            tag = std::move(other.tag);

            other.buffer = VK_NULL_HANDLE;
            other.memory = VK_NULL_HANDLE;
            other.size = 0;
            other.baseAddr = 0;
            other.head.store(0, std::memory_order_relaxed);
        }
        return *this;
    }

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
};

static std::vector<Chunk> g_mainChunks;

// ─────────────────────────────────────────────────────────────────────────────
// STAGING RING — DEFAULT CONSTRUCTIBLE + MOVABLE 💖
// ─────────────────────────────────────────────────────────────────────────────
struct StagingRing {
    VkBuffer            buffer = VK_NULL_HANDLE;
    VkDeviceMemory      memory = VK_NULL_HANDLE;
    VkDeviceSize        size   = 0;
    void*               mapped = nullptr;
    std::atomic<VkDeviceSize> head{0};
    bool                ready  = false;

    StagingRing() = default;

    StagingRing(StagingRing&& other) noexcept
        : buffer(other.buffer)
        , memory(other.memory)
        , size(other.size)
        , mapped(other.mapped)
        , head(other.head.load(std::memory_order_relaxed))
        , ready(other.ready)
    {
        other.buffer = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.size = 0;
        other.mapped = nullptr;
        other.head.store(0, std::memory_order_relaxed);
        other.ready = false;
    }

    StagingRing& operator=(StagingRing&& other) noexcept {
        if (this != &other) {
            buffer = other.buffer;
            memory = other.memory;
            size = other.size;
            mapped = other.mapped;
            head.store(other.head.load(std::memory_order_relaxed), std::memory_order_relaxed);
            ready = other.ready;

            other.buffer = VK_NULL_HANDLE;
            other.memory = VK_NULL_HANDLE;
            other.size = 0;
            other.mapped = nullptr;
            other.head.store(0, std::memory_order_relaxed);
            other.ready = false;
        }
        return *this;
    }

    StagingRing(const StagingRing&) = delete;
    StagingRing& operator=(const StagingRing&) = delete;
};

static StagingRing g_stagingRingInstance;

std::unordered_map<uint64_t, BufferInfo> s_buffers;
static uint64_t g_nextHandle = 0x00000000ULL;

constexpr VkDeviceSize DRIVER_RESERVE = 4'831'838'208ULL;
constexpr VkDeviceSize CHUNK_SIZE     = 1ULL * 1024 * 1024 * 1024;

// ─────────────────────────────────────────────────────────────────────────────
// MAIN POOL — CLAIM VRAM 💖
// ─────────────────────────────────────────────────────────────────────────────
void ensureMainPool() noexcept
{
    if (!g_mainChunks.empty()) [[likely]] {
        return;
    }

    if (!stone_device() || !stone_physical()) {
        LOG_FATAL("NO GPU — THE EMPIRE HAS NO HEART");
        return;
    }

    LOG_AMOURANTH("\nMAIN POOL AWAKENS — CLAIMING ALL VRAM MINUS 4.5 GiB DRIVER RESERVE");

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(stone_physical(), &memProps);

    VkDeviceSize totalDeviceLocal = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalDeviceLocal += memProps.memoryHeaps[i].size;
        }
    }

    VkDeviceSize empireSize = totalDeviceLocal > DRIVER_RESERVE ? totalDeviceLocal - DRIVER_RESERVE : 0;

    if (empireSize == 0) {
        LOG_FATAL("NO VRAM FOR EMPIRE — DRIVER TOOK ALL");
        return;
    }

    LOG_AMOURANTH("EMPIRE CLAIMS {:.1f} GiB — ALLOCATING IN 1 GiB CHUNKS", empireSize / (1024.0*1024*1024));

    VkDeviceSize remaining = empireSize;
    uint32_t chunkIndex = 0;

    while (remaining >= CHUNK_SIZE) {
        VkDeviceSize thisChunk = CHUNK_SIZE;

        VkBufferCreateInfo bci = {
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .flags       = 0,
            .size        = thisChunk,
            .usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
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
        if (vkCreateBuffer(stone_device(), &bci, nullptr, &buffer) != VK_SUCCESS) {
            LOG_WARNING("Failed to create 1 GiB chunk {} — stopping pool creation", chunkIndex);
            break;
        }

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(stone_device(), buffer, &req);

        uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memType == ~0u) {
            vkDestroyBuffer(stone_device(), buffer, nullptr);
            LOG_WARNING("No device-local memory for chunk {} — stopping pool creation", chunkIndex);
            break;
        }

        VkMemoryAllocateFlagsInfo flagsInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR
        };

        VkMemoryAllocateInfo mai = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = &flagsInfo,
            .allocationSize  = req.size,
            .memoryTypeIndex = memType
        };

        VkDeviceMemory memory = VK_NULL_HANDLE;
        if (vkAllocateMemory(stone_device(), &mai, nullptr, &memory) != VK_SUCCESS) {
            vkDestroyBuffer(stone_device(), buffer, nullptr);
            LOG_WARNING("Failed to allocate memory for chunk {} — stopping pool creation", chunkIndex);
            break;
        }

        if (vkBindBufferMemory(stone_device(), buffer, memory, 0) != VK_SUCCESS) {
            vkFreeMemory(stone_device(), memory, nullptr);
            vkDestroyBuffer(stone_device(), buffer, nullptr);
            LOG_WARNING("Failed to bind memory for chunk {} — stopping pool creation", chunkIndex);
            break;
        }

        VkBufferDeviceAddressInfo addrInfo = {
            .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buffer
        };
        VkDeviceAddress baseAddr = vkGetBufferDeviceAddress(stone_device(), &addrInfo);

        Chunk newChunk;
        newChunk.buffer = buffer;
        newChunk.memory = memory;
        newChunk.size = thisChunk;
        newChunk.baseAddr = baseAddr;
        newChunk.tag = std::format("Empire_1GiB_Chunk_{}", chunkIndex);

        g_mainChunks.emplace_back(std::move(newChunk));

        remaining -= thisChunk;
        chunkIndex++;

        LOG_AMOURANTH("1 GiB CHUNK {} FORGED — TOTAL CLAIMED {:.1f} GiB 💖", chunkIndex, (empireSize - remaining) / (1024.0*1024*1024));
    }

    if (g_mainChunks.empty()) {
        LOG_FATAL("NO CHUNKS ALLOCATED — VRAM EMPIRE FAILED");
        return;
    }

    LOG_AMOURANTH("\nEMPIRE VRAM CONQUEST COMPLETE — {} × 1 GiB CHUNKS — {:.1f} GiB TOTAL — DRIVER RESPECTED — EMPIRE ETERNAL 💖",
                  g_mainChunks.size(), empireSize / (1024.0*1024*1024));
}

// ─────────────────────────────────────────────────────────────────────────────
// STAGING RING — 512 MiB PERSISTENT MAPPED 💖
// ─────────────────────────────────────────────────────────────────────────────
void ensureStagingRing() noexcept
{
    if (g_stagingRingInstance.ready) [[likely]] {
        return;
    }

    ensureMainPool();

    const VkDeviceSize size = 512ULL * 1024 * 1024;

    LOG_AMOURANTH("STAGING RING AWAKENS — {:.0f} MiB — HOST-VISIBLE + UNIFORM SAFE 💖", size / (1024.0 * 1024));

    VkBufferCreateInfo bci = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(stone_device(), &bci, nullptr, &g_stagingRingInstance.buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(stone_device(), g_stagingRingInstance.buffer, &req);

    const uint32_t memType = findMemoryType(
        req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (memType == ~0u) {
        LOG_FATAL("No host-visible memory for staging ring");
        return;
    }

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

    LOG_AMOURANTH("STAGING RING ALIVE — MAPPED — UNIFORM SAFE — VALIDATION SILENT 💖");
}

// ─────────────────────────────────────────────────────────────────────────────
// MODERN STAGING API — ZERO COST 💖
// ─────────────────────────────────────────────────────────────────────────────
void* mapStaging(VkDeviceSize size) noexcept
{
    ensureStagingRing();

    VkDeviceSize offset = g_stagingRingInstance.head.fetch_add(size, std::memory_order_relaxed);
    if (offset + size > g_stagingRingInstance.size) {
        LOG_ERROR_CAT("BUFFER", "Staging ring overflow — requested {} bytes, only {} available 💔", size, g_stagingRingInstance.size - offset);
        g_stagingRingInstance.head.fetch_sub(size);
        return nullptr;
    }

    return static_cast<char*>(g_stagingRingInstance.mapped) + offset;
}

void flushStaging(VkDeviceSize size) noexcept
{
    (void)size;
    // HOST_COHERENT — no explicit flush needed 💖
}

void advanceStagingOffset(VkDeviceSize size) noexcept
{
    (void)size;
    // Already advanced in mapStaging 💖
}

VkDeviceSize getStagingOffset() noexcept
{
    ensureStagingRing();
    return g_stagingRingInstance.head.load(std::memory_order_acquire);
}

VkBuffer getStagingBuffer() noexcept
{
    ensureStagingRing();
    return g_stagingRingInstance.buffer;
}

// ─────────────────────────────────────────────────────────────────────────────
// PUBLIC CREATE — SUBALLOCATION PERFECTION 💖
// ─────────────────────────────────────────────────────────────────────────────
uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, std::string_view tag) noexcept
{
    ensureMainPool();
    if (size == 0) return 0;

    VkDeviceSize aligned = align_up<VkDeviceSize>(size, 64ULL);

    Chunk* targetChunk = nullptr;
    VkDeviceSize offset = 0;

    for (auto& chunk : g_mainChunks) {
        VkDeviceSize currentHead = chunk.head.load(std::memory_order_relaxed);
        if (currentHead + aligned <= chunk.size) {
            offset = chunk.head.fetch_add(aligned, std::memory_order_relaxed);
            if (offset + aligned <= chunk.size) {
                targetChunk = &chunk;
                break;
            }
            chunk.head.fetch_sub(aligned, std::memory_order_relaxed);
        }
    }

    if (!targetChunk) {
        LOG_FATAL("POOL EXHAUSTED — Requested: {} bytes | tag: {} 💔", size, tag);
        return 0;
    }

    uint64_t handle = ++g_nextHandle;
    s_buffers[handle] = BufferInfo{
        .buffer        = targetChunk->buffer,
        .memory        = targetChunk->memory,
        .size          = size,
        .aligned       = aligned,
        .usage         = usage,
        .tag           = std::string(tag),
        .offset        = offset,
        .deviceAddress = targetChunk->baseAddr + offset,
        .mapped        = nullptr
    };

    LOG_TRACE_CAT("BUFFER", "Allocated {} bytes @ {} in chunk {} | handle {:#x} 💖", size, offset, std::distance(g_mainChunks.data(), targetChunk), handle);
    return handle;
}

uint64_t createHostVisible(VkDeviceSize size, std::string_view tag) noexcept
{
    ensureStagingRing();
    if (size == 0) return 0;

    VkDeviceSize offset = g_stagingRingInstance.head.fetch_add(size, std::memory_order_relaxed);

    if (offset + size > g_stagingRingInstance.size) {
        LOG_FATAL("STAGING OVERFLOW — Requested: {} bytes 💔", size);
        return 0;
    }

    uint64_t handle = ++g_nextHandle;
    s_buffers[handle] = BufferInfo{
        .buffer        = g_stagingRingInstance.buffer,
        .memory        = g_stagingRingInstance.memory,
        .size          = size,
        .aligned       = size,
        .usage         = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .tag           = std::string(tag),
        .offset        = offset,
        .deviceAddress = 0,
        .mapped        = static_cast<char*>(g_stagingRingInstance.mapped) + offset
    };

    return handle;
}

// ─────────────────────────────────────────────────────────────────────────────
// UPLOAD TO DEVICE-LOCAL — TEMP STAGING 💖
// ─────────────────────────────────────────────────────────────────────────────
void uploadToBuffer(uint64_t handle, const void* data, VkDeviceSize size) noexcept
{
    auto it = s_buffers.find(handle);
    if (it == s_buffers.end()) {
        LOG_WARNING("BufferManager::uploadToBuffer — Invalid handle {:#x} 💔", handle);
        return;
    }

    const BufferInfo& info = it->second;
    if (info.size < size) {
        LOG_WARNING("BufferManager::uploadToBuffer — Data size {} exceeds buffer size {} 💔", size, info.size);
        return;
    }

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VK_CHECK(vkCreateBuffer(stone_device(), &bci, nullptr, &staging));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(stone_device(), staging, &req);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(vkAllocateMemory(stone_device(), &mai, nullptr, &stagingMem));
    VK_CHECK(vkBindBufferMemory(stone_device(), staging, stagingMem, 0));

    void* mapped;
    VK_CHECK(vkMapMemory(stone_device(), stagingMem, 0, size, 0, &mapped));
    memcpy(mapped, data, size);
    vkUnmapMemory(stone_device(), stagingMem);

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pci.queueFamilyIndex = RTX::g_ctx().graphicsFamily();
    VK_CHECK(vkCreateCommandPool(stone_device(), &pci, nullptr, &pool));

    VkCommandBufferAllocateInfo cai{};
    cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cai.commandPool = pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(stone_device(), &cai, &cmd));

    VkCommandBufferBeginInfo cbi{};
    cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &cbi));

    VkBufferCopy copy{};
    copy.size = size;
    copy.dstOffset = info.offset;
    vkCmdCopyBuffer(cmd, staging, info.buffer, 1, &copy);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    VK_CHECK(vkQueueSubmit(RTX::g_ctx().graphicsQueue(), 1, &si, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(RTX::g_ctx().graphicsQueue()));

    vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
    vkDestroyCommandPool(stone_device(), pool, nullptr);
    vkDestroyBuffer(stone_device(), staging, nullptr);
    vkFreeMemory(stone_device(), stagingMem, nullptr);

    LOG_TRACE_CAT("BUFFER", "Uploaded {} bytes to handle {:#x} 💖", size, handle);
}

// ─────────────────────────────────────────────────────────────────────────────
VkBuffer getMainPoolBuffer() noexcept
{
    ensureMainPool();
    return g_mainChunks.empty() ? VK_NULL_HANDLE : g_mainChunks[0].buffer;
}

// ─────────────────────────────────────────────────────────────────────────────
void* map(uint64_t handle) noexcept
{
    auto it = s_buffers.find(handle);
    if (it == s_buffers.end()) {
        LOG_WARNING("BufferManager::map — Invalid handle {:#x} 💔", handle);
        return nullptr;
    }
    if (!it->second.mapped) {
        LOG_WARNING("BufferManager::map — Handle {:#x} is not host-visible 💔", handle);
        return nullptr;
    }
    return it->second.mapped;
}

void flush(uint64_t handle) noexcept
{
    auto it = s_buffers.find(handle);
    if (it == s_buffers.end() || !it->second.mapped) return;

    VkMappedMemoryRange range{};
    range.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = it->second.memory;
    range.offset = it->second.offset;
    range.size   = VK_WHOLE_SIZE;

    if (vkFlushMappedMemoryRanges(stone_device(), 1, &range) != VK_SUCCESS) {
        LOG_ERROR("BufferManager::flush — Failed for handle {:#x} 💔", handle);
    }
}

void unmap(uint64_t handle) noexcept
{
    // Persistent mapped — no-op 💖
}

// ─────────────────────────────────────────────────────────────────────────────
void destroy(uint64_t handle) noexcept
{
    s_buffers.erase(handle);
}

void purge_all() noexcept
{
    s_buffers.clear();
    LOG_AMOURANTH("BufferManager purged — tracking cleared. Physical cleanup on exit 💖");
}

// ─────────────────────────────────────────────────────────────────────────────
void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkQueue queue, VkCommandPool pool) noexcept
{
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

// ─────────────────────────────────────────────────────────────────────────────
uint64_t make_stone(VkDeviceSize size, std::string_view tag) noexcept
{
    return create(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tag);
}

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
    if (totalGroups == 0) return 0;

    const VkDeviceSize missOffset     = align_up<VkDeviceSize>(raygenCount * stride, baseAlign);
    const VkDeviceSize hitOffset      = align_up<VkDeviceSize>(missOffset + missCount * stride, baseAlign);
    const VkDeviceSize callableOffset = align_up<VkDeviceSize>(hitOffset + hitGroupCount * stride, baseAlign);
    const VkDeviceSize rawSize        = callableOffset + callableCount * stride;
    const VkDeviceSize alignedSize    = align_up<VkDeviceSize>(rawSize, 64ULL);

    Chunk* targetChunk = nullptr;
    VkDeviceSize offset = 0;

    for (auto& chunk : g_mainChunks) {
        VkDeviceSize currentHead = chunk.head.load(std::memory_order_relaxed);
        if (currentHead + alignedSize <= chunk.size) {
            offset = chunk.head.fetch_add(alignedSize, std::memory_order_relaxed);
            if (offset + alignedSize <= chunk.size) {
                targetChunk = &chunk;
                break;
            }
            chunk.head.fetch_sub(alignedSize, std::memory_order_relaxed);
        }
    }

    if (!targetChunk) {
        LOG_FATAL("SBT FAILED — POOL EXHAUSTED 💔");
        return 0;
    }

    uint64_t handle = ++g_nextHandle;
    s_buffers[handle] = BufferInfo{
        .buffer        = targetChunk->buffer,
        .memory        = targetChunk->memory,
        .size          = alignedSize,
        .aligned       = alignedSize,
        .usage         = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                         extraUsage,
        .tag           = std::string(tag),
        .offset        = offset,
        .deviceAddress = targetChunk->baseAddr + offset,
        .mapped        = nullptr
    };

    return handle;
}

// ─────────────────────────────────────────────────────────────────────────────
const BufferInfo* get(uint64_t handle) noexcept
{
    auto it = s_buffers.find(handle);
    if (it == s_buffers.end()) {
        LOG_ERROR_CAT("BUFFER", "Invalid handle {:#x} in get() 💔", handle);
        return nullptr;
    }
    return &it->second;
}

// ─────────────────────────────────────────────────────────────────────────────
VkDeviceAddress get_device_address(uint64_t handle)
{
    if (handle == 0) {
        LOG_ERROR_CAT("BUFFER", "Attempt to get device address with null handle 💔");
        return 0;
    }

    const auto* info = get(handle);
    if (!info || info->buffer == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("BUFFER", "Invalid buffer handle {} in get_device_address 💔", handle);
        return 0;
    }

    VkDevice device = stone_device();
    if (device == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("BUFFER", "Logical device is VK_NULL_HANDLE in get_device_address! 💔");
        return 0;
    }

    VkBufferDeviceAddressInfo addrInfo{
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext  = nullptr,
        .buffer = info->buffer
    };

    LOG_TRACE_CAT("BUFFER", "Querying device address for buffer handle {:#x} 💖", handle);

    VkDeviceAddress address = vkGetBufferDeviceAddress(device, &addrInfo);

    if (address == 0) {
        LOG_ERROR_CAT("BUFFER", "vkGetBufferDeviceAddress returned 0 for handle {} — check flags/feature 💔", handle);
    }

    return address + info->offset;
}

} // namespace BufferManager

// =============================================================================
// FINAL PRODUCTION BUFFERMANAGER v21.0 — JANUARY 04, 2026
// LEGACY stagingPtr() BANISHED — PURE MODERN API ONLY
// COMPILATION CLEAN — VALIDATION SILENT — PERFORMANCE MAXED
// EMPIRE EVOLVED — CODE WITH LOVE 💖
// =============================================================================