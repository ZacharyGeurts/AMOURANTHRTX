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
#include <list>
#include <mutex>
#include <queue>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

// ---------------------------------------------------------------------------
//  VK_CHECK with EXPLOSIVE ERROR
// ---------------------------------------------------------------------------
#define VK_CHECK(expr, msg)                                                               \
    do {                                                                                  \
        VkResult _res = (expr);                                                           \
        if (_res != VK_SUCCESS) {                                                         \
            LOG_ERROR_CAT("BufferMgr", "VULKAN PANIC: {} failed -> {} (VkResult={})",       \
                          msg, #expr, static_cast<int>(_res));                            \
            throw std::runtime_error(std::format("{} : {}", msg, #expr));                \
        }                                                                                 \
    } while (0)

// ---------------------------------------------------------------------------
//  Pointer -> Hex String
// ---------------------------------------------------------------------------
inline std::string ptr_to_hex(const void* p) {
    return std::format("{:p}", p);
}

// ---------------------------------------------------------------------------
//  RAII ScopeGuard
// ---------------------------------------------------------------------------
template <class F>
class ScopeGuard {
public:
    explicit ScopeGuard(F&& f) noexcept : f_(std::move(f)), active_(true) {}
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator = (const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard&& o) noexcept : f_(std::move(o.f_)), active_(o.active_) { o.active_ = false; }
    ~ScopeGuard() { if (active_) f_(); }
    void release() noexcept { active_ = false; }

private:
    F    f_;
    bool active_;
};

template <class F>
ScopeGuard<F> make_scope_guard(F&& f) noexcept { return ScopeGuard<F>(std::forward<F>(f)); }

// ---------------------------------------------------------------------------
//  Memory Barrier
// ---------------------------------------------------------------------------
inline void insertMemoryBarrier(VkCommandBuffer cb,
                                VkPipelineStageFlags srcStage,
                                VkPipelineStageFlags dstStage,
                                VkAccessFlags srcAccess,
                                VkAccessFlags dstAccess)
{
    const VkMemoryBarrier b{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = srcAccess,
        .dstAccessMask = dstAccess
    };
    vkCmdPipelineBarrier(cb, srcStage, dstStage, 0, 1, &b, 0, nullptr, 0, nullptr);
}

// ---------------------------------------------------------------------------
//  Extension Support
// ---------------------------------------------------------------------------
static bool isExtensionSupported(VkPhysicalDevice pd, const char* name) {
    uint32_t cnt = 0;
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &cnt, nullptr);
    std::vector<VkExtensionProperties> exts(cnt);
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &cnt, exts.data());
    const bool supported = std::any_of(exts.begin(), exts.end(),
        [name](const auto& e) { return std::strcmp(e.extensionName, name) == 0; });
    LOG_DEBUG_CAT("BufferMgr", "Extension {}: {}", name, supported ? "SUPPORTED" : "MISSING");
    return supported;
}

// ---------------------------------------------------------------------------
//  Temp Cleanup -- MARKED UNUSED
// ---------------------------------------------------------------------------
[[maybe_unused]]
static void cleanupTmpResources(VkDevice dev, VkCommandPool pool,
                                VkBuffer buf, VkDeviceMemory mem,
                                VkCommandBuffer cb = VK_NULL_HANDLE,
                                VkFence fence = VK_NULL_HANDLE)
{
    if (cb != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
        std::vector<VkCommandBuffer> vec = {cb};
        Dispose::freeCommandBuffers(dev, pool, vec);
        LOG_DEBUG_CAT("BufferMgr", "Freed transient command buffer");
    }
    if (buf != VK_NULL_HANDLE) {
        Dispose::destroySingleBuffer(dev, buf);
        LOG_DEBUG_CAT("BufferMgr", "Destroyed staging buffer {}", ptr_to_hex(buf));
    }
    if (mem != VK_NULL_HANDLE) {
        Dispose::freeSingleDeviceMemory(dev, mem);
        LOG_DEBUG_CAT("BufferMgr", "Freed staging memory {}", ptr_to_hex(mem));
    }
    if (fence != VK_NULL_HANDLE) {
        vkDestroyFence(dev, fence, nullptr);
        LOG_DEBUG_CAT("BufferMgr", "Destroyed upload fence");
    }
}

// ---------------------------------------------------------------------------
//  Impl
// ---------------------------------------------------------------------------
struct VulkanBufferManager::Impl {
    Vulkan::Context&                     context;
    VkBuffer                             arenaBuffer          = VK_NULL_HANDLE;
    VkDeviceMemory                       arenaMemory          = VK_NULL_HANDLE;
    VkDeviceSize                         arenaSize            = 0;
    VkDeviceSize                         vertexOffset         = 0;
    VkDeviceSize                         indexOffset          = 0;
    VkDeviceSize                         meshletOffset        = 0;
    std::vector<VkDeviceSize>            uniformBufferOffsets;
    std::unordered_map<uint64_t, VkDeviceSize> scratchSizeCache;
    std::list<uint64_t>                  scratchCacheOrder;
    std::vector<VkBuffer>                scratchBuffers;
    std::vector<VkDeviceMemory>          scratchBufferMemories;
    std::vector<VkDeviceAddress>         scratchBufferAddresses;
    VkCommandPool                        commandPool          = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer>         commandBuffers;
    uint32_t                             nextCommandBufferIdx = 0;
    VkQueue                              transferQueue        = VK_NULL_HANDLE;
    uint32_t                             transferQueueFamily  = UINT32_MAX;
    VkSemaphore                          timelineSemaphore    = VK_NULL_HANDLE;
    uint64_t                             nextUpdateId         = 1;
    std::unordered_map<uint64_t, uint64_t> updateSignals;
    bool                                 useMeshShaders       = false;
    bool                                 useDescriptorIndexing = false;

    mutable std::mutex                   mutex;
    std::condition_variable              cv;
    std::queue<std::pair<uint64_t, std::function<void(uint64_t)>>> callbackQueue;
    std::thread                          callbackThread;
    bool                                 shutdown = false;

    explicit Impl(Vulkan::Context& ctx)
        : context(ctx),
          callbackThread([this] { processCallbacks(); })
    {
        LOG_INFO_CAT("BufferMgr", "Impl constructed @ {}", ptr_to_hex(this));
    }

    ~Impl() {
        {
            std::lock_guard<std::mutex> lk(mutex);
            shutdown = true;
        }
        cv.notify_one();
        if (callbackThread.joinable()) callbackThread.join();
        LOG_INFO_CAT("BufferMgr", "Impl destroyed @ {}", ptr_to_hex(this));
    }

    void processCallbacks() {
        LOG_DEBUG_CAT("BufferMgr", "Callback thread STARTED [tid={}]", std::this_thread::get_id());
        while (true) {
            std::unique_lock<std::mutex> lk(mutex);
            cv.wait(lk, [this] { return !callbackQueue.empty() || shutdown; });
            if (shutdown && callbackQueue.empty()) break;

            auto [id, cb] = std::move(callbackQueue.front());
            callbackQueue.pop();
            lk.unlock();

            uint64_t cur = 0;
            while (cur < id) {
                vkGetSemaphoreCounterValue(context.device, timelineSemaphore, &cur);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            LOG_DEBUG_CAT("BufferMgr", "Executing callback for updateId={}", id);
            cb(id);
        }
        LOG_DEBUG_CAT("BufferMgr", "Callback thread TERMINATED");
    }

    void evictScratchCache() {
        while (scratchCacheOrder.size() > 128) {
            uint64_t key = scratchCacheOrder.front();
            scratchCacheOrder.pop_front();
            scratchSizeCache.erase(key);
            LOG_DEBUG_CAT("BufferMgr", "EVICTED scratch cache entry 0x{:x}", key);
        }
    }
};

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------
VulkanBufferManager::VulkanBufferManager(Vulkan::Context& ctx,
                                         std::span<const glm::vec3> vertices,
                                         std::span<const uint32_t>  indices)
    : context_(ctx),
      vertexCount_(0),
      indexCount_(0),
      vertexBufferAddress_(0),
      indexBufferAddress_(0),
      scratchBufferAddress_(0),
      impl_(std::make_unique<Impl>(ctx))
{
    LOG_INFO_CAT("BufferMgr", "Creating VulkanBufferManager (v={}, i={}) @ {}",
                 vertices.size(), indices.size(), ptr_to_hex(this));

    if (ctx.device == VK_NULL_HANDLE || ctx.physicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("Invalid Vulkan context");

    // VRAM Check
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(ctx.physicalDevice, &memProps);
    VkDeviceSize devLocal = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i)
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            devLocal += memProps.memoryHeaps[i].size;

    LOG_DEBUG_CAT("BufferMgr", "Device local memory: {} GiB", devLocal / (1024*1024*1024));
    if (devLocal < 8ULL * 1024 * 1024 * 1024)
        throw std::runtime_error("Device has <8 GiB local memory");

    if (vertices.empty() || indices.empty())
        throw std::invalid_argument("Vertex / index data cannot be empty");

    vertexCount_ = static_cast<uint32_t>(vertices.size());
    indexCount_  = static_cast<uint32_t>(indices.size());
    if (indexCount_ < 3 || indexCount_ % 3 != 0)
        throw std::invalid_argument("Invalid triangle list");

    // Extensions
    impl_->useMeshShaders        = isExtensionSupported(ctx.physicalDevice, VK_EXT_MESH_SHADER_EXTENSION_NAME);
    impl_->useDescriptorIndexing = isExtensionSupported(ctx.physicalDevice, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    initializeCommandPool();

    const VkDeviceSize vSize = sizeof(glm::vec3) * vertices.size() * 4;
    const VkDeviceSize iSize = sizeof(uint32_t)  * indices.size()  * 4;
    const VkDeviceSize total = vSize + iSize;
    const VkDeviceSize arenaSize = std::max<VkDeviceSize>(16ULL * 1024 * 1024, total * 2);

    LOG_DEBUG_CAT("BufferMgr", "Geometry size: V={} B, I={} B -> Arena={} B", vSize, iSize, arenaSize);
    if (arenaSize > devLocal / 2)
        throw std::runtime_error("Arena exceeds half of device memory");

    reserveArena(arenaSize, BufferType::GEOMETRY);

    vertexBufferAddress_ = asyncUpdateBuffers(vertices, indices, nullptr);
    indexBufferAddress_  = vertexBufferAddress_ + vSize;

    createUniformBuffers(3);

    const VkDeviceSize scratch = getScratchSize(vertexCount_, indexCount_) * 4;
    LOG_DEBUG_CAT("BufferMgr", "Scratch pool: {} B", scratch);
    if (scratch > devLocal / 4)
        throw std::runtime_error("Scratch pool exceeds 1/4 of device memory");

    reserveScratchPool(scratch, 4);
    scratchBufferAddress_ = impl_->scratchBufferAddresses[0];

    LOG_INFO_CAT("BufferMgr", "VulkanBufferManager FULLY INITIALIZED @ {}", ptr_to_hex(this));
}

VulkanBufferManager::~VulkanBufferManager() {
    LOG_INFO_CAT("BufferMgr", "Destroying VulkanBufferManager @ {}", ptr_to_hex(this));

    for (size_t i = 0; i < impl_->scratchBuffers.size(); ++i) {
        context_.resourceManager.removeBuffer(impl_->scratchBuffers[i]);
        Dispose::destroySingleBuffer(context_.device, impl_->scratchBuffers[i]);
        context_.resourceManager.removeMemory(impl_->scratchBufferMemories[i]);
        Dispose::freeSingleDeviceMemory(context_.device, impl_->scratchBufferMemories[i]);
        LOG_DEBUG_CAT("BufferMgr", "Destroyed scratch buffer #{}", i);
    }

    if (impl_->arenaBuffer != VK_NULL_HANDLE) {
        context_.resourceManager.removeBuffer(impl_->arenaBuffer);
        Dispose::destroySingleBuffer(context_.device, impl_->arenaBuffer);
        LOG_DEBUG_CAT("BufferMgr", "Destroyed arena buffer");
    }
    if (impl_->arenaMemory != VK_NULL_HANDLE) {
        context_.resourceManager.removeMemory(impl_->arenaMemory);
        Dispose::freeSingleDeviceMemory(context_.device, impl_->arenaMemory);
        LOG_DEBUG_CAT("BufferMgr", "Freed arena memory");
    }

    if (impl_->commandPool != VK_NULL_HANDLE) {
        Dispose::freeCommandBuffers(context_.device, impl_->commandPool, impl_->commandBuffers);
        vkDestroyCommandPool(context_.device, impl_->commandPool, nullptr);
        LOG_DEBUG_CAT("BufferMgr", "Destroyed command pool");
    }

    if (impl_->timelineSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(context_.device, impl_->timelineSemaphore, nullptr);
        LOG_DEBUG_CAT("BufferMgr", "Destroyed timeline semaphore");
    }

    LOG_INFO_CAT("BufferMgr", "VulkanBufferManager DESTROYED");
}

// ---------------------------------------------------------------------------
//  IMPLEMENTATION OF MISSING METHODS -- RAW VULKAN, NO WRAPPER
// ---------------------------------------------------------------------------

void VulkanBufferManager::initializeCommandPool() {
    // Use raw Vulkan query for queue family indices
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, queueFamilies.data());

    uint32_t transferFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            transferFamily = i;
            break;
        }
    }

    if (transferFamily == UINT32_MAX) {
        throw std::runtime_error("No transfer queue family found");
    }

    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = transferFamily
    };

    VK_CHECK(vkCreateCommandPool(context_.device, &poolInfo, nullptr, &impl_->commandPool),
             "Failed to create command pool");

    impl_->transferQueueFamily = transferFamily;
    vkGetDeviceQueue(context_.device, impl_->transferQueueFamily, 0, &impl_->transferQueue);

    LOG_DEBUG_CAT("BufferMgr", "Created command pool 0x{:x} for transfer queue family {}", 
                  ptr_to_hex(impl_->commandPool), transferFamily);
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

