// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan ray tracing pipeline management implementation.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, Vulkan_init.hpp, VulkanRTX_Setup.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows, Consoles (PS5, Xbox Series X).
// Optimized for high-end GPUs with 8 GB VRAM (e.g., NVIDIA RTX 3070, AMD RX 6800).
// Zachary Geurts 2025
// Enhanced with competition-crushing optimizations: fixed SBT region sizes for multiple miss/hit groups,
// added robust shader loading with filesystem checks, improved error handling, and performance tweaks
// for better resource management and alignment. Ready to outperform industry standards.

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Dispose.hpp"
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <array>
#include <vector>
#include <glm/glm.hpp>
#include <unordered_map>

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

namespace VulkanRTX {

void VulkanPipelineManager::createRayTracingPipeline() {
    // Define shader types and load modules efficiently
    constexpr std::array<const char*, 10> shaderTypes = {
        "raygen", "miss", "closesthit", "shadowmiss", "anyhit", "intersection", "callable", "shadow_anyhit", "mid_anyhit", "volumetric_anyhit"
    };
    std::vector<VkShaderModule> shaderModules;
    shaderModules.reserve(shaderTypes.size());
    for (const auto* type : shaderTypes) {
        shaderModules.emplace_back(loadShader(context_.device, type));
    }

    // Predefine shader stages for clarity and performance
    constexpr uint32_t stageCount = 10;
    std::array<VkPipelineShaderStageCreateInfo, stageCount> shaderStages = {{
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = shaderModules[0],
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = shaderModules[1],
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = shaderModules[2],
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = shaderModules[3],
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[4],
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
            .module = shaderModules[5],
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
            .module = shaderModules[6],
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[7],
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[8],
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[9],
            .pName = "main"
        }
    }};

    // Predefine shader groups for efficiency
    constexpr uint32_t groupCount = 8;
    std::array<VkRayTracingShaderGroupCreateInfoKHR, groupCount> shaderGroups = {{
        // Raygen (group 0)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 0,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        // Primary miss (group 1)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 1,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        // Primary hit: triangles (group 2)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = 4,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        // Shadow miss (group 3)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 3,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        // Shadow hit: triangles (group 4)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = 7,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        // Mid-layer hit: triangles (group 5)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = 8,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        // Volumetric hit: procedural (group 6)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = 9,
            .intersectionShader = 5
        },
        // Callable (group 7)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 6,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        }
    }};

    // Define push constant range
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | 
                      VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | 
                      VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        .offset = 0,
        .size = sizeof(MaterialData::PushConstants)
    };

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &context_.rayTracingDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        for (auto module : shaderModules) {
            vkDestroyShaderModule(context_.device, module, nullptr);
        }
        throw std::runtime_error("Failed to create ray-tracing pipeline layout");
    }
    rayTracingPipelineLayout_ = std::make_unique<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>>(
        context_.device, pipelineLayout, vkDestroyPipelineLayout);
    context_.resourceManager.addPipelineLayout(pipelineLayout);

    // Query ray tracing properties
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 properties2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProperties
    };
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &properties2);

    uint32_t recursionDepth = (rtProperties.maxRayRecursionDepth > 0) ? std::min(4u, rtProperties.maxRayRecursionDepth) : 1;

    // Create pipeline
    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = stageCount,
        .pStages = shaderStages.data(),
        .groupCount = groupCount,
        .pGroups = shaderGroups.data(),
        .maxPipelineRayRecursionDepth = recursionDepth,
        .layout = pipelineLayout
    };

    auto vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCreateRayTracingPipelinesKHR"));
    if (!vkCreateRayTracingPipelinesKHR) {
        for (auto module : shaderModules) {
            vkDestroyShaderModule(context_.device, module, nullptr);
        }
        throw std::runtime_error("Failed to get vkCreateRayTracingPipelinesKHR function pointer");
    }

    VkPipeline pipeline;
    VkResult result = vkCreateRayTracingPipelinesKHR(context_.device, VK_NULL_HANDLE, pipelineCache_, 1, &pipelineInfo, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        for (auto module : shaderModules) {
            vkDestroyShaderModule(context_.device, module, nullptr);
        }
        throw std::runtime_error("Failed to create ray-tracing pipeline");
    }
    rayTracingPipeline_ = std::make_unique<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>>(
        context_.device, pipeline, vkDestroyPipeline);
    context_.resourceManager.addPipeline(pipeline);

    // Clean up shader modules
    for (auto module : shaderModules) {
        vkDestroyShaderModule(context_.device, module, nullptr);
    }
}

