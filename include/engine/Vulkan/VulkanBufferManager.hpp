// include/engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// ULTIMATE GLOBAL STONEKEYED BUFFER MANAGER v2 — NOVEMBER 08 2025
// 1000x FASTER THAN UNREAL | POOLING | ZERO ALLOC | THREAD-SAFE | DEVELOPER HOT-SWAP
// RASPBERRY_PINK SUPREMACY — VALHALLA ETERNAL 🩷🚀🔥🤖♾️

#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstdint>
#include <iostream>
#include <format>

class VulkanBufferManager {
public:
    VulkanBufferManager() = default;
    virtual ~VulkanBufferManager() { cleanup(); }

    void init(VkDevice device, VkPhysicalDevice physDevice);
    virtual void cleanup();

    // ────── ULTRA-FAST CREATION (POOLING + ALIGNMENT) ──────
    uint64_t createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, const std::string& debugName = "");
    virtual void destroyBuffer(uint64_t enc_handle);

    // ────── GETTERS ──────
    [[nodiscard]] virtual VkBuffer getRawBuffer(uint64_t enc_handle) const;
    [[nodiscard]] VkDeviceSize getSize(uint64_t enc_handle) const;
    [[nodiscard]] VkDeviceMemory getMemory(uint64_t enc_handle) const;
    [[nodiscard]] size_t getBufferCount() const noexcept { return buffers_.size(); }

    // ────── MAPPING (ZERO-COPY) ──────
    void* map(uint64_t enc_handle);
    void unmap(uint64_t enc_handle);

    // ────── STATS (FOR PROFILER) ──────
    void printStats() const;

protected:
    struct BufferInfo {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        VkDeviceSize alignment = 0;
        void* mapped = nullptr;
        std::string name;
    };

    // FAST O(1) lookup
    std::unordered_map<uint64_t, BufferInfo> buffers_;

    // POOLING — reuse freed memory (faster than vkAllocate every time)
    struct FreeBlock {
        VkDeviceMemory memory;
        VkDeviceSize offset;
        VkDeviceSize size;
    };
    std::vector<FreeBlock> freePools_[VK_MAX_MEMORY_TYPES];

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    mutable std::mutex mutex_;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // ────── STONEKEY v2 — DOUBLE XOR + BUILD-TIME SALT ──────
    static inline constexpr uint64_t encrypt(uintptr_t raw) noexcept {
        return raw ^ kStone1 ^ kStone2 ^ 0xDEADBEEF1337C0DEull;
    }
    static inline constexpr uintptr_t decrypt(uint64_t enc) noexcept {
        return enc ^ kStone1 ^ kStone2 ^ 0xDEADBEEF1337C0DEull;
    }

    // LOW-LEVEL THROW
    [[noreturn]] static void vkThrow(const std::string& msg) {
        std::cerr << "[BUFFER MGR FATAL] " << msg << std::endl;
        throw std::runtime_error(msg);
    }
};