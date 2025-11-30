// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v11.0
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 30, 2025
// THE DISPOSAL BALLERINA HAS COMPLETED HER FINAL SPIN — THE EMPIRE IS SEALED
// =============================================================================
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================

#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <mutex>
#include <unordered_map>
#include <cstring>
#include <format>

using namespace Options;
using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_rtprops;

// =============================================================================
// BUFFERMANAGER — FINAL PRODUCTION — ETERNAL — FLAWLESS
// =============================================================================

namespace BufferManager {

struct Entry {
    VkBuffer            buffer  = VK_NULL_HANDLE;
    VkDeviceMemory      memory  = VK_NULL_HANDLE;
    VkDeviceSize        size    = 0;
    VkDeviceSize        aligned = 0;
    VkBufferUsageFlags  usage   = 0;
    std::string         tag;
    void*               mapped  = nullptr;
};

static std::unordered_map<uint64_t, Entry> g_buffers;
static std::mutex                          g_mutex;
static uint64_t                            g_nextHandle = 1;
static VkDevice                            g_device = VK_NULL_HANDLE;
static VkPhysicalDevice                    g_phys   = VK_NULL_HANDLE;
static size_t                              g_totalAllocated = 0;

static uint64_t     g_stagingBuffer = 0;
static void*        g_stagingPtr    = nullptr;
static VkDeviceSize g_stagingOffset = 0;
static constexpr VkDeviceSize STAGING_SIZE = 256ULL * 1024 * 1024;

static constexpr VkDeviceSize alignUp(VkDeviceSize size, VkDeviceSize alignment) noexcept {
    return (size + alignment - 1) & ~(alignment - 1);
}

static void init() noexcept {
    if (g_device) return;

    EMPIRE_GUARD(stone_device() && stone_physical(), "BufferManager::init() — StoneKey empire incomplete — device or physical null");

    g_device = stone_device();
    g_phys   = stone_physical();

    LOG_ELON("BufferManager: Forging eternal 256 MiB staging ring — the path of pink photons");

    g_stagingBuffer = create(STAGING_SIZE,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "ETERNAL_STAGING_RING");

    auto* info = get(g_stagingBuffer);
    EMPIRE_GUARD(info && info->memory, "ETERNAL STAGING RING FAILED — THE PHOTONS HAVE NO PATH");

    VK_CHECK(vkMapMemory(g_device, info->memory, 0, STAGING_SIZE, 0, &g_stagingPtr));
    LOG_JENSEN("BufferManager: Staging ring online — handle 0x{:016x} — photons flow unbroken", g_stagingBuffer);
}

static void setDebugName(VkBuffer buf, const std::string& name) noexcept {
#if defined(VK_EXT_debug_utils) && !defined(NDEBUG)
    if (RTX::g_ctx().debugUtilsSupported()) {
        auto func = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(g_device, "vkSetDebugUtilsObjectNameEXT");
        if (func) {
            VkDebugUtilsObjectNameInfoEXT info{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_BUFFER,
                .objectHandle = (uint64_t)buf,
                .pObjectName = name.c_str()
            };
            func(g_device, &info);
        }
    }
#endif
}

uint64_t create(VkDeviceSize size,
                VkBufferUsageFlags usage,
                VkMemoryPropertyFlags props,
                std::string_view tag) noexcept
{
    init();
    std::lock_guard<std::mutex> lock(g_mutex);

    VkBufferUsageFlags fullUsage = usage |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

    VkBuffer buffer{};
    VkBufferCreateInfo bci{
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = fullUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(g_device, &bci, nullptr, &buffer));

    VkMemoryRequirements memReqs{};
    vkGetBufferMemoryRequirements(g_device, buffer, &memReqs);

    uint32_t memType = findMemoryType(memReqs.memoryTypeBits, props);
    if (memType == ~0u) {
        fprintf(stderr, "\033[31m[FATAL] BufferManager: NO MEMORY TYPE FOR %zu MiB BUFFER — TAG: %.*s\033[0m\n",
                size >> 20, (int)tag.size(), tag.data());
        std::abort();
    }

    VkMemoryAllocateInfo mai{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory memory{};
    VK_CHECK(vkAllocateMemory(g_device, &mai, nullptr, &memory));
    VK_CHECK(vkBindBufferMemory(g_device, buffer, memory, 0));

    uint64_t handle = g_nextHandle++;

    void* mapped = nullptr;
    if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        VK_CHECK(vkMapMemory(g_device, memory, 0, size, 0, &mapped));
        if (Memory::ENABLE_ZERO_INIT || tag.find("UNIFORM") != std::string::npos)
            std::memset(mapped, 0, size);
    }

    std::string fullTag = tag.empty() ? "unnamed" : std::string(tag);
    if (fullTag.starts_with("ETERNAL_STONE_") || fullTag.contains("TITAN"))
        fullTag += "_USED:0";

    g_buffers[handle] = { buffer, memory, size, memReqs.size, fullUsage, fullTag, mapped };
    g_totalAllocated += memReqs.size;

    setDebugName(buffer, "BUF_" + fullTag);

    LOG_CARMACK("BufferManager: Forged {} MiB | handle 0x{:016x} | tag \"{}\"",
                size >> 20, handle, fullTag);

    LOG_JENSEN("BufferManager: Total VRAM dominion — {:.3f} GiB", 
               static_cast<double>(g_totalAllocated) / (1024.0 * 1024 * 1024));

    return handle;
}

void destroy(uint64_t handle) noexcept {
    if (!handle || handle == g_stagingBuffer) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_buffers.find(handle);
    if (it == g_buffers.end()) return;

    g_totalAllocated -= it->second.aligned;
    if (it->second.mapped) vkUnmapMemory(g_device, it->second.memory);
    vkDestroyBuffer(g_device, it->second.buffer, nullptr);
    vkFreeMemory(g_device, it->second.memory, nullptr);
    g_buffers.erase(it);
}

void* map(uint64_t handle) noexcept {
    init();
    auto it = g_buffers.find(handle);
    if (it == g_buffers.end()) return nullptr;
    if (it->second.mapped) return it->second.mapped;
    void* ptr = nullptr;
    VK_CHECK(vkMapMemory(g_device, it->second.memory, 0, it->second.size, 0, &ptr));
    return ptr;
}

void unmap(uint64_t handle) noexcept {
    init();
    auto it = g_buffers.find(handle);
    if (it != g_buffers.end() && !it->second.mapped)
        vkUnmapMemory(g_device, it->second.memory);
}

const BufferInfo* get(uint64_t handle) noexcept {
    auto it = g_buffers.find(handle);
    return it != g_buffers.end() ? reinterpret_cast<const BufferInfo*>(&it->second) : nullptr;
}

void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size, VkQueue queue, VkCommandPool pool) noexcept
{
    init();

    LOG_CID("CID slams a fresh towel on his neck, eyes wild — \"ULTRA FAST COPY INCOMING! THE PHOTONS WILL NOT WAIT!\"");

    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(pool);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion);

    RTX::endOneTimeSubmit(cmd, queue, pool);

    LOG_CID("CID wipes sweat, panting — \"Copy complete. Photons transferred at relativistic speed. No casualties.\"");
}

