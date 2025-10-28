// AMOURANTH RTX Engine, October 2025 - Vulkan buffer management.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, Vulkan_init.hpp, ue_init.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows.
// Optimized for high-end GPUs with 8 GB VRAM (e.g., NVIDIA RTX 3070, AMD RX 6800).
// Fixes: Zero-initialize scaled padding in staging/arena to prevent UB/device loss in RT/AS reads.

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/types.hpp"
#include "engine/Dispose.hpp"
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
#include <mutex>
#include <condition_variable>
#include <list>
#include <chrono>
#include <queue>

namespace {

// Scaling factors optimized for 8 GB VRAM
constexpr VkDeviceSize VERTEX_BUFFER_SCALE_FACTOR = 4;
constexpr VkDeviceSize INDEX_BUFFER_SCALE_FACTOR  = 4;
constexpr VkDeviceSize SCRATCH_BUFFER_SCALE_FACTOR = 4;
constexpr VkDeviceSize UNIFORM_BUFFER_DEFAULT_SIZE = 2048;
constexpr uint32_t DEFAULT_UNIFORM_BUFFER_COUNT = 64;
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
constexpr VkDeviceSize ARENA_DEFAULT_SIZE = 16 * 1024 * 1024;
constexpr VkDeviceSize ARENA_GROWTH_FACTOR = 2;
constexpr uint32_t COMMAND_BUFFER_POOL_SIZE = 32;
constexpr uint32_t SCRATCH_BUFFER_POOL_COUNT = 4;
constexpr uint64_t FENCE_TIMEOUT = 5'000'000'000ULL;
constexpr size_t SCRATCH_CACHE_MAX_SIZE = 128;

bool isMeshShaderSupported(VkPhysicalDevice pd) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, exts.data());
    for (const auto& e : exts)
        if (std::strcmp(e.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0)
            return true;
    return false;
}

bool isDescriptorIndexingSupported(VkPhysicalDevice pd) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, exts.data());
    for (const auto& e : exts)
        if (std::strcmp(e.extensionName, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0)
            return true;
    return false;
}

void cleanupResources(VkDevice dev, VkCommandPool pool,
                      VkBuffer buf, VkDeviceMemory mem,
                      VkCommandBuffer cb = VK_NULL_HANDLE,
                      VkFence fence = VK_NULL_HANDLE) {
    if (cb != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
        std::vector<VkCommandBuffer> vec = {cb};
        Dispose::freeCommandBuffers(dev, pool, vec);
    }
    if (buf != VK_NULL_HANDLE) Dispose::destroySingleBuffer(dev, buf);
    if (mem != VK_NULL_HANDLE) Dispose::freeSingleDeviceMemory(dev, mem);
    if (fence != VK_NULL_HANDLE) vkDestroyFence(dev, fence, nullptr);
}

} // namespace

struct VulkanBufferManager::Impl {
    Vulkan::Context& context;
    VkBuffer arenaBuffer = VK_NULL_HANDLE;
    VkDeviceMemory arenaMemory = VK_NULL_HANDLE;
    VkDeviceSize arenaSize = 0;
    VkDeviceSize vertexOffset = 0;
    VkDeviceSize indexOffset = 0;
    VkDeviceSize meshletOffset = 0;
    std::vector<VkDeviceSize> uniformBufferOffsets;
    std::unordered_map<uint64_t, VkDeviceSize> scratchSizeCache;
    std::list<uint64_t> scratchCacheOrder;
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
    std::mutex mutex;
    std::condition_variable cv;
    std::queue<std::pair<uint64_t, std::function<void(uint64_t)>>> callbackQueue;
    std::thread callbackThread;
    bool shutdown = false;

    Impl(Vulkan::Context& ctx) : context(ctx), callbackThread([this] { processCallbacks(); }) {}
    ~Impl() {
        { std::lock_guard<std::mutex> l(mutex); shutdown = true; }
        cv.notify_one();
        if (callbackThread.joinable()) callbackThread.join();
    }

