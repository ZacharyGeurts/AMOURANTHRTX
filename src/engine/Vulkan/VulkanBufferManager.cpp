// AMOURANTH RTX Engine, October 2025 - Vulkan buffer management.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, Vulkan_init.hpp, ue_init.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows.
// Optimized for high-end GPUs with 8 GB VRAM (e.g., NVIDIA RTX 3070, AMD RX 6800).
// Zachary Geurts 2025

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Dispose.hpp"
#include "ue_init.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <thread>

namespace {
// Scaling factors optimized for 8 GB VRAM
constexpr VkDeviceSize VERTEX_BUFFER_SCALE_FACTOR = 4;
constexpr VkDeviceSize INDEX_BUFFER_SCALE_FACTOR = 4;
constexpr VkDeviceSize SCRATCH_BUFFER_SCALE_FACTOR = 4;
constexpr VkDeviceSize UNIFORM_BUFFER_DEFAULT_SIZE = 2048;
constexpr uint32_t DEFAULT_UNIFORM_BUFFER_COUNT = 64;
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
constexpr VkDeviceSize ARENA_DEFAULT_SIZE = 16 * 1024 * 1024;
constexpr uint32_t COMMAND_BUFFER_POOL_SIZE = 32;
constexpr uint32_t SCRATCH_BUFFER_POOL_COUNT = 4;
// Timeout for fence waiting (5 seconds)
constexpr uint64_t FENCE_TIMEOUT = 5'000'000'000ULL;

bool isMeshShaderSupported(VkPhysicalDevice physicalDevice) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());
    for (const auto& ext : extensions) {
        if (std::strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

bool isDescriptorIndexingSupported(VkPhysicalDevice physicalDevice) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());
    for (const auto& ext : extensions) {
        if (std::strcmp(ext.extensionName, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

void cleanupResources(
    VkDevice device, VkCommandPool commandPool,
    VkBuffer buffer, VkDeviceMemory memory,
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE,
    VkFence fence = VK_NULL_HANDLE) {
    if (commandBuffer != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE) {
        std::vector<VkCommandBuffer> vec = {commandBuffer};
        Dispose::freeCommandBuffers(device, commandPool, vec);
    }
    if (buffer != VK_NULL_HANDLE) {
        Dispose::destroySingleBuffer(device, buffer);
    }
    if (memory != VK_NULL_HANDLE) {
        Dispose::freeSingleDeviceMemory(device, memory);
    }
    if (fence != VK_NULL_HANDLE) {
        vkDestroyFence(device, fence, nullptr);
    }
}
} // namespace

struct VulkanBufferManager::Impl {
    VkBuffer arenaBuffer = VK_NULL_HANDLE;
    VkDeviceMemory arenaMemory = VK_NULL_HANDLE;
    VkDeviceSize arenaSize = 0;
    VkDeviceSize vertexOffset = 0;
    VkDeviceSize indexOffset = 0;
    VkDeviceSize meshletOffset = 0;
    std::vector<VkDeviceSize> uniformBufferOffsets; // Offsets for uniform buffers
    std::unordered_map<uint64_t, VkDeviceSize> scratchSizeCache;
    std::vector<VkBuffer> scratchBuffers;
    std::vector<VkDeviceMemory> scratchBufferMemories;
    std::vector<VkDeviceAddress> scratchBufferAddresses;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    uint32_t nextCommandBuffer = 0;
    VkQueue transferQueue = VK_NULL_HANDLE;
    uint32_t transferQueueFamily = UINT32_MAX;
    VkSemaphore timelineSemaphore = VK_NULL_HANDLE;
    uint64_t nextUpdateId = 1;
    std::unordered_map<uint64_t, uint64_t> updateSignals;
    bool useMeshShaders = false;
    bool useDescriptorIndexing = false;
};

VulkanBufferManager::VulkanBufferManager(Vulkan::Context& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices)
    : context_(context), vertexCount_(0), indexCount_(0), vertexBufferAddress_(0), indexBufferAddress_(0), scratchBufferAddress_(0), impl_(std::make_unique<Impl>()) {
    if (context_.device == VK_NULL_HANDLE || context_.physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Invalid Vulkan device or physical device");
    }

    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProperties);
    VkDeviceSize totalDeviceLocalMemory = 0;
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; ++i) {
        if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalDeviceLocalMemory += memProperties.memoryHeaps[i].size;
        }
    }
    if (totalDeviceLocalMemory < 8ULL * 1024 * 1024 * 1024) {
    }

    // Validate input
    if (vertices.empty() || indices.empty()) {
        throw std::invalid_argument("Vertex or index data cannot be empty");
    }
    vertexCount_ = static_cast<uint32_t>(vertices.size());
    indexCount_ = static_cast<uint32_t>(indices.size());
    if (indexCount_ < 3 || indexCount_ % 3 != 0) {
        throw std::invalid_argument("Invalid geometry for triangle-based rendering");
    }

    // Query device properties
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = nullptr,
        .properties = {}
    };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
        .pNext = nullptr,
        .maxGeometryCount = 0,
        .maxInstanceCount = 0,
        .maxPrimitiveCount = 0,
        .maxPerStageDescriptorAccelerationStructures = 0,
        .maxPerStageDescriptorUpdateAfterBindAccelerationStructures = 0,
        .maxDescriptorSetAccelerationStructures = 0,
        .maxDescriptorSetUpdateAfterBindAccelerationStructures = 0,
        .minAccelerationStructureScratchOffsetAlignment = 0
    };
    props2.pNext = &accelProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    impl_->useMeshShaders = isMeshShaderSupported(context_.physicalDevice);
    impl_->useDescriptorIndexing = isDescriptorIndexingSupported(context_.physicalDevice);

    // Initialize command buffer mega-pool
    initializeCommandPool();

    // Create unified arena
    VkDeviceSize vertexBufferSize = sizeof(glm::vec3) * vertices.size() * VERTEX_BUFFER_SCALE_FACTOR;
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size() * INDEX_BUFFER_SCALE_FACTOR;
    VkDeviceSize totalBufferSize = vertexBufferSize + indexBufferSize;
    VkDeviceSize arenaSize = std::max(ARENA_DEFAULT_SIZE, totalBufferSize * 2);
    if (arenaSize > totalDeviceLocalMemory / 2) {
        throw std::runtime_error("Arena size exceeds VRAM capacity");
    }
    reserveArena(arenaSize, BufferType::GEOMETRY);

    // Copy data to arena
    vertexBufferAddress_ = asyncUpdateBuffers(vertices, indices, nullptr);
    indexBufferAddress_ = vertexBufferAddress_ + vertexBufferSize;

    // Initialize uniform buffers
    createUniformBuffers(MAX_FRAMES_IN_FLIGHT);

    // Initialize scratch buffer pool
    VkDeviceSize scratchSize = getScratchSize(vertexCount_, indexCount_) * SCRATCH_BUFFER_SCALE_FACTOR;
    if (scratchSize > totalDeviceLocalMemory / 4) {
        throw std::runtime_error("Scratch buffer size exceeds VRAM capacity");
    }
    reserveScratchPool(scratchSize, SCRATCH_BUFFER_POOL_COUNT);
    scratchBufferAddress_ = impl_->scratchBufferAddresses[0];
}

