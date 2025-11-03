// include/engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: C++20, texture loading, full logging, inline device address helpers, optimized uploads

#ifndef VULKAN_BUFFER_MANAGER_HPP
#define VULKAN_BUFFER_MANAGER_HPP

#include "engine/Vulkan/VulkanCommon.hpp"
#include "VulkanCore.hpp"
#include "Vulkan_init.hpp"
#include "VulkanRTX_Setup.hpp"
#include "VulkanCommon.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <tuple>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>  // For std::span in batchCopyToArena
#include <array> // For std::array if needed

namespace VulkanRTX {

enum class BufferType { GEOMETRY, UNIFORM };

struct DimensionState; // Forward declare if not defined elsewhere

struct CopyRegion {
    VkBuffer srcBuffer;
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
    uint32_t getIndexCount() const { return indexCount_; }

    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                             VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                             VkBuffer& buffer, VkDeviceMemory& memory,
                             const VkMemoryAllocateFlagsInfo* allocFlags, Vulkan::Context& context);

    // -----------------------------------------------------------------------
    //  Device Address Helpers – inline to avoid ODR issues
    // -----------------------------------------------------------------------
    static inline VkDeviceAddress getBufferDeviceAddress(const Vulkan::Context& context, VkBuffer buffer)
    {
        VkBufferDeviceAddressInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buffer
        };
        return vkGetBufferDeviceAddress(context.device, &info);
    }

    static inline VkDeviceAddress getAccelerationStructureDeviceAddress(
        const Vulkan::Context& context, VkAccelerationStructureKHR as)
    {
        VkAccelerationStructureDeviceAddressInfoKHR info = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = as
        };
        return vkGetAccelerationStructureDeviceAddressKHR(context.device, &info);
    }

    void reserveScratchPool(VkDeviceSize size, uint32_t count);

    VkBuffer getVertexBuffer() const { return vertexBuffer_; }
    VkBuffer getIndexBuffer() const { return indexBuffer_; }
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
    void createTextureImage(const unsigned char* pixels, int width, int height, int channels, VkFormat format);
    void createTextureImageView(VkFormat format);
    void createTextureSampler();

    Vulkan::Context& context_;
    uint32_t vertexCount_ = 0;
    uint32_t indexCount_ = 0;
    VkDeviceAddress vertexBufferAddress_ = 0;
    VkDeviceAddress indexBufferAddress_ = 0;

    VkImage textureImage_ = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory_ = VK_NULL_HANDLE;
    VkImageView textureImageView_ = VK_NULL_HANDLE;
    VkSampler textureSampler_ = VK_NULL_HANDLE;

    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace VulkanRTX

#endif // VULKAN_BUFFER_MANAGER_HPP