    void processCallbacks() {
        while (true) {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [this] { return !callbackQueue.empty() || shutdown; });
            if (shutdown && callbackQueue.empty()) break;
            auto [id, cb] = std::move(callbackQueue.front());
            callbackQueue.pop();
            lock.unlock();

            uint64_t value = 0;
            while (value < id) {
                vkGetSemaphoreCounterValue(context.device, timelineSemaphore, &value);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            cb(id);
        }
    }

    void evictScratchCache() {
        while (scratchCacheOrder.size() > SCRATCH_CACHE_MAX_SIZE) {
            uint64_t key = scratchCacheOrder.front();
            scratchCacheOrder.pop_front();
            scratchSizeCache.erase(key);
        }
    }
};

VulkanBufferManager::VulkanBufferManager(Vulkan::Context& context,
                                         std::span<const glm::vec3> vertices,
                                         std::span<const uint32_t> indices)
    : context_(context),
      vertexCount_(0),
      indexCount_(0),
      vertexBufferAddress_(0),
      indexBufferAddress_(0),
      scratchBufferAddress_(0),
      impl_(std::make_unique<Impl>(context_))
{
    if (context_.device == VK_NULL_HANDLE || context_.physicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("Invalid Vulkan context");

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProps);
    VkDeviceSize devLocal = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i)
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            devLocal += memProps.memoryHeaps[i].size;
    if (devLocal < 8ULL * 1024 * 1024 * 1024)
        throw std::runtime_error("Insufficient device local memory");

    if (vertices.empty() || indices.empty())
        throw std::invalid_argument("Vertex/index data cannot be empty");
    vertexCount_ = static_cast<uint32_t>(vertices.size());
    indexCount_  = static_cast<uint32_t>(indices.size());
    if (indexCount_ < 3 || indexCount_ % 3 != 0)
        throw std::invalid_argument("Invalid triangle geometry");

    impl_->useMeshShaders       = isMeshShaderSupported(context_.physicalDevice);
    impl_->useDescriptorIndexing = isDescriptorIndexingSupported(context_.physicalDevice);

    initializeCommandPool();

    VkDeviceSize vSize = sizeof(glm::vec3) * vertices.size() * VERTEX_BUFFER_SCALE_FACTOR;
    VkDeviceSize iSize = sizeof(uint32_t)  * indices.size()  * INDEX_BUFFER_SCALE_FACTOR;
    VkDeviceSize total = vSize + iSize;
    VkDeviceSize arenaSize = std::max(ARENA_DEFAULT_SIZE, total * 2);
    if (arenaSize > devLocal / 2)
        throw std::runtime_error("Arena exceeds VRAM");
    reserveArena(arenaSize, BufferType::GEOMETRY);

    vertexBufferAddress_ = asyncUpdateBuffers(vertices, indices, nullptr);
    indexBufferAddress_  = vertexBufferAddress_ + vSize;

    createUniformBuffers(MAX_FRAMES_IN_FLIGHT);

    VkDeviceSize scratch = getScratchSize(vertexCount_, indexCount_) * SCRATCH_BUFFER_SCALE_FACTOR;
    if (scratch > devLocal / 4)
        throw std::runtime_error("Scratch exceeds VRAM");
    reserveScratchPool(scratch, SCRATCH_BUFFER_POOL_COUNT);
    scratchBufferAddress_ = impl_->scratchBufferAddresses[0];
}

