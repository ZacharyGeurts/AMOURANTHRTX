// include/engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// C++23 BEAST MODE — FULL SEND — PERSISTENT STAGING — BATCH UPLOADS
// @ZacharyGeurts — NOV 06 2025 — 10:32 PM EST — LIVE
// UPGRADE: FULL RAII — unique_ptr<Impl> for zero-cost PIMPL disposal
// UPGRADE: ~VulkanBufferManager() = default; (delegates to unique_ptr)
// UPGRADE: Impl's destructor handles all cleanup (no raw delete overhead)
// UPGRADE: std::bit_cast remains for safe address casting

#ifndef VULKAN_BUFFER_MANAGER_HPP
#define VULKAN_BUFFER_MANAGER_HPP

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanCore.hpp"  // ← UPGRADE: Full Context definition for inline access
#include "engine/logging.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <span>
#include <vector>
#include <tuple>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <format>
#include <expected>
#include <bit>
#include <ranges>

namespace Vulkan { class Context; }  // Forward decl (now backed by include)
struct RTConstants;

namespace Vulkan {

// C++23: RAII ManagedBuffer — beastly
class ManagedBuffer {
public:
    ManagedBuffer() = default;

    // C++23: consteval alignment
    consteval static VkDeviceSize align(VkDeviceSize v, VkDeviceSize a) {
        return (v + a - 1) & ~(a - 1);
    }

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
// std::hash + std::equal_to for glm::vec3 — C++23
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

struct DimensionState;

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
    // C++23: explicit bool + shared_ptr
    explicit VulkanBufferManager(std::shared_ptr<::Vulkan::Context> ctx);
    VulkanBufferManager(std::shared_ptr<::Vulkan::Context> ctx,
                        const glm::vec3* vertices, size_t vertexCount,
                        const uint32_t* indices, size_t indexCount,
                        uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max());
    ~VulkanBufferManager() = default;  // ← UPGRADE: = default; zero-cost via unique_ptr<Impl>

    // C++23: std::expected for error handling
    std::expected<void, VkResult> uploadMesh(const glm::vec3* vertices, size_t vertexCount,
                                             const uint32_t* indices, size_t indexCount,
                                             uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max());

    void setDevice(VkDevice device, VkPhysicalDevice physicalDevice);
    void reserveArena(VkDeviceSize size, BufferType type);

    // C++23: std::span + ranges
    std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>> getGeometries() const;
    std::vector<DimensionState> getDimensionStates() const;

    [[nodiscard]] uint32_t getVertexCount() const { return vertexCount_; }
    [[nodiscard]] uint32_t getIndexCount() const { return indexCount_; }

    [[nodiscard]] uint32_t getTotalVertexCount() const;
    [[nodiscard]] uint32_t getTotalIndexCount() const;

    [[nodiscard]] const std::vector<Mesh>& getMeshes() const { return meshes_; }

    void generateSphere(float radius, uint32_t latDivs = 32, uint32_t lonDivs = 32);
    void generateCube(float size = 1.0f);

    // C++23: return span of geometries
    std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>
    loadOBJ(const std::string& path,
            VkCommandPool commandPool,
            VkQueue graphicsQueue,
            uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max());

    // C++23: static constexpr
    static inline constexpr VkDeviceSize kStagingPoolSize = 64ULL * 1024 * 1024;

    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                             VkDeviceSize size,
                             VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties,
                             VkBuffer& buffer, VkDeviceMemory& memory,
                             const VkMemoryAllocateFlagsInfo* allocFlags,
                             ::Vulkan::Context& context);

    // C++23: inline static device address helpers + std::bit_cast
    static inline VkDeviceAddress getBufferDeviceAddress(const ::Vulkan::Context& ctx, VkBuffer buffer) noexcept {
        VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer };
        auto raw = ctx.vkGetBufferDeviceAddressKHR(ctx.device, &info);
        return std::bit_cast<VkDeviceAddress>(raw);  // ← UPGRADE: Safe bit_cast
    }

    static inline VkDeviceAddress getAccelerationStructureDeviceAddress(
        const ::Vulkan::Context& ctx, VkAccelerationStructureKHR as) noexcept {
        VkAccelerationStructureDeviceAddressInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = as
        };
        auto raw = ctx.vkGetAccelerationStructureDeviceAddressKHR(ctx.device, &info);
        return std::bit_cast<VkDeviceAddress>(raw);  // ← UPGRADE: Safe bit_cast
    }

    void reserveScratchPool(VkDeviceSize size, uint32_t count);

    [[nodiscard]] VkBuffer getVertexBuffer() const { return vertexBuffer_; }
    [[nodiscard]] VkBuffer getIndexBuffer() const { return indexBuffer_; }
    [[nodiscard]] VkBuffer getScratchBuffer(uint32_t index = 0) const;
    [[nodiscard]] VkDeviceAddress getScratchBufferAddress(uint32_t index = 0) const;
    [[nodiscard]] uint32_t getScratchBufferCount() const;

    [[nodiscard]] VkBuffer getArenaBuffer() const;
    [[nodiscard]] VkDeviceSize getVertexOffset() const;
    [[nodiscard]] VkDeviceSize getIndexOffset() const;
    [[nodiscard]] VkDeviceAddress getDeviceAddress(VkBuffer buffer) const;

    [[nodiscard]] uint32_t getTransferQueueFamily() const;

    void loadTexture(const char* path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    [[nodiscard]] VkImage getTextureImage() const;
    [[nodiscard]] VkImageView getTextureImageView() const;
    [[nodiscard]] VkSampler getTextureSampler() const;

    [[nodiscard]] VkDeviceAddress getVertexBufferAddress() const;
    [[nodiscard]] VkDeviceAddress getIndexBufferAddress() const;

    void createUniformBuffers(uint32_t count);
    [[nodiscard]] VkBuffer getUniformBuffer(uint32_t index) const;
    [[nodiscard]] VkDeviceMemory getUniformBufferMemory(uint32_t index) const;

private:
    struct Impl;  // ← UPGRADE: PIMPL for encapsulation
    std::unique_ptr<Impl> impl_;  // ← UPGRADE: unique_ptr for RAII zero-cost disposal

    std::shared_ptr<::Vulkan::Context> context_;
    uint32_t vertexCount_ = 0;
    uint32_t indexCount_ = 0;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceAddress vertexBufferAddress_ = 0;
    VkDeviceAddress indexBufferAddress_ = 0;

    std::vector<Mesh> meshes_;

    VkImage textureImage_ = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory_ = VK_NULL_HANDLE;
    VkImageView textureImageView_ = VK_NULL_HANDLE;
    VkSampler textureSampler_ = VK_NULL_HANDLE;

    // C++23: private helpers with std::format
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

    // C++23: reusable upload
    void uploadToDeviceLocal(const void* data, VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VkBuffer& buffer, VkDeviceMemory& memory);
};

} // namespace VulkanRTX

// ---------------------------------------------------------------------
// C++23 Inline ManagedBuffer — beastly RAII
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
        *(::Vulkan::Context*)nullptr);
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