void VulkanBufferManager::initializeCommandPool() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, queueFamilies.data());
    impl_->transferQueueFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            impl_->transferQueueFamily = i;
            break;
        }
    }
    if (impl_->transferQueueFamily == UINT32_MAX) {
        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                impl_->transferQueueFamily = i;
                break;
            }
        }
    }
    if (impl_->transferQueueFamily == UINT32_MAX) {
        throw std::runtime_error("No suitable queue family for transfer");
    }

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = impl_->transferQueueFamily
    };
    if (vkCreateCommandPool(context_.device, &poolInfo, nullptr, &impl_->commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    impl_->commandBuffers.resize(COMMAND_BUFFER_POOL_SIZE);
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = impl_->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = COMMAND_BUFFER_POOL_SIZE
    };
    if (vkAllocateCommandBuffers(context_.device, &allocInfo, impl_->commandBuffers.data()) != VK_SUCCESS) {
        vkDestroyCommandPool(context_.device, impl_->commandPool, nullptr);
        throw std::runtime_error("Failed to allocate command buffers");
    }

    vkGetDeviceQueue(context_.device, impl_->transferQueueFamily, 0, &impl_->transferQueue);
    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    if (vkCreateSemaphore(context_.device, &semaphoreInfo, nullptr, &impl_->timelineSemaphore) != VK_SUCCESS) {
        vkDestroyCommandPool(context_.device, impl_->commandPool, nullptr);
        throw std::runtime_error("Failed to create timeline semaphore");
    }
}

