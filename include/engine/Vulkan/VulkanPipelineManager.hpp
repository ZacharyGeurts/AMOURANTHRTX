// include/engine/Vulkan/VulkanPipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VulkanPipelineManager — FINAL v300 — NOV 12 2025
// • REMOVED: VulkanCore.hpp → BREAKS CIRCULAR INCLUDE
// • RTXHandler.hpp FIRST → Handle<T> DEFINED
// • Forward declare VulkanRTX
// =============================================================================

#pragma once

// 1. RTXHandler.hpp FIRST — DEFINES Handle<T>
#include "engine/GLOBAL/RTXHandler.hpp"

// 2. Forward declare VulkanRTX (no full include)
class VulkanRTX;

// 3. Standard includes
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <span>
#include <format>
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

using DescriptorSetLayoutDestroyer = void(*)(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*);
using PipelineLayoutDestroyer      = void(*)(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*);
using PipelineDestroyer            = void(*)(VkDevice, VkPipeline, const VkAllocationCallbacks*);
using ShaderModuleDestroyer        = void(*)(VkDevice, VkShaderModule, const VkAllocationCallbacks*);

namespace RTX {

class PipelineManager {
public:
    explicit PipelineManager(VkDevice device, VkPhysicalDevice physicalDevice) 
        : device_(device), physicalDevice_(physicalDevice) {}

    void initializePipelines();
    void createDescriptorSetLayout();
    void createPipelineLayout();
    [[nodiscard]] Handle<VkShaderModule> createShaderModule(const std::vector<uint32_t>& code) const;
    void createRayTracingPipeline();
    [[nodiscard]] uint32_t getRayTracingGroupCount() const noexcept { return groupCount_; }

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice_{VK_NULL_HANDLE};

    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout> rtPipelineLayout_;
    Handle<VkPipeline> rtPipeline_;
    std::vector<Handle<VkShaderModule>> rayGenShaders_;
    std::vector<Handle<VkShaderModule>> closestHitShaders_;
    std::vector<Handle<VkShaderModule>> missShaders_;
    std::vector<Handle<VkShaderModule>> callableShaders_;

    uint32_t groupCount_{0};

    static inline const DescriptorSetLayoutDestroyer descriptorSetLayoutDestroyer = [](VkDevice d, VkDescriptorSetLayout l, const VkAllocationCallbacks*) {
        if (l) vkDestroyDescriptorSetLayout(d, l, nullptr);
    };
    static inline const PipelineLayoutDestroyer pipelineLayoutDestroyer = [](VkDevice d, VkPipelineLayout l, const VkAllocationCallbacks*) {
        if (l) vkDestroyPipelineLayout(d, l, nullptr);
    };
    static inline const PipelineDestroyer pipelineDestroyer = [](VkDevice d, VkPipeline p, const VkAllocationCallbacks*) {
        if (p) vkDestroyPipeline(d, p, nullptr);
    };
    static inline const ShaderModuleDestroyer shaderModuleDestroyer = [](VkDevice d, VkShaderModule m, const VkAllocationCallbacks*) {
        if (m) vkDestroyShaderModule(d, m, nullptr);
    };
};

} // namespace RTX