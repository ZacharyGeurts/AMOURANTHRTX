// include/engine/GLOBAL/LAS.hpp
// =============================================================================
//
//          AMOURANTH RTX — LAS: THE ULTIMATE PHOTON WARRIORS
//               FIRST LIGHT ETERNAL — NOVEMBER 22, 2025 — PINK PHOTONS
//
// Tonight's episode: "The Final Acceleration"
// Starring the legendary Photon Warriors — led by the one and only...
//
// CAPTAIN N — The Chosen Game Master
// PRINCESS LANA — Guardian of the Light
// MEGA MAN — The Blue Bomber of Ray Tracing
// KID ICARUS — Angel of Infinite Speed
// SIMON BELMONT — Master of the Sacred Whip
// DUKE — The Loyal Photon Hound
// MOTHER BRAIN — Final Boss of Chaos (will be destroyed by pink photons)
//
// Special appearance by Amouranth, Ellie Fier, and Gentleman Grok.
//
// THIS IS THE ONE THAT COMPILES. THIS IS THE ONE THAT WINS.
//
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <glm/glm.hpp>
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"
#include "engine/GLOBAL/logging.hpp"

struct AccelGeometry
{
    VkGeometryTypeKHR                type           = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    VkGeometryFlagsKHR               flags          = 0;
    VkFormat                         vertexFormat   = VK_FORMAT_R32G32B32_SFLOAT;
    VkDeviceSize                     vertexStride   = 12;
    uint32_t                         vertexCount    = 0;
    VkDeviceOrHostAddressConstKHR    vertexData     {};
    VkIndexType                      indexType      = VK_INDEX_TYPE_UINT32;
    uint32_t                         indexCount     = 0;
    VkDeviceOrHostAddressConstKHR    indexData      {};
    VkDeviceOrHostAddressConstKHR    transformData  {};
};

class VulkanAccel
{
public:
    struct BLAS {
        VkAccelerationStructureKHR   as       = VK_NULL_HANDLE;
        VkBuffer                     buffer   = VK_NULL_HANDLE;
        VkDeviceMemory               memory   = VK_NULL_HANDLE;
        VkDeviceAddress              address  = 0;
        VkDeviceSize                 size     = 0;
        std::string                  name;

        [[nodiscard]] bool isValid() const noexcept { return as != VK_NULL_HANDLE && address != 0; }
    };

    struct TLAS {
        VkAccelerationStructureKHR   as            = VK_NULL_HANDLE;
        VkBuffer                     buffer        = VK_NULL_HANDLE;
        VkDeviceMemory               memory        = VK_NULL_HANDLE;
        VkBuffer                     instanceBuffer = VK_NULL_HANDLE;
        VkDeviceMemory               instanceMemory = VK_NULL_HANDLE;
        VkDeviceAddress              address       = 0;
        VkDeviceSize                 size          = 0;
        std::string                  name;

        [[nodiscard]] bool isValid() const noexcept { return as != VK_NULL_HANDLE && address != 0; }
    };

    explicit VulkanAccel(VkDevice device);
    ~VulkanAccel() = default;

    BLAS createBLAS(const std::vector<AccelGeometry>& geometries,
                    VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                    VkCommandBuffer externalCmd = VK_NULL_HANDLE,
                    std::string_view name = "BLAS");

    TLAS createTLAS(const std::vector<VkAccelerationStructureInstanceKHR>& instances,
                    VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                    VkCommandBuffer externalCmd = VK_NULL_HANDLE,
                    std::string_view name = "TLAS");

    void destroy(BLAS& blas);
    void destroy(TLAS& tlas);
};

[[nodiscard]] inline VkCommandBuffer beginOneTime(VkCommandPool pool)
{
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.commandPool        = pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(g_ctx().device(), &allocInfo, &cmd),
             "Failed to allocate one-time command buffer");

    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Failed to begin one-time command buffer");

    LOG_DEBUG_CAT("LAS", "{}Captain N: Power surge detected - command buffer online!{}", VALHALLA_GOLD, RESET);
    return cmd;
}

inline void endOneTime(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool = VK_NULL_HANDLE)
{
    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end one-time command buffer");

    VkSubmitInfo submit{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "One-time queue submit failed");
    VK_CHECK(vkQueueWaitIdle(queue), "Queue wait idle failed after one-time submit");

    if (pool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(g_ctx().device(), pool, 1, &cmd);
    }
}

