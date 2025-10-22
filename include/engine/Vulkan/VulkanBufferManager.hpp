// engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine, October 2025 - Vulkan buffer management header.
// Manages vertex, index, uniform, and scratch buffers via Vulkan::Context, with cleanup offloaded to Dispose module.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp.
// Supported platforms: Linux, Windows.
// Optimized for high-end GPUs with 8 GB VRAM (e.g., NVIDIA RTX 3070, AMD RX 6800).
// Zachary Geurts 2025

#pragma once
#include "engine/Vulkan/VulkanCore.hpp" // Moved to top to ensure Vulkan namespace is defined
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <span>
#include <vector>

class VulkanBufferManager {
public:
    // Constructs buffer manager, initializing vertex and index buffers in context_.
    // Buffers are registered with context_.resourceManager; cleanup is handled by Dispose::cleanupVulkanContext.
    VulkanBufferManager(Vulkan::Context& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);

    // No cleanup in destructor; resources are managed by Dispose module.
    ~VulkanBufferManager() = default;

    // Creates uniform buffers in context_.uniformBuffers and context_.uniformBufferMemories.
    void createUniformBuffers(uint32_t count);

    // Creates scratch buffer in context_.scratchBuffer and context_.scratchBufferMemory.
    void createScratchBuffer(VkDeviceSize size);

    // Calculates required scratch buffer size for acceleration structure builds.
    VkDeviceSize calculateScratchBufferSize();

    // Prepares descriptor buffer info for uniform buffers.
    void prepareDescriptorBufferInfo(std::vector<VkDescriptorBufferInfo>& bufferInfos, uint32_t count) const;

    // Updates descriptor set with uniform buffer bindings.
    void updateDescriptorSet(VkDescriptorSet descriptorSet, uint32_t binding, uint32_t descriptorCount, VkDescriptorType descriptorType) const;

    // Getters for buffer and memory handles from context_.
    VkDeviceMemory getVertexBufferMemory() const;
    VkDeviceMemory getIndexBufferMemory() const;
    VkDeviceMemory getUniformBufferMemory(uint32_t index) const;
    VkDeviceAddress getVertexBufferAddress() const;
    VkDeviceAddress getIndexBufferAddress() const;
    VkDeviceAddress getScratchBufferAddress() const;
    VkBuffer getVertexBuffer() const;
    VkBuffer getIndexBuffer() const;
    uint32_t getVertexCount() const;
    uint32_t getIndexCount() const;
    VkBuffer getUniformBuffer(uint32_t index) const;
    uint32_t getUniformBufferCount() const;

private:
    Vulkan::Context& context_; // Reference to Vulkan context holding buffers and memories.
    uint32_t vertexCount_;     // Number of vertices.
    uint32_t indexCount_;      // Number of indices.
    VkDeviceAddress vertexBufferAddress_;  // Device address of vertex buffer.
    VkDeviceAddress indexBufferAddress_;   // Device address of index buffer.
    VkDeviceAddress scratchBufferAddress_; // Device address of scratch buffer.

    // Validates uniform buffer index to prevent out-of-bounds access.
    void validateUniformBufferIndex(uint32_t index) const;
};