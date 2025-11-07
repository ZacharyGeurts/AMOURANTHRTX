// VulkanBufferManager.cpp
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include <stdexcept>

void VulkanBufferManager::init(VkDevice device) {
    device_ = device;
}

void VulkanBufferManager::cleanup() {
    for (const auto& buf : buffers_) {
        if (buf.buffer_enc_) {
            VkBuffer raw = decrypt<VkBuffer>(buf.buffer_enc_);
            vkDestroyBuffer(device_, raw, nullptr);
        }
        if (buf.memory_enc_) {
            VkDeviceMemory raw = decrypt<VkDeviceMemory>(buf.memory_enc_);
            vkFreeMemory(device_, raw, nullptr);
        }
    }
    buffers_.clear();
}

uint64_t VulkanBufferManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer rawBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &rawBuffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, rawBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    // Simplified — in real code find memory type
    allocInfo.memoryTypeIndex = 0;

    VkDeviceMemory rawMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &rawMemory));
    VK_CHECK(vkBindBufferMemory(device_, rawBuffer, rawMemory, 0));

    BufferInfo info;
    info.buffer_enc_ = encrypt(rawBuffer);
    info.memory_enc_ = encrypt(rawMemory);
    info.size_ = size;

    buffers_.push_back(info);
    return encrypt(rawBuffer); // Return encrypted handle
}

void VulkanBufferManager::destroyBuffer(uint64_t enc_handle) {
    VkBuffer raw = decrypt<VkBuffer>(enc_handle);
    // Find and destroy (simplified — real code would search)
    vkDestroyBuffer(device_, raw, nullptr);
    // Also free memory...
}