VulkanBufferManager::~VulkanBufferManager() {
    if (context_.device == VK_NULL_HANDLE) return;

    for (auto b : impl_->scratchBuffers) {
        context_.resourceManager.removeBuffer(b);
        Dispose::destroySingleBuffer(context_.device, b);
    }
    for (auto m : impl_->scratchBufferMemories) {
        context_.resourceManager.removeMemory(m);
        Dispose::freeSingleDeviceMemory(context_.device, m);
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
    if (impl_->timelineSemaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(context_.device, impl_->timelineSemaphore, nullptr);
}

void VulkanBufferManager::initializeCommandPool() {
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &qCount, qProps.data());

    impl_->transferQueueFamily = UINT32_MAX;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (qProps[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            impl_->transferQueueFamily = i; break;
        }
    }
    if (impl_->transferQueueFamily == UINT32_MAX) {
        for (uint32_t i = 0; i < qCount; ++i) {
            if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                impl_->transferQueueFamily = i; break;
            }
        }
    }
    if (impl_->transferQueueFamily == UINT32_MAX)
        throw std::runtime_error("No transfer queue");

    VkCommandPoolCreateInfo pci{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = impl_->transferQueueFamily
    };
    if (vkCreateCommandPool(context_.device, &pci, nullptr, &impl_->commandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");

    impl_->commandBuffers.resize(COMMAND_BUFFER_POOL_SIZE);
    VkCommandBufferAllocateInfo ai{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = impl_->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = COMMAND_BUFFER_POOL_SIZE
    };
    if (vkAllocateCommandBuffers(context_.device, &ai, impl_->commandBuffers.data()) != VK_SUCCESS) {
        vkDestroyCommandPool(context_.device, impl_->commandPool, nullptr);
        throw std::runtime_error("Failed to allocate command buffers");
    }

    vkGetDeviceQueue(context_.device, impl_->transferQueueFamily, 0, &impl_->transferQueue);

    VkSemaphoreCreateInfo sci{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (vkCreateSemaphore(context_.device, &sci, nullptr, &impl_->timelineSemaphore) != VK_SUCCESS) {
        vkDestroyCommandPool(context_.device, impl_->commandPool, nullptr);
        throw std::runtime_error("Failed to create timeline semaphore");
    }
}

void VulkanBufferManager::reserveArena(VkDeviceSize size, BufferType type) {
    if (impl_->arenaBuffer != VK_NULL_HANDLE && size <= impl_->arenaSize) return;
    size = std::max(size, impl_->arenaSize * ARENA_GROWTH_FACTOR);

    if (impl_->arenaBuffer != VK_NULL_HANDLE) {
        context_.resourceManager.removeBuffer(impl_->arenaBuffer);
        Dispose::destroySingleBuffer(context_.device, impl_->arenaBuffer);
        context_.resourceManager.removeMemory(impl_->arenaMemory);
        Dispose::freeSingleDeviceMemory(context_.device, impl_->arenaMemory);
        impl_->arenaBuffer = VK_NULL_HANDLE;
        impl_->arenaMemory = VK_NULL_HANDLE;
    }

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (type == BufferType::GEOMETRY) {
        usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        if (impl_->useMeshShaders) usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    } else if (type == BufferType::UNIFORM) {
        usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    VkPhysicalDeviceProperties2 props2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
    props2.pNext = &accelProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    size = (size + props2.properties.limits.minStorageBufferOffsetAlignment - 1) &
           ~(props2.properties.limits.minStorageBufferOffsetAlignment - 1);

    VkMemoryAllocateFlagsInfo flagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice, size,
        usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        impl_->arenaBuffer, impl_->arenaMemory, &flagsInfo, context_.resourceManager
    );
    if (!impl_->arenaBuffer || !impl_->arenaMemory)
        throw std::runtime_error("Failed to create arena buffer");
    impl_->arenaSize = size;

    // FIX: Zero-initialize arena to prevent garbage in padding
    VkCommandBuffer zeroCmd = getCommandBuffer(BufferOperation::TRANSFER);
    VkCommandBufferBeginInfo beginZero{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(zeroCmd, &beginZero);
    vkCmdFillBuffer(zeroCmd, impl_->arenaBuffer, 0, size, 0);  // Fill with 0
    vkEndCommandBuffer(zeroCmd);
    VkSubmitInfo submitZero{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &zeroCmd};
    vkQueueSubmit(impl_->transferQueue, 1, &submitZero, VK_NULL_HANDLE);
    vkQueueWaitIdle(impl_->transferQueue);  // Sync zeroing
}

VkDeviceAddress VulkanBufferManager::asyncUpdateBuffers(
    std::span<const glm::vec3> vertices,
    std::span<const uint32_t> indices,
    std::function<void(uint64_t)> callback)
{
    VkDeviceSize unscaledVSize = sizeof(glm::vec3) * vertices.size();
    VkDeviceSize unscaledISize = sizeof(uint32_t)  * indices.size();
    VkDeviceSize vSize = unscaledVSize * VERTEX_BUFFER_SCALE_FACTOR;
    VkDeviceSize iSize = unscaledISize * INDEX_BUFFER_SCALE_FACTOR;
    VkDeviceSize total = vSize + iSize;
    if (total > impl_->arenaSize) reserveArena(total, BufferType::GEOMETRY);

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkCommandBuffer cmd = getCommandBuffer(BufferOperation::TRANSFER);

    VkBufferCreateInfo sci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = total,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };
    vkCreateBuffer(context_.device, &sci, nullptr, &staging);
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(context_.device, staging, &req);
    uint32_t type = VulkanInitializer::findMemoryType(
        context_.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo mai{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                             .allocationSize = req.size,
                             .memoryTypeIndex = type};
    vkAllocateMemory(context_.device, &mai, nullptr, &stagingMem);
    vkBindBufferMemory(context_.device, staging, stagingMem, 0);

    void* map;
    vkMapMemory(context_.device, stagingMem, 0, total, 0, &map);
    
    // FIX: Zero full scaled staging to prevent garbage padding
    memset(map, 0, total);
    
    // Copy original data to start (padding remains zero)
    memcpy(map, vertices.data(), unscaledVSize);
    memcpy(static_cast<char*>(map) + unscaledVSize, indices.data(), unscaledISize);
    
    vkUnmapMemory(context_.device, stagingMem);

    VkFenceCreateInfo fci{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(context_.device, &fci, nullptr, &fence);

    VkCommandBufferBeginInfo cbi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(cmd, &cbi);
    
    // FIX: Copy FULL scaled sizes (matches alloc, with zeroed padding)
    VkBufferCopy vcopy{.srcOffset = 0, .dstOffset = impl_->vertexOffset, .size = vSize};
    VkBufferCopy icopy{.srcOffset = vSize, .dstOffset = impl_->indexOffset, .size = iSize};
    vkCmdCopyBuffer(cmd, staging, impl_->arenaBuffer, 1, &vcopy);
    vkCmdCopyBuffer(cmd, staging, impl_->arenaBuffer, 1, &icopy);

    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                         VK_ACCESS_INDEX_READ_BIT |
                         VK_ACCESS_SHADER_READ_BIT
    };
    if (impl_->useMeshShaders) barrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR |
                         (impl_->useMeshShaders ? VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT : 0),
                         0, 1, &barrier, 0, nullptr, 0, nullptr);
    vkEndCommandBuffer(cmd);

    uint64_t updateId = impl_->nextUpdateId++;
    VkTimelineSemaphoreSubmitInfo tsi{
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &updateId
    };
    VkSubmitInfo si{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = &tsi,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &impl_->timelineSemaphore
    };
    vkQueueSubmit(impl_->transferQueue, 1, &si, fence);

    if (vkWaitForFences(context_.device, 1, &fence, VK_TRUE, FENCE_TIMEOUT) != VK_SUCCESS)
        throw std::runtime_error("Fence timeout");

    impl_->updateSignals[updateId] = updateId;
    if (callback) {
        { std::lock_guard<std::mutex> l(impl_->mutex);
          impl_->callbackQueue.emplace(updateId, std::move(callback));
        }
        impl_->cv.notify_one();
    }

    cleanupResources(context_.device, impl_->commandPool, staging, stagingMem, cmd, fence);

    // FIX: Update offsets to full scaled for next use
    impl_->vertexOffset += vSize;
    impl_->indexOffset  += iSize;

    vertexCount_ = static_cast<uint32_t>(vertices.size());
    indexCount_  = static_cast<uint32_t>(indices.size());
    return VulkanInitializer::getBufferDeviceAddress(context_.device, impl_->arenaBuffer) + impl_->vertexOffset - vSize;  // Return start of vertices
}

void VulkanBufferManager::createUniformBuffers(uint32_t count) {
    if (count == 0) return;
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

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(context_.physicalDevice, &props);
    VkDeviceSize bufSize = (sizeof(UniformBufferObject) + sizeof(int) +
                            UNIFORM_BUFFER_DEFAULT_SIZE +
                            props.limits.minUniformBufferOffsetAlignment - 1) &
                           ~(props.limits.minUniformBufferOffsetAlignment - 1);
    VkDeviceSize total = bufSize * count;

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProps);
    VkDeviceSize devLocal = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i)
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            devLocal += memProps.memoryHeaps[i].size;
    if (total > devLocal / 8) throw std::runtime_error("Uniforms exceed VRAM");

    context_.uniformBuffers.resize(count);
    context_.uniformBufferMemories.resize(count);
    impl_->uniformBufferOffsets.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VulkanInitializer::createBuffer(
            context_.device, context_.physicalDevice, bufSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            context_.uniformBuffers[i], context_.uniformBufferMemories[i],
            nullptr, context_.resourceManager
        );
        impl_->uniformBufferOffsets[i] = 0;
    }
}

