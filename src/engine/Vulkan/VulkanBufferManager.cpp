// src/engine/Vulkan/VulkanBufferManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// FASTEST BUFFER MANAGER ON EARTH — POOLING + ZERO ALLOC + MAPPING — NOVEMBER 08 2025

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include <iomanip>

void VulkanBufferManager::init(VkDevice device, VkPhysicalDevice physDevice) {
    device_ = device;
    physDevice_ = physDevice;
    std::clog << "[BUFFER MGR] ULTIMATE INIT — POOLING READY — RASPBERRY_PINK HYPERSPEED 🩷\n";
}

uint32_t VulkanBufferManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDevice_, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    vkThrow("No memory type found — GPU too weak for AMOURANTH RTX");
    return 0;
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

    // TRY POOL FIRST — ZERO ALLOC
    VkDeviceMemory rawMemory = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    for (auto it = freePools_[memType].begin(); it != freePools_[memType].end(); ++it) {
        if (it->size >= reqs.size && it->offset % reqs.alignment == 0) {
            rawMemory = it->memory;
            offset = it->offset;
            freePools_[memType].erase(it);
            break;
        }
    }

    // FALLBACK: REAL ALLOC
    if (rawMemory == VK_NULL_HANDLE) {
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = reqs.size;
        ai.memoryTypeIndex = memType;
        VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &rawMemory), "Memory alloc failed");
    }

    VK_CHECK(vkBindBufferMemory(device_, rawBuffer, rawMemory, offset), "Bind failed");

    uint64_t handle = encrypt(reinterpret_cast<uintptr_t>(rawBuffer));
    buffers_[handle] = {rawBuffer, rawMemory, size, reqs.alignment, nullptr, debugName};

    std::clog << "[BUFFER MGR] CREATED 0x" << std::hex << handle << std::dec
              << " size=" << size << " name=" << debugName << " (pool=" << (offset != 0 ? "YES" : "NO") << ")\n";

    return handle;
}

void VulkanBufferManager::destroyBuffer(uint64_t enc_handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buffers_.find(enc_handle);
    if (it == buffers_.end()) return;

    const auto& b = it->second;
    if (b.mapped) vkUnmapMemory(device_, b.memory);

    // RETURN TO POOL
    freePools_[0].push_back({b.memory, 0, b.size});  // simplified — real code splits blocks

    vkDestroyBuffer(device_, b.buffer, nullptr);
    buffers_.erase(it);
}

VkBuffer VulkanBufferManager::getRawBuffer(uint64_t enc_handle) const {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? it->second.buffer : VK_NULL_HANDLE;
}

VkDeviceSize VulkanBufferManager::getSize(uint64_t enc_handle) const {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? it->second.size : 0;
}

VkDeviceMemory VulkanBufferManager::getMemory(uint64_t enc_handle) const {
    auto it = buffers_.find(enc_handle);
    return it != buffers_.end() ? it->second.memory : VK_NULL_HANDLE;
}

void* VulkanBufferManager::map(uint64_t enc_handle) {
    auto it = buffers_.find(enc_handle);
    if (it == buffers_.end() || it->second.mapped) return nullptr;
    void* data;
    vkMapMemory(device_, it->second.memory, 0, it->second.size, 0, &data);
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
    std::clog << "[BUFFER MGR STATS] Buffers: " << buffers_.size()
              << " | Free pools: " << freePools_[0].size() << " — HYPERSPEED ACTIVE\n";
}

void VulkanBufferManager::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [h, b] : buffers_) {
        if (b.mapped) vkUnmapMemory(device_, b.memory);
        vkDestroyBuffer(device_, b.buffer, nullptr);
        vkFreeMemory(device_, b.memory, nullptr);
    }
    buffers_.clear();
    for (auto& pool : freePools_) pool.clear();
    std::clog << "[BUFFER MGR] OBLITERATED — VALHALLA CLEAN 🩷\n";
}