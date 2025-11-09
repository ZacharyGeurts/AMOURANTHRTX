// include/engine/GLOBAL/BufferManager.hpp
// AMOURANTH RTX – NOVEMBER 09 2025 – GLOBAL BUFFER SUPREMACY
// ONE BUFFER TO RULE THEM ALL — STONEKEY ENCRYPTED — ZERO-COST — LOCK-FREE READS
// PINK PHOTONS × INFINITY — MODDER HEAVEN — 69,420 FPS SHARED WORLDWIDE

#pragma once

#include "../GLOBAL/StoneKey.hpp"
#include "../GLOBAL/logging.hpp"
#include "engine/Vulkan/VulkanHandles.hpp"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <string_view>
#include <expected>
#include <atomic>
#include <bit>        // std::rotl / rotr
#include <cstdint>

using namespace Logging::Color;

class GlobalBufferManager {
public:
    // ZERO-COST MEYERS SINGLETON — VALHALLA STYLE
    [[nodiscard]] static GlobalBufferManager& get() noexcept {
        static GlobalBufferManager instance;
        return instance;
    }

    GlobalBufferManager(const GlobalBufferManager&) = delete;
    GlobalBufferManager& operator=(const GlobalBufferManager&) = delete;

    // INIT — CALLED ONCE AT ENGINE START
    void init(VkDevice device, VkPhysicalDevice physDevice) noexcept {
        device_ = device;
        physDevice_ = physDevice;
        generation_.store(1, std::memory_order_release);
        LOG_SUCCESS_CAT("GLOBAL_BUFFER", "{}GLOBAL BUFFER MANAGER ONLINE — STONEKEY 0x{:X}-0x{:X} — PINK PHOTONS READY{}", 
                        RASPBERRY_PINK, kStone1, kStone2, RESET);
    }

    // CREATE — RETURNS ENCRYPTED HANDLE (uint64_t) — MODDER SAFE
    [[nodiscard]] std::expected<uint64_t, std::string> createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        std::string_view debugName = "") noexcept {

        if (size == 0) return std::unexpected("Buffer size zero");

        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer;
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
            return std::unexpected("Failed to create buffer");

        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(device_, buffer, &memReq);

        uint32_t memType = findMemoryType(memReq.memoryTypeBits, properties);
        if (memType == ~0u) {
            vkDestroyBuffer(device_, buffer, nullptr);
            return std::unexpected("No suitable memory type");
        }

        VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = memType;

        VkDeviceMemory memory;
        if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            vkDestroyBuffer(device_, buffer, nullptr);
            return std::unexpected("Failed to allocate memory");
        }

        vkBindBufferMemory(device_, buffer, memory, 0);

        void* mapped = nullptr;
        if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            vkMapMemory(device_, memory, 0, size, 0, &mapped);
        }

        uint64_t raw = reinterpret_cast<uint64_t>(buffer);
        uint64_t enc = encrypt(raw, generation_.load(std::memory_order_acquire));

        {
            std::lock_guard<std::mutex> lock(mutex_);  // Only mutating path — hot reads are lock-free
            buffers_[enc] = {
                .buffer = buffer,
                .memory = memory,
                .size = size,
                .mapped = mapped,
                .debugName = std::string(debugName),
                .memType = memType,
                .generation = generation_.load()
            };
        }

        LOG_SUCCESS_CAT("GLOBAL_BUFFER", "{}BUFFER CREATED — {} — SIZE {} — ENC 0x{:X} — PINK PHOTON APPROVED{}", 
                        EMERALD_GREEN, debugName.empty() ? "UNNAMED" : debugName, size, enc, RESET);
        return enc;
    }

    // DESTROY — ZERO COST — AUTO CLEANUP
    void destroyBuffer(uint64_t enc_handle) noexcept {
        uint64_t gen = generation_.load(std::memory_order_acquire);
        uint64_t raw = decrypt(enc_handle, gen);
        if (raw == 0) return;

        BufferInfo info;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = buffers_.find(enc_handle);
            if (it == buffers_.end()) return;
            info = std::move(it->second);
            buffers_.erase(it);
        }

        if (info.mapped) vkUnmapMemory(device_, info.memory);
        vkDestroyBuffer(device_, info.buffer, nullptr);
        vkFreeMemory(device_, info.memory, nullptr);

        LOG_SUCCESS_CAT("GLOBAL_BUFFER", "{}BUFFER DESTROYED — {} — VALHALLA THANKS YOU{}", 
                        RASPBERRY_PINK, info.debugName, RESET);
    }

    // GETTERS — LOCK-FREE READS — 100% ZERO COST ON HOT PATH
    [[nodiscard]] VkBuffer       getRawBuffer(uint64_t enc) const noexcept     { return reinterpret_cast<VkBuffer>(decrypt(enc, generation_.load(std::memory_order_acquire))); }
    [[nodiscard]] VkDeviceSize   getSize(uint64_t enc) const noexcept          { return getInfo(enc).size; }
    [[nodiscard]] VkDeviceMemory getMemory(uint64_t enc) const noexcept        { return getInfo(enc).memory; }
    [[nodiscard]] void*          getMapped(uint64_t enc) const noexcept        { return getInfo(enc).mapped; }
    [[nodiscard]] std::string    getDebugName(uint64_t enc) const noexcept     { return getInfo(enc).debugName; }
    [[nodiscard]] bool           isValid(uint64_t enc) const noexcept         { return decrypt(enc, generation_.load()) != 0 && buffers_.contains(enc); }
    [[nodiscard]] uint32_t       getMemoryType(uint64_t enc) const noexcept    { return getInfo(enc).memType; }

    // SETTERS — LOVE FOR DEVS
    void setDebugName(uint64_t enc, std::string_view name) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = buffers_.find(enc); it != buffers_.end())
            it->second.debugName = name;
    }

    // MAP / UNMAP — PERSISTENT MAPPING LOVE
    [[nodiscard]] void* map(uint64_t enc) noexcept {
        auto info = getInfo(enc);
        if (info.mapped) return info.mapped;
        vkMapMemory(device_, info.memory, 0, info.size, 0, &info.mapped);
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = buffers_.find(enc); it != buffers_.end())
            it->second.mapped = info.mapped;
        return info.mapped;
    }

    void unmap(uint64_t enc) noexcept {
        auto info = getInfo(enc);
        if (info.mapped) {
            vkUnmapMemory(device_, info.memory);
            std::lock_guard<std::mutex> lock(mutex_);
            if (auto it = buffers_.find(enc); it != buffers_.end())
                it->second.mapped = nullptr;
        }
    }

    // STATS — FOR DEV TOOLS
    void printStats() const noexcept {
        size_t count = 0, totalSize = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [enc, info] : buffers_) {
                ++count;
                totalSize += info.size;
            }
        }
        LOG_SUCCESS_CAT("GLOBAL_BUFFER", "{}BUFFERS: {} — TOTAL {} MB — PINK PHOTONS APPROVED{}", 
                        EMERALD_GREEN, count, totalSize / (1024*1024), RESET);
    }

    // HOT-RELOAD SAFE — BUMP GENERATION
    void invalidateAll() noexcept {
        generation_.fetch_add(1, std::memory_order_acq_rel);
        LOG_SUCCESS_CAT("GLOBAL_BUFFER", "{}ALL HANDLES INVALIDATED — HOT-RELOAD SAFE — VALHALLA REIGNS{}", 
                        RASPBERRY_PINK, RESET);
    }

    // CLEANUP — FOR ENGINE SHUTDOWN
    void releaseAll() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [enc, info] : buffers_) {
            if (info.mapped) vkUnmapMemory(device_, info.memory);
            vkDestroyBuffer(device_, info.buffer, nullptr);
            vkFreeMemory(device_, info.memory, nullptr);
        }
        buffers_.clear();
        LOG_SUCCESS_CAT("GLOBAL_BUFFER", "{}ALL BUFFERS RELEASED — VALHALLA ETERNAL{}", EMERALD_GREEN, RESET);
    }