void VulkanBufferManager::configureUniformBuffers(uint32_t count, VkDeviceSize size_per_buffer) {
    if (count == 0) return;
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

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(context_.physicalDevice, &props);
    size_per_buffer = (size_per_buffer + props.limits.minUniformBufferOffsetAlignment - 1) &
                      ~(props.limits.minUniformBufferOffsetAlignment - 1);
    VkDeviceSize total = size_per_buffer * count;

    reserveArena(total, BufferType::UNIFORM);
    context_.uniformBuffers.resize(count);
    context_.uniformBufferMemories.resize(count);
    impl_->uniformBufferOffsets.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        context_.uniformBuffers[i] = impl_->arenaBuffer;
        context_.uniformBufferMemories[i] = impl_->arenaMemory;
        impl_->uniformBufferOffsets[i] = i * size_per_buffer;
    }
}

void VulkanBufferManager::enableMeshShaders(bool enable, const MeshletConfig&) {
    impl_->useMeshShaders = enable && isMeshShaderSupported(context_.physicalDevice);
    if (impl_->useMeshShaders)
        impl_->meshletOffset = impl_->indexOffset +
            (sizeof(uint32_t) * indexCount_ * INDEX_BUFFER_SCALE_FACTOR);
}

void VulkanBufferManager::addMeshletBatch(std::span<const MeshletData> meshlets) {
    if (!impl_->useMeshShaders) throw std::runtime_error("Mesh shaders not enabled");

    VkDeviceSize total = 0;
    for (const auto& m : meshlets) total += m.size;
    if (impl_->meshletOffset + total > impl_->arenaSize)
        reserveArena(impl_->arenaSize + total, BufferType::GEOMETRY);

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkCommandBuffer cmd = getCommandBuffer(BufferOperation::TRANSFER);

    VkBufferCreateInfo sci{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                           .size = total,
                           .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
    vkCreateBuffer(context_.device, &sci, nullptr, &staging);
    VkMemoryRequirements req; vkGetBufferMemoryRequirements(context_.device, staging, &req);
    uint32_t type = VulkanInitializer::findMemoryType(
        context_.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo mai{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                             .allocationSize = req.size,
                             .memoryTypeIndex = type};
    vkAllocateMemory(context_.device, &mai, nullptr, &stagingMem);
    vkBindBufferMemory(context_.device, staging, stagingMem, 0);

    void* map; vkMapMemory(context_.device, stagingMem, 0, total, 0, &map);
    
    // FIX: Zero full staging
    memset(map, 0, total);
    
    // Copy data
    VkDeviceSize off = 0;
    for (const auto& m : meshlets) {
        memcpy(static_cast<char*>(map) + off, m.data, m.size);
        off += m.size;
    }
    vkUnmapMemory(context_.device, stagingMem);

    VkFenceCreateInfo fci{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(context_.device, &fci, nullptr, &fence);

    VkCommandBufferBeginInfo cbi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(cmd, &cbi);
    VkBufferCopy copy{.srcOffset = 0, .dstOffset = impl_->meshletOffset, .size = total};
    vkCmdCopyBuffer(cmd, staging, impl_->arenaBuffer, 1, &copy);
    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    vkQueueSubmit(impl_->transferQueue, 1, &si, fence);
    if (vkWaitForFences(context_.device, 1, &fence, VK_TRUE, FENCE_TIMEOUT) != VK_SUCCESS)
        throw std::runtime_error("Fence timeout (meshlet)");

    cleanupResources(context_.device, impl_->commandPool, staging, stagingMem, cmd, fence);
    impl_->meshletOffset += total;
}

VkCommandBuffer VulkanBufferManager::getCommandBuffer(BufferOperation) {
    VkCommandBuffer cb = impl_->commandBuffers[impl_->nextCommandBuffer];
    impl_->nextCommandBuffer = (impl_->nextCommandBuffer + 1) % COMMAND_BUFFER_POOL_SIZE;
    vkResetCommandBuffer(cb, 0);
    return cb;
}

void VulkanBufferManager::batchTransferAsync(
    std::span<const BufferCopy> copies,
    std::function<void(uint64_t)> callback)
{
    VkDeviceSize total = 0;
    for (const auto& c : copies) total += c.size;
    if (total > impl_->arenaSize) reserveArena(total, BufferType::GEOMETRY);

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkCommandBuffer cmd = getCommandBuffer(BufferOperation::TRANSFER);

    VkBufferCreateInfo sci{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                           .size = total,
                           .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
    vkCreateBuffer(context_.device, &sci, nullptr, &staging);
    VkMemoryRequirements req; vkGetBufferMemoryRequirements(context_.device, staging, &req);
    uint32_t type = VulkanInitializer::findMemoryType(
        context_.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo mai{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                             .allocationSize = req.size,
                             .memoryTypeIndex = type};
    vkAllocateMemory(context_.device, &mai, nullptr, &stagingMem);
    vkBindBufferMemory(context_.device, staging, stagingMem, 0);

    void* map; vkMapMemory(context_.device, stagingMem, 0, total, 0, &map);
    
    // FIX: Zero full staging
    memset(map, 0, total);
    
    VkDeviceSize off = 0;
    for (const auto& c : copies) {
        memcpy(static_cast<char*>(map) + off, c.data, c.size);
        off += c.size;
    }
    vkUnmapMemory(context_.device, stagingMem);

    VkFenceCreateInfo fci{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(context_.device, &fci, nullptr, &fence);

    VkCommandBufferBeginInfo cbi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(cmd, &cbi);

    std::vector<VkBufferCopy> vkCopies(copies.size());
    off = 0;
    for (size_t i = 0; i < copies.size(); ++i) {
        vkCopies[i] = {off, copies[i].dstOffset, copies[i].size};
        off += copies[i].size;
    }
    vkCmdCopyBuffer(cmd, staging, impl_->arenaBuffer, static_cast<uint32_t>(vkCopies.size()), vkCopies.data());

    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                         VK_ACCESS_INDEX_READ_BIT |
                         VK_ACCESS_SHADER_READ_BIT
    };
    if (impl_->useMeshShaders) barrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR |
                         (impl_->useMeshShaders ? VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT : 0),
                         0, 1, &barrier, 0, nullptr, 0, nullptr);
    vkEndCommandBuffer(cmd);

    uint64_t updateId = impl_->nextUpdateId++;
    VkTimelineSemaphoreSubmitInfo tsi{
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &updateId
    };
    VkSubmitInfo si{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = &tsi,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &impl_->timelineSemaphore
    };
    vkQueueSubmit(impl_->transferQueue, 1, &si, fence);
    if (vkWaitForFences(context_.device, 1, &fence, VK_TRUE, FENCE_TIMEOUT) != VK_SUCCESS)
        throw std::runtime_error("Fence timeout (batch)");

    impl_->updateSignals[updateId] = updateId;
    if (callback) {
        { std::lock_guard<std::mutex> l(impl_->mutex);
          impl_->callbackQueue.emplace(updateId, std::move(callback));
        }
        impl_->cv.notify_one();
    }
    cleanupResources(context_.device, impl_->commandPool, staging, stagingMem, cmd, fence);
}

void VulkanBufferManager::reserveScratchPool(VkDeviceSize size_per_buffer, uint32_t count) {
    for (auto b : impl_->scratchBuffers) {
        context_.resourceManager.removeBuffer(b);
        Dispose::destroySingleBuffer(context_.device, b);
    }
    for (auto m : impl_->scratchBufferMemories) {
        context_.resourceManager.removeMemory(m);
        Dispose::freeSingleDeviceMemory(context_.device, m);
    }
    impl_->scratchBuffers.clear();
    impl_->scratchBufferMemories.clear();
    impl_->scratchBufferAddresses.clear();

    VkPhysicalDeviceProperties2 props2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
    props2.pNext = &accelProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    size_per_buffer = (size_per_buffer + accelProps.minAccelerationStructureScratchOffsetAlignment - 1) &
                      ~(accelProps.minAccelerationStructureScratchOffsetAlignment - 1);

    VkMemoryAllocateFlagsInfo flagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };
    impl_->scratchBuffers.resize(count);
    impl_->scratchBufferMemories.resize(count);
    impl_->scratchBufferAddresses.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VulkanInitializer::createBuffer(
            context_.device, context_.physicalDevice, size_per_buffer,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            impl_->scratchBuffers[i], impl_->scratchBufferMemories[i],
            &flagsInfo, context_.resourceManager
        );
        impl_->scratchBufferAddresses[i] =
            VulkanInitializer::getBufferDeviceAddress(context_.device, impl_->scratchBuffers[i]);
    }
    context_.scratchBuffer = impl_->scratchBuffers[0];
    context_.scratchBufferMemory = impl_->scratchBufferMemories[0];
    scratchBufferAddress_ = impl_->scratchBufferAddresses[0];
}

VkDeviceSize VulkanBufferManager::getScratchSize(uint32_t vertexCount, uint32_t indexCount) {
    uint64_t key = (static_cast<uint64_t>(vertexCount) << 32) | indexCount;
    {
        std::lock_guard<std::mutex> l(impl_->mutex);
        auto it = impl_->scratchSizeCache.find(key);
        if (it != impl_->scratchSizeCache.end()) {
            auto ord = std::find(impl_->scratchCacheOrder.begin(), impl_->scratchCacheOrder.end(), key);
            if (ord != impl_->scratchCacheOrder.end()) {
                impl_->scratchCacheOrder.erase(ord);
            }
            impl_->scratchCacheOrder.push_back(key);
            return it->second;
        }
    }

    if (indexCount < 3 || indexCount % 3 != 0 || vertexCount == 0)
        throw std::invalid_argument("Invalid geometry for AS build");

    auto fn = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetAccelerationStructureBuildSizesKHR"));
    if (!fn) throw std::runtime_error("Missing vkGetAccelerationStructureBuildSizesKHR");

    VkAccelerationStructureGeometryTrianglesDataKHR tri{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = {.deviceAddress = vertexBufferAddress_},
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = vertexCount - 1,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = {.deviceAddress = indexBufferAddress_}
    };
    VkAccelerationStructureGeometryKHR geom{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {.triangles = tri},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    VkAccelerationStructureBuildGeometryInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                 VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geom
    };
    uint32_t primCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizes{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    fn(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
       &info, &primCount, &sizes);

    {
        std::lock_guard<std::mutex> l(impl_->mutex);
        impl_->scratchSizeCache[key] = sizes.buildScratchSize;
        impl_->scratchCacheOrder.push_back(key);
        impl_->evictScratchCache();
    }
    return sizes.buildScratchSize;
}

void VulkanBufferManager::createScratchBuffer(VkDeviceSize size) {
    reserveScratchPool(size, 1);
}

void VulkanBufferManager::batchDescriptorUpdate(std::span<const DescriptorUpdate> updates, uint32_t maxBindings) {
    if (!impl_->useDescriptorIndexing && maxBindings > 32)
        maxBindings = 32;

    std::vector<VkWriteDescriptorSet> writes(updates.size());
    std::vector<VkDescriptorBufferInfo> infos(updates.size());
    for (size_t i = 0; i < updates.size(); ++i) {
        if (updates[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            if (updates[i].bufferIndex >= context_.uniformBuffers.size())
                throw std::runtime_error("Invalid uniform buffer index");
            infos[i] = {
                .buffer = context_.uniformBuffers[updates[i].bufferIndex],
                .offset = impl_->uniformBufferOffsets[updates[i].bufferIndex],
                .range = updates[i].size
            };
        }
        writes[i] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = updates[i].descriptorSet,
            .dstBinding = updates[i].binding,
            .descriptorCount = 1,
            .descriptorType = updates[i].type,
            .pBufferInfo = &infos[i]
        };
    }
    vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanBufferManager::updateDescriptorSet(VkDescriptorSet ds, uint32_t binding,
                                              uint32_t count, VkDescriptorType type) const {
    if (ds == VK_NULL_HANDLE) throw std::invalid_argument("Null descriptor set");
    if (count > context_.uniformBuffers.size()) throw std::runtime_error("Not enough uniform buffers");

    std::vector<VkDescriptorBufferInfo> infos(count);
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(context_.physicalDevice, &props);
    VkDeviceSize bufSize = (sizeof(UniformBufferObject) + sizeof(int) +
                            UNIFORM_BUFFER_DEFAULT_SIZE +
                            props.limits.minUniformBufferOffsetAlignment - 1) &
                           ~(props.limits.minUniformBufferOffsetAlignment - 1);

    for (uint32_t i = 0; i < count; ++i) {
        infos[i] = {
            .buffer = context_.uniformBuffers[i],
            .offset = impl_->uniformBufferOffsets[i],
            .range = bufSize
        };
    }

    std::vector<VkWriteDescriptorSet> writes(count);
    for (uint32_t i = 0; i < count; ++i) {
        writes[i] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds,
            .dstBinding = binding + i,
            .descriptorCount = 1,
            .descriptorType = type,
            .pBufferInfo = &infos[i]
        };
    }
    vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanBufferManager::prepareDescriptorBufferInfo(std::vector<VkDescriptorBufferInfo>& out, uint32_t count) const {
    if (count > context_.uniformBuffers.size()) throw std::runtime_error("Not enough uniform buffers");
    out.resize(count);
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(context_.physicalDevice, &props);
    VkDeviceSize bufSize = (sizeof(UniformBufferObject) + sizeof(int) +
                            UNIFORM_BUFFER_DEFAULT_SIZE +
                            props.limits.minUniformBufferOffsetAlignment - 1) &
                           ~(props.limits.minUniformBufferOffsetAlignment - 1);
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = {
            .buffer = context_.uniformBuffers[i],
            .offset = impl_->uniformBufferOffsets[i],
            .range = bufSize
        };
    }
}

bool VulkanBufferManager::checkUpdateStatus(uint64_t updateId) const {
    auto it = impl_->updateSignals.find(updateId);
    if (it == impl_->updateSignals.end()) return true;
    uint64_t val = 0;
    vkGetSemaphoreCounterValue(context_.device, impl_->timelineSemaphore, &val);
    return val >= it->second;
}

VkDeviceMemory VulkanBufferManager::getVertexBufferMemory() const { return impl_->arenaMemory; }
VkDeviceMemory VulkanBufferManager::getIndexBufferMemory() const { return impl_->arenaMemory; }
VkDeviceMemory VulkanBufferManager::getUniformBufferMemory(uint32_t i) const {
    validateUniformBufferIndex(i);
    return context_.uniformBufferMemories[i];
}
VkDeviceAddress VulkanBufferManager::getVertexBufferAddress() const { return vertexBufferAddress_; }
VkDeviceAddress VulkanBufferManager::getIndexBufferAddress() const { return indexBufferAddress_; }
VkDeviceAddress VulkanBufferManager::getScratchBufferAddress() const { return scratchBufferAddress_; }
VkBuffer VulkanBufferManager::getVertexBuffer() const { return impl_->arenaBuffer; }
VkBuffer VulkanBufferManager::getIndexBuffer() const { return impl_->arenaBuffer; }
VkBuffer VulkanBufferManager::getMeshletBuffer(VkDeviceSize& off) const {
    off = impl_->meshletOffset;
    return impl_->arenaBuffer;
}
uint32_t VulkanBufferManager::getVertexCount() const { return vertexCount_; }
uint32_t VulkanBufferManager::getIndexCount() const { return indexCount_; }
VkBuffer VulkanBufferManager::getUniformBuffer(uint32_t i) const {
    validateUniformBufferIndex(i);
    return context_.uniformBuffers[i];
}
uint32_t VulkanBufferManager::getUniformBufferCount() const {
    return static_cast<uint32_t>(context_.uniformBuffers.size());
}
void VulkanBufferManager::validateUniformBufferIndex(uint32_t i) const {
    if (i >= context_.uniformBuffers.size())
        throw std::out_of_range("Uniform buffer index out of range");
}