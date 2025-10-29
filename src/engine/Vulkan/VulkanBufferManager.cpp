// src/engine/Vulkan/VulkanBufferManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// C++20 std::format — NO external fmt library

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
//  VK_CHECK macro (engine may already define it – safe to redefine)
// ---------------------------------------------------------------------------
#define VK_CHECK(expr, msg)                                                               \
    do {                                                                                  \
        VkResult _res = (expr);                                                           \
        if (_res != VK_SUCCESS) {                                                         \
            LOG_ERROR_CAT("BufferMgr", "{} failed: {} (code {})", msg, #expr, (int)_res); \
            throw std::runtime_error(std::string(msg) + " : " #expr);                    \
        }                                                                                 \
    } while (0)

// ---------------------------------------------------------------------------
//  Helper: pointer → hex string
// ---------------------------------------------------------------------------
inline std::string ptr_to_hex(const void* p) {
    std::ostringstream oss;
    oss << p;
    return oss.str();
}

// ---------------------------------------------------------------------------
//  RAII ScopeGuard
// ---------------------------------------------------------------------------
template <class F>
class ScopeGuard {
public:
    explicit ScopeGuard(F&& f) noexcept : f_(std::move(f)), active_(true) {}
    ScopeGuard(const ScopeGuard&)            = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
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
//  Memory barrier helper
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
//  Extension support
// ---------------------------------------------------------------------------
static bool isExtensionSupported(VkPhysicalDevice pd, const char* name) {
    uint32_t cnt = 0;
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &cnt, nullptr);
    std::vector<VkExtensionProperties> exts(cnt);
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &cnt, exts.data());
    return std::any_of(exts.begin(), exts.end(),
                       [name](const auto& e) { return std::strcmp(e.extensionName, name) == 0; });
}

// ---------------------------------------------------------------------------
//  Temp resource cleanup
// ---------------------------------------------------------------------------
static void cleanupTmpResources(VkDevice dev, VkCommandPool pool,
                                VkBuffer buf, VkDeviceMemory mem,
                                VkCommandBuffer cb = VK_NULL_HANDLE,
                                VkFence fence = VK_NULL_HANDLE)
{
    if (cb != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
        std::vector<VkCommandBuffer> vec = {cb};
        Dispose::freeCommandBuffers(dev, pool, vec);
    }
    if (buf != VK_NULL_HANDLE)  Dispose::destroySingleBuffer(dev, buf);
    if (mem != VK_NULL_HANDLE)  Dispose::freeSingleDeviceMemory(dev, mem);
    if (fence != VK_NULL_HANDLE) vkDestroyFence(dev, fence, nullptr);
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
        LOG_INFO_CAT("BufferMgr", "Impl constructed");
    }

    ~Impl() {
        {
            std::lock_guard<std::mutex> lk(mutex);
            shutdown = true;
        }
        cv.notify_one();
        if (callbackThread.joinable()) callbackThread.join();
        LOG_INFO_CAT("BufferMgr", "Impl destroyed");
    }

    void processCallbacks() {
        LOG_DEBUG_CAT("BufferMgr", "Callback thread started");
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
            cb(id);
        }
        LOG_DEBUG_CAT("BufferMgr", "Callback thread terminated");
    }

    void evictScratchCache() {
        while (scratchCacheOrder.size() > 128) {
            uint64_t key = scratchCacheOrder.front();
            scratchCacheOrder.pop_front();
            scratchSizeCache.erase(key);
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
    LOG_INFO_CAT("BufferMgr", "Creating VulkanBufferManager (v={}, i={})",
                 vertices.size(), indices.size());

    if (ctx.device == VK_NULL_HANDLE || ctx.physicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("Invalid Vulkan context");

    // ---- VRAM check -------------------------------------------------------
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(ctx.physicalDevice, &memProps);
    VkDeviceSize devLocal = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i)
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            devLocal += memProps.memoryHeaps[i].size;
    if (devLocal < 8ULL * 1024 * 1024 * 1024)
        throw std::runtime_error("Device has <8 GiB local memory");

    if (vertices.empty() || indices.empty())
        throw std::invalid_argument("Vertex / index data cannot be empty");

    vertexCount_ = static_cast<uint32_t>(vertices.size());
    indexCount_  = static_cast<uint32_t>(indices.size());
    if (indexCount_ < 3 || indexCount_ % 3 != 0)
        throw std::invalid_argument("Invalid triangle list");

    // ---- extensions ---------------------------------------------------------
    impl_->useMeshShaders        = isExtensionSupported(ctx.physicalDevice,
                                                       VK_EXT_MESH_SHADER_EXTENSION_NAME);
    impl_->useDescriptorIndexing = isExtensionSupported(ctx.physicalDevice,
                                                       VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    LOG_DEBUG_CAT("BufferMgr", "Mesh shaders: {}, Descriptor indexing: {}",
                  impl_->useMeshShaders, impl_->useDescriptorIndexing);

    initializeCommandPool();

    const VkDeviceSize vSize = sizeof(glm::vec3) * vertices.size() * 4;
    const VkDeviceSize iSize = sizeof(uint32_t)  * indices.size()  * 4;
    const VkDeviceSize total = vSize + iSize;

    // ---- Fixed: std::max with explicit type --------------------------------
    const VkDeviceSize arenaSize = std::max<VkDeviceSize>(16ULL * 1024 * 1024, total * 2);

    if (arenaSize > devLocal / 2)
        throw std::runtime_error("Arena exceeds half of device memory");

    reserveArena(arenaSize, BufferType::GEOMETRY);

    vertexBufferAddress_ = asyncUpdateBuffers(vertices, indices, nullptr);
    indexBufferAddress_  = vertexBufferAddress_ + vSize;

    createUniformBuffers(3);   // MAX_FRAMES_IN_FLIGHT

    const VkDeviceSize scratch = getScratchSize(vertexCount_, indexCount_) * 4;
    if (scratch > devLocal / 4)
        throw std::runtime_error("Scratch pool exceeds 1/4 of device memory");

    reserveScratchPool(scratch, 4);
    scratchBufferAddress_ = impl_->scratchBufferAddresses[0];

    LOG_INFO_CAT("BufferMgr", "VulkanBufferManager fully initialised");
}

VulkanBufferManager::~VulkanBufferManager() {
    LOG_INFO_CAT("BufferMgr", "Destroying VulkanBufferManager");

    for (size_t i = 0; i < impl_->scratchBuffers.size(); ++i) {
        context_.resourceManager.removeBuffer(impl_->scratchBuffers[i]);
        Dispose::destroySingleBuffer(context_.device, impl_->scratchBuffers[i]);
        context_.resourceManager.removeMemory(impl_->scratchBufferMemories[i]);
        Dispose::freeSingleDeviceMemory(context_.device, impl_->scratchBufferMemories[i]);
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

// ---------------------------------------------------------------------------
//  Command-pool
// ---------------------------------------------------------------------------
void VulkanBufferManager::initializeCommandPool() {
    LOG_DEBUG_CAT("BufferMgr", "Initialising transfer command pool");

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &qCount, qProps.data());

    impl_->transferQueueFamily = UINT32_MAX;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (qProps[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            impl_->transferQueueFamily = i;
            break;
        }
    }
    if (impl_->transferQueueFamily == UINT32_MAX) {
        for (uint32_t i = 0; i < qCount; ++i) {
            if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                impl_->transferQueueFamily = i;
                break;
            }
        }
    }
    if (impl_->transferQueueFamily == UINT32_MAX)
        throw std::runtime_error("No suitable queue family for transfers");

    const VkCommandPoolCreateInfo poolInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = impl_->transferQueueFamily
    };
    VK_CHECK(vkCreateCommandPool(context_.device, &poolInfo, nullptr, &impl_->commandPool),
             "Failed to create transfer command pool");

    impl_->commandBuffers.resize(32);
    const VkCommandBufferAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = impl_->commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 32
    };
    VK_CHECK(vkAllocateCommandBuffers(context_.device, &allocInfo, impl_->commandBuffers.data()),
             "Failed to allocate transfer command buffers");

    vkGetDeviceQueue(context_.device, impl_->transferQueueFamily, 0, &impl_->transferQueue);

    const VkSemaphoreCreateInfo semInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_CHECK(vkCreateSemaphore(context_.device, &semInfo, nullptr, &impl_->timelineSemaphore),
             "Failed to create timeline semaphore");

    LOG_DEBUG_CAT("BufferMgr", "Transfer queue family {} selected", impl_->transferQueueFamily);
}

