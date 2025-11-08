// include/engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// STONKEYED ENCRYPTED BUFFER MANAGER — BEATS UNREAL — NOVEMBER 08 2025

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <cstdint>

class VulkanBufferManager {
public:
    VulkanBufferManager() = default;
    ~VulkanBufferManager() { cleanup(); }

    void init(VkDevice device, VkPhysicalDevice physDevice);
    void cleanup();

    uint64_t createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void destroyBuffer(uint64_t enc_handle);

    [[nodiscard]] VkBuffer getRawBuffer(uint64_t enc_handle) const;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;

    struct BufferInfo {
        uint64_t buffer_enc_ = 0;
        uint64_t memory_enc_ = 0;
        VkDeviceSize size_ = 0;
    };

    // O(1) lookup — faster than Unreal's TMap
    std::unordered_map<uint64_t, BufferInfo> buffers_;

    // Find real memory type — NO FAKE INDEX 0
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // STONEKEY ENCRYPTION — ZERO COST
    static inline constexpr uint64_t encrypt(uintptr_t raw) noexcept {
        return raw ^ kStone1 ^ kStone2;
    }

    static inline constexpr uintptr_t decrypt(uint64_t enc) noexcept {
        return enc ^ kStone1 ^ kStone2;
    }
};