void VulkanBufferManager::reserveArena(VkDeviceSize size, BufferType type) {
    if (impl_->arenaBuffer != VK_NULL_HANDLE) {
        context_.resourceManager.removeBuffer(impl_->arenaBuffer);
        Dispose::destroySingleBuffer(context_.device, impl_->arenaBuffer);
        context_.resourceManager.removeMemory(impl_->arenaMemory);
        Dispose::freeSingleDeviceMemory(context_.device, impl_->arenaMemory);
        impl_->arenaBuffer = VK_NULL_HANDLE;
        impl_->arenaMemory = VK_NULL_HANDLE;
    }

    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (type == BufferType::GEOMETRY) {
        usageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        if (impl_->useMeshShaders) {
            usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }
    } else if (type == BufferType::UNIFORM) {
        usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = nullptr,
        .properties = {}
    };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
        .pNext = nullptr,
        .maxGeometryCount = 0,
        .maxInstanceCount = 0,
        .maxPrimitiveCount = 0,
        .maxPerStageDescriptorAccelerationStructures = 0,
        .maxPerStageDescriptorUpdateAfterBindAccelerationStructures = 0,
        .maxDescriptorSetAccelerationStructures = 0,
        .maxDescriptorSetUpdateAfterBindAccelerationStructures = 0,
        .minAccelerationStructureScratchOffsetAlignment = 0
    };
    props2.pNext = &accelProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    size = (size + props2.properties.limits.minStorageBufferOffsetAlignment - 1) & ~(props2.properties.limits.minStorageBufferOffsetAlignment - 1);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0
    };
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice, size,
        usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        impl_->arenaBuffer, impl_->arenaMemory, &allocFlagsInfo, context_.resourceManager
    );
    if (impl_->arenaBuffer == VK_NULL_HANDLE || impl_->arenaMemory == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to create arena buffer");
    }
    impl_->arenaSize = size;
}

VkDeviceAddress VulkanBufferManager::asyncUpdateBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices, std::function<void(uint64_t)> callback) {
    VkDeviceSize vertexSize = sizeof(glm::vec3) * vertices.size() * VERTEX_BUFFER_SCALE_FACTOR;
    VkDeviceSize indexSize = sizeof(uint32_t) * indices.size() * INDEX_BUFFER_SCALE_FACTOR;
    VkDeviceSize totalSize = vertexSize + indexSize;
    if (totalSize > impl_->arenaSize) {
        throw std::runtime_error("Update size exceeds arena capacity");
    }

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkCommandBuffer cmd = getCommandBuffer(BufferOperation::TRANSFER);

    // Create staging buffer
    VkBufferCreateInfo stagingInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = totalSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    if (vkCreateBuffer(context_.device, &stagingInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer");
    }

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(context_.device, stagingBuffer, &memReq);
    uint32_t memType = VulkanInitializer::findMemoryType(context_.physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memReq.size,
        .memoryTypeIndex = memType
    };
    if (vkAllocateMemory(context_.device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        throw std::runtime_error("Failed to allocate staging memory");
    }
    if (vkBindBufferMemory(context_.device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingMemory);
        throw std::runtime_error("Failed to bind staging memory");
    }

    // Map and copy data
    void* data;
    if (vkMapMemory(context_.device, stagingMemory, 0, totalSize, 0, &data) != VK_SUCCESS) {
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingMemory);
        throw std::runtime_error("Failed to map staging memory");
    }
    memcpy(data, vertices.data(), vertexSize / VERTEX_BUFFER_SCALE_FACTOR);
    memcpy(static_cast<char*>(data) + vertexSize / VERTEX_BUFFER_SCALE_FACTOR, indices.data(), indexSize / INDEX_BUFFER_SCALE_FACTOR);
    vkUnmapMemory(context_.device, stagingMemory);

    // Create fence for synchronization
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0 // Not signaled initially
    };
    if (vkCreateFence(context_.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingMemory);
        throw std::runtime_error("Failed to create fence for command buffer");
    }

    // Record command buffer
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
        throw std::runtime_error("Failed to begin command buffer");
    }

    VkBufferCopy vertexCopy{.srcOffset = 0, .dstOffset = impl_->vertexOffset, .size = vertexSize / VERTEX_BUFFER_SCALE_FACTOR};
    VkBufferCopy indexCopy{.srcOffset = vertexSize / VERTEX_BUFFER_SCALE_FACTOR, .dstOffset = impl_->indexOffset, .size = indexSize / INDEX_BUFFER_SCALE_FACTOR};
    vkCmdCopyBuffer(cmd, stagingBuffer, impl_->arenaBuffer, 1, &vertexCopy);
    vkCmdCopyBuffer(cmd, stagingBuffer, impl_->arenaBuffer, 1, &indexCopy);

    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_SHADER_READ_BIT
    };
    if (impl_->useMeshShaders) {
        barrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    }
    vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | (impl_->useMeshShaders ? VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT : 0),
        0, 1, &barrier, 0, nullptr, 0, nullptr
    );

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
        throw std::runtime_error("Failed to end command buffer");
    }

    // Submit command buffer with fence
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &impl_->timelineSemaphore
    };
    uint64_t updateId = impl_->nextUpdateId++;
    VkTimelineSemaphoreSubmitInfo timelineInfo{
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreValueCount = 0,
        .pWaitSemaphoreValues = nullptr,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &updateId
    };
    submitInfo.pNext = &timelineInfo;

    if (vkQueueSubmit(impl_->transferQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
        throw std::runtime_error("Failed to submit command buffer");
    }

    // Wait for fence
    VkResult result = vkWaitForFences(context_.device, 1, &fence, VK_TRUE, FENCE_TIMEOUT);
    if (result != VK_SUCCESS) {
        cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
        throw std::runtime_error("Failed to wait for fence for command buffer");
    }

    // Signal callback if provided
    impl_->updateSignals[updateId] = updateId;
    if (callback) {
        std::thread([this, updateId, callback]() {
            uint64_t value = 0;
            while (value < updateId) {
                vkGetSemaphoreCounterValue(context_.device, impl_->timelineSemaphore, &value);
                std::this_thread::yield();
            }
            impl_->updateSignals.erase(updateId);
            callback(updateId);
        }).detach();
    }

    // Cleanup
    cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);

    impl_->vertexOffset = 0;
    impl_->indexOffset = vertexSize;
    vertexCount_ = static_cast<uint32_t>(vertices.size());
    indexCount_ = static_cast<uint32_t>(indices.size());
    return VulkanInitializer::getBufferDeviceAddress(context_.device, impl_->arenaBuffer) + impl_->vertexOffset;
}

