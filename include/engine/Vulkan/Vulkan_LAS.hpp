// include/engine/Vulkan/Vulkan_LAS.hpp
// AMOURANTH RTX — VALHALLA LOCAL SUPREMACY — NOVEMBER 08 2025
// PendingTLAS = PRIVATE TO LAS — BUT PUBLIC FOR VulkanRTX — NO COMMON

#pragma once

#include "../GLOBAL/StoneKey.hpp"  // STONEKEY LOCAL
#include "VulkanCommon.hpp"        // ONLY FOR VulkanHandle + factories
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <tuple>

struct Context;
class VulkanRenderer;

// ──────────────────────────────────────────────────────────────────────────────
// STONEKEY OBFUSCATION — LOCAL TO LAS — ZERO COST
// ──────────────────────────────────────────────────────────────────────────────
constexpr uint64_t kHandleObfuscator = 0xDEADC0DE1337BEEFULL ^ kStone1 ^ kStone2;
inline constexpr auto obfuscate(auto h) noexcept { return decltype(h)(uint64_t(h) ^ kHandleObfuscator); }
inline constexpr auto deobfuscate(auto h) noexcept { return decltype(h)(uint64_t(h) ^ kHandleObfuscator); }

// ──────────────────────────────────────────────────────────────────────────────
// Vulkan_LAS — GOD CLASS
// ──────────────────────────────────────────────────────────────────────────────
class Vulkan_LAS {
public:
    Vulkan_LAS(VkDevice device, VkPhysicalDevice physicalDevice);
    ~Vulkan_LAS();

    VkAccelerationStructureKHR buildBLAS(VkCommandPool cmdPool, VkQueue queue,
                                         VkBuffer vertexBuffer, VkBuffer indexBuffer,
                                         uint32_t vertexCount, uint32_t indexCount,
                                         uint64_t flags = 0);

    VkAccelerationStructureKHR buildTLASSync(VkCommandPool cmdPool, VkQueue queue,
                                             const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances);

    void buildTLASAsync(VkCommandPool cmdPool, VkQueue queue,
                        const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances,
                        VulkanRenderer* renderer = nullptr);

    bool pollTLAS();

    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept {
        return tlas_.valid() ? deobfuscate(tlas_.raw()) : VK_NULL_HANDLE;
    }
    [[nodiscard]] bool isTLASReady() const noexcept { return tlasReady_; }
    [[nodiscard]] bool isTLASPending() const noexcept { return !pendingTLAS_.completed && pendingTLAS_.tlas.valid(); }

    // ──────────────────────────────────────────────────────────────────────────
    // PendingTLAS — **PUBLIC** SO VulkanRTX CAN USE IT — NO FORWARD DECLARE NEEDED
    // ──────────────────────────────────────────────────────────────────────────
    struct PendingTLAS {
        VulkanRenderer* renderer = nullptr;
        bool completed = false;
        VulkanHandle<VkBuffer> instanceBuffer, tlasBuffer, scratchBuffer;
        VulkanHandle<VkDeviceMemory> instanceMemory, tlasMemory, scratchMemory;
        VulkanHandle<VkAccelerationStructureKHR> tlas;
    };

    PendingTLAS pendingTLAS_{};  // NOW PUBLIC — VulkanRTX SEES FULL DEFINITION

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    VulkanHandle<VkAccelerationStructureKHR> tlas_;
    bool tlasReady_ = false;

    VulkanHandle<VkFence> buildFence_;

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                      VulkanHandle<VkBuffer>& buffer, VulkanHandle<VkDeviceMemory>& memory);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const;
    static VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) noexcept;
    VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool);
    void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool);
};