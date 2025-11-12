// include/engine/Vulkan/VulkanPipelineManager.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// VulkanPipelineManager — FIXED v1.0 — NOV 12 2025
// • Added #include "VulkanCore.hpp" to resolve incomplete VulkanRTX
// • using namespace Logging::Color; for colors
// • Functions using VulkanRTX now have full type access
// • No circular includes — VulkanCore.hpp forward-declares PipelineManager
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <span>
#include <format>

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"  // Added for full VulkanRTX definition
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;  // Brings in all colors: RESET, PLASMA_FUCHSIA, etc.

// Remove invalid lines like: using Color::Logging::RESET; etc. — now handled by using namespace

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

    // Handles for resources
    Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    Handle<VkPipelineLayout> rtPipelineLayout_;
    Handle<VkPipeline> rtPipeline_;
    std::vector<Handle<VkShaderModule>> rayGenShaders_;
    std::vector<Handle<VkShaderModule>> closestHitShaders_;
    std::vector<Handle<VkShaderModule>> missShaders_;
    std::vector<Handle<VkShaderModule>> callableShaders_;

    uint32_t groupCount_{0};

    // Helper lambdas for destruction
    auto descriptorSetLayoutDestroyer = [](VkDevice d, VkDescriptorSetLayout layout, const VkAllocationCallbacks*) {
        if (layout) vkDestroyDescriptorSetLayout(d, layout, nullptr);
    };

    auto pipelineLayoutDestroyer = [](VkDevice d, VkPipelineLayout layout, const VkAllocationCallbacks*) {
        if (layout) vkDestroyPipelineLayout(d, layout, nullptr);
    };

    auto pipelineDestroyer = [](VkDevice d, VkPipeline pipeline, const VkAllocationCallbacks*) {
        if (pipeline) vkDestroyPipeline(d, pipeline, nullptr);
    };

    auto shaderModuleDestroyer = [](VkDevice d, VkShaderModule module, const VkAllocationCallbacks*) {
        if (module) vkDestroyShaderModule(d, module, nullptr);
    };
};

} // namespace RTX