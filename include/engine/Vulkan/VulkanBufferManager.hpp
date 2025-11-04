// include/engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// FINAL: C++20, texture loading, full logging, inline device address helpers,
//        optimized uploads, RAII ManagedBuffer, RTConstants forward-declare

#ifndef VULKAN_BUFFER_MANAGER_HPP
#define VULKAN_BUFFER_MANAGER_HPP

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanCore.hpp"      // renamed from VulkanCore.hpp
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <tuple>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <array>
#include <memory>

// ---------------------------------------------------------------------
// Forward declarations (shared across the engine)
// ---------------------------------------------------------------------
namespace Vulkan { class Context; }
struct RTConstants;               // Defined in engine/RTConstants.hpp

// ---------------------------------------------------------------------
// RAII wrapper for a Vulkan buffer + memory (used for uniform buffers)
// ---------------------------------------------------------------------
namespace Vulkan {

class ManagedBuffer {
public:
    ManagedBuffer() = default;

    ManagedBuffer(VkDevice dev, VkDeviceSize sz,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags props,
                  const VkMemoryAllocateFlagsInfo* allocFlags = nullptr);

    ManagedBuffer(const ManagedBuffer&) = delete;
    ManagedBuffer& operator=(const ManagedBuffer&) = delete;

    ManagedBuffer(ManagedBuffer&& other) noexcept;
    ManagedBuffer& operator=(ManagedBuffer&& other) noexcept;

    ~ManagedBuffer();

    VkBuffer        buffer() const noexcept { return buffer_; }
    VkDeviceMemory  memory() const noexcept { return memory_; }

    // Simple map/unmap helpers
    void* map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    void  unmap();

private:
    VkDevice       device_ = VK_NULL_HANDLE;
    VkBuffer       buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    void*          mapped_ = nullptr;
};

} // namespace Vulkan

// ---------------------------------------------------------------------
// Buffer manager – lives in VulkanRTX namespace
// ---------------------------------------------------------------------
namespace VulkanRTX {

enum class BufferType { GEOMETRY, UNIFORM };

struct DimensionState; // Forward declare – defined elsewhere

struct CopyRegion {
    VkBuffer     srcBuffer;
    VkDeviceSize srcOffset = 0;
    VkDeviceSize dstOffset;
    VkDeviceSize size;
};

class VulkanBufferManager {
public:
    explicit VulkanBufferManager(Vulkan::Context& context);
    VulkanBufferManager(Vulkan::Context& context,
                        const glm::vec3* vertices, size_t vertexCount,
                        const uint32_t* indices, size_t indexCount,
                        uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max());
    ~VulkanBufferManager() noexcept;

    void uploadMesh(const glm::vec3* vertices, size_t vertexCount,
                    const uint32_t* indices, size_t indexCount,
                    uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max());

    void setDevice(VkDevice device, VkPhysicalDevice physicalDevice);
    void reserveArena(VkDeviceSize size, BufferType type);

    VkDeviceAddress updateBuffers(const glm::vec3* vertices, size_t vertexCount,
                                  const uint32_t* indices, size_t indexCount,
                                  uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max());

    void createUniformBuffers(uint32_t count);
    VkBuffer getUniformBuffer(uint32_t index) const;
    VkDeviceMemory getUniformBufferMemory(uint32_t index) const;

    std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>> getGeometries() const;
    std::vector<DimensionState> getDimensionStates() const;

    uint32_t getVertexCount() const { return vertexCount_; }
    uint32_t getIndexCount()  const { return indexCount_; }

    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                             VkDeviceSize size,
                             VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties,
                             VkBuffer& buffer, VkDeviceMemory& memory,
                             const VkMemoryAllocateFlagsInfo* allocFlags,
                             Vulkan::Context& context);