void VulkanBufferManager::createUniformBuffers(uint32_t count) {
    if (count == 0) {
        return;
    }

    if (!context_.uniformBuffers.empty()) {
        for (size_t i = 0; i < context_.uniformBuffers.size(); ++i) {
            context_.resourceManager.removeBuffer(context_.uniformBuffers[i]);
            context_.resourceManager.removeMemory(context_.uniformBufferMemories[i]);
        }
        Dispose::destroyBuffers(context_.device, context_.uniformBuffers);
        Dispose::freeDeviceMemories(context_.device, context_.uniformBufferMemories);
        context_.uniformBuffers.clear();
        context_.uniformBufferMemories.clear();
        impl_->uniformBufferOffsets.clear();
    }

    VkPhysicalDeviceProperties deviceProps{};
    vkGetPhysicalDeviceProperties(context_.physicalDevice, &deviceProps);
    VkDeviceSize bufferSize = (sizeof(UE::UniformBufferObject) + sizeof(int) + UNIFORM_BUFFER_DEFAULT_SIZE + deviceProps.limits.minUniformBufferOffsetAlignment - 1) & ~(deviceProps.limits.minUniformBufferOffsetAlignment - 1);
    VkDeviceSize totalSize = bufferSize * count;
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProperties);
    VkDeviceSize totalDeviceLocalMemory = 0;
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; ++i) {
        if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalDeviceLocalMemory += memProperties.memoryHeaps[i].size;
        }
    }
    if (totalSize > totalDeviceLocalMemory / 8) {
        throw std::runtime_error("Uniform buffer size exceeds VRAM capacity");
    }

    context_.uniformBuffers.resize(count);
    context_.uniformBufferMemories.resize(count);
    impl_->uniformBufferOffsets.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VulkanInitializer::createBuffer(
            context_.device, context_.physicalDevice, bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            context_.uniformBuffers[i], context_.uniformBufferMemories[i], nullptr, context_.resourceManager
        );
        if (context_.uniformBuffers[i] == VK_NULL_HANDLE || context_.uniformBufferMemories[i] == VK_NULL_HANDLE) {
            for (uint32_t j = 0; j < i; ++j) {
                context_.resourceManager.removeBuffer(context_.uniformBuffers[j]);
                context_.resourceManager.removeMemory(context_.uniformBufferMemories[j]);
            }
            throw std::runtime_error("Failed to create uniform buffer");
        }
        impl_->uniformBufferOffsets[i] = 0; // Individual buffers, no offsets needed
    }
}

void VulkanBufferManager::configureUniformBuffers(uint32_t count, VkDeviceSize size_per_buffer) {
    if (count == 0) {
        return;
    }

    if (!context_.uniformBuffers.empty()) {
        for (size_t i = 0; i < context_.uniformBuffers.size(); ++i) {
            context_.resourceManager.removeBuffer(context_.uniformBuffers[i]);
            context_.resourceManager.removeMemory(context_.uniformBufferMemories[i]);
        }
        Dispose::destroyBuffers(context_.device, context_.uniformBuffers);
        Dispose::freeDeviceMemories(context_.device, context_.uniformBufferMemories);
        context_.uniformBuffers.clear();
        context_.uniformBufferMemories.clear();
        impl_->uniformBufferOffsets.clear();
    }

    VkPhysicalDeviceProperties deviceProps{};
    vkGetPhysicalDeviceProperties(context_.physicalDevice, &deviceProps);
    size_per_buffer = (size_per_buffer + deviceProps.limits.minUniformBufferOffsetAlignment - 1) & ~(deviceProps.limits.minUniformBufferOffsetAlignment - 1);
    VkDeviceSize totalSize = size_per_buffer * count;
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProperties);
    VkDeviceSize totalDeviceLocalMemory = 0;
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; ++i) {
        if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalDeviceLocalMemory += memProperties.memoryHeaps[i].size;
        }
    }
    if (totalSize > totalDeviceLocalMemory / 8) {
        throw std::runtime_error("Uniform buffer size exceeds VRAM capacity");
    }

    reserveArena(totalSize, BufferType::UNIFORM);
    context_.uniformBuffers.resize(count);
    context_.uniformBufferMemories.resize(count);
    impl_->uniformBufferOffsets.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        context_.uniformBuffers[i] = impl_->arenaBuffer;
        context_.uniformBufferMemories[i] = impl_->arenaMemory;
        impl_->uniformBufferOffsets[i] = i * size_per_buffer;
    }
}