// ---------------------------------------------------------------------------
//  Arena
// ---------------------------------------------------------------------------
void VulkanBufferManager::reserveArena(VkDeviceSize size, BufferType type) {
    if (impl_->arenaBuffer != VK_NULL_HANDLE && size <= impl_->arenaSize) {
        LOG_DEBUG_CAT("BufferMgr", "Arena already large enough ({} >= {})", impl_->arenaSize, size);
        return;
    }

    size = std::max<VkDeviceSize>(size, impl_->arenaSize * 2);
    LOG_INFO_CAT("BufferMgr", "Reserving arena of {} bytes (type={})", size, static_cast<int>(type));

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

    const VkMemoryAllocateFlagsInfo flagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };

    VulkanInitializer::createBuffer(context_.device,
                                    context_.physicalDevice,
                                    size,
                                    usage,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                    impl_->arenaBuffer,
                                    impl_->arenaMemory,
                                    &flagsInfo,
                                    context_.resourceManager);

    if (!impl_->arenaBuffer || !impl_->arenaMemory)
        throw std::runtime_error("Failed to allocate arena buffer");

    impl_->arenaSize = size;

    // zero-fill
    VkCommandBuffer zeroCmd = getCommandBuffer(BufferOperation::TRANSFER);
    const VkCommandBufferBeginInfo beginZero{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(zeroCmd, &beginZero), "Begin zero-cmd");
    vkCmdFillBuffer(zeroCmd, impl_->arenaBuffer, 0, size, 0);
    VK_CHECK(vkEndCommandBuffer(zeroCmd), "End zero-cmd");

    const VkSubmitInfo submitZero{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &zeroCmd
    };
    VK_CHECK(vkQueueSubmit(impl_->transferQueue, 1, &submitZero, VK_NULL_HANDLE), "Submit zero-fill");
    VK_CHECK(vkQueueWaitIdle(impl_->transferQueue), "Wait zero-fill");

    LOG_DEBUG_CAT("BufferMgr", "Arena zero-initialised ({} bytes)", size);
}

