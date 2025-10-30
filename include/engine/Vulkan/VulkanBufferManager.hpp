// include/engine/Vulkan/VulkanBufferManager.hpp
#ifndef VULKAN_BUFFER_MANAGER_HPP
#define VULKAN_BUFFER_MANAGER_HPP

#include "engine/Vulkan/VulkanCore.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <span>
#include <vector>
#include <functional>

enum class BufferType { GEOMETRY, UNIFORM };

class VulkanBufferManager {
public:
    VulkanBufferManager() = default;
    VulkanBufferManager(Vulkan::Context& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);
    ~VulkanBufferManager();

    void setDevice(VkDevice device, VkPhysicalDevice physicalDevice);

    void reserveArena(VkDeviceSize size, BufferType type);
    VkDeviceAddress asyncUpdateBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices, std::function<void(uint64_t)> callback);
    void createUniformBuffers(uint32_t count);
    VkBuffer getUniformBuffer(uint32_t index) const;
    VkDeviceMemory getUniformBufferMemory(uint32_t index) const;
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
    void reserveScratchPool(VkDeviceSize size, uint32_t count);
    VkBuffer getVertexBuffer() const;
    VkBuffer getIndexBuffer() const;
    VkBuffer getScratchBuffer(uint32_t index = 0) const;
    VkDeviceAddress getScratchBufferAddress(uint32_t index = 0) const;
    uint32_t getScratchBufferCount() const;

    VkBuffer getArenaBuffer() const { return arenaBuffer_; }
    VkDeviceSize getVertexOffset() const { return vertexOffset_; }
    VkDeviceSize getIndexOffset() const { return indexOffset_; }

private:
    void initializeCommandPool();

    Vulkan::Context* context_ = nullptr;
    uint32_t vertexCount_ = 0;
    uint32_t indexCount_ = 0;
    VkDeviceAddress vertexBufferAddress_ = 0;
    VkDeviceAddress indexBufferAddress_ = 0;

    VkBuffer arenaBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory arenaMemory_ = VK_NULL_HANDLE;
    VkDeviceSize vertexOffset_ = 0;
    VkDeviceSize indexOffset_ = 0;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif // VULKAN_BUFFER_MANAGER_HPP