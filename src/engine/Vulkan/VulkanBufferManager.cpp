// src/engine/Vulkan/VulkanBufferManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// GLOBAL STONEKEYED BUFFER MANAGER — LOW-LEVEL LOGGING — NOVEMBER 08 2025

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include <stdexcept>
#include <format>
#include <iomanip>

void VulkanBufferManager::init(VkDevice device, VkPhysicalDevice physDevice) {
    device_ = device;
    physDevice_ = physDevice;
    std::clog << "[BUFFER MGR] Initialized — device @ " << static_cast<void*>(device)
              << " — phys @ " << static_cast<void*>(physDevice) << std::endl;
}

uint32_t VulkanBufferManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDevice_, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    vkThrow(std::format("No suitable memory type — filter=0x{:x} props=0x{:x}", typeFilter, properties));
    return 0; // unreachable
}

void VulkanBufferManager::cleanup() {
    std::clog << "[BUFFER MGR] Cleanup — " << buffers_.size() << " buffers" << std::endl;
    for (const auto& [enc, buf] : buffers_) {
        if (buf.buffer_enc_) {
            VkBuffer b = reinterpret_cast<VkBuffer>(decrypt(buf.buffer_enc_));
            std::clog << "[DISPOSE] vkDestroyBuffer @ " << static_cast<void*>(b) << std::endl;
            vkDestroyBuffer(device_, b, nullptr);
        }
        if (buf.memory_enc_) {
            VkDeviceMemory m = reinterpret_cast<VkDeviceMemory>(decrypt(buf.memory_enc_));
            std::clog << "[DISPOSE] vkFreeMemory @ " << static_cast<void*>(m) << std::endl;
            vkFreeMemory(device_, m, nullptr);
        }
    }
    buffers_.clear();
}

uint64_t VulkanBufferManager::createBuffer(VkDeviceSize size,
                                           VkBufferUsageFlags usage,
                                           VkMemoryPropertyFlags properties) {
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = size; ci.usage = usage; ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer rawBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device_, &ci, nullptr, &rawBuffer),
             std::format("vkCreateBuffer failed size={} usage=0x{:x}", size, usage));

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device_, rawBuffer, &reqs);

    uint32_t memType = findMemoryType(reqs.memoryTypeBits, properties);

    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = reqs.size; ai.memoryTypeIndex = memType;

    VkDeviceMemory rawMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &rawMemory),
             std::format("vkAllocateMemory failed size={}", reqs.size));

    VK_CHECK(vkBindBufferMemory(device_, rawBuffer, rawMemory, 0),
             "vkBindBufferMemory failed");

    BufferInfo info;
    info.buffer_enc_ = encrypt(reinterpret_cast<uintptr_t>(rawBuffer));
    info.memory_enc_ = encrypt(reinterpret_cast<uintptr_t>(rawMemory));
    info.size_ = size;

    uint64_t handle = info.buffer_enc_;
    buffers_[handle] = std::move(info);

    std::clog << "[BUFFER MGR] Created enc_handle=0x" << std::hex << handle << std::dec
              << " raw=" << static_cast<void*>(rawBuffer) << " size=" << size << std::endl;

    return handle;
}

void VulkanBufferManager::destroyBuffer(uint64_t enc_handle) {
    auto it = buffers_.find(enc_handle);
    if (it == buffers_.end()) {
        std::clog << "[BUFFER MGR] destroyBuffer unknown handle 0x" << std::hex << enc_handle << std::dec << std::endl;
        return;
    }
    const auto& buf = it->second;
    VkBuffer b = reinterpret_cast<VkBuffer>(decrypt(buf.buffer_enc_));
    vkDestroyBuffer(device_, b, nullptr);
    if (buf.memory_enc_) {
        VkDeviceMemory m = reinterpret_cast<VkDeviceMemory>(decrypt(buf.memory_enc_));
        vkFreeMemory(device_, m, nullptr);
    }
    buffers_.erase(it);
    std::clog << "[BUFFER MGR] Destroyed handle 0x" << std::hex << enc_handle << std::dec << std::endl;
}

VkBuffer VulkanBufferManager::getRawBuffer(uint64_t enc_handle) const {
    auto it = buffers_.find(enc_handle);
    if (it == buffers_.end()) {
        std::clog << "[BUFFER MGR] getRawBuffer invalid handle 0x" << std::hex << enc_handle << std::dec << std::endl;
        return VK_NULL_HANDLE;
    }
    return reinterpret_cast<VkBuffer>(decrypt(it->second.buffer_enc_));
}