// src/engine/Vulkan/VulkanBufferManager.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"
#include "engine/utils.hpp"  // ← ptr_to_hex

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdio>
#include <ctime>
#include <limits>

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

namespace VulkanRTX {

using namespace Logging::Color;  // ← BRING IN COLOR PALETTE

// ---------------------------------------------------------------------------
//  TIMING HELPER (C++11 safe)
// ---------------------------------------------------------------------------
static double getMicroseconds() {
    static std::clock_t start = std::clock();
    std::clock_t now = std::clock();
    return static_cast<double>(now - start) * 1000000.0 / CLOCKS_PER_SEC;
}

// ---------------------------------------------------------------------------
//  SAFE VK_CHECK_NO_THROW FOR DESTRUCTORS
// ---------------------------------------------------------------------------
#define VK_CHECK_NO_THROW(expr, msg)                                                      \
    do {                                                                                  \
        double _start = getMicroseconds();                                                \
        VkResult _res = (expr);                                                           \
        double _end = getMicroseconds();                                                  \
        double _dur = _end - _start;                                                      \
        if (_res != VK_SUCCESS) {                                                         \
            char _buf[512];                                                               \
            std::snprintf(_buf, sizeof(_buf), "VULKAN ERROR in cleanup: %s failed -> %s (VkResult=%d) [%.1f us]", \
                          msg, #expr, static_cast<int>(_res), _dur);                      \
            LOG_ERROR_CAT("BufferMgr", "%s", _buf);                                       \
        } else {                                                                          \
            char _buf[256];                                                               \
            std::snprintf(_buf, sizeof(_buf), "VK_CLEANUP: %s -> OK [%.1f us]", #expr, _dur);\
            LOG_TRACE_CAT("BufferMgr", "%s", _buf);                                       \
        }                                                                                 \
    } while (0)

// ---------------------------------------------------------------------------
//  MEMORY TYPE FINDER (from VulkanInitializer)
// ---------------------------------------------------------------------------
static uint32_t findMemoryType(VkPhysicalDevice pd, uint32_t filter, VkMemoryPropertyFlags props) {
    return VulkanInitializer::findMemoryType(pd, filter, props);  // ← FIXED
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
    uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max();

    VkSemaphore timelineSemaphore = VK_NULL_HANDLE;
    uint64_t nextUpdateId = 1;

    explicit Impl(Vulkan::Context& ctx) : context(ctx) {}
};

// ---------------------------------------------------------------------------
//  CONTEXT-ONLY CONSTRUCTOR
// ---------------------------------------------------------------------------
VulkanBufferManager::VulkanBufferManager(Vulkan::Context& ctx)
    : context_(ctx),
      vertexCount_(0),
      indexCount_(0),
      vertexBufferAddress_(0),
      indexBufferAddress_(0),
      impl_(new Impl(ctx))
{
    LOG_INFO_CAT("BufferMgr", "{}VulkanBufferManager created (deferred) @ {}{}", 
                  ARCTIC_CYAN, ptr_to_hex(this), RESET);

    if (ctx.device == VK_NULL_HANDLE)
        throw std::runtime_error("Invalid Vulkan context");

    initializeCommandPool();

    const VkDeviceSize arenaSize = 64ULL * 1024 * 1024;
    reserveArena(arenaSize, BufferType::GEOMETRY);

    VkSemaphoreTypeCreateInfo timeline = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr, VK_SEMAPHORE_TYPE_TIMELINE, 0 };
    VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &timeline };
    VK_CHECK(vkCreateSemaphore(ctx.device, &semInfo, nullptr, &impl_->timelineSemaphore), "Timeline semaphore");
}

// ---------------------------------------------------------------------------
//  FULL CONSTRUCTOR (DELEGATES)
// ---------------------------------------------------------------------------
VulkanBufferManager::VulkanBufferManager(Vulkan::Context& ctx,
                                         const glm::vec3* vertices,
                                         size_t vertexCount,
                                         const uint32_t* indices,
                                         size_t indexCount)
    : VulkanBufferManager(ctx)
{
    uploadMesh(vertices, vertexCount, indices, indexCount);
}

// ---------------------------------------------------------------------------
//  DEFERRED MESH UPLOAD
// ---------------------------------------------------------------------------
void VulkanBufferManager::uploadMesh(const glm::vec3* vertices,
                                     size_t vertexCount,
                                     const uint32_t* indices,
                                     size_t indexCount)
{
    vertexCount_ = static_cast<uint32_t>(vertexCount);
    indexCount_  = static_cast<uint32_t>(indexCount);

    const VkDeviceSize vSize = vertexCount * sizeof(glm::vec3);
    const VkDeviceSize iSize = indexCount * sizeof(uint32_t);
    const VkDeviceSize total = vSize + iSize;

    if (total > impl_->arenaSize) {
        LOG_INFO_CAT("BufferMgr", "{}Resizing arena: {} → {} bytes{}", 
                      OCEAN_TEAL, impl_->arenaSize, total * 2, RESET);
        vkDestroyBuffer(context_.device, impl_->arenaBuffer, nullptr);
        vkFreeMemory(context_.device, impl_->arenaMemory, nullptr);
        reserveArena(std::max<VkDeviceSize>(64ULL * 1024 * 1024, total * 2), BufferType::GEOMETRY);
    }

    VkBuffer stagingV = VK_NULL_HANDLE, stagingI = VK_NULL_HANDLE;
    VkDeviceMemory memV = VK_NULL_HANDLE, memI = VK_NULL_HANDLE;

    createStagingBuffer(vSize, stagingV, memV);
    mapCopyUnmap(memV, vSize, vertices);
    createStagingBuffer(iSize, stagingI, memI);
    mapCopyUnmap(memI, iSize, indices);

    copyToArena(stagingV, 0, vSize);
    copyToArena(stagingI, impl_->indexOffset, iSize);

    vkDestroyBuffer(context_.device, stagingV, nullptr);
    vkFreeMemory(context_.device, memV, nullptr);
    vkDestroyBuffer(context_.device, stagingI, nullptr);
    vkFreeMemory(context_.device, memI, nullptr);

    vertexBufferAddress_ = VulkanInitializer::getBufferDeviceAddress(context_, impl_->arenaBuffer);
    indexBufferAddress_  = vertexBufferAddress_ + impl_->indexOffset;

    LOG_INFO_CAT("BufferMgr", "{}Mesh uploaded: {}v {}i @ {}{}", 
                  EMERALD_GREEN, vertexCount, indexCount, ptr_to_hex((void*)vertexBufferAddress_), RESET);
}

// ---------------------------------------------------------------------------
//  DESTRUCTOR
// ---------------------------------------------------------------------------
VulkanBufferManager::~VulkanBufferManager() noexcept {
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
        VK_CHECK_NO_THROW(vkResetCommandPool(context_.device, impl_->commandPool, 0), "Reset command pool");
        vkDestroyCommandPool(context_.device, impl_->commandPool, nullptr);
    }

