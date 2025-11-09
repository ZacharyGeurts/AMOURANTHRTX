// include/engine/GLOBAL/BufferManager.hpp
// AMOURANTH RTX Engine – November 08 2025 – Vulkan Buffer Manager
// Professional, high-performance, thread-safe buffer pooling with encrypted handles

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Dispose.hpp"  // For g_destructionCounter and DestroyTracker
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>
#include <mutex>
#include <random>
#include <set>

class VulkanBufferManager {
public:
    // VK_CHECK macro – visible to .cpp
    #define VK_CHECK(call, msg) do {                  \
        VkResult __res = (call);                      \
        if (__res != VK_SUCCESS) {                    \
            VulkanBufferManager::vkError(__res, msg, __FILE__, __LINE__); \
        }                                             \
    } while (0)

    static VulkanBufferManager& get() {
        static VulkanBufferManager instance;
        return instance;
    }

    VulkanBufferManager(const VulkanBufferManager&) = delete;
    VulkanBufferManager& operator=(const VulkanBufferManager&) = delete;
    virtual ~VulkanBufferManager() { cleanup(); }

    void init(VkDevice device, VkPhysicalDevice physDevice);
    virtual void cleanup();

    // Global releaseAll used by Dispose
    void releaseAll(VkDevice device) noexcept;

    uint64_t createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, const std::string& debugName = "");
    virtual void destroyBuffer(uint64_t enc_handle);

    [[nodiscard]] VkBuffer       getRawBuffer(uint64_t enc_handle) const noexcept;
    [[nodiscard]] VkDeviceSize   getSize(uint64_t enc_handle) const noexcept;
    [[nodiscard]] VkDeviceMemory getMemory(uint64_t enc_handle) const noexcept;
    [[nodiscard]] void*          getMapped(uint64_t enc_handle) const noexcept;
    [[nodiscard]] std::string    getDebugName(uint64_t enc_handle) const noexcept;
    [[nodiscard]] bool           isValid(uint64_t enc_handle) const noexcept;

    void* map(uint64_t enc_handle);
    void unmap(uint64_t enc_handle);

    void printStats() const;
    void setDebugName(uint64_t enc_handle, const std::string& name);

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

private:
    VulkanBufferManager() {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dis;
        runtimeSalt = 0xCAFEBABEDEADFA11ull ^ dis(gen) ^ __builtin_ia32_rdtsc();
    }

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    std::mutex mutex_;

    std::unordered_map<uint64_t, BufferInfo> buffers_;
    std::unordered_map<uint32_t, std::vector<FreeBlock>> freePools_;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    static inline uint64_t runtimeSalt = 0;

    static inline constexpr uint64_t encrypt(uintptr_t raw) noexcept {
        uint64_t x = raw ^ kStone1 ^ kStone2 ^ 0xDEADBEEF1337C0DEull ^ runtimeSalt;
        x = std::rotl(x, 13) ^ 0x517CC1B727220A95ull;
        return x ^ (x >> 7) ^ (x << 25);
    }
    static inline constexpr uintptr_t decrypt(uint64_t enc) noexcept {
        uint64_t x = enc;
        x = x ^ (x >> 7) ^ (x << 25);
        x = std::rotr(x, 13) ^ 0x517CC1B727220A95ull;
        return x ^ kStone1 ^ kStone2 ^ 0xDEADBEEF1337C0DEull ^ runtimeSalt;
    }

    [[noreturn]] static void vkError(VkResult res, const std::string& msg, const char* file, int line);
    [[noreturn]] static void vkThrow(const std::string& msg);
};

#undef VK_CHECK

#define BUFFER_MGR VulkanBufferManager::get()
#define CREATE_BUFFER(...) BUFFER_MGR.createBuffer(__VA_ARGS__)
#define DESTROY_BUFFER(h) BUFFER_MGR.destroyBuffer(h)
#define RAW_BUFFER(h) BUFFER_MGR.getRawBuffer(h)
#define BUFFER_SIZE(h) BUFFER_MGR.getSize(h)
#define BUFFER_NAME(h) BUFFER_MGR.getDebugName(h)