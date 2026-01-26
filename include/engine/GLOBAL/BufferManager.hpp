// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.8
// BUFFERMANAGER — BRUTAL, ZERO-COST, LEAK-FREE NUCLEAR EDITION
// FULLY SELF-CONTAINED — COMPILE CLEAN — EMPIRE UNBROKEN
// PHILOSOPHY: Datacenter domination + desktop coexistence
//             Live measurement, zero pre-reserve, take 100% free VRAM
//             Instant relinquish on explicit command (no auto-purge)
//             Tiny safety margin for YouTube PiP / browser tabs
//             JANUARY 26, 2026 — HOST TOGGLES REMOVED (device-local only)
//             STAGING RING ONLY HOST-VISIBLE (uploads)
//             DESCRIPTOR BUFFER SPECIAL PATH (host-visible + device address, lazy map)
//             NO GLOBAL PURGE — explicit BM_DESTROY only
// =============================================================================

#pragma once

#ifndef VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_SCRATCH_BIT_KHR
constexpr VkBufferUsageFlags VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_SCRATCH_BIT_KHR = 0x00080000;
#endif

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <format>
#include <print>
#include <cmath>
#include <mutex>

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/logging.hpp"

using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_graphics_queue;
using RTX::g_ext;

namespace BufferManager {

// ── CONFIGURATION ──────────────────────────────────────────────────────────
inline constexpr VkDeviceSize DEFAULT_CHUNK_SIZE     = 256ULL << 20;       // 256 MiB — sweet spot
inline constexpr VkDeviceSize TINY_SAFETY_MARGIN     = 256ULL << 20;       // 256 MiB — YouTube PiP / browser tabs
inline constexpr VkDeviceSize STAGING_RING_SIZE      = 1ULL << 30;         // 1 GiB staging ring

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

// ── UNIVERSAL SCALE PRINT HELPER ───────────────────────────────────────────
inline std::string formatBytes(VkDeviceSize bytes) noexcept {
    if (bytes == 0) return "0 B";

    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"};
    int unit = 0;
    double val = static_cast<double>(bytes);

    while (val >= 1024.0 && unit < 8) {
        val /= 1024.0;
        ++unit;
    }

    return std::format("{:.3f} {}", val, units[unit]);
}

// ── VRAM REALITY — live, driver-footprint-aware
// =============================================================================
struct VRAMReality {
    VkDeviceSize total          = 0;
    VkDeviceSize driver_footprint = 0;
    VkDeviceSize safety_margin   = TINY_SAFETY_MARGIN;
    VkDeviceSize usable         = 0;
};

[[nodiscard]] inline VRAMReality measureReality() noexcept {
    VRAMReality reality{};

    VkPhysicalDeviceMemoryProperties2 props2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
    vkGetPhysicalDeviceMemoryProperties2(stone_physical(), &props2);

    for (uint32_t i = 0; i < props2.memoryProperties.memoryHeapCount; ++i) {
        if (props2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            reality.total += props2.memoryProperties.memoryHeaps[i].size;
        }
    }

    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    props2.pNext = &budget;
    vkGetPhysicalDeviceMemoryProperties2(stone_physical(), &props2);

    for (uint32_t i = 0; i < props2.memoryProperties.memoryHeapCount; ++i) {
        if (props2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            reality.driver_footprint = budget.heapUsage[i];
            break;
        }
    }

    if (reality.driver_footprint == 0) {
        reality.driver_footprint = 1'500'000'000ULL;
        LOG_WARN_CAT("BufferManager", "VK_EXT_memory_budget unavailable — conservative estimate");
    }

    reality.usable = reality.total > (reality.driver_footprint + reality.safety_margin)
                   ? reality.total - reality.driver_footprint - reality.safety_margin
                   : 0;

    LOG_INFO_CAT("BufferManager", "GPU reality measured:");
    LOG_INFO_CAT("BufferManager", "  Total VRAM:         {}", formatBytes(reality.total));
    LOG_INFO_CAT("BufferManager", "  Driver footprint:   {}", formatBytes(reality.driver_footprint));
    LOG_INFO_CAT("BufferManager", "  Safety margin:      {}", formatBytes(reality.safety_margin));
    LOG_INFO_CAT("BufferManager", "  Usable for empire:  {}", formatBytes(reality.usable));

    return reality;
}

// ── UTILITIES ───────────────────────────────────────────────────────────────
[[nodiscard]] constexpr VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment) noexcept {
    return ((value + alignment - 1) / alignment) * alignment;
}