    if (impl_->timelineSemaphore) {
        vkDestroySemaphore(context_.device, impl_->timelineSemaphore, nullptr);
    }

    delete impl_;
}

// ---------------------------------------------------------------------------
//  HELPER: STAGING BUFFER
// ---------------------------------------------------------------------------
void VulkanBufferManager::createStagingBuffer(VkDeviceSize size, VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size,
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE };
    VK_CHECK(vkCreateBuffer(context_.device, &info, nullptr, &buf), "Staging buffer");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(context_.device, buf, &reqs);

    VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, reqs.size,
        findMemoryType(context_.physicalDevice,
                       reqs.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
    VK_CHECK(vkAllocateMemory(context_.device, &alloc, nullptr, &mem), "Staging memory");
    VK_CHECK(vkBindBufferMemory(context_.device, buf, mem, 0), "Bind staging");
}

// ---------------------------------------------------------------------------
//  REST OF METHODS (unchanged)
// ---------------------------------------------------------------------------
void VulkanBufferManager::mapCopyUnmap(VkDeviceMemory mem, VkDeviceSize size, const void* data) {
    void* ptr;
    VK_CHECK(vkMapMemory(context_.device, mem, 0, size, 0, &ptr), "Map");
    std::memcpy(ptr, data, static_cast<size_t>(size));
    vkUnmapMemory(context_.device, mem);
}

void VulkanBufferManager::copyToArena(VkBuffer src, VkDeviceSize dstOffset, VkDeviceSize size) {
    VkCommandBuffer cb;
    VkCommandBufferAllocateInfo alloc = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
                                          impl_->commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
    VK_CHECK(vkAllocateCommandBuffers(context_.device, &alloc, &cb), "Alloc CB");

    VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                                       VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    VK_CHECK(vkBeginCommandBuffer(cb, &begin), "Begin CB");

    VkBufferCopy copy = { 0, dstOffset, size };
    vkCmdCopyBuffer(cb, src, impl_->arenaBuffer, 1, &copy);

    VK_CHECK(vkEndCommandBuffer(cb), "End CB");

    VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr,
                            1, &cb, 0, nullptr };
    VK_CHECK(vkQueueSubmit(impl_->transferQueue, 1, &submit, VK_NULL_HANDLE), "Submit");
    vkQueueWaitIdle(impl_->transferQueue);

    vkFreeCommandBuffers(context_.device, impl_->commandPool, 1, &cb);
}

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
    if (impl_->transferQueueFamily == std::numeric_limits<uint32_t>::max())
        impl_->transferQueueFamily = context_.graphicsQueueFamilyIndex;

    VkCommandPoolCreateInfo info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        impl_->transferQueueFamily };
    VK_CHECK(vkCreateCommandPool(context_.device, &info, nullptr, &impl_->commandPool), "Command pool");

    vkGetDeviceQueue(context_.device, impl_->transferQueueFamily, 0, &impl_->transferQueue);
}