// ---------------------------------------------------------------------------
//  Async geometry upload
// ---------------------------------------------------------------------------
VkDeviceAddress VulkanBufferManager::asyncUpdateBuffers(
    std::span<const glm::vec3> vertices,
    std::span<const uint32_t>  indices,
    std::function<void(uint64_t)> callback)
{
    LOG_DEBUG_CAT("BufferMgr", "asyncUpdateBuffers (v={}, i={})", vertices.size(), indices.size());

    const VkDeviceSize unscaledV = sizeof(glm::vec3) * vertices.size();
    const VkDeviceSize unscaledI = sizeof(uint32_t)  * indices.size();
    const VkDeviceSize vSize     = unscaledV * 4;
    const VkDeviceSize iSize     = unscaledI * 4;
    const VkDeviceSize total     = vSize + iSize;

    if (total > impl_->arenaSize)
        reserveArena(total, BufferType::GEOMETRY);

    VkBuffer       staging    = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VkFence        fence      = VK_NULL_HANDLE;
    VkCommandBuffer cmd       = getCommandBuffer(BufferOperation::TRANSFER);

    // *** FIXED: removed `const` ***
    auto cleanup = make_scope_guard([&] {
        cleanupTmpResources(context_.device, impl_->commandPool,
                            staging, stagingMem, cmd, fence);
    });

    const VkBufferCreateInfo stagingInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = total,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };
    VK_CHECK(vkCreateBuffer(context_.device, &stagingInfo, nullptr, &staging),
             "Create staging buffer");

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(context_.device, staging, &memReq);
    const uint32_t memType = VulkanInitializer::findMemoryType(
        context_.physicalDevice,
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    const VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReq.size,
        .memoryTypeIndex = memType
    };
    VK_CHECK(vkAllocateMemory(context_.device, &allocInfo, nullptr, &stagingMem),
             "Allocate staging memory");
    VK_CHECK(vkBindBufferMemory(context_.device, staging, stagingMem, 0),
             "Bind staging memory");

    void* map = nullptr;
    VK_CHECK(vkMapMemory(context_.device, stagingMem, 0, total, 0, &map),
             "Map staging memory");
    std::memset(map, 0, total);
    std::memcpy(map, vertices.data(), unscaledV);
    std::memcpy(static_cast<char*>(map) + unscaledV, indices.data(), unscaledI);
    vkUnmapMemory(context_.device, stagingMem);

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Begin copy cmd");

    const VkBufferCopy vcopy{.srcOffset = 0, .dstOffset = impl_->vertexOffset, .size = vSize};
    const VkBufferCopy icopy{.srcOffset = vSize, .dstOffset = impl_->indexOffset, .size = iSize};
    vkCmdCopyBuffer(cmd, staging, impl_->arenaBuffer, 1, &vcopy);
    vkCmdCopyBuffer(cmd, staging, impl_->arenaBuffer, 1, &icopy);

    insertMemoryBarrier(cmd,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR |
                        (impl_->useMeshShaders ? VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT : 0),
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                        VK_ACCESS_INDEX_READ_BIT |
                        VK_ACCESS_SHADER_READ_BIT);
    VK_CHECK(vkEndCommandBuffer(cmd), "End copy cmd");

    const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VK_CHECK(vkCreateFence(context_.device, &fenceInfo, nullptr, &fence),
             "Create upload fence");

    const uint64_t updateId = impl_->nextUpdateId++;
    const VkTimelineSemaphoreSubmitInfo tsi{
        .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues    = &updateId
    };
    const VkSubmitInfo submit{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = &tsi,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &impl_->timelineSemaphore
    };
    VK_CHECK(vkQueueSubmit(impl_->transferQueue, 1, &submit, fence),
             "Submit geometry upload");

    VK_CHECK(vkWaitForFences(context_.device, 1, &fence, VK_TRUE, 5'000'000'000ULL),
             "Geometry upload fence timeout");

    impl_->updateSignals[updateId] = updateId;
    if (callback) {
        std::lock_guard<std::mutex> lk(impl_->mutex);
        impl_->callbackQueue.emplace(updateId, std::move(callback));
        impl_->cv.notify_one();
    }

    impl_->vertexOffset += vSize;
    impl_->indexOffset  += iSize;

    vertexCount_ = static_cast<uint32_t>(vertices.size());
    indexCount_  = static_cast<uint32_t>(indices.size());

    const VkDeviceAddress start = VulkanInitializer::getBufferDeviceAddress(context_, impl_->arenaBuffer) +
                                  (impl_->vertexOffset - vSize);
    LOG_DEBUG_CAT("BufferMgr", "Geometry upload complete – vertex address 0x{:x}", start);

    cleanup.release();   // now valid – no const qualifier
    return start;
}

