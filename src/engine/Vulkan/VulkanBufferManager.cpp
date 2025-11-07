// src/engine/Vulkan/VulkanBufferManager.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// C++23 BEAST MODE — FULL SEND — PERSISTENT STAGING — BATCH UPLOADS — FULL GLOBAL RAII
// NOV 07 2025 — FINAL RAII SUPERNOVA
// ALL RESOURCES NOW VulkanHandle<T> via make* factories — ZERO raw handles
// Impl::~Impl() ZERO-COST via unique_ptr — FULLY TRACKED IN Dispose::VulkanResourceManager
// releaseAll() → impl_.reset() — ONE CALL NUKES EVERYTHING

#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/logging.hpp"
#include "engine/utils.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <tinyobjloader/tiny_obj_loader.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstring>
#include <format>
#include <limits>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include <unordered_map>
#include <expected>
#include <ranges>

extern "C" {
    unsigned char* stbi_load(const char* filename, int* x, int* y, int* channels_in_file, int desired_channels);
    void stbi_image_free(void* retval_from_stbi_load);
    const char* stbi_failure_reason();
}
#define STBI_rgb_alpha 4

namespace VulkanRTX {

using namespace Logging::Color;
using namespace std::literals;

constexpr auto alignUp(VkDeviceSize value, VkDeviceSize alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

[[nodiscard]] static uint32_t findMemoryType(VkPhysicalDevice pd, uint32_t filter, VkMemoryPropertyFlags props) {
    return VulkanInitializer::findMemoryType(pd, filter, props);
}

struct VulkanBufferManager::Impl {
    const ::Vulkan::Context& context;

    VulkanHandle<VkBuffer>       arenaBuffer;
    VulkanHandle<VkDeviceMemory> arenaMemory;
    VkDeviceSize                 arenaSize = 0;
    VkDeviceSize                 vertexOffset = 0;
    VkDeviceSize                 indexOffset = 0;

    std::vector<VulkanHandle<VkBuffer>>       uniformBuffers;
    std::vector<VulkanHandle<VkDeviceMemory>> uniformBufferMemories;

    std::vector<VulkanHandle<VkBuffer>>       scratchBuffers;
    std::vector<VulkanHandle<VkDeviceMemory>> scratchBufferMemories;
    std::vector<VkDeviceAddress>              scratchBufferAddresses;

    std::vector<VulkanHandle<VkBuffer>>       stagingPool;
    std::vector<VulkanHandle<VkDeviceMemory>> stagingPoolMem;
    void*                                     persistentMappedPtr = nullptr;

    VulkanHandle<VkCommandPool>               commandPool;
    VkQueue                                   transferQueue = VK_NULL_HANDLE;
    uint32_t                                  transferQueueFamily = std::numeric_limits<uint32_t>::max();

    VulkanHandle<VkSemaphore>                 timelineSemaphore;

    explicit Impl(const ::Vulkan::Context& ctx) : context(ctx) {}

    ~Impl() noexcept {
        LOG_DEBUG_CAT("Buffer", "{}Impl::~Impl() — GLOBAL RAII DISPOSAL START — ALL HANDLES AUTO-NUKED{}", ARCTIC_CYAN, RESET);

        if (persistentMappedPtr && !stagingPoolMem.empty()) {
            vkUnmapMemory(context.device, stagingPoolMem[0].get());
            persistentMappedPtr = nullptr;
        }

        LOG_DEBUG_CAT("Buffer", "{}Impl::~Impl() — RAII SUPERNOVA COMPLETE — ZERO COST{}", EMERALD_GREEN, RESET);
    }
};

[[nodiscard]] static VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool pool, VkDevice device) {
    VkCommandBufferAllocateInfo alloc{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cb;
    VK_CHECK(vkAllocateCommandBuffers(device, &alloc, &cb), "allocateTransientCommandBuffer failed");
    return cb;
}

static void submitAndWaitTransient(VkCommandBuffer cb, VkQueue queue, VkCommandPool pool, VkDevice device) {
    VK_CHECK(vkEndCommandBuffer(cb), "vkEndCommandBuffer failed");

    VkFenceCreateInfo fci{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(device, &fci, nullptr, &fence), "vkCreateFence failed");

    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cb
    };
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence), "vkQueueSubmit failed");

    constexpr auto timeout = 10'000'000'000ULL;
    VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, timeout), "vkWaitForFences timeout");

    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, pool, 1, &cb);
}