void VulkanBufferManager::reserveArena(VkDeviceSize size, BufferType /*type*/) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

    VkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, usage, VK_SHARING_MODE_EXCLUSIVE };
    VK_CHECK(vkCreateBuffer(context_.device, &info, nullptr, &impl_->arenaBuffer), "Arena buffer");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(context_.device, impl_->arenaBuffer, &reqs);

    VkMemoryAllocateFlagsInfo flags = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, nullptr,
                                        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT };
    VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &flags, reqs.size,
        findMemoryType(context_.physicalDevice,
                       reqs.memoryTypeBits,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
    VK_CHECK(vkAllocateMemory(context_.device, &alloc, nullptr, &impl_->arenaMemory), "Arena memory");
    VK_CHECK(vkBindBufferMemory(context_.device, impl_->arenaBuffer, impl_->arenaMemory, 0), "Bind arena");

    impl_->arenaSize = size;
    impl_->vertexOffset = 0;
    impl_->indexOffset = (static_cast<VkDeviceSize>(vertexCount_) * sizeof(glm::vec3) + 255) & ~255ULL;
}

// ---------------------------------------------------------------------------
//  UPDATE BUFFERS (SYNC ONLY)
// ---------------------------------------------------------------------------
VkDeviceAddress VulkanBufferManager::updateBuffers(
    const glm::vec3* vertices,
    size_t vertexCount,
    const uint32_t* indices,
    size_t indexCount)
{
    vertexCount_ = static_cast<uint32_t>(vertexCount);
    indexCount_ = static_cast<uint32_t>(indexCount);

    const VkDeviceSize vSize = vertexCount * sizeof(glm::vec3);
    const VkDeviceSize iSize = indexCount * sizeof(uint32_t);

    VkBuffer stagingV = VK_NULL_HANDLE, stagingI = VK_NULL_HANDLE;
    VkDeviceMemory memV = VK_NULL_HANDLE, memI = VK_NULL_HANDLE;

    createStagingBuffer(vSize, stagingV, memV);
    createStagingBuffer(iSize, stagingI, memI);
    mapCopyUnmap(memV, vSize, vertices);
    mapCopyUnmap(memI, iSize, indices);

    copyToArena(stagingV, impl_->vertexOffset, vSize);
    copyToArena(stagingI, impl_->indexOffset, iSize);

    vkDestroyBuffer(context_.device, stagingV, nullptr);
    vkFreeMemory(context_.device, memV, nullptr);
    vkDestroyBuffer(context_.device, stagingI, nullptr);
    vkFreeMemory(context_.device, memI, nullptr);

    vertexBufferAddress_ = VulkanInitializer::getBufferDeviceAddress(context_, impl_->arenaBuffer);
    indexBufferAddress_  = vertexBufferAddress_ + impl_->indexOffset;

    return vertexBufferAddress_;
}

// ---------------------------------------------------------------------------
//  CREATE UNIFORM BUFFERS
// ---------------------------------------------------------------------------
void VulkanBufferManager::createUniformBuffers(uint32_t count) {
    impl_->uniformBuffers.resize(count);
    impl_->uniformBufferMemories.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VulkanBufferManager::createBuffer(context_.device,
                                          context_.physicalDevice,
                                          sizeof(UniformBufferObject),
                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                          impl_->uniformBuffers[i],
                                          impl_->uniformBufferMemories[i],
                                          nullptr,
                                          context_);

        context_.resourceManager.addBuffer(impl_->uniformBuffers[i]);
        context_.resourceManager.addMemory(impl_->uniformBufferMemories[i]);
    }
}

// ---------------------------------------------------------------------------
//  GET UNIFORM BUFFER / MEMORY
// ---------------------------------------------------------------------------
VkBuffer VulkanBufferManager::getUniformBuffer(uint32_t index) const {
    if (index >= impl_->uniformBuffers.size())
        throw std::out_of_range("Uniform buffer index out of range");
    return impl_->uniformBuffers[index];
}
VkDeviceMemory VulkanBufferManager::getUniformBufferMemory(uint32_t index) const {
    if (index >= impl_->uniformBufferMemories.size())
        throw std::out_of_range("Uniform buffer memory index out of range");
    return impl_->uniformBufferMemories[index];
}

