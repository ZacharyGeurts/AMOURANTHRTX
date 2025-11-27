// include/engine/GLOBAL/DynamicStone.hpp
// NOVEMBER 27, 2025 — FINAL FIXED — LAZY SINGLETON — NO STARTUP CRASH
// PINK PHOTONS ETERNAL — THE EMPIRE IS INFINITE AND SAFE

#pragma once

#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"   // for stone_physical()
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace RTX {

class DynamicStone {
public:
    static constexpr VkDeviceSize STONE_SIZE        = 4ULL * 1024 * 1024 * 1024;  // 4 GiB
    static constexpr uint32_t     MAX_REASONABLE_STONES = 32;

    struct Block {
        uint64_t     bufferHandle = 0;
        VkDeviceSize offset       = 0;
        VkDeviceSize size         = 0;
        uint32_t     stoneIndex   = ~0u;
    };

    // THE ONE AND ONLY WAY TO ACCESS IT — LAZY + SAFE
    static DynamicStone& get() {
        static DynamicStone instance;
        return instance;
    }

    // Delete copy/assignment
    DynamicStone(const DynamicStone&) = delete;
    DynamicStone& operator=(const DynamicStone&) = delete;

    Block allocate(VkDeviceSize size, VkBufferUsageFlags extraUsage = 0, const char* debugName = "") {
        if (size == 0) return {};
        if (size > STONE_SIZE) [[unlikely]] {
            LOG_FATAL("DynamicStone: Single allocation {} > 4 GiB — split your mesh!", size);
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // Reuse freed blocks
        for (auto it = freeList_.begin(); it != freeList_.end(); ++it) {
            if (it->size >= size) {
                Block block = *it;
                if (it->size > size + 64*1024) {
                    it->offset += size;
                    it->size   -= size;
                } else {
                    freeList_.erase(it);
                }
                return block;
            }
        }

        // Find stone with space
        for (uint32_t i = 0; i < stones_.size(); ++i) {
            if (stones_[i].used + size <= STONE_SIZE) {
                return allocate_in_stone(i, size, extraUsage, debugName);
            }
        }

        // Need new stone — check VRAM limit
        const VkDeviceSize neededTotal = total_allocated_ + STONE_SIZE;
        if (neededTotal > maxSafeBytes_) [[unlikely]] {
            LOG_FATAL_CAT("Stone", "VRAM LIMIT EXCEEDED — cannot forge new stone!");
        }
        if (stones_.size() >= MAX_REASONABLE_STONES) [[unlikely]] {
            LOG_FATAL("DynamicStone: Reached max stones ({})", MAX_REASONABLE_STONES);
        }

        uint32_t newIdx = static_cast<uint32_t>(stones_.size());
        stones_.emplace_back();
        auto& s = stones_.back();

        s.handle = BufferManager::create(
            STONE_SIZE,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            extraUsage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "DYNAMIC_STONE_" + std::to_string(newIdx)
        );

        s.used = 0;
        total_allocated_ += STONE_SIZE;

        LOG_SUCCESS_CAT("Stone", "FORGED STONE #{} — TOTAL: {} / {} GiB",
                        newIdx, total_allocated_ >> 30, safeHeadroomGiB_);

        return allocate_in_stone(newIdx, size, extraUsage, debugName);
    }

    void free(const Block& block) {
        if (!block.bufferHandle) return;
        std::lock_guard<std::mutex> lock(mutex_);
        freeList_.push_back(block);
    }

    VkDeviceAddress get_device_address(const Block& block) const noexcept {
        return BufferManager::get_device_address(block.bufferHandle) + block.offset;
    }

    uint64_t total_allocated_bytes() const noexcept { return total_allocated_; }
    uint64_t total_device_memory_gib() const noexcept { return totalDeviceMemoryGiB_; }
    uint64_t safe_headroom_gib() const noexcept { return safeHeadroomGiB_; }
    uint32_t stone_count() const noexcept { return static_cast<uint32_t>(stones_.size()); }

private:
    DynamicStone() {
        query_vram_limits();
        LOG_SUCCESS_CAT("Stone", "DynamicStone ONLINE — {} GiB VRAM → Safe limit {} GiB",
                        totalDeviceMemoryGiB_, safeHeadroomGiB_);
    }

    ~DynamicStone() { release_all(); }

    void query_vram_limits() {
        VkPhysicalDevice phys = stone_physical();
        if (phys == VK_NULL_HANDLE) {
            LOG_WARNING_CAT("Stone", "Physical device not ready — using safe defaults");
            totalDeviceMemoryGiB_ = 16;
            safeHeadroomGiB_      = 14;
            maxSafeBytes_         = safeHeadroomGiB_ * 1024ULL * 1024 * 1024;
            return;
        }

        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(phys, &memProps);

        VkDeviceSize deviceLocal = 0;
        for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
            if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                deviceLocal += memProps.memoryHeaps[i].size;
            }
        }

        totalDeviceMemoryGiB_ = (deviceLocal + (1024*1024*1024-1)) >> 30;
        safeHeadroomGiB_ = totalDeviceMemoryGiB_ >= 20 ? totalDeviceMemoryGiB_ - 2 :
                          totalDeviceMemoryGiB_ >= 12 ? totalDeviceMemoryGiB_ - 1 :
                          (totalDeviceMemoryGiB_ * 9) / 10;
        maxSafeBytes_ = safeHeadroomGiB_ * 1024ULL * 1024 * 1024;
    }

    Block allocate_in_stone(uint32_t idx, VkDeviceSize size, VkBufferUsageFlags extraUsage, const char* name) {
        auto& s = stones_[idx];
        VkDeviceSize aligned = (size + 4095) & ~4095ULL;

        Block b;
        b.bufferHandle = s.handle;
        b.offset       = s.used;
        b.size         = size;
        b.stoneIndex   = idx;

        s.used += aligned;
        return b;
    }

    void release_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& s : stones_) {
            if (s.handle) BufferManager::destroy(s.handle);
        }
        stones_.clear();
        freeList_.clear();
        total_allocated_ = 0;
    }

    struct Stone { uint64_t handle = 0; VkDeviceSize used = 0; };
    std::vector<Stone> stones_;
    std::vector<Block> freeList_;
    std::mutex mutex_;
    std::atomic<uint64_t> total_allocated_{0};

    uint64_t totalDeviceMemoryGiB_ = 0;
    uint64_t safeHeadroomGiB_      = 0;
    VkDeviceSize maxSafeBytes_     = 0;
};

// CONVENIENT MACRO (optional)
#define DYNAMIC_STONE RTX::DynamicStone::get()

} // namespace RTX