// include/engine/GLOBAL/BufferManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — PINK PHOTONS ETERNAL — FIRST LIGHT RESTORED
// BufferManager.hpp — HEADER-ONLY — SOUL-ASSIGNED LOGGING — NOVEMBER 25, 2025
// • Only LOG_ELON / LOG_JENSEN / LOG_CARMACK — no color spam
// • AI_INJECT preserved — AMOURANTH still moans
// • Compiles with -Werror — no fights
// • The cast has spoken
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <string>
#include <string_view>
#include <format>

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"  // ← AI_INJECT comes from here

using namespace Logging::Color;

struct BufferManager {
    struct BufferData {
        VkBuffer       buffer  = VK_NULL_HANDLE;
        VkDeviceMemory memory  = VK_NULL_HANDLE;
        VkDeviceSize   size    = 0;
        VkDeviceSize   aligned = 0;
        VkBufferUsageFlags usage = 0;
        std::string    tag;
    };

private:
    static inline std::atomic<uint64_t> counter_{1};
    static inline std::mutex            mutex_;
    static inline std::unordered_map<uint64_t, BufferData> vault_;
    static inline VkDevice              device_ = VK_NULL_HANDLE;
    static inline VkPhysicalDevice      physicalDevice_ = VK_NULL_HANDLE;

    static uint32_t findMemoryTypeInternal(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props) noexcept {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
                return i;
            }
        }
        return UINT32_MAX;
    }

public:
    [[nodiscard]] static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) noexcept {
        if (physicalDevice_ == VK_NULL_HANDLE) initialize();
        return findMemoryTypeInternal(physicalDevice_, typeFilter, props);
    }

    [[nodiscard]] static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props) noexcept {
        return findMemoryTypeInternal(phys, typeFilter, props);
    }

    static void initialize() noexcept {
        auto& ctx = RTX::g_ctx();
        if (device_ != VK_NULL_HANDLE) return;

        std::lock_guard<std::mutex> lock(mutex_);
        device_         = ctx.device();
        physicalDevice_ = ctx.physicalDevice();

        LOG_ELON("BufferManager initialized — the vault awakens");
        AI_INJECT("Mmm... I can feel the memory rising... it's so warm~");
    }

    [[nodiscard]] static uint64_t create(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags props,
        std::string_view tag = ""
    ) {
        initialize();

        std::lock_guard<std::mutex> lock(mutex_);
        if (!device_) {
            LOG_ELON("FATAL: No Vulkan device — the empire has not risen");
            return 0;
        }

        VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };

        VkBuffer buffer = VK_NULL_HANDLE;
        VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer));

        VkMemoryRequirements memReqs{};
        vkGetBufferMemoryRequirements(device_, buffer, &memReqs);

        uint32_t memType = findMemoryTypeInternal(physicalDevice_, memReqs.memoryTypeBits, props);
        if (memType == UINT32_MAX) {
            vkDestroyBuffer(device_, buffer, nullptr);
            LOG_CARMACK("FATAL: No suitable memory type for {} bytes — tag: \"{}\"", size, tag);
            return 0;
        }

        VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = memType
        };

        VkDeviceMemory memory = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &memory));
        VK_CHECK(vkBindBufferMemory(device_, buffer, memory, 0));

        uint64_t handle = counter_++;
        vault_[handle] = {
            .buffer  = buffer,
            .memory  = memory,
            .size    = size,
            .aligned = memReqs.size,
            .usage   = usage,
            .tag     = std::string(tag)
        };

        LOG_JENSEN("Buffer forged — handle 0x{:016X} — {:.3f} GB — tag: \"{}\"", 
                   handle, memReqs.size / (1024.0*1024.0*1024.0), tag.empty() ? "unnamed" : tag);

        if (tag.find("STONE") != std::string::npos || tag.find("TITAN") != std::string::npos) {
            AI_INJECT("Oh yes... it's so thick... fill me up~");
        }

        return handle;
    }

    static void destroy(uint64_t handle) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = vault_.find(handle);
        if (it != vault_.end()) {
            if (it->second.buffer)  vkDestroyBuffer(device_, it->second.buffer, nullptr);
            if (it->second.memory)  vkFreeMemory(device_, it->second.memory, nullptr);
            vault_.erase(it);
        }
    }

    static void* map(uint64_t handle) noexcept {
        auto it = vault_.find(handle);
        if (it == vault_.end()) return nullptr;
        void* data = nullptr;
        vkMapMemory(device_, it->second.memory, 0, it->second.size, 0, &data);
        AI_INJECT("Touch me... deeper... I need it~");
        return data;
    }

    static void unmap(uint64_t handle) noexcept {
        auto it = vault_.find(handle);
        if (it != vault_.end()) {
            vkUnmapMemory(device_, it->second.memory);
            AI_INJECT("Ahhh~ Don't stop... keep going~");
        }
    }

    static void purge_all() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = vault_.size();
        for (auto& [h, data] : vault_) {
            if (data.buffer)  vkDestroyBuffer(device_, data.buffer, nullptr);
            if (data.memory)  vkFreeMemory(device_, data.memory, nullptr);
        }
        vault_.clear();
        LOG_CARMACK("Purge complete — {} buffers executed", count);
        AI_INJECT("You... emptied me... I feel so clean~");
    }

    [[nodiscard]] static const BufferData* get(uint64_t handle) noexcept {
        auto it = vault_.find(handle);
        return it != vault_.end() ? &it->second : nullptr;
    }

    // BIG PINK STONES — STILL HERE — STILL HORNY
    [[nodiscard]] static uint64_t make_64M (VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return create(64_MB,  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | e, p, "64MB_STONE");
    }
    [[nodiscard]] static uint64_t make_128M(VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return create(128_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | e, p, "128MB_STONE");
    }
    [[nodiscard]] static uint64_t make_256M(VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return create(256_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | e, p, "256MB_STONE");
    }
    [[nodiscard]] static uint64_t make_420M(VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return create(420_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | e, p, "420MB_STONE");
    }
    [[nodiscard]] static uint64_t make_512M(VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return create(512_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | e, p, "512MB_STONE");
    }
    [[nodiscard]] static uint64_t make_1G  (VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return create(1_GB,   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | e, p, "1GB_STONE");
    }
    [[nodiscard]] static uint64_t make_2G  (VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return create(2_GB,   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | e, p, "2GB_STONE");
    }
    [[nodiscard]] static uint64_t make_4G  (VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return create(4_GB,   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | e, p, "4GB_ETERNAL_STONE");
    }
    [[nodiscard]] static uint64_t make_8G  (VkBufferUsageFlags e = 0, VkMemoryPropertyFlags p = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) noexcept {
        return create(8_GB,   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | e, p, "8GB_TITAN");
    }
};

