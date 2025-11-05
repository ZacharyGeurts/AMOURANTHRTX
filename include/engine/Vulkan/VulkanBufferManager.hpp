// include/engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: C++20, texture loading, full logging, inline device address helpers,
//        optimized uploads, RAII ManagedBuffer, RTConstants forward-declare
// FIXED: All members (vertexCount_, etc.) + all functions declared
// FIXED: Constructor takes std::shared_ptr<Vulkan::Context> | Matches .cpp
// FIXED: Added getMeshes(), generateSphere(), getTotalVertexCount(), getTotalIndexCount()
// NEW: generateCube() – fallback geometry
// NEW: loadOBJ() – tinyobjloader, dedup vertices, upload to GPU, return geometry data
// GROK PROTIPS: Persistent staging pool, batch uploads, Dispose integration

#ifndef VULKAN_BUFFER_MANAGER_HPP
#define VULKAN_BUFFER_MANAGER_HPP

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/logging.hpp"

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
#include <string>
#include <unordered_map>
#include <functional>  // std::hash

namespace Vulkan { class Context; }
struct RTConstants;  // Per-frame push constants

namespace Vulkan {

class ManagedBuffer {
public:
    ManagedBuffer() = default;

    void generateSphere(float radius, unsigned int latitudeBands, unsigned int longitudeBands);
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
// std::hash + std::equal_to for glm::vec3 (required for unordered_map)
// ---------------------------------------------------------------------
namespace std {
    template<> struct hash<glm::vec3> {
        size_t operator()(const glm::vec3& v) const noexcept {
            auto h1 = hash<float>{}(v.x);
            auto h2 = hash<float>{}(v.y);
            auto h3 = hash<float>{}(v.z);
            return ((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1);
        }
    };

    template<> struct equal_to<glm::vec3> {
        bool operator()(const glm::vec3& a, const glm::vec3& b) const noexcept {
            return glm::all(glm::equal(a, b));
        }
    };
} // namespace std

namespace VulkanRTX {

enum class BufferType { GEOMETRY, UNIFORM };

struct DimensionState; // Forward declare – defined elsewhere

struct CopyRegion {
    VkBuffer     srcBuffer;
    VkDeviceSize srcOffset = 0;
    VkDeviceSize dstOffset;
    VkDeviceSize size;
};

struct Mesh {
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

class VulkanBufferManager {
public:
    explicit VulkanBufferManager(std::shared_ptr<Vulkan::Context> ctx);
    VulkanBufferManager(std::shared_ptr<Vulkan::Context> ctx,
                        const glm::vec3* vertices, size_t vertexCount,
                        const uint32_t* indices, size_t indexCount,
                        uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max());
    ~VulkanBufferManager() noexcept;

    void uploadMesh(const glm::vec3* vertices, size_t vertexCount,
                    const uint32_t* indices, size_t indexCount,
                    uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max());

    void setDevice(VkDevice device, VkPhysicalDevice physicalDevice);
    void reserveArena(VkDeviceSize size, BufferType type);

    std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>> getGeometries() const;
    std::vector<DimensionState> getDimensionStates() const;

    uint32_t getVertexCount() const { return vertexCount_; }
    uint32_t getIndexCount() const { return indexCount_; }

    uint32_t getTotalVertexCount() const;
    uint32_t getTotalIndexCount() const;

    const std::vector<Mesh>& getMeshes() const { return meshes_; }

    void generateSphere(float radius, uint32_t latDivs = 32, uint32_t lonDivs = 32);
    void generateCube(float size = 1.0f);  // NEW: Fallback geometry

    // GROK PROTIP: Load OBJ → dedup → upload → return geometry data for AS build
    std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>
    loadOBJ(const std::string& path,
            VkCommandPool commandPool,
            VkQueue graphicsQueue,
            uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max());

    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                             VkDeviceSize size,
                             VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties,
                             VkBuffer& buffer, VkDeviceMemory& memory,
                             const VkMemoryAllocateFlagsInfo* allocFlags,
                             Vulkan::Context& context);

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
    VkBuffer getIndexBuffer() const { return indexBuffer_; }
    VkBuffer getScratchBuffer(uint32_t index = 0) const;
    VkDeviceAddress getScratchBufferAddress(uint32_t index = 0) const;
    uint32_t getScratchBufferCount() const;

    VkBuffer getArenaBuffer() const;
    VkDeviceSize getVertexOffset() const;
    VkDeviceSize getIndexOffset() const;
    VkDeviceAddress getDeviceAddress(VkBuffer buffer) const;

    uint32_t getTransferQueueFamily() const;

    void loadTexture(const char* path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    VkImage getTextureImage() const;
    VkImageView getTextureImageView() const;
    VkSampler getTextureSampler() const;

    // --- DeviceAddress getters ---
    VkDeviceAddress getVertexBufferAddress() const;
    VkDeviceAddress getIndexBufferAddress() const;

    // --- Uniform buffer helpers (NOW DECLARED) ---
    void createUniformBuffers(uint32_t count);
    VkBuffer getUniformBuffer(uint32_t index) const;
    VkDeviceMemory getUniformBufferMemory(uint32_t index) const;

private:
    struct Impl;
    Impl* impl_ = nullptr;

    // Core members
    std::shared_ptr<Vulkan::Context> context_;
    uint32_t vertexCount_ = 0;
    uint32_t indexCount_ = 0;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceAddress vertexBufferAddress_ = 0;
    VkDeviceAddress indexBufferAddress_ = 0;

    std::vector<Mesh> meshes_;

    // Texture
    VkImage textureImage_ = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory_ = VK_NULL_HANDLE;
    VkImageView textureImageView_ = VK_NULL_HANDLE;
    VkSampler textureSampler_ = VK_NULL_HANDLE;

    // Private helpers
    void persistentCopy(const void* data, VkDeviceSize size, VkDeviceSize offset);
    void initializeStagingPool();
    void createStagingBuffer(VkDeviceSize size, VkBuffer& buf, VkDeviceMemory& mem);
    void mapCopyUnmap(VkDeviceMemory mem, VkDeviceSize size, const void* data);
    void batchCopyToArena(std::span<const CopyRegion> regions);
    void copyToArena(VkBuffer src, VkDeviceSize dstOffset, VkDeviceSize size);
    void initializeCommandPool();
    void createTextureImage(const unsigned char* pixels, int w, int h, int channels, VkFormat format);
    void createTextureImageView(VkFormat format);
    void createTextureSampler();

    // GROK PROTIP: Upload helper used by loadOBJ and uploadMesh
    void uploadToDeviceLocal(const void* data, VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VkBuffer& buffer, VkDeviceMemory& memory);
};

} // namespace VulkanRTX

// ---------------------------------------------------------------------
// Inline implementation of ManagedBuffer (header for simplicity)
// ---------------------------------------------------------------------
namespace Vulkan {

inline ManagedBuffer::ManagedBuffer(VkDevice dev, VkDeviceSize sz,
                                   VkBufferUsageFlags usage,
                                   VkMemoryPropertyFlags props,
                                   const VkMemoryAllocateFlagsInfo* allocFlags)
    : device_(dev)
{
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