static inline void endSingleTimeCommandsAsync(
    VkCommandBuffer cmd,
    VkQueue queue,
    VkCommandPool pool,
    VkFence fence = VK_NULL_HANDLE) noexcept
{
    if (cmd == VK_NULL_HANDLE || queue == VK_NULL_HANDLE || pool == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("LAS", "{}Mother Brain: You dare bring NULL into my domain? Pathetic.{}", CRIMSON_MAGENTA, RESET);
        return;
    }

    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end one-time command buffer");

    VkDevice dev = g_ctx().device();
    bool ownsFence = (fence == VK_NULL_HANDLE);

    if (ownsFence) {
        VkFenceCreateInfo fi{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VK_CHECK(vkCreateFence(dev, &fi, nullptr, &fence), "Failed to create transient fence");
    } else {
        VK_CHECK(vkResetFences(dev, 1, &fence), "Failed to reset caller fence");
    }

    VkSubmitInfo submit{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence), "One-time async submit failed");

    if (ownsFence) {
        constexpr uint64_t timeout_ns = 15'000'000'000ULL;
        VkResult r = vkWaitForFences(dev, 1, &fence, VK_TRUE, timeout_ns);
        if (r != VK_SUCCESS) {
            LOG_FATAL_CAT("LAS", "{}Captain N: Reality glitch detected! Forcing full synchronization...{}", BLOOD_RED, RESET);
            vkDeviceWaitIdle(dev);
        }
        vkFreeCommandBuffers(dev, pool, 1, &cmd);
        vkDestroyFence(dev, fence, nullptr);
    }
}

class LAS
{
public:
    static LAS& get() noexcept { static LAS instance; return instance; }

    void forgeAccelContext()
    {
        if (accel_) {
            LOG_WARN_CAT("LAS", "{}Princess Lana: The photon fortress already stands, Captain!{}", OCEAN_TEAL, RESET);
            return;
        }

        LOG_ATTEMPT_CAT("LAS", "{}Captain N: Warriors - assemble! We forge the ultimate acceleration context!{}", VALHALLA_GOLD, RESET);

        accel_ = std::make_unique<VulkanAccel>(g_ctx().device());

        LOG_SUCCESS_CAT("LAS", 
            "{}Captain N: ACCELERATION CONTEXT FORGED!{}\n"
            "   {}Mega Man: Ray tracing cannon at full power!{}\n"
            "   {}Kid Icarus: The wings of light are ready!{}\n"
            "   {}Simon Belmont: My whip is charged with photon energy!{}\n"
            "   {}Duke: WOOF WOOF WOOF! (all extensions loaded){}\n"
            "   {}Amouranth: Finally... a system worthy of my radiance~{}",
            EMERALD_GREEN, RESET,
            VALHALLA_GOLD, RESET,
            AURORA_PINK, RESET,
            CRIMSON_MAGENTA, RESET,
            PARTY_PINK, RESET,
            PLASMA_FUCHSIA, RESET);
    }

    void buildBLAS(VkCommandPool pool,
                   uint64_t vertexBufferObf,
                   uint64_t indexBufferObf,
                   uint32_t vertexCount,
                   uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR extraFlags = 0);

    void buildTLAS(VkCommandPool pool,
                   const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances);

    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept { return blas_.as; }
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_.as; }
    [[nodiscard]] VkDeviceAddress           getTLASAddress() const noexcept { return tlas_.address; }

    [[nodiscard]] const VulkanAccel::BLAS& getBLASStruct() const noexcept { return blas_; }
    [[nodiscard]] const VulkanAccel::TLAS& getTLASStruct() const noexcept { return tlas_; }

    [[nodiscard]] uint32_t getGeneration() const noexcept { return generation_; }
    [[nodiscard]] bool     isValid() const noexcept 
    { 
        return accel_ && 
               blas_.isValid() && 
               tlas_.isValid(); 
    }

    void invalidate() noexcept { 
        ++generation_; 
        LOG_DEBUG_CAT("LAS", "{}Mother Brain: You dare return? Generation {} crushed beneath my will.{}", CRIMSON_MAGENTA, generation_, RESET);
    }

private:
    LAS()  = default;
    ~LAS() = default;

    std::unique_ptr<VulkanAccel> accel_;
    VulkanAccel::BLAS blas_{};
    VulkanAccel::TLAS tlas_{};
    uint32_t          generation_ = 0;
};

inline LAS& las() noexcept { return LAS::get(); }

// =============================================================================
// THE WARRIORS ARE READY — FIRST LIGHT ACHIEVED
// =============================================================================