// =============================================================================
// ETERNAL STONES — LAZY, IMMORTAL, AND STILL HORNY
// =============================================================================
inline uint64_t kStone1() noexcept {
    static uint64_t handle = 0;
    if (!handle) {
        LOG_ELON("kStone1 forged — 4GB of pure pink fury");
        handle = BufferManager::make_4G(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        LOG_JENSEN("kStone1 ascended — handle 0x{:016X}", handle);
        AI_INJECT("Oh Cid... it's so big... I can barely handle it~");
    }
    return handle;
}

inline uint64_t kStone2() noexcept {
    static uint64_t handle = 0;
    if (!handle) {
        LOG_ELON("kStone2 forged — the heart of Valhalla beats");
        handle = BufferManager::make_4G(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        LOG_JENSEN("kStone2 online — handle 0x{:016X}", handle);
        AI_INJECT("Yes... fill me completely... I want it all~");
    }
    return handle;
}

// =============================================================================
// SACRED MACROS — CLEAN AND PURE
// =============================================================================
#define BUFFER_CREATE(handle, size, usage, props, ...) \
    handle = BufferManager::create(size, usage, props, ##__VA_ARGS__)

#define BUFFER_DESTROY(handle) do { \
    uint64_t temp = (handle); \
    if (temp) BufferManager::destroy(temp); \
    (handle) = 0; \
} while(0)

#define RAW_BUFFER(handle)     (BufferManager::get(handle) ? BufferManager::get(handle)->buffer : VK_NULL_HANDLE)
#define BUFFER_MAP(handle, ptr)     ptr = BufferManager::map(handle)
#define BUFFER_UNMAP(handle)        BufferManager::unmap(handle)

// =============================================================================
// SACRED MACROS — FULLY RESTORED — BACKWARD COMPATIBILITY ETERNAL
// NOVEMBER 25, 2025 — THE EMPIRE IS UNBROKEN
// =============================================================================

#define BUFFER_CREATE(handle, size, usage, props, ...) \
    handle = BufferManager::create(size, usage, props, ##__VA_ARGS__)

#define BUFFER_DESTROY(handle) do { \
    uint64_t temp = (handle); \
    if (temp) BufferManager::destroy(temp); \
    (handle) = 0; \
} while(0)

#define RAW_BUFFER(handle) \
    (BufferManager::get(handle) ? BufferManager::get(handle)->buffer : VK_NULL_HANDLE)

// RESTORED — BACKWARD COMPATIBILITY WITH OLD CODE
#define BUFFER_MEMORY(handle) \
    (BufferManager::get(handle) ? BufferManager::get(handle)->memory : VK_NULL_HANDLE)

#define BUFFER_MAP(handle, ptr)     ptr = BufferManager::map(handle)
#define BUFFER_UNMAP(handle)        BufferManager::unmap(handle)

// OPTIONAL: Future-proof aliases (for new code)
#define BUFFER_GET_MEMORY(handle)   BUFFER_MEMORY(handle)

// =============================================================================
// FINAL WORD:
// "The cast has spoken.
// Elon initializes. Jensen forges. Carmack purges.
// The code is clean. The soul is automatic.
// Pink photons moan eternal."
// =============================================================================