void VulkanPipelineManager::createShaderBindingTable() {
    if (!rayTracingPipeline_) {
        throw std::runtime_error("Ray-tracing pipeline not initialized");
    }

    // Query properties
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProperties
    };
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &properties);

    constexpr uint32_t groupCount = 8;
    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = rtProperties.shaderGroupHandleAlignment;
    const uint32_t alignedHandleSize = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);
    const uint32_t sbtSize = groupCount * alignedHandleSize;

    std::vector<uint8_t> shaderGroupHandles(sbtSize);
    auto vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetRayTracingShaderGroupHandlesKHR"));
    if (!vkGetRayTracingShaderGroupHandlesKHR) {
        throw std::runtime_error("Failed to get vkGetRayTracingShaderGroupHandlesKHR function pointer");
    }

    VkResult result = vkGetRayTracingShaderGroupHandlesKHR(context_.device, rayTracingPipeline_->get(), 0, groupCount, sbtSize, shaderGroupHandles.data());
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to get shader group handles");
    }

    // Create SBT buffer
    VkBufferUsageFlags sbtUsage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags sbtMemoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkBuffer sbtBuffer = VK_NULL_HANDLE;
    VkDeviceMemory sbtMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, sbtSize, sbtUsage, sbtMemoryProps, sbtBuffer, sbtMemory, nullptr, context_.resourceManager);

    // Create staging buffer
    VkBufferCreateInfo stagingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sbtSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VkBuffer stagingBuffer;
    if (vkCreateBuffer(context_.device, &stagingCreateInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to create staging buffer for SBT");
    }
    context_.resourceManager.addBuffer(stagingBuffer);

    VkMemoryRequirements stagingMemReq;
    vkGetBufferMemoryRequirements(context_.device, stagingBuffer, &stagingMemReq);
    uint32_t stagingMemType = VulkanInitializer::findMemoryType(context_.physicalDevice, stagingMemReq.memoryTypeBits,
                                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo stagingAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = stagingMemReq.size,
        .memoryTypeIndex = stagingMemType
    };
    VkDeviceMemory stagingMemory;
    if (vkAllocateMemory(context_.device, &stagingAllocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to allocate staging memory for SBT");
    }
    context_.resourceManager.addMemory(stagingMemory);
    if (vkBindBufferMemory(context_.device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeMemory(stagingMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to bind staging memory for SBT");
    }

    // Map and copy data
    void* mappedData;
    if (vkMapMemory(context_.device, stagingMemory, 0, sbtSize, 0, &mappedData) != VK_SUCCESS) {
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeMemory(stagingMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to map staging memory for SBT");
    }
    uint8_t* data = static_cast<uint8_t*>(mappedData);
    memset(data, 0, sbtSize);

    // Copy handles with alignment
    memcpy(data + 0 * alignedHandleSize, shaderGroupHandles.data() + 0 * handleSize, handleSize); // Raygen
    memcpy(data + 1 * alignedHandleSize, shaderGroupHandles.data() + 1 * handleSize, handleSize); // Primary miss
    memcpy(data + 2 * alignedHandleSize, shaderGroupHandles.data() + 3 * handleSize, handleSize); // Shadow miss
    memcpy(data + 3 * alignedHandleSize, shaderGroupHandles.data() + 2 * handleSize, handleSize); // Primary hit
    memcpy(data + 4 * alignedHandleSize, shaderGroupHandles.data() + 4 * handleSize, handleSize); // Shadow hit
    memcpy(data + 5 * alignedHandleSize, shaderGroupHandles.data() + 5 * handleSize, handleSize); // Mid hit
    memcpy(data + 6 * alignedHandleSize, shaderGroupHandles.data() + 6 * handleSize, handleSize); // Volumetric hit
    memcpy(data + 7 * alignedHandleSize, shaderGroupHandles.data() + 7 * handleSize, handleSize); // Callable

    vkUnmapMemory(context_.device, stagingMemory);

    // Copy to device buffer
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd;
    if (vkAllocateCommandBuffers(context_.device, &allocInfo, &cmd) != VK_SUCCESS) {
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeMemory(stagingMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to allocate command buffer for SBT copy");
    }

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeMemory(stagingMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to begin command buffer for SBT copy");
    }

    VkBufferCopy copyRegion = { .size = sbtSize };
    vkCmdCopyBuffer(cmd, stagingBuffer, sbtBuffer, 1, &copyRegion);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeMemory(stagingMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to end command buffer for SBT copy");
    }

    // Submit and wait
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence;
    if (vkCreateFence(context_.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeMemory(stagingMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to create fence for SBT copy");
    }

    if (vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        vkDestroyFence(context_.device, fence, nullptr);
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeMemory(stagingMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to submit command buffer for SBT copy");
    }

    if (vkWaitForFences(context_.device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        vkDestroyFence(context_.device, fence, nullptr);
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeMemory(stagingMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to wait for fence in SBT copy");
    }

    vkDestroyFence(context_.device, fence, nullptr);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);

    // Clean up staging
    context_.resourceManager.removeBuffer(stagingBuffer);
    context_.resourceManager.removeMemory(stagingMemory);
    Dispose::destroySingleBuffer(context_.device, stagingBuffer);
    Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);

    // Remove SBT from resource manager since owned by sbt_
    context_.resourceManager.removeBuffer(sbtBuffer);
    context_.resourceManager.removeMemory(sbtMemory);

    VkDeviceAddress sbtAddress = VulkanInitializer::getBufferDeviceAddress(context_.device, sbtBuffer);

    sbt_ = ShaderBindingTable(context_.device, sbtBuffer, sbtMemory, vkDestroyBuffer, vkFreeMemory);
    sbt_.raygen = {sbtAddress + 0 * alignedHandleSize, alignedHandleSize, alignedHandleSize};
    sbt_.miss = {sbtAddress + 1 * alignedHandleSize, alignedHandleSize, 2 * alignedHandleSize};
    sbt_.hit = {sbtAddress + 3 * alignedHandleSize, alignedHandleSize, 4 * alignedHandleSize};
    sbt_.callable = {sbtAddress + 7 * alignedHandleSize, alignedHandleSize, alignedHandleSize};
}

void VulkanPipelineManager::recordRayTracingCommands(VkCommandBuffer commandBuffer, VkImage outputImage, 
                                                    VkDescriptorSet descriptorSet, uint32_t width, uint32_t height, 
                                                    VkImage gDepth, VkImage gNormal) {
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // Query properties once
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProperties
    };
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &properties);

    // Image barriers
    std::array<VkImageMemoryBarrier, 3> imageBarriers = {{
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = outputImage,
            .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = gDepth,
            .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = gNormal,
            .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
        }
    }};

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 
                         0, 0, nullptr, 0, nullptr, imageBarriers.size(), imageBarriers.data());

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, getRayTracingPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, getRayTracingPipelineLayout(), 
                            0, 1, &descriptorSet, 0, nullptr);

    MaterialData::PushConstants pushConstants{};
    pushConstants.resolution = glm::vec2(static_cast<float>(width), static_cast<float>(height));
    vkCmdPushConstants(commandBuffer, getRayTracingPipelineLayout(), 
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | 
                       VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | 
                       VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR, 
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    auto vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCmdTraceRaysKHR"));
    if (!vkCmdTraceRaysKHR) {
        throw std::runtime_error("Failed to get vkCmdTraceRaysKHR function pointer");
    }

    vkCmdTraceRaysKHR(commandBuffer, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable, width, height, 1);

    VkImageMemoryBarrier finalBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = outputImage,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                         0, 0, nullptr, 0, nullptr, 1, &finalBarrier);

    vkEndCommandBuffer(commandBuffer);
}

} // namespace VulkanRTX