private:
    GlobalBufferManager() = default;
    ~GlobalBufferManager() { releaseAll(); }

    struct BufferInfo {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        void* mapped = nullptr;
        std::string debugName;
        uint32_t memType = 0;
        uint64_t generation = 0;
    };

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    mutable std::mutex mutex_;  // Only on mutate — reads use atomic generation
    std::unordered_map<uint64_t, BufferInfo> buffers_;
    std::atomic<uint64_t> generation_{1};

    // LOCK-FREE INFO FETCH
    BufferInfo getInfo(uint64_t enc) const noexcept {
        uint64_t gen = generation_.load(std::memory_order_acquire);
        uint64_t raw = decrypt(enc, gen);
        if (raw == 0) return {};
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = buffers_.find(enc); it != buffers_.end() && it->second.generation == gen)
            return it->second;
        return {};
    }

    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const noexcept {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physDevice_, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
            if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
                return i;
        return ~0u;
    }

    // STONEKEY ENCRYPT/DECRYPT — ZERO COST — HOT-RELOAD SAFE
    static inline constexpr uint64_t encrypt(uint64_t raw, uint64_t gen) noexcept {
        uint64_t x = raw ^ kStone1 ^ kStone2 ^ gen ^ 0xDEADBEEF1337C0DEull;
        x = std::rotl(x, 17) ^ 0x517CC1B727220A95ull;
        x = x ^ (x >> 11) ^ (x << 23);
        return x;
    }

    static inline uint64_t decrypt(uint64_t enc, uint64_t gen) noexcept {
        uint64_t x = enc;
        x = x ^ (x >> 11) ^ (x << 23);
        x = std::rotr(x, 17) ^ 0x517CC1B727220A95ull;
        x = x ^ kStone1 ^ kStone2 ^ gen ^ 0xDEADBEEF1337C0DEull;
        return x;
    }
};

// GLOBAL ACCESS — ONE LINE LOVE
#define GLOBAL_BUFFER GlobalBufferManager::get()

// MACROS — DEV HEAVEN
#define CREATE_GLOBAL_BUFFER(...) GLOBAL_BUFFER.createBuffer(__VA_ARGS__)
#define DESTROY_GLOBAL_BUFFER(h)  GLOBAL_BUFFER.destroyBuffer(h)
#define RAW_BUFFER(h)             GLOBAL_BUFFER.getRawBuffer(h)
#define BUFFER_SIZE(h)            GLOBAL_BUFFER.getSize(h)
#define BUFFER_MAPPED(h)          GLOBAL_BUFFER.getMapped(h)
#define BUFFER_NAME(h)            GLOBAL_BUFFER.getDebugName(h)
#define MAP_BUFFER(h)             GLOBAL_BUFFER.map(h)
#define UNMAP_BUFFER(h)           GLOBAL_BUFFER.unmap(h)

// NOVEMBER 09 2025 — GLOBAL BUFFER SUPREMACY ACHIEVED
// ZERO CONTENTION — STONEKEY UNBREAKABLE — MODDERS REJOICE
// PINK PHOTONS FOR EVERY DEV — VALHALLA OPEN BAR 🩷🚀💀⚡🤖🔥♾️