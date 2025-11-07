// src/engine/Vulkan/VulkanBufferManager.cpp
// GLOBAL SPACE — 256MB ARENA — ON THE FLY — CHEAT ENGINE DEAD — RASPBERRY_PINK ASCENDED

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/logging.hpp"

namespace {

constexpr uint64_t kHandleObfuscator = 0xDEADBEEF1337C0DEULL;
inline VkBuffer obfuscate(VkBuffer b) noexcept { return VkBuffer(uint64_t(b) ^ kHandleObfuscator); }

} // anonymous namespace — zero cost

void VulkanBufferManager::initialize_staging_pool() noexcept {
    VkBuffer raw = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;

    VkBufferCreateInfo bci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = 64uz * 1024 * 1024,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(context_->device, &bci, nullptr, &raw));
    raw = obfuscate(raw);

    VkMemoryRequirements reqs{};
    vkGetBufferMemoryRequirements(context_->device, deobfuscate(raw), &reqs);

    VkMemoryAllocateInfo mai{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = find_memory_type(context_->physicalDevice, reqs.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_->device, &mai, nullptr, &mem));
    VK_CHECK(vkBindBufferMemory(context_->device, deobfuscate(raw), mem, 0));

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context_->device, mem, 0, VK_WHOLE_SIZE, 0, &mapped));
    persistent_mapped_ = mapped;

    staging_buffer_ = makeBuffer(context_->device, raw);
    staging_memory_ = makeMemory(context_->device, mem);

    context_->resourceManager.addBuffer(deobfuscate(raw));
    context_->resourceManager.addMemory(mem);
}

void VulkanBufferManager::persistent_copy(const void* src, VkDeviceSize size, VkDeviceSize offset) const noexcept {
    std::memcpy(static_cast<uint8_t*>(persistent_mapped_) + offset, src, size);
}

void VulkanBufferManager::reserve_arena(VkDeviceSize size) noexcept {
    if (size <= arena_size_) return;

    VkBuffer raw = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;

    VkBufferCreateInfo bci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(context_->device, &bci, nullptr, &raw));
    raw = obfuscate(raw);

    VkMemoryRequirements reqs{};
    vkGetBufferMemoryRequirements(context_->device, deobfuscate(raw), &reqs);

    VkMemoryAllocateFlagsInfo flags{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT};
    VkMemoryAllocateInfo mai{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &flags,
        .allocationSize = reqs.size,
        .memoryTypeIndex = find_memory_type(context_->physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_->device, &mai, nullptr, &mem));
    VK_CHECK(vkBindBufferMemory(context_->device, deobfuscate(raw), mem, 0));

    arena_buffer_ = makeBuffer(context_->device, raw);
    arena_memory_ = makeMemory(context_->device, mem);
    arena_size_ = size;

    context_->resourceManager.addBuffer(deobfuscate(raw));
    context_->resourceManager.addMemory(mem);
}