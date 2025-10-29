// AMOURANTH RTX Engine © 2025 Zachary Geurts gzac5314@gmail.com | CC BY-NC 4.0
// Vulkan compute pipeline: batched creation, push constants, denoiser barriers.
// Vulkan 1.3+ | GLM | 8GB+ GPUs (RTX 3070, RX 6800) | Linux/Windows/PS5/XSX

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Dispose.hpp"
#include <utility>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <array>
#include <vector>
#include <glm/glm.hpp>
#include <unordered_map>
#include <format>

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

namespace VulkanRTX {

#define VK_CHECK(x) do { VkResult r = (x); if (r != VK_SUCCESS) { LOG_ERROR_CAT("Pipeline", "{} failed: {}", #x, static_cast<int>(r)); throw std::runtime_error(std::format("{} failed", #x)); } } while(0)

// RAII ScopeGuard
template<class F>
struct ScopeGuard { F f; ~ScopeGuard() { f(); } };
template<class F>
ScopeGuard<F> finally(F&& f) { return {std::forward<F>(f)}; }

void VulkanPipelineManager::createComputePipeline() {
    std::vector<std::string> computeShaders = {"compute", "raster_prepass", "denoiser_post"};
    std::vector<VkShaderModule> shaderModules;
    shaderModules.reserve(computeShaders.size());

    auto cleanup = finally([&]{
        for (auto m : shaderModules)
            vkDestroyShaderModule(context_.device, m, nullptr);
    });

    for (const auto& type : computeShaders)
        shaderModules.emplace_back(loadShader(context_.device, type));

    VkPushConstantRange pushRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .size = sizeof(MaterialData::PushConstants)
    };

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &context_.rayTracingDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange
    };

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout));
    computePipelineLayout_ = std::make_unique<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>>(
        context_.device, layout, vkDestroyPipelineLayout);
    context_.resourceManager.addPipelineLayout(layout);

    std::vector<VkComputePipelineCreateInfo> pipelineInfos(computeShaders.size());
    for (size_t i = 0; i < computeShaders.size(); ++i) {
        VkPipelineShaderStageCreateInfo stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shaderModules[i],
            .pName = "main"
        };
        pipelineInfos[i] = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = stage,
            .layout = layout
        };
    }

    std::vector<VkPipeline> pipelines(computeShaders.size());
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, static_cast<uint32_t>(pipelineInfos.size()),
                                      pipelineInfos.data(), nullptr, pipelines.data()));

    for (size_t i = 0; i < pipelines.size(); ++i) {
        context_.resourceManager.addPipeline(pipelines[i]);
        const auto& type = computeShaders[i];
        if (type == "compute")
            computePipeline_ = std::make_unique<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>>(context_.device, pipelines[i], vkDestroyPipeline);
        else if (type == "raster_prepass")
            rasterPrepassPipeline_ = pipelines[i];
        else if (type == "denoiser_post")
            denoiserPostPipeline_ = pipelines[i];
    }
}

void VulkanPipelineManager::recordComputeCommands(VkCommandBuffer cmd, VkImage output, VkDescriptorSet ds,
                                                 uint32_t w, uint32_t h, VkImage gDepth, VkImage gNormal, VkImage history) {
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    VkImageMemoryBarrier barriers[4] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = output,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = gDepth,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = gNormal,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = history,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        }
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 4, barriers);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rasterPrepassPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, getComputePipelineLayout(), 0, 1, &ds, 0, nullptr);

    MaterialData::PushConstants pc{};
    pc.resolution = glm::vec2(static_cast<float>(w), static_cast<float>(h));
    vkCmdPushConstants(cmd, getComputePipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

    uint32_t gx = (w + 15) / 16, gy = (h + 15) / 16;
    vkCmdDispatch(cmd, gx, gy, 1);

    VkImageMemoryBarrier postPrepassBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = output,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &postPrepassBarrier);

    VkImageMemoryBarrier preDenoiserBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = output,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &preDenoiserBarrier);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, denoiserPostPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, getComputePipelineLayout(), 0, 1, &ds, 0, nullptr);
    vkCmdPushConstants(cmd, getComputePipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vkCmdDispatch(cmd, gx, gy, 1);

    VkImageMemoryBarrier finalBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = output,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &finalBarrier);

    VK_CHECK(vkEndCommandBuffer(cmd));
}

} // namespace VulkanRTX