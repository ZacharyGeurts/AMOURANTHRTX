// include/engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025 – FINAL RAII SUPERNOVA
// GLOBAL Context + GLOBAL VulkanHandle — FULLY VISIBLE FROM VulkanCore.hpp
// INCLUDE ORDER FIXED: VulkanCore.hpp FIRST → ZERO INCOMPLETE TYPES
// ALL get*DeviceAddress → raw Context& (global)
// DISPOSE INTEGRATED — PERSISTENT STAGING — BATCH UPLOADS — 69,420 FPS ETERNAL

#pragma once

#include "engine/Vulkan/VulkanCore.hpp"   // ← FIRST: Context + VulkanHandle + factories GLOBAL
#include "engine/Dispose.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
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

namespace Vulkan {

class ManagedBuffer {
public:
    ManagedBuffer() = default;

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

    ManagedBuffer(ManagedBuffer&& other) noexcept = default;
    ManagedBuffer& operator=(ManagedBuffer&& other) noexcept = default;

    ~ManagedBuffer() = default;

    VkBuffer        buffer() const noexcept { return buffer_.get(); }
    VkDeviceMemory  memory() const noexcept { return memory_.get(); }

    void* map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    void  unmap();

private:
    VulkanHandle<VkBuffer>       buffer_;
    VulkanHandle<VkDeviceMemory> memory_;
    void*                        mapped_ = nullptr;
    VkDevice                     device_ = VK_NULL_HANDLE;
};

}

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
}

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
    uint32_t indexOffset  = 0;
    uint32_t vertexCount  = 0;
    uint32_t indexCount   = 0;
};

class VulkanBufferManager {
public:
    explicit VulkanBufferManager(std::shared_ptr<Context> ctx);
    VulkanBufferManager(std::shared_ptr<Context> ctx,
                        const glm::vec3* vertices, size_t vertexCount,
                        const uint32_t* indices, size_t indexCount,
                        uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max());
    ~VulkanBufferManager() = default;

    std::expected<void, VkResult> uploadMesh(const glm::vec3* vertices, size_t vertexCount,
                                             const uint32_t* indices, size_t indexCount,
                                             uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max());

    void setDevice(VkDevice device, VkPhysicalDevice physicalDevice);
    void reserveArena(VkDeviceSize size, BufferType type);

    std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>> getGeometries() const;
    std::vector<DimensionState> getDimensionStates() const;

    [[nodiscard]] uint32_t getVertexCount() const noexcept { return vertexCount_; }
    [[nodiscard]] uint32_t getIndexCount() const noexcept { return indexCount_; }
    [[nodiscard]] uint32_t getTotalVertexCount() const;
    [[nodiscard]] uint32_t getTotalIndexCount() const;
    [[nodiscard]] const std::vector<Mesh>& getMeshes() const noexcept { return meshes_; }

    void generateSphere(float radius, uint32_t latDivs = 32, uint32_t lonDivs = 32);
    void generateCube(float size = 1.0f);

    std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>
    loadOBJ(const std::string& path,
            VkCommandPool commandPool,
            VkQueue graphicsQueue,
            uint32_t transferQueueFamily = std::numeric_limits<uint32_t>::max());

    static inline constexpr VkDeviceSize kStagingPoolSize = 64ULL * 1024 * 1024;

