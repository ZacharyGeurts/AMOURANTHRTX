// include/engine/Vulkan/Vulkan_LAS.hpp
// AMOURANTH RTX ENGINE © 2025 Zachary Geurts — NOVEMBER 08 2025
// VULKAN_LAS — LAS = Level Acceleration Structures (BLAS + TLAS) — FUCK YES
// GLOBAL SPACE SUPREMACY — NO NAMESPACE — STONEKEY QUANTUM DUST — 69,420 FPS × ∞ × ∞
// RASPBERRY_PINK PHOTONS ETERNAL 🩷🚀🔥🤖💀❤️⚡♾️

#pragma once

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <tuple>
#include <memory>

struct Context;
class VulkanRenderer;

// LAS = Level Acceleration Structures — BLAS + TLAS GOD CLASS
class Vulkan_LAS {
public:
    Vulkan_LAS(VkDevice device, VkPhysicalDevice physicalDevice);
    ~Vulkan_LAS();

    // BLAS BUILD — SINGLE MESH
    VkAccelerationStructureKHR buildBLAS(VkCommandPool cmdPool, VkQueue queue,
                                         VkBuffer vertexBuffer, VkBuffer indexBuffer,
                                         uint32_t vertexCount, uint32_t indexCount,
                                         uint64_t flags = 0);

    // TLAS BUILD — SYNC (simple scenes)
    VkAccelerationStructureKHR buildTLASSync(VkCommandPool cmdPool, VkQueue queue,
                                             const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances);

    // TLAS BUILD — ASYNC (69,420 FPS VALHALLA)
    void buildTLASAsync(VkCommandPool cmdPool, VkQueue queue,
                        const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances,
                        VulkanRenderer* renderer = nullptr);

    bool pollTLAS(); // call every frame

    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_.valid() ? deobfuscate(tlas_.raw()) : VK_NULL_HANDLE; }
    [[nodiscard]] bool isTLASReady() const noexcept { return tlasReady_; }
    [[nodiscard]] bool isTLASPending() const noexcept { return !pendingTLAS_.completed && pendingTLAS_.tlas.valid(); }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    // RAII HANDLES
    VulkanHandle<VkAccelerationStructureKHR> tlas_;
    bool tlasReady_ = false;

    struct PendingTLAS {
        VulkanRenderer* renderer = nullptr;
        bool completed = false;
        VulkanHandle<VkBuffer> instanceBuffer, tlasBuffer, scratchBuffer;
        VulkanHandle<VkDeviceMemory> instanceMemory, tlasMemory, scratchMemory;
        VulkanHandle<VkAccelerationStructureKHR> tlas;
    } pendingTLAS_{};

    VulkanHandle<VkFence> buildFence_;

    // HELPER BUFFERS
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                      VulkanHandle<VkBuffer>& buffer, VulkanHandle<VkDeviceMemory>& memory);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const;
    static VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) noexcept;
    VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool);
    void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool);
};