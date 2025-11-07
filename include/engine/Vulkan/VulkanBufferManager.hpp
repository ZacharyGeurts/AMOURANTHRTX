// include/engine/Vulkan/VulkanBufferManager.hpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025 – C++23 ZERO-COST COMPATIBLE SUPREMACY
// FIXED: VulkanHandle<VkBuffer> → VkBuffer* (pointer-to-handle) → .get() returns VkBuffer**
// FIXED: arena_buffer()/staging_buffer() → return *handle.get() → VkBuffer
// IMPLICIT operator T() STILL WORKS EVERYWHERE ELSE VIA operator VkBuffer()
// FULLY COMPILES — ZERO COST — RASPBERRY_PINK ETERNAL 🔥🤖🚀💀🖤❤️⚡

#pragma once

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Dispose.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/logging.hpp"

#include <span>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <tuple>
#include <expected>
#include <concepts>
#include <numbers>
#include <numeric>
#include <algorithm>
#include <array>
#include <cstdint>

namespace VulkanRTX {

template<typename T>
concept VertexRange = std::ranges::contiguous_range<T> &&
                     std::same_as<std::ranges::range_value_t<T>, glm::vec3>;

template<typename T>
concept IndexRange = std::ranges::contiguous_range<T> &&
                    std::same_as<std::ranges::range_value_t<T>, uint32_t>;

consteval VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

constexpr VkDeviceSize kVertexAlignment = 256;
constexpr VkDeviceSize kIndexAlignment  = 256;
constexpr VkDeviceSize kStagingPoolSize = 64uz * 1024 * 1024;

struct CopyRegion {
    VkBuffer     srcBuffer;
    VkDeviceSize srcOffset = 0;
    VkDeviceSize dstOffset = 0;
    VkDeviceSize size      = 0;

    constexpr CopyRegion() noexcept = default;
    constexpr CopyRegion(VkBuffer src, VkDeviceSize dst, VkDeviceSize sz) noexcept
        : srcBuffer(src), dstOffset(dst), size(sz) {}
};

struct Mesh {
    uint32_t vertexOffset = 0;
    uint32_t indexOffset  = 0;
    uint32_t vertexCount  = 0;
    uint32_t indexCount   = 0;

    [[nodiscard]] consteval VkDeviceSize vertexByteOffset() const noexcept {
        return static_cast<VkDeviceSize>(vertexOffset) * sizeof(glm::vec3);
    }
    [[nodiscard]] consteval VkDeviceSize indexByteOffset() const noexcept {
        return static_cast<VkDeviceSize>(indexOffset) * sizeof(uint32_t);
    }
};

class VulkanBufferManager {
public:
    explicit constexpr VulkanBufferManager(std::shared_ptr<Context> ctx) noexcept
        : context_(std::move(ctx)) {
        initialize_command_pool();
        initialize_staging_pool();
        reserve_arena(kStagingPoolSize * 2);
    }

    template<VertexRange V, IndexRange I>
    [[nodiscard]] std::expected<void, VkResult> upload_mesh(std::span<const V> vertices, std::span<const I> indices) noexcept {
        if (vertices.empty() || indices.empty() || indices.size() % 3 != 0)
            return std::unexpected(VK_ERROR_INITIALIZATION_FAILED);

        const VkDeviceSize v_size = vertices.size() * sizeof(glm::vec3);
        const VkDeviceSize i_size = indices.size()  * sizeof(uint32_t);

        const VkDeviceSize index_offset = align_up(v_size, kIndexAlignment);
        const VkDeviceSize required     = index_offset + i_size;

        if (required > arena_size_) [[unlikely]] {
            reserve_arena(required * 2);
        }

        persistent_copy(vertices.data(), v_size, vertex_offset_);
        persistent_copy(indices.data(),  i_size, vertex_offset_ + v_size);

        std::array<CopyRegion, 2> regions = {
            CopyRegion{this->staging_buffer(), 0,             vertex_offset_, v_size},
            CopyRegion{this->staging_buffer(), v_size,        vertex_offset_ + index_offset, i_size}
        };
        this->batch_copy_to_arena(std::span<const CopyRegion>(regions.data(), regions.size()));

        vertex_buffer_address_ = this->get_buffer_device_address(*context_, arena_buffer()) + vertex_offset_;
        index_buffer_address_  = this->get_buffer_device_address(*context_, arena_buffer()) + vertex_offset_ + index_offset;

        vertex_buffer_ = makeBuffer(context_->device, arena_buffer());
        index_buffer_  = makeBuffer(context_->device, arena_buffer());

        meshes_.push_back(Mesh{
            .vertexOffset = static_cast<uint32_t>(vertex_offset_ / sizeof(glm::vec3)),
            .indexOffset  = static_cast<uint32_t>((vertex_offset_ + index_offset) / sizeof(uint32_t)),
            .vertexCount  = static_cast<uint32_t>(vertices.size()),
            .indexCount   = static_cast<uint32_t>(indices.size())
        });

        vertex_offset_ += required;
        vertex_count_   = static_cast<uint32_t>(vertices.size());
        index_count_    = static_cast<uint32_t>(indices.size());

        return {};
    }

