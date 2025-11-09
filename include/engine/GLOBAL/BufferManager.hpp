// include/engine/GLOBAL/BufferManager.hpp
// AMOURANTH RTX Engine – November 08 2025 – Vulkan Buffer Manager
// Zero-cost, thread-safe pooling | Encrypted handles | C++23 | Console-portable

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Dispose.hpp"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>
#include <mutex>
#include <expected>  // C++23 for error handling

class BufferManager {
public:
    #define VK_CHECK(call, msg) do {                  \
        VkResult __res = (call);                      \
        if (__res != VK_SUCCESS) {                    \
            BufferManager::vkError(__res, msg, __FILE__, __LINE__); \
        }                                             \
    } while (0)

    static BufferManager& get() noexcept {  // Zero-cost Meyers singleton
        static BufferManager instance;
        return instance;
    }

    BufferManager(const BufferManager&) = delete;
    BufferManager& operator=(const BufferManager&) = delete;
    ~BufferManager() { cleanup(); }

    void init(VkDevice device, VkPhysicalDevice physDevice) noexcept;

    [[nodiscard]] std::expected<uint64_t, std::string> createBuffer(
        VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        std::string_view debugName = "") noexcept;  // C++23 string_view

    void destroyBuffer(uint64_t enc_handle) noexcept;

    [[nodiscard]] VkBuffer       getRawBuffer(uint64_t enc_handle) const noexcept;
    [[nodiscard]] VkDeviceSize   getSize(uint64_t enc_handle) const noexcept;
    [[nodiscard]] VkDeviceMemory getMemory(uint64_t enc_handle) const noexcept;
    [[nodiscard]] void*          getMapped(uint64_t enc_handle) const noexcept;
    [[nodiscard]] std::string    getDebugName(uint64_t enc_handle) const noexcept;
    [[nodiscard]] bool           isValid(uint64_t enc_handle) const noexcept;

    [[nodiscard]] void* map(uint64_t enc_handle) noexcept;
    void unmap(uint64_t enc_handle) noexcept;

    void printStats() const noexcept;
    void setDebugName(uint64_t enc_handle, std::string_view name) noexcept;

    void releaseAll(VkDevice device) noexcept;  // For Dispose

private:
    BufferManager() = default;  // Zero-cost init

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    mutable std::mutex mutex_;  // Lock only on mutate (hot-path reads lock-free if no concurrent mod)

    struct FreeBlock {
        VkDeviceMemory memory;
        VkDeviceSize offset;
        VkDeviceSize size;
    };

    struct BufferInfo {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        VkDeviceSize alignment = 0;
        VkDeviceSize offset = 0;
        void* mapped = nullptr;
        std::string debugName;
        uint32_t memType = 0;
    };

    std::unordered_map<uint64_t, BufferInfo> buffers_;
    std::unordered_map<uint32_t, std::vector<FreeBlock>> freePools_;

    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept;

    static inline constexpr uint64_t encrypt(uintptr_t raw) noexcept {  // Zero-cost constexpr
        uint64_t x = static_cast<uint64_t>(raw) ^ kStone1 ^ kStone2 ^ 0xDEADBEEF1337C0DEull;
        x = std::rotl(x, 13) ^ 0x517CC1B727220A95ull;  // C++20 rotl, zero-cost
        return x ^ (x >> 7) ^ (x << 25);
    }

    static inline constexpr uintptr_t decrypt(uint64_t enc) noexcept {
        uint64_t x = enc;
        x = x ^ (x >> 7) ^ (x << 25);
        x = std::rotr(x, 13) ^ 0x517CC1B727220A95ull;
        return static_cast<uintptr_t>(x ^ kStone1 ^ kStone2 ^ 0xDEADBEEF1337C0DEull);
    }

    void cleanup() noexcept { releaseAll(device_); }

    [[noreturn]] static void vkError(VkResult res, std::string_view msg, const char* file, int line);
    [[noreturn]] static void vkThrow(std::string_view msg);
};

#undef VK_CHECK

#define BUFFER_MGR BufferManager::get()
#define CREATE_BUFFER(...) BUFFER_MGR.createBuffer(__VA_ARGS__)
#define DESTROY_BUFFER(h) BUFFER_MGR.destroyBuffer(h)
#define RAW_BUFFER(h) BUFFER_MGR.getRawBuffer(h)
#define BUFFER_SIZE(h) BUFFER_MGR.getSize(h)
#define BUFFER_NAME(h) BUFFER_MGR.getDebugName(h)