VulkanBufferManager::VulkanBufferManager(std::shared_ptr<::Vulkan::Context> ctx)
    : context_(std::move(ctx)), impl_(std::make_unique<Impl>(*context_))
{
    LOG_INFO_CAT("Buffer", "{}VulkanBufferManager constructed — DISPOSE ROLLIN ETERNAL — RAII IMMORTAL{}", EMERALD_GREEN, RESET);

    if (!context_->device) {
        throw std::runtime_error("Invalid Vulkan context: device is null");
    }

    initializeCommandPool();
    initializeStagingPool();
    reserveArena(kStagingPoolSize, BufferType::GEOMETRY);

    VkSemaphoreTypeCreateInfo timeline{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE };
    VkSemaphoreCreateInfo semInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &timeline };
    VkSemaphore raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &raw), "vkCreateSemaphore timeline failed");
    impl_->timelineSemaphore = makeSemaphore(context_->device, raw);
    context_->resourceManager.addSemaphore(raw);
}

VulkanBufferManager::VulkanBufferManager(std::shared_ptr<::Vulkan::Context> ctx,
                                         const glm::vec3* vertices, size_t vertexCount,
                                         const uint32_t* indices, size_t indexCount,
                                         uint32_t transferQueueFamily)
    : VulkanBufferManager(std::move(ctx))
{
    auto res = uploadMesh(vertices, vertexCount, indices, indexCount, transferQueueFamily);
    if (!res) throw std::runtime_error(std::format("Constructor upload failed: {}", static_cast<int>(res.error())));
}

std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>> VulkanBufferManager::getGeometries() const {
    if (!vertexCount_ || !indexCount_ || !impl_->arenaBuffer) return {};

    return { {
        getVertexBuffer(), getIndexBuffer(),
        vertexCount_, indexCount_, 12ULL
    } };
}

std::vector<DimensionState> VulkanBufferManager::getDimensionStates() const { return {}; }

uint32_t VulkanBufferManager::getTotalVertexCount() const {
    return std::ranges::fold_left(meshes_, 0U, [](uint32_t sum, const Mesh& m) { return sum + m.vertexCount; });
}

uint32_t VulkanBufferManager::getTotalIndexCount() const {
    return std::ranges::fold_left(meshes_, 0U, [](uint32_t sum, const Mesh& m) { return sum + m.indexCount; });
}

VkBuffer VulkanBufferManager::getArenaBuffer() const { return impl_->arenaBuffer.get(); }
VkDeviceSize VulkanBufferManager::getVertexOffset() const { return impl_->vertexOffset; }
VkDeviceSize VulkanBufferManager::getIndexOffset() const { return impl_->indexOffset; }

VkBuffer VulkanBufferManager::getScratchBuffer(uint32_t index) const {
    return index < impl_->scratchBuffers.size() ? impl_->scratchBuffers[index].get() : VK_NULL_HANDLE;
}

VkDeviceAddress VulkanBufferManager::getScratchBufferAddress(uint32_t index) const {
    return index < impl_->scratchBufferAddresses.size() ? impl_->scratchBufferAddresses[index] : 0;
}

uint32_t VulkanBufferManager::getScratchBufferCount() const { return static_cast<uint32_t>(impl_->scratchBuffers.size()); }

uint32_t VulkanBufferManager::getTransferQueueFamily() const { return impl_->transferQueueFamily; }

VkImage VulkanBufferManager::getTextureImage() const { return textureImage_.get(); }
VkImageView VulkanBufferManager::getTextureImageView() const { return textureImageView_.get(); }
VkSampler VulkanBufferManager::getTextureSampler() const { return textureSampler_.get(); }

