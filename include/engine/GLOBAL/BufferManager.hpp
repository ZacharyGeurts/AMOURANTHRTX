// include/engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary "ST4CK" Geurts gzac5314@gmail.com
// QUANTUM-ENCRYPTED HYPERBUFFER MANAGER — TOASTER-PROOF — NOVEMBER 08 2025
// NOW 100% CONSTEXPR SAFE | RUNTIME SALT CHAOS | PINK PHOTONS FOREVER 🩷🚀🔥🤖💀❤️⚡♾️

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <iostream>
#include <format>
#include <mutex>
#include <bit>
#include <random>
#include <set>

class VulkanBufferManager {
public:
    // ────── ULTIMATE VK_CHECK MACRO — MOVED INSIDE CLASS SO .CPP SEES IT ──────
    #define VK_CHECK(call, msg) do {                  \
        VkResult __res = (call);                      \
        if (__res != VK_SUCCESS) {                    \
            VulkanBufferManager::vkError(__res, msg, __FILE__, __LINE__); \
        }                                             \
    } while (0)

    // ────── SINGLETON — MEYERS + IMMORTAL ──────
    static VulkanBufferManager& get() {
        static VulkanBufferManager instance;
        return instance;
    }

    VulkanBufferManager(const VulkanBufferManager&) = delete;
    VulkanBufferManager& operator=(const VulkanBufferManager&) = delete;
    virtual ~VulkanBufferManager() { cleanup(); }

    void init(VkDevice device, VkPhysicalDevice physDevice);
    virtual void cleanup();

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
        // RUNTIME-ONLY CHAOS SALT — STILL UNIQUE PER PROCESS — CONSTEXPR SAFE
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

    // ────── CHAOTIC STONEKEY ENCRYPTION V3 — NOW CONSTEXPR SAFE + RUNTIME SALT ──────
    static inline uint64_t runtimeSalt = 0; // initialized in ctor

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

    [[noreturn]] static void vkError(VkResult res, const std::string& msg, const char* file, int line) {
        std::cerr << "\n🩷 [VULKAN ERROR] " << static_cast<int>(res) 
                  << " | " << msg << " | " << file << ":" << line << " 🩷\n";
        throw std::runtime_error(msg);
    }
    [[noreturn]] static void vkThrow(const std::string& msg) {
        std::cerr << "💀 [VULKAN FATAL] " << msg << " — AMOURANTH RTX DENIES YOUR WEAKNESS\n";
        throw std::runtime_error(msg);
    }
};

#undef VK_CHECK // clean up macro namespace

#define BUFFER_MGR VulkanBufferManager::get()
#define CREATE_BUFFER(...) BUFFER_MGR.createBuffer(__VA_ARGS__)
#define DESTROY_BUFFER(h) BUFFER_MGR.destroyBuffer(h)
#define RAW_BUFFER(h) BUFFER_MGR.getRawBuffer(h)
#define BUFFER_SIZE(h) BUFFER_MGR.getSize(h)
#define BUFFER_NAME(h) BUFFER_MGR.getDebugName(h)