// ---------------------------------------------------------------------------
//  RESERVE SCRATCH POOL
// ---------------------------------------------------------------------------
void VulkanBufferManager::reserveScratchPool(VkDeviceSize size, uint32_t count) {
    impl_->scratchBuffers.resize(count);
    impl_->scratchBufferMemories.resize(count);
    impl_->scratchBufferAddresses.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        VulkanBufferManager::createBuffer(context_.device,
                                          context_.physicalDevice,
                                          size,
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                          impl_->scratchBuffers[i],
                                          impl_->scratchBufferMemories[i],
                                          nullptr,
                                          context_);

        impl_->scratchBufferAddresses[i] =
            VulkanInitializer::getBufferDeviceAddress(context_, impl_->scratchBuffers[i]);

        context_.resourceManager.addBuffer(impl_->scratchBuffers[i]);
        context_.resourceManager.addMemory(impl_->scratchBufferMemories[i]);
    }
}

// ---------------------------------------------------------------------------
//  GETTERS
// ---------------------------------------------------------------------------
VkBuffer VulkanBufferManager::getVertexBuffer() const { return impl_->arenaBuffer; }
VkBuffer VulkanBufferManager::getIndexBuffer() const  { return impl_->arenaBuffer; }

VkDeviceAddress VulkanBufferManager::getVertexBufferAddress() const { return vertexBufferAddress_; }
VkDeviceAddress VulkanBufferManager::getIndexBufferAddress() const  { return indexBufferAddress_; }

VkBuffer VulkanBufferManager::getScratchBuffer(uint32_t i) const {
    if (i >= impl_->scratchBuffers.size())
        throw std::out_of_range("Scratch buffer index out of range");
    return impl_->scratchBuffers[i];
}
VkDeviceAddress VulkanBufferManager::getScratchBufferAddress(uint32_t i) const {
    if (i >= impl_->scratchBufferAddresses.size())
        throw std::out_of_range("Scratch buffer address index out of range");
    return impl_->scratchBufferAddresses[i];
}
uint32_t VulkanBufferManager::getScratchBufferCount() const {
    return static_cast<uint32_t>(impl_->scratchBuffers.size());
}

// ---------------------------------------------------------------------------
//  STATIC CREATE BUFFER (forwards to VulkanInitializer)
// ---------------------------------------------------------------------------
void VulkanBufferManager::createBuffer(VkDevice device,
                                       VkPhysicalDevice physicalDevice,
                                       VkDeviceSize size,
                                       VkBufferUsageFlags usage,
                                       VkMemoryPropertyFlags properties,
                                       VkBuffer& buffer,
                                       VkDeviceMemory& memory,
                                       const VkMemoryAllocateFlagsInfo* allocFlags,
                                       Vulkan::Context& context)
{
    VulkanInitializer::createBuffer(device, physicalDevice, size, usage, properties,
                                    buffer, memory, allocFlags, context);
}

// ---------------------------------------------------------------------------
//  STATIC GET BUFFER DEVICE ADDRESS
// ---------------------------------------------------------------------------
VkDeviceAddress VulkanBufferManager::getBufferDeviceAddress(const Vulkan::Context& context,
                                                            VkBuffer buffer)
{
    return VulkanInitializer::getBufferDeviceAddress(context, buffer);
}

// ---------------------------------------------------------------------------
//  STATIC GET AS DEVICE ADDRESS
// ---------------------------------------------------------------------------
VkDeviceAddress VulkanBufferManager::getAccelerationStructureDeviceAddress(
    const Vulkan::Context& context,
    VkAccelerationStructureKHR as)
{
    return VulkanInitializer::getAccelerationStructureDeviceAddress(context, as);
}

// ---------------------------------------------------------------------------
//  MOVED FROM HEADER (now safe)
// ---------------------------------------------------------------------------
VkBuffer VulkanBufferManager::getArenaBuffer() const { return impl_->arenaBuffer; }
VkDeviceSize VulkanBufferManager::getVertexOffset() const { return impl_->vertexOffset; }
VkDeviceSize VulkanBufferManager::getIndexOffset() const  { return impl_->indexOffset; }

VkDeviceAddress VulkanBufferManager::getDeviceAddress(VkBuffer buffer) const {
    return VulkanInitializer::getBufferDeviceAddress(context_, buffer);
}

} // namespace VulkanRTX