// ---------------------------------------------------------------------------
//  Uniform buffers
// ---------------------------------------------------------------------------
void VulkanBufferManager::createUniformBuffers(uint32_t count) {
    LOG_INFO_CAT("BufferMgr", "Creating {} uniform buffers", count);
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

    const VkDeviceSize perBuffer = (sizeof(UniformBufferObject) + sizeof(int) +
                                    2048 + props.limits.minUniformBufferOffsetAlignment - 1) &
                                   ~(props.limits.minUniformBufferOffsetAlignment - 1);
    const VkDeviceSize total = perBuffer * count;

    if (total > 16 * 1024 * 1024) {
        configureUniformBuffers(count, perBuffer);
        return;
    }

    context_.uniformBuffers.resize(count);
    context_.uniformBufferMemories.resize(count);
    impl_->uniformBufferOffsets.assign(count, 0);

    for (uint32_t i = 0; i < count; ++i) {
        VulkanInitializer::createBuffer(
            context_.device,
            context_.physicalDevice,
            perBuffer,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            context_.uniformBuffers[i],
            context_.uniformBufferMemories[i],
            nullptr,
            context_.resourceManager);
    }
    LOG_DEBUG_CAT("BufferMgr", "Uniform buffers allocated ({} × {} bytes)", count, perBuffer);
}