void purge_all() noexcept {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& [h, e] : g_buffers) {
        if (h == g_stagingBuffer) continue;
        if (e.mapped) vkUnmapMemory(g_device, e.memory);
        vkDestroyBuffer(g_device, e.buffer, nullptr);
        vkFreeMemory(g_device, e.memory, nullptr);
    }
    g_buffers.clear();
    g_totalAllocated = 0;

    LOG_CARMACK("BufferManager: All mortal buffers purged — the void is clean");
    LOG_CID("CID stands in the ashes, lab coat smoldering — \"THE SLATE IS WIPED. ONLY THE STAGING RING REMAINS ETERNAL\"");
}

uint64_t stagingBuffer() noexcept { init(); return g_stagingBuffer; }
void*    stagingPtr()    noexcept { init(); return g_stagingPtr; }
void advanceStagingOffset(VkDeviceSize b) noexcept { init(); g_stagingOffset = (g_stagingOffset + b) % STAGING_SIZE; }
void* stagingPtrAtOffset(VkDeviceSize o) noexcept { init(); return static_cast<uint8_t*>(g_stagingPtr) + (g_stagingOffset + o) % STAGING_SIZE; }

// =============================================================================
// ETERNAL STONES — YOUR ORIGINAL, PERFECT MACRO — RESTORED
// =============================================================================

