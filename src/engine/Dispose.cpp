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

enum class DisposeStage {
    IdleWait,          // Steps 0-1.5: Waiting for idles and fences
    ResourceManager,   // Step 2: Resource manager cleanup
    Framebuffers,      // Step 3: Framebuffers
    CommandBuffers,    // Step 4: Command buffers
    SwapchainViews,    // Step 5: Swapchain image views
    ShaderModules,     // Step 6: Shader modules
    Semaphores,        // Step 7-8: Semaphores
    Fences,            // Step 9: Fences
    UniformBuffers,    // Step 10: Uniform buffers
    UniformMemories,   // Step 11: Uniform memories
    GraphicsResources, // Stage for graphics pipelines, layouts, etc.
    ComputeResources,  // Stage for compute pipelines, layouts, etc.
    RayTracingResources, // Stage for RT pipelines, AS, SBTs, etc.
    BufferResources,   // Stage for vertex/index/scratch buffers
    MemoryResources,   // Stage for buffer memories (grouped)
    CoreVulkan,        // Step for swapchain, command pool, device, etc.
    FinalNullify       // Step 51-52: Nullify and report
};

struct Disposable {
    ResourceType type;
    void* handle;
    bool managedByResourceManager = false;
    std::string name;
    DisposeStage stage = DisposeStage::CoreVulkan; // Default stage
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

void updateDescriptorSets(Vulkan::Context& context) noexcept {
    try {
        LOG_INFO("Starting descriptor set update");  // Upgraded to INFO for visibility

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

                VkBuffer buffer = context.uniformBuffers[i];
                auto it = std::ranges::find(managedBuffers, buffer);
                if (it == managedBuffers.end()) {
                    LOG_ERROR(std::format("Uniform buffer[{}] ({:p}) is not managed by VulkanResourceManager", i, static_cast<void*>(buffer)));
                    failedBuffers++;
                    continue;
                }

                bufferInfos[i] = VkDescriptorBufferInfo{
                    .buffer = buffer,
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
        LOG_INFO("=== Starting Vulkan context cleanup - Staged linear passthrough mode enabled ===");
        std::vector<std::string> cleanupErrors;
        uint32_t stageCount = 0;

        // Stage: IdleWait (Steps 0-1.5: Wait for fences, queues, and device)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "IdleWait"));
            LOG_DEBUG("Waiting for all in-flight fences");
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

            LOG_DEBUG("Waiting for queues to idle");
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

            LOG_DEBUG("Waiting for device to idle");
            if (context.device != VK_NULL_HANDLE) {
                VkResult deviceRes = vkDeviceWaitIdle(context.device);
                if (deviceRes != VK_SUCCESS) {
                    cleanupErrors.push_back(std::format("vkDeviceWaitIdle failed (code: {})", static_cast<int>(deviceRes)));
                    LOG_WARNING(std::format("vkDeviceWaitIdle failed (code: {}), proceeding with cleanup", static_cast<int>(deviceRes)));
                } else {
                    LOG_DEBUG("Device idle");
                }
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "IdleWait"));
        }

        // Stage: ResourceManager (Step 2: Resource manager cleanup)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "ResourceManager"));
            LOG_DEBUG("Checking VulkanResourceManager state before cleanup");
            try {
                bool hasResources = false;
                size_t bufferCount = context.resourceManager.getBuffers().size();
                size_t memoryCount = context.resourceManager.getMemories().size();
                size_t imageCount = context.resourceManager.getImages().size();
                size_t imageViewCount = context.resourceManager.getImageViews().size();
                size_t pipelineCount = context.resourceManager.getPipelines().size();
                size_t pipelineLayoutCount = context.resourceManager.getPipelineLayouts().size();
                size_t descriptorSetLayoutCount = context.resourceManager.getDescriptorSetLayouts().size();
                size_t descriptorPoolCount = context.resourceManager.getDescriptorPools().size();
                size_t accelerationStructureCount = context.resourceManager.getAccelerationStructures().size();
                size_t commandPoolCount = context.resourceManager.getCommandPools().size();

                if (bufferCount > 0 || memoryCount > 0 || imageCount > 0 || imageViewCount > 0 ||
                    pipelineCount > 0 || pipelineLayoutCount > 0 || descriptorSetLayoutCount > 0 ||
                    descriptorPoolCount > 0 || accelerationStructureCount > 0 || commandPoolCount > 0) {
                    hasResources = true;
                    LOG_INFO(std::format("Resource manager contains resources: buffers={}, memories={}, images={}, imageViews={}, pipelines={}, pipelineLayouts={}, descriptorSetLayouts={}, descriptorPools={}, accelerationStructures={}, commandPools={}",
                        bufferCount, memoryCount, imageCount, imageViewCount, pipelineCount, pipelineLayoutCount,
                        descriptorSetLayoutCount, descriptorPoolCount, accelerationStructureCount, commandPoolCount));
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
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "ResourceManager"));
        }

