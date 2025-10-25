// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan ray tracing pipeline management implementation.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, Vulkan_init.hpp, VulkanRTX_Setup.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows, Consoles (PS5, Xbox Series X).
// Optimized for high-end GPUs with 8 GB VRAM (e.g., NVIDIA RTX 3070, AMD RX 6800).
// Zachary Geurts 2025

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Dispose.hpp"
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <array>
#include <fstream>
#include <filesystem>
#include <vector>
#include <glm/glm.hpp>
#include <unordered_map>

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

namespace VulkanRTX {

void VulkanPipelineManager::createRayTracingPipeline() {
    std::vector<std::string> shaderTypes = {"raygen", "miss", "closesthit", "shadowmiss", "anyhit", "intersection", "callable", "shadow_anyhit", "mid_anyhit", "volumetric_anyhit", "compute"};
    std::vector<VkShaderModule> shaderModules;
    for (const auto& type : shaderTypes) {
        shaderModules.push_back(loadShader(context_.device, type));
    }

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = shaderModules[0],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = shaderModules[1],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = shaderModules[2],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = shaderModules[3],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[4],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
            .module = shaderModules[5],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
            .module = shaderModules[6],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[7],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[8],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[9],
            .pName = "main",
            .pSpecializationInfo = nullptr
        }
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups = {
        // Raygen (group 0)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 0,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Primary miss (group 1)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 1,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Primary hit: triangles (group 2)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = 4,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Shadow miss (group 3)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 3,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Shadow hit: triangles (group 4)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = 7,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Mid-layer hit: triangles (group 5)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = 8,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Volumetric hit: procedural (group 6)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = 9,
            .intersectionShader = 5,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Callable (group 7)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 6,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        }
    };

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | 
                      VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | 
                      VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        .offset = 0,
        .size = sizeof(MaterialData::PushConstants)
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
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

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {};
    rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 properties2 = {};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &properties2);

    uint32_t recursionDepth = (rtProperties.maxRayRecursionDepth > 0) ? std::min(4u, rtProperties.maxRayRecursionDepth) : 1;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .groupCount = static_cast<uint32_t>(shaderGroups.size()),
        .pGroups = shaderGroups.data(),
        .maxPipelineRayRecursionDepth = recursionDepth,
        .pLibraryInfo = nullptr,
        .pLibraryInterface = nullptr,
        .pDynamicState = nullptr,
        .layout = pipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    auto vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCreateRayTracingPipelinesKHR"));
    if (!vkCreateRayTracingPipelinesKHR) {
        for (auto module : shaderModules) {
            vkDestroyShaderModule(context_.device, module, nullptr);
        }
        vkDestroyPipelineLayout(context_.device, pipelineLayout, nullptr);
        throw std::runtime_error("Failed to get vkCreateRayTracingPipelinesKHR function pointer");
    }

    VkPipeline pipeline;
    VkResult result = vkCreateRayTracingPipelinesKHR(context_.device, VK_NULL_HANDLE, pipelineCache_, 1, &pipelineInfo, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        for (auto module : shaderModules) {
            vkDestroyShaderModule(context_.device, module, nullptr);
        }
        vkDestroyPipelineLayout(context_.device, pipelineLayout, nullptr);
        throw std::runtime_error("Failed to create ray-tracing pipeline");
    }
    rayTracingPipeline_ = std::make_unique<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>>(
        context_.device, pipeline, vkDestroyPipeline);
    context_.resourceManager.addPipeline(pipeline);

    for (auto module : shaderModules) {
        vkDestroyShaderModule(context_.device, module, nullptr);
    }
}