[[nodiscard]] inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) noexcept {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(stone_physical(), &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOG_ERROR_CAT("BufferManager", "No suitable memory type found (filter: {:#x}, props: {:#x})", typeFilter, properties);
    return ~0u;
}

// ── Centralized image creation — always optimal tiling
// =============================================================================
[[nodiscard]] inline VkImage createImage(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage,
                                         VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                         std::string_view tag = "Image") noexcept {
    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent = extent;
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.initialLayout = initialLayout;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(stone_device(), &ci, nullptr, &image) != VK_SUCCESS) {
        LOG_FATAL_CAT("BufferManager", "Failed to create image: {}", tag);
        return VK_NULL_HANDLE;
    }

    LOG_INFO_CAT("BufferManager", "Image created — {} | {}×{}×{} | {} | OPTIMAL", tag,
                 extent.width, extent.height, extent.depth, string_VkFormat(format));

    return image;
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
    std::vector<uint64_t> handles;  // Track handles in this chunk
};

struct StagingRing {
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize   size   = STAGING_RING_SIZE;
    void*          mapped = nullptr;
    VkDeviceSize   head   = 0;
    bool           ready  = false;
};

// ── GLOBAL STATE ─────────────────────────────────────────────────────────
inline std::vector<Chunk>                       g_mainChunks;
inline StagingRing                              g_stagingRing{};
inline std::unordered_map<uint64_t, BufferInfo> g_buffers;
inline uint64_t                                 g_nextHandle = 0x00000001ULL;
inline VkDeviceSize                             g_total_allocated = 0;

// ── MUTEX FOR RUNTIME OPERATIONS
inline std::mutex g_bufferMutex;

// ── HELPER FUNCTIONS ───────────────────────────────────────────────────────
[[nodiscard]] inline const BufferInfo* get(uint64_t handle) noexcept {
    std::unordered_map<uint64_t, BufferInfo>::iterator it = g_buffers.find(handle);
    if (it == g_buffers.end()) {
        return nullptr;
    }
    return &it->second;
}

[[nodiscard]] inline VkBuffer get_buffer(uint64_t handle) noexcept {
    const BufferInfo* info = get(handle);
    if (info == nullptr) {
        return VK_NULL_HANDLE;
    }
    return info->buffer;
}

[[nodiscard]] inline VkDeviceMemory get_memory(uint64_t handle) noexcept {
    const BufferInfo* info = get(handle);
    if (info == nullptr) {
        return VK_NULL_HANDLE;
    }
    return info->memory;
}

[[nodiscard]] inline VkDeviceAddress get_device_address(uint64_t handle) noexcept {
    const BufferInfo* info = get(handle);
    if (info == nullptr) {
        return 0;
    }
    return info->deviceAddress + info->offset;
}