void VulkanBufferManager::reserveArena(VkDeviceSize size, BufferType type) {
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(context_.device, &bufferInfo, nullptr, &impl_->arenaBuffer),
             "Failed to create arena buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(context_.device, impl_->arenaBuffer, &memReqs);

    VkMemoryAllocateFlagsInfo flagsInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &flagsInfo,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = findMemoryType(context_.physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vkAllocateMemory(context_.device, &allocInfo, nullptr, &impl_->arenaMemory),
             "Failed to allocate arena memory");

    VK_CHECK(vkBindBufferMemory(context_.device, impl_->arenaBuffer, impl_->arenaMemory, 0),
             "Failed to bind arena buffer memory");

    impl_->arenaSize = size;
    impl_->vertexOffset = 0;
    impl_->indexOffset = sizeof(glm::vec3) * vertexCount_ * 4;

    LOG_INFO_CAT("BufferMgr", "Reserved arena: {} MiB, buffer=0x{:x}, memory=0x{:x}",
             size / (1024 * 1024), 
             reinterpret_cast<uintptr_t>(impl_->arenaBuffer), 
             reinterpret_cast<uintptr_t>(impl_->arenaMemory));
}

VkDeviceAddress VulkanBufferManager::asyncUpdateBuffers(
    std::span<const glm::vec3> vertices,
    std::span<const uint32_t> indices,
    std::function<void(uint64_t)> callback)
{
    const VkDeviceSize vSize = sizeof(glm::vec3) * vertices.size() * 4;
    const VkDeviceSize iSize = sizeof(uint32_t) * indices.size() * 4;

    VkBuffer stagingV = VK_NULL_HANDLE, stagingI = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemV = VK_NULL_HANDLE, stagingMemI = VK_NULL_HANDLE;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;

    auto cleanup = make_scope_guard([&]() {
        cleanupTmpResources(context_.device, impl_->commandPool, stagingV, stagingMemV, cb, fence);
        cleanupTmpResources(context_.device, impl_->commandPool, stagingI, stagingMemI, cb, fence);
    });

    // Staging vertex buffer
    VkBufferCreateInfo stagingInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(context_.device, &stagingInfo, nullptr, &stagingV), "Create staging vertex");
    VkMemoryRequirements memReqsV;
    vkGetBufferMemoryRequirements(context_.device, stagingV, &memReqsV);
    VkMemoryAllocateInfo allocInfoV = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqsV.size,
        .memoryTypeIndex = findMemoryType(context_.physicalDevice, memReqsV.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_.device, &allocInfoV, nullptr, &stagingMemV), "Alloc staging vertex");
    VK_CHECK(vkBindBufferMemory(context_.device, stagingV, stagingMemV, 0), "Bind staging vertex");

    void* dataV;
    VK_CHECK(vkMapMemory(context_.device, stagingMemV, 0, vSize, 0, &dataV), "Map staging vertex");
    std::memcpy(dataV, vertices.data(), vSize);
    vkUnmapMemory(context_.device, stagingMemV);

    // Staging index buffer
    stagingInfo.size = iSize;
    VK_CHECK(vkCreateBuffer(context_.device, &stagingInfo, nullptr, &stagingI), "Create staging index");
    VkMemoryRequirements memReqsI;
    vkGetBufferMemoryRequirements(context_.device, stagingI, &memReqsI);
    VkMemoryAllocateInfo allocInfoI = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqsI.size,
        .memoryTypeIndex = findMemoryType(context_.physicalDevice, memReqsI.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_.device, &allocInfoI, nullptr, &stagingMemI), "Alloc staging index");
    VK_CHECK(vkBindBufferMemory(context_.device, stagingI, stagingMemI, 0), "Bind staging index");

    void* dataI;
    VK_CHECK(vkMapMemory(context_.device, stagingMemI, 0, iSize, 0, &dataI), "Map staging index");
    std::memcpy(dataI, indices.data(), iSize);
    vkUnmapMemory(context_.device, stagingMemI);

    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = impl_->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(context_.device, &allocInfo, &cb), "Alloc cmd buffer");

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cb, &beginInfo), "Begin cmd buffer");

    VkBufferCopy vcopy = { .size = vSize };
    vkCmdCopyBuffer(cb, stagingV, impl_->arenaBuffer, 1, &vcopy);
    VkBufferCopy icopy = { .srcOffset = 0, .dstOffset = impl_->indexOffset, .size = iSize };
    vkCmdCopyBuffer(cb, stagingI, impl_->arenaBuffer, 1, &icopy);

    VK_CHECK(vkEndCommandBuffer(cb), "End cmd buffer");

    // Submit
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cb
    };
    VK_CHECK(vkQueueSubmit(impl_->transferQueue, 1, &submit, VK_NULL_HANDLE), "Queue submit");

    uint64_t updateId = impl_->nextUpdateId++;

    if (callback) {
        std::lock_guard<std::mutex> lk(impl_->mutex);
        impl_->callbackQueue.emplace(updateId, std::move(callback));
        impl_->cv.notify_one();
    }

    cleanup.release();

    VkBufferDeviceAddressInfo addrInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = impl_->arenaBuffer
    };
    return vkGetBufferDeviceAddress(context_.device, &addrInfo);
}