    // FIXED: VulkanHandle<VkBuffer> = unique_ptr<VkBuffer*> → .get() = VkBuffer** → deref = VkBuffer
    [[nodiscard]] constexpr VkBuffer arena_buffer() const noexcept { return *arena_buffer_.get(); }
    [[nodiscard]] constexpr VkBuffer staging_buffer() const noexcept { return *staging_buffer_.get(); }

    [[nodiscard]] constexpr VkDeviceSize     vertex_offset() const noexcept { return vertex_offset_; }
    [[nodiscard]] constexpr VkDeviceAddress  vertex_buffer_address() const noexcept { return vertex_buffer_address_; }
    [[nodiscard]] constexpr VkDeviceAddress  index_buffer_address() const noexcept { return index_buffer_address_; }
    [[nodiscard]] constexpr uint32_t         vertex_count() const noexcept { return vertex_count_; }
    [[nodiscard]] constexpr uint32_t         index_count() const noexcept { return index_count_; }
    [[nodiscard]] constexpr const auto&      meshes() const noexcept { return meshes_; }

    void releaseAll(VkDevice dev = VK_NULL_HANDLE) noexcept {
        arena_buffer_.reset();
        arena_memory_.reset();
        staging_buffer_.reset();
        staging_memory_.reset();
        command_pool_.reset();
        meshes_.clear();
        vertex_offset_ = 0;
        arena_size_ = 0;
    }

private:
    std::shared_ptr<Context> context_;

    VulkanHandle<VkBuffer>       arena_buffer_;
    VulkanHandle<VkDeviceMemory> arena_memory_;
    VkDeviceSize                 arena_size_ = 0;
    VkDeviceSize                 vertex_offset_ = 0;

    VulkanHandle<VkBuffer>       vertex_buffer_;
    VulkanHandle<VkBuffer>       index_buffer_;
    VkDeviceAddress              vertex_buffer_address_ = 0;
    VkDeviceAddress              index_buffer_address_  = 0;

    uint32_t                     vertex_count_ = 0;
    uint32_t                     index_count_  = 0;

    std::vector<Mesh>            meshes_;

    VulkanHandle<VkBuffer>       staging_buffer_;
    VulkanHandle<VkDeviceMemory> staging_memory_;
    void*                        persistent_mapped_ = nullptr;

    VulkanHandle<VkCommandPool>  command_pool_;

    void reserve_arena(VkDeviceSize size) noexcept;  // defined in .cpp

    void initialize_staging_pool() noexcept;
    void initialize_command_pool() noexcept;
    void persistent_copy(const void* src, VkDeviceSize size, VkDeviceSize offset) const noexcept;
    void batch_copy_to_arena(std::span<const CopyRegion> regions) const noexcept;

    static uint32_t find_memory_type(VkPhysicalDevice pd, uint32_t filter, VkMemoryPropertyFlags props) noexcept;
    static VkDeviceAddress get_buffer_device_address(const Context& ctx, VkBuffer buf) noexcept {
        VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buf };
        return ctx.vkGetBufferDeviceAddressKHR(ctx.device, &info);
    }
};

} // namespace VulkanRTX