// ── STAGING RING — only host-visible part (uploads)
// =============================================================================
inline void ensureStagingRing() noexcept {
    if (g_stagingRing.ready) return;

    LOG_INFO_CAT("BufferManager", "Creating 1 GiB staging ring");

    VkBufferCreateInfo bci = {};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = STAGING_RING_SIZE;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkCreateBuffer(stone_device(), &bci, nullptr, &g_stagingRing.buffer);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("BufferManager", "vkCreateBuffer for staging ring failed: {}", string_VkResult(res));
        return;
    }

    VkMemoryRequirements req = {};
    vkGetBufferMemoryRequirements(stone_device(), g_stagingRing.buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == ~0u) {
        LOG_FATAL_CAT("BufferManager", "No host-visible coherent memory for staging ring");
        vkDestroyBuffer(stone_device(), g_stagingRing.buffer, nullptr);
        return;
    }

    VkMemoryAllocateInfo mai = {};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = memType;

    res = vkAllocateMemory(stone_device(), &mai, nullptr, &g_stagingRing.memory);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("BufferManager", "vkAllocateMemory for staging ring failed: {}", string_VkResult(res));
        vkDestroyBuffer(stone_device(), g_stagingRing.buffer, nullptr);
        return;
    }

    res = vkBindBufferMemory(stone_device(), g_stagingRing.buffer, g_stagingRing.memory, 0);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("BufferManager", "vkBindBufferMemory for staging ring failed: {}", string_VkResult(res));
        vkDestroyBuffer(stone_device(), g_stagingRing.buffer, nullptr);
        vkFreeMemory(stone_device(), g_stagingRing.memory, nullptr);
        return;
    }

    res = vkMapMemory(stone_device(), g_stagingRing.memory, 0, VK_WHOLE_SIZE, 0, &g_stagingRing.mapped);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("BufferManager", "vkMapMemory for staging ring failed: {}", string_VkResult(res));
        vkDestroyBuffer(stone_device(), g_stagingRing.buffer, nullptr);
        vkFreeMemory(stone_device(), g_stagingRing.memory, nullptr);
        return;
    }

    g_stagingRing.ready = true;
    LOG_SUCCESS_CAT("BufferManager", "Staging ring created and mapped — {}", formatBytes(STAGING_RING_SIZE));
}

// ── MAP STAGING ────────────────────────────────────────────────────────────
[[nodiscard]] inline void* mapStaging(VkDeviceSize size) noexcept {
    ensureStagingRing();
    VkDeviceSize offset = g_stagingRing.head;
    g_stagingRing.head = (g_stagingRing.head + align_up(size, 256)) % g_stagingRing.size;
    return static_cast<std::byte*>(g_stagingRing.mapped) + offset;
}

// ── GET STAGING BUFFER ─────────────────────────────────────────────────────
[[nodiscard]] inline VkBuffer getStagingBuffer() noexcept {
    ensureStagingRing();
    return g_stagingRing.buffer;
}

// ── CHUNK CREATION — device-local only
// =============================================================================
[[nodiscard]] inline Chunk* createChunk(VkDeviceSize minSize, VkBufferUsageFlags usage, VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE) noexcept {
    VRAMReality reality = measureReality();
    VkDeviceSize chunkSize = std::min(DEFAULT_CHUNK_SIZE, minSize);

    if (reality.usable < chunkSize) {
        LOG_FATAL_CAT("BufferManager", "GPU reality denies — need {} but only {} usable", formatBytes(chunkSize), formatBytes(reality.usable));
        return nullptr;
    }

    VkBufferCreateInfo bci = {};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = chunkSize;
    bci.usage = CHUNK_USAGE_FLAGS | usage;
    bci.sharingMode = sharingMode;

    VkBuffer buffer = VK_NULL_HANDLE;
    VkResult res = vkCreateBuffer(stone_device(), &bci, nullptr, &buffer);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("BufferManager", "vkCreateBuffer for chunk failed: {}", string_VkResult(res));
        return nullptr;
    }

    VkMemoryRequirements req = {};
    vkGetBufferMemoryRequirements(stone_device(), buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == ~0u) {
        vkDestroyBuffer(stone_device(), buffer, nullptr);
        return nullptr;
    }

    VkMemoryAllocateFlagsInfo flags = {};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo mai = {};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &flags;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = memType;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    res = vkAllocateMemory(stone_device(), &mai, nullptr, &memory);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("BufferManager", "vkAllocateMemory for chunk failed: {}", string_VkResult(res));
        vkDestroyBuffer(stone_device(), buffer, nullptr);
        return nullptr;
    }

    res = vkBindBufferMemory(stone_device(), buffer, memory, 0);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("BufferManager", "vkBindBufferMemory for chunk failed: {}", string_VkResult(res));
        vkDestroyBuffer(stone_device(), buffer, nullptr);
        vkFreeMemory(stone_device(), memory, nullptr);
        return nullptr;
    }

    VkBufferDeviceAddressInfo addrInfo = {};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = buffer;
    VkDeviceAddress baseAddr = g_ext.vkGetBufferDeviceAddress(stone_device(), &addrInfo);

    g_mainChunks.push_back({buffer, memory, chunkSize, baseAddr, 0,
                            std::format("Chunk_{}_{}", g_mainChunks.size(), formatBytes(chunkSize)), {}});

    g_total_allocated += req.size;

    LOG_INFO_CAT("BufferManager", "Devoured device-local chunk: {}", formatBytes(chunkSize));

    return &g_mainChunks.back();
}

