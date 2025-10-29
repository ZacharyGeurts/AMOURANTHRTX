// AMOURANTH RTX Engine, October 2025 - Implementations for centralized resource disposal.
// Zachary Geurts 2025

#include "engine/Dispose.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/types.hpp"
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <format>
#include <ranges>
#include <functional>
#include <unordered_map>
#include "engine/logging.hpp"

namespace Dispose {

enum class ResourceType {
    StorageImageView, StorageImage, StorageImageMemory,
    RenderPass, GraphicsPipeline, ComputePipeline,
    GraphicsPipelineLayout, RayTracingPipelineLayout, ComputePipelineLayout,
    GraphicsDescriptorSet, DescriptorSet,
    GraphicsDescriptorPool, DescriptorPool,
    GraphicsDescriptorSetLayout, RayTracingDescriptorSetLayout,
    Sampler,
    TopLevelAS, BottomLevelAS,
    BottomLevelASBuffer, BottomLevelASMemory,
    TopLevelASBuffer, TopLevelASMemory,
    RaygenSbtBuffer, RaygenSbtMemory,
    MissSbtBuffer, MissSbtMemory,
    HitSbtBuffer, HitSbtMemory,
    Swapchain, CommandPool, Device, DebugMessenger, Instance,
    VertexBuffer, VertexBufferMemory,
    IndexBuffer, IndexBufferMemory,
    ScratchBuffer, ScratchBufferMemory,
    RayTracingPipeline,
    Surface
};

struct Disposable {
    ResourceType type;
    void* handle;
    bool managedByResourceManager = false;
    std::string name;
};

using CleanupFn = std::function<void(Vulkan::Context&, void*, bool)>;
static const std::unordered_map<ResourceType, CleanupFn> cleanupLookup = {
    {ResourceType::StorageImageView, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkImageView handle = static_cast<VkImageView>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& imageViews = ctx.resourceManager.getImageViews();
            if (std::ranges::find(imageViews, handle) != imageViews.end()) {
                LOG_DEBUG(std::format("Removing storage image view {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeImageView(handle);
            }
        }
        destroySingleImageView(ctx.device, handle);
    }},
    {ResourceType::StorageImage, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkImage handle = static_cast<VkImage>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& images = ctx.resourceManager.getImages();
            if (std::ranges::find(images, handle) != images.end()) {
                LOG_DEBUG(std::format("Removing storage image {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeImage(handle);
            }
        }
        destroySingleImage(ctx.device, handle);
    }},
    {ResourceType::StorageImageMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            if (std::ranges::find(memories, handle) != memories.end()) {
                LOG_DEBUG(std::format("Removing storage image memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
    }},
    {ResourceType::RenderPass, [](Vulkan::Context& ctx, void* h, bool) {
        VkRenderPass handle = static_cast<VkRenderPass>(h);
        destroySingleRenderPass(ctx.device, handle);
    }},
    {ResourceType::GraphicsPipeline, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkPipeline handle = static_cast<VkPipeline>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& pipelines = ctx.resourceManager.getPipelines();
            if (std::ranges::find(pipelines, handle) != pipelines.end()) {
                LOG_DEBUG(std::format("Removing graphics pipeline {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removePipeline(handle);
            }
        }
        destroySinglePipeline(ctx.device, handle);
    }},
    {ResourceType::ComputePipeline, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkPipeline handle = static_cast<VkPipeline>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& pipelines = ctx.resourceManager.getPipelines();
            if (std::ranges::find(pipelines, handle) != pipelines.end()) {
                LOG_DEBUG(std::format("Removing compute pipeline {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removePipeline(handle);
            }
        }
        destroySinglePipeline(ctx.device, handle);
    }},
    {ResourceType::RayTracingPipeline, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkPipeline handle = static_cast<VkPipeline>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& pipelines = ctx.resourceManager.getPipelines();
            if (std::ranges::find(pipelines, handle) != pipelines.end()) {
                LOG_DEBUG(std::format("Removing ray-tracing pipeline {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removePipeline(handle);
            }
        }
        destroySinglePipeline(ctx.device, handle);
    }},
    {ResourceType::GraphicsPipelineLayout, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkPipelineLayout handle = static_cast<VkPipelineLayout>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& layouts = ctx.resourceManager.getPipelineLayouts();
            if (std::ranges::find(layouts, handle) != layouts.end()) {
                LOG_DEBUG(std::format("Removing graphics pipeline layout {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removePipelineLayout(handle);
            }
        }
        destroySinglePipelineLayout(ctx.device, handle);
    }},
    {ResourceType::RayTracingPipelineLayout, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkPipelineLayout handle = static_cast<VkPipelineLayout>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& layouts = ctx.resourceManager.getPipelineLayouts();
            if (std::ranges::find(layouts, handle) != layouts.end()) {
                LOG_DEBUG(std::format("Removing ray-tracing pipeline layout {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removePipelineLayout(handle);
            }
        }
        destroySinglePipelineLayout(ctx.device, handle);
    }},
    {ResourceType::ComputePipelineLayout, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkPipelineLayout handle = static_cast<VkPipelineLayout>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& layouts = ctx.resourceManager.getPipelineLayouts();
            if (std::ranges::find(layouts, handle) != layouts.end()) {
                LOG_DEBUG(std::format("Removing compute pipeline layout {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removePipelineLayout(handle);
            }
        }
        destroySinglePipelineLayout(ctx.device, handle);
    }},
    {ResourceType::GraphicsDescriptorSet, [](Vulkan::Context& ctx, void* h, bool) {
        VkDescriptorSet handle = static_cast<VkDescriptorSet>(h);
        if (ctx.graphicsDescriptorPool != VK_NULL_HANDLE) {
            freeSingleDescriptorSet(ctx.device, ctx.graphicsDescriptorPool, handle);
        }
    }},
    {ResourceType::DescriptorSet, [](Vulkan::Context& ctx, void* h, bool) {
        VkDescriptorSet handle = static_cast<VkDescriptorSet>(h);
        if (ctx.descriptorPool != VK_NULL_HANDLE) {
            freeSingleDescriptorSet(ctx.device, ctx.descriptorPool, handle);
        }
    }},
    {ResourceType::GraphicsDescriptorPool, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDescriptorPool handle = static_cast<VkDescriptorPool>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& pools = ctx.resourceManager.getDescriptorPools();
            if (std::ranges::find(pools, handle) != pools.end()) {
                LOG_DEBUG(std::format("Removing graphics descriptor pool {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeDescriptorPool(handle);
            }
        }
        destroySingleDescriptorPool(ctx.device, handle);
    }},
    {ResourceType::DescriptorPool, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDescriptorPool handle = static_cast<VkDescriptorPool>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& pools = ctx.resourceManager.getDescriptorPools();
            if (std::ranges::find(pools, handle) != pools.end()) {
                LOG_DEBUG(std::format("Removing descriptor pool {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeDescriptorPool(handle);
            }
        }
        destroySingleDescriptorPool(ctx.device, handle);
    }},
    {ResourceType::GraphicsDescriptorSetLayout, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDescriptorSetLayout handle = static_cast<VkDescriptorSetLayout>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& layouts = ctx.resourceManager.getDescriptorSetLayouts();
            if (std::ranges::find(layouts, handle) != layouts.end()) {
                LOG_DEBUG(std::format("Removing graphics descriptor set layout {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeDescriptorSetLayout(handle);
            }
        }
        destroySingleDescriptorSetLayout(ctx.device, handle);
    }},
    {ResourceType::RayTracingDescriptorSetLayout, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDescriptorSetLayout handle = static_cast<VkDescriptorSetLayout>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& layouts = ctx.resourceManager.getDescriptorSetLayouts();
            if (std::ranges::find(layouts, handle) != layouts.end()) {
                LOG_DEBUG(std::format("Removing ray-tracing descriptor set layout {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeDescriptorSetLayout(handle);
            }
        }
        destroySingleDescriptorSetLayout(ctx.device, handle);
    }},
    {ResourceType::Sampler, [](Vulkan::Context& ctx, void* h, bool) {
        VkSampler handle = static_cast<VkSampler>(h);
        destroySingleSampler(ctx.device, handle);
    }},
    {ResourceType::TopLevelAS, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkAccelerationStructureKHR handle = static_cast<VkAccelerationStructureKHR>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& structures = ctx.resourceManager.getAccelerationStructures();
            if (std::ranges::find(structures, handle) != structures.end()) {
                LOG_DEBUG(std::format("Removing top-level AS {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeAccelerationStructure(handle);
            }
        }
        destroySingleAccelerationStructure(ctx.device, handle);
    }},
    {ResourceType::BottomLevelAS, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkAccelerationStructureKHR handle = static_cast<VkAccelerationStructureKHR>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& structures = ctx.resourceManager.getAccelerationStructures();
            if (std::ranges::find(structures, handle) != structures.end()) {
                LOG_DEBUG(std::format("Removing bottom-level AS {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeAccelerationStructure(handle);
            }
        }
        destroySingleAccelerationStructure(ctx.device, handle);
    }},
    {ResourceType::BottomLevelASBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            if (std::ranges::find(buffers, handle) != buffers.end()) {
                LOG_DEBUG(std::format("Removing bottom-level AS buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
            }
        }
        destroySingleBuffer(ctx.device, handle);
    }},
    {ResourceType::BottomLevelASMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            if (std::ranges::find(memories, handle) != memories.end()) {
                LOG_DEBUG(std::format("Removing bottom-level AS memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
    }},
    {ResourceType::TopLevelASBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            if (std::ranges::find(buffers, handle) != buffers.end()) {
                LOG_DEBUG(std::format("Removing top-level AS buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
            }
        }
        destroySingleBuffer(ctx.device, handle);
    }},
    {ResourceType::TopLevelASMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            if (std::ranges::find(memories, handle) != memories.end()) {
                LOG_DEBUG(std::format("Removing top-level AS memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
    }},
    {ResourceType::RaygenSbtBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            if (std::ranges::find(buffers, handle) != buffers.end()) {
                LOG_DEBUG(std::format("Removing raygen SBT buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
            }
        }
        destroySingleBuffer(ctx.device, handle);
    }},
    {ResourceType::RaygenSbtMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            if (std::ranges::find(memories, handle) != memories.end()) {
                LOG_DEBUG(std::format("Removing raygen SBT memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
    }},
    {ResourceType::MissSbtBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            if (std::ranges::find(buffers, handle) != buffers.end()) {
                LOG_DEBUG(std::format("Removing miss SBT buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
            }
        }
        destroySingleBuffer(ctx.device, handle);
    }},
    {ResourceType::MissSbtMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            if (std::ranges::find(memories, handle) != memories.end()) {
                LOG_DEBUG(std::format("Removing miss SBT memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
    }},
    {ResourceType::HitSbtBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            if (std::ranges::find(buffers, handle) != buffers.end()) {
                LOG_DEBUG(std::format("Removing hit SBT buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
            }
        }
        destroySingleBuffer(ctx.device, handle);
    }},
    {ResourceType::HitSbtMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            if (std::ranges::find(memories, handle) != memories.end()) {
                LOG_DEBUG(std::format("Removing hit SBT memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
    }},
    {ResourceType::VertexBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            if (std::ranges::find(buffers, handle) != buffers.end()) {
                LOG_DEBUG(std::format("Removing vertex buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
            }
        }
        destroySingleBuffer(ctx.device, handle);
    }},
    {ResourceType::VertexBufferMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            if (std::ranges::find(memories, handle) != memories.end()) {
                LOG_DEBUG(std::format("Removing vertex buffer memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
    }},
    {ResourceType::IndexBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            if (std::ranges::find(buffers, handle) != buffers.end()) {
                LOG_DEBUG(std::format("Removing index buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
            }
        }
        destroySingleBuffer(ctx.device, handle);
    }},
    {ResourceType::IndexBufferMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            if (std::ranges::find(memories, handle) != memories.end()) {
                LOG_DEBUG(std::format("Removing index buffer memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
    }},
    {ResourceType::ScratchBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            if (std::ranges::find(buffers, handle) != buffers.end()) {
                LOG_DEBUG(std::format("Removing scratch buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
            }
        }
        destroySingleBuffer(ctx.device, handle);
    }},
    {ResourceType::ScratchBufferMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            if (std::ranges::find(memories, handle) != memories.end()) {
                LOG_DEBUG(std::format("Removing scratch buffer memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
    }},
    {ResourceType::Swapchain, [](Vulkan::Context& ctx, void* h, bool) {
        VkSwapchainKHR handle = static_cast<VkSwapchainKHR>(h);
        if (handle != VK_NULL_HANDLE && ctx.device != VK_NULL_HANDLE) {
            destroySingleSwapchain(ctx.device, handle);
        }
    }},
    {ResourceType::CommandPool, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkCommandPool handle = static_cast<VkCommandPool>(h);
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& pools = ctx.resourceManager.getCommandPools();
            if (std::ranges::find(pools, handle) != pools.end()) {
                LOG_DEBUG(std::format("Removing command pool {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeCommandPool(handle);
            }
        }
        destroySingleCommandPool(ctx.device, handle);
    }},
    {ResourceType::Device, [](Vulkan::Context&, void* h, bool) {
        VkDevice handle = static_cast<VkDevice>(h);
        destroyDevice(handle);
    }},
    {ResourceType::DebugMessenger, [](Vulkan::Context& ctx, void* h, bool) {
        VkDebugUtilsMessengerEXT handle = static_cast<VkDebugUtilsMessengerEXT>(h);
        destroyDebugUtilsMessengerEXT(ctx.instance, handle);
    }},
    {ResourceType::Instance, [](Vulkan::Context&, void* h, bool) {
        VkInstance handle = static_cast<VkInstance>(h);
        destroyInstance(handle);
    }},
    {ResourceType::Surface, [](Vulkan::Context& ctx, void* h, bool) {
        VkSurfaceKHR handle = static_cast<VkSurfaceKHR>(h);
        destroySurfaceKHR(ctx.instance, handle);
    }}
};

void updateDescriptorSets(Vulkan::Context& context) noexcept {
    try {
        LOG_DEBUG("Starting descriptor set update");

        if (context.device == VK_NULL_HANDLE) {
            LOG_ERROR("Cannot update descriptor sets: device is null");
            return;
        }

        if (context.descriptorSet == VK_NULL_HANDLE) {
            LOG_ERROR("Descriptor set is null");
            return;
        }

        const uint32_t expectedDescriptorCount = 26;
        if (context.uniformBuffers.size() < expectedDescriptorCount) {
            LOG_ERROR(std::format("Insufficient uniform buffers: have {}, need {}", context.uniformBuffers.size(), expectedDescriptorCount));
            return;
        }

        std::vector<VkDescriptorBufferInfo> bufferInfos(expectedDescriptorCount);
        const auto& managedBuffers = context.resourceManager.getBuffers();
        uint32_t successfulBuffers = 0;
        uint32_t failedBuffers = 0;

        for (uint32_t i = 0; i < expectedDescriptorCount; ++i) {
            try {
                if (i >= context.uniformBuffers.size() || context.uniformBuffers[i] == VK_NULL_HANDLE) {
                    LOG_ERROR(std::format("Uniform buffer[{}] is null or out of range", i));
                    failedBuffers++;
                    continue;
                }

                if (std::ranges::find(managedBuffers, context.uniformBuffers[i]) == managedBuffers.end()) {
                    LOG_ERROR(std::format("Uniform buffer[{}] ({:p}) is not managed by VulkanResourceManager", i, static_cast<void*>(context.uniformBuffers[i])));
                    failedBuffers++;
                    continue;
                }

                bufferInfos[i] = VkDescriptorBufferInfo{
                    .buffer = context.uniformBuffers[i],
                    .offset = 0,
                    .range = sizeof(UniformBufferObject) + sizeof(int) + 2048
                };
                LOG_DEBUG(std::format("Prepared descriptor buffer info[{}]: buffer={:p}", i, static_cast<void*>(bufferInfos[i].buffer)));
                successfulBuffers++;
            } catch (const std::exception& e) {
                LOG_ERROR(std::format("Error preparing buffer[{}]: {}", i, e.what()));
                failedBuffers++;
            }
        }

        if (successfulBuffers < expectedDescriptorCount) {
            LOG_WARNING(std::format("Descriptor set update incomplete: {}/{} buffers prepared successfully", successfulBuffers, expectedDescriptorCount));
            if (successfulBuffers == 0) {
                LOG_ERROR("No valid buffers prepared, aborting descriptor set update");
                return;
            }
        }

        VkWriteDescriptorSet descriptorWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = context.descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = expectedDescriptorCount,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = bufferInfos.data(),
            .pTexelBufferView = nullptr
        };

        std::vector<VkWriteDescriptorSet> descriptorWrites = {descriptorWrite};
        vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        LOG_INFO(std::format("Updated descriptor set {:p} with {} buffers ({} successful, {} failed)", 
                            static_cast<void*>(context.descriptorSet), expectedDescriptorCount, successfulBuffers, failedBuffers));
    } catch (const std::exception& e) {
        LOG_ERROR(std::format("Unexpected error in updateDescriptorSets: {}", e.what()));
    }
}

void cleanupVulkanContext(Vulkan::Context& context) noexcept {
    try {        
        LOG_INFO("Starting Vulkan context cleanup - Linear passthrough mode enabled");
        std::vector<std::string> cleanupErrors;

        // Step 0: Wait for all in-flight resources
        LOG_DEBUG("Step 0: Waiting for all in-flight fences");
        if (!context.inFlightFences.empty()) {
            std::vector<VkFence> fencesToWait;
            for (const auto& fence : context.inFlightFences) {
                if (fence != VK_NULL_HANDLE) {
                    fencesToWait.push_back(fence);
                }
            }
            if (!fencesToWait.empty() && context.device != VK_NULL_HANDLE) {
                const uint64_t timeout = 3000000000ULL; // 3 seconds
                VkResult fenceRes = vkWaitForFences(context.device, static_cast<uint32_t>(fencesToWait.size()), fencesToWait.data(), VK_TRUE, timeout);
                if (fenceRes == VK_ERROR_DEVICE_LOST) {
                    cleanupErrors.push_back("Device lost while waiting for fences");
                    LOG_ERROR("Device lost while waiting for fences");
                } else if (fenceRes == VK_TIMEOUT) {
                    cleanupErrors.push_back("Fence wait timed out after 3 seconds");
                    LOG_WARNING("Fence wait timed out after 3 seconds, proceeding with cleanup");
                } else if (fenceRes != VK_SUCCESS) {
                    cleanupErrors.push_back(std::format("vkWaitForFences failed (code: {})", static_cast<int>(fenceRes)));
                    LOG_WARNING(std::format("vkWaitForFences failed (code: {}), proceeding with cleanup", static_cast<int>(fenceRes)));
                } else {
                    LOG_DEBUG("All in-flight fences waited successfully");
                }
            }
            context.inFlightFences.clear();
        }

        // Step 1: Wait for queues to idle
        LOG_DEBUG("Step 1: Waiting for queues to idle");
        if (context.graphicsQueue != VK_NULL_HANDLE && context.device != VK_NULL_HANDLE) {
            VkResult result = vkQueueWaitIdle(context.graphicsQueue);
            if (result != VK_SUCCESS) {
                cleanupErrors.push_back(std::format("vkQueueWaitIdle for graphics queue failed (code: {})", static_cast<int>(result)));
                LOG_WARNING(std::format("vkQueueWaitIdle for graphics queue failed (code: {}), proceeding with cleanup", static_cast<int>(result)));
            } else {
                LOG_DEBUG("Graphics queue idle");
            }
        }
        if (context.presentQueue != VK_NULL_HANDLE && context.device != VK_NULL_HANDLE) {
            VkResult result = vkQueueWaitIdle(context.presentQueue);
            if (result != VK_SUCCESS) {
                cleanupErrors.push_back(std::format("vkQueueWaitIdle for present queue failed (code: {})", static_cast<int>(result)));
                LOG_WARNING(std::format("vkQueueWaitIdle for present queue failed (code: {}), proceeding with cleanup", static_cast<int>(result)));
            } else {
                LOG_DEBUG("Present queue idle");
            }
        }

        // Step 1.5: Final device wait
        LOG_DEBUG("Step 1.5: Waiting for device to idle");
        if (context.device != VK_NULL_HANDLE) {
            VkResult deviceRes = vkDeviceWaitIdle(context.device);
            if (deviceRes != VK_SUCCESS) {
                cleanupErrors.push_back(std::format("vkDeviceWaitIdle failed (code: {})", static_cast<int>(deviceRes)));
                LOG_WARNING(std::format("vkDeviceWaitIdle failed (code: {}), proceeding with cleanup", static_cast<int>(deviceRes)));
            } else {
                LOG_DEBUG("Device idle");
            }
        }

        // Step 2: Resource Manager Cleanup
        LOG_DEBUG("Step 2: Checking VulkanResourceManager state before cleanup");
        try {
            bool hasResources = false;
            if (!context.resourceManager.getBuffers().empty() ||
                !context.resourceManager.getMemories().empty() ||
                !context.resourceManager.getImages().empty() ||
                !context.resourceManager.getImageViews().empty() ||
                !context.resourceManager.getPipelines().empty() ||
                !context.resourceManager.getPipelineLayouts().empty() ||
                !context.resourceManager.getDescriptorSetLayouts().empty() ||
                !context.resourceManager.getDescriptorPools().empty() ||
                !context.resourceManager.getAccelerationStructures().empty() ||
                !context.resourceManager.getCommandPools().empty()) {
                hasResources = true;
                LOG_DEBUG(std::format("Resource manager contains resources: buffers={}, memories={}, images={}, imageViews={}, pipelines={}, pipelineLayouts={}, descriptorSetLayouts={}, descriptorPools={}, accelerationStructures={}, commandPools={}",
                    context.resourceManager.getBuffers().size(),
                    context.resourceManager.getMemories().size(),
                    context.resourceManager.getImages().size(),
                    context.resourceManager.getImageViews().size(),
                    context.resourceManager.getPipelines().size(),
                    context.resourceManager.getPipelineLayouts().size(),
                    context.resourceManager.getDescriptorSetLayouts().size(),
                    context.resourceManager.getDescriptorPools().size(),
                    context.resourceManager.getAccelerationStructures().size(),
                    context.resourceManager.getCommandPools().size()));
            } else {
                LOG_DEBUG("Resource manager is empty, skipping cleanup");
            }

            if (hasResources && context.device != VK_NULL_HANDLE) {
                LOG_DEBUG("Executing VulkanResourceManager cleanup");
                context.resourceManager.cleanup(context.device);
                LOG_INFO("VulkanResourceManager cleanup completed");
            } else {
                LOG_INFO("VulkanResourceManager cleanup skipped (no resources or null device)");
            }
        } catch (const std::exception& e) {
            cleanupErrors.push_back(std::format("Error in resource manager cleanup: {}", e.what()));
            LOG_ERROR(std::format("Error in resource manager cleanup: {}", e.what()));
        }

        // Step 3: Framebuffers
        LOG_DEBUG("Step 3: Cleaning up framebuffers");
        if (!context.framebuffers.empty()) {
            for (size_t i = 0; i < context.framebuffers.size(); ++i) {
                LOG_DEBUG(std::format("Framebuffer[{}] handle: {:p}", i, static_cast<void*>(context.framebuffers[i])));
                if (context.framebuffers[i] == VK_NULL_HANDLE) {
                    LOG_WARNING(std::format("Null framebuffer at index {}", i));
                }
            }
            destroyFramebuffers(context.device, context.framebuffers);
            context.framebuffers.clear();
            LOG_DEBUG("Framebuffers cleanup completed");
        }

        // Step 4: Command Buffers
        LOG_DEBUG("Step 4: Cleaning up command buffers");
        if (!context.commandBuffers.empty()) {
            for (const auto& cb : context.commandBuffers) {
                if (cb == VK_NULL_HANDLE) {
                    LOG_WARNING("Null command buffer detected");
                }
            }
            if (context.commandPool != VK_NULL_HANDLE && context.device != VK_NULL_HANDLE) {
                freeCommandBuffers(context.device, context.commandPool, context.commandBuffers);
            }
            context.commandBuffers.clear();
            LOG_DEBUG("Command buffers cleanup completed");
        }

        // Step 5: Swapchain Image Views
        LOG_DEBUG("Step 5: Cleaning up swapchain image views");
        if (!context.swapchainImageViews.empty()) {
            for (const auto& view : context.swapchainImageViews) {
                if (view == VK_NULL_HANDLE) {
                    LOG_WARNING("Null image view detected");
                }
            }
            destroyImageViews(context.device, context.swapchainImageViews);
            context.swapchainImageViews.clear();
            LOG_DEBUG("Swapchain image views cleanup completed");
        }

        // Step 6: Shader Modules
        LOG_DEBUG("Step 6: Cleaning up shader modules");
        try {
            if (!context.shaderModules.empty()) {
                destroyShaderModules(context.device, context.shaderModules);
                LOG_DEBUG("Shader modules cleanup completed");
                context.shaderModules.clear();
            }
        } catch (const std::exception& e) {
            cleanupErrors.push_back(std::format("Error in shader modules cleanup: {}", e.what()));
            LOG_ERROR(std::format("Error in shader modules cleanup: {}", e.what()));
            context.shaderModules.clear();
        }

        // Step 7: Image Available Semaphores
        LOG_DEBUG("Step 7: Cleaning up image available semaphores");
        try {
            if (!context.imageAvailableSemaphores.empty()) {
                destroySemaphores(context.device, context.imageAvailableSemaphores);
                LOG_DEBUG("Image available semaphores cleanup completed");
                context.imageAvailableSemaphores.clear();
            }
        } catch (const std::exception& e) {
            cleanupErrors.push_back(std::format("Error in image available semaphores cleanup: {}", e.what()));
            LOG_ERROR(std::format("Error in image available semaphores cleanup: {}", e.what()));
            context.imageAvailableSemaphores.clear();
        }

        // Step 8: Render Finished Semaphores
        LOG_DEBUG("Step 8: Cleaning up render finished semaphores");
        try {
            if (!context.renderFinishedSemaphores.empty()) {
                destroySemaphores(context.device, context.renderFinishedSemaphores);
                LOG_DEBUG("Render finished semaphores cleanup completed");
                context.renderFinishedSemaphores.clear();
            }
        } catch (const std::exception& e) {
            cleanupErrors.push_back(std::format("Error in render finished semaphores cleanup: {}", e.what()));
            LOG_ERROR(std::format("Error in render finished semaphores cleanup: {}", e.what()));
            context.renderFinishedSemaphores.clear();
        }

        // Step 9: In-Flight Fences
        LOG_DEBUG("Step 9: Cleaning up in-flight fences");
        try {
            if (!context.inFlightFences.empty()) {
                destroyFences(context.device, context.inFlightFences);
                LOG_DEBUG("In-flight fences cleanup completed");
                context.inFlightFences.clear();
            }
        } catch (const std::exception& e) {
            cleanupErrors.push_back(std::format("Error in in-flight fences cleanup: {}", e.what()));
            LOG_ERROR(std::format("Error in in-flight fences cleanup: {}", e.what()));
            context.inFlightFences.clear();
        }

        // Step 10: Uniform Buffers
        LOG_DEBUG("Step 10: Cleaning up uniform buffers");
        try {
            if (!context.uniformBuffers.empty()) {
                for (const auto& buffer : context.uniformBuffers) {
                    if (buffer != VK_NULL_HANDLE && std::ranges::find(context.resourceManager.getBuffers(), buffer) != context.resourceManager.getBuffers().end()) {
                        LOG_DEBUG(std::format("Removing uniform buffer {:p} from resource manager", static_cast<void*>(buffer)));
                        context.resourceManager.removeBuffer(buffer);
                    }
                }
                if (context.device != VK_NULL_HANDLE) {
                    destroyBuffers(context.device, context.uniformBuffers);
                    LOG_DEBUG("Uniform buffers cleanup completed");
                }
                context.uniformBuffers.clear();
            }
        } catch (const std::exception& e) {
            cleanupErrors.push_back(std::format("Error in uniform buffers cleanup: {}", e.what()));
            LOG_ERROR(std::format("Error in uniform buffers cleanup: {}", e.what()));
            context.uniformBuffers.clear();
        }

        // Step 11: Uniform Buffer Memories
        LOG_DEBUG("Step 11: Cleaning up uniform buffer memories");
        try {
            if (!context.uniformBufferMemories.empty()) {
                for (const auto& memory : context.uniformBufferMemories) {
                    if (memory != VK_NULL_HANDLE && std::ranges::find(context.resourceManager.getMemories(), memory) != context.resourceManager.getMemories().end()) {
                        LOG_DEBUG(std::format("Removing uniform buffer memory {:p} from resource manager", static_cast<void*>(memory)));
                        context.resourceManager.removeMemory(memory);
                    }
                }
                if (context.device != VK_NULL_HANDLE) {
                    freeDeviceMemories(context.device, context.uniformBufferMemories);
                    LOG_DEBUG("Uniform buffer memories cleanup completed");
                }
                context.uniformBufferMemories.clear();
            }
        } catch (const std::exception& e) {
            cleanupErrors.push_back(std::format("Error in uniform buffer memories cleanup: {}", e.what()));
            LOG_ERROR(std::format("Error in uniform buffer memories cleanup: {}", e.what()));
            context.uniformBufferMemories.clear();
        }

        // Step 13-50: Iterate disposables with lookup-based cleanup
        std::vector<Disposable> disposables = {
            {ResourceType::StorageImageView, reinterpret_cast<void*>(context.storageImageView), true, "Storage Image View"},
            {ResourceType::StorageImage, reinterpret_cast<void*>(context.storageImage), true, "Storage Image"},
            {ResourceType::StorageImageMemory, reinterpret_cast<void*>(context.storageImageMemory), true, "Storage Image Memory"},
            {ResourceType::RenderPass, reinterpret_cast<void*>(context.renderPass), false, "Render Pass"},
            {ResourceType::GraphicsPipeline, reinterpret_cast<void*>(context.graphicsPipeline), true, "Graphics Pipeline"},
            {ResourceType::ComputePipeline, reinterpret_cast<void*>(context.computePipeline), true, "Compute Pipeline"},
            {ResourceType::RayTracingPipeline, reinterpret_cast<void*>(context.rayTracingPipeline), true, "Ray Tracing Pipeline"},
            {ResourceType::GraphicsPipelineLayout, reinterpret_cast<void*>(context.graphicsPipelineLayout), true, "Graphics Pipeline Layout"},
            {ResourceType::RayTracingPipelineLayout, reinterpret_cast<void*>(context.rayTracingPipelineLayout), true, "Ray Tracing Pipeline Layout"},
            {ResourceType::ComputePipelineLayout, reinterpret_cast<void*>(context.computePipelineLayout), true, "Compute Pipeline Layout"},
            {ResourceType::GraphicsDescriptorSet, reinterpret_cast<void*>(context.graphicsDescriptorSet), false, "Graphics Descriptor Set"},
            {ResourceType::DescriptorSet, reinterpret_cast<void*>(context.descriptorSet), false, "Descriptor Set"},
            {ResourceType::GraphicsDescriptorPool, reinterpret_cast<void*>(context.graphicsDescriptorPool), true, "Graphics Descriptor Pool"},
            {ResourceType::DescriptorPool, reinterpret_cast<void*>(context.descriptorPool), true, "Descriptor Pool"},
            {ResourceType::GraphicsDescriptorSetLayout, reinterpret_cast<void*>(context.graphicsDescriptorSetLayout), true, "Graphics Descriptor Set Layout"},
            {ResourceType::RayTracingDescriptorSetLayout, reinterpret_cast<void*>(context.rayTracingDescriptorSetLayout), true, "Ray Tracing Descriptor Set Layout"},
            {ResourceType::Sampler, reinterpret_cast<void*>(context.sampler), false, "Sampler"},
            {ResourceType::TopLevelAS, reinterpret_cast<void*>(context.topLevelAS), true, "Top-Level Acceleration Structure"},
            {ResourceType::BottomLevelAS, reinterpret_cast<void*>(context.bottomLevelAS), true, "Bottom-Level Acceleration Structure"},
            {ResourceType::BottomLevelASBuffer, reinterpret_cast<void*>(context.bottomLevelASBuffer), true, "Bottom-Level AS Buffer"},
            {ResourceType::BottomLevelASMemory, reinterpret_cast<void*>(context.bottomLevelASMemory), true, "Bottom-Level AS Memory"},
            {ResourceType::TopLevelASBuffer, reinterpret_cast<void*>(context.topLevelASBuffer), true, "Top-Level AS Buffer"},
            {ResourceType::TopLevelASMemory, reinterpret_cast<void*>(context.topLevelASMemory), true, "Top-Level AS Memory"},
            {ResourceType::RaygenSbtBuffer, reinterpret_cast<void*>(context.raygenSbtBuffer), true, "Raygen SBT Buffer"},
            {ResourceType::RaygenSbtMemory, reinterpret_cast<void*>(context.raygenSbtMemory), true, "Raygen SBT Memory"},
            {ResourceType::MissSbtBuffer, reinterpret_cast<void*>(context.missSbtBuffer), true, "Miss SBT Buffer"},
            {ResourceType::MissSbtMemory, reinterpret_cast<void*>(context.missSbtMemory), true, "Miss SBT Memory"},
            {ResourceType::HitSbtBuffer, reinterpret_cast<void*>(context.hitSbtBuffer), true, "Hit SBT Buffer"},
            {ResourceType::HitSbtMemory, reinterpret_cast<void*>(context.hitSbtMemory), true, "Hit SBT Memory"},
            {ResourceType::VertexBuffer, reinterpret_cast<void*>(context.vertexBuffer), true, "Vertex Buffer"},
            {ResourceType::VertexBufferMemory, reinterpret_cast<void*>(context.vertexBufferMemory), true, "Vertex Buffer Memory"},
            {ResourceType::IndexBuffer, reinterpret_cast<void*>(context.indexBuffer), true, "Index Buffer"},
            {ResourceType::IndexBufferMemory, reinterpret_cast<void*>(context.indexBufferMemory), true, "Index Buffer Memory"},
            {ResourceType::ScratchBuffer, reinterpret_cast<void*>(context.scratchBuffer), true, "Scratch Buffer"},
            {ResourceType::ScratchBufferMemory, reinterpret_cast<void*>(context.scratchBufferMemory), true, "Scratch Buffer Memory"},
            {ResourceType::Swapchain, reinterpret_cast<void*>(context.swapchain), false, "Swapchain"},
            {ResourceType::CommandPool, reinterpret_cast<void*>(context.commandPool), true, "Command Pool"},
            {ResourceType::Device, reinterpret_cast<void*>(context.device), false, "Device"},
            {ResourceType::Surface, reinterpret_cast<void*>(context.surface), false, "Surface"},
            {ResourceType::DebugMessenger, reinterpret_cast<void*>(context.debugMessenger), false, "Debug Messenger"},
            {ResourceType::Instance, reinterpret_cast<void*>(context.instance), false, "Instance"}
        };

        // Iterate disposables with lookup-based cleanup
        for (size_t i = 0; i < disposables.size(); ++i) {
            const auto& disp = disposables[i];
            if (disp.handle == VK_NULL_HANDLE) continue;

            auto it = cleanupLookup.find(disp.type);
            if (it == cleanupLookup.end()) {
                LOG_WARNING(std::format("Unknown resource type: {}, skipping", static_cast<int>(disp.type)));
                continue;
            }

            try {
                LOG_DEBUG(std::format("Step {}: Cleaning up {}", i + 13, disp.name));
                it->second(context, disp.handle, disp.managedByResourceManager);
                LOG_DEBUG(std::format("{} cleanup completed", disp.name));

                // Nullify context fields
                switch (disp.type) {
                    case ResourceType::StorageImageView: context.storageImageView = VK_NULL_HANDLE; break;
                    case ResourceType::StorageImage: context.storageImage = VK_NULL_HANDLE; break;
                    case ResourceType::StorageImageMemory: context.storageImageMemory = VK_NULL_HANDLE; break;
                    case ResourceType::RenderPass: context.renderPass = VK_NULL_HANDLE; break;
                    case ResourceType::GraphicsPipeline: context.graphicsPipeline = VK_NULL_HANDLE; break;
                    case ResourceType::ComputePipeline: context.computePipeline = VK_NULL_HANDLE; break;
                    case ResourceType::RayTracingPipeline: context.rayTracingPipeline = VK_NULL_HANDLE; break;
                    case ResourceType::GraphicsPipelineLayout: context.graphicsPipelineLayout = VK_NULL_HANDLE; break;
                    case ResourceType::RayTracingPipelineLayout: context.rayTracingPipelineLayout = VK_NULL_HANDLE; break;
                    case ResourceType::ComputePipelineLayout: context.computePipelineLayout = VK_NULL_HANDLE; break;
                    case ResourceType::GraphicsDescriptorSet: context.graphicsDescriptorSet = VK_NULL_HANDLE; break;
                    case ResourceType::DescriptorSet: context.descriptorSet = VK_NULL_HANDLE; break;
                    case ResourceType::GraphicsDescriptorPool: context.graphicsDescriptorPool = VK_NULL_HANDLE; break;
                    case ResourceType::DescriptorPool: context.descriptorPool = VK_NULL_HANDLE; break;
                    case ResourceType::GraphicsDescriptorSetLayout: context.graphicsDescriptorSetLayout = VK_NULL_HANDLE; break;
                    case ResourceType::RayTracingDescriptorSetLayout: context.rayTracingDescriptorSetLayout = VK_NULL_HANDLE; break;
                    case ResourceType::Sampler: context.sampler = VK_NULL_HANDLE; break;
                    case ResourceType::TopLevelAS: context.topLevelAS = VK_NULL_HANDLE; break;
                    case ResourceType::BottomLevelAS: context.bottomLevelAS = VK_NULL_HANDLE; break;
                    case ResourceType::BottomLevelASBuffer: context.bottomLevelASBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::BottomLevelASMemory: context.bottomLevelASMemory = VK_NULL_HANDLE; break;
                    case ResourceType::TopLevelASBuffer: context.topLevelASBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::TopLevelASMemory: context.topLevelASMemory = VK_NULL_HANDLE; break;
                    case ResourceType::RaygenSbtBuffer: context.raygenSbtBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::RaygenSbtMemory: context.raygenSbtMemory = VK_NULL_HANDLE; break;
                    case ResourceType::MissSbtBuffer: context.missSbtBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::MissSbtMemory: context.missSbtMemory = VK_NULL_HANDLE; break;
                    case ResourceType::HitSbtBuffer: context.hitSbtBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::HitSbtMemory: context.hitSbtMemory = VK_NULL_HANDLE; break;
                    case ResourceType::VertexBuffer: context.vertexBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::VertexBufferMemory: context.vertexBufferMemory = VK_NULL_HANDLE; break;
                    case ResourceType::IndexBuffer: context.indexBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::IndexBufferMemory: context.indexBufferMemory = VK_NULL_HANDLE; break;
                    case ResourceType::ScratchBuffer: context.scratchBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::ScratchBufferMemory: context.scratchBufferMemory = VK_NULL_HANDLE; break;
                    case ResourceType::Swapchain: context.swapchain = VK_NULL_HANDLE; break;
                    case ResourceType::CommandPool: context.commandPool = VK_NULL_HANDLE; break;
                    case ResourceType::Device: context.device = VK_NULL_HANDLE; break;
                    case ResourceType::DebugMessenger: context.debugMessenger = VK_NULL_HANDLE; break;
                    case ResourceType::Instance: context.instance = VK_NULL_HANDLE; break;
                    case ResourceType::Surface: context.surface = VK_NULL_HANDLE; break;
                }
            } catch (const std::exception& e) {
                cleanupErrors.push_back(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                LOG_ERROR(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                // Nullify on error
                switch (disp.type) {
                    case ResourceType::StorageImageView: context.storageImageView = VK_NULL_HANDLE; break;
                    case ResourceType::StorageImage: context.storageImage = VK_NULL_HANDLE; break;
                    case ResourceType::StorageImageMemory: context.storageImageMemory = VK_NULL_HANDLE; break;
                    case ResourceType::RenderPass: context.renderPass = VK_NULL_HANDLE; break;
                    case ResourceType::GraphicsPipeline: context.graphicsPipeline = VK_NULL_HANDLE; break;
                    case ResourceType::ComputePipeline: context.computePipeline = VK_NULL_HANDLE; break;
                    case ResourceType::RayTracingPipeline: context.rayTracingPipeline = VK_NULL_HANDLE; break;
                    case ResourceType::GraphicsPipelineLayout: context.graphicsPipelineLayout = VK_NULL_HANDLE; break;
                    case ResourceType::RayTracingPipelineLayout: context.rayTracingPipelineLayout = VK_NULL_HANDLE; break;
                    case ResourceType::ComputePipelineLayout: context.computePipelineLayout = VK_NULL_HANDLE; break;
                    case ResourceType::GraphicsDescriptorSet: context.graphicsDescriptorSet = VK_NULL_HANDLE; break;
                    case ResourceType::DescriptorSet: context.descriptorSet = VK_NULL_HANDLE; break;
                    case ResourceType::GraphicsDescriptorPool: context.graphicsDescriptorPool = VK_NULL_HANDLE; break;
                    case ResourceType::DescriptorPool: context.descriptorPool = VK_NULL_HANDLE; break;
                    case ResourceType::GraphicsDescriptorSetLayout: context.graphicsDescriptorSetLayout = VK_NULL_HANDLE; break;
                    case ResourceType::RayTracingDescriptorSetLayout: context.rayTracingDescriptorSetLayout = VK_NULL_HANDLE; break;
                    case ResourceType::Sampler: context.sampler = VK_NULL_HANDLE; break;
                    case ResourceType::TopLevelAS: context.topLevelAS = VK_NULL_HANDLE; break;
                    case ResourceType::BottomLevelAS: context.bottomLevelAS = VK_NULL_HANDLE; break;
                    case ResourceType::BottomLevelASBuffer: context.bottomLevelASBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::BottomLevelASMemory: context.bottomLevelASMemory = VK_NULL_HANDLE; break;
                    case ResourceType::TopLevelASBuffer: context.topLevelASBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::TopLevelASMemory: context.topLevelASMemory = VK_NULL_HANDLE; break;
                    case ResourceType::RaygenSbtBuffer: context.raygenSbtBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::RaygenSbtMemory: context.raygenSbtMemory = VK_NULL_HANDLE; break;
                    case ResourceType::MissSbtBuffer: context.missSbtBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::MissSbtMemory: context.missSbtMemory = VK_NULL_HANDLE; break;
                    case ResourceType::HitSbtBuffer: context.hitSbtBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::HitSbtMemory: context.hitSbtMemory = VK_NULL_HANDLE; break;
                    case ResourceType::VertexBuffer: context.vertexBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::VertexBufferMemory: context.vertexBufferMemory = VK_NULL_HANDLE; break;
                    case ResourceType::IndexBuffer: context.indexBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::IndexBufferMemory: context.indexBufferMemory = VK_NULL_HANDLE; break;
                    case ResourceType::ScratchBuffer: context.scratchBuffer = VK_NULL_HANDLE; break;
                    case ResourceType::ScratchBufferMemory: context.scratchBufferMemory = VK_NULL_HANDLE; break;
                    case ResourceType::Swapchain: context.swapchain = VK_NULL_HANDLE; break;
                    case ResourceType::CommandPool: context.commandPool = VK_NULL_HANDLE; break;
                    case ResourceType::Device: context.device = VK_NULL_HANDLE; break;
                    case ResourceType::DebugMessenger: context.debugMessenger = VK_NULL_HANDLE; break;
                    case ResourceType::Instance: context.instance = VK_NULL_HANDLE; break;
                    case ResourceType::Surface: context.surface = VK_NULL_HANDLE; break;
                }
            }
        }

        // Step 51: Nullify remaining fields
        LOG_DEBUG("Step 51: Nullifying remaining context fields");
        context.physicalDevice = VK_NULL_HANDLE;
        context.graphicsQueue = VK_NULL_HANDLE;
        context.presentQueue = VK_NULL_HANDLE;
        context.swapchainImageFormat = VK_FORMAT_UNDEFINED;
        context.swapchainExtent = {0, 0};
        context.graphicsQueueFamilyIndex = UINT32_MAX;
        context.presentQueueFamilyIndex = UINT32_MAX;
        context.enableRayTracing = false;
        LOG_INFO("Remaining context fields nullified");

        // Step 52: Final nullification and logging
        LOG_DEBUG("Step 52: Final context cleanup");
        if (!cleanupErrors.empty()) {
            LOG_ERROR("Cleanup completed with errors:");
            for (const auto& error : cleanupErrors) {
                LOG_ERROR(error);
            }
        } else {
            LOG_INFO("Vulkan context cleanup completed successfully - All steps processed linearly with passthrough");
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::format("Unexpected error in cleanupVulkanContext: {}", e.what()));
        // Minimal cleanup to ensure safe exit
        context.device = VK_NULL_HANDLE;
        context.instance = VK_NULL_HANDLE;
        context.surface = VK_NULL_HANDLE;
        context.swapchain = VK_NULL_HANDLE;
        context.uniformBuffers.clear();
        context.uniformBufferMemories.clear();
        context.shaderModules.clear();
        context.imageAvailableSemaphores.clear();
        context.renderFinishedSemaphores.clear();
        context.inFlightFences.clear();
        context.framebuffers.clear();
        context.commandBuffers.clear();
        context.swapchainImageViews.clear();
        context.vertexBuffer = VK_NULL_HANDLE;
        context.vertexBufferMemory = VK_NULL_HANDLE;
        context.indexBuffer = VK_NULL_HANDLE;
        context.indexBufferMemory = VK_NULL_HANDLE;
        context.scratchBuffer = VK_NULL_HANDLE;
        context.scratchBufferMemory = VK_NULL_HANDLE;
    }
}

} // namespace Dispose