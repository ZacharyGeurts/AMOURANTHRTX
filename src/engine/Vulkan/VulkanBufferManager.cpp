// src/engine/Vulkan/VulkanBufferManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/types.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <mutex>
#include <queue>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

// ---------------------------------------------------------------------------
//  VK_CHECK WITH MICROSECOND TIMING
// ---------------------------------------------------------------------------
#define VK_CHECK(expr, msg)                                                               \
    do {                                                                                  \
        auto start = std::chrono::high_resolution_clock::now();                           \
        VkResult _res = (expr);                                                           \
        auto end = std::chrono::high_resolution_clock::now();                             \
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start);   \
        if (_res != VK_SUCCESS) {                                                         \
            LOG_ERROR_CAT("BufferMgr", "VULKAN PANIC: {} failed -> {} (VkResult={}) [{} us]", \
                          msg, #expr, static_cast<int>(_res), dur.count());                 \
            throw std::runtime_error(std::format("{} : {}", msg, #expr));                \
        } else {                                                                          \
            LOG_TRACE_CAT("BufferMgr", "VK_CALL: {} -> OK [{} us]", #expr, dur.count());   \
        }                                                                                 \
    } while (0)

// ---------------------------------------------------------------------------
//  MEMORY TYPE FINDER
// ---------------------------------------------------------------------------
static uint32_t findMemoryType(VkPhysicalDevice pd, uint32_t filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(pd, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((filter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find memory type");
}

// ---------------------------------------------------------------------------
//  IMPL DEFINITION
// ---------------------------------------------------------------------------
struct VulkanBufferManager::Impl {
    Vulkan::Context& context;
    VkBuffer arenaBuffer = VK_NULL_HANDLE;
    VkDeviceMemory arenaMemory = VK_NULL_HANDLE;
    VkDeviceSize arenaSize = 0;
    VkDeviceSize vertexOffset = 0;
    VkDeviceSize indexOffset = 0;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBufferMemories;

    std::vector<VkBuffer> scratchBuffers;
    std::vector<VkDeviceMemory> scratchBufferMemories;
    std::vector<VkDeviceAddress> scratchBufferAddresses;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
    uint32_t transferQueueFamily = UINT32_MAX;

    VkSemaphore timelineSemaphore = VK_NULL_HANDLE;
    uint64_t nextUpdateId = 1;

    std::mutex mutex;
    std::condition_variable cv;
    std::queue<std::pair<uint64_t, std::function<void(uint64_t)>>> callbackQueue;
    std::thread callbackThread;
    bool shutdown = false;

    explicit Impl(Vulkan::Context& ctx) : context(ctx),
        callbackThread([this] { processCallbacks(); }) {}

    ~Impl() {
        { std::lock_guard<std::mutex> lk(mutex); shutdown = true; }
        cv.notify_one();
        if (callbackThread.joinable()) callbackThread.join();
    }

    void processCallbacks() {
        while (true) {
            std::unique_lock<std::mutex> lk(mutex);
            cv.wait(lk, [this] { return !callbackQueue.empty() || shutdown; });
            if (shutdown && callbackQueue.empty()) break;
            auto [id, cb] = std::move(callbackQueue.front());
            callbackQueue.pop();
            lk.unlock();
            cb(id);
        }
    }
};

// ---------------------------------------------------------------------------
//  CONSTRUCTOR
// ---------------------------------------------------------------------------
VulkanBufferManager::VulkanBufferManager(Vulkan::Context& ctx,
                                         std::span<const glm::vec3> vertices,
                                         std::span<const uint32_t> indices)
    : context_(ctx),
      vertexCount_(static_cast<uint32_t>(vertices.size())),
      indexCount_(static_cast<uint32_t>(indices.size())),
      impl_(std::make_unique<Impl>(ctx))
{
    if (ctx.device == VK_NULL_HANDLE) throw std::runtime_error("Invalid context");

    LOG_INFO_CAT("BufferMgr", "Initializing: {} verts, {} indices", vertices.size(), indices.size());

    initializeCommandPool();

    const VkDeviceSize vSize = vertices.size() * sizeof(glm::vec3);
    const VkDeviceSize iSize = indices.size() * sizeof(uint32_t);
    const VkDeviceSize total = vSize + iSize;
    const VkDeviceSize arenaSize = std::max<VkDeviceSize>(64ULL * 1024 * 1024, total * 2);

    reserveArena(arenaSize, BufferType::GEOMETRY);

    VkBuffer stagingV, stagingI;
    VkDeviceMemory memV, memI;

    createStagingBuffer(vSize, stagingV, memV);
    mapCopyUnmap(memV, vSize, vertices.data());

    createStagingBuffer(iSize, stagingI, memI);
    mapCopyUnmap(memI, iSize, indices.data());

    copyToArena(stagingV, 0, vSize);
    copyToArena(stagingI, impl_->indexOffset, iSize);

    vkDestroyBuffer(ctx.device, stagingV, nullptr);
    vkFreeMemory(ctx.device, memV, nullptr);
    vkDestroyBuffer(ctx.device, stagingI, nullptr);
    vkFreeMemory(ctx.device, memI, nullptr);

    VkBufferDeviceAddressInfo addrInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = impl_->arenaBuffer };
    vertexBufferAddress_ = vkGetBufferDeviceAddress(ctx.device, &addrInfo);
    indexBufferAddress_  = vertexBufferAddress_ + impl_->indexOffset;

    VkSemaphoreTypeCreateInfo timeline = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE, .initialValue = 0 };
    VkSemaphoreCreateInfo semInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &timeline };
    VK_CHECK(vkCreateSemaphore(ctx.device, &semInfo, nullptr, &impl_->timelineSemaphore), "Timeline semaphore");
}

// ---------------------------------------------------------------------------
//  DESTRUCTOR
// ---------------------------------------------------------------------------
VulkanBufferManager::~VulkanBufferManager() {
    vkDeviceWaitIdle(context_.device);

    for (size_t i = 0; i < impl_->scratchBuffers.size(); ++i) {
        context_.resourceManager.removeBuffer(impl_->scratchBuffers[i]);
        vkDestroyBuffer(context_.device, impl_->scratchBuffers[i], nullptr);
        context_.resourceManager.removeMemory(impl_->scratchBufferMemories[i]);
        vkFreeMemory(context_.device, impl_->scratchBufferMemories[i], nullptr);
    }

    for (size_t i = 0; i < impl_->uniformBuffers.size(); ++i) {
        context_.resourceManager.removeBuffer(impl_->uniformBuffers[i]);
        vkDestroyBuffer(context_.device, impl_->uniformBuffers[i], nullptr);
        context_.resourceManager.removeMemory(impl_->uniformBufferMemories[i]);
        vkFreeMemory(context_.device, impl_->uniformBufferMemories[i], nullptr);
    }

    if (impl_->arenaBuffer) {
        context_.resourceManager.removeBuffer(impl_->arenaBuffer);
        vkDestroyBuffer(context_.device, impl_->arenaBuffer, nullptr);
    }
    if (impl_->arenaMemory) {
        context_.resourceManager.removeMemory(impl_->arenaMemory);
        vkFreeMemory(context_.device, impl_->arenaMemory, nullptr);
    }

    if (impl_->commandPool) {
        // NOTE: No commandBuffers vector in Impl → use vkResetCommandPool instead
        VK_CHECK(vkResetCommandPool(context_.device, impl_->commandPool, 0), "Reset command pool");
        vkDestroyCommandPool(context_.device, impl_->commandPool, nullptr);
    }

    if (impl_->timelineSemaphore) {
        vkDestroySemaphore(context_.device, impl_->timelineSemaphore, nullptr);
    }
}

// ---------------------------------------------------------------------------
//  HELPER: STAGING BUFFER
// ---------------------------------------------------------------------------
void VulkanBufferManager::createStagingBuffer(VkDeviceSize size, VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    VK_CHECK(vkCreateBuffer(context_.device, &info, nullptr, &buf), "Staging buffer");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(context_.device, buf, &reqs);

    VkMemoryAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = reqs.size,
        .memoryTypeIndex = findMemoryType(context_.physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
    VK_CHECK(vkAllocateMemory(context_.device, &alloc, nullptr, &mem), "Staging memory");
    VK_CHECK(vkBindBufferMemory(context_.device, buf, mem, 0), "Bind staging");
}

// ---------------------------------------------------------------------------
//  HELPER: MAP + COPY + UNMAP
// ---------------------------------------------------------------------------
void VulkanBufferManager::mapCopyUnmap(VkDeviceMemory mem, VkDeviceSize size, const void* data) {
    void* ptr;
    VK_CHECK(vkMapMemory(context_.device, mem, 0, size, 0, &ptr), "Map");
    std::memcpy(ptr, data, size);
    vkUnmapMemory(context_.device, mem);
}

// ---------------------------------------------------------------------------
//  HELPER: COPY TO ARENA
// ---------------------------------------------------------------------------
void VulkanBufferManager::copyToArena(VkBuffer src, VkDeviceSize dstOffset, VkDeviceSize size) {
    VkCommandBuffer cb;
    VkCommandBufferAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = impl_->commandPool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
    VK_CHECK(vkAllocateCommandBuffers(context_.device, &alloc, &cb), "Alloc CB");

    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    VK_CHECK(vkBeginCommandBuffer(cb, &begin), "Begin CB");

    VkBufferCopy copy = { .srcOffset = 0, .dstOffset = dstOffset, .size = size };
    vkCmdCopyBuffer(cb, src, impl_->arenaBuffer, 1, &copy);

    VK_CHECK(vkEndCommandBuffer(cb), "End CB");

    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb };
    VK_CHECK(vkQueueSubmit(impl_->transferQueue, 1, &submit, VK_NULL_HANDLE), "Submit");
    vkQueueWaitIdle(impl_->transferQueue);

    vkFreeCommandBuffers(context_.device, impl_->commandPool, 1, &cb);
}

// ---------------------------------------------------------------------------
//  INITIALIZE COMMAND POOL
// ---------------------------------------------------------------------------
void VulkanBufferManager::initializeCommandPool() {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            impl_->transferQueueFamily = i;
            break;
        }
    }
    if (impl_->transferQueueFamily == UINT32_MAX) impl_->transferQueueFamily = context_.graphicsQueueFamilyIndex;

    VkCommandPoolCreateInfo info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = impl_->transferQueueFamily };
    VK_CHECK(vkCreateCommandPool(context_.device, &info, nullptr, &impl_->commandPool), "Command pool");

    vkGetDeviceQueue(context_.device, impl_->transferQueueFamily, 0, &impl_->transferQueue);
}