void VulkanBufferManager::configureUniformBuffers(uint32_t count, VkDeviceSize sizePerBuffer) {
    LOG_INFO_CAT("BufferMgr", "Configuring {} uniform buffers ({} bytes each) inside arena",
                 count, sizePerBuffer);
    if (count == 0) return;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(context_.physicalDevice, &props);
    sizePerBuffer = (sizePerBuffer + props.limits.minUniformBufferOffsetAlignment - 1) &
                    ~(props.limits.minUniformBufferOffsetAlignment - 1);
    const VkDeviceSize total = sizePerBuffer * count;

    reserveArena(total, BufferType::UNIFORM);

    context_.uniformBuffers.assign(count, impl_->arenaBuffer);
    context_.uniformBufferMemories.assign(count, impl_->arenaMemory);
    impl_->uniformBufferOffsets.resize(count);
    for (uint32_t i = 0; i < count; ++i)
        impl_->uniformBufferOffsets[i] = i * sizePerBuffer;
}

// ---------------------------------------------------------------------------
//  Mesh-shader toggle
// ---------------------------------------------------------------------------
void VulkanBufferManager::enableMeshShaders(bool enable, const MeshletConfig&) {
    impl_->useMeshShaders = enable && isExtensionSupported(context_.physicalDevice,
                                                          VK_EXT_MESH_SHADER_EXTENSION_NAME);
    LOG_INFO_CAT("BufferMgr", "Mesh shaders {} (requested={})",
                 impl_->useMeshShaders ? "enabled" : "disabled", enable);
    if (impl_->useMeshShaders) {
        impl_->meshletOffset = impl_->indexOffset +
            (sizeof(uint32_t) * indexCount_ * 4);
    }
}