void VulkanBufferManager::enableMeshShaders(bool enable, [[maybe_unused]] const MeshletConfig& config) {
    impl_->useMeshShaders = enable && isMeshShaderSupported(context_.physicalDevice);
    if (impl_->useMeshShaders) {
        impl_->meshletOffset = impl_->indexOffset + (sizeof(uint32_t) * indexCount_ * INDEX_BUFFER_SCALE_FACTOR);
    }
}

void VulkanBufferManager::addMeshletBatch(std::span<const MeshletData> meshlets) {
    if (!impl_->useMeshShaders) {
        throw std::runtime_error("Mesh shaders not enabled");
    }
    VkDeviceSize totalSize = 0;
    for (const auto& meshlet : meshlets) {
        totalSize += meshlet.size;
    }
    if (impl_->meshletOffset + totalSize > impl_->arenaSize) {
        throw std::runtime_error("Meshlet data exceeds arena capacity");
    }

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkCommandBuffer cmd = getCommandBuffer(BufferOperation::TRANSFER);

    VkBufferCreateInfo stagingInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = totalSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    if (vkCreateBuffer(context_.device, &stagingInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer for meshlets");
    }

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(context_.device, stagingBuffer, &memReq);
    uint32_t memType = VulkanInitializer::findMemoryType(context_.physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memReq.size,
        .memoryTypeIndex = memType
    };
    if (vkAllocateMemory(context_.device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        throw std::runtime_error("Failed to allocate staging memory for meshlets");
    }
    if (vkBindBufferMemory(context_.device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingMemory);
        throw std::runtime_error("Failed to bind staging memory for meshlets");
    }

    void* data;
    if (vkMapMemory(context_.device, stagingMemory, 0, totalSize, 0, &data) != VK_SUCCESS) {
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingMemory);
        throw std::runtime_error("Failed to map staging memory for meshlets");
    }
    VkDeviceSize offset = 0;
    for (const auto& meshlet : meshlets) {
        memcpy(static_cast<char*>(data) + offset, meshlet.data, meshlet.size);
        offset += meshlet.size;
    }
    vkUnmapMemory(context_.device, stagingMemory);

    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    if (vkCreateFence(context_.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingMemory);
        throw std::runtime_error("Failed to create fence for meshlet command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
        throw std::runtime_error("Failed to begin command buffer for meshlets");
    }

    VkBufferCopy copy{.srcOffset = 0, .dstOffset = impl_->meshletOffset, .size = totalSize};
    vkCmdCopyBuffer(cmd, stagingBuffer, impl_->arenaBuffer, 1, &copy);
    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
        throw std::runtime_error("Failed to end command buffer for meshlets");
    }

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };

    if (vkQueueSubmit(impl_->transferQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
        throw std::runtime_error("Failed to submit command buffer for meshlets");
    }

    VkResult result = vkWaitForFences(context_.device, 1, &fence, VK_TRUE, FENCE_TIMEOUT);
    if (result != VK_SUCCESS) {
        cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
        throw std::runtime_error("Failed to wait for fence for meshlet command buffer");
    }

    cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
    impl_->meshletOffset += totalSize;
}

VkCommandBuffer VulkanBufferManager::getCommandBuffer(BufferOperation /*op*/) {
    VkCommandBuffer cmd = impl_->commandBuffers[impl_->nextCommandBuffer];
    impl_->nextCommandBuffer = (impl_->nextCommandBuffer + 1) % COMMAND_BUFFER_POOL_SIZE;
    vkResetCommandBuffer(cmd, 0);
    return cmd;
}

void VulkanBufferManager::batchTransferAsync(std::span<const BufferCopy> copies, std::function<void(uint64_t)> callback) {
    VkDeviceSize totalSize = 0;
    for (const auto& copy : copies) {
        totalSize += copy.size;
    }
    if (totalSize > impl_->arenaSize) {
        throw std::runtime_error("Batch transfer size exceeds arena capacity");
    }

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkCommandBuffer cmd = getCommandBuffer(BufferOperation::TRANSFER);

    VkBufferCreateInfo stagingInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = totalSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    if (vkCreateBuffer(context_.device, &stagingInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer for batch transfer");
    }

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(context_.device, stagingBuffer, &memReq);
    uint32_t memType = VulkanInitializer::findMemoryType(context_.physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memReq.size,
        .memoryTypeIndex = memType
    };
    if (vkAllocateMemory(context_.device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        throw std::runtime_error("Failed to allocate staging memory for batch transfer");
    }
    if (vkBindBufferMemory(context_.device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingMemory);
        throw std::runtime_error("Failed to bind staging memory for batch transfer");
    }

    void* data;
    if (vkMapMemory(context_.device, stagingMemory, 0, totalSize, 0, &data) != VK_SUCCESS) {
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingMemory);
        throw std::runtime_error("Failed to map staging memory for batch transfer");
    }
    VkDeviceSize offset = 0;
    for (const auto& copy : copies) {
        memcpy(static_cast<char*>(data) + offset, copy.data, copy.size);
        offset += copy.size;
    }
    vkUnmapMemory(context_.device, stagingMemory);

    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    if (vkCreateFence(context_.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        cleanupResources(context_.device, VK_NULL_HANDLE, stagingBuffer, stagingMemory);
        throw std::runtime_error("Failed to create fence for batch transfer command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
        throw std::runtime_error("Failed to begin command buffer for batch transfer");
    }

    std::vector<VkBufferCopy> vkCopies(copies.size());
    offset = 0;
    for (size_t i = 0; i < copies.size(); ++i) {
        vkCopies[i] = {offset, copies[i].dstOffset, copies[i].size};
        offset += copies[i].size;
    }
    vkCmdCopyBuffer(cmd, stagingBuffer, impl_->arenaBuffer, static_cast<uint32_t>(vkCopies.size()), vkCopies.data());

    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_SHADER_READ_BIT
    };
    if (impl_->useMeshShaders) {
        barrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    }
    vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | (impl_->useMeshShaders ? VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT : 0),
        0, 1, &barrier, 0, nullptr, 0, nullptr
    );

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
        throw std::runtime_error("Failed to end command buffer for batch transfer");
    }

    uint64_t updateId = impl_->nextUpdateId++;
    VkTimelineSemaphoreSubmitInfo timelineInfo{
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreValueCount = 0,
        .pWaitSemaphoreValues = nullptr,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &updateId
    };
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = &timelineInfo,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &impl_->timelineSemaphore
    };
    if (vkQueueSubmit(impl_->transferQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
        throw std::runtime_error("Failed to submit command buffer for batch transfer");
    }

    VkResult result = vkWaitForFences(context_.device, 1, &fence, VK_TRUE, FENCE_TIMEOUT);
    if (result != VK_SUCCESS) {
        cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
        throw std::runtime_error("Failed to wait for fence for batch transfer command buffer");
    }

    impl_->updateSignals[updateId] = updateId;
    if (callback) {
        std::thread([this, updateId, callback]() {
            uint64_t value = 0;
            while (value < updateId) {
                vkGetSemaphoreCounterValue(context_.device, impl_->timelineSemaphore, &value);
                std::this_thread::yield();
            }
            impl_->updateSignals.erase(updateId);
            callback(updateId);
        }).detach();
    }
    cleanupResources(context_.device, impl_->commandPool, stagingBuffer, stagingMemory, cmd, fence);
}

void VulkanBufferManager::reserveScratchPool(VkDeviceSize size_per_buffer, uint32_t count) {
    for (auto buffer : impl_->scratchBuffers) {
        context_.resourceManager.removeBuffer(buffer);
        Dispose::destroySingleBuffer(context_.device, buffer);
    }
    for (auto memory : impl_->scratchBufferMemories) {
        context_.resourceManager.removeMemory(memory);
        Dispose::freeSingleDeviceMemory(context_.device, memory);
    }
    impl_->scratchBuffers.clear();
    impl_->scratchBufferMemories.clear();
    impl_->scratchBufferAddresses.clear();

    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = nullptr,
        .properties = {}
    };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
        .pNext = nullptr,
        .maxGeometryCount = 0,
        .maxInstanceCount = 0,
        .maxPrimitiveCount = 0,
        .maxPerStageDescriptorAccelerationStructures = 0,
        .maxPerStageDescriptorUpdateAfterBindAccelerationStructures = 0,
        .maxDescriptorSetAccelerationStructures = 0,
        .maxDescriptorSetUpdateAfterBindAccelerationStructures = 0,
        .minAccelerationStructureScratchOffsetAlignment = 0
    };
    props2.pNext = &accelProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    size_per_buffer = (size_per_buffer + accelProps.minAccelerationStructureScratchOffsetAlignment - 1) & ~(accelProps.minAccelerationStructureScratchOffsetAlignment - 1);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0
    };
    impl_->scratchBuffers.resize(count);
    impl_->scratchBufferMemories.resize(count);
    impl_->scratchBufferAddresses.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VulkanInitializer::createBuffer(
            context_.device, context_.physicalDevice, size_per_buffer,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, impl_->scratchBuffers[i], impl_->scratchBufferMemories[i], &allocFlagsInfo, context_.resourceManager
        );
        if (impl_->scratchBuffers[i] == VK_NULL_HANDLE || impl_->scratchBufferMemories[i] == VK_NULL_HANDLE) {
            for (uint32_t j = 0; j < i; ++j) {
                context_.resourceManager.removeBuffer(impl_->scratchBuffers[j]);
                context_.resourceManager.removeMemory(impl_->scratchBufferMemories[j]);
            }
            throw std::runtime_error("Failed to create scratch buffer");
        }
        impl_->scratchBufferAddresses[i] = VulkanInitializer::getBufferDeviceAddress(context_.device, impl_->scratchBuffers[i]);
    }
    context_.scratchBuffer = impl_->scratchBuffers[0];
    context_.scratchBufferMemory = impl_->scratchBufferMemories[0];
    scratchBufferAddress_ = impl_->scratchBufferAddresses[0];
}

