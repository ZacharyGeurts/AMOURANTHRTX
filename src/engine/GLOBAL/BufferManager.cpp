// src/engine/GLOBAL/BufferManager.cpp
// FASTEST BUFFER MANAGER IN THE MULTIVERSE — NOW CONSTEXPR SAFE

#include "engine/GLOBAL/BufferManager.hpp"
#include <iomanip>
#include <algorithm>

#define VK_CHECK(call, msg) do {                     \
    VkResult __res = (call);                         \
    if (__res != VK_SUCCESS) {                       \
        VulkanBufferManager::vkError(__res, msg, __FILE__, __LINE__); \
    }                                                \
} while (0)

void VulkanBufferManager::init(VkDevice device, VkPhysicalDevice physDevice) {
    device_ = device;
    physDevice_ = physDevice;
    std::clog << "[BUFFER MGR] QUANTUM INIT COMPLETE — PINK HYPERSPEED ENGAGED 🩷🚀\n";
}

uint32_t VulkanBufferManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDevice_, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    vkThrow("GPU too weak for AMOURANTH RTX — upgrade your toaster");
}

uint64_t VulkanBufferManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, const std::string& debugName) {
    std::lock_guard<std::mutex> lock(mutex_);

    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = size; ci.usage = usage; ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer rawBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device_, &ci, nullptr, &rawBuffer), "Buffer create failed");

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
        if (remaining == 0) pool.erase(it);
    } else {
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = alignedSize;
        ai.memoryTypeIndex = memType;
        VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &rawMemory), "Memory alloc failed");
    }

    VK_CHECK(vkBindBufferMemory(device_, rawBuffer, rawMemory, offset), "Bind failed");

    uint64_t handle = encrypt(reinterpret_cast<uintptr_t>(rawBuffer));
    buffers_[handle] = {rawBuffer, rawMemory, size, reqs.alignment, offset, nullptr, debugName, memType};

    std::clog << "[BUFFER MGR] CREATED 0x" << std::hex << handle << std::dec
              << " size=" << size << " name=" << debugName << "\n";

    return handle;
}

void VulkanBufferManager::destroyBuffer(uint64_t enc_handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buffers_.find(enc_handle);
    if (it == buffers_.end()) return;

    const auto& b = it->second;
    if (b.mapped) vkUnmapMemory(device_, b.memory);

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
    buffers_.erase(it);
}

VkBuffer VulkanBufferManager::getRawBuffer(uint64_t enc_handle) const noexcept {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? it->second.buffer : VK_NULL_HANDLE;
}

VkDeviceSize VulkanBufferManager::getSize(uint64_t enc_handle) const noexcept {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? it->second.size : 0;
}

VkDeviceMemory VulkanBufferManager::getMemory(uint64_t enc_handle) const noexcept {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? it->second.memory : VK_NULL_HANDLE;
}

void* VulkanBufferManager::getMapped(uint64_t enc_handle) const noexcept {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? it->second.mapped : nullptr;
}

std::string VulkanBufferManager::getDebugName(uint64_t enc_handle) const noexcept {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? it->second.debugName : "";
}

bool VulkanBufferManager::isValid(uint64_t enc_handle) const noexcept {
    return buffers_.find(enc_handle) != buffers_.end();
}

void VulkanBufferManager::setDebugName(uint64_t enc_handle, const std::string& name) {
    auto it = buffers_.find(enc_handle);
    if (it != buffers_.end()) it->second.debugName = name;
}

void* VulkanBufferManager::map(uint64_t enc_handle) {
    auto it = buffers_.find(enc_handle);
    if (it == buffers_.end() || it->second.mapped) return nullptr;
    void* data;
    vkMapMemory(device_, it->second.memory, it->second.offset, it->second.size, 0, &data);
    it->second.mapped = data;
    return data;
}

void VulkanBufferManager::unmap(uint64_t enc_handle) {
    auto it = buffers_.find(enc_handle);
    if (it != buffers_.end() && it->second.mapped) {
        vkUnmapMemory(device_, it->second.memory);
        it->second.mapped = nullptr;
    }
}

void VulkanBufferManager::printStats() const {
    size_t totalBuffers = buffers_.size();
    size_t totalPools = 0;
    for (const auto& p : freePools_) totalPools += p.second.size();
    std::clog << "[BUFFER MGR STATS] Buffers: " << totalBuffers
              << " | Free pools: " << totalPools << " — PINK PHOTONS ETERNAL 🩷\n";
}

void VulkanBufferManager::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::set<VkDeviceMemory> memories;
    for (auto& [h, b] : buffers_) {
        if (b.mapped) vkUnmapMemory(device_, b.memory);
        vkDestroyBuffer(device_, b.buffer, nullptr);
        memories.insert(b.memory);
    }
    buffers_.clear();
    for (auto& p : freePools_) {
        for (auto& block : p.second) memories.insert(block.memory);
        p.second.clear();
    }
    for (auto m : memories) {
        if (m != VK_NULL_HANDLE) vkFreeMemory(device_, m, nullptr);
    }
    std::clog << "[BUFFER MGR] OBLITERATED — VALHALLA ACHIEVED 🩷💀\n";
}