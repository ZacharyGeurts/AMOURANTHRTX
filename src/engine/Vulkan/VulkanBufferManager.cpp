// src/engine/Vulkan/VulkanBufferManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// EXTREME LOGGING MODE: EVERY BYTE, EVERY CALL, EVERY NANOSECOND
// FIXED: REMOVED getScratchSize() + reserveScratchPool() from constructor
// FIXED: reserveScratchPool() now called AFTER vkGetAccelerationStructureBuildSizesKHR in VulkanRenderer
// FIXED: Scratch buffer usage includes ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
// ENHANCED: Added vkQueueWaitIdle post-submit in asyncUpdateBuffers for synchronous safety during debug

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
//  VK_CHECK WITH NUCLEAR EXPLOSION
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
    LOG_TRACE_CAT("BufferMgr", "INSERT BARRIER: srcStage=0x{:x}, dstStage=0x{:x}, srcAccess=0x{:x}, dstAccess=0x{:x}",
                  srcStage, dstStage, srcAccess, dstAccess);
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
    LOG_INFO_CAT("BufferMgr", "EXTENSION CHECK: {} -> {}", name, supported ? "SUPPORTED" : "MISSING");
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
        LOG_TRACE_CAT("BufferMgr", "FREED transient command buffer 0x{:x}", reinterpret_cast<uintptr_t>(cb));
    }
    if (buf != VK_NULL_HANDLE) {
        Dispose::destroySingleBuffer(dev, buf);
        LOG_TRACE_CAT("BufferMgr", "DESTROYED staging buffer 0x{:x}", reinterpret_cast<uintptr_t>(buf));
    }
    if (mem != VK_NULL_HANDLE) {
        Dispose::freeSingleDeviceMemory(dev, mem);
        LOG_TRACE_CAT("BufferMgr", "FREED staging memory 0x{:x}", reinterpret_cast<uintptr_t>(mem));
    }
    if (fence != VK_NULL_HANDLE) {
        vkDestroyFence(dev, fence, nullptr);
        LOG_TRACE_CAT("BufferMgr", "DESTROYED upload fence 0x{:x}", reinterpret_cast<uintptr_t>(fence));
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
    std::vector<VkBuffer>                uniformBuffers;
    std::vector<VkDeviceMemory>          uniformBufferMemories;
    std::vector<VkDeviceSize>            uniformBufferOffsets;
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
        LOG_INFO_CAT("BufferMgr", "Impl CONSTRUCTED @ 0x{:x} | context.device=0x{:x} | context.physicalDevice=0x{:x}",
                     reinterpret_cast<uintptr_t>(this),
                     reinterpret_cast<uintptr_t>(ctx.device),
                     reinterpret_cast<uintptr_t>(ctx.physicalDevice));
    }

    ~Impl() {
        {
            std::lock_guard<std::mutex> lk(mutex);
            shutdown = true;
        }
        cv.notify_one();
        if (callbackThread.joinable()) {
            LOG_TRACE_CAT("BufferMgr", "JOINING callback thread [tid={}]", std::this_thread::get_id());
            callbackThread.join();
        }
        LOG_INFO_CAT("BufferMgr", "Impl DESTROYED @ 0x{:x}", reinterpret_cast<uintptr_t>(this));
    }

    void processCallbacks() {
        LOG_INFO_CAT("BufferMgr", "CALLBACK THREAD STARTED [tid={}]", std::this_thread::get_id());
        while (true) {
            std::unique_lock<std::mutex> lk(mutex);
            cv.wait(lk, [this] { return !callbackQueue.empty() || shutdown; });
            if (shutdown && callbackQueue.empty()) break;

            auto [id, cb] = std::move(callbackQueue.front());
            callbackQueue.pop();
            lk.unlock();

            LOG_TRACE_CAT("BufferMgr", "PROCESSING callback for updateId={}", id);
            uint64_t cur = 0;
            while (cur < id) {
                vkGetSemaphoreCounterValue(context.device, timelineSemaphore, &cur);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            LOG_DEBUG_CAT("BufferMgr", "EXECUTING user callback for updateId={}", id);
            cb(id);
        }
        LOG_INFO_CAT("BufferMgr", "CALLBACK THREAD TERMINATED");
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
    LOG_INFO_CAT("BufferMgr", "CREATING VulkanBufferManager @ 0x{:x} | vertices={} | indices={}",
                 reinterpret_cast<uintptr_t>(this), vertices.size(), indices.size());

    if (ctx.device == VK_NULL_HANDLE || ctx.physicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("Invalid Vulkan context");

    // VRAM Check
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(ctx.physicalDevice, &memProps);
    VkDeviceSize devLocal = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i)
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            devLocal += memProps.memoryHeaps[i].size;

    LOG_INFO_CAT("BufferMgr", "DEVICE LOCAL MEMORY: {} GiB ({} B)", devLocal / (1024*1024*1024), devLocal);
    if (devLocal < 8ULL * 1024 * 1024 * 1024)
        throw std::runtime_error("Device has <8 GiB local memory");

    if (vertices.empty() || indices.empty())
        throw std::invalid_argument("Vertex / index data cannot be empty");

    vertexCount_ = static_cast<uint32_t>(vertices.size());
    indexCount_  = static_cast<uint32_t>(indices.size());
    if (indexCount_ < 3 || indexCount_ % 3 != 0)
        throw std::invalid_argument("Invalid triangle list");

    LOG_INFO_CAT("BufferMgr", "GEOMETRY: {} vertices, {} indices ({} triangles)",
                 vertexCount_, indexCount_, indexCount_ / 3);

    // Extensions
    impl_->useMeshShaders        = isExtensionSupported(ctx.physicalDevice, VK_EXT_MESH_SHADER_EXTENSION_NAME);
    impl_->useDescriptorIndexing = isExtensionSupported(ctx.physicalDevice, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    initializeCommandPool();

    const VkDeviceSize vSize = sizeof(glm::vec3) * vertices.size() * 4;
    const VkDeviceSize iSize = sizeof(uint32_t)  * indices.size()  * 4;
    const VkDeviceSize total = vSize + iSize;
    const VkDeviceSize arenaSize = std::max<VkDeviceSize>(16ULL * 1024 * 1024, total * 2);

    LOG_INFO_CAT("BufferMgr", "GEOMETRY SIZE: V={} B | I={} B | TOTAL={} B | ARENA={} B",
                 vSize, iSize, total, arenaSize);
    if (arenaSize > devLocal / 2)
        throw std::runtime_error("Arena exceeds half of device memory");

    reserveArena(arenaSize, BufferType::GEOMETRY);

    vertexBufferAddress_ = asyncUpdateBuffers(vertices, indices, nullptr);
    indexBufferAddress_  = vertexBufferAddress_ + vSize;

    LOG_INFO_CAT("BufferMgr", "VERTEX BUFFER @ 0x{:x} | INDEX BUFFER @ 0x{:x}",
                 vertexBufferAddress_, indexBufferAddress_);

    // SCRATCH POOL RESERVED LATER IN VulkanRenderer::buildAccelerationStructures()
    // AFTER vkGetAccelerationStructureBuildSizesKHR()

    LOG_INFO_CAT("BufferMgr", "VulkanBufferManager FULLY INITIALIZED @ 0x{:x}", reinterpret_cast<uintptr_t>(this));
}

VulkanBufferManager::~VulkanBufferManager() {
    LOG_INFO_CAT("BufferMgr", "DESTROYING VulkanBufferManager @ 0x{:x}", reinterpret_cast<uintptr_t>(this));

    for (size_t i = 0; i < impl_->uniformBuffers.size(); ++i) {
        VkBuffer buf = impl_->uniformBuffers[i];
        if (buf != VK_NULL_HANDLE) {
            try { context_.resourceManager.removeBuffer(buf); }
            catch (const std::exception& e) { LOG_WARNING_CAT("BufferMgr", "Failed to remove uniform buffer #{}: {}", i, e.what()); }
            vkDestroyBuffer(context_.device, buf, nullptr);
            LOG_INFO_CAT("BufferMgr", "DESTROYED uniform buffer #{} @ 0x{:x}", i, reinterpret_cast<uintptr_t>(buf));
        }
        VkDeviceMemory mem = impl_->uniformBufferMemories[i];
        if (mem != VK_NULL_HANDLE) {
            try { context_.resourceManager.removeMemory(mem); }
            catch (const std::exception& e) { LOG_WARNING_CAT("BufferMgr", "Failed to remove uniform mem #{}: {}", i, e.what()); }
            vkFreeMemory(context_.device, mem, nullptr);
        }
    }

    for (size_t i = 0; i < impl_->scratchBuffers.size(); ++i) {
        VkBuffer buf = impl_->scratchBuffers[i];
        if (buf != VK_NULL_HANDLE) {
            try { context_.resourceManager.removeBuffer(buf); }
            catch (const std::exception& e) { LOG_WARNING_CAT("BufferMgr", "Failed to remove scratch buffer #{}: {}", i, e.what()); }
            vkDestroyBuffer(context_.device, buf, nullptr);
            LOG_INFO_CAT("BufferMgr", "DESTROYED scratch buffer #{} @ 0x{:x}", i, reinterpret_cast<uintptr_t>(buf));
        }
        VkDeviceMemory mem = impl_->scratchBufferMemories[i];
        if (mem != VK_NULL_HANDLE) {
            try { context_.resourceManager.removeMemory(mem); }
            catch (const std::exception& e) { LOG_WARNING_CAT("BufferMgr", "Failed to remove scratch mem #{}: {}", i, e.what()); }
            vkFreeMemory(context_.device, mem, nullptr);
        }
    }

    if (impl_->arenaBuffer != VK_NULL_HANDLE) {
        try { context_.resourceManager.removeBuffer(impl_->arenaBuffer); }
        catch (const std::exception& e) { LOG_WARNING_CAT("BufferMgr", "Failed to remove arena buffer: {}", e.what()); }
        vkDestroyBuffer(context_.device, impl_->arenaBuffer, nullptr);
        LOG_INFO_CAT("BufferMgr", "DESTROYED arena buffer 0x{:x}", reinterpret_cast<uintptr_t>(impl_->arenaBuffer));
    }
    if (impl_->arenaMemory != VK_NULL_HANDLE) {
        try { context_.resourceManager.removeMemory(impl_->arenaMemory); }
        catch (const std::exception& e) { LOG_WARNING_CAT("BufferMgr", "Failed to remove arena memory: {}", e.what()); }
        vkFreeMemory(context_.device, impl_->arenaMemory, nullptr);
        LOG_INFO_CAT("BufferMgr", "FREED arena memory 0x{:x}", reinterpret_cast<uintptr_t>(impl_->arenaMemory));
    }

    if (impl_->commandPool != VK_NULL_HANDLE) {
        try { Dispose::freeCommandBuffers(context_.device, impl_->commandPool, impl_->commandBuffers); }
        catch (const std::exception& e) { LOG_WARNING_CAT("BufferMgr", "Failed to free command buffers: {}", e.what()); }
        vkDestroyCommandPool(context_.device, impl_->commandPool, nullptr);
        LOG_INFO_CAT("BufferMgr", "DESTROYED command pool 0x{:x}", reinterpret_cast<uintptr_t>(impl_->commandPool));
    }

    if (impl_->timelineSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(context_.device, impl_->timelineSemaphore, nullptr);
        LOG_INFO_CAT("BufferMgr", "DESTROYED timeline semaphore 0x{:x}", reinterpret_cast<uintptr_t>(impl_->timelineSemaphore));
    }

    LOG_INFO_CAT("BufferMgr", "VulkanBufferManager FULLY DESTROYED");
}

// ---------------------------------------------------------------------------
//  IMPLEMENTATION OF MISSING METHODS -- RAW VULKAN, NO WRAPPER
// ---------------------------------------------------------------------------

void VulkanBufferManager::initializeCommandPool() {
    LOG_INFO_CAT("BufferMgr", "INITIALIZING command pool for transfer queue");

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context_.physicalDevice, &queueFamilyCount, queueFamilies.data());

    uint32_t transferFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            transferFamily = i;
            LOG_TRACE_CAT("BufferMgr", "FOUND transfer queue family: {} (flags=0x{:x})", i, queueFamilies[i].queueFlags);
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

    LOG_INFO_CAT("BufferMgr", "COMMAND POOL CREATED: 0x{:x} | queue family {} | queue 0x{:x}",
                 reinterpret_cast<uintptr_t>(impl_->commandPool),
                 transferFamily,
                 reinterpret_cast<uintptr_t>(impl_->transferQueue));
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    LOG_TRACE_CAT("BufferMgr", "FINDING memory type: filter=0x{:x}, properties=0x{:x}", typeFilter, properties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            LOG_TRACE_CAT("BufferMgr", "MATCHED memory type {} (flags=0x{:x})", i, memProperties.memoryTypes[i].propertyFlags);
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

void VulkanBufferManager::reserveArena(VkDeviceSize size, BufferType type) {
    LOG_INFO_CAT("BufferMgr", "RESERVING ARENA: {} B ({} MiB) | type={}", size, size/(1024*1024), static_cast<int>(type));

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VK_CHECK(vkCreateBuffer(context_.device, &bufferInfo, nullptr, &impl_->arenaBuffer),
             "Failed to create arena buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(context_.device, impl_->arenaBuffer, &memReqs);

    LOG_TRACE_CAT("BufferMgr", "ARENA BUFFER REQS: size={} B, alignment={} B, typeBits=0x{:x}",
                  memReqs.size, memReqs.alignment, memReqs.memoryTypeBits);

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

    LOG_INFO_CAT("BufferMgr", "ARENA RESERVED: {} MiB | buffer=0x{:x} | memory=0x{:x} | vertexOffset={} | indexOffset={}",
                 size / (1024*1024),
                 reinterpret_cast<uintptr_t>(impl_->arenaBuffer),
                 reinterpret_cast<uintptr_t>(impl_->arenaMemory),
                 impl_->vertexOffset, impl_->indexOffset);
}

VkDeviceAddress VulkanBufferManager::asyncUpdateBuffers(
    std::span<const glm::vec3> vertices,
    std::span<const uint32_t> indices,
    std::function<void(uint64_t)> callback)
{
    const VkDeviceSize vSize = sizeof(glm::vec3) * vertices.size() * 4;
    const VkDeviceSize iSize = sizeof(uint32_t) * indices.size() * 4;

    LOG_INFO_CAT("BufferMgr", "ASYNC UPDATE: vSize={} B | iSize={} B | updateId={}",
                 vSize, iSize, impl_->nextUpdateId);

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
    LOG_TRACE_CAT("BufferMgr", "COPIED {} B vertex data to staging buffer 0x{:x}", vSize, reinterpret_cast<uintptr_t>(stagingV));

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
    LOG_TRACE_CAT("BufferMgr", "COPIED {} B index data to staging buffer 0x{:x}", iSize, reinterpret_cast<uintptr_t>(stagingI));

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
    LOG_TRACE_CAT("BufferMgr", "COPY: stagingV -> arena (size={} B)", vSize);

    VkBufferCopy icopy = { .srcOffset = 0, .dstOffset = impl_->indexOffset, .size = iSize };
    vkCmdCopyBuffer(cb, stagingI, impl_->arenaBuffer, 1, &icopy);
    LOG_TRACE_CAT("BufferMgr", "COPY: stagingI -> arena+{} (size={} B)", impl_->indexOffset, iSize);

    VK_CHECK(vkEndCommandBuffer(cb), "End cmd buffer");

    // Submit
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cb
    };
    VK_CHECK(vkQueueSubmit(impl_->transferQueue, 1, &submit, VK_NULL_HANDLE), "Queue submit");

    vkQueueWaitIdle(impl_->transferQueue);
    LOG_INFO_CAT("BufferMgr", "Transfer queue idle post-submit - data synced");

    uint64_t updateId = impl_->nextUpdateId++;

    if (callback) {
        std::lock_guard<std::mutex> lk(impl_->mutex);
        impl_->callbackQueue.emplace(updateId, std::move(callback));
        impl_->cv.notify_one();
        LOG_TRACE_CAT("BufferMgr", "ENQUEUED callback for updateId={}", updateId);
    }

    cleanup.release();

    VkBufferDeviceAddressInfo addrInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = impl_->arenaBuffer
    };
    VkDeviceAddress addr = vkGetBufferDeviceAddress(context_.device, &addrInfo);
    LOG_INFO_CAT("BufferMgr", "UPDATE COMPLETE: updateId={} | new arena address=0x{:x}", updateId, addr);
    return addr;
}

void VulkanBufferManager::createUniformBuffers(uint32_t count) {
    LOG_INFO_CAT("BufferMgr", "CREATING {} uniform buffers (1024 B each)", count);
    impl_->uniformBuffers.resize(count);
    impl_->uniformBufferMemories.resize(count);
    impl_->uniformBufferOffsets.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkBufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(UniformBufferObject),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        VK_CHECK(vkCreateBuffer(context_.device, &info, nullptr, &impl_->uniformBuffers[i]),
                 "Create uniform buffer");

        VkMemoryRequirements reqs;
        vkGetBufferMemoryRequirements(context_.device, impl_->uniformBuffers[i], &reqs);
        VkMemoryAllocateInfo alloc = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = reqs.size,
            .memoryTypeIndex = findMemoryType(context_.physicalDevice, reqs.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };
        VK_CHECK(vkAllocateMemory(context_.device, &alloc, nullptr, &impl_->uniformBufferMemories[i]),
                 "Alloc uniform memory");
        VK_CHECK(vkBindBufferMemory(context_.device, impl_->uniformBuffers[i], impl_->uniformBufferMemories[i], 0),
                 "Bind uniform");

        impl_->uniformBufferOffsets[i] = 0;
        LOG_INFO_CAT("BufferMgr", "UNIFORM BUFFER #{}: buffer=0x{:x} | memory=0x{:x} | size={} B",
                     i,
                     reinterpret_cast<uintptr_t>(impl_->uniformBuffers[i]),
                     reinterpret_cast<uintptr_t>(impl_->uniformBufferMemories[i]),
                     reqs.size);
    }
}

VkBuffer VulkanBufferManager::getUniformBuffer(uint32_t index) const {
    if (index >= impl_->uniformBuffers.size()) {
        throw std::out_of_range("Uniform buffer index out of range");
    }
    LOG_TRACE_CAT("BufferMgr", "GET uniform buffer #{}", index);
    return impl_->uniformBuffers[index];
}

VkDeviceMemory VulkanBufferManager::getUniformBufferMemory(uint32_t index) const {
    if (index >= impl_->uniformBufferMemories.size()) {
        throw std::out_of_range("Uniform buffer memory index out of range");
    }
    LOG_TRACE_CAT("BufferMgr", "GET uniform memory #{}", index);
    return impl_->uniformBufferMemories[index];
}

void VulkanBufferManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                       VkBuffer& buffer, VkDeviceMemory& memory) {
    LOG_INFO_CAT("BufferMgr", "Creating buffer: size={} B, usage=0x{:x}, properties=0x{:x}", size, usage, properties);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(context_.device, &bufferInfo, nullptr, &buffer), "Failed to create buffer");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context_.device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(context_.physicalDevice, memRequirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(context_.device, &allocInfo, nullptr, &memory), "Failed to allocate buffer memory");

    VK_CHECK(vkBindBufferMemory(context_.device, buffer, memory, 0), "Failed to bind buffer memory");

    LOG_INFO_CAT("BufferMgr", "Buffer created successfully: buffer=0x{:x}, memory=0x{:x}", reinterpret_cast<uintptr_t>(buffer), reinterpret_cast<uintptr_t>(memory));
}