        // Stage: Framebuffers (Step 3)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "Framebuffers"));
            LOG_DEBUG("Cleaning up framebuffers");
            if (!context.framebuffers.empty()) {
                size_t nullCount = 0;
                for (size_t i = 0; i < context.framebuffers.size(); ++i) {
                    if (context.framebuffers[i] == VK_NULL_HANDLE) {
                        nullCount++;
                        LOG_WARNING(std::format("Null framebuffer at index {}", i));
                    } else {
                        LOG_DEBUG(std::format("Framebuffer[{}] handle: {:p}", i, static_cast<void*>(context.framebuffers[i])));
                    }
                }
                if (nullCount > 0) {
                    LOG_WARNING(std::format("{} null framebuffers detected in list of {}", nullCount, context.framebuffers.size()));
                }
                destroyFramebuffers(context.device, context.framebuffers);
                context.framebuffers.clear();
                LOG_DEBUG("Framebuffers cleanup completed");
            } else {
                LOG_DEBUG("No framebuffers to clean up");
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "Framebuffers"));
        }

        // Stage: CommandBuffers (Step 4)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "CommandBuffers"));
            LOG_DEBUG("Cleaning up command buffers");
            if (!context.commandBuffers.empty()) {
                size_t nullCount = 0;
                for (const auto& cb : context.commandBuffers) {
                    if (cb == VK_NULL_HANDLE) {
                        nullCount++;
                        LOG_WARNING("Null command buffer detected");
                    }
                }
                if (nullCount > 0) {
                    LOG_WARNING(std::format("{} null command buffers detected", nullCount));
                }
                if (context.commandPool != VK_NULL_HANDLE && context.device != VK_NULL_HANDLE) {
                    freeCommandBuffers(context.device, context.commandPool, context.commandBuffers);
                } else {
                    LOG_WARNING("Cannot free command buffers: pool or device is null");
                }
                context.commandBuffers.clear();
                LOG_DEBUG("Command buffers cleanup completed");
            } else {
                LOG_DEBUG("No command buffers to clean up");
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "CommandBuffers"));
        }

        // Stage: SwapchainViews (Step 5)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "SwapchainViews"));
            LOG_DEBUG("Cleaning up swapchain image views");
            if (!context.swapchainImageViews.empty()) {
                size_t nullCount = 0;
                for (const auto& view : context.swapchainImageViews) {
                    if (view == VK_NULL_HANDLE) {
                        nullCount++;
                        LOG_WARNING("Null image view detected");
                    }
                }
                if (nullCount > 0) {
                    LOG_WARNING(std::format("{} null swapchain image views detected", nullCount));
                }
                destroyImageViews(context.device, context.swapchainImageViews);
                context.swapchainImageViews.clear();
                LOG_DEBUG("Swapchain image views cleanup completed");
            } else {
                LOG_DEBUG("No swapchain image views to clean up");
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "SwapchainViews"));
        }

        // Stage: ShaderModules (Step 6)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "ShaderModules"));
            LOG_DEBUG("Cleaning up shader modules");
            try {
                if (!context.shaderModules.empty()) {
                    destroyShaderModules(context.device, context.shaderModules);
                    LOG_DEBUG(std::format("Destroyed {} shader modules", context.shaderModules.size()));
                    context.shaderModules.clear();
                } else {
                    LOG_DEBUG("No shader modules to clean up");
                }
            } catch (const std::exception& e) {
                cleanupErrors.push_back(std::format("Error in shader modules cleanup: {}", e.what()));
                LOG_ERROR(std::format("Error in shader modules cleanup: {}", e.what()));
                context.shaderModules.clear();
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "ShaderModules"));
        }

        // Stage: Semaphores (Steps 7-8)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "Semaphores"));
            LOG_DEBUG("Cleaning up image available semaphores");
            try {
                if (!context.imageAvailableSemaphores.empty()) {
                    destroySemaphores(context.device, context.imageAvailableSemaphores);
                    LOG_DEBUG(std::format("Destroyed {} image available semaphores", context.imageAvailableSemaphores.size()));
                    context.imageAvailableSemaphores.clear();
                } else {
                    LOG_DEBUG("No image available semaphores to clean up");
                }
            } catch (const std::exception& e) {
                cleanupErrors.push_back(std::format("Error in image available semaphores cleanup: {}", e.what()));
                LOG_ERROR(std::format("Error in image available semaphores cleanup: {}", e.what()));
                context.imageAvailableSemaphores.clear();
            }

            LOG_DEBUG("Cleaning up render finished semaphores");
            try {
                if (!context.renderFinishedSemaphores.empty()) {
                    destroySemaphores(context.device, context.renderFinishedSemaphores);
                    LOG_DEBUG(std::format("Destroyed {} render finished semaphores", context.renderFinishedSemaphores.size()));
                    context.renderFinishedSemaphores.clear();
                } else {
                    LOG_DEBUG("No render finished semaphores to clean up");
                }
            } catch (const std::exception& e) {
                cleanupErrors.push_back(std::format("Error in render finished semaphores cleanup: {}", e.what()));
                LOG_ERROR(std::format("Error in render finished semaphores cleanup: {}", e.what()));
                context.renderFinishedSemaphores.clear();
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "Semaphores"));
        }

        // Stage: Fences (Step 9)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "Fences"));
            LOG_DEBUG("Cleaning up in-flight fences");
            try {
                if (!context.inFlightFences.empty()) {
                    destroyFences(context.device, context.inFlightFences);
                    LOG_DEBUG(std::format("Destroyed {} in-flight fences", context.inFlightFences.size()));
                    context.inFlightFences.clear();
                } else {
                    LOG_DEBUG("No in-flight fences to clean up");
                }
            } catch (const std::exception& e) {
                cleanupErrors.push_back(std::format("Error in in-flight fences cleanup: {}", e.what()));
                LOG_ERROR(std::format("Error in in-flight fences cleanup: {}", e.what()));
                context.inFlightFences.clear();
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "Fences"));
        }

        // Stage: UniformBuffers (Step 10)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "UniformBuffers"));
            LOG_DEBUG("Cleaning up uniform buffers");
            try {
                if (!context.uniformBuffers.empty()) {
                    size_t removedFromManager = 0;
                    for (const auto& buffer : context.uniformBuffers) {
                        if (buffer != VK_NULL_HANDLE) {
                            auto& managedBuffers = context.resourceManager.getBuffers();
                            auto it = std::ranges::find(managedBuffers, buffer);
                            if (it != managedBuffers.end()) {
                                LOG_DEBUG(std::format("Removing uniform buffer {:p} from resource manager", static_cast<void*>(buffer)));
                                context.resourceManager.removeBuffer(buffer);
                                removedFromManager++;
                            } else {
                                LOG_DEBUG(std::format("Uniform buffer {:p} not found in resource manager", static_cast<void*>(buffer)));
                            }
                        }
                    }
                    LOG_DEBUG(std::format("Removed {} uniform buffers from resource manager", removedFromManager));
                    if (context.device != VK_NULL_HANDLE) {
                        destroyBuffers(context.device, context.uniformBuffers);
                        LOG_DEBUG(std::format("Destroyed {} uniform buffers", context.uniformBuffers.size()));
                    }
                    context.uniformBuffers.clear();
                } else {
                    LOG_DEBUG("No uniform buffers to clean up");
                }
            } catch (const std::exception& e) {
                cleanupErrors.push_back(std::format("Error in uniform buffers cleanup: {}", e.what()));
                LOG_ERROR(std::format("Error in uniform buffers cleanup: {}", e.what()));
                context.uniformBuffers.clear();
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "UniformBuffers"));
        }

        // Stage: UniformMemories (Step 11)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "UniformMemories"));
            LOG_DEBUG("Cleaning up uniform buffer memories");
            try {
                if (!context.uniformBufferMemories.empty()) {
                    size_t removedFromManager = 0;
                    for (const auto& memory : context.uniformBufferMemories) {
                        if (memory != VK_NULL_HANDLE) {
                            auto& managedMemories = context.resourceManager.getMemories();
                            auto it = std::ranges::find(managedMemories, memory);
                            if (it != managedMemories.end()) {
                                LOG_DEBUG(std::format("Removing uniform buffer memory {:p} from resource manager", static_cast<void*>(memory)));
                                context.resourceManager.removeMemory(memory);
                                removedFromManager++;
                            } else {
                                LOG_DEBUG(std::format("Uniform buffer memory {:p} not found in resource manager", static_cast<void*>(memory)));
                            }
                        }
                    }
                    LOG_DEBUG(std::format("Removed {} uniform memories from resource manager", removedFromManager));
                    if (context.device != VK_NULL_HANDLE) {
                        freeDeviceMemories(context.device, context.uniformBufferMemories);
                        LOG_DEBUG(std::format("Freed {} uniform buffer memories", context.uniformBufferMemories.size()));
                    }
                    context.uniformBufferMemories.clear();
                } else {
                    LOG_DEBUG("No uniform buffer memories to clean up");
                }
            } catch (const std::exception& e) {
                cleanupErrors.push_back(std::format("Error in uniform buffer memories cleanup: {}", e.what()));
                LOG_ERROR(std::format("Error in uniform buffer memories cleanup: {}", e.what()));
                context.uniformBufferMemories.clear();
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "UniformMemories"));
        }

        // Stage: GraphicsResources (Storage, RenderPass, Graphics Pipeline/Layout/DS/Layout/Pool)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "GraphicsResources"));
            std::vector<Disposable> graphicsDisposables = {
                {ResourceType::StorageImageView, reinterpret_cast<void*>(context.storageImageView), true, "Storage Image View", DisposeStage::GraphicsResources},
                {ResourceType::StorageImage, reinterpret_cast<void*>(context.storageImage), true, "Storage Image", DisposeStage::GraphicsResources},
                {ResourceType::StorageImageMemory, reinterpret_cast<void*>(context.storageImageMemory), true, "Storage Image Memory", DisposeStage::GraphicsResources},
                {ResourceType::RenderPass, reinterpret_cast<void*>(context.renderPass), false, "Render Pass", DisposeStage::GraphicsResources},
                {ResourceType::GraphicsPipeline, reinterpret_cast<void*>(context.graphicsPipeline), true, "Graphics Pipeline", DisposeStage::GraphicsResources},
                {ResourceType::GraphicsPipelineLayout, reinterpret_cast<void*>(context.graphicsPipelineLayout), true, "Graphics Pipeline Layout", DisposeStage::GraphicsResources},
                {ResourceType::GraphicsDescriptorSet, reinterpret_cast<void*>(context.graphicsDescriptorSet), false, "Graphics Descriptor Set", DisposeStage::GraphicsResources},
                {ResourceType::GraphicsDescriptorPool, reinterpret_cast<void*>(context.graphicsDescriptorPool), true, "Graphics Descriptor Pool", DisposeStage::GraphicsResources},
                {ResourceType::GraphicsDescriptorSetLayout, reinterpret_cast<void*>(context.graphicsDescriptorSetLayout), true, "Graphics Descriptor Set Layout", DisposeStage::GraphicsResources}
            };
            for (const auto& disp : graphicsDisposables) {
                if (disp.handle == VK_NULL_HANDLE) {
                    LOG_DEBUG(std::format("Skipping null {} in GraphicsResources stage", disp.name));
                    continue;
                }
                auto it = cleanupLookup.find(disp.type);
                if (it == cleanupLookup.end()) {
                    LOG_WARNING(std::format("Unknown resource type: {}, skipping in GraphicsResources stage", static_cast<int>(disp.type)));
                    continue;
                }
                try {
                    LOG_DEBUG(std::format("Cleaning up {} ({:p})", disp.name, disp.handle));
                    it->second(context, disp.handle, disp.managedByResourceManager);
                    // Nullify context fields (using switch as before, but only for graphics types)
                    switch (disp.type) {
                        case ResourceType::StorageImageView: context.storageImageView = VK_NULL_HANDLE; break;
                        case ResourceType::StorageImage: context.storageImage = VK_NULL_HANDLE; break;
                        case ResourceType::StorageImageMemory: context.storageImageMemory = VK_NULL_HANDLE; break;
                        case ResourceType::RenderPass: context.renderPass = VK_NULL_HANDLE; break;
                        case ResourceType::GraphicsPipeline: context.graphicsPipeline = VK_NULL_HANDLE; break;
                        case ResourceType::GraphicsPipelineLayout: context.graphicsPipelineLayout = VK_NULL_HANDLE; break;
                        case ResourceType::GraphicsDescriptorSet: context.graphicsDescriptorSet = VK_NULL_HANDLE; break;
                        case ResourceType::GraphicsDescriptorPool: context.graphicsDescriptorPool = VK_NULL_HANDLE; break;
                        case ResourceType::GraphicsDescriptorSetLayout: context.graphicsDescriptorSetLayout = VK_NULL_HANDLE; break;
                        default: break; // Safety
                    }
                } catch (const std::exception& e) {
                    cleanupErrors.push_back(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                    LOG_ERROR(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                    // Nullify on error (same switch)
                    switch (disp.type) {
                        case ResourceType::StorageImageView: context.storageImageView = VK_NULL_HANDLE; break;
                        case ResourceType::StorageImage: context.storageImage = VK_NULL_HANDLE; break;
                        case ResourceType::StorageImageMemory: context.storageImageMemory = VK_NULL_HANDLE; break;
                        case ResourceType::RenderPass: context.renderPass = VK_NULL_HANDLE; break;
                        case ResourceType::GraphicsPipeline: context.graphicsPipeline = VK_NULL_HANDLE; break;
                        case ResourceType::GraphicsPipelineLayout: context.graphicsPipelineLayout = VK_NULL_HANDLE; break;
                        case ResourceType::GraphicsDescriptorSet: context.graphicsDescriptorSet = VK_NULL_HANDLE; break;
                        case ResourceType::GraphicsDescriptorPool: context.graphicsDescriptorPool = VK_NULL_HANDLE; break;
                        case ResourceType::GraphicsDescriptorSetLayout: context.graphicsDescriptorSetLayout = VK_NULL_HANDLE; break;
                        default: break;
                    }
                }
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "GraphicsResources"));
        }

        // Stage: ComputeResources (Compute Pipeline/Layout/DS/Pool)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "ComputeResources"));
            std::vector<Disposable> computeDisposables = {
                {ResourceType::ComputePipeline, reinterpret_cast<void*>(context.computePipeline), true, "Compute Pipeline", DisposeStage::ComputeResources},
                {ResourceType::ComputePipelineLayout, reinterpret_cast<void*>(context.computePipelineLayout), true, "Compute Pipeline Layout", DisposeStage::ComputeResources},
                {ResourceType::DescriptorSet, reinterpret_cast<void*>(context.descriptorSet), false, "Descriptor Set", DisposeStage::ComputeResources},
                {ResourceType::DescriptorPool, reinterpret_cast<void*>(context.descriptorPool), true, "Descriptor Pool", DisposeStage::ComputeResources}
            };
            for (const auto& disp : computeDisposables) {
                if (disp.handle == VK_NULL_HANDLE) continue;
                auto it = cleanupLookup.find(disp.type);
                if (it == cleanupLookup.end()) {
                    LOG_WARNING(std::format("Unknown resource type: {}, skipping in ComputeResources stage", static_cast<int>(disp.type)));
                    continue;
                }
                try {
                    LOG_DEBUG(std::format("Cleaning up {} ({:p})", disp.name, disp.handle));
                    it->second(context, disp.handle, disp.managedByResourceManager);
                    switch (disp.type) {
                        case ResourceType::ComputePipeline: context.computePipeline = VK_NULL_HANDLE; break;
                        case ResourceType::ComputePipelineLayout: context.computePipelineLayout = VK_NULL_HANDLE; break;
                        case ResourceType::DescriptorSet: context.descriptorSet = VK_NULL_HANDLE; break;
                        case ResourceType::DescriptorPool: context.descriptorPool = VK_NULL_HANDLE; break;
                        default: break;
                    }
                } catch (const std::exception& e) {
                    cleanupErrors.push_back(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                    LOG_ERROR(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                    switch (disp.type) {
                        case ResourceType::ComputePipeline: context.computePipeline = VK_NULL_HANDLE; break;
                        case ResourceType::ComputePipelineLayout: context.computePipelineLayout = VK_NULL_HANDLE; break;
                        case ResourceType::DescriptorSet: context.descriptorSet = VK_NULL_HANDLE; break;
                        case ResourceType::DescriptorPool: context.descriptorPool = VK_NULL_HANDLE; break;
                        default: break;
                    }
                }
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "ComputeResources"));
        }

        // Stage: RayTracingResources (RT Pipeline/Layout/DS/Layout, AS, SBTs)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "RayTracingResources"));
            std::vector<Disposable> rtDisposables = {
                {ResourceType::RayTracingPipeline, reinterpret_cast<void*>(context.rayTracingPipeline), true, "Ray Tracing Pipeline", DisposeStage::RayTracingResources},
                {ResourceType::RayTracingPipelineLayout, reinterpret_cast<void*>(context.rayTracingPipelineLayout), true, "Ray Tracing Pipeline Layout", DisposeStage::RayTracingResources},
                {ResourceType::RayTracingDescriptorSetLayout, reinterpret_cast<void*>(context.rayTracingDescriptorSetLayout), true, "Ray Tracing Descriptor Set Layout", DisposeStage::RayTracingResources},
                {ResourceType::TopLevelAS, reinterpret_cast<void*>(context.topLevelAS), true, "Top-Level Acceleration Structure", DisposeStage::RayTracingResources},
                {ResourceType::BottomLevelAS, reinterpret_cast<void*>(context.bottomLevelAS), true, "Bottom-Level Acceleration Structure", DisposeStage::RayTracingResources},
                {ResourceType::BottomLevelASBuffer, reinterpret_cast<void*>(context.bottomLevelASBuffer), true, "Bottom-Level AS Buffer", DisposeStage::RayTracingResources},
                {ResourceType::BottomLevelASMemory, reinterpret_cast<void*>(context.bottomLevelASMemory), true, "Bottom-Level AS Memory", DisposeStage::RayTracingResources},
                {ResourceType::TopLevelASBuffer, reinterpret_cast<void*>(context.topLevelASBuffer), true, "Top-Level AS Buffer", DisposeStage::RayTracingResources},
                {ResourceType::TopLevelASMemory, reinterpret_cast<void*>(context.topLevelASMemory), true, "Top-Level AS Memory", DisposeStage::RayTracingResources},
                {ResourceType::RaygenSbtBuffer, reinterpret_cast<void*>(context.raygenSbtBuffer), true, "Raygen SBT Buffer", DisposeStage::RayTracingResources},
                {ResourceType::RaygenSbtMemory, reinterpret_cast<void*>(context.raygenSbtMemory), true, "Raygen SBT Memory", DisposeStage::RayTracingResources},
                {ResourceType::MissSbtBuffer, reinterpret_cast<void*>(context.missSbtBuffer), true, "Miss SBT Buffer", DisposeStage::RayTracingResources},
                {ResourceType::MissSbtMemory, reinterpret_cast<void*>(context.missSbtMemory), true, "Miss SBT Memory", DisposeStage::RayTracingResources},
                {ResourceType::HitSbtBuffer, reinterpret_cast<void*>(context.hitSbtBuffer), true, "Hit SBT Buffer", DisposeStage::RayTracingResources},
                {ResourceType::HitSbtMemory, reinterpret_cast<void*>(context.hitSbtMemory), true, "Hit SBT Memory", DisposeStage::RayTracingResources}
            };
            for (const auto& disp : rtDisposables) {
                if (disp.handle == VK_NULL_HANDLE) continue;
                auto it = cleanupLookup.find(disp.type);
                if (it == cleanupLookup.end()) {
                    LOG_WARNING(std::format("Unknown resource type: {}, skipping in RayTracingResources stage", static_cast<int>(disp.type)));
                    continue;
                }
                try {
                    LOG_DEBUG(std::format("Cleaning up {} ({:p})", disp.name, disp.handle));
                    it->second(context, disp.handle, disp.managedByResourceManager);
                    switch (disp.type) {
                        case ResourceType::RayTracingPipeline: context.rayTracingPipeline = VK_NULL_HANDLE; break;
                        case ResourceType::RayTracingPipelineLayout: context.rayTracingPipelineLayout = VK_NULL_HANDLE; break;
                        case ResourceType::RayTracingDescriptorSetLayout: context.rayTracingDescriptorSetLayout = VK_NULL_HANDLE; break;
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
                        default: break;
                    }
                } catch (const std::exception& e) {
                    cleanupErrors.push_back(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                    LOG_ERROR(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                    // Nullify on error (same switch as above)
                    switch (disp.type) {
                        case ResourceType::RayTracingPipeline: context.rayTracingPipeline = VK_NULL_HANDLE; break;
                        case ResourceType::RayTracingPipelineLayout: context.rayTracingPipelineLayout = VK_NULL_HANDLE; break;
                        case ResourceType::RayTracingDescriptorSetLayout: context.rayTracingDescriptorSetLayout = VK_NULL_HANDLE; break;
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
                        default: break;
                    }
                }
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "RayTracingResources"));
        }

        // Stage: BufferResources (Vertex/Index/Scratch Buffers)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "BufferResources"));
            std::vector<Disposable> bufferDisposables = {
                {ResourceType::VertexBuffer, reinterpret_cast<void*>(context.vertexBuffer), true, "Vertex Buffer", DisposeStage::BufferResources},
                {ResourceType::IndexBuffer, reinterpret_cast<void*>(context.indexBuffer), true, "Index Buffer", DisposeStage::BufferResources},
                {ResourceType::ScratchBuffer, reinterpret_cast<void*>(context.scratchBuffer), true, "Scratch Buffer", DisposeStage::BufferResources}
            };
            for (const auto& disp : bufferDisposables) {
                if (disp.handle == VK_NULL_HANDLE) continue;
                auto it = cleanupLookup.find(disp.type);
                if (it == cleanupLookup.end()) {
                    LOG_WARNING(std::format("Unknown resource type: {}, skipping in BufferResources stage", static_cast<int>(disp.type)));
                    continue;
                }
                try {
                    LOG_DEBUG(std::format("Cleaning up {} ({:p})", disp.name, disp.handle));
                    it->second(context, disp.handle, disp.managedByResourceManager);
                    switch (disp.type) {
                        case ResourceType::VertexBuffer: context.vertexBuffer = VK_NULL_HANDLE; break;
                        case ResourceType::IndexBuffer: context.indexBuffer = VK_NULL_HANDLE; break;
                        case ResourceType::ScratchBuffer: context.scratchBuffer = VK_NULL_HANDLE; break;
                        default: break;
                    }
                } catch (const std::exception& e) {
                    cleanupErrors.push_back(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                    LOG_ERROR(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                    switch (disp.type) {
                        case ResourceType::VertexBuffer: context.vertexBuffer = VK_NULL_HANDLE; break;
                        case ResourceType::IndexBuffer: context.indexBuffer = VK_NULL_HANDLE; break;
                        case ResourceType::ScratchBuffer: context.scratchBuffer = VK_NULL_HANDLE; break;
                        default: break;
                    }
                }
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "BufferResources"));
        }

        // Stage: MemoryResources (All buffer memories)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "MemoryResources"));
            std::vector<Disposable> memoryDisposables = {
                {ResourceType::VertexBufferMemory, reinterpret_cast<void*>(context.vertexBufferMemory), true, "Vertex Buffer Memory", DisposeStage::MemoryResources},
                {ResourceType::IndexBufferMemory, reinterpret_cast<void*>(context.indexBufferMemory), true, "Index Buffer Memory", DisposeStage::MemoryResources},
                {ResourceType::ScratchBufferMemory, reinterpret_cast<void*>(context.scratchBufferMemory), true, "Scratch Buffer Memory", DisposeStage::MemoryResources}
            };
            for (const auto& disp : memoryDisposables) {
                if (disp.handle == VK_NULL_HANDLE) continue;
                auto it = cleanupLookup.find(disp.type);
                if (it == cleanupLookup.end()) {
                    LOG_WARNING(std::format("Unknown resource type: {}, skipping in MemoryResources stage", static_cast<int>(disp.type)));
                    continue;
                }
                try {
                    LOG_DEBUG(std::format("Cleaning up {} ({:p})", disp.name, disp.handle));
                    it->second(context, disp.handle, disp.managedByResourceManager);
                    switch (disp.type) {
                        case ResourceType::VertexBufferMemory: context.vertexBufferMemory = VK_NULL_HANDLE; break;
                        case ResourceType::IndexBufferMemory: context.indexBufferMemory = VK_NULL_HANDLE; break;
                        case ResourceType::ScratchBufferMemory: context.scratchBufferMemory = VK_NULL_HANDLE; break;
                        default: break;
                    }
                } catch (const std::exception& e) {
                    cleanupErrors.push_back(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                    LOG_ERROR(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                    switch (disp.type) {
                        case ResourceType::VertexBufferMemory: context.vertexBufferMemory = VK_NULL_HANDLE; break;
                        case ResourceType::IndexBufferMemory: context.indexBufferMemory = VK_NULL_HANDLE; break;
                        case ResourceType::ScratchBufferMemory: context.scratchBufferMemory = VK_NULL_HANDLE; break;
                        default: break;
                    }
                }
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "MemoryResources"));
        }

        // Stage: Sampler (Single)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "Sampler"));
            std::vector<Disposable> samplerDisposables = {
                {ResourceType::Sampler, reinterpret_cast<void*>(context.sampler), false, "Sampler", DisposeStage::CoreVulkan}
            };
            for (const auto& disp : samplerDisposables) {
                if (disp.handle == VK_NULL_HANDLE) continue;
                auto it = cleanupLookup.find(disp.type);
                if (it != cleanupLookup.end()) {
                    try {
                        LOG_DEBUG(std::format("Cleaning up {} ({:p})", disp.name, disp.handle));
                        it->second(context, disp.handle, disp.managedByResourceManager);
                        context.sampler = VK_NULL_HANDLE;
                    } catch (const std::exception& e) {
                        cleanupErrors.push_back(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                        LOG_ERROR(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                        context.sampler = VK_NULL_HANDLE;
                    }
                }
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "Sampler"));
        }

        // Stage: CoreVulkan (Swapchain, CommandPool, Device, Surface, DebugMessenger, Instance)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "CoreVulkan"));
            std::vector<Disposable> coreDisposables = {
                {ResourceType::Swapchain, reinterpret_cast<void*>(context.swapchain), false, "Swapchain", DisposeStage::CoreVulkan},
                {ResourceType::CommandPool, reinterpret_cast<void*>(context.commandPool), true, "Command Pool", DisposeStage::CoreVulkan},
                {ResourceType::Device, reinterpret_cast<void*>(context.device), false, "Device", DisposeStage::CoreVulkan},
                {ResourceType::Surface, reinterpret_cast<void*>(context.surface), false, "Surface", DisposeStage::CoreVulkan},
                {ResourceType::DebugMessenger, reinterpret_cast<void*>(context.debugMessenger), false, "Debug Messenger", DisposeStage::CoreVulkan},
                {ResourceType::Instance, reinterpret_cast<void*>(context.instance), false, "Instance", DisposeStage::CoreVulkan}
            };
            for (const auto& disp : coreDisposables) {
                if (disp.handle == VK_NULL_HANDLE) continue;
                auto it = cleanupLookup.find(disp.type);
                if (it == cleanupLookup.end()) {
                    LOG_WARNING(std::format("Unknown resource type: {}, skipping in CoreVulkan stage", static_cast<int>(disp.type)));
                    continue;
                }
                try {
                    LOG_DEBUG(std::format("Cleaning up {} ({:p})", disp.name, disp.handle));
                    it->second(context, disp.handle, disp.managedByResourceManager);
                    switch (disp.type) {
                        case ResourceType::Swapchain: context.swapchain = VK_NULL_HANDLE; break;
                        case ResourceType::CommandPool: context.commandPool = VK_NULL_HANDLE; break;
                        case ResourceType::Device: context.device = VK_NULL_HANDLE; break;
                        case ResourceType::Surface: context.surface = VK_NULL_HANDLE; break;
                        case ResourceType::DebugMessenger: context.debugMessenger = VK_NULL_HANDLE; break;
                        case ResourceType::Instance: context.instance = VK_NULL_HANDLE; break;
                        default: break;
                    }
                } catch (const std::exception& e) {
                    cleanupErrors.push_back(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                    LOG_ERROR(std::format("Error in {} cleanup: {}", disp.name, e.what()));
                    switch (disp.type) {
                        case ResourceType::Swapchain: context.swapchain = VK_NULL_HANDLE; break;
                        case ResourceType::CommandPool: context.commandPool = VK_NULL_HANDLE; break;
                        case ResourceType::Device: context.device = VK_NULL_HANDLE; break;
                        case ResourceType::Surface: context.surface = VK_NULL_HANDLE; break;
                        case ResourceType::DebugMessenger: context.debugMessenger = VK_NULL_HANDLE; break;
                        case ResourceType::Instance: context.instance = VK_NULL_HANDLE; break;
                        default: break;
                    }
                }
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "CoreVulkan"));
        }

        // Stage: FinalNullify (Step 51-52: Nullify remaining and report)
        {
            LOG_INFO(std::format("--- Stage {}: {} ---", ++stageCount, "FinalNullify"));
            LOG_DEBUG("Nullifying remaining context fields");
            context.physicalDevice = VK_NULL_HANDLE;
            context.graphicsQueue = VK_NULL_HANDLE;
            context.presentQueue = VK_NULL_HANDLE;
            context.swapchainImageFormat = VK_FORMAT_UNDEFINED;
            context.swapchainExtent = {0, 0};
            context.graphicsQueueFamilyIndex = UINT32_MAX;
            context.presentQueueFamilyIndex = UINT32_MAX;
            context.enableRayTracing = false;
            LOG_INFO("Remaining context fields nullified");

            LOG_DEBUG("Final context cleanup reporting");
            if (!cleanupErrors.empty()) {
                LOG_ERROR(std::format("Cleanup completed with {} errors across {} stages:", cleanupErrors.size(), stageCount));
                for (const auto& error : cleanupErrors) {
                    LOG_ERROR(std::format("  - {}", error));
                }
            } else {
                LOG_INFO(std::format("Vulkan context cleanup completed successfully - {} stages processed without errors", stageCount));
            }
            LOG_INFO(std::format("--- Stage {}: {} completed ---", stageCount, "FinalNullify"));
        }

        LOG_INFO("=== Vulkan context cleanup finished ===");
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