// VulkanBufferManager.hpp
#pragma once
#include "StoneKey.hpp"
#include <vulkan/vulkan.h>
#include <vector>

class VulkanBufferManager {
public:
    VulkanBufferManager() = default;
    ~VulkanBufferManager() { cleanup(); }

    void init(VkDevice device);
    void cleanup();

    // Encrypted handles — Reclass sees garbage
    uint64_t createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void destroyBuffer(uint64_t enc_handle);

private:
    VkDevice device_ = VK_NULL_HANDLE;

    struct BufferInfo {
        uint64_t buffer_enc_ = 0;
        uint64_t memory_enc_ = 0;
        VkDeviceSize size_ = 0;
    };
    std::vector<BufferInfo> buffers_;

    // ZERO COST — ONE XOR — ASSEMBLY: xor rax, imm64; xor rax, imm64
    template<typename T>
    [[nodiscard]] static inline constexpr uint64_t encrypt(T raw) noexcept {
        return static_cast<uint64_t>(raw) ^ kStone1 ^ kStone2;
    }

    template<typename T>
    [[nodiscard]] static inline constexpr T decrypt(uint64_t enc) noexcept {
        return static_cast<T>(enc ^ kStone1 ^ kStone2);
    }
};