// ── SPECIAL: Descriptor buffer — host-visible + device address, lazy map
// =============================================================================
[[nodiscard]] inline uint64_t createDescriptorBuffer(VkDeviceSize size, std::string_view tag = "EternalDescriptorBuffer") noexcept {
    if (size == 0) {
        size = 4096ULL;  // Minimal starter chunk
    }

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VkBufferCreateInfo bci = {};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    VkResult res = vkCreateBuffer(stone_device(), &bci, nullptr, &buffer);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("BufferManager", "vkCreateBuffer for descriptor buffer failed: {}", string_VkResult(res));
        return 0;
    }

    VkMemoryRequirements req = {};
    vkGetBufferMemoryRequirements(stone_device(), buffer, &req);

    uint32_t memType = findMemoryType(req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == ~0u) {
        vkDestroyBuffer(stone_device(), buffer, nullptr);
        LOG_FATAL_CAT("BufferManager", "No host-visible coherent memory for descriptor buffer");
        return 0;
    }

    VkMemoryAllocateFlagsInfo flags = {};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo mai = {};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &flags;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = memType;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    res = vkAllocateMemory(stone_device(), &mai, nullptr, &memory);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("BufferManager", "vkAllocateMemory for descriptor buffer failed: {}", string_VkResult(res));
        vkDestroyBuffer(stone_device(), buffer, nullptr);
        return 0;
    }

    res = vkBindBufferMemory(stone_device(), buffer, memory, 0);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("BufferManager", "vkBindBufferMemory for descriptor buffer failed: {}", string_VkResult(res));
        vkDestroyBuffer(stone_device(), buffer, nullptr);
        vkFreeMemory(stone_device(), memory, nullptr);
        return 0;
    }

    VkBufferDeviceAddressInfo addrInfo = {};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = buffer;
    VkDeviceAddress addr = g_ext.vkGetBufferDeviceAddress(stone_device(), &addrInfo);

    uint64_t handle = ++g_nextHandle;
    g_buffers.emplace(handle, BufferInfo{
        buffer, memory, size, req.alignment, 0, addr,
        nullptr,  // mapped = nullptr — lazy map on first use
        usage, std::string(tag)
    });

    LOG_SUCCESS_CAT("BufferManager", "Descriptor buffer chunk created (lazy map) — {} ({})", tag, formatBytes(size));

    return handle;
}