void VulkanBufferManager::createUniformBuffers(uint32_t count) {
    impl_->uniformBufferOffsets.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkBufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = 1024,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        VkBuffer buf;
        VK_CHECK(vkCreateBuffer(context_.device, &info, nullptr, &buf), "Create uniform buffer");

        VkMemoryRequirements reqs;
        vkGetBufferMemoryRequirements(context_.device, buf, &reqs);
        VkMemoryAllocateInfo alloc = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = reqs.size,
            .memoryTypeIndex = findMemoryType(context_.physicalDevice, reqs.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };
        VkDeviceMemory mem;
        VK_CHECK(vkAllocateMemory(context_.device, &alloc, nullptr, &mem), "Alloc uniform memory");
        VK_CHECK(vkBindBufferMemory(context_.device, buf, mem, 0), "Bind uniform");

        impl_->uniformBufferOffsets[i] = 0;
        LOG_DEBUG_CAT("BufferMgr", "Created uniform buffer #{} @ 0x{:x}", i, ptr_to_hex(buf));
    }
}

VkDeviceSize VulkanBufferManager::getScratchSize(uint32_t vertexCount, uint32_t indexCount) {
    return static_cast<VkDeviceSize>(vertexCount) * 64 + static_cast<VkDeviceSize>(indexCount) * 16;
}

