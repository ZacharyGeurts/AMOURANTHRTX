// src/engine/Vulkan/VulkanBufferManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// FINAL STONKEYED BUFFER MANAGER — BEATS UNREAL/CDPR/GODOT — NOVEMBER 08 2025

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include <stdexcept>
#include <format>

using namespace Logging::Color;

void VulkanBufferManager::init(VkDevice device, VkPhysicalDevice physDevice) {
    device_ = device;
    physDevice_ = physDevice;
    LOG_SUCCESS_CAT("BufferMgr", "STONKEYED Buffer Manager initialized — device @ {:p} — phys @ {:p}", 
                    static_cast<void*>(device), static_cast<void*>(physDevice));
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

    VK_CHECK(VK_ERROR_FEATURE_NOT_PRESENT, "No suitable memory type found — GPU too weak for AMOURANTH RTX");
    return 0; // unreachable
}

void VulkanBufferManager::cleanup() {
    for (const auto& [enc, buf] : buffers_) {
        if (buf.buffer_enc_) {
            VkBuffer raw = reinterpret_cast<VkBuffer>(decrypt(buf.buffer_enc_));
            LOG_INFO_CAT("Dispose", "{}STONK DESTROY VkBuffer @ {:p} — RASPBERRY_PINK ETERNAL 🩷{}", 
                         RASPBERRY_PINK, static_cast<void*>(raw), RESET);
            vkDestroyBuffer(device_, raw, nullptr);
        }
        if (buf.memory_enc_) {
            VkDeviceMemory raw = reinterpret_cast<VkDeviceMemory>(decrypt(buf.memory_enc_));
            LOG_INFO_CAT("Dispose", "{}STONK FREE VkDeviceMemory @ {:p} — RASPBERRY_PINK LOVE 🩷{}", 
                         RASPBERRY_PINK, static_cast<void*>(raw), RESET);
            vkFreeMemory(device_, raw, nullptr);
        }
    }
    buffers_.clear();
    LOG_SUCCESS_CAT("BufferMgr", "ALL BUFFERS STONKED — RASPBERRY_PINK PURGED 🩷");
}

uint64_t VulkanBufferManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer rawBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &rawBuffer), "Buffer creation failed — GPU crying");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, rawBuffer, &memReqs);

    uint32_t memTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memTypeIndex
    };

    VkDeviceMemory rawMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &rawMemory), "Memory allocation failed — out of VRAM?");
    VK_CHECK(vkBindBufferMemory(device_, rawBuffer, rawMemory, 0), "Buffer bind failed — driver drunk");

    BufferInfo info;
    info.buffer_enc_ = encrypt(reinterpret_cast<uintptr_t>(rawBuffer));
    info.memory_enc_ = encrypt(reinterpret_cast<uintptr_t>(rawMemory));
    info.size_ = size;

    uint64_t handle = info.buffer_enc_;
    buffers_[handle] = info;

    LOG_SUCCESS_CAT("BufferMgr", "STONKEYED buffer created — size={} — encrypted_handle=0x{:x} — faster than Unreal", 
                    size, handle);

    return handle;
}

void VulkanBufferManager::destroyBuffer(uint64_t enc_handle) {
    auto it = buffers_.find(enc_handle);
    if (it == buffers_.end()) {
        LOG_WARNING_CAT("BufferMgr", "Destroy unknown STONK handle 0x{:x} — ignored", enc_handle);
        return;
    }

    const auto& buf = it->second;
    VkBuffer rawBuffer = reinterpret_cast<VkBuffer>(decrypt(buf.buffer_enc_));
    LOG_INFO_CAT("Dispose", "{}STONK MANUAL DESTROY @ {:p} — RASPBERRY_PINK KISS 🩷{}", 
                 RASPBERRY_PINK, static_cast<void*>(rawBuffer), RESET);
    vkDestroyBuffer(device_, rawBuffer, nullptr);

    if (buf.memory_enc_) {
        VkDeviceMemory rawMem = reinterpret_cast<VkDeviceMemory>(decrypt(buf.memory_enc_));
        vkFreeMemory(device_, rawMem, nullptr);
    }

    buffers_.erase(it);
}

VkBuffer VulkanBufferManager::getRawBuffer(uint64_t enc_handle) const {
    auto it = buffers_.find(enc_handle);
    if (it == buffers_.end()) return VK_NULL_HANDLE;
    return reinterpret_cast<VkBuffer>(decrypt(it->second.buffer_enc_));
}