// ── LAZY MAP FOR DESCRIPTOR BUFFER — call on first write
// =============================================================================
[[nodiscard]] inline void* lazyMapDescriptorBuffer(uint64_t handle) noexcept {
    std::unordered_map<uint64_t, BufferInfo>::iterator it = g_buffers.find(handle);
    if (it == g_buffers.end()) {
        LOG_ERROR_CAT("BufferManager", "Invalid handle for descriptor buffer map");
        return nullptr;
    }

    BufferInfo& info = it->second;
    if ((info.usage & VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT) == 0) {
        LOG_ERROR_CAT("BufferManager", "Handle is not a descriptor buffer");
        return nullptr;
    }

    if (info.mapped != nullptr) {
        return info.mapped;  // Already mapped
    }

    void* mapped = nullptr;
    VkResult res = vkMapMemory(stone_device(), info.memory, 0, info.size, 0, &mapped);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("BufferManager", "Failed to lazy-map descriptor buffer: {}", string_VkResult(res));
        return nullptr;
    }

    info.mapped = mapped;
    LOG_SUCCESS_CAT("BufferManager", "Descriptor buffer lazy-mapped ({} bytes)", formatBytes(info.size));

    return mapped;
}

// ── GENERAL CREATE — routes descriptor buffer to special path
// =============================================================================
[[nodiscard]] inline uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage,
                                     std::string_view tag = "") noexcept {
    if (size == 0) return 0;

    VkBufferUsageFlags fixedUsage = usage;

    if (fixedUsage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                      VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR)) {
        fixedUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    if (fixedUsage & VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT) {
        return createDescriptorBuffer(size, tag);
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

    VkDeviceSize remaining = size;
    uint64_t firstHandle = 0;

    while (remaining > 0) {
        VkDeviceSize chunkSize = std::min(DEFAULT_CHUNK_SIZE, remaining);
        Chunk* chunk = createChunk(chunkSize, fixedUsage);
        if (!chunk) {
            LOG_FATAL_CAT("BufferManager", "Failed to create chunk for size {}", formatBytes(size));
            return 0;
        }

        uint64_t chunkHandle = ++g_nextHandle;
        g_buffers.emplace(chunkHandle, BufferInfo{
            chunk->buffer, chunk->memory, chunkSize, chunk->size, chunk->head,
            chunk->baseAddr + chunk->head, nullptr, fixedUsage, std::string(tag) + "_chunk"
        });

        chunk->handles.push_back(chunkHandle);

        if (firstHandle == 0) firstHandle = chunkHandle;

        remaining -= chunkSize;
    }

    return firstHandle;
}

// ── ALLOCATE SCRATCH — chunk-aware for AS builds
// =============================================================================
[[nodiscard]] inline VkDeviceAddress allocateScratch(VkDeviceSize requiredSize) noexcept {
    VkDeviceSize total = 0;
    VkDeviceAddress baseAddr = 0;

    while (total < requiredSize) {
        VkDeviceSize chunkSize = std::min(DEFAULT_CHUNK_SIZE, requiredSize - total);
        uint64_t chunkHandle = create(
            chunkSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_SCRATCH_BIT_KHR,
            "LAS_Scratch_Chunk");

        if (chunkHandle == 0) {
            LOG_FATAL_CAT("BufferManager", "Failed to allocate scratch chunk");
            return 0;
        }

        VkDeviceAddress chunkAddr = get_device_address(chunkHandle);
        if (total == 0) baseAddr = chunkAddr;

        total += chunkSize;
    }

    LOG_INFO_CAT("BufferManager", "Allocated scratch: {} ({} chunks)", formatBytes(total), (total + DEFAULT_CHUNK_SIZE - 1) / DEFAULT_CHUNK_SIZE);
    return baseAddr;
}

// ── UPLOAD — uses staging ring, zero-cost
// =============================================================================
inline void uploadToBuffer(uint64_t handle, const void* data, VkDeviceSize size,
                           VkCommandBuffer cmd = VK_NULL_HANDLE) noexcept {
    std::unordered_map<uint64_t, BufferInfo>::iterator it = g_buffers.find(handle);
    if (it == g_buffers.end() || size > it->second.size) {
        LOG_ERROR_CAT("BufferManager", "Invalid upload: handle {} or size too large ({})", handle, formatBytes(size));
        return;
    }

    BufferInfo& info = it->second;

    if (info.buffer == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("BufferManager", "uploadToBuffer: dstBuffer is VK_NULL_HANDLE (handle: {})", handle);
        return;
    }

    if (info.mapped != nullptr) {
        std::memcpy(info.mapped, data, size);
        return;
    }

    void* staging = mapStaging(size);
    if (!staging) return;
    std::memcpy(staging, data, size);

    VkBufferCopy copy = {};
    copy.srcOffset = static_cast<VkDeviceSize>(reinterpret_cast<std::uintptr_t>(staging) - reinterpret_cast<std::uintptr_t>(g_stagingRing.mapped));
    copy.dstOffset = info.offset;
    copy.size = size;

    if (cmd != VK_NULL_HANDLE) {
        vkCmdCopyBuffer(cmd, g_stagingRing.buffer, info.buffer, 1, &copy);
    } else {
        VkCommandPool pool = {};
        VkCommandPoolCreateInfo pci = {};
        pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pci.queueFamilyIndex = StoneKey::stone_graphics_family();
        VK_CHECK(vkCreateCommandPool(stone_device(), &pci, nullptr, &pool));

        VkCommandBuffer tcmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo ai = {};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(stone_device(), &ai, &tcmd));

        VkCommandBufferBeginInfo bi = {};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(tcmd, &bi));
        vkCmdCopyBuffer(tcmd, g_stagingRing.buffer, info.buffer, 1, &copy);
        VK_CHECK(vkEndCommandBuffer(tcmd));

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fci = {};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VK_CHECK(vkCreateFence(stone_device(), &fci, nullptr, &fence));

        VkSubmitInfo si = {};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &tcmd;
        VK_CHECK(vkQueueSubmit(stone_graphics_queue(), 1, &si, fence));
        VK_CHECK(vkWaitForFences(stone_device(), 1, &fence, VK_TRUE, UINT64_MAX));

        vkDestroyFence(stone_device(), fence, nullptr);
        vkFreeCommandBuffers(stone_device(), pool, 1, &tcmd);
        vkDestroyCommandPool(stone_device(), pool, nullptr);
    }
}

