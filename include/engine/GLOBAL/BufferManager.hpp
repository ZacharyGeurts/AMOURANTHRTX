// include/engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// GLOBAL STONEKEYED BUFFER MANAGER — DEVELOPER CONFIGURABLE — NOVEMBER 08 2025
// LOW-LEVEL STDERR / CLOG + THROW | ZERO-OVERHEAD | THREAD-SAFE

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <cstdint>
#include <iostream>
#include <format>

class VulkanBufferManager {
public:
    VulkanBufferManager() = default;
    virtual ~VulkanBufferManager() { cleanup(); }

    void init(VkDevice device, VkPhysicalDevice physDevice);
    virtual void cleanup();

    uint64_t createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    virtual void destroyBuffer(uint64_t enc_handle);

    [[nodiscard]] virtual VkBuffer getRawBuffer(uint64_t enc_handle) const;

protected:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;

    struct BufferInfo {
        uint64_t buffer_enc_ = 0;
        uint64_t memory_enc_ = 0;
        VkDeviceSize size_ = 0;
    };

    std::unordered_map<uint64_t, BufferInfo> buffers_;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // ────── STONEKEY ENCRYPTION (constexpr zero-cost) ──────
    static inline constexpr uint64_t encrypt(uintptr_t raw) noexcept {
        return raw ^ kStone1 ^ kStone2;
    }
    static inline constexpr uintptr_t decrypt(uint64_t enc) noexcept {
        return enc ^ kStone1 ^ kStone2;
    }

    // ────── LOW-LEVEL ERROR SYSTEM ──────
    [[noreturn]] static void vkError(VkResult res, const std::string& msg) {
        std::cerr << "[VULKAN ERROR] " << static_cast<int>(res) << " — " << msg << std::endl;
        throw std::runtime_error(msg);
    }
    [[noreturn]] static void vkThrow(const std::string& msg) {
        std::cerr << "[VULKAN FATAL] " << msg << std::endl;
        throw std::runtime_error(msg);
    }
};