VkDeviceSize VulkanBufferManager::getScratchSize(uint32_t vertexCount, uint32_t indexCount) {
    uint64_t key = (static_cast<uint64_t>(vertexCount) << 32) | indexCount;
    auto it = impl_->scratchSizeCache.find(key);
    if (it != impl_->scratchSizeCache.end()) {
        return it->second;
    }

    if (indexCount < 3 || indexCount % 3 != 0) {
        throw std::invalid_argument("Invalid geometry for acceleration structure");
    }
    if (vertexCount == 0) {
        throw std::invalid_argument("No vertices for acceleration structure");
    }

    auto vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetAccelerationStructureBuildSizesKHR"));
    if (!vkGetAccelerationStructureBuildSizesKHR) {
        throw std::runtime_error("Failed to load vkGetAccelerationStructureBuildSizesKHR");
    }

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .pNext = nullptr,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = vertexBufferAddress_ },
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = vertexCount - 1,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = indexBufferAddress_ },
        .transformData = { .deviceAddress = 0 }
    };
    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = triangles },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .ppGeometries = nullptr,
        .scratchData = { .deviceAddress = 0 }
    };
    uint32_t primitiveCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizesInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    vkGetAccelerationStructureBuildSizesKHR(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizesInfo);
    impl_->scratchSizeCache[key] = sizesInfo.buildScratchSize;
    return sizesInfo.buildScratchSize;
}