// ---------------------------------------------------------------------------
//  RESERVE ARENA
// ---------------------------------------------------------------------------
void VulkanBufferManager::reserveArena(VkDeviceSize size, BufferType type) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

    VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    VK_CHECK(vkCreateBuffer(context_.device, &info, nullptr, &impl_->arenaBuffer), "Arena buffer");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(context_.device, impl_->arenaBuffer, &reqs);

    VkMemoryAllocateFlagsInfo flags = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT };
    VkMemoryAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &flags, .allocationSize = reqs.size,
        .memoryTypeIndex = findMemoryType(context_.physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
    VK_CHECK(vkAllocateMemory(context_.device, &alloc, nullptr, &impl_->arenaMemory), "Arena memory");
    VK_CHECK(vkBindBufferMemory(context_.device, impl_->arenaBuffer, impl_->arenaMemory, 0), "Bind arena");

    impl_->arenaSize = size;
    impl_->vertexOffset = 0;
    impl_->indexOffset = (vertexCount_ * sizeof(glm::vec3) + 255) & ~255;
}

// ---------------------------------------------------------------------------
//  ASYNC UPDATE BUFFERS
// ---------------------------------------------------------------------------
VkDeviceAddress VulkanBufferManager::asyncUpdateBuffers(
    std::span<const glm::vec3> vertices,
    std::span<const uint32_t> indices,
    std::function<void(uint64_t)> callback)
{
    const uint64_t id = impl_->nextUpdateId++;
    const VkDeviceSize vSize = vertices.size() * sizeof(glm::vec3);
    const VkDeviceSize iSize = indices.size() * sizeof(uint32_t);

    VkBuffer stagingV, stagingI;
    VkDeviceMemory memV, memI;
    createStagingBuffer(vSize, stagingV, memV);
    createStagingBuffer(iSize, stagingI, memI);
    mapCopyUnmap(memV, vSize, vertices.data());
    mapCopyUnmap(memI, iSize, indices.data());

    copyToArena(stagingV, impl_->vertexOffset, vSize);
    copyToArena(stagingI, impl_->indexOffset, iSize);

    vkDestroyBuffer(context_.device, stagingV, nullptr);
    vkFreeMemory(context_.device, memV, nullptr);
    vkDestroyBuffer(context_.device, stagingI, nullptr);
    vkFreeMemory(context_.device, memI, nullptr);

    if (callback) {
        std::lock_guard<std::mutex> lk(impl_->mutex);
        impl_->callbackQueue.emplace(id, std::move(callback));
        impl_->cv.notify_one();
    }

    return vertexBufferAddress_;
}

