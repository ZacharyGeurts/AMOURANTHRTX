// include/engine/Vulkan/VulkanResourceManager.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts — VALHALLA RAII FORTRESS
// C++23 | STONEKEY ENCRYPTED HANDLES | HACKER APOCALYPSE GUARANTEED

#pragma once
#include "StoneKey.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <functional>
#include <span>

class VulkanCore;  // Forward decl

class VulkanResourceManager {
public:
    VulkanResourceManager() = default;
    VulkanResourceManager(VulkanResourceManager&&) noexcept;
    VulkanResourceManager& operator=(VulkanResourceManager&&) noexcept;
    ~VulkanResourceManager();

    void init(VulkanCore* core);
    void cleanup(VkDevice device = VK_NULL_HANDLE);

    // === ENCRYPTED HANDLE RETURNS ===
    uint64_t createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props);
    uint64_t createImage(VkImageCreateInfo* info, VkMemoryPropertyFlags props);
    uint64_t createImageView(VkImageViewCreateInfo* info);
    uint64_t createSampler(VkSamplerCreateInfo* info);
    uint64_t createAccelerationStructure(VkAccelerationStructureCreateInfoKHR* info);
    uint64_t createDescriptorPool(VkDescriptorPoolCreateInfo* info);
    uint64_t createCommandPool(VkCommandPoolCreateInfo* info);
    uint64_t createRenderPass(VkRenderPassCreateInfo* info);
    uint64_t createDescriptorSetLayout(VkDescriptorSetLayoutCreateInfo* info);
    uint64_t createPipelineLayout(VkDescriptorSetLayout* layouts, uint32_t count);
    uint64_t createGraphicsPipeline(VkGraphicsPipelineCreateInfo* info, const std::string& name = "");
    uint64_t createComputePipeline(VkComputePipelineCreateInfo* info, const std::string& name = "");
    uint64_t createShaderModule(std::span<const uint32_t> spirv);
    std::vector<uint64_t> allocateDescriptorSets(VkDescriptorSetAllocateInfo* info);
    uint64_t createFence(bool signaled = false);  // NEW

    // === HEADER-ONLY addFence() — C++23 INLINE GOD MODE ===
    inline void addFence(VkFence fence) noexcept {
        if (fence != VK_NULL_HANDLE) {
            fences_.push_back(encrypt(fence));
            LOG_DEBUG_CAT("ResourceMgr", "Tracked Fence: {:p} → enc 0x{:016x}", 
                          static_cast<void*>(fence), fences_.back());
        }
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

private:
    template<typename T>
    [[nodiscard]] static inline constexpr uint64_t encrypt(T raw) noexcept {
        return static_cast<uint64_t>(raw) ^ kStone1 ^ kStone2;
    }

    template<typename T>
    [[nodiscard]] static inline constexpr T decrypt(uint64_t enc) noexcept {
        return static_cast<T>(enc ^ kStone1 ^ kStone2);
    }

    // === ENCRYPTED CONTAINERS ===
    std::vector<uint64_t> buffers_;
    std::vector<uint64_t> memories_;
    std::vector<uint64_t> images_;
    std::vector<uint64_t> imageViews_;
    std::vector<uint64_t> samplers_;
    std::vector<uint64_t> accelerationStructures_;
    std::vector<uint64_t> descriptorPools_;
    std::vector<uint64_t> commandPools_;
    std::vector<uint64_t> renderPasses_;
    std::vector<uint64_t> descriptorSetLayouts_;
    std::vector<uint64_t> pipelineLayouts_;
    std::vector<uint64_t> pipelines_;
    std::vector<uint64_t> shaderModules_;
    std::vector<uint64_t> descriptorSets_;
    std::vector<uint64_t> fences_;  // FULLY TRACKED

    std::unordered_map<std::string, uint64_t> pipelineMap_;

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    class VulkanBufferManager* bufferManager_ = nullptr;
    VkDevice* contextDevicePtr_ = nullptr;

    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR_ = nullptr;
};