void VulkanBufferManager::reserveScratchPool(VkDeviceSize size, uint32_t count) {
    impl_->scratchBuffers.resize(count);
    impl_->scratchBufferMemories.resize(count);
    impl_->scratchBufferAddresses.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        VkBufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        VK_CHECK(vkCreateBuffer(context_.device, &info, nullptr, &impl_->scratchBuffers[i]),
                 "Create scratch buffer");

        VkMemoryRequirements reqs;
        vkGetBufferMemoryRequirements(context_.device, impl_->scratchBuffers[i], &reqs);
        VkMemoryAllocateFlagsInfo flags = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
        };
        VkMemoryAllocateInfo alloc = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &flags,
            .allocationSize = reqs.size,
            .memoryTypeIndex = findMemoryType(context_.physicalDevice, reqs.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        VK_CHECK(vkAllocateMemory(context_.device, &alloc, nullptr, &impl_->scratchBufferMemories[i]),
                 "Alloc scratch memory");
        VK_CHECK(vkBindBufferMemory(context_.device, impl_->scratchBuffers[i], impl_->scratchBufferMemories[i], 0),
                 "Bind scratch");

        VkBufferDeviceAddressInfo addrInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = impl_->scratchBuffers[i]
        };
        impl_->scratchBufferAddresses[i] = vkGetBufferDeviceAddress(context_.device, &addrInfo);

        LOG_DEBUG_CAT("BufferMgr", "Reserved scratch buffer #{}: {} MiB @ 0x{:x}", i, size/(1024*1024), impl_->scratchBufferAddresses[i]);
    }
}

VkBuffer VulkanBufferManager::getUniformBuffer(uint32_t index) const {
    return VK_NULL_HANDLE;
}

VkDeviceMemory VulkanBufferManager::getUniformBufferMemory(uint32_t index) const {
    return VK_NULL_HANDLE;
}