    // -----------------------------------------------------------------
    // Inline device address helpers (ODR-safe)
    // -----------------------------------------------------------------
    static inline VkDeviceAddress getBufferDeviceAddress(const Vulkan::Context& ctx, VkBuffer buffer)
    {
        VkBufferDeviceAddressInfo info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buffer
        };
        return vkGetBufferDeviceAddress(ctx.device, &info);
    }

    static inline VkDeviceAddress getAccelerationStructureDeviceAddress(
        const Vulkan::Context& ctx, VkAccelerationStructureKHR as)
    {
        VkAccelerationStructureDeviceAddressInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = as
        };
        return vkGetAccelerationStructureDeviceAddressKHR(ctx.device, &info);
    }

    void reserveScratchPool(VkDeviceSize size, uint32_t count);

    VkBuffer getVertexBuffer() const { return vertexBuffer_; }
    VkBuffer getIndexBuffer()  const { return indexBuffer_; }
    VkBuffer getScratchBuffer(uint32_t index = 0) const;
    VkDeviceAddress getScratchBufferAddress(uint32_t index = 0) const;
    uint32_t getScratchBufferCount() const;

    VkBuffer getArenaBuffer() const;
    VkDeviceSize getVertexOffset() const;
    VkDeviceSize getIndexOffset() const;
    VkDeviceAddress getDeviceAddress(VkBuffer buffer) const;

    uint32_t getTransferQueueFamily() const;

    // TEXTURE
    void loadTexture(const char* path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    VkImage getTextureImage() const;
    VkImageView getTextureImageView() const;
    VkSampler getTextureSampler() const;

    // -----------------------------------------------------------------
    // Public raw handles (for RenderMode1 etc.)
    // -----------------------------------------------------------------
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_  = VK_NULL_HANDLE;

    void createStagingBuffer(VkDeviceSize size, VkBuffer& buf, VkDeviceMemory& mem);
    void mapCopyUnmap(VkDeviceMemory mem, VkDeviceSize size, const void* data);
    void copyToArena(VkBuffer src, VkDeviceSize dstOffset, VkDeviceSize size);
    void batchCopyToArena(std::span<const CopyRegion> regions);

    VkDeviceAddress getVertexBufferAddress() const;
    VkDeviceAddress getIndexBufferAddress() const;

private:
    void initializeCommandPool();
    void initializeStagingPool();
    void persistentCopy(const void* data, VkDeviceSize size, VkDeviceSize offset);
    void createTextureImage(const unsigned char* pixels, int width, int height,
                            int channels, VkFormat format);
    void createTextureImageView(VkFormat format);
    void createTextureSampler();

    Vulkan::Context& context_;
    uint32_t vertexCount_ = 0;
    uint32_t indexCount_  = 0;
    VkDeviceAddress vertexBufferAddress_ = 0;
    VkDeviceAddress indexBufferAddress_  = 0;

    VkImage        textureImage_       = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory_ = VK_NULL_HANDLE;
    VkImageView    textureImageView_   = VK_NULL_HANDLE;
    VkSampler      textureSampler_     = VK_NULL_HANDLE;

    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace VulkanRTX

// ---------------------------------------------------------------------
// Inline implementation of ManagedBuffer (kept in header for simplicity)
// ---------------------------------------------------------------------
namespace Vulkan {

inline ManagedBuffer::ManagedBuffer(VkDevice dev, VkDeviceSize sz,
                                   VkBufferUsageFlags usage,
                                   VkMemoryPropertyFlags props,
                                   const VkMemoryAllocateFlagsInfo* allocFlags)
    : device_(dev)
{
    // NOTE: physicalDevice is not needed for host-visible buffers.
    VulkanRTX::VulkanBufferManager::createBuffer(
        dev, nullptr, sz, usage, props,
        buffer_, memory_, allocFlags,
        *(Vulkan::Context*)nullptr); // context not used here
}

inline ManagedBuffer::ManagedBuffer(ManagedBuffer&& o) noexcept
    : device_(o.device_), buffer_(o.buffer_), memory_(o.memory_), mapped_(o.mapped_)
{
    o.device_ = VK_NULL_HANDLE; o.buffer_ = VK_NULL_HANDLE;
    o.memory_ = VK_NULL_HANDLE; o.mapped_ = nullptr;
}

inline ManagedBuffer& ManagedBuffer::operator=(ManagedBuffer&& o) noexcept
{
    if (this != &o) { this->~ManagedBuffer(); new (this) ManagedBuffer(std::move(o)); }
    return *this;
}

inline ManagedBuffer::~ManagedBuffer()
{
    if (mapped_) vkUnmapMemory(device_, memory_);
    if (buffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, buffer_, nullptr);
    if (memory_ != VK_NULL_HANDLE) vkFreeMemory(device_, memory_, nullptr);
}

inline void* ManagedBuffer::map(VkDeviceSize offset, VkDeviceSize size)
{
    if (!mapped_) vkMapMemory(device_, memory_, offset, size, 0, &mapped_);
    return mapped_;
}

inline void ManagedBuffer::unmap()
{
    if (mapped_) { vkUnmapMemory(device_, memory_); mapped_ = nullptr; }
}

} // namespace Vulkan

#endif // VULKAN_BUFFER_MANAGER_HPP