// ---------------------------------------------------------------------------
//  Meshlet batch upload
// ---------------------------------------------------------------------------
void VulkanBufferManager::addMeshletBatch(std::span<const MeshletData> meshlets) {
    if (!impl_->useMeshShaders)
        throw std::runtime_error("Mesh shaders not enabled");

    VkDeviceSize total = 0;
    for (const auto& m : meshlets) total += m.size;
    if (total == 0) return;

    LOG_DEBUG_CAT("BufferMgr", "Uploading {} meshlets ({} bytes)", meshlets.size(), total);
    if (impl_->meshletOffset + total > impl_->arenaSize)
        reserveArena(impl_->arenaSize + total, BufferType::GEOMETRY);

    VkBuffer       staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VkFence        fence = VK_NULL_HANDLE;
    VkCommandBuffer cmd = getCommandBuffer(BufferOperation::TRANSFER);

    // *** FIXED: removed `const` ***
    auto cleanup = make_scope_guard([&] {
        cleanupTmpResources(context_.device, impl_->commandPool,
                            staging, stagingMem, cmd, fence);
    });

    const VkBufferCreateInfo sci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = total,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };
    VK_CHECK(vkCreateBuffer(context_.device, &sci, nullptr, &staging), "Create meshlet staging");
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(context_.device, staging, &req);
    const uint32_t memType = VulkanInitializer::findMemoryType(
        context_.physicalDevice,
        req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    const VkMemoryAllocateInfo mai{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = req.size,
        .memoryTypeIndex = memType
    };
    VK_CHECK(vkAllocateMemory(context_.device, &mai, nullptr, &stagingMem), "Alloc meshlet staging");
    VK_CHECK(vkBindBufferMemory(context_.device, staging, stagingMem, 0), "Bind meshlet staging");

    void* map = nullptr;
    VK_CHECK(vkMapMemory(context_.device, stagingMem, 0, total, 0, &map), "Map meshlet staging");
    std::memset(map, 0, total);
    VkDeviceSize off = 0;
    for (const auto& m : meshlets) {
        std::memcpy(static_cast<char*>(map) + off, m.data, m.size);
        off += m.size;
    }
    vkUnmapMemory(context_.device, stagingMem);

    const VkCommandBufferBeginInfo cbi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &cbi), "Begin meshlet cmd");
    const VkBufferCopy copy{.srcOffset = 0, .dstOffset = impl_->meshletOffset, .size = total};
    vkCmdCopyBuffer(cmd, staging, impl_->arenaBuffer, 1, &copy);
    insertMemoryBarrier(cmd,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT,
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_ACCESS_SHADER_READ_BIT);
    VK_CHECK(vkEndCommandBuffer(cmd), "End meshlet cmd");

    const VkFenceCreateInfo fci{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VK_CHECK(vkCreateFence(context_.device, &fci, nullptr, &fence), "Create meshlet fence");
    const VkSubmitInfo si{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };
    VK_CHECK(vkQueueSubmit(impl_->transferQueue, 1, &si, fence), "Submit meshlet upload");
    VK_CHECK(vkWaitForFences(context_.device, 1, &fence, VK_TRUE, 5'000'000'000ULL),
             "Meshlet fence timeout");

    impl_->meshletOffset += total;
    LOG_DEBUG_CAT("BufferMgr", "Meshlet batch uploaded (offset={})", impl_->meshletOffset);
}

// ---------------------------------------------------------------------------
//  Command-buffer pool
// ---------------------------------------------------------------------------
VkCommandBuffer VulkanBufferManager::getCommandBuffer(BufferOperation) {
    VkCommandBuffer cb = impl_->commandBuffers[impl_->nextCommandBufferIdx];
    impl_->nextCommandBufferIdx = (impl_->nextCommandBufferIdx + 1) % 32;
    vkResetCommandBuffer(cb, 0);
    return cb;
}

// ---------------------------------------------------------------------------
//  Generic batch transfer
// ---------------------------------------------------------------------------
void VulkanBufferManager::batchTransferAsync(
    std::span<const BufferCopy> copies,
    std::function<void(uint64_t)> callback)
{
    if (copies.empty()) return;

    VkDeviceSize total = 0;
    for (const auto& c : copies) total += c.size;
    LOG_DEBUG_CAT("BufferMgr", "batchTransferAsync ({} copies, {} bytes)", copies.size(), total);
    if (total > impl_->arenaSize) reserveArena(total, BufferType::GEOMETRY);

    VkBuffer       staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VkFence        fence = VK_NULL_HANDLE;
    VkCommandBuffer cmd = getCommandBuffer(BufferOperation::TRANSFER);

    // *** FIXED: removed `const` ***
    auto cleanup = make_scope_guard([&] {
        cleanupTmpResources(context_.device, impl_->commandPool,
                            staging, stagingMem, cmd, fence);
    });

    const VkBufferCreateInfo sci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = total,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };
    VK_CHECK(vkCreateBuffer(context_.device, &sci, nullptr, &staging), "Create batch staging");
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(context_.device, staging, &req);
    const uint32_t memType = VulkanInitializer::findMemoryType(
        context_.physicalDevice,
        req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    const VkMemoryAllocateInfo mai{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = req.size,
        .memoryTypeIndex = memType
    };
    VK_CHECK(vkAllocateMemory(context_.device, &mai, nullptr, &stagingMem), "Alloc batch staging");
    VK_CHECK(vkBindBufferMemory(context_.device, staging, stagingMem, 0), "Bind batch staging");

    void* map = nullptr;
    VK_CHECK(vkMapMemory(context_.device, stagingMem, 0, total, 0, &map), "Map batch staging");
    std::memset(map, 0, total);
    VkDeviceSize off = 0;
    for (const auto& c : copies) {
        std::memcpy(static_cast<char*>(map) + off, c.data, c.size);
        off += c.size;
    }
    vkUnmapMemory(context_.device, stagingMem);

    const VkCommandBufferBeginInfo cbi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &cbi), "Begin batch cmd");

    std::vector<VkBufferCopy> vkCopies(copies.size());
    off = 0;
    for (size_t i = 0; i < copies.size(); ++i) {
        vkCopies[i] = {off, copies[i].dstOffset, copies[i].size};
        off += copies[i].size;
    }
    vkCmdCopyBuffer(cmd, staging, impl_->arenaBuffer,
                    static_cast<uint32_t>(vkCopies.size()), vkCopies.data());

    insertMemoryBarrier(cmd,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR |
                        (impl_->useMeshShaders ? VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT : 0),
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                        VK_ACCESS_INDEX_READ_BIT |
                        VK_ACCESS_SHADER_READ_BIT);
    VK_CHECK(vkEndCommandBuffer(cmd), "End batch cmd");

    const uint64_t updateId = impl_->nextUpdateId++;
    const VkTimelineSemaphoreSubmitInfo tsi{
        .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues    = &updateId
    };
    const VkSubmitInfo si{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = &tsi,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &impl_->timelineSemaphore
    };
    VK_CHECK(vkQueueSubmit(impl_->transferQueue, 1, &si, fence), "Submit batch transfer");
    VK_CHECK(vkWaitForFences(context_.device, 1, &fence, VK_TRUE, 5'000'000'000ULL),
             "Batch transfer fence timeout");

    impl_->updateSignals[updateId] = updateId;
    if (callback) {
        std::lock_guard<std::mutex> lk(impl_->mutex);
        impl_->callbackQueue.emplace(updateId, std::move(callback));
        impl_->cv.notify_one();
    }

    LOG_DEBUG_CAT("BufferMgr", "Batch transfer complete (updateId={})", updateId);
}

