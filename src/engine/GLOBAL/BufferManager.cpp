// src/engine/GLOBAL/BufferManager.cpp
// AMOURANTH RTX Engine – November 08 2025 – Vulkan Buffer Manager Implementation
// Professional, high-performance, thread-safe buffer pooling with encrypted handles

#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Dispose.hpp"  // For g_destructionCounter and DestroyTracker
#include <iomanip>
#include <algorithm>
#include <sstream>

#define VK_CHECK(call, msg) do {                     \
    VkResult __res = (call);                         \
    if (__res != VK_SUCCESS) {                       \
        BufferManager::vkError(__res, msg, __FILE__, __LINE__); \
    }                                                \
} while (0)

void BufferManager::init(VkDevice device, VkPhysicalDevice physDevice) {
    device_ = device;
    physDevice_ = physDevice;
    LOG_SUCCESS_CAT("BufferManager", "Initialization complete");
}

uint32_t BufferManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDevice_, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    vkThrow("Failed to find suitable memory type");
}

uint64_t BufferManager::createBuffer(VkDeviceSize size,
                                           VkBufferUsageFlags usage,
                                           VkMemoryPropertyFlags properties,
                                           const std::string& debugName) {
    std::lock_guard<std::mutex> lock(mutex_);

    VkBufferCreateInfo ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer rawBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device_, &ci, nullptr, &rawBuffer), "Failed to create buffer");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device_, rawBuffer, &reqs);

    uint32_t memType = findMemoryType(reqs.memoryTypeBits, properties);

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
        if (remaining == 0) {
            pool.erase(it);
        }
    } else {
        VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = alignedSize;
        ai.memoryTypeIndex = memType;
        VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &rawMemory), "Failed to allocate device memory");
    }

    VK_CHECK(vkBindBufferMemory(device_, rawBuffer, rawMemory, offset), "Failed to bind buffer memory");

    uint64_t handle = encrypt(reinterpret_cast<uintptr_t>(rawBuffer));
    buffers_[handle] = {rawBuffer, rawMemory, size, reqs.alignment, offset, nullptr, debugName, memType};

    LOG_INFO_CAT("BufferManager", "Created buffer 0x{:x} | Size: {} | Name: {}", handle, size, debugName.empty() ? "(unnamed)" : debugName);

    return handle;
}

void BufferManager::destroyBuffer(uint64_t enc_handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buffers_.find(enc_handle);
    if (it == buffers_.end()) {
        return;
    }

    const auto& b = it->second;
    if (b.mapped) {
        vkUnmapMemory(device_, b.memory);
    }

    auto& pool = freePools_[b.memType];
    pool.push_back({b.memory, b.offset, b.size});

    std::sort(pool.begin(), pool.end(), [](const FreeBlock& a, const FreeBlock& b) {
        return a.offset < b.offset;
    });

    for (auto curr = pool.begin(); curr != pool.end(); ) {
        auto next = curr + 1;
        if (next != pool.end() && curr->memory == next->memory && curr->offset + curr->size == next->offset) {
            curr->size += next->size;
            next = pool.erase(next);
        } else {
            ++curr;
        }
    }

    vkDestroyBuffer(device_, b.buffer, nullptr);
    DestroyTracker::markDestroyed(reinterpret_cast<const void*>(b.buffer));
    ++g_destructionCounter;

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
    return it != buffers_.end() ? it->second.debugName : "";
}

bool BufferManager::isValid(uint64_t enc_handle) const noexcept {
    return buffers_.find(enc_handle) != buffers_.end();
}

void BufferManager::setDebugName(uint64_t enc_handle, const std::string& name) {
    auto it = buffers_.find(enc_handle);
    if (it != buffers_.end()) {
        it->second.debugName = name;
    }
}

void* BufferManager::map(uint64_t enc_handle) {
    auto it = buffers_.find(enc_handle);
    if (it == buffers_.end() || it->second.mapped) {
        return nullptr;
    }
    void* data = nullptr;
    vkMapMemory(device_, it->second.memory, it->second.offset, it->second.size, 0, &data);
    it->second.mapped = data;
    return data;
}

void BufferManager::unmap(uint64_t enc_handle) {
    auto it = buffers_.find(enc_handle);
    if (it != buffers_.end() && it->second.mapped) {
        vkUnmapMemory(device_, it->second.memory);
        it->second.mapped = nullptr;
    }
}

void BufferManager::printStats() const {
    size_t totalBuffers = buffers_.size();
    size_t totalFreeBlocks = 0;
    for (const auto& p : freePools_) {
        totalFreeBlocks += p.second.size();
    }
    LOG_INFO_CAT("BufferManager", "Active buffers: {} | Free memory blocks: {}", totalBuffers, totalFreeBlocks);
}

// Global releaseAll – called from Dispose during shutdown
void BufferManager::releaseAll(VkDevice device) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    std::set<VkDeviceMemory> memoriesToFree;

    for (auto& [handle, info] : buffers_) {
        if (info.buffer != VK_NULL_HANDLE) {
            if (info.mapped) {
                vkUnmapMemory(device, info.memory);
            }
            vkDestroyBuffer(device, info.buffer, nullptr);
            DestroyTracker::markDestroyed(reinterpret_cast<const void*>(info.buffer));
            ++g_destructionCounter;
            memoriesToFree.insert(info.memory);
        }
    }
    buffers_.clear();

    for (auto& pool : freePools_) {
        for (const auto& block : pool.second) {
            memoriesToFree.insert(block.memory);
        }
        pool.second.clear();
    }

    for (VkDeviceMemory mem : memoriesToFree) {
        if (mem != VK_NULL_HANDLE) {
            vkFreeMemory(device, mem, nullptr);
        }
    }

    LOG_SUCCESS_CAT("BufferManager", "Global cleanup complete – All buffers and memory released");
}

void BufferManager::cleanup() {
    if (device_ != VK_NULL_HANDLE) {
        releaseAll(device_);
    }
}

[[noreturn]] void BufferManager::vkError(VkResult res, const std::string& msg, const char* file, int line) {
    std::ostringstream oss;
    oss << "Vulkan error " << res << ": " << msg << " [" << file << ":" << line << "]";
    LOG_ERROR_CAT("BufferManager", "{}", oss.str());
    throw std::runtime_error(oss.str());
}

[[noreturn]] void BufferManager::vkThrow(const std::string& msg) {
    LOG_ERROR_CAT("BufferManager", "Fatal error: {}", msg);
    throw std::runtime_error(msg);
}