void VulkanBufferManager::reserveScratchPool(VkDeviceSize size, uint32_t count) {
    LOG_INFO_CAT("BufferMgr", "RESERVING scratch pool: {} buffers x {} B ({} MiB total)", count, size, count*size/(1024*1024));
    impl_->scratchBuffers.resize(count);
    impl_->scratchBufferMemories.resize(count);
    impl_->scratchBufferAddresses.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        VkBufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
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

        LOG_INFO_CAT("BufferMgr", "SCRATCH BUFFER #{}: buffer=0x{:x} | memory=0x{:x} | address=0x{:x} | size={} B",
                     i,
                     reinterpret_cast<uintptr_t>(impl_->scratchBuffers[i]),
                     reinterpret_cast<uintptr_t>(impl_->scratchBufferMemories[i]),
                     impl_->scratchBufferAddresses[i],
                     size);
    }
}

VkBuffer VulkanBufferManager::getVertexBuffer() const {
    return impl_->arenaBuffer;
}

VkBuffer VulkanBufferManager::getIndexBuffer() const {
    return impl_->arenaBuffer;
}

// === NEW: SCRATCH BUFFER ACCESSORS ===
VkBuffer VulkanBufferManager::getScratchBuffer(uint32_t index) const {
    if (index >= impl_->scratchBuffers.size()) {
        throw std::out_of_range(std::format("Scratch buffer index {} out of range (count: {})", index, impl_->scratchBuffers.size()));
    }
    LOG_TRACE_CAT("BufferMgr", "GET scratch buffer #{} -> 0x{:x}", index, reinterpret_cast<uintptr_t>(impl_->scratchBuffers[index]));
    return impl_->scratchBuffers[index];
}

VkDeviceAddress VulkanBufferManager::getScratchBufferAddress(uint32_t index) const {
    if (index >= impl_->scratchBufferAddresses.size()) {
        throw std::out_of_range(std::format("Scratch buffer address index {} out of range", index));
    }
    LOG_TRACE_CAT("BufferMgr", "GET scratch address #{} -> 0x{:x}", index, impl_->scratchBufferAddresses[index]);
    return impl_->scratchBufferAddresses[index];
}

uint32_t VulkanBufferManager::getScratchBufferCount() const {
    return static_cast<uint32_t>(impl_->scratchBuffers.size());
}