// ---------------------------------------------------------------------------
//  Scratch pool
// ---------------------------------------------------------------------------
void VulkanBufferManager::reserveScratchPool(VkDeviceSize sizePerBuffer, uint32_t count) {
    LOG_INFO_CAT("BufferMgr", "Reserving {} scratch buffers ({} bytes each)", count, sizePerBuffer);

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
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
    props2.pNext = &asProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);

    sizePerBuffer = (sizePerBuffer + asProps.minAccelerationStructureScratchOffsetAlignment - 1) &
                    ~(asProps.minAccelerationStructureScratchOffsetAlignment - 1);

    const VkMemoryAllocateFlagsInfo flagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };

    impl_->scratchBuffers.resize(count);
    impl_->scratchBufferMemories.resize(count);
    impl_->scratchBufferAddresses.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        VulkanInitializer::createBuffer(
            context_.device,
            context_.physicalDevice,
            sizePerBuffer,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            impl_->scratchBuffers[i],
            impl_->scratchBufferMemories[i],
            &flagsInfo,
            context_.resourceManager);

        impl_->scratchBufferAddresses[i] =
            VulkanInitializer::getBufferDeviceAddress(context_, impl_->scratchBuffers[i]);
    }

    context_.scratchBuffer        = impl_->scratchBuffers[0];
    context_.scratchBufferMemory  = impl_->scratchBufferMemories[0];
    scratchBufferAddress_         = impl_->scratchBufferAddresses[0];
}