// ── DESTROY — thread-safe
// =============================================================================
inline void destroy(uint64_t handle) noexcept {
    std::lock_guard<std::mutex> lock(g_bufferMutex);
    std::unordered_map<uint64_t, BufferInfo>::iterator it = g_buffers.find(handle);
    if (it == g_buffers.end()) return;

    BufferInfo& info = it->second;
    if (info.mapped != nullptr) {
        vkUnmapMemory(stone_device(), info.memory);
    }
    vkDestroyBuffer(stone_device(), info.buffer, nullptr);
    vkFreeMemory(stone_device(), info.memory, nullptr);

    g_buffers.erase(it);
}

// ── MACROS — ZERO-COST, PRINT-FRIENDLY
// =============================================================================
#define BM_CREATE(h, s, u, ...)             h = BufferManager::create(s, u, ##__VA_ARGS__)
#define BM_CREATE_DESCRIPTOR(h, s, ...)     h = BufferManager::createDescriptorBuffer(s, ##__VA_ARGS__)
#define BM_DESTROY(h)                       BufferManager::destroy(h)
#define BM_GET(h)                           BufferManager::get(h)
#define BM_GET_BUFFER(h)                    BufferManager::get_buffer(h)
#define BM_GET_MEMORY(h)                    BufferManager::get_memory(h)
#define BM_GET_DEVICE_ADDRESS(h)            BufferManager::get_device_address(h)
#define BM_UPLOAD_TO_BUFFER(h, d, sz, ...)   BufferManager::uploadToBuffer(h, d, sz, ##__VA_ARGS__)
#define BM_ALLOC_SCRATCH(sz)                BufferManager::allocateScratch(sz)
#define BM_LAZY_MAP_DESCRIPTOR(h)           BufferManager::lazyMapDescriptorBuffer(h)

} // namespace BufferManager