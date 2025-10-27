// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan compute pipeline management implementation.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, Vulkan_init.hpp, VulkanRTX_Setup.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows, Consoles (PS5, Xbox Series X).
// Optimized for high-end GPUs with 8 GB VRAM (e.g., NVIDIA RTX 3070, AMD RX 6800).
// Zachary Geurts 2025
// Beast mode enhancements: Batched compute pipeline creation for faster initialization, added push constants for dynamic resolution,
// fixed image layout transitions to allow denoiser write-back, integrated history image barrier for temporal denoising support,
// improved error handling and shader loading with filesystem validation. Crushes competition with ultra-efficient compute dispatching.

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

void VulkanPipelineManager::createComputePipeline() {
    std::vector<std::string> computeShaders = {"compute", "raster_prepass", "denoiser_post"};
    std::vector<VkShaderModule> shaderModules;
    shaderModules.reserve(computeShaders.size());
    for (const auto& type : computeShaders) {
        shaderModules.emplace_back(loadShader(context_.device, type));
    }

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
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
        throw std::runtime_error("Failed to create compute pipeline layout");
    }
    computePipelineLayout_ = std::make_unique<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>>(
        context_.device, pipelineLayout, vkDestroyPipelineLayout);
    context_.resourceManager.addPipelineLayout(pipelineLayout);

    std::vector<VkComputePipelineCreateInfo> pipelineInfos(computeShaders.size());
    for (size_t i = 0; i < computeShaders.size(); ++i) {
        VkPipelineShaderStageCreateInfo shaderStage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shaderModules[i],
            .pName = "main",
            .pSpecializationInfo = nullptr
        };

        pipelineInfos[i] = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = shaderStage,
            .layout = pipelineLayout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };
    }

    std::vector<VkPipeline> computePipelines(computeShaders.size());
    if (vkCreateComputePipelines(context_.device, pipelineCache_, static_cast<uint32_t>(pipelineInfos.size()), pipelineInfos.data(), nullptr, computePipelines.data()) != VK_SUCCESS) {
        for (auto module : shaderModules) {
            vkDestroyShaderModule(context_.device, module, nullptr);
        }
        vkDestroyPipelineLayout(context_.device, pipelineLayout, nullptr);
        throw std::runtime_error("Failed to create compute pipelines");
    }

    for (size_t i = 0; i < computePipelines.size(); ++i) {
        context_.resourceManager.addPipeline(computePipelines[i]);
        const auto& shaderType = computeShaders[i];
        if (shaderType == "compute") {
            computePipeline_ = std::make_unique<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>>(
                context_.device, computePipelines[i], vkDestroyPipeline);
        } else if (shaderType == "raster_prepass") {
            rasterPrepassPipeline_ = computePipelines[i];
        } else if (shaderType == "denoiser_post") {
            denoiserPostPipeline_ = computePipelines[i];
        }
    }

    // Clean up shader modules
    for (auto module : shaderModules) {
        vkDestroyShaderModule(context_.device, module, nullptr);
    }
}

void VulkanPipelineManager::recordComputeCommands(VkCommandBuffer commandBuffer, VkImage outputImage, 
                                                 VkDescriptorSet descriptorSet, uint32_t width, uint32_t height, 
                                                 VkImage gDepth, VkImage gNormal, VkImage historyImage) {

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin compute command buffer");
    }

    std::array<VkImageMemoryBarrier, 4> imageBarriers = {{
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
            .image = historyImage,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        }
    }};

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                         0, 0, nullptr, 0, nullptr, static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, rasterPrepassPipeline_);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, getComputePipelineLayout(), 
                            0, 1, &descriptorSet, 0, nullptr);

    MaterialData::PushConstants pushConstants{};
    pushConstants.resolution = glm::vec2(static_cast<float>(width), static_cast<float>(height));
    vkCmdPushConstants(commandBuffer, getComputePipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    uint32_t groupCountX = (width + 15) / 16;
    uint32_t groupCountY = (height + 15) / 16;
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

    VkImageMemoryBarrier postPrepassBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                         0, 0, nullptr, 0, nullptr, 1, &postPrepassBarrier);

    // Transition output back to general for denoiser write
    VkImageMemoryBarrier preDenoiserBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                         0, 0, nullptr, 0, nullptr, 1, &preDenoiserBarrier);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoiserPostPipeline_);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, getComputePipelineLayout(), 
                            0, 1, &descriptorSet, 0, nullptr);
    vkCmdPushConstants(commandBuffer, getComputePipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 
                       0, sizeof(MaterialData::PushConstants), &pushConstants);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

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

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                         0, 0, nullptr, 0, nullptr, 1, &finalBarrier);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end compute command buffer");
    }
}

} // namespace VulkanRTX