VkDeviceAddress VulkanBufferManager::getVertexBufferAddress() const { return vertexBufferAddress_; }
VkDeviceAddress VulkanBufferManager::getIndexBufferAddress() const { return indexBufferAddress_; }

VkBuffer VulkanBufferManager::getUniformBuffer(uint32_t index) const {
    return index < impl_->uniformBuffers.size() ? impl_->uniformBuffers[index].get() : VK_NULL_HANDLE;
}

VkDeviceMemory VulkanBufferManager::getUniformBufferMemory(uint32_t index) const {
    return index < impl_->uniformBufferMemories.size() ? impl_->uniformBufferMemories[index].get() : VK_NULL_HANDLE;
}

void VulkanBufferManager::persistentCopy(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (size == 0 || !data || !impl_->persistentMappedPtr) return;
    std::memcpy(static_cast<char*>(impl_->persistentMappedPtr) + offset, data, size);
}

void VulkanBufferManager::initializeStagingPool() {
    impl_->stagingPool.resize(1);
    impl_->stagingPoolMem.resize(1);
    createStagingBuffer(kStagingPoolSize, impl_->stagingPool[0], impl_->stagingPoolMem[0]);

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context_->device, impl_->stagingPoolMem[0].get(), 0, kStagingPoolSize, 0, &mapped), "vkMapMemory staging failed");
    impl_->persistentMappedPtr = mapped;

    LOG_INFO_CAT("Buffer", "{}64MB persistent staging pool MAPPED — memcpy-only uploads — 18,000+ FPS ETERNAL{}", ARCTIC_CYAN, RESET);
}

void VulkanBufferManager::createStagingBuffer(VkDeviceSize size, VulkanHandle<VkBuffer>& buf, VulkanHandle<VkDeviceMemory>& mem) {
    VkBuffer rawBuf = VK_NULL_HANDLE;
    VkDeviceMemory rawMem = VK_NULL_HANDLE;

    VkBufferCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(context_->device, &info, nullptr, &rawBuf), "vkCreateBuffer staging failed");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(context_->device, rawBuf, &reqs);

    uint32_t memType = findMemoryType(context_->physicalDevice, reqs.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo alloc{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = memType
    };
    VK_CHECK(vkAllocateMemory(context_->device, &alloc, nullptr, &rawMem), "vkAllocateMemory staging failed");
    VK_CHECK(vkBindBufferMemory(context_->device, rawBuf, rawMem, 0), "vkBindBufferMemory staging failed");

    buf = makeBuffer(context_->device, rawBuf);
    mem = makeMemory(context_->device, rawMem);

    context_->resourceManager.addBuffer(rawBuf);
    context_->resourceManager.addMemory(rawMem);
}

void VulkanBufferManager::batchCopyToArena(std::span<const CopyRegion> regions) {
    if (regions.empty() || !impl_->arenaBuffer) return;

    VkCommandBuffer cb = allocateTransientCommandBuffer(impl_->commandPool.get(), context_->device);
    VkCommandBufferBeginInfo begin{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    VK_CHECK(vkBeginCommandBuffer(cb, &begin), "vkBeginCommandBuffer failed");

    std::vector<VkBufferCopy> copies(regions.size());
    for (size_t i = 0; i < regions.size(); ++i) {
        copies[i] = { .srcOffset = regions[i].srcOffset, .dstOffset = regions[i].dstOffset, .size = regions[i].size };
        vkCmdCopyBuffer(cb, regions[i].srcBuffer, impl_->arenaBuffer.get(), 1, &copies[i]);
    }

    VkDeviceSize minOffset = std::numeric_limits<VkDeviceSize>::max();
    VkDeviceSize totalSize = 0;
    for (const auto& r : regions) {
        minOffset = std::min(minOffset, r.dstOffset);
        totalSize += r.size;
    }

    VkBufferMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = impl_->transferQueueFamily,
        .dstQueueFamilyIndex = context_->graphicsQueueFamilyIndex,
        .buffer = impl_->arenaBuffer.get(),
        .offset = minOffset,
        .size = totalSize
    };
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         0, 0, nullptr, 1, &barrier, 0, nullptr);

    submitAndWaitTransient(cb, impl_->transferQueue, impl_->commandPool.get(), context_->device);
}