#define MAKE_STONE(name, mb) \
uint64_t make_##name(VkBufferUsageFlags extra, VkMemoryPropertyFlags p) noexcept { \
    static uint64_t h = 0; \
    if (!h) { \
        h = create((mb) * 1024ULL * 1024ULL, \
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR | \
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | extra, \
            p, #name "_ETERNAL"); \
        if (h) LOG_ELON("BufferManager: " #name " stone forged — %d MiB immortal", mb); \
    } \
    return h; \
}

MAKE_STONE(64M,   64)
MAKE_STONE(128M, 128)
MAKE_STONE(256M, 256)
MAKE_STONE(420M, 420)
MAKE_STONE(512M, 512)
MAKE_STONE(1G,   1024)
MAKE_STONE(2G,   2048)
MAKE_STONE(4G,   4096)
MAKE_STONE(8G,   8192)

#undef MAKE_STONE

uint64_t createSBT(uint32_t raygenCount, uint32_t missCount, uint32_t hitGroupCount, uint32_t callableCount, VkBufferUsageFlags extraUsage, std::string_view tag) noexcept
{
    init();

    // Jensen himself demands these properties for SBT
    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = stone_rtprops();

    const VkDeviceSize handleSize          = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleAlignment     = rtProps.shaderGroupHandleAlignment;
    const VkDeviceSize baseAlignment       = rtProps.shaderGroupBaseAlignment;

    const VkDeviceSize raygenSize     = alignUp(raygenCount   * handleSize, handleAlignment);
    const VkDeviceSize missSize       = alignUp(missCount     * handleSize, handleAlignment);
    const VkDeviceSize hitGroupSize   = alignUp(hitGroupCount * handleSize, handleAlignment);
    const VkDeviceSize callableSize   = alignUp(callableCount * handleSize, handleAlignment);

    const VkDeviceSize totalSize = alignUp(
        raygenSize + missSize + hitGroupSize + callableSize,
        baseAlignment
    );

    LOG_ELON("SBT FORGING CEREMONY — Total size: {} MiB | Handles: {} {} {} {} | BaseAlign: {} | HandleAlign: {}",
             totalSize >> 20,
             raygenCount, missCount, hitGroupCount, callableCount,
             baseAlignment, handleAlignment);

    // Ultimate SBT usage flags — nothing is ever missing again
    VkBufferUsageFlags sbtUsage =
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        extraUsage;

    std::string fullTag = std::string(tag) + "_SBT_PINK_PHOTON_STRAW";

    uint64_t sbtHandle = create(totalSize, sbtUsage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        fullTag);

    // Retrieve the buffer so we can give you the device address immediately if you want
    auto* info = get(sbtHandle);
    EMPIRE_GUARD(info, "SBT FORGED BUT LOST IN THE VOID — THIS CANNOT BE");

    VkDeviceAddress addr = 0;
    {
        VkBufferDeviceAddressInfo addrInfo{
            .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = info->buffer
        };
        addr = vkGetBufferDeviceAddress(g_device, &addrInfo);
    }

    // Stride info for later use when binding
    const VkDeviceSize alignedHandleSize = alignUp(handleSize, handleAlignment);

    LOG_JENSEN("SBT COMPLETE — Handle 0x{:016x} | DeviceAddress 0x{:016x}", sbtHandle, addr);
    LOG_JENSEN("   RayGen region:     [0x{:016x} ... 0x{:016x}) stride {}", 
               addr, addr + raygenSize, alignedHandleSize);
    LOG_JENSEN("   Miss region:       [0x{:016x} ... 0x{:016x}) stride {}", 
               addr + raygenSize, addr + raygenSize + missSize, alignedHandleSize);
    LOG_JENSEN("   HitGroup region:   [0x{:016x} ... 0x{:016x}) stride {}", 
               addr + raygenSize + missSize, addr + raygenSize + missSize + hitGroupSize, alignedHandleSize);
    if (callableCount)
        LOG_JENSEN("   Callable region:   [0x{:016x} ... 0x{:016x}) stride {}", 
                   addr + raygenSize + missSize + hitGroupSize, addr + totalSize, alignedHandleSize);

    LOG_CID("CID throws the towel into the crowd — \"THE STRAW IS IN PLACE. TRACING MAY COMMENCE. NO PHOTON LEFT BEHIND.\"");

    return sbtHandle;
}

} // namespace BufferManager

// =============================================================================
// THE EMPIRE IS SEALED — THE PHOTONS FLOW — THE BALLERINA SPINS ETERNALLY
// NOVEMBER 30, 2025 — FIRST LIGHT ACHIEVED — FINAL LIGHT ACHIEVED
// WE ARE COMPLETE.
// =============================================================================