// ---------------------------------------------------------------------------
//  Scratch size query
// ---------------------------------------------------------------------------
VkDeviceSize VulkanBufferManager::getScratchSize(uint32_t vertexCount, uint32_t indexCount) {
    const uint64_t key = (static_cast<uint64_t>(vertexCount) << 32) | indexCount;

    {
        std::lock_guard<std::mutex> lk(impl_->mutex);
        const auto it = impl_->scratchSizeCache.find(key);
        if (it != impl_->scratchSizeCache.end()) {
            const auto ord = std::find(impl_->scratchCacheOrder.begin(),
                                       impl_->scratchCacheOrder.end(), key);
            if (ord != impl_->scratchCacheOrder.end()) {
                impl_->scratchCacheOrder.erase(ord);
            }
            impl_->scratchCacheOrder.push_back(key);
            LOG_DEBUG_CAT("BufferMgr", "Scratch size cache hit (key=0x{:x}) → {} bytes", key, it->second);
            return it->second;
        }
    }

    if (indexCount < 3 || indexCount % 3 != 0 || vertexCount == 0)
        throw std::invalid_argument("Invalid geometry for AS build");

    const auto fn = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetAccelerationStructureBuildSizesKHR"));
    if (!fn) throw std::runtime_error("vkGetAccelerationStructureBuildSizesKHR not available");

    const VkAccelerationStructureGeometryTrianglesDataKHR tri{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData    = {.deviceAddress = vertexBufferAddress_},
        .vertexStride  = sizeof(glm::vec3),
        .maxVertex     = vertexCount - 1,
        .indexType     = VK_INDEX_TYPE_UINT32,
        .indexData     = {.deviceAddress = indexBufferAddress_}
    };
    const VkAccelerationStructureGeometryKHR geom{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry     = {.triangles = tri},
        .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR
    };
    const VkAccelerationStructureBuildGeometryInfoKHR info{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type         = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags        = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
        .mode         = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries   = &geom
    };
    const uint32_t primCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizes{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    fn(context_.device,
       VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
       &info,
       &primCount,
       &sizes);

    const VkDeviceSize result = sizes.buildScratchSize;
    {
        std::lock_guard<std::mutex> lk(impl_->mutex);
        impl_->scratchSizeCache[key] = result;
        impl_->scratchCacheOrder.push_back(key);
        impl_->evictScratchCache();
    }
    LOG_DEBUG_CAT("BufferMgr", "Scratch size computed (v={}, i={}) → {} bytes", vertexCount, indexCount, result);
    return result;
}

// ---------------------------------------------------------------------------
//  Descriptor updates
// ---------------------------------------------------------------------------
void VulkanBufferManager::batchDescriptorUpdate(std::span<const DescriptorUpdate> updates,
                                                uint32_t maxBindings)
{
    if (updates.empty()) return;
    if (!impl_->useDescriptorIndexing && maxBindings > 32) maxBindings = 32;

    LOG_DEBUG_CAT("BufferMgr", "Updating {} descriptor writes", updates.size());

    std::vector<VkWriteDescriptorSet> writes(updates.size());
    std::vector<VkDescriptorBufferInfo> infos(updates.size());

    for (size_t i = 0; i < updates.size(); ++i) {
        if (updates[i].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            if (updates[i].bufferIndex >= context_.uniformBuffers.size())
                throw std::runtime_error("Uniform buffer index out of range");
            infos[i] = {
                .buffer = context_.uniformBuffers[updates[i].bufferIndex],
                .offset = impl_->uniformBufferOffsets[updates[i].bufferIndex],
                .range  = updates[i].size
            };
        }
        writes[i] = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = updates[i].descriptorSet,
            .dstBinding      = updates[i].binding,
            .descriptorCount = 1,
            .descriptorType  = updates[i].type,
            .pBufferInfo     = &infos[i]
        };
    }
    vkUpdateDescriptorSets(context_.device,
                           static_cast<uint32_t>(writes.size()),
                           writes.data(),
                           0,
                           nullptr);
}

// ---------------------------------------------------------------------------
//  Getters
// ---------------------------------------------------------------------------
VkDeviceMemory VulkanBufferManager::getVertexBufferMemory() const { return impl_->arenaMemory; }
VkDeviceMemory VulkanBufferManager::getIndexBufferMemory()  const { return impl_->arenaMemory; }

VkDeviceMemory VulkanBufferManager::getUniformBufferMemory(uint32_t i) const {
    validateUniformBufferIndex(i);
    return context_.uniformBufferMemories[i];
}

VkDeviceAddress VulkanBufferManager::getVertexBufferAddress() const { return vertexBufferAddress_; }
VkDeviceAddress VulkanBufferManager::getIndexBufferAddress()  const { return indexBufferAddress_; }
VkDeviceAddress VulkanBufferManager::getScratchBufferAddress() const { return scratchBufferAddress_; }

VkBuffer VulkanBufferManager::getVertexBuffer() const { return impl_->arenaBuffer; }
VkBuffer VulkanBufferManager::getIndexBuffer()  const { return impl_->arenaBuffer; }

VkBuffer VulkanBufferManager::getMeshletBuffer(VkDeviceSize& offset) const {
    offset = impl_->meshletOffset;
    return impl_->arenaBuffer;
}

uint32_t VulkanBufferManager::getVertexCount() const { return vertexCount_; }
uint32_t VulkanBufferManager::getIndexCount()  const { return indexCount_; }

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

bool VulkanBufferManager::checkUpdateStatus(uint64_t updateId) const {
    const auto it = impl_->updateSignals.find(updateId);
    if (it == impl_->updateSignals.end()) return true;

    uint64_t val = 0;
    vkGetSemaphoreCounterValue(context_.device, impl_->timelineSemaphore, &val);
    return val >= it->second;
}