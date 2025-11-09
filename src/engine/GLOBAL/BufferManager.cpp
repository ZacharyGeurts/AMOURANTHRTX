// src/engine/GLOBAL/BufferManager.cpp
// AMOURANTH RTX Engine – November 08 2025 – Vulkan Buffer Manager Implementation
// Zero-cost pooling | Terminal low-level output | C++23 | Console-portable (Vulkan 1.3+)

#include "engine/GLOBAL/BufferManager.hpp"
#include <algorithm>  // For std::find_if, std::sort (infrequent)
#include <sstream>    // For low-level hex formatting
#include <iomanip>    // For std::hex, setw
#include <iostream>   // For std::cout/std::cerr terminal output
#include <chrono>     // C++23 for high-res timestamps
#include <format>     // C++23 std::format for zero-cost string building (if avail; fallback ostringstream)

#ifdef __cpp_lib_format  // C++23 std::format
using std::format;
#define FMT_STR(fmt, ...) format(fmt, __VA_ARGS__)
#else
#define FMT_STR(fmt, ...) []{ std::ostringstream oss; oss << fmt; return oss.str(); }()
#endif

// Terminal timestamp helper (zero-cost if compiled out)
inline void printTimestamp() noexcept {
    auto now = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::cout << "[BufferMgr:" << ms << "ms] ";
}

// Redefine VK_CHECK for this file (direct, undef at end)
#define VK_CHECK(call, msg) do {                  \
    VkResult __res = (call);                      \
    if (__res != VK_SUCCESS) {                    \
        BufferManager::vkError(__res, msg, __FILE__, __LINE__); \
    }                                             \
} while (0)

void BufferManager::init(VkDevice device, VkPhysicalDevice physDevice) noexcept {
    device_ = device;
    physDevice_ = physDevice;
    printTimestamp();
    std::cout << "Init | Device:0x" << std::hex << reinterpret_cast<uintptr_t>(device)
              << " | Phys:0x" << reinterpret_cast<uintptr_t>(physDevice) << std::dec << std::endl;
}

std::expected<uint64_t, std::string> BufferManager::createBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
    std::string_view debugName) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer rawBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device_, &ci, nullptr, &rawBuffer), "Buffer create fail");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device_, rawBuffer, &reqs);

    uint32_t memType = findMemoryType(reqs.memoryTypeBits, properties);
    if (memType == static_cast<uint32_t>(-1)) {
        vkDestroyBuffer(device_, rawBuffer, nullptr);
        return std::unexpected("No mem type");
    }

    VkDeviceMemory rawMemory = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    VkDeviceSize alignedSize = (size + reqs.alignment - 1) & ~(reqs.alignment - 1);

    auto& pool = freePools_[memType];
    auto it = std::find_if(pool.begin(), pool.end(), [&](const FreeBlock& b) {
        return b.size >= alignedSize && (b.offset % reqs.alignment == 0);
    });

    if (it != pool.end()) {
        rawMemory = it->memory;
        offset = it->offset;
        VkDeviceSize remaining = it->size - alignedSize;
        it->offset += alignedSize;
        it->size = remaining;
        if (remaining == 0) pool.erase(it);
        printTimestamp();
        std::cout << "Pool alloc | Mem:0x" << std::hex << reinterpret_cast<uintptr_t>(rawMemory)
                  << " | Off:0x" << offset << " | Sz:0x" << alignedSize << " | Align:0x" << reqs.alignment
                  << std::dec << std::endl;
    } else {
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = alignedSize;
        ai.memoryTypeIndex = memType;
        VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &rawMemory), "Mem alloc fail");
        printTimestamp();
        std::cout << "Fresh alloc | Mem:0x" << std::hex << reinterpret_cast<uintptr_t>(rawMemory)
                  << " | Sz:0x" << alignedSize << std::dec << " | Type:" << memType << std::endl;
    }

    VK_CHECK(vkBindBufferMemory(device_, rawBuffer, rawMemory, offset), "Bind fail");

    uint64_t handle = encrypt(reinterpret_cast<uintptr_t>(rawBuffer));
    buffers_[handle] = {rawBuffer, rawMemory, size, reqs.alignment, offset, nullptr, 
                        std::string(debugName), memType};  // Zero-cost string_view to string

    // Low-level terminal output: Handle hex, raw ptr, usage bits
    printTimestamp();
    std::cout << "Create | H:0x" << std::hex << handle << " | Raw:0x" << reinterpret_cast<uintptr_t>(rawBuffer)
              << " | Sz:0x" << size << " | Usage:0x" << usage << " | Props:0x" << properties
              << std::dec << " | Name:" << debugName << std::endl;

    return handle;
}

void BufferManager::destroyBuffer(uint64_t enc_handle) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buffers_.find(enc_handle);
    if (it == buffers_.end()) return;

    const auto& b = it->second;
    if (b.mapped) vkUnmapMemory(device_, b.memory);

    auto& pool = freePools_[b.memType];
    pool.push_back({b.memory, b.offset, b.size});

    // Merge: O(n) scan, infrequent (destroy path)
    std::sort(pool.begin(), pool.end(), [](const FreeBlock& a, const FreeBlock& b) {
        return a.offset < b.offset;
    });
    for (auto curr = pool.begin(); curr != pool.end(); ) {
        auto next = std::next(curr);
        if (next != pool.end() && curr->memory == next->memory && 
            curr->offset + curr->size == next->offset) {
            curr->size += next->size;
            pool.erase(next);
        } else {
            ++curr;
        }
    }

    vkDestroyBuffer(device_, b.buffer, nullptr);
    DestroyTracker::markDestroyed(reinterpret_cast<const void*>(b.buffer));  // Static call
    ++g_destructionCounter;

    // Low-level terminal output: Pre-destruction state
    printTimestamp();
    std::cout << "Destroy | H:0x" << std::hex << enc_handle << " | Raw:0x" << reinterpret_cast<uintptr_t>(b.buffer)
              << " | Mem:0x" << reinterpret_cast<uintptr_t>(b.memory) << " | Off:0x" << b.offset
              << " | Sz:0x" << b.size << std::dec << " | Name:" << b.debugName << std::endl;

    buffers_.erase(it);
}

