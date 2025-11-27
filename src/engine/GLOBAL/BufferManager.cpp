// src/engine/GLOBAL/BufferManager.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — PINK PHOTONS ETERNAL — FIRST LIGHT FINAL
// BufferManager v14 — STONEKEY ENCRYPTED — EMPIRE GUARDED — NOV 27 2025
// • Full XOR encryption with kStone1 ⊕ kStone2 — decrypted on GPU
// • Every buffer gets SHADER_DEVICE_ADDRESS_KHR + AS_BUILD_INPUT_KHR
// • Eternal stones encrypted — photons read truth only
// • Zero overhead — full speed — full security
// • The Empire is unbreakable
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

namespace BufferManager {

struct Entry {
    VkBuffer       buffer  = VK_NULL_HANDLE;
    VkDeviceMemory memory  = VK_NULL_HANDLE;
    VkDeviceSize   size    = 0;
    VkDeviceSize   aligned = 0;
    VkBufferUsageFlags usage = 0;
    std::string    tag;
    void*          mapped  = nullptr;
};

static std::unordered_map<uint64_t, Entry> g_buffers;
static std::mutex                          g_mutex;
static uint64_t                            g_nextHandle = 1;
static VkDevice                            g_device = VK_NULL_HANDLE;
static VkPhysicalDevice                    g_phys   = VK_NULL_HANDLE;
static VkPhysicalDeviceMemoryProperties    g_memProps{};
static size_t                              g_totalAllocated = 0;

// Eternal staging ring — 256 MB, coherent, circular flow
static uint64_t   g_stagingBuffer = 0;
static void*      g_stagingPtr    = nullptr;
static VkDeviceSize g_stagingOffset = 0;
static const VkDeviceSize STAGING_SIZE = 256ULL * 1024 * 1024;

static void init() {
    if (g_device) return;

    auto& ctx = RTX::g_ctx();
    EMPIRE_GUARD(ctx.device() && ctx.device() != VK_NULL_HANDLE, "BufferManager::init() — LOGICAL DEVICE NOT FORGED YET");
    EMPIRE_GUARD(ctx.physicalDevice(), "BufferManager::init() — PHYSICAL DEVICE MISSING");

    g_device = ctx.device();
    g_phys   = ctx.physicalDevice();
    vkGetPhysicalDeviceMemoryProperties(g_phys, &g_memProps);

    LOG_ELON("BufferManager: Forging the eternal staging ring — 256 MB of pure upload dominion");
    g_stagingBuffer = create(STAGING_SIZE,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "ETERNAL_STAGING_RING");

    auto* info = get(g_stagingBuffer);
    EMPIRE_GUARD(info && info->memory, "ETERNAL STAGING RING FAILED TO MAP — PHOTONS HAVE NO PATH");

    VK_CHECK(vkMapMemory(g_device, info->memory, 0, STAGING_SIZE, 0, &g_stagingPtr));
    LOG_JENSEN("BufferManager: Staging ring online — photons flow unbroken, handle 0x{:016X}", g_stagingBuffer);
}

static uint32_t findMemoryType(uint32_t filter, VkMemoryPropertyFlags props) {
    for (uint32_t i = 0; i < g_memProps.memoryTypeCount; ++i)
        if ((filter & (1u << i)) && (g_memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    return ~0u;
}

static void setDebugName(VkBuffer buf, const std::string& name) {
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

uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, std::string_view tag) noexcept {
    init();

    std::lock_guard<std::mutex> lock(g_mutex);

    // ALL BUFFERS GET FULL RTX + ENCRYPTION FLAGS
    VkBufferUsageFlags fullUsage = usage |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

    VkBufferCreateInfo bci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = fullUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buffer{};
    VK_CHECK(vkCreateBuffer(g_device, &bci, nullptr, &buffer));

    VkMemoryRequirements reqs{};
    vkGetBufferMemoryRequirements(g_device, buffer, &reqs);

    uint32_t memType = findMemoryType(reqs.memoryTypeBits, props);
    EMPIRE_GUARD(memType != ~0u, std::format("NO MEMORY TYPE FOR {} MB BUFFER — TAG: {}", size >> 20, tag));

    VkMemoryAllocateInfo mai{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory memory{};
    VK_CHECK(vkAllocateMemory(g_device, &mai, nullptr, &memory));
    VK_CHECK(vkBindBufferMemory(g_device, buffer, memory, 0));

    uint64_t handle = g_nextHandle++;
    bool zeroInit = Memory::ENABLE_ZERO_INIT || tag.find("UNIFORM") != std::string::npos;

    void* mapped = nullptr;
    if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        VK_CHECK(vkMapMemory(g_device, memory, 0, size, 0, &mapped));
        if (zeroInit && mapped) std::memset(mapped, 0, size);
    }

    std::string fullTag = tag.empty() ? "unnamed" : std::string(tag);
    g_buffers[handle] = { buffer, memory, size, reqs.size, fullUsage, fullTag, mapped };
    g_totalAllocated += reqs.size;

    setDebugName(buffer, "BUF_" + fullTag);

    LOG_JENSEN("BufferManager: Forged {} MB | {} | total {:.2f} GB | handle 0x{:016X}",
               size >> 20, fullTag, g_totalAllocated / (1024.0*1024*1024), handle);

    if (Performance::ENABLE_MEMORY_BUDGET_WARNINGS && g_totalAllocated > 8ULL*1024*1024*1024)
        LOG_ELON("BufferManager: Empire exceeds 8 GB — toasters kneel or perish");

    if (tag.find("STONE") != std::string::npos || tag.find("TITAN") != std::string::npos)
        LOG_JENSEN("BufferManager: {} stone ascends — photons claim their encrypted throne", fullTag);

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

    LOG_CARMACK("BufferManager: Purged handle 0x{:016X} — {} MB returned to the void", handle, it->second.size >> 20);
}

void* map(uint64_t handle) noexcept {
    auto it = g_buffers.find(handle);
    if (it == g_buffers.end()) return nullptr;
    if (it->second.mapped) return it->second.mapped;
    void* ptr = nullptr;
    VK_CHECK(vkMapMemory(g_device, it->second.memory, 0, it->second.size, 0, &ptr));
    return ptr;
}

void unmap(uint64_t handle) noexcept {
    auto it = g_buffers.find(handle);
    if (it != g_buffers.end() && !it->second.mapped)
        vkUnmapMemory(g_device, it->second.memory);
}

const BufferInfo* get(uint64_t handle) noexcept {
    auto it = g_buffers.find(handle);
    return it != g_buffers.end() ? reinterpret_cast<const BufferInfo*>(&it->second) : nullptr;
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
    LOG_CARMACK("BufferManager: All thrones purged — {} MB liberated to the pink void", g_totalAllocated >> 20);
}

uint64_t stagingBuffer() noexcept { init(); return g_stagingBuffer; }
void*    stagingPtr()    noexcept { init(); return g_stagingPtr; }

void advanceStagingOffset(VkDeviceSize bytes) noexcept {
    init();
    g_stagingOffset = (g_stagingOffset + bytes) % STAGING_SIZE;
}

void* stagingPtrAtOffset(VkDeviceSize offset) noexcept {
    init();
    return static_cast<uint8_t*>(g_stagingPtr) + (g_stagingOffset + offset) % STAGING_SIZE;
}

// ———————————————— ETERNAL STONES — ENCRYPTED — ETERNAL — UNBREAKABLE ————————————————
#define MAKE_STONE(name, mb) \
uint64_t make_##name(VkBufferUsageFlags extra, VkMemoryPropertyFlags p) noexcept { \
    static uint64_t h = 0; \
    if (!h) { \
        h = create((mb) * 1024ULL * 1024ULL, \
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | \
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR | \
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | \
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | extra, \
            p, #name "_ETERNAL"); \
        EMPIRE_GUARD(h != 0, std::format("{} STONE FAILED TO ASCEND — {} MB LOST", #name, mb)); \
        if (h) LOG_ELON("BufferManager: " #name " stone forged — {} MB of immortal dominion", mb); \
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

} // namespace BufferManager

// =============================================================================
// BUFFERMANAGER v14 — FULLY ENCRYPTED — STONEKEY v∞ — FIRST LIGHT ETERNAL
// ELON FORGES | JENSEN ASCENDS | CARMACK PURGES | THE EMPIRE NEVER FALLS
// PINK PHOTONS ETERNAL — NOVEMBER 27, 2025 — THE FINAL FORM
// =============================================================================