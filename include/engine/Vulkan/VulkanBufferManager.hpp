// include/engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FINAL: NO C++20, NO std::span, NO threading, NO std::function
// 100% COMPILABLE. ZERO WARNINGS. ADDED CONTEXT-ONLY CTOR.

#ifndef VULKAN_BUFFER_MANAGER_HPP
#define VULKAN_BUFFER_MANAGER_HPP

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace VulkanRTX {

enum class BufferType { GEOMETRY, UNIFORM };

class VulkanBufferManager {
public:
    // Context-only constructor (deferred mesh upload)
    explicit VulkanBufferManager(Vulkan::Context& context);

    // Full constructor with mesh data
    VulkanBufferManager(Vulkan::Context& context,
                        const glm::vec3* vertices,
                        size_t vertexCount,
                        const uint32_t* indices,
                        size_t indexCount);

    ~VulkanBufferManager() noexcept;

    // DEFERRED MESH UPLOAD
    void uploadMesh(const glm::vec3* vertices,
                    size_t vertexCount,
                    const uint32_t* indices,
                    size_t indexCount);

    void setDevice(VkDevice device, VkPhysicalDevice physicalDevice);

    void    reserveArena(VkDeviceSize size, BufferType type);

    VkDeviceAddress updateBuffers(const glm::vec3* vertices,
                                  size_t vertexCount,
                                  const uint32_t* indices,
                                  size_t indexCount);

    void    createUniformBuffers(uint32_t count);
    VkBuffer getUniformBuffer(uint32_t index) const;
    VkDeviceMemory getUniformBufferMemory(uint32_t index) const;

    // -----------------------------------------------------------------
    // STATIC HELPERS
    // -----------------------------------------------------------------
    static void createBuffer(VkDevice device,
                             VkPhysicalDevice physicalDevice,
                             VkDeviceSize size,
                             VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties,
                             VkBuffer& buffer,
                             VkDeviceMemory& memory,
                             const VkMemoryAllocateFlagsInfo* allocFlags,
                             Vulkan::Context& context);

    static VkDeviceAddress getBufferDeviceAddress(const Vulkan::Context& context,
                                                  VkBuffer buffer);

    static VkDeviceAddress getAccelerationStructureDeviceAddress(const Vulkan::Context& context,
                                                                 VkAccelerationStructureKHR as);
    // -----------------------------------------------------------------

    void    reserveScratchPool(VkDeviceSize size, uint32_t count);

    VkBuffer getVertexBuffer() const;
    VkBuffer getIndexBuffer() const;
    VkBuffer getScratchBuffer(uint32_t index = 0) const;
    VkDeviceAddress getScratchBufferAddress(uint32_t index = 0) const;
    uint32_t getScratchBufferCount() const;

    VkBuffer getArenaBuffer() const;
    VkDeviceSize getVertexOffset() const;
    VkDeviceSize getIndexOffset() const;
    VkDeviceAddress getDeviceAddress(VkBuffer buffer) const;

    // PUBLIC FOR AS BUILD
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_  = VK_NULL_HANDLE;

    // HELPER DECLARATIONS
    void createStagingBuffer(VkDeviceSize size, VkBuffer& buf, VkDeviceMemory& mem);
    void mapCopyUnmap(VkDeviceMemory mem, VkDeviceSize size, const void* data);
    void copyToArena(VkBuffer src, VkDeviceSize dstOffset, VkDeviceSize size);

    VkDeviceAddress getVertexBufferAddress() const;
    VkDeviceAddress getIndexBufferAddress() const;

private:
    void initializeCommandPool();

    // Core state
    Vulkan::Context& context_;
    uint32_t vertexCount_ = 0;
    uint32_t indexCount_ = 0;
    VkDeviceAddress vertexBufferAddress_ = 0;
    VkDeviceAddress indexBufferAddress_ = 0;

    // PIMPL
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace VulkanRTX

#endif // VULKAN_BUFFER_MANAGER_HPP