void VulkanBufferManager::createScratchBuffer(VkDeviceSize size) {
    reserveScratchPool(size, 1);
}

void VulkanBufferManager::batchDescriptorUpdate(std::span<const DescriptorUpdate> updates, uint32_t maxBindings) {
    if (!impl_->useDescriptorIndexing && maxBindings > 32) {
        maxBindings = std::min(maxBindings, 32u);
    }

    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(updates.size());
    std::vector<VkDescriptorBufferInfo> bufferInfos(updates.size());
    for (size_t i = 0; i < updates.size(); ++i) {
        if (updates[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            if (updates[i].bufferIndex >= context_.uniformBuffers.size()) {
                throw std::runtime_error("Invalid uniform buffer index");
            }
            bufferInfos[i].buffer = context_.uniformBuffers[updates[i].bufferIndex];
            bufferInfos[i].offset = impl_->uniformBufferOffsets[updates[i].bufferIndex];
            bufferInfos[i].range = updates[i].size;
        }
        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = updates[i].descriptorSet,
            .dstBinding = updates[i].binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = updates[i].type,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfos[i],
            .pTexelBufferView = nullptr
        };
        writes.push_back(write);
    }
    vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanBufferManager::updateDescriptorSet(VkDescriptorSet descriptorSet, uint32_t binding, uint32_t descriptorCount, VkDescriptorType descriptorType) const {
    if (descriptorSet == VK_NULL_HANDLE) {
        throw std::invalid_argument("Descriptor set cannot be null");
    }

    std::vector<VkDescriptorBufferInfo> bufferInfos;
    prepareDescriptorBufferInfo(bufferInfos, descriptorCount);
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(descriptorCount);
    for (uint32_t i = 0; i < descriptorCount; ++i) {
        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = binding + i,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = descriptorType,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfos[i],
            .pTexelBufferView = nullptr
        };
        writes.push_back(write);
    }
    vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanBufferManager::prepareDescriptorBufferInfo(std::vector<VkDescriptorBufferInfo>& bufferInfos, uint32_t count) const {
    if (count > context_.uniformBuffers.size()) {
        throw std::runtime_error("Insufficient uniform buffers");
    }

    bufferInfos.resize(count);
    VkPhysicalDeviceProperties deviceProps{};
    vkGetPhysicalDeviceProperties(context_.physicalDevice, &deviceProps);
    VkDeviceSize bufferSize = (sizeof(UE::UniformBufferObject) + sizeof(int) + UNIFORM_BUFFER_DEFAULT_SIZE + deviceProps.limits.minUniformBufferOffsetAlignment - 1) & ~(deviceProps.limits.minUniformBufferOffsetAlignment - 1);

    for (uint32_t i = 0; i < count; ++i) {
        if (context_.uniformBuffers[i] == VK_NULL_HANDLE) {
            throw std::runtime_error("Invalid uniform buffer handle");
        }
        bufferInfos[i].buffer = context_.uniformBuffers[i];
        bufferInfos[i].offset = impl_->uniformBufferOffsets[i];
        bufferInfos[i].range = bufferSize;
    }
}

bool VulkanBufferManager::checkUpdateStatus(uint64_t updateId) const {
    auto it = impl_->updateSignals.find(updateId);
    if (it == impl_->updateSignals.end()) {
        return true; // Update completed or invalid ID
    }
    uint64_t value = 0;
    vkGetSemaphoreCounterValue(context_.device, impl_->timelineSemaphore, &value);
    return value >= it->second;
}

VkDeviceMemory VulkanBufferManager::getVertexBufferMemory() const { return impl_->arenaMemory; }
VkDeviceMemory VulkanBufferManager::getIndexBufferMemory() const { return impl_->arenaMemory; }
VkDeviceMemory VulkanBufferManager::getUniformBufferMemory(uint32_t index) const {
    validateUniformBufferIndex(index);
    return context_.uniformBufferMemories[index];
}
VkDeviceAddress VulkanBufferManager::getVertexBufferAddress() const { return vertexBufferAddress_; }
VkDeviceAddress VulkanBufferManager::getIndexBufferAddress() const { return indexBufferAddress_; }
VkDeviceAddress VulkanBufferManager::getScratchBufferAddress() const { return scratchBufferAddress_; }
VkBuffer VulkanBufferManager::getVertexBuffer() const { return impl_->arenaBuffer; }
VkBuffer VulkanBufferManager::getIndexBuffer() const { return impl_->arenaBuffer; }
VkBuffer VulkanBufferManager::getMeshletBuffer(VkDeviceSize& offset_out) const {
    offset_out = impl_->meshletOffset;
    return impl_->arenaBuffer;
}
uint32_t VulkanBufferManager::getVertexCount() const { return vertexCount_; }
uint32_t VulkanBufferManager::getIndexCount() const { return indexCount_; }
VkBuffer VulkanBufferManager::getUniformBuffer(uint32_t index) const {
    validateUniformBufferIndex(index);
    return context_.uniformBuffers[index];
}
uint32_t VulkanBufferManager::getUniformBufferCount() const { return static_cast<uint32_t>(context_.uniformBuffers.size()); }
void VulkanBufferManager::validateUniformBufferIndex(uint32_t index) const {
    if (index >= context_.uniformBuffers.size()) {
        throw std::out_of_range("Uniform buffer index out of range");
    }
}

VulkanBufferManager::~VulkanBufferManager() {
    if (context_.device != VK_NULL_HANDLE) {
        for (auto buffer : impl_->scratchBuffers) {
            context_.resourceManager.removeBuffer(buffer);
            Dispose::destroySingleBuffer(context_.device, buffer);
        }
        for (auto memory : impl_->scratchBufferMemories) {
            context_.resourceManager.removeMemory(memory);
            Dispose::freeSingleDeviceMemory(context_.device, memory);
        }
        if (impl_->arenaBuffer != VK_NULL_HANDLE) {
            context_.resourceManager.removeBuffer(impl_->arenaBuffer);
            Dispose::destroySingleBuffer(context_.device, impl_->arenaBuffer);
        }
        if (impl_->arenaMemory != VK_NULL_HANDLE) {
            context_.resourceManager.removeMemory(impl_->arenaMemory);
            Dispose::freeSingleDeviceMemory(context_.device, impl_->arenaMemory);
        }
        if (impl_->commandPool != VK_NULL_HANDLE) {
            Dispose::freeCommandBuffers(context_.device, impl_->commandPool, impl_->commandBuffers);
            vkDestroyCommandPool(context_.device, impl_->commandPool, nullptr);
        }
        if (impl_->timelineSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(context_.device, impl_->timelineSemaphore, nullptr);
        }
    }
}