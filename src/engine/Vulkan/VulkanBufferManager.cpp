// src/engine/Vulkan/VulkanBufferManager.cpp
// C++23 ZERO-COST — reserve_arena DEFINED — VK_CHECK FIXED — memcpy GLOBAL
// FULLY COMPILES — NO WARNINGS — RASPBERRY_PINK ROCKETSHIP 🔥🤖🚀💀🖤❤️⚡

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/logging.hpp"

#include <cstring>
#include <cstdint>

namespace VulkanRTX {

void VulkanBufferManager::initialize_staging_pool() noexcept {
    VkBuffer raw = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;

    VkBufferCreateInfo bci_{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = kStagingPoolSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(context_->device, &bci_, nullptr, &raw), "create staging buffer");

    VkMemoryRequirements reqs{};
    vkGetBufferMemoryRequirements(context_->device, raw, &reqs);

    VkMemoryAllocateInfo mai_{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = find_memory_type(context_->physicalDevice, reqs.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_->device, &mai_, nullptr, &mem), "alloc staging memory");
    VK_CHECK(vkBindBufferMemory(context_->device, raw, mem, 0), "bind staging memory");

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context_->device, mem, 0, kStagingPoolSize, 0, &mapped), "map staging memory");
    persistent_mapped_ = mapped;

    staging_buffer_ = makeBuffer(context_->device, raw);
    staging_memory_ = makeMemory(context_->device, mem);

    context_->resourceManager.addBuffer(raw);
    context_->resourceManager.addMemory(mem);
}

void VulkanBufferManager::persistent_copy(const void* src, VkDeviceSize size, VkDeviceSize offset) const noexcept {
    ::memcpy(static_cast<uint8_t*>(persistent_mapped_) + offset, src, static_cast<size_t>(size));
}

void VulkanBufferManager::reserve_arena(VkDeviceSize size) noexcept {
    if (size <= arena_size_) return;

    VkBuffer raw = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;

    constexpr VkBufferUsageFlags usage =
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

    VkBufferCreateInfo bci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(context_->device, &bci, nullptr, &raw), "reserve_arena: create buffer");

    VkMemoryRequirements reqs{};
    vkGetBufferMemoryRequirements(context_->device, raw, &reqs);

    VkMemoryAllocateFlagsInfo flags{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };
    VkMemoryAllocateInfo mai{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &flags,
        .allocationSize = reqs.size,
        .memoryTypeIndex = find_memory_type(context_->physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_->device, &mai, nullptr, &mem), "reserve_arena: alloc memory");
    VK_CHECK(vkBindBufferMemory(context_->device, raw, mem, 0), "reserve_arena: bind memory");

    arena_buffer_ = makeBuffer(context_->device, raw);
    arena_memory_ = makeMemory(context_->device, mem);
    arena_size_   = size;

    context_->resourceManager.addBuffer(raw);
    context_->resourceManager.addMemory(mem);
}

} // namespace VulkanRTX