void VulkanPipelineManager::createShaderBindingTable() {
    if (!rayTracingPipeline_) {
        throw std::runtime_error("Ray-tracing pipeline not initialized");
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {};
    rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &properties);

    const uint32_t groupCount = 8; // From createRayTracingPipeline: raygen, miss, closesthit, shadowmiss, shadow_anyhit, mid_anyhit, volumetric_anyhit, callable
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

    VkBufferUsageFlags sbtUsage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags sbtMemoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkBuffer sbtBuffer = VK_NULL_HANDLE;
    VkDeviceMemory sbtMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, sbtSize, sbtUsage, sbtMemoryProps, sbtBuffer, sbtMemory, nullptr, context_.resourceManager);

    // Create staging buffer for SBT initialization
    VkBufferCreateInfo stagingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = sbtSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(context_.device, &stagingCreateInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to create staging buffer for SBT");
    }
    context_.resourceManager.addBuffer(stagingBuffer);  // Add to resource manager for cleanup

    VkMemoryRequirements stagingMemReq{};
    vkGetBufferMemoryRequirements(context_.device, stagingBuffer, &stagingMemReq);
    uint32_t stagingMemType = VulkanInitializer::findMemoryType(context_.physicalDevice, stagingMemReq.memoryTypeBits,
                                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo stagingAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = stagingMemReq.size,
        .memoryTypeIndex = stagingMemType
    };
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(context_.device, &stagingAllocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to allocate staging memory for SBT");
    }
    context_.resourceManager.addMemory(stagingMemory);  // Add to resource manager for cleanup
    if (vkBindBufferMemory(context_.device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
        // Cleanup
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

    // Map staging and copy data with proper alignment
    void* mappedData = nullptr;
    if (vkMapMemory(context_.device, stagingMemory, 0, sbtSize, 0, &mappedData) != VK_SUCCESS) {
        // Cleanup
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

    // Raygen (group 0) at offset 0
    memcpy(data + 0 * alignedHandleSize, shaderGroupHandles.data() + 0 * handleSize, handleSize);

    // Miss: primary (group 1) at 1a, shadow (group 3) at 2a
    memcpy(data + 1 * alignedHandleSize, shaderGroupHandles.data() + 1 * handleSize, handleSize);
    memcpy(data + 2 * alignedHandleSize, shaderGroupHandles.data() + 3 * handleSize, handleSize);

    // Hit: primary (group 2) at 3a + 0a, shadow (group 4) at 3a + 1a, mid (group 5) at 3a + 2a, vol (group 6) at 3a + 3a
    memcpy(data + 3 * alignedHandleSize + 0 * alignedHandleSize, shaderGroupHandles.data() + 2 * handleSize, handleSize);
    memcpy(data + 3 * alignedHandleSize + 1 * alignedHandleSize, shaderGroupHandles.data() + 4 * handleSize, handleSize);
    memcpy(data + 3 * alignedHandleSize + 2 * alignedHandleSize, shaderGroupHandles.data() + 5 * handleSize, handleSize);
    memcpy(data + 3 * alignedHandleSize + 3 * alignedHandleSize, shaderGroupHandles.data() + 6 * handleSize, handleSize);

    // Callable (group 7) at 7a
    memcpy(data + 7 * alignedHandleSize, shaderGroupHandles.data() + 7 * handleSize, handleSize);

    vkUnmapMemory(context_.device, stagingMemory);

    // Copy from staging to SBT buffer using command buffer
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(context_.device, &allocInfo, &cmd) != VK_SUCCESS) {
        // Cleanup
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
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        // Cleanup staging and sbt...
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

    VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = sbtSize
    };
    vkCmdCopyBuffer(cmd, stagingBuffer, sbtBuffer, 1, &copyRegion);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        // Cleanup...
        throw std::runtime_error("Failed to end command buffer for SBT copy");
    }

    // Submit
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(context_.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        // Cleanup...
        throw std::runtime_error("Failed to create fence for SBT copy");
    }

    if (vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        vkDestroyFence(context_.device, fence, nullptr);
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        // Cleanup...
        throw std::runtime_error("Failed to submit command buffer for SBT copy");
    }

    if (vkWaitForFences(context_.device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        vkDestroyFence(context_.device, fence, nullptr);
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        // Cleanup...
        throw std::runtime_error("Failed to wait for fence in SBT copy");
    }

    vkDestroyFence(context_.device, fence, nullptr);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);

    // Cleanup staging
    context_.resourceManager.removeBuffer(stagingBuffer);
    context_.resourceManager.removeMemory(stagingMemory);
    Dispose::destroySingleBuffer(context_.device, stagingBuffer);
    Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);

    VkDeviceAddress sbtAddress = VulkanInitializer::getBufferDeviceAddress(context_.device, sbtBuffer);

    sbt_ = ShaderBindingTable(context_.device, sbtBuffer, sbtMemory, vkDestroyBuffer, vkFreeMemory);
    sbt_.raygen = {sbtAddress + 0 * alignedHandleSize, alignedHandleSize, alignedHandleSize};
    sbt_.miss = {sbtAddress + 1 * alignedHandleSize, alignedHandleSize, alignedHandleSize};
    sbt_.hit = {sbtAddress + 3 * alignedHandleSize, alignedHandleSize, alignedHandleSize};
    sbt_.callable = {sbtAddress + 7 * alignedHandleSize, alignedHandleSize, alignedHandleSize};
}

void VulkanPipelineManager::recordRayTracingCommands(VkCommandBuffer commandBuffer, VkImage outputImage, 
                                                    VkDescriptorSet descriptorSet, uint32_t width, uint32_t height, 
                                                    VkImage gDepth, VkImage gNormal) {
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {};
    rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &properties);

    VkImageMemoryBarrier imageBarriers[3] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = outputImage,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = gDepth,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = gNormal,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        }
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 
                         0, 0, nullptr, 0, nullptr, 3, imageBarriers);

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

    VkStridedDeviceAddressRegionKHR raygenSbt = sbt_.raygen;
    VkStridedDeviceAddressRegionKHR missSbt = sbt_.miss;
    VkStridedDeviceAddressRegionKHR hitSbt = sbt_.hit;
    VkStridedDeviceAddressRegionKHR callableSbt = sbt_.callable;

    vkCmdTraceRaysKHR(commandBuffer, &raygenSbt, &missSbt, &hitSbt, &callableSbt, width, height, 1);

    VkImageMemoryBarrier finalBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = outputImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                         0, 0, nullptr, 0, nullptr, 1, &finalBarrier);

    vkEndCommandBuffer(commandBuffer);
}

} // namespace VulkanRTX