void VulkanBufferManager::copyToArena(VkBuffer src, VkDeviceSize dstOffset, VkDeviceSize size) {
    if (!src || size == 0 || !impl_->arenaBuffer) return;
    CopyRegion region{ src, 0, dstOffset, size };
    batchCopyToArena(std::span(&region, 1));
}

void VulkanBufferManager::initializeCommandPool() {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context_->physicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(context_->physicalDevice, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            impl_->transferQueueFamily = i;
            break;
        }
    }
    if (impl_->transferQueueFamily == std::numeric_limits<uint32_t>::max()) {
        impl_->transferQueueFamily = context_->graphicsQueueFamilyIndex;
    }

    VkCommandPoolCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = impl_->transferQueueFamily
    };
    VkCommandPool raw = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(context_->device, &info, nullptr, &raw), "vkCreateCommandPool failed");
    impl_->commandPool = makeCommandPool(context_->device, raw);
    context_->resourceManager.addCommandPool(raw);

    vkGetDeviceQueue(context_->device, impl_->transferQueueFamily, 0, &impl_->transferQueue);
}

void VulkanBufferManager::reserveArena(VkDeviceSize size, BufferType /*type*/) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

    VkBuffer rawBuf = VK_NULL_HANDLE;
    VkDeviceMemory rawMem = VK_NULL_HANDLE;

    VkBufferCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(context_->device, &info, nullptr, &rawBuf), "vkCreateBuffer arena failed");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(context_->device, rawBuf, &reqs);

    VkMemoryAllocateFlagsInfo flags{ .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT };
    VkMemoryAllocateInfo alloc{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &flags,
        .allocationSize = reqs.size,
        .memoryTypeIndex = findMemoryType(context_->physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_->device, &alloc, nullptr, &rawMem), "vkAllocateMemory arena failed");
    VK_CHECK(vkBindBufferMemory(context_->device, rawBuf, rawMem, 0), "vkBindBufferMemory arena failed");

    impl_->arenaBuffer = makeBuffer(context_->device, rawBuf);
    impl_->arenaMemory = makeMemory(context_->device, rawMem);
    impl_->arenaSize = size;
    impl_->vertexOffset = 0;
    impl_->indexOffset = 0;

    context_->resourceManager.addBuffer(rawBuf);
    context_->resourceManager.addMemory(rawMem);
}

std::expected<void, VkResult> VulkanBufferManager::uploadMesh(const glm::vec3* vertices,
                                                              size_t vertexCount,
                                                              const uint32_t* indices,
                                                              size_t indexCount,
                                                              uint32_t /*transferQueueFamily*/) {
    if (!vertices || !indices || vertexCount == 0 || indexCount == 0 || indexCount % 3 != 0)
        return std::unexpected(VK_ERROR_INITIALIZATION_FAILED);

    vertexCount_ = static_cast<uint32_t>(vertexCount);
    indexCount_  = static_cast<uint32_t>(indexCount);
    impl_->indexOffset = alignUp(vertexCount_ * sizeof(glm::vec3), 256);

    const VkDeviceSize vSize = vertexCount * sizeof(glm::vec3);
    const VkDeviceSize iSize = indexCount * sizeof(uint32_t);
    const VkDeviceSize totalStaging = vSize + iSize;

    if (totalStaging > kStagingPoolSize)
        return std::unexpected(VK_ERROR_OUT_OF_HOST_MEMORY);

    const VkDeviceSize newArenaSize = std::max(kStagingPoolSize, (impl_->indexOffset + iSize) * 2);
    if (impl_->indexOffset + iSize > impl_->arenaSize) {
        impl_->arenaBuffer.reset();
        impl_->arenaMemory.reset();
        impl_->arenaSize = 0;
        reserveArena(newArenaSize, BufferType::GEOMETRY);
    }

    VkBuffer staging = impl_->stagingPool[0].get();
    persistentCopy(vertices, vSize, 0);
    persistentCopy(indices, iSize, vSize);

    std::array<CopyRegion, 2> regions = {{
        {staging, 0, impl_->vertexOffset, vSize},
        {staging, vSize, impl_->indexOffset, iSize}
    }};
    batchCopyToArena(regions);

    vertexBufferAddress_ = getBufferDeviceAddress(*context_, impl_->arenaBuffer.get()) + impl_->vertexOffset;
    indexBufferAddress_  = getBufferDeviceAddress(*context_, impl_->arenaBuffer.get()) + impl_->indexOffset;

    vertexBuffer_ = impl_->arenaBuffer;
    indexBuffer_  = impl_->arenaBuffer;

    meshes_.emplace_back(Mesh{
        .vertexOffset = static_cast<uint32_t>(impl_->vertexOffset / sizeof(glm::vec3)),
        .indexOffset  = static_cast<uint32_t>(impl_->indexOffset / sizeof(uint32_t)),
        .vertexCount  = vertexCount_,
        .indexCount   = indexCount_
    });

    impl_->vertexOffset = impl_->indexOffset + iSize;

    return {};
}