// ---------------------------------------------------------------------------
//  CREATE UNIFORM BUFFERS
// ---------------------------------------------------------------------------
void VulkanBufferManager::createUniformBuffers(uint32_t count) {
    impl_->uniformBuffers.resize(count);
    impl_->uniformBufferMemories.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        createBuffer(sizeof(UniformBufferObject),
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     impl_->uniformBuffers[i], impl_->uniformBufferMemories[i]);
        context_.resourceManager.addBuffer(impl_->uniformBuffers[i]);
        context_.resourceManager.addMemory(impl_->uniformBufferMemories[i]);
    }
}

// ---------------------------------------------------------------------------
//  RESERVE SCRATCH POOL
// ---------------------------------------------------------------------------
void VulkanBufferManager::reserveScratchPool(VkDeviceSize size, uint32_t count) {
    impl_->scratchBuffers.resize(count);
    impl_->scratchBufferMemories.resize(count);
    impl_->scratchBufferAddresses.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        createBuffer(size,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     impl_->scratchBuffers[i], impl_->scratchBufferMemories[i]);

        VkBufferDeviceAddressInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = impl_->scratchBuffers[i] };
        impl_->scratchBufferAddresses[i] = vkGetBufferDeviceAddress(context_.device, &info);

        context_.resourceManager.addBuffer(impl_->scratchBuffers[i]);
        context_.resourceManager.addMemory(impl_->scratchBufferMemories[i]);
    }
}

