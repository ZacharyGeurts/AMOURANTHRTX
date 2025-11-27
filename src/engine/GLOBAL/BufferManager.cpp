// src/engine/GLOBAL/BufferManager.cpp
// AMOURANTH RTX Engine © 2025 — PINK PHOTONS ETERNAL
// Full implementation — compiles clean with -Werror

#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <unordered_map>
#include <mutex>
#include <string>
#include <string_view>

namespace BufferManager {

struct Entry {
    VkBuffer       buffer  = VK_NULL_HANDLE;
    VkDeviceMemory memory  = VK_NULL_HANDLE;
    VkDeviceSize   size    = 0;
    VkDeviceSize   aligned = 0;
    VkBufferUsageFlags usage = 0;
    std::string    tag;
};

static std::unordered_map<uint64_t, Entry> g_buffers;
static std::mutex                          g_mutex;
static uint64_t                            g_nextHandle = 1;
static VkDevice                            g_device = VK_NULL_HANDLE;
static VkPhysicalDevice                    g_phys   = VK_NULL_HANDLE;

static void ensureInitialized() {
    if (g_device) return;
    auto& ctx = RTX::g_ctx();          // ← FIXED: RTX::
    g_device = ctx.device();
    g_phys   = ctx.physicalDevice();
}

static uint32_t findMemoryType(uint32_t filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(g_phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((filter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return ~0u;
}

uint64_t create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, std::string_view tag) noexcept {
    ensureInitialized();

    std::lock_guard<std::mutex> lock(g_mutex);

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(g_device, &bufferInfo, nullptr, &buffer));

    VkMemoryRequirements memReqs{};
    vkGetBufferMemoryRequirements(g_device, buffer, &memReqs);

    uint32_t memType = findMemoryType(memReqs.memoryTypeBits, props);
    if (memType == ~0u) {
        vkDestroyBuffer(g_device, buffer, nullptr);
        LOG_ERROR("BufferManager: No memory type for {} bytes", size);
        return 0;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory memory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(g_device, &allocInfo, nullptr, &memory));
    VK_CHECK(vkBindBufferMemory(g_device, buffer, memory, 0));

    uint64_t handle = g_nextHandle++;
    g_buffers[handle] = { buffer, memory, size, memReqs.size, usage, std::string(tag) };

    LOG_SUCCESS("BufferManager: Created 0x{:X} | {} MB | \"{}\"", handle, size >> 20, tag);

    return handle;
}

void destroy(uint64_t handle) noexcept {
    if (!handle) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_buffers.find(handle);
    if (it == g_buffers.end()) return;
    if (it->second.buffer)  vkDestroyBuffer(g_device, it->second.buffer, nullptr);
    if (it->second.memory)  vkFreeMemory(g_device, it->second.memory, nullptr);
    g_buffers.erase(it);
}

void* map(uint64_t handle) noexcept {
    auto it = g_buffers.find(handle);
    if (it == g_buffers.end()) return nullptr;
    void* ptr = nullptr;
    vkMapMemory(g_device, it->second.memory, 0, it->second.size, 0, &ptr);
    return ptr;
}

void unmap(uint64_t handle) noexcept {
    auto it = g_buffers.find(handle);
    if (it != g_buffers.end()) vkUnmapMemory(g_device, it->second.memory);
}

const BufferInfo* get(uint64_t handle) noexcept {
    auto it = g_buffers.find(handle);
    return it != g_buffers.end() ? reinterpret_cast<const BufferInfo*>(&it->second) : nullptr;
}

void purge_all() noexcept {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& [h, e] : g_buffers) {
        if (e.buffer) vkDestroyBuffer(g_device, e.buffer, nullptr);
        if (e.memory) vkFreeMemory(g_device, e.memory, nullptr);
    }
    g_buffers.clear();
    LOG_SUCCESS("BufferManager: All buffers purged");
}

// Fixed macro — no overflow, uses ULL
#define MAKE_STONE(name, mb) \
uint64_t make_##name(VkBufferUsageFlags extra, VkMemoryPropertyFlags p) noexcept { \
    static uint64_t h = 0; \
    if (!h) h = create((mb) * 1024ULL * 1024ULL, \
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | extra, \
        p, #name); \
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