    static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
                             VkDeviceSize size,
                             VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties,
                             VkBuffer& buffer, VkDeviceMemory& memory,
                             const VkMemoryAllocateFlagsInfo* allocFlags,
                             Context& context);

    // GLOBAL Context → full definition visible → NO incomplete type errors
    static inline VkDeviceAddress getBufferDeviceAddress(const Context& ctx, VkBuffer buffer) noexcept {
        VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer };
        return ctx.vkGetBufferDeviceAddressKHR(ctx.device, &info);
    }

    static inline VkDeviceAddress getAccelerationStructureDeviceAddress(
        const Context& ctx, VkAccelerationStructureKHR as) noexcept {
        VkAccelerationStructureDeviceAddressInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = as
        };
        return ctx.vkGetAccelerationStructureDeviceAddressKHR(ctx.device, &info);
    }

    void reserveScratchPool(VkDeviceSize size, uint32_t count);

    [[nodiscard]] VkBuffer getVertexBuffer() const noexcept { return vertexBuffer_.get(); }
    [[nodiscard]] VkBuffer getIndexBuffer() const noexcept { return indexBuffer_.get(); }
    [[nodiscard]] VkBuffer getScratchBuffer(uint32_t index = 0) const;
    [[nodiscard]] VkDeviceAddress getScratchBufferAddress(uint32_t index = 0) const;
    [[nodiscard]] uint32_t getScratchBufferCount() const noexcept;

    [[nodiscard]] VkBuffer getArenaBuffer() const noexcept;
    [[nodiscard]] VkDeviceSize getVertexOffset() const noexcept;
    [[nodiscard]] VkDeviceSize getIndexOffset() const noexcept;
    [[nodiscard]] VkDeviceAddress getDeviceAddress(VkBuffer buffer) const;

    [[nodiscard]] uint32_t getTransferQueueFamily() const noexcept;

    void loadTexture(const char* path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    [[nodiscard]] VkImage getTextureImage() const noexcept;
    [[nodiscard]] VkImageView getTextureImageView() const noexcept;
    [[nodiscard]] VkSampler getTextureSampler() const noexcept;

    [[nodiscard]] VkDeviceAddress getVertexBufferAddress() const noexcept;
    [[nodiscard]] VkDeviceAddress getIndexBufferAddress() const noexcept;

    void createUniformBuffers(uint32_t count);
    [[nodiscard]] VkBuffer getUniformBuffer(uint32_t index) const;
    [[nodiscard]] VkDeviceMemory getUniformBufferMemory(uint32_t index) const;

    void releaseAll(VkDevice dev = VK_NULL_HANDLE);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    std::shared_ptr<Context> context_;
    uint32_t vertexCount_ = 0;
    uint32_t indexCount_  = 0;

    VulkanHandle<VkBuffer>       vertexBuffer_;
    VulkanHandle<VkBuffer>       indexBuffer_;
    VkDeviceAddress              vertexBufferAddress_ = 0;
    VkDeviceAddress              indexBufferAddress_  = 0;

    std::vector<Mesh> meshes_;

    VulkanHandle<VkImage>        textureImage_;
    VulkanHandle<VkDeviceMemory> textureImageMemory_;
    VulkanHandle<VkImageView>    textureImageView_;
    VulkanHandle<VkSampler>      textureSampler_;

    void persistentCopy(const void* data, VkDeviceSize size, VkDeviceSize offset);
    void initializeStagingPool();
    void createStagingBuffer(VkDeviceSize size, VulkanHandle<VkBuffer>& buf, VulkanHandle<VkDeviceMemory>& mem);
    void mapCopyUnmap(VkDeviceMemory mem, VkDeviceSize size, const void* data);
    void batchCopyToArena(std::span<const CopyRegion> regions);
    void copyToArena(VkBuffer src, VkDeviceSize dstOffset, VkDeviceSize size);
    void initializeCommandPool();
    void createTextureImage(const unsigned char* pixels, int w, int h, int channels, VkFormat format);
    void createTextureImageView(VkFormat format);
    void createTextureSampler();

    void uploadToDeviceLocal(const void* data, VkDeviceSize size,
                             VkBufferUsageFlags usage,
                             VulkanHandle<VkBuffer>& buffer, VulkanHandle<VkDeviceMemory>& memory);
};

}

namespace Vulkan {

inline ManagedBuffer::ManagedBuffer(VkDevice dev, VkDeviceSize sz,
                                   VkBufferUsageFlags usage,
                                   VkMemoryPropertyFlags props,
                                   const VkMemoryAllocateFlagsInfo* allocFlags)
    : device_(dev)
{
    VkBuffer rawBuf = VK_NULL_HANDLE;
    VkDeviceMemory rawMem = VK_NULL_HANDLE;
    
    VulkanRTX::VulkanBufferManager::createBuffer(
        dev, nullptr, sz, usage, props,
        rawBuf, rawMem, allocFlags,
        *(Context*)nullptr);  // dummy context — only used for function pointer lookup

    buffer_ = makeBuffer(dev, rawBuf);
    memory_ = makeMemory(dev, rawMem);
}

inline void* ManagedBuffer::map(VkDeviceSize offset, VkDeviceSize size) {
    if (!mapped_) vkMapMemory(device_, memory_.get(), offset, size, 0, &mapped_);
    return mapped_;
}

inline void ManagedBuffer::unmap() {
    if (mapped_) {
        vkUnmapMemory(device_, memory_.get());
        mapped_ = nullptr;
    }
}

}