// ---------------------------------------------------------------------------
//  GETTERS
// ---------------------------------------------------------------------------
VkBuffer VulkanBufferManager::getVertexBuffer() const { return impl_->arenaBuffer; }
VkBuffer VulkanBufferManager::getIndexBuffer() const { return impl_->arenaBuffer; }

VkDeviceAddress VulkanBufferManager::getVertexBufferAddress() const { return vertexBufferAddress_; }
VkDeviceAddress VulkanBufferManager::getIndexBufferAddress() const { return indexBufferAddress_; }

VkBuffer VulkanBufferManager::getScratchBuffer(uint32_t i) const { return impl_->scratchBuffers[i]; }
VkDeviceAddress VulkanBufferManager::getScratchBufferAddress(uint32_t i) const { return impl_->scratchBufferAddresses[i]; }
uint32_t VulkanBufferManager::getScratchBufferCount() const { return static_cast<uint32_t>(impl_->scratchBuffers.size()); }

// ---------------------------------------------------------------------------
//  CREATE BUFFER (GENERAL)
// ---------------------------------------------------------------------------
void VulkanBufferManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    VK_CHECK(vkCreateBuffer(context_.device, &info, nullptr, &buf), "Create buffer");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(context_.device, buf, &reqs);

    VkMemoryAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = reqs.size,
        .memoryTypeIndex = findMemoryType(context_.physicalDevice, reqs.memoryTypeBits, props) };
    VK_CHECK(vkAllocateMemory(context_.device, &alloc, nullptr, &mem), "Alloc memory");
    VK_CHECK(vkBindBufferMemory(context_.device, buf, mem, 0), "Bind memory");
}

// ---------------------------------------------------------------------------
//  MOVED FROM HEADER: NOW SAFE
// ---------------------------------------------------------------------------
VkBuffer VulkanBufferManager::getArenaBuffer() const {
    return impl_->arenaBuffer;
}

VkDeviceSize VulkanBufferManager::getVertexOffset() const {
    return impl_->vertexOffset;
}

VkDeviceSize VulkanBufferManager::getIndexOffset() const {
    return impl_->indexOffset;
}

VkDeviceAddress VulkanBufferManager::getDeviceAddress(VkBuffer buffer) const {
    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };
    return vkGetBufferDeviceAddress(context_.device, &info);
}