VkBuffer BufferManager::getRawBuffer(uint64_t enc_handle) const noexcept {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? it->second.buffer : VK_NULL_HANDLE;
}

VkDeviceSize BufferManager::getSize(uint64_t enc_handle) const noexcept {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? it->second.size : 0;
}

VkDeviceMemory BufferManager::getMemory(uint64_t enc_handle) const noexcept {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? it->second.memory : VK_NULL_HANDLE;
}

void* BufferManager::getMapped(uint64_t enc_handle) const noexcept {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? it->second.mapped : nullptr;
}

std::string BufferManager::getDebugName(uint64_t enc_handle) const noexcept {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? std::move(it->second.debugName) : std::string{};  // Zero-cost move
}

bool BufferManager::isValid(uint64_t enc_handle) const noexcept {
    return buffers_.contains(enc_handle);  // C++20 contains, zero-cost
}

void BufferManager::setDebugName(uint64_t enc_handle, std::string_view name) noexcept {
    auto it = buffers_.find(enc_handle);
    if (it != buffers_.end()) {
        it->second.debugName.assign(name);  // Efficient assign
    }
}

void* BufferManager::map(uint64_t enc_handle) noexcept {
    auto it = buffers_.find(enc_handle);
    if (it == buffers_.end() || it->second.mapped) return nullptr;
    void* data = nullptr;
    vkMapMemory(device_, it->second.memory, it->second.offset, it->second.size, 0, &data);
    if (data) {
        it->second.mapped = data;
        printTimestamp();
        std::cout << "Map | H:0x" << std::hex << enc_handle << " | Ptr:0x" << reinterpret_cast<uintptr_t>(data)
                  << " | Sz:0x" << it->second.size << std::dec << std::endl;
    }
    return data;
}

void BufferManager::unmap(uint64_t enc_handle) noexcept {
    auto it = buffers_.find(enc_handle);
    if (it != buffers_.end() && it->second.mapped) {
        uintptr_t ptr = reinterpret_cast<uintptr_t>(it->second.mapped);
        vkUnmapMemory(device_, it->second.memory);
        printTimestamp();
        std::cout << "Unmap | H:0x" << std::hex << enc_handle << " | Ptr:0x" << ptr << std::dec << std::endl;
        it->second.mapped = nullptr;
    }
}

void BufferManager::printStats() const noexcept {
    size_t totalBuffers = buffers_.size();
    VkDeviceSize totalAllocated = 0;
    for (const auto& [h, info] : buffers_) totalAllocated += info.size;
    size_t totalFreeBlocks = 0;
    for (const auto& p : freePools_) totalFreeBlocks += p.second.size();
    printTimestamp();
    std::cout << "Stats | Buffs:" << totalBuffers << " | Alloc:0x" << std::hex << totalAllocated
              << std::dec << "B | FreeBlocks:" << totalFreeBlocks << std::endl;
}

void BufferManager::releaseAll(VkDevice device) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    std::unordered_set<VkDeviceMemory, std::hash<VkDeviceMemory>, std::equal_to<VkDeviceMemory>> memories;  // C++23 unordered_set for speed

    for (auto& [handle, info] : buffers_) {
        if (info.mapped) vkUnmapMemory(device, info.memory);
        if (info.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, info.buffer, nullptr);
            DestroyTracker::markDestroyed(reinterpret_cast<const void*>(info.buffer));  // Static call
            ++g_destructionCounter;
        }
        if (info.memory != VK_NULL_HANDLE) memories.insert(info.memory);
        printTimestamp();
        std::cout << "Release | H:0x" << std::hex << handle << " | Raw:0x" << reinterpret_cast<uintptr_t>(info.buffer)
                  << " | Mem:0x" << reinterpret_cast<uintptr_t>(info.memory) << std::dec << std::endl;
    }
    buffers_.clear();  // Zero-cost clear

    for (auto& pool : freePools_) {
        for (const auto& block : pool.second) {
            memories.insert(block.memory);
        }
        pool.second.clear();
    }
    freePools_.clear();

    for (VkDeviceMemory mem : memories) {
        if (mem != VK_NULL_HANDLE) vkFreeMemory(device, mem, nullptr);
    }

    printTimestamp();
    std::cout << "ReleaseAll | Destroyed:" << g_destructionCounter << " | Memories freed:" << memories.size() << std::endl;
}

uint32_t BufferManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDevice_, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    vkThrow("No mem type");
    return static_cast<uint32_t>(-1);  // Unreachable
}

[[noreturn]] void BufferManager::vkError(VkResult res, std::string_view msg, const char* file, int line) {
    printTimestamp();
    std::cerr << "VK:" << static_cast<int>(res) << " | " << msg << " | " << file << ':' << line << std::endl;
    std::terminate();  // Zero-cost fatal
}

[[noreturn]] void BufferManager::vkThrow(std::string_view msg) {
    printTimestamp();
    std::cerr << "Throw: " << msg << std::endl;
    std::terminate();
}

#undef VK_CHECK