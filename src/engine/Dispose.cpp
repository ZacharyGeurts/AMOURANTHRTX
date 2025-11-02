// AMOURANTH RTX Engine, October 2025 - Implementations for centralized resource disposal.
// Zachary Geurts 2025

#include "engine/Dispose.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
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

// =============================================================================
// ONLY NON-INLINE DEFINITIONS BELOW
// updateDescriptorSets and cleanupVulkanContext are inline in Dispose.hpp → REMOVED
// =============================================================================

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

enum class DisposeStage {
    IdleWait,
    ResourceManager,
    Framebuffers,
    CommandBuffers,
    SwapchainViews,
    ShaderModules,
    Semaphores,
    Fences,
    UniformBuffers,
    UniformMemories,
    GraphicsResources,
    ComputeResources,
    RayTracingResources,
    BufferResources,
    MemoryResources,
    CoreVulkan,
    FinalNullify
};

struct Disposable {
    ResourceType type;
    void* handle;
    bool managedByResourceManager = false;
    std::string name;
    DisposeStage stage = DisposeStage::CoreVulkan;
};

using CleanupFn = std::function<void(Vulkan::Context&, void*, bool)>;
static const std::unordered_map<ResourceType, CleanupFn> cleanupLookup = {
    {ResourceType::StorageImageView, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkImageView handle = static_cast<VkImageView>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null storage image view cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& imageViews = ctx.resourceManager.getImageViews();
            auto it = std::ranges::find(imageViews, handle);
            if (it != imageViews.end()) {
                LOG_DEBUG(std::format("Removing storage image view {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeImageView(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Storage image view {:p} not found in resource manager (possibly externally managed or already removed)", static_cast<void*>(handle)));
            }
        }
        destroySingleImageView(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed storage image view {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::StorageImage, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkImage handle = static_cast<VkImage>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null storage image cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& images = ctx.resourceManager.getImages();
            auto it = std::ranges::find(images, handle);
            if (it != images.end()) {
                LOG_DEBUG(std::format("Removing storage image {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeImage(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Storage image {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySingleImage(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed storage image {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::StorageImageMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null storage image memory cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            auto it = std::ranges::find(memories, handle);
            if (it != memories.end()) {
                LOG_DEBUG(std::format("Removing storage image memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Storage image memory {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
        LOG_DEBUG(std::format("Freed storage image memory {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::RenderPass, [](Vulkan::Context& ctx, void* h, bool) {
        VkRenderPass handle = static_cast<VkRenderPass>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null render pass cleanup");
            return;
        }
        destroySingleRenderPass(ctx.device, handle);
        LOG_INFO(std::format("Destroyed render pass {:p}", static_cast<void*>(handle)));
    }},
    {ResourceType::GraphicsPipeline, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkPipeline handle = static_cast<VkPipeline>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null graphics pipeline cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& pipelines = ctx.resourceManager.getPipelines();
            auto it = std::ranges::find(pipelines, handle);
            if (it != pipelines.end()) {
                LOG_DEBUG(std::format("Removing graphics pipeline {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removePipeline(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Graphics pipeline {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySinglePipeline(ctx.device, handle);
        LOG_INFO(std::format("Destroyed graphics pipeline {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::ComputePipeline, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkPipeline handle = static_cast<VkPipeline>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null compute pipeline cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& pipelines = ctx.resourceManager.getPipelines();
            auto it = std::ranges::find(pipelines, handle);
            if (it != pipelines.end()) {
                LOG_DEBUG(std::format("Removing compute pipeline {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removePipeline(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Compute pipeline {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySinglePipeline(ctx.device, handle);
        LOG_INFO(std::format("Destroyed compute pipeline {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::RayTracingPipeline, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkPipeline handle = static_cast<VkPipeline>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null ray-tracing pipeline cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& pipelines = ctx.resourceManager.getPipelines();
            auto it = std::ranges::find(pipelines, handle);
            if (it != pipelines.end()) {
                LOG_DEBUG(std::format("Removing ray-tracing pipeline {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removePipeline(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Ray-tracing pipeline {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySinglePipeline(ctx.device, handle);
        LOG_INFO(std::format("Destroyed ray-tracing pipeline {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::GraphicsPipelineLayout, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkPipelineLayout handle = static_cast<VkPipelineLayout>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null graphics pipeline layout cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& layouts = ctx.resourceManager.getPipelineLayouts();
            auto it = std::ranges::find(layouts, handle);
            if (it != layouts.end()) {
                LOG_DEBUG(std::format("Removing graphics pipeline layout {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removePipelineLayout(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Graphics pipeline layout {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySinglePipelineLayout(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed graphics pipeline layout {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::RayTracingPipelineLayout, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkPipelineLayout handle = static_cast<VkPipelineLayout>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null ray-tracing pipeline layout cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& layouts = ctx.resourceManager.getPipelineLayouts();
            auto it = std::ranges::find(layouts, handle);
            if (it != layouts.end()) {
                LOG_DEBUG(std::format("Removing ray-tracing pipeline layout {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removePipelineLayout(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Ray-tracing pipeline layout {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySinglePipelineLayout(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed ray-tracing pipeline layout {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::ComputePipelineLayout, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkPipelineLayout handle = static_cast<VkPipelineLayout>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null compute pipeline layout cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& layouts = ctx.resourceManager.getPipelineLayouts();
            auto it = std::ranges::find(layouts, handle);
            if (it != layouts.end()) {
                LOG_DEBUG(std::format("Removing compute pipeline layout {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removePipelineLayout(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Compute pipeline layout {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySinglePipelineLayout(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed compute pipeline layout {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::GraphicsDescriptorSet, [](Vulkan::Context& ctx, void* h, bool) {
        VkDescriptorSet handle = static_cast<VkDescriptorSet>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null graphics descriptor set cleanup");
            return;
        }
        if (ctx.graphicsDescriptorPool != VK_NULL_HANDLE) {
            freeSingleDescriptorSet(ctx.device, ctx.graphicsDescriptorPool, handle);
            LOG_DEBUG(std::format("Freed graphics descriptor set {:p}", static_cast<void*>(handle)));
        } else {
            LOG_WARNING("Cannot free graphics descriptor set: pool is null");
        }
    }},
    {ResourceType::DescriptorSet, [](Vulkan::Context& ctx, void* h, bool) {
        VkDescriptorSet handle = static_cast<VkDescriptorSet>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null descriptor set cleanup");
            return;
        }
        if (ctx.descriptorPool != VK_NULL_HANDLE) {
            freeSingleDescriptorSet(ctx.device, ctx.descriptorPool, handle);
            LOG_DEBUG(std::format("Freed descriptor set {:p}", static_cast<void*>(handle)));
        } else {
            LOG_WARNING("Cannot free descriptor set: pool is null");
        }
    }},
    {ResourceType::GraphicsDescriptorPool, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDescriptorPool handle = static_cast<VkDescriptorPool>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null graphics descriptor pool cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& pools = ctx.resourceManager.getDescriptorPools();
            auto it = std::ranges::find(pools, handle);
            if (it != pools.end()) {
                LOG_DEBUG(std::format("Removing graphics descriptor pool {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeDescriptorPool(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Graphics descriptor pool {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySingleDescriptorPool(ctx.device, handle);
        LOG_INFO(std::format("Destroyed graphics descriptor pool {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::DescriptorPool, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDescriptorPool handle = static_cast<VkDescriptorPool>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null descriptor pool cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& pools = ctx.resourceManager.getDescriptorPools();
            auto it = std::ranges::find(pools, handle);
            if (it != pools.end()) {
                LOG_DEBUG(std::format("Removing descriptor pool {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeDescriptorPool(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Descriptor pool {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySingleDescriptorPool(ctx.device, handle);
        LOG_INFO(std::format("Destroyed descriptor pool {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::GraphicsDescriptorSetLayout, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDescriptorSetLayout handle = static_cast<VkDescriptorSetLayout>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null graphics descriptor set layout cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& layouts = ctx.resourceManager.getDescriptorSetLayouts();
            auto it = std::ranges::find(layouts, handle);
            if (it != layouts.end()) {
                LOG_DEBUG(std::format("Removing graphics descriptor set layout {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeDescriptorSetLayout(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Graphics descriptor set layout {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySingleDescriptorSetLayout(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed graphics descriptor set layout {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::RayTracingDescriptorSetLayout, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDescriptorSetLayout handle = static_cast<VkDescriptorSetLayout>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null ray-tracing descriptor set layout cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& layouts = ctx.resourceManager.getDescriptorSetLayouts();
            auto it = std::ranges::find(layouts, handle);
            if (it != layouts.end()) {
                LOG_DEBUG(std::format("Removing ray-tracing descriptor set layout {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeDescriptorSetLayout(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Ray-tracing descriptor set layout {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySingleDescriptorSetLayout(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed ray-tracing descriptor set layout {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::Sampler, [](Vulkan::Context& ctx, void* h, bool) {
        VkSampler handle = static_cast<VkSampler>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null sampler cleanup");
            return;
        }
        destroySingleSampler(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed sampler {:p}", static_cast<void*>(handle)));
    }},
    {ResourceType::TopLevelAS, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkAccelerationStructureKHR handle = static_cast<VkAccelerationStructureKHR>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null top-level AS cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& structures = ctx.resourceManager.getAccelerationStructures();
            auto it = std::ranges::find(structures, handle);
            if (it != structures.end()) {
                LOG_DEBUG(std::format("Removing top-level AS {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeAccelerationStructure(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Top-level AS {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySingleAccelerationStructure(ctx.device, handle);
        LOG_INFO(std::format("Destroyed top-level AS {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::BottomLevelAS, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkAccelerationStructureKHR handle = static_cast<VkAccelerationStructureKHR>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null bottom-level AS cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& structures = ctx.resourceManager.getAccelerationStructures();
            auto it = std::ranges::find(structures, handle);
            if (it != structures.end()) {
                LOG_DEBUG(std::format("Removing bottom-level AS {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeAccelerationStructure(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Bottom-level AS {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySingleAccelerationStructure(ctx.device, handle);
        LOG_INFO(std::format("Destroyed bottom-level AS {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::BottomLevelASBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null bottom-level AS buffer cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            auto it = std::ranges::find(buffers, handle);
            if (it != buffers.end()) {
                LOG_DEBUG(std::format("Removing bottom-level AS buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Bottom-level AS buffer {:p} not found in resource manager (possibly managed by BufferMgr)", static_cast<void*>(handle)));
            }
        }
        destroySingleBuffer(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed bottom-level AS buffer {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::BottomLevelASMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null bottom-level AS memory cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            auto it = std::ranges::find(memories, handle);
            if (it != memories.end()) {
                LOG_DEBUG(std::format("Removing bottom-level AS memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Bottom-level AS memory {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
        LOG_DEBUG(std::format("Freed bottom-level AS memory {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::TopLevelASBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null top-level AS buffer cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            auto it = std::ranges::find(buffers, handle);
            if (it != buffers.end()) {
                LOG_DEBUG(std::format("Removing top-level AS buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Top-level AS buffer {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySingleBuffer(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed top-level AS buffer {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::TopLevelASMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null top-level AS memory cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            auto it = std::ranges::find(memories, handle);
            if (it != memories.end()) {
                LOG_DEBUG(std::format("Removing top-level AS memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Top-level AS memory {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
        LOG_DEBUG(std::format("Freed top-level AS memory {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::RaygenSbtBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null raygen SBT buffer cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            auto it = std::ranges::find(buffers, handle);
            if (it != buffers.end()) {
                LOG_DEBUG(std::format("Removing raygen SBT buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Raygen SBT buffer {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySingleBuffer(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed raygen SBT buffer {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::RaygenSbtMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null raygen SBT memory cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            auto it = std::ranges::find(memories, handle);
            if (it != memories.end()) {
                LOG_DEBUG(std::format("Removing raygen SBT memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Raygen SBT memory {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
        LOG_DEBUG(std::format("Freed raygen SBT memory {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::MissSbtBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null miss SBT buffer cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            auto it = std::ranges::find(buffers, handle);
            if (it != buffers.end()) {
                LOG_DEBUG(std::format("Removing miss SBT buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Miss SBT buffer {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySingleBuffer(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed miss SBT buffer {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::MissSbtMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null miss SBT memory cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            auto it = std::ranges::find(memories, handle);
            if (it != memories.end()) {
                LOG_DEBUG(std::format("Removing miss SBT memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Miss SBT memory {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
        LOG_DEBUG(std::format("Freed miss SBT memory {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::HitSbtBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null hit SBT buffer cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            auto it = std::ranges::find(buffers, handle);
            if (it != buffers.end()) {
                LOG_DEBUG(std::format("Removing hit SBT buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Hit SBT buffer {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySingleBuffer(ctx.device, handle);
        LOG_DEBUG(std::format("Destroyed hit SBT buffer {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::HitSbtMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null hit SBT memory cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            auto it = std::ranges::find(memories, handle);
            if (it != memories.end()) {
                LOG_DEBUG(std::format("Removing hit SBT memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Hit SBT memory {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
        LOG_DEBUG(std::format("Freed hit SBT memory {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::VertexBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null vertex buffer cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            auto it = std::ranges::find(buffers, handle);
            if (it != buffers.end()) {
                LOG_DEBUG(std::format("Removing vertex buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Vertex buffer {:p} not found in resource manager (possibly managed by BufferMgr)", static_cast<void*>(handle)));
            }
        }
        destroySingleBuffer(ctx.device, handle);
        LOG_INFO(std::format("Destroyed vertex buffer {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::VertexBufferMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null vertex buffer memory cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            auto it = std::ranges::find(memories, handle);
            if (it != memories.end()) {
                LOG_DEBUG(std::format("Removing vertex buffer memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Vertex buffer memory {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
        LOG_DEBUG(std::format("Freed vertex buffer memory {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::IndexBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null index buffer cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            auto it = std::ranges::find(buffers, handle);
            if (it != buffers.end()) {
                LOG_DEBUG(std::format("Removing index buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Index buffer {:p} not found in resource manager (possibly managed by BufferMgr)", static_cast<void*>(handle)));
            }
        }
        destroySingleBuffer(ctx.device, handle);
        LOG_INFO(std::format("Destroyed index buffer {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::IndexBufferMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null index buffer memory cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            auto it = std::ranges::find(memories, handle);
            if (it != memories.end()) {
                LOG_DEBUG(std::format("Removing index buffer memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Index buffer memory {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
        LOG_DEBUG(std::format("Freed index buffer memory {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::ScratchBuffer, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkBuffer handle = static_cast<VkBuffer>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null scratch buffer cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& buffers = ctx.resourceManager.getBuffers();
            auto it = std::ranges::find(buffers, handle);
            if (it != buffers.end()) {
                LOG_DEBUG(std::format("Removing scratch buffer {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeBuffer(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Scratch buffer {:p} not found in resource manager (likely managed by VulkanBufferManager)", static_cast<void*>(handle)));
            }
        }
        destroySingleBuffer(ctx.device, handle);
        LOG_INFO(std::format("Destroyed scratch buffer {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::ScratchBufferMemory, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkDeviceMemory handle = static_cast<VkDeviceMemory>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null scratch buffer memory cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& memories = ctx.resourceManager.getMemories();
            auto it = std::ranges::find(memories, handle);
            if (it != memories.end()) {
                LOG_DEBUG(std::format("Removing scratch buffer memory {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeMemory(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Scratch buffer memory {:p} not found in resource manager (likely managed by VulkanBufferManager)", static_cast<void*>(handle)));
            }
        }
        freeSingleDeviceMemory(ctx.device, handle);
        LOG_DEBUG(std::format("Freed scratch buffer memory {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::Swapchain, [](Vulkan::Context& ctx, void* h, bool) {
        VkSwapchainKHR handle = static_cast<VkSwapchainKHR>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null swapchain cleanup");
            return;
        }
        if (ctx.device != VK_NULL_HANDLE) {
            destroySingleSwapchain(ctx.device, handle);
            LOG_INFO(std::format("Destroyed swapchain {:p}", static_cast<void*>(handle)));
        } else {
            LOG_WARNING("Cannot destroy swapchain: device is null");
        }
    }},
    {ResourceType::CommandPool, [](Vulkan::Context& ctx, void* h, bool managed) {
        VkCommandPool handle = static_cast<VkCommandPool>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null command pool cleanup");
            return;
        }
        bool wasManaged = false;
        if (managed && ctx.device != VK_NULL_HANDLE) {
            auto& pools = ctx.resourceManager.getCommandPools();
            auto it = std::ranges::find(pools, handle);
            if (it != pools.end()) {
                LOG_DEBUG(std::format("Removing command pool {:p} from resource manager", static_cast<void*>(handle)));
                ctx.resourceManager.removeCommandPool(handle);
                wasManaged = true;
            } else {
                LOG_DEBUG(std::format("Command pool {:p} not found in resource manager", static_cast<void*>(handle)));
            }
        }
        destroySingleCommandPool(ctx.device, handle);
        LOG_INFO(std::format("Destroyed command pool {:p} (managed: {})", static_cast<void*>(handle), wasManaged));
    }},
    {ResourceType::Device, [](Vulkan::Context&, void* h, bool) {
        VkDevice handle = static_cast<VkDevice>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null device cleanup");
            return;
        }
        destroyDevice(handle);
        LOG_INFO(std::format("Destroyed device {:p}", static_cast<void*>(handle)));
    }},
    {ResourceType::DebugMessenger, [](Vulkan::Context& ctx, void* h, bool) {
        VkDebugUtilsMessengerEXT handle = static_cast<VkDebugUtilsMessengerEXT>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null debug messenger cleanup");
            return;
        }
        destroyDebugUtilsMessengerEXT(ctx.instance, handle);
        LOG_DEBUG(std::format("Destroyed debug messenger {:p}", static_cast<void*>(handle)));
    }},
    {ResourceType::Instance, [](Vulkan::Context&, void* h, bool) {
        VkInstance handle = static_cast<VkInstance>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null instance cleanup");
            return;
        }
        destroyInstance(handle);
        LOG_INFO(std::format("Destroyed instance {:p}", static_cast<void*>(handle)));
    }},
    {ResourceType::Surface, [](Vulkan::Context& ctx, void* h, bool) {
        VkSurfaceKHR handle = static_cast<VkSurfaceKHR>(h);
        if (handle == VK_NULL_HANDLE) {
            LOG_DEBUG("Skipping null surface cleanup");
            return;
        }
        destroySurfaceKHR(ctx.instance, handle);
        LOG_DEBUG(std::format("Destroyed surface {:p}", static_cast<void*>(handle)));
    }}
};

} // namespace Dispose