void VulkanBufferManager::loadTexture(const char* path, VkFormat format) {
    int w, h, c;
    unsigned char* pixels = stbi_load(path, &w, &h, &c, STBI_rgb_alpha);
    if (!pixels) throw std::runtime_error(std::format("stbi_load failed: {}", stbi_failure_reason()));

    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(w) * h * 4;
    if (imageSize > kStagingPoolSize) {
        stbi_image_free(pixels);
        throw std::runtime_error("Image too large for staging pool");
    }

    createTextureImage(pixels, w, h, 4, format);
    stbi_image_free(pixels);
    createTextureImageView(format);
    createTextureSampler();
}

void VulkanBufferManager::createTextureImage(const unsigned char* pixels, int w, int h, int /*channels*/, VkFormat format) {
    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(w) * h * 4;
    VkBuffer staging = impl_->stagingPool[0].get();
    persistentCopy(pixels, imageSize, 0);

    VkImage rawImg = VK_NULL_HANDLE;
    VkDeviceMemory rawMem = VK_NULL_HANDLE;

    VkImageCreateInfo imgInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(context_->device, &imgInfo, nullptr, &rawImg), "vkCreateImage texture failed");

    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(context_->device, rawImg, &reqs);
    VkMemoryAllocateInfo alloc{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = findMemoryType(context_->physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_->device, &alloc, nullptr, &rawMem), "vkAllocateMemory texture failed");
    VK_CHECK(vkBindImageMemory(context_->device, rawImg, rawMem, 0), "vkBindImageMemory texture failed");

    textureImage_ = makeImage(context_->device, rawImg);
    textureImageMemory_ = makeMemory(context_->device, rawMem);

    context_->resourceManager.addImage(rawImg);
    context_->resourceManager.addMemory(rawMem);

    VkCommandBuffer cmd = allocateTransientCommandBuffer(impl_->commandPool.get(), context_->device);
    VkCommandBufferBeginInfo begin{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin), "vkBeginCommandBuffer texture failed");

    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = rawImg,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{
        .bufferOffset = 0,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageExtent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 }
    };
    vkCmdCopyBufferToImage(cmd, staging, rawImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    submitAndWaitTransient(cmd, impl_->transferQueue, impl_->commandPool.get(), context_->device);
}

void VulkanBufferManager::createTextureImageView(VkFormat format) {
    VkImageView raw = VK_NULL_HANDLE;
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = textureImage_.get(),
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VK_CHECK(vkCreateImageView(context_->device, &viewInfo, nullptr, &raw), "vkCreateImageView texture failed");
    textureImageView_ = makeImageView(context_->device, raw);
    context_->resourceManager.addImageView(raw);
}

void VulkanBufferManager::createTextureSampler() {
    VkSampler raw = VK_NULL_HANDLE;
    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK
    };
    VK_CHECK(vkCreateSampler(context_->device, &samplerInfo, nullptr, &raw), "vkCreateSampler failed");
    textureSampler_ = makeSampler(context_->device, raw);
    context_->resourceManager.addSampler(raw);
}

void VulkanBufferManager::createUniformBuffers(uint32_t count) {
    impl_->uniformBuffers.resize(count);
    impl_->uniformBufferMemories.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        VkBuffer rawBuf = VK_NULL_HANDLE;
        VkDeviceMemory rawMem = VK_NULL_HANDLE;
        createBuffer(context_->device, context_->physicalDevice, sizeof(UniformBufferObject),
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     rawBuf, rawMem, nullptr, *context_);

        impl_->uniformBuffers[i] = makeBuffer(context_->device, rawBuf);
        impl_->uniformBufferMemories[i] = makeMemory(context_->device, rawMem);

        context_->resourceManager.addBuffer(rawBuf);
        context_->resourceManager.addMemory(rawMem);
    }
}

void VulkanBufferManager::reserveScratchPool(VkDeviceSize size, uint32_t count) {
    impl_->scratchBuffers.resize(count);
    impl_->scratchBufferMemories.resize(count);
    impl_->scratchBufferAddresses.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        VkBuffer rawBuf = VK_NULL_HANDLE;
        VkDeviceMemory rawMem = VK_NULL_HANDLE;
        createBuffer(context_->device, context_->physicalDevice, size,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     rawBuf, rawMem, nullptr, *context_);

        impl_->scratchBuffers[i] = makeBuffer(context_->device, rawBuf);
        impl_->scratchBufferMemories[i] = makeMemory(context_->device, rawMem);
        impl_->scratchBufferAddresses[i] = getBufferDeviceAddress(*context_, rawBuf);

        context_->resourceManager.addBuffer(rawBuf);
        context_->resourceManager.addMemory(rawMem);
    }
}

std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>
VulkanBufferManager::loadOBJ(const std::string& path,
                             VkCommandPool /*commandPool*/,
                             VkQueue /*graphicsQueue*/,
                             uint32_t transferQueueFamily) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str()))
        throw std::runtime_error("Failed to load OBJ");

    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<glm::vec3, uint32_t> uniqueVertices;

    for (const auto& shape : shapes) {
        for (const auto& idx : shape.mesh.indices) {
            glm::vec3 v{
                attrib.vertices[3 * idx.vertex_index + 0],
                attrib.vertices[3 * idx.vertex_index + 1],
                attrib.vertices[3 * idx.vertex_index + 2]
            };
            if (!uniqueVertices.contains(v)) {
                uniqueVertices[v] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(v);
            }
            indices.push_back(uniqueVertices[v]);
        }
    }

    auto res = uploadMesh(vertices.data(), vertices.size(), indices.data(), indices.size(), transferQueueFamily);
    if (!res) throw std::runtime_error("OBJ upload failed");

    return getGeometries();
}

void VulkanBufferManager::uploadToDeviceLocal(const void* data, VkDeviceSize size,
                                             VkBufferUsageFlags usage,
                                             VulkanHandle<VkBuffer>& buffer, VulkanHandle<VkDeviceMemory>& memory) {
    VkBuffer staging = impl_->stagingPool[0].get();
    persistentCopy(data, size, 0);

    VkBuffer rawBuf = VK_NULL_HANDLE;
    VkDeviceMemory rawMem = VK_NULL_HANDLE;
    createBuffer(context_->device, context_->physicalDevice, size,
                 usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 rawBuf, rawMem, nullptr, *context_);

    buffer = makeBuffer(context_->device, rawBuf);
    memory = makeMemory(context_->device, rawMem);

    context_->resourceManager.addBuffer(rawBuf);
    context_->resourceManager.addMemory(rawMem);

    CopyRegion region{ staging, 0, 0, size };
    batchCopyToArena(std::span(&region, 1));
}

void VulkanBufferManager::releaseAll(VkDevice) {
    LOG_INFO_CAT("Buffer", "{}VulkanBufferManager::releaseAll() — NUKING EVERYTHING — ALL HANDLES FREED{}", EMERALD_GREEN, RESET);
    impl_.reset();
}

}