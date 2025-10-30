// engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine, October 2025 - Vulkan buffer management header.
// Manages vertex, index, uniform, meshlet, and scratch buffers via Vulkan::Context, with cleanup offloaded to Dispose module.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp.
// Supported platforms: Linux, Windows.
// Optimized for high-end GPUs with 8 GB VRAM (e.g., NVIDIA RTX 3070, AMD RX 6800).
// Zachary Geurts 2025

#ifndef VULKAN_BUFFER_MANAGER_HPP
#define VULKAN_BUFFER_MANAGER_HPP

#include "engine/Vulkan/VulkanCore.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <span>
#include <vector>
#include <functional>

namespace Vulkan {
struct Context; // Forward declaration
}

// Meshlet data structure for mesh shader support
struct MeshletData {
    const void* data;       // Pointer to meshlet data
    VkDeviceSize size;      // Size of meshlet data in bytes
};

// Configuration for mesh shaders
struct MeshletConfig {
    uint32_t maxTriangles = 128; // Maximum triangles per meshlet
};

// Structure for buffer copy operations
struct BufferCopy {
    const void* data;       // Source data
    VkDeviceSize size;      // Size of data in bytes
    VkDeviceSize dstOffset; // Destination offset in arena
};

// Structure for descriptor updates
struct DescriptorUpdate {
    VkDescriptorSet descriptorSet; // Target descriptor set
    uint32_t binding;             // Binding number
    VkDescriptorType type;        // Descriptor type (e.g., VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    uint32_t bufferIndex;         // Index for uniform buffer
    VkDeviceSize size;            // Size of buffer range
};

// Buffer types for arena allocation
enum class BufferType {
    GEOMETRY, // Vertex, index, and meshlet data
    UNIFORM   // Uniform buffers
};

// Buffer operations for command buffer allocation
enum class BufferOperation {
    TRANSFER // Transfer operations (e.g., buffer copies)
};

class VulkanBufferManager {
public:
    // Constructs buffer manager, initializing vertex, index, uniform, and scratch buffers in a unified arena.
    // Buffers are registered with context_.resourceManager; cleanup is handled by Dispose module.
    VulkanBufferManager(Vulkan::Context& context, std::span<const glm::vec3> vertices, std::span<const uint32_t> indices);

    // Cleans up resources explicitly managed by the implementation (arena, command pool, etc.).
    ~VulkanBufferManager();

    // Reserves a unified arena for geometry or uniform buffers with specified size.
    void reserveArena(VkDeviceSize size, BufferType type);

    // Asynchronously updates vertex and index buffers in the arena, returning the vertex buffer address.
    VkDeviceAddress asyncUpdateBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices, std::function<void(uint64_t)> callback);

    // Configures uniform buffers in the arena with specified count and size per buffer.
    void configureUniformBuffers(uint32_t count, VkDeviceSize size_per_buffer);

    // Creates uniform buffers in context_.uniformBuffers and context_.uniformBufferMemories (legacy compatibility).
    void createUniformBuffers(uint32_t count);

    // Enables or disables mesh shaders with provided configuration; falls back to vertex pipeline if unsupported.
    void enableMeshShaders(bool enable, const MeshletConfig& config);

    // Adds a batch of meshlet data to the arena for mesh shader pipelines.
    void addMeshletBatch(std::span<const MeshletData> meshlets);

    // Retrieves a command buffer from the preallocated mega-pool for the specified operation.
    VkCommandBuffer getCommandBuffer(BufferOperation op);

    // Asynchronously transfers multiple buffer copies to the arena.
    void batchTransferAsync(std::span<const BufferCopy> copies, std::function<void(uint64_t)> callback);

    // Reserves a pool of scratch buffers for acceleration structure builds.
    void reserveScratchPool(VkDeviceSize size_per_buffer, uint32_t count);

    // Calculates or retrieves cached scratch buffer size for given vertex and index counts.
    VkDeviceSize getScratchSize(uint32_t vertexCount, uint32_t indexCount);

    // Creates a single scratch buffer (legacy compatibility; uses pool internally).
    void createScratchBuffer(VkDeviceSize size);

    // Updates multiple descriptor sets in a single call, supporting up to maxBindings.
    void batchDescriptorUpdate(std::span<const DescriptorUpdate> updates, uint32_t maxBindings);

    // Updates a single descriptor set with uniform buffer bindings (legacy compatibility).
    void updateDescriptorSet(VkDescriptorSet descriptorSet, uint32_t binding, uint32_t descriptorCount, VkDescriptorType descriptorType) const;

    // Checks the status of an asynchronous update by its update ID.
    bool checkUpdateStatus(uint64_t updateId) const;

    // Getters for buffer and memory handles from context_ or arena.
    VkDeviceMemory getVertexBufferMemory() const;
    VkDeviceMemory getIndexBufferMemory() const;
    VkDeviceMemory getUniformBufferMemory(uint32_t index) const;
    VkDeviceAddress getVertexBufferAddress() const;
    VkDeviceAddress getIndexBufferAddress() const;
    VkDeviceAddress getScratchBufferAddress() const;
    VkBuffer getVertexBuffer() const;
    VkBuffer getIndexBuffer() const;
    VkBuffer getMeshletBuffer(VkDeviceSize& offset_out) const;
    uint32_t getVertexCount() const;
    uint32_t getIndexCount() const;
    VkBuffer getUniformBuffer(uint32_t index) const;
    uint32_t getUniformBufferCount() const;

    // Creates a buffer with specified size, usage, and properties.
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory);

    // === NEW: EXPOSE ARENA + OFFSETS ===
    VkBuffer getArenaBuffer() const { return arenaBuffer_; }
    VkDeviceSize getVertexOffset() const { return vertexOffset_; }
    VkDeviceSize getIndexOffset() const { return indexOffset_; }

    // === NEW: SCRATCH BUFFER POOL ACCESS ===
    VkBuffer getScratchBuffer(uint32_t index = 0) const;
    VkDeviceAddress getScratchBufferAddress(uint32_t index = 0) const;
    uint32_t getScratchBufferCount() const;

private:
    // Initializes the command buffer mega-pool and transfer queue.
    void initializeCommandPool();

    // Prepares descriptor buffer info for uniform buffers.
    void prepareDescriptorBufferInfo(std::vector<VkDescriptorBufferInfo>& bufferInfos, uint32_t count) const;

    // Validates uniform buffer index to prevent out-of-bounds access.
    void validateUniformBufferIndex(uint32_t index) const;

    Vulkan::Context& context_; // Reference to Vulkan context holding buffers and memories
    uint32_t vertexCount_;     // Number of vertices
    uint32_t indexCount_;      // Number of indices
    VkDeviceAddress vertexBufferAddress_;  // Device address of vertex buffer in arena
    VkDeviceAddress indexBufferAddress_;   // Device address of index buffer in arena
    VkDeviceAddress scratchBufferAddress_; // Device address of primary scratch buffer

    // === ARENA STATE ===
    VkBuffer arenaBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory arenaMemory_ = VK_NULL_HANDLE;
    VkDeviceSize vertexOffset_ = 0;
    VkDeviceSize indexOffset_ = 0;

    struct Impl;
    std::unique_ptr<Impl> impl_; // Pimpl idiom for internal